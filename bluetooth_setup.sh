#!/usr/bin/env bash
#
# Configure the Raspberry Pi as a Bluetooth PAN (NAP) access point, so a PC
# can reach wm_headless over Bluetooth instead of WiFi/Ethernet. wm_headless
# itself needs no changes for this — it already listens on all interfaces
# (QHostAddress::Any in headless_main.cpp), and a Bluetooth PAN connection
# just shows up as another network interface (bnep0, bridged into pan0
# below) carrying ordinary TCP/IP. See docs/user_guide.md for how a PC
# pairs against this once it's running.
#
# NOT verified on real hardware. Built from the documented BlueZ NAP pattern
# (bluetoothd's built-in network server, driven via bluez-tools' bt-network),
# but there's no Bluetooth-capable Pi or PC available to pair against and
# test in the environment this was written in. Run it, then work through
# any gap between this and what your BlueZ/Raspberry Pi OS version actually
# does — please report back what needed fixing so this can be corrected.
#
# Usage: ./bluetooth_setup.sh   (on the Pi itself, needs sudo)
#
set -e

PAN_IF="pan0"
PAN_SUBNET="10.42.0.0/24"
PAN_IP="10.42.0.1"
DHCP_RANGE_LOW="10.42.0.50"
DHCP_RANGE_HIGH="10.42.0.100"

if ! command -v apt >/dev/null; then
    echo "bluetooth_setup.sh: this script targets Raspberry Pi OS / Debian (apt)."
    exit 1
fi

echo "==> Installing bluez, bluez-tools, dnsmasq"
sudo apt install -y bluez bluez-tools dnsmasq

echo "==> Creating the $PAN_IF bridge (static IP $PAN_IP) via NetworkManager"
# Raspberry Pi OS Bookworm defaults to NetworkManager. If nmcli isn't
# available (older Bullseye/dhcpcd-based image), this needs an equivalent
# manual bridge (`ip link add pan0 type bridge` + a static entry in
# /etc/dhcpcd.conf) instead — not attempted here since dhcpcd's config
# format/precedence is a different enough setup to need its own testing.
if command -v nmcli >/dev/null; then
    if ! nmcli -t -f NAME connection show | grep -qx "$PAN_IF"; then
        sudo nmcli connection add type bridge ifname "$PAN_IF" con-name "$PAN_IF" \
            ipv4.method manual ipv4.addresses "$PAN_IP/24" ipv6.method disabled
    fi
    sudo nmcli connection up "$PAN_IF"
else
    echo "bluetooth_setup.sh: nmcli not found — this Pi isn't using NetworkManager."
    echo "  You'll need to create the $PAN_IF bridge manually for your network stack."
    exit 1
fi

echo "==> Scoping dnsmasq to $PAN_IF only (DHCP for connecting PCs, $DHCP_RANGE_LOW-$DHCP_RANGE_HIGH)"
# bind-interfaces + interface=$PAN_IF keeps this from touching the Pi's
# main network (WiFi/Ethernet) DHCP/DNS in any way.
printf 'interface=%s\nbind-interfaces\ndhcp-range=%s,%s,255.255.255.0,24h\n' \
    "$PAN_IF" "$DHCP_RANGE_LOW" "$DHCP_RANGE_HIGH" \
    | sudo tee /etc/dnsmasq.d/pan0.conf >/dev/null
sudo systemctl enable --now dnsmasq

echo "==> Installing systemd services: pairing agent + NAP access point"
# Two separate long-running services, not one script — both bt-agent and
# bt-network -s nap block/stay resident (they're daemons, not one-shot
# commands), so each needs its own Type=simple unit rather than being
# chained with shell operators systemd's Exec= doesn't interpret anyway.
#
# - bt-agent --capability=NoInputNoOutput: auto-accepts pairing with no PIN
#   prompt (there's no display/keyboard on this headless box to answer one).
# - bt-network -s nap pan0: registers the NAP (Network Access Point) role,
#   bridging any incoming Bluetooth PAN client into pan0.
sudo tee /etc/systemd/system/bt-agent.service >/dev/null <<'EOF'
[Unit]
Description=Bluetooth pairing agent (no PIN prompt — headless box)
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=simple
ExecStart=/usr/bin/bt-agent --capability=NoInputNoOutput
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo tee /usr/local/sbin/bt-pan-nap-start.sh >/dev/null <<'EOF'
#!/bin/bash
set -e
bluetoothctl power on
bluetoothctl discoverable on
bluetoothctl pairable on
exec bt-network -s nap pan0
EOF
sudo chmod +x /usr/local/sbin/bt-pan-nap-start.sh

sudo tee /etc/systemd/system/bt-pan-nap.service >/dev/null <<EOF
[Unit]
Description=Bluetooth PAN (NAP) access point for wm_headless
After=bluetooth.service NetworkManager.service bt-agent.service
Requires=bluetooth.service bt-agent.service

[Service]
Type=simple
ExecStart=/usr/local/sbin/bt-pan-nap-start.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

echo "==> Installing systemd service to auto-start wm_headless at boot"
# The whole point of "no GUI on the Pi" is that nobody has to SSH in and
# launch anything by hand — wm_headless just needs to already be running,
# ready for pc_client to connect over whichever network gets it there
# (Bluetooth PAN above, or plain WiFi/Ethernet — wm_headless doesn't care).
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
sudo tee /etc/systemd/system/wm-headless.service >/dev/null <<EOF
[Unit]
Description=wm_headless — I-P characterisation server (no GUI, remote-controlled)
After=network.target bluetooth.service bt-pan-nap.service

[Service]
Type=simple
WorkingDirectory=$REPO_DIR
ExecStart=$REPO_DIR/run_headless.sh
Restart=on-failure
RestartSec=5
User=$USER

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now bt-agent.service bt-pan-nap.service wm-headless.service

echo "==> Done."
echo "    Pi is now discoverable/pairable over Bluetooth, bridges paired"
echo "    devices onto $PAN_SUBNET via $PAN_IF, and wm_headless is running"
echo "    and will auto-start on every boot."
echo "    Pair from the PC (see docs/user_guide.md), then point pc_client at"
echo "    whatever IP the Pi got on $PAN_IF (should be $PAN_IP) — same"
echo "    wm_headless, same TCP port, no code changes needed on either side."
echo "    Check status with: sudo systemctl status bt-agent bt-pan-nap wm-headless dnsmasq"

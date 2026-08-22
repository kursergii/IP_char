#!/usr/bin/env bash
#
# Shared by run.sh (WM GUI) and run_headless.sh (wm_headless): power-cycles
# the Thorlabs PM101A's hub port via uhubctl to clear its wedged-on-powerup
# USBTMC state — a real VBUS power-cycle, since a USB bus reset doesn't
# clear it. Source this; it isn't meant to run standalone.
#
# Needs: uhubctl + passwordless sudo for it (set up by ./build.sh deps).

METER="1313:8076"   # Thorlabs PM101A vendor:product

power_cycle_meter() {
    if ! command -v uhubctl >/dev/null; then
        echo "$0: uhubctl not installed  ->  sudo apt install uhubctl"
        return 1
    fi
    if ! sudo -n true 2>/dev/null; then
        echo "$0: passwordless sudo not set. Run './build.sh deps', or:"
        echo "  echo \"\$USER ALL=(root) NOPASSWD: \$(command -v uhubctl)\" | sudo tee /etc/sudoers.d/uhubctl-nopasswd"
        return 1
    fi

    # Auto-detect the meter's hub location + port (robust to which port it's in).
    local info hub port
    info=$(sudo -n uhubctl 2>/dev/null | awk -v id="$METER" '
        /Current status for hub/ { h=$5 }
        index($0, id)            { gsub(/:/,"",$2); print h, $2; exit }')
    if [ -z "$info" ]; then
        echo "$0: meter ($METER) not found by uhubctl (hub without per-port power?)"
        return 1
    fi
    hub=${info% *}; port=${info#* }

    echo "$0: power-cycling meter (hub $hub port $port) to clear wedged state"
    sudo -n uhubctl -a cycle -l "$hub" -p "$port" -d 2
    sleep 3
    echo "$0: meter power-cycled"
}

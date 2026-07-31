#!/usr/bin/env bash
#
# Launch the WM / IP_char app on the Raspberry Pi kiosk.
#
# The Thorlabs PM101A can power up in a wedged USBTMC state that only a real
# VBUS power-cycle clears (a USB bus reset does not). This script finds the
# meter's hub port and power-cycles just that port with uhubctl before starting
# WM, so Connect works without any manual unplug/replug. Once un-wedged the app
# talks to the meter fine and does not re-wedge it.
#
# Needs: uhubctl + passwordless sudo for it (set up by ./build.sh deps).
# Any arguments are passed through to WM (e.g. ./run.sh -sim).
#
set -e
cd "$(dirname "$0")"

METER="1313:8076"   # Thorlabs PM101A vendor:product

power_cycle_meter() {
    if ! command -v uhubctl >/dev/null; then
        echo "run.sh: uhubctl not installed  ->  sudo apt install uhubctl"
        return 1
    fi
    if ! sudo -n true 2>/dev/null; then
        echo "run.sh: passwordless sudo not set. Run './build.sh deps', or:"
        echo "  echo \"\$USER ALL=(root) NOPASSWD: \$(command -v uhubctl)\" | sudo tee /etc/sudoers.d/uhubctl-nopasswd"
        return 1
    fi

    # Auto-detect the meter's hub location + port (robust to which port it's in).
    local info hub port
    info=$(sudo -n uhubctl 2>/dev/null | awk -v id="$METER" '
        /Current status for hub/ { h=$5 }
        index($0, id)            { gsub(/:/,"",$2); print h, $2; exit }')
    if [ -z "$info" ]; then
        echo "run.sh: meter ($METER) not found by uhubctl (hub without per-port power?)"
        return 1
    fi
    hub=${info% *}; port=${info#* }

    echo "run.sh: power-cycling meter (hub $hub port $port) to clear wedged state"
    sudo -n uhubctl -a cycle -l "$hub" -p "$port" -d 2
    sleep 3
    echo "run.sh: meter power-cycled"
}

power_cycle_meter || echo "run.sh: WARNING — meter not power-cycled; Connect may fail if it is wedged"

echo "run.sh: starting WM"
exec ./bin/WM "$@"

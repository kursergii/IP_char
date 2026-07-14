#!/usr/bin/env bash
#
# Launch the WM / IP_char app on the Raspberry Pi kiosk.
#
# The Thorlabs PM101A can power up in a wedged USBTMC state that only a real
# VBUS power-cycle clears (a USB bus reset does not). This script finds the
# meter's hub port and power-cycles just that port with uhubctl before starting
# WM, so Connect works without any manual unplug/replug.
#
# Requires: uhubctl, and passwordless sudo for it (see build.sh deps).
# Any arguments are passed through to WM (e.g. ./run.sh -sim).
#
set -e
cd "$(dirname "$0")"

METER_VIDPID="1313:8076"   # Thorlabs PM101A

# Auto-detect the meter's hub location + port from uhubctl (robust to which
# physical port it's plugged into). Falls back to skipping if not found.
info=$(sudo uhubctl 2>/dev/null \
    | awk -v id="$METER_VIDPID" '
        /Current status for hub/ { hub=$5 }
        index($0, id)            { gsub(/:/,"",$2); print hub, $2; exit }')

if [ -n "$info" ]; then
    hub=${info% *}; port=${info#* }
    echo "==> Power-cycling power meter (hub $hub port $port) to clear wedged state"
    sudo uhubctl -a cycle -l "$hub" -p "$port" -d 2 || \
        echo "    (uhubctl failed — check it's installed and passwordless sudo is set)"
    sleep 3
else
    echo "==> Power meter ($METER_VIDPID) not found by uhubctl — launching anyway"
fi

echo "==> Starting WM"
exec ./bin/WM "$@"

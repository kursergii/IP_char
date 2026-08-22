#!/usr/bin/env bash
#
# Launch the WM / IP_char GUI app on the Raspberry Pi kiosk touchscreen.
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
source ./power_cycle_meter.sh

power_cycle_meter || echo "run.sh: WARNING — meter not power-cycled; Connect may fail if it is wedged"

echo "run.sh: starting WM"
exec ./bin/WM "$@"

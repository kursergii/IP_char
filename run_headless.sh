#!/usr/bin/env bash
#
# Launch wm_headless (no GUI) on the Raspberry Pi — the server half of the
# remote-control split (see src/wmcore.h, pc_client/). A PC running
# pc_client reaches this over whatever network the Pi has: WiFi/Ethernet
# directly, or the Bluetooth PAN set up by bluetooth_setup.sh.
#
# Same PM101A un-wedge power-cycle as run.sh (shared helper) — same hardware
# fix regardless of which binary ends up talking to the meter.
# Any arguments are passed through to wm_headless (e.g. ./run_headless.sh -sim).
#
set -e
cd "$(dirname "$0")"
source ./power_cycle_meter.sh

power_cycle_meter || echo "run_headless.sh: WARNING — meter not power-cycled; connect may fail if it is wedged"

echo "run_headless.sh: starting wm_headless"
exec ./bin/wm_headless "$@"

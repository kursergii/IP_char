#!/usr/bin/env bash
#
# Clean build of the WM / IP_char app (Widgets version).
# Removes bin/ and build/, then configures and builds from scratch.
#
# Usage:
#   ./build.sh deps     one-time setup: install dependencies + udev rule (needs sudo)
#   ./build.sh          clean build
#   ./build.sh run      clean build, then run
#   ./build.sh sim      clean build, then run in simulation mode (-sim)
#
set -e

# Work from the project root (this script's directory)
cd "$(dirname "$0")"

# ── One-time deployment setup (Raspberry Pi OS / Debian) ─────────────────────
setup_deps() {
    echo "==> Installing build dependencies (apt)"
    sudo apt install -y cmake g++ qt6-base-dev libqt6serialport6-dev \
        qt6-wayland libusb-1.0-0-dev

    echo "==> Installing udev rule for Thorlabs power meter (vendor 1313)"
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", MODE="0666"' \
        | sudo tee /etc/udev/rules.d/99-thorlabs-libusb.rules
    sudo udevadm control --reload-rules && sudo udevadm trigger
    echo "==> Setup done. (libps2000 PicoScope driver must be installed separately.)"
}

if [ "${1:-}" = "deps" ] || [ "${1:-}" = "setup" ]; then
    setup_deps
    exit 0
fi

echo "==> Cleaning bin/ and build/"
rm -rf bin build

echo "==> Creating build directory"
mkdir build
cd build

echo "==> Configuring (CMake, Release)"
cmake ../src -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build . -j"$(nproc)"

cd ..
echo "==> Done -> bin/WM"

case "${1:-}" in
    run) echo "==> Running"; ./bin/WM ;;
    sim) echo "==> Running (simulation)"; ./bin/WM -sim ;;
esac

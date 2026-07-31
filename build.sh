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

# ── One-time deployment setup (Raspberry Pi OS / Debian, or Fedora) ──────────
setup_deps() {
    if command -v apt >/dev/null; then
        echo "==> Installing build dependencies (apt / Debian, Raspberry Pi)"
        sudo apt install -y cmake g++ qt6-base-dev libqt6serialport6-dev \
            qt6-wayland libusb-1.0-0-dev uhubctl
    elif command -v dnf >/dev/null; then
        echo "==> Installing build dependencies (dnf / Fedora)"
        sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel qt6-qtserialport-devel \
            qt6-qtwayland libusb1-devel uhubctl
    else
        echo "==> No apt or dnf found — install the equivalents of:"
        echo "    cmake, g++, Qt6 base+serialport+wayland dev, libusb-1.0 dev, uhubctl"
    fi

    echo "==> Adding $USER to 'dialout' group (serial access to the pulser)"
    sudo usermod -aG dialout "$USER"   # takes effect after next login/reboot

    echo "==> Installing udev rule for Thorlabs power meter (vendor 1313)"
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", MODE="0666"' \
        | sudo tee /etc/udev/rules.d/99-thorlabs-libusb.rules
    sudo udevadm control --reload-rules && sudo udevadm trigger

    # The app talks to the meter via libusb. The kernel usbtmc driver both
    # fails on the PM101A and wedges it on probe, so stop it loading at all.
    # 'blacklist' alone doesn't block hotplug auto-load; 'install ... /bin/true'
    # does. Unloading it now also un-wedges an already-attached meter.
    echo "==> Blocking kernel usbtmc driver (app uses libusb directly)"
    printf 'blacklist usbtmc\ninstall usbtmc /bin/true\n' \
        | sudo tee /etc/modprobe.d/blacklist-usbtmc.conf
    sudo modprobe -r usbtmc 2>/dev/null || true

    # run.sh power-cycles the meter's hub port (uhubctl) to un-wedge the PM101A.
    # Allow it without a password so the kiosk launcher needs no interaction.
    echo "==> Granting passwordless sudo for uhubctl (used by run.sh)"
    UHUBCTL=$(command -v uhubctl || echo /usr/sbin/uhubctl)
    echo "$USER ALL=(root) NOPASSWD: $UHUBCTL" \
        | sudo tee /etc/sudoers.d/uhubctl-nopasswd
    sudo chmod 440 /etc/sudoers.d/uhubctl-nopasswd

    echo "==> Setup done. Build with ./build.sh, then launch with ./run.sh"
    echo "    (libps2000 PicoScope driver must be installed separately.)"
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

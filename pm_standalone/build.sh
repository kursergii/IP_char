#!/usr/bin/env bash
# Build the standalone power-meter reader (pm).
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -o pm pm.cpp $(pkg-config --cflags --libs libusb-1.0)
echo "built -> $(pwd)/pm    (run: ./pm)"

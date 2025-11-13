#!/bin/bash
set -e

echo "================================================"
echo "Configuring Raspberry Pi 4 for ANTZ development"
echo "================================================"
echo ""

# Check if running on ARM
if [[ "$(uname -m)" != "armv7l" && "$(uname -m)" != "aarch64" ]]; then
    echo "WARNING: This script is designed for Raspberry Pi 4 (ARM architecture)"
    echo "Detected architecture: $(uname -m)"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "Updating package lists..."
sudo apt-get update -q

echo ""
echo "Installing build dependencies..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

echo ""
echo "Installing runtime dependencies..."
sudo apt-get install -y \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    libmosquitto1 \
    libmosquitto-dev

echo ""
echo "Checking for ANT+ USB dongle..."
if lsusb | grep -q "0fcf:"; then
    echo "✓ ANT+ USB dongle detected (Dynastream/Garmin)"
elif lsusb | grep -q "ANT"; then
    echo "✓ ANT+ USB dongle detected"
else
    echo "⚠ No ANT+ USB dongle detected"
    echo "  Make sure your ANT+ USB stick is connected"
fi

echo ""
echo "Creating udev rule for ANT+ USB stick..."
UDEV_RULE='/etc/udev/rules.d/99-antz.rules'
if [ ! -f "$UDEV_RULE" ]; then
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0fcf", ATTRS{idProduct}=="1009", MODE="0666"' | sudo tee $UDEV_RULE > /dev/null
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    echo "✓ udev rule created (allows all users to access ANT+ USB dongles)"
else
    echo "✓ udev rule already exists"
fi

echo ""
echo "================================================"
echo "Configuration complete!"
echo "================================================"
echo ""
echo "Next steps:"
echo "  1. Build the project: ./scripts/build-rpi4.sh"
echo "  2. Or install from releases: see README.md"
echo ""

#!/bin/bash
set -e

# Build script for Raspberry Pi 4 (ARM) Debian packages
# This script sets up the build environment and creates a .deb package

BUILD_DIR="build-rpi4"
PLATFORM="linux"

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Building ANT+ Discovery for Raspberry Pi 4..."

# Clean previous build
if [ -d "$PROJECT_ROOT/$BUILD_DIR" ]; then
    echo "Cleaning previous build directory..."
    rm -rf "$PROJECT_ROOT/$BUILD_DIR"
fi

# Create build directory
mkdir -p "$PROJECT_ROOT/$BUILD_DIR"
cd "$PROJECT_ROOT/$BUILD_DIR"

# Configure CMake for Linux/ARM platform
cmake "$PROJECT_ROOT" \
    -DANTZ_PLATFORM=$PLATFORM \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

# Build the project
echo "Building project..."
make -j$(nproc)

# Create the package
echo "Creating Debian package..."
cpack

echo "Build complete! Package created in $BUILD_DIR/"
ls -lh *.deb

# ant_discovery

ant_discovery is a legacy, monolithic implementation of ANT+ discovery and profile 
decoding, originally created as a single-file prototype to explore how Garmin Asset Tracker 
and HRM profiles behave in real-time across multiple ANT channels.

This module is temporarily preserved during the migration to the modular 
`libs/antz_core`, `libs/antz_bridge`, and `libs/antz_platform` structure of the main `antz` project.

## How to use
If you have a ANT+ usb stick installed in the system, this is the recommended way to use ant_discovery.
```bash
ant_discovery -d 0
```
It tells the app to use the first ANT+ USB stick it finds. If you have several ANT+ sticks, you can 
omit the `-d` flag and it will prompt you to select one. 

There are several other options available, see `ant_discovery -h` for details. 
* If you need to specify the minimum distance a device must move before
outputting a message, you can use the `-e` flag which accepts a value in meters. 
* If you want to output a specific message format, you can use the `-f` flag which accepts `text`, `json`, or `csv`. 
* If you want to output to a specific MQTT broker, you can use the `-m` flag which accepts a MQTT connection string like
`"mqtt://user:pass@broker.example.com:1883/ant?retain=1&qos=1"`. The message payload format is given by the `-f` flag. 

## Purpose
- Provides a self-contained implementation of ANT+ message parsing, device management, and profile-specific handling (Asset Tracker, HRM)
- Useful for regression testing, experimentation, and migration reference
- Bridges ANT+ messages from connected ANT USB dongles to terminal output, future BLE, or GUI consumers

## Structure
- Includes `discovery.cpp`: core event loop, ANT channel logic, and message dispatch
- Handles channel assignment, extended message decoding, and page-based data interpretation
- Contains initial implementation of event loop, retry logic, and basic device caching

## Build dependencies on Linux
You need to install the following packages on Linux:
```bash
# Update package list
sudo apt update
# C++ standard library types like uint8_t, threads, etc
sudo apt install -y build-essential cmake git pkg-config
# libusb for USB access
sudo apt install -y libusb-1.0-0-dev
# libmosquitto-dev for MQTT support
sudo apt install libmosquitto-dev.
```

## Build dependencies on MacOS (Darwin)
You need to install the following packages on Darwin:
```bash
# Update package list
sudo brew update
# Core build tools and compilers
brew install cmake make pkg-config
# C++ standard library and headers (provided by Xcode Command Line Tools)
xcode-select --install
# libusb for USB device access
brew install libusb
# mosquitto for MQTT support
brew install mosquitto 
```

## ⚙️ Build Instructions
This module is included conditionally from the top-level `CMakeLists.txt`:
```cmake
option(ANTZ_DISCOVERY "Include legacy ant_discovery module" ON)
```
It is default ON, so you can build it by default.

To build (from project root) on MacOS (Darwin):
```sh
cmake -B build/darwin -S . -DANTZ_PLATFORM=darwin -DANTZ_DISCOVERY=ON
cmake --build build/darwin --target ant_discovery
```

To build (from project root) on Linux:
```sh
cmake -B build/linux -S . -DANTZ_PLATFORM=linux -DANTZ_DISCOVERY=ON
cmake --build build/linux --target ant_discovery
```

## 📦 Installing on Raspberry Pi 4

### Quick install from GitHub Releases (Recommended)
Download and install the latest pre-built package directly:

```sh
# Download latest release (replace VERSION with actual version, e.g., v0.1.0)
VERSION=v0.1.0
wget https://github.com/kengu/antz/releases/download/${VERSION}/antz-0.1.0-Linux.deb

# Install the package
sudo dpkg -i antz-0.1.0-Linux.deb

# Fix missing dependencies if needed
sudo apt-get install -f

# Enable and start the service
sudo systemctl enable antz
sudo systemctl start antz

# Monitor logs
sudo journalctl -u antz -f
```

The package includes:
- Binary installed to `/usr/bin/antz`
- Systemd service file at `/lib/systemd/system/antz.service`
- udev rules at `/lib/udev/rules.d/99-antz.rules` (automatic USB permissions)
- Dependencies: `libmosquitto1`, `libusb-1.0-0`

### Building from source on RPI4
If you prefer to build the package yourself:

```sh
# First time setup: install all dependencies
./scripts/configure-rpi4.sh

# Build the package
./scripts/build-rpi4.sh

# Install and run
sudo dpkg -i build-rpi4/antz-*.deb
sudo systemctl enable antz
sudo systemctl start antz
```

The `configure-rpi4.sh` script will:
- Install build tools (cmake, gcc, etc.)
- Install runtime dependencies (libusb, libmosquitto)
- Set up USB permissions for ANT+ dongles
- Create udev rules for automatic device access
- Check for ANT+ USB stick presence

### For developers: Creating releases
Packages are automatically built via GitHub Actions when you push a tag:
```sh
git tag v0.1.0
git push origin v0.1.0
```

The workflow (`.github/workflows/build-rpi4.yml`) runs on self-hosted RPI4 runners and publishes the `.deb` to GitHub Releases.

# Migration Plan
- `ant_discovery` will be migrated to `libs/antz_core`
- Functionality in `discovery.cpp` is being split into modular components:
    - Profile decoding → `libs/core`
    - Data bridging → `libs/bridge`
    - Platform adapters → `libs/platform/<platform>`
- The logic will be reused in structured test apps and firmware (see `apps/`)

This module will be deprecated and removed once all features are ported into modular components.

## License
This file inherits the license from the `antz` project. See root-level `LICENSE` file for details.
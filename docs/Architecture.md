# Architecture Overview: antz – ANT+ Tooling Framework

This document outlines the architectural foundation of the `antz` project — 
a modular, cross-platform framework for working with ANT+ protocols across multiple 
use cases. While a primary application is an ANT+ to BLE bridge for Garmin Astro and 
Alpha dog trackers, the project is designed to support broader functionality such as 
simulation, desktop analysis, and cloud integration. It is located in the `docs/` 
folder of the repository.

## Project Goal

Design and implement a flexible, cross-platform ANT+ processing toolkit that can be 
adapted to a variety of environments:

* Run as a BLE-capable firmware for dongles (e.g., nRF52) to forward Garmin tracker data to smartphones
* Operate as a desktop or daemonized ANT+ listener (e.g., Linux/Darwin with USB stick)
* Optionally connect to the internet to relay ANT+ data to a cloud backend
* Simulate BLE Alpha X devices to make older Astro collars appear in Garmin Explore
* Support ANT+ profile decoding (HRM, Asset Tracker) in a testable, modular, platform-independent way

## Design Requirements

* Must support **ANT+** reception for Garmin **Asset Tracker** and **HRM** profiles
* Must support **BLE Peripheral** mode (where applicable) to expose decoded data
* Must run on **nRF52 hardware**, **Linux**, and **Darwin**, using appropriate SDKs
* Must enable reuse of **decoding logic** across targets via a clean, portable C++ core

## Project Structure

```plaintext
antz/
├── libs/                  # Modular runtime libraries
│   ├── antz_core/         # Platform-agnostic ANT+ core libary
│   │   ├── profiles/      # Parsers for HRM, Asset Tracker pages
│   │   ├── events/        # Device state & change propagation
│   │   └── simulators/    # Test data generators for simulation
│   │
│   ├── antz_bridge/       # Common bridging logic (ANT+ to BLE, JSON, etc.)
│   │   ├── ant_bridge.cpp # Translates device data to output events
│   │   └── ble_output.cpp # Example BLE output handler
│   │
│   └── antz_platform/     # Platform-specific integration with ANT+ SDKs
│       ├── nrf5/          # ANT+ w/BLE via SoftDevice S340 (nRF5 SDK v17.1.0)
│       ├── linux/         # ANT USB stick using DSIFramerANT (Linux SDK 3.8.200 BETA)
│       ├── darwin/        # ANT USB stick using DSIFramerANT (Mac SDK 3.5)
│       └── win/           # ANT USB stick using DSIFramerANT (PC SDK 3.5)
│
├── apps/                  # Concrete products and test targets
│   ├── antz_discvery/     # Monolithic implementation of ANT+ discovery and profile decoding (legacy)
│   ├── antz_brigde_nrf5/  # Firmware for nRF52 BLE dongle (future)
│   ├── antz_test_gui/     # Desktop test harness or visualizer (future)
│   └── antz_emulator/     # BLE simulator for Alpha X spoofing (future)
│
├── sdks/                  # External SDKs (Mac, Linux, Windows)
│   ├── ANT-SDK_Mac.3.5/   # Includes build fixes and CMake support (included in repo)
│   ...                    # Additional SDKs are added here as needed 
│
├── docs/                  # Documentation and specifications
│   ├── Architecture.md    # Outlines the architectural foundation of the `antz` project
│   └── nrf5_platform.md   # Embedded firmware platform guidance
```

Each module plays a distinct role:

* `libs/core/` handles ANT+ profile parsing in a testable, portable way
* `libs/bridge/` connects parsed device data to outputs like BLE or file logs
* `libs/platform/` handles ANT channel setup, packet transport, and SDK interop
* `apps/` provide full builds for firmware, test tools, and simulators
* `sdks/` stores patched external SDKs or local build support
* `docs/` contains architecture, platform, and usage documentation
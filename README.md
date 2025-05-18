# antz

![License](https://img.shields.io/badge/license-BSD%202--Clause-blue.svg)
![CMake](https://img.shields.io/badge/cmake-%3E=3.16-blue)
![Platform](https://img.shields.io/badge/platform-Darwin-lightgrey)

This monorepo contains a modular, cross-platform framework for working with ANT+ protocols. 
While a primary use case is bridging Garmin trackers (such as Astro and Alpha series devices) to BLE, 
the project is designed to support a wider range of applications including simulation, desktop tooling, 
and data forwarding. It provides a portable decoding engine and platform-specific adapters for receiving 
ANT+ messages and connecting them to various outputs, including BLE, GATT, and cloud services.

## ✨ Features

* Decode **Asset Tracker** and **HRM** ANT+ profiles
* Receive ANT+ data from Garmin **Astro** (legacy, ANT+ only) and **Alpha** (ANT+/BLE) devices

## 🚧 Roadmap

* Bridge tracker data to BLE GATT for mobile integration
* Reusable across nRF52 embedded systems, Linux/macOS tools, and test environments
* Modular simulation support for test-driven development
* BLE simulation (e.g., mimicking Alpha X in Garmin Explore)

## 📦 Structure

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

## ✅ Current Platform Focus

* Embedded BLE dongle using **nRF52840** (e.g., Seeed Studio XIAO)
* Built with **nRF5 SDK v17.1.0** and **SoftDevice S340**
* Desktop support for **Linux/macOS** using **DSIFramerANT** (ANT USB stick)
* Optional future Zephyr/NCS-based support via Garmin's ANT stack
* Embedded BLE dongle using **nRF52840** (e.g., Seeed Studio XIAO)
* Built with **nRF5 SDK v17.1.0** and **SoftDevice S340**
* ANT+ support based on Nordic + Garmin profile specifications

## 🛠 Development

After first checkout you need to configure build with `cmake` for 
the platform you are developing code for. If you are developing for 
Mac, you should run the following command from the root of the repo;

```bash
cmake -B build/darwin -S . -DANTZ_PLATFORM=darwin
```

You can compile ANT SDK for this platform with:

```bash
cmake --build build/darwin --target full_sdk_build
```

which builds binaries to `sdks/ANT-SDK_Mac.3.5/Bin`

## Apps 
Each antz app is a concrete use case that uses antz to implement it. 
They can be implemented for a concrete platform or support multiple 
platforms. It all depends on the use case described in the README for 
each of them.

### ant_discovery (legacy)
To support the migration to the new modular structure, the legacy implementation under 
[apps/ant_discovery](apps/ant_discovery/README.md) is temporarily preserved and included as a standalone build target. 
This allows side-by-side builds and comparison during refactoring. If you wish to include it 
in the build, the top-level [CMakeLists.txt](CMakeLists.txt) contains:

```cmake
option(ANTZ_DISCOVERY "Include ant_discovery module" ON)
```

This keeps compatibility while new modules are migrated incrementally. You can use ANT SDK binaries for this platform with:

```bash
cmake --build build/darwin --target ant_discovery
```

If you have an ANT+ USB dongle connected to your development machine, run 
```bash
sh ./ant_discovery/Bin/ant_discovery
```
to start discovery of HRM and Asset Tracker devices near you. 

## 🐾 Use Case

**Antz** is especially relevant for Nordic hunting communities, where older **Garmin Astro + DC 50** collars are still in use and **cannot communicate via BLE**. This project bridges that gap:

* Adds mobile and cloud support to Astro-series devices
* Extends the life and utility of legacy Garmin tracking gear
* Enables mixed Astro + Alpha setups from one phone

## 📜 License

Dual-licensed:

* `core/` and `apps/`: BSD 2-Clause (by this project)
* Nordic SDK and Garmin ANT+ code: Subject to their respective licenses

## 🔗 Contributing

GitHub: [https://github.com/kengu/antz](https://github.com/kengu/antz). PRs and feedback are welcome!

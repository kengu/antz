# ANT+ development with MacOS

![License](https://img.shields.io/badge/license-BSD%202--Clause-blue.svg)
![CMake](https://img.shields.io/badge/cmake-%3E=3.16-blue)
![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)


Welcome to the **Antz** monorepo! This repository contains multiple projects, with the main focus being the **ANT-SDK\_Mac.3.5** project for macOS development, specifically for handling ANT+ communication protocols.

## Directory Structure

```text
antz/
│
├── ANT-SDK_Mac.3.5/          # Main project for ANT+ SDK on macOS
│   ├── Bin/*                 # Ant+ binaries to include in other projects 
│   ├── Bin/demo_lib          # Statically linked demo executable 
│   ├── Bin/demo_dylib        # Dynamically linked demo executable 
│   ├── CMakeLists.txt        # CMake build configuration
│   ├── README.md             # Project-specific documentation (optional)
│   └── ...                   # Source code, libraries, binaries, etc.
├── ant_discovery/            # Test project for ant discovery methods
│   ├── include/*             # Discovery header files
│   ├── src/*                 # Discovery implementations
│   ├── Bin/*                 # Ant+ binaries to include in other projects 
│   ├── CMakeLists.txt        # CMake build configuration for ant discovery binary
│
└── README.md                 # This file
```

## ANT-SDK\_Mac.3.5

The **ANT-SDK\_Mac.3.5** folder contains the source code and build setup for the ANT+ Software Development Kit (SDK) tailored for macOS. This SDK provides support for integrating ANT+ communication in macOS applications, including device handling, ANT+ protocol support, and related utilities.

### Prerequisites

Ensure you have the following dependencies installed:

* **CMake** (version 3.16 or higher)
* **Xcode** with command-line tools
* **clang++** with C++14 support

### Building the Project

From the `ANT-SDK_Mac.3.5` directory, run:

```bash
cmake -S . -B build
cmake --build build --target full_build
```

This will compile both the static and shared libraries, and the demo executables.

### Building Individual Targets

To build selectively:

```bash
cmake --build build --target static_build   # For demo_lib
cmake --build build --target shared_build   # For demo_dylib
cmake --build build --target demo_dll       # For DLL demo
```

### Installing Binaries

To install all targets to the `./Bin` folder:

```bash
cmake --build build --target install_local
```

### Cleaning the Build

To remove the build directory:

```bash
cmake --build build --target clean_build
```

### Available Build Targets

* `demo_lib`: Builds demo linked statically with libantbase.a
* `demo_dylib`: Builds demo linked dynamically with libantbase\_shared.dylib
* `demo_dll`: Builds demo using libant.dylib (from DEMO\_DLL)
* `antbase`: Static library (libantbase.a)
* `antbase_shared`: Shared library (libantbase\_shared.dylib)
* `libant`: Shared library (libant.dylib)

### Utility Targets

* `static_build`: Builds demo\_lib
* `shared_build`: Builds demo\_dylib
* `full_build`: Builds all demos and libraries
* `clean_build`: Deletes build output
* `install_local`: Installs build output to `./Bin`

## Executables

* `demo_lib` statically links against `libantbase.a`
* `demo_dylib` dynamically links against `libantbase_shared.dylib`
* `demo_dll` dynamically links against `libant.dylib`

## Ant+ discovery project

Simple test project for algorithms for discovery of ant devices

## License

This repository is organized as a monorepo containing both proprietary and third-party code.

* **ANT-SDK\_Mac.3.5** is distributed under the [ANT+ Shared Source License](https://www.thisisant.com/developer/ant/licensing) by Dynastream Innovations, Inc. See [ANT-SDK\_Mac.3.5/License.txt](ANT-SDK_Mac.3.5/License.txt) for details.
* All other code in this repository is distributed under the terms of the **BSD 2-Clause License** unless stated otherwise.

Please ensure that you comply with the specific license terms when using or redistributing components from this repository.


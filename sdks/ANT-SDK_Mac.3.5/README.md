# ANT+ development with MacOS

![CMake](https://img.shields.io/badge/cmake-%3E=3.16-blue)
![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)

**ANT-SDK\_Mac.3.5** folder contains the source code and build setup for the ANT+ Software Development Kit (SDK) tailored for macOS. This SDK provides support for integrating ANT+ communication in macOS applications, including device handling, ANT+ protocol support, and related utilities.

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

## Notes

The SDK uses libusb under the hood — ensure it's not blocked by macOS security (e.g., System Extensions)
You may need to run the final binary with elevated privileges (sudo) depending on device permissions
This SDK is considered legacy and is provided as-is by Garmin at 
[thisisant.com](https://www.thisisant.com/business/go-ant/ant-brand)

## License

The Garmin ANT SDK for macOS is released under its own license terms. 
Please ensure that you comply with the specific license terms when using or 
redistributing components from this repository.

## Source

This project is a branch from the official 
[MAC ANT SDK v3.5](https://www.thisisant.com/resources/ant-macosx-library-package-with-source-code/).

You need to register as an ANT+ adopter to access these and other resources.

All official SDK downloads is available here https://www.thisisant.com/developer/resources/downloads.

The SDK have the following content
* `include/` — C/C++ header files
* `src/` — Source implementation for macOS USB access
* `Build/` — Output folder after cmake && make (the binaries)
* `README.md` — This file

For integration with antz, see the main project: https://github.com/kengu/antz.
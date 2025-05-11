# nRF5 Platform Notes

This document outlines the rationale, tooling, and considerations specific to using the **nRF5 SDK v17.1.0** with **SoftDevice S340** as the embedded platform implementation for the `antz` project.

---

## Why Use the nRF5 SDK + S340?

The nRF5 SDK provides a mature and robust foundation for embedded development on nRF52840 devices. It includes:

* Support for **ANT+ and BLE dual-mode operation** via SoftDevice S340
* Fully featured ANT+ profile implementations (e.g., HRM)
* ANT+ simulators for test-driven development
* Tight integration with **Segger Embedded Studio (SES)**
* Ready-to-use device drivers and board definitions

---

## Pros

* ✅ Stable and widely tested in the field
* ✅ Turnkey support for ANT+ with SoftDevice
* ✅ Includes ANT+ profile logic and page encoders
* ✅ Efficient for bare-metal embedded targets

## Cons

* ❌ SDK is in **maintenance mode** (no new features)
* ❌ Not based on Zephyr or RTOS model
* ❌ Requires proprietary SoftDevice stack

---

## SoftDevice: S340

S340 is the Nordic-provided combined SoftDevice that enables both:

* BLE peripheral and central roles
* ANT and ANT+ channel roles

It must be flashed separately as a precompiled `.hex` before your application image.

```sh
nrfjprog --program s340_nrf52_6.1.1_softdevice.hex --verify --reset
```

---

## ANT+ Profile Support

The following ANT+ profiles are directly supported in the SDK:

* Heart Rate Monitor (HRM)
* Bicycle Speed/Cadence
* Bicycle Power
* Stride-based Speed and Distance (SDM)

The `antz` project currently uses nRF5 SDK for:

* Heart Rate Monitor (via `ant_hrm_rx`)

---

## nRF5 SDK in Antz
This nRF5 SDK serves as the glue between [antz_core](../libs/antz_core) 
logic and the underlying SoftDevice and ANT+ infrastructure.

```plaintext
antz/
└── libs/
    └── platform/
        └── nrf5/
            ├── main.cpp             # Entry point
            ├── ant_adapter.cpp      # SoftDevice ANT integration
            ├── ble_bridge.cpp       # BLE GATT advertiser
            └── ...
```

---

## Development Environment

* Segger Embedded Studio (Nordic edition)
* nRF5 SDK v17.1.0
* SoftDevice S340 v6.1.1
* nrfjprog + command line tools

---

## Alternatives Considered

This section evaluates alternative approaches to using the nRF5 SDK with SoftDevice S340, including the emerging Garmin-provided ANT stack for Zephyr/NCS environments.

### 1. ANT Support in nRF Connect SDK (Garmin Stack)

Garmin has released an ANT protocol stack for **nRF Connect SDK (NCS)**, enabling ANT+ support inside Zephyr-based environments. This allows BLE + Thread + ANT+ integration within a single RTOS application.

See: [Garmin ANT Protocol Stack for nRF Connect SDK – Getting Started](https://www.thisisant.com/APIassets/ANTnRFConnectDoc/doc/getting_started.html)

**Pros:**

* ✅ Enables BLE + Thread + Matter in one RTOS app
* ✅ More modern toolchain and long-term roadmap
* ✅ Allows long-term consolidation into a single Zephyr-based codebase

**Cons:**

* ❌ More complex integration for ANT+ via Garmin's layer
* ❌ Smaller community, harder debugging
* ❌ Not officially supported by Nordic
* ❌ Limited ecosystem maturity and real-world testing

### 2. Custom ANT stack

Developing a fully custom ANT or ANT+ stack from scratch is considered impractical for several reasons:

* 🔒 The ANT protocol and especially the ANT+ device profiles are **proprietary and closed-source**, maintained by Garmin (via Dynastream).
* 🧩 ANT+ profiles define precise message formats, state transitions, page rotations, and behavior — most of which are not publicly specified.
* 🛠 Building a compliant ANT or ANT+ stack would require **reverse-engineering**, which violates licensing terms and may not be interoperable.
* 📶 RF PHY layer and channel timing behavior is tightly specified and relies on chipset-level integration (e.g., Nordic SoftDevice, nRF chips).
* 🧪 Lack of official certification or test tools would make compatibility with other ANT+ devices uncertain.

For these reasons, using **vendor-supplied ANT stacks** (e.g., Nordic SoftDevice or Garmin’s Zephyr integration) is the only realistic option for ANT+ development today.

---

## Summary

The nRF5 SDK is the most practical and stable way to build an ANT+ to BLE embedded bridge today. While long-term migration to NCS + Garmin ANT stack may be possible, this path ensures rapid development and hardware reliability for the first release of `antz`.

This platform implementation is one of several platform targets under the `antz` modular architecture, which also includes Linux, macOS, and other integration layers through platform-specific adapters.

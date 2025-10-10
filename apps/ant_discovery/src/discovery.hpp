#pragma once

#include "types.h"
#include <usb_device_handle.hpp>

namespace ant {
    bool initialize(const USBDevice& pDevice, UCHAR ucDeviceNumber);
    bool startDiscovery();
    void runEventLoop();
    void cleanup();
}

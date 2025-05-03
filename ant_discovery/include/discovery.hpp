#pragma once

#include <cstddef>
#include "types.h"

namespace ant {
    bool initialize();
    bool initialize(UCHAR ucDeviceNumber);
    bool startDiscovery();
    void runEventLoop();
    void cleanup();
}

//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#ifndef DISCOVERY_MACHINE_H
#define DISCOVERY_MACHINE_H

#include "profile_discovery.h"
#include "dsi_framer_ant.hpp"
#include <memory>
#include <vector>

#include "dsi_serial_generic.hpp"

class DiscoveryMachine {
public:
    DiscoveryMachine() = default;

    void registerProfile(std::unique_ptr<ProfileDiscovery> profile);
    bool initialize(UCHAR ucDeviceNumber);
    bool startDiscovery() const;
    void runEventLoop();
    void cleanup();

public:
    DSIFramerANT* pclANT{nullptr};

private:
    bool searching_ = false;
    DSISerialGeneric* pclSerial{nullptr};
    std::vector<std::unique_ptr<ProfileDiscovery>> profiles;

};

#endif // DISCOVERY_MACHINE_H


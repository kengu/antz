#include "discovery_machine.h"
#include <iostream>

#include <csignal>

#include "asset_tracker_discovery.h"
#include "hrm_discovery.h"
#include "types.h"

DiscoveryMachine machine;

static void onSignal(int) {
    std::cout << "CTRL+C detected, cleanup..." << std::endl;
    machine.cleanup();
    std::exit(0);
}

int main() {
    std::signal(SIGINT, onSignal);
    std::cout << "Starting ANT+ discovery..." << std::endl;
    UCHAR deviceNumber = 0xFF;
    if (deviceNumber == 0xFF) {
        std::cout << "USB Device number? " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        deviceNumber = line.empty() ? 1 : static_cast<UCHAR>(std::stoul(line));
    }

    if (!machine.initialize(deviceNumber)) {
        std::cerr << "[Machine] ANT initialization failed." << std::endl;
        return 1;
    }

    // Register profile discoveries
    machine.registerProfile(std::make_unique<HRMDiscovery>(machine.pclANT));
    machine.registerProfile(std::make_unique<AssetTrackerDiscovery>(machine.pclANT));

    if (!machine.startDiscovery()) {
        std::cerr << "[Machine] Failed to start ANT+ discovery." << std::endl;
        return 2;
    }
    // Now enter the run loop to process incoming messages
    machine.runEventLoop();

    return 0;
}

/*

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
*/

#include "discovery.hpp"
#include <iostream>

#include <csignal>

#include "types.h"

#include <execinfo.h>
#include <stdexcept>

void print_stacktrace() {
    constexpr int max_frames = 64;
    void* frames[max_frames];
    const int frame_count = backtrace(frames, max_frames);
    char** symbols = backtrace_symbols(frames, frame_count);

    std::cerr << "Stack trace:\n";
    for (int i = 0; i < frame_count; ++i) {
        std::cerr << symbols[i] << std::endl;
    }

    free(symbols);
}


static void onSignal(int) {
    std::cout << "CTRL+C detected, cleanup..." << std::endl;
    ant::cleanup();
    std::exit(0);
}

int main() {
    std::signal(SIGINT, onSignal);
    try {

        UCHAR deviceNumber = 0xFF;
        if (deviceNumber == 0xFF) {
            std::cout << "USB Device number? " << std::flush;
            std::string line;
            std::getline(std::cin, line);
            deviceNumber = line.empty() ? 1 : static_cast<UCHAR>(std::stoul(line));
        }
        if (!ant::initialize(deviceNumber)) {
            std::cerr << "ANT initialization failed." << std::endl;
            return 1;
        }
        if (!ant::startDiscovery()) {
            std::cerr << "Failed to start ANT+ discovery." << std::endl;
            return 2;
        }

        // Now enter the run loop to process incoming messages
        ant::runEventLoop();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        print_stacktrace();
        onSignal(0);
        return 1;
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
        print_stacktrace();
        onSignal(0);
        return 1;
    }
}


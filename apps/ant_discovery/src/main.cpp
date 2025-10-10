#include "discovery.hpp"
#include "types.h"
#include <execinfo.h>
#include "usb_checker.cpp"

#include <iostream>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <usb_device_handle.hpp>

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
    std::signal(SIGTERM, onSignal);
    try {

        if (!check_usb_is_available())
        {
            std::cout << "USB is not available." << std::endl;
            return 1;
        }

        // Get all ANT devices (detected via USB)
        const ANTDeviceList devList = USBDeviceHandle::GetAllDevices();
        if (devList.GetSize() == 0)
        {
            std::cerr << "No ANT devices found!" << std::endl;
            return 1;
        }

        std::cout << "Number of ANT devices found: " << devList.GetSize() << std::endl;
        for (unsigned int i = 0; i < devList.GetSize(); ++i) {
            const USBDevice& dev = *devList[i];
            std::cout << "[" << i << "]: Vendor ID: 0x" << std::hex << dev.GetVid()
                      << ", Product ID: 0x" << std::hex << dev.GetPid() << std::dec;

            // Serial string
            UCHAR serialBuf[128] = {0};
            if (dev.GetSerialString(serialBuf, sizeof(serialBuf) - 1)) {
                std::cout << ", Serial: " << serialBuf;
            }

            // Product description
            UCHAR prodBuf[128] = {0};
            if (dev.GetProductDescription(prodBuf, sizeof(prodBuf) - 1)) {
                std::cout << ", Product: " << prodBuf;
            }
            std::cout << std::endl;

        }

        UCHAR deviceNumber = 0xFF;
        if (deviceNumber == 0xFF) {
            std::cout << "USB Device number? " << std::flush;
            std::string line;
            std::getline(std::cin, line);
            deviceNumber = line.empty() ? 0 : static_cast<UCHAR>(std::stoul(line));
            if (deviceNumber >= devList.GetSize())
            {
                std::cerr << "USB Device ["+ std::to_string(deviceNumber) + "] not found " << std::endl;
                return 1;
            }

            std::cout << "USB Device [" << static_cast<unsigned int>(deviceNumber) << "]" << std::endl;
        }

        if (!ant::initialize(*devList[deviceNumber], deviceNumber)) {
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
        std::cerr << "Unknown Exception: " << e.what() << std::endl;
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
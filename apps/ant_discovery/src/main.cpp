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
    ::_exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    // Default to 0 unless overridden by -d/--device
    double meters = 1;
    std::string mqttCnn;
    UCHAR  deviceNumber = 0;
    auto deviceNotGiven = true;

    // Default to false/Info unless overridden by -v/--verbose
    auto verbose = false;
    auto logLevel = ant::LogLevel::Info;

    // Default to text unless overridden by -f/--format
    auto outputFormat = ant::OutputFormat::Text;

    // Simple CLI arg parsing
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; (arg == "-f" || arg == "--format") && i + 1 < argc) {
            std::string fmt = argv[++i];
            std::ranges::transform(
                fmt,
                fmt.begin(),
                [](const unsigned char c){ return std::tolower(c); }
            );
            if (fmt == "json")      outputFormat = ant::OutputFormat::JSON;
            else if (fmt == "csv")  outputFormat = ant::OutputFormat::CSV;
            else if (fmt == "text") outputFormat = ant::OutputFormat::Text;
            else {
                std::cerr << "Unknown format: " << fmt
                          << " (expected text, json, or csv)" << std::endl;
                return 1;
            }
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "-d" || arg == "--device") {
            deviceNumber = static_cast<UCHAR>(std::stoul(argv[++i]));
            deviceNotGiven = false;
        }
        else if (arg == "-e") {
            try {
                meters = std::stod(argv[++i]);
            } catch (const std::exception& _) {
                std::cerr << "Invalid value for " << arg << ": " << argv[i] << "\n";
                return 2;
            }
        }
        // --mqtt "mqtt://user:pass@broker.example.com:1883/ant?retain=1&qos=1"
        else if (arg == "-m" || arg == "--mqtt") {
            if (i + 1 < argc) {
                mqttCnn = argv[++i];
            }
        }
        else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-f|--format text|json|csv] [-e <meters>]  [-m|--mqtt <cnn>] [-v|--verbose]\n";
            return 0;
        }
    }

    try {

        if (!check_usb_is_available()) {
            std::cout << "USB is not available." << std::endl;
            return 1;
        }


        // Get all ANT devices (detected via USB)
        const ANTDeviceList devList = USBDeviceHandle::GetAllDevices();
        if (devList.GetSize() == 0) {
            std::cerr << "No ANT devices found!" << std::endl;
            return 1;
        }

        if (deviceNotGiven) {
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

            if (deviceNumber == 0xFF) {
                std::cout << "USB Device number? " << std::flush;
                std::string line;
                std::getline(std::cin, line);
                deviceNumber = line.empty() ? deviceNumber : static_cast<UCHAR>(std::stoul(line));

                std::cout << "USB Device [" << static_cast<unsigned int>(deviceNumber) << "]" << std::endl;
            }
        }

        if (deviceNumber >= devList.GetSize()){
            std::cerr << "USB Device ["+ std::to_string(deviceNumber) + "] not found " << std::endl;
            return 1;
        }

        if (verbose) {
            logLevel = outputFormat == ant::OutputFormat::Text
                ? ant::LogLevel::Fine
                : ant::LogLevel::Info;
        } else {
            logLevel = outputFormat == ant::OutputFormat::Text
                ? ant::LogLevel::Info
                : ant::LogLevel::Error;
        }

        ant::setLogLevel(logLevel);
        ant::setFormat(outputFormat);
        ant::setMqtt(mqttCnn);
        ant::setEpsLatLng(meters);

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
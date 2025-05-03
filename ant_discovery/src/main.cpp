#include "discovery.hpp"
#include <iostream>

#include <csignal>
static void onSignal(int) {
    std::cout << "CTRL+C detected, cleanup..." << std::endl;
    ant::cleanup();
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
}

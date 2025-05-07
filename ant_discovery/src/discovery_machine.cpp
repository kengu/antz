//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//
// DiscoveryMachine.cpp

#include "dsi_framer_ant.hpp"

#include <thread>
#include <string>
#include <map>

#include "logging.h"
#include "discovery_machine.h"
#include "profile_discovery.h"

bool DiscoveryMachine::initialize(const UCHAR ucDeviceNumber) {
    info("ANT initialization started...");

    pclSerial = new DSISerialGeneric();
    if (!pclSerial->Init(50000, ucDeviceNumber)) {
        std::ostringstream oss;
        oss << "Failed to open USB port " << static_cast<int>(ucDeviceNumber);
        info(oss.str());
        return false;
    }

    pclANT = new DSIFramerANT(pclSerial);
    pclSerial->SetCallback(pclANT);
    if (!pclANT->Init()) {
        info("Framer Init failed");
        return false;
    }
    if (!pclSerial->Open()) {
        info("Serial Open failed");
        return false;
    }

    pclANT->ResetSystem();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (true) {
        const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);
        if (length > 0 && length != DSI_FRAMER_TIMEDOUT) {
            ANT_MESSAGE msg;
            pclANT->GetMessage(&msg);
            std::ostringstream oss;
            oss << "Message ID was " << static_cast<int>(msg.ucMessageID);
            info(oss.str());
            if (msg.ucMessageID == MESG_STARTUP_MESG_ID) break;
        }
    }
    return true;
}

void DiscoveryMachine::registerProfile(std::unique_ptr<ProfileDiscovery> profile) {
    profiles.push_back(std::move(profile));
}

bool DiscoveryMachine::startDiscovery() const {
    info("[Machine] Start ANT discovery...");

    if (!pclANT->SetNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, MESSAGE_TIMEOUT)) {
        severe("[Machine] SetNetworkKey failed");
        return false;
    }
    return true;
}

void DiscoveryMachine::runEventLoop() {
    searching_ = true;
    info("[Machine] Starting event loop...");
    auto lastMessageTime = std::chrono::steady_clock::now();
    while (searching_) {
        auto now = std::chrono::steady_clock::now();
        const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT);

        if (length == DSI_FRAMER_TIMEDOUT || length == 0) {
            const auto secondsSinceLast = std::chrono::duration_cast<std::chrono::seconds>(now - lastMessageTime).count();
            if (secondsSinceLast > 5) {
                std::ostringstream oss;
                oss << "[Machine] No ANT messages received in the last "
                    << secondsSinceLast
                    << " seconds";
                info(oss.str());
                lastMessageTime = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ANT_MESSAGE msg;
        pclANT->GetMessage(&msg);

        const UCHAR ucMessageID = msg.ucMessageID;

        if (ucMessageID == MESG_BROADCAST_DATA_ID || ucMessageID == MESG_EXT_BROADCAST_DATA_ID) {
            lastMessageTime = now;
            ExtendedInfo ext = {};

            if (length >= 13) {
                const uint8_t flags = msg.aucData[length - 1];
                const uint8_t* trailer = msg.aucData + (length - 1 - trailerLengthGuess(flags));
                ext = parseExtendedInfo(trailer, flags);
            };

            // Flag for tracking whether a profile has accepted the message
            bool messageHandled = false;

            for (const auto& profile : profiles) {
                if (profile->accept(msg, length, ext)) {
                    profile->handleMessage(msg, length, ext);
                    messageHandled = true;
                    break; // Once a profile handles the message, stop processing further
                }
            }

            if (!messageHandled) {
                // Optionally log if no profile handled the message
                info("[Machine] No profile accepted this message.");
            }
        }
    }
}

void DiscoveryMachine::cleanup() {
    info("[Machine] Stopping event loop...");
    searching_ = false;
    for (const auto& profile : profiles) {
        profile->closeChannel();
    }
    if (pclANT) {
        std::ostringstream oss;
        oss << "[Machine] Resetting ANT system...";
        info(oss.str());
        delete pclANT;
        pclANT = nullptr;
    }
    if (pclSerial) {
        pclSerial->Close();
        delete pclSerial;
        pclSerial = nullptr;
    }
    std::ostringstream oss;
    oss << "[Machine] Event loop stopped.";
    info(oss.str());
}


//
// Created by Kenneth Gulbrandsøy on 05/05/2025.
//

#ifndef PROFILE_DISCOVERY_H
#define PROFILE_DISCOVERY_H

#include "ant_device.h"
#include "dsi_framer_ant.hpp"  // for ANT_MESSAGE

#include "ant_profiles.h"
#include "logging.h"
#include "ant_constants.h"

class ProfileDiscovery {
public:
    explicit ProfileDiscovery(
        const AntProfile profileType,
        DSIFramerANT* ant,
        const UCHAR channel,
        const UCHAR channelType,
        const USHORT channelPeriod,
        const UCHAR deviceType,
        const ULONG searchTimeout,
        const UCHAR channelRFFrequency = USER_CHANNEL_RF_FREQ
        ) : channel_(channel),
            deviceType_(deviceType),
            channelType_(channelType),
            channelPeriod_(channelPeriod),
            searchTimeout_(searchTimeout),
            channelRFFrequency_(channelRFFrequency),
            ant_(ant), profileType(profileType) {}

    virtual ~ProfileDiscovery() = default;

    // Called whenever a message is received on this profile's channel
    virtual bool accept(const ANT_MESSAGE& msg, uint8_t length, ExtendedInfo& ext) = 0;

    // Called whenever a message is received on this profile's channel
    virtual void handleMessage(const ANT_MESSAGE& message, uint8_t length, ExtendedInfo& ext) = 0;

    // Called once to configure channel
    bool setupChannel() const {
        // Sample setup logic for the profile channel, you can add more steps as needed.
        if (!ant_->AssignChannel(channel_, channelType_, USER_NETWORK_NUM, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to assign channel");
            return false;
        }

        if (!ant_->SetChannelID(channel_, 0, deviceType_, TRANSMISSION_TYPE_WILDCARD, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to set channel ID");
            return false;
        }

        if (!ant_->SetChannelPeriod(channel_, channelPeriod_, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to set channel period");
            return false;
        }

        if (!ant_->SetChannelRFFrequency(channel_, channelRFFrequency_, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to set channel RF frequency");
            return false;
        }

        if (!ant_->SetChannelSearchTimeout(channel_, searchTimeout_, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to set channel search timeout");
            return false;
        }

        if (!ant_->OpenChannel(channel_, MESSAGE_TIMEOUT)) {
            severe("[Profile] Failed to open channel");
            return false;
        }

        return true;
    }

    void closeChannel() const {
        if (ant_) {
            std::ostringstream oss;
            oss << "[Profile] Closing ANT channel " << std::dec << channel_ << "...";
            info(oss.str());
            ant_->CloseChannel(channel_);
            ant_->UnAssignChannel(channel_);
        }
    }

    // Channel number this profile is assigned to
    virtual uint8_t getChannel() const {
        return channel_;
    }

    // Get profile type (AntProfile) for logger or other purposes
    AntProfile getProfileType() const {
        return profileType;
    }

protected:
    UCHAR channel_;
    UCHAR deviceType_;
    UCHAR channelType_;
    USHORT channelPeriod_;
    ULONG searchTimeout_;
    UCHAR channelRFFrequency_;
    DSIFramerANT* ant_;
    AntProfile profileType;
};

#endif // PROFILE_DISCOVERY_H

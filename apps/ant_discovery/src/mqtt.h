#pragma once
#include <regex>
#include <sstream>
#include <iostream>
#include <chrono>
#include <mosquitto.h>
#include "logging.h"

struct MqttConfig {
    std::string host = "localhost";
    int         port = 1883;
    std::string client_id = "ant_discovery";
    std::string username;
    std::string password;
    int         keepalive = 30;
    int         qos = 1;         // 0,1,2
    bool        retain = false;
    bool        enabled = false;
    std::string topic = "ant"; // e.g. ant/asset/<devId>/...
    int         max_retry_attempts = 100; // Skip messages after this many failures
    int         reconnect_delay_ms = 5000; // Wait before attempting reconnect
};

inline bool parseMqttConnectionString(const std::string& uri, MqttConfig& cfg) {
    std::regex re(R"(^(mqtts?)://(?:([^:@]+)(?::([^@]+))?@)?([^:/?#]+)(?::(\d+))?(?:/([^?#]*))?(?:\?([^#]*))?$)");
    std::smatch m;
    if (!std::regex_match(uri, m, re)) {
        return false;
    }

    cfg.enabled = true;
    const bool secure = (m[1] == "mqtts");
    cfg.host = m[4];
    cfg.port = m[5].matched ? std::stoi(m[5]) : (secure ? 8883 : 1883);
    cfg.username = m[2];
    cfg.password = m[3];
    cfg.topic = m[6].matched ? m[6].str() : "ant";

    // Parse query parameters
    if (m[7].matched) {
        std::istringstream q(m[7]);
        std::string kv;
        while (std::getline(q, kv, '&')) {
            const auto pos = kv.find('=');
            if (pos == std::string::npos) continue;
            const auto key = kv.substr(0, pos);
            const auto val = kv.substr(pos + 1);
            if (key == "qos") cfg.qos = std::stoi(val);
            else if (key == "retain") cfg.retain = (val == "1" || val == "true");
            else if (key == "keepalive") cfg.keepalive = std::stoi(val);
        }
    }

    return true;
}

class MqttPublisher {
public:
    explicit MqttPublisher() = default;
    ~MqttPublisher() { stop(); }

    bool start(const MqttConfig& cfg) {
        cfg_ = cfg;
        if (!cfg_.enabled) return true;

        mosquitto_lib_init();
        mosq_ = mosquitto_new(cfg_.client_id.c_str(), true, nullptr);
        if (!mosq_) {
            ant::error("MQTT: Failed to create client");
            return false;
        }

        // Set up callbacks for connection monitoring
        mosquitto_connect_callback_set(mosq_, on_connect);
        mosquitto_disconnect_callback_set(mosq_, on_disconnect);

        if (!cfg_.username.empty()) {
            mosquitto_username_pw_set(
                mosq_, cfg_.username.c_str(),
                cfg_.password.empty() ? nullptr : cfg_.password.c_str()
            );
        }

        // Enable automatic reconnection
        mosquitto_reconnect_delay_set(mosq_, 1, cfg_.reconnect_delay_ms / 1000, false);

        if (mosquitto_connect(mosq_, cfg_.host.c_str(), cfg_.port, cfg_.keepalive) != MOSQ_ERR_SUCCESS){
            ant::error("MQTT: Failed to connect to " + cfg_.host + ":" + std::to_string(cfg_.port));
            return false;
        }

        // Start internal network loop (threaded)
        if (mosquitto_loop_start(mosq_) != MOSQ_ERR_SUCCESS){
            ant::error("MQTT: Failed to start loop");
            return false;
        }

        connected_ = true;
        ant::info("MQTT: Connecting to " + cfg_.host + ":" + std::to_string(cfg_.port));
        return true;
    }

    void stop() {
        if (!mosq_) return;
        mosquitto_loop_stop(mosq_, true);
        mosquitto_disconnect(mosq_);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        mosquitto_lib_cleanup();
        ant::info("MQTT: Stopped");
    }

    [[nodiscard]] bool publish(const std::string& topic, const std::string& payload)
    {
        if (!cfg_.enabled || !mosq_) return false;

        // If we're in backoff mode, skip messages
        if (failed_attempts_ >= cfg_.max_retry_attempts) {
            messages_skipped_++;
            const int remaining = 100 - messages_skipped_;

            if (messages_skipped_ == 1) {
                ant::warn("MQTT: Backoff mode - skipping next 100 messages before retry");
            } else if (messages_skipped_ % 25 == 0) {
                ant::warn("MQTT: Skipped " + std::to_string(messages_skipped_) + "/100, " +
                          std::to_string(remaining) + " more before reconnect");
            }

            // Try to reconnect after skipping enough messages
            if (messages_skipped_ >= 100) {
                ant::info("MQTT: Reconnecting after 100 skipped messages...");
                attemptReconnect();
                messages_skipped_ = 0;
                failed_attempts_ = 0;
            }
            return false;
        }

        const int rc = mosquitto_publish(
            mosq_, nullptr, topic.c_str(),
            static_cast<int>(payload.size()),
            payload.data(),
            cfg_.qos, cfg_.retain
        );

        if (rc == MOSQ_ERR_SUCCESS) {
            // Reset failure counter on success
            if (failed_attempts_ > 0) {
                ant::info("MQTT: Recovered after " + std::to_string(failed_attempts_) + " failures");
                failed_attempts_ = 0;
                messages_skipped_ = 0;
            }
            return true;
        }

        // Handle publish failure
        failed_attempts_++;

        if (failed_attempts_ == 1) {
            ant::error("MQTT: Publish failed - " + std::string(mosquitto_strerror(rc)));
        } else if (failed_attempts_ == cfg_.max_retry_attempts) {
            ant::error("MQTT: " + std::to_string(failed_attempts_) + " failures, entering backoff mode");
        } else if (failed_attempts_ % 20 == 0) {
            ant::error("MQTT: " + std::to_string(failed_attempts_) + " consecutive failures");
        }

        // Try to reconnect if we detect connection issues
        if (rc == MOSQ_ERR_NO_CONN || rc == MOSQ_ERR_CONN_LOST) {
            ant::warn("MQTT: Connection lost, reconnecting...");
            attemptReconnect();
        }

        return false;
    }

    [[nodiscard]] const MqttConfig& config() const { return cfg_; }
    [[nodiscard]] bool isConnected() const { return connected_; }
    [[nodiscard]] int failedAttempts() const { return failed_attempts_; }

private:
    void attemptReconnect() {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_reconnect_attempt_ < std::chrono::milliseconds(cfg_.reconnect_delay_ms)) {
            return; // Don't reconnect too frequently
        }
        last_reconnect_attempt_ = now;

        ant::info("MQTT: Reconnecting to " + cfg_.host + ":" + std::to_string(cfg_.port));
        const int rc = mosquitto_reconnect(mosq_);
        if (rc != MOSQ_ERR_SUCCESS) {
            ant::error("MQTT: Reconnect failed - " + std::string(mosquitto_strerror(rc)));
        }
    }

    static void on_connect(struct mosquitto*, void*, int rc) {
        if (rc == 0) {
            ant::info("MQTT: Connected");
        } else {
            ant::error("MQTT: Connect failed, code " + std::to_string(rc));
        }
    }

    static void on_disconnect(struct mosquitto*, void*, int rc) {
        if (rc == 0) {
            ant::info("MQTT: Disconnected");
        } else {
            ant::warn("MQTT: Unexpected disconnect (code " + std::to_string(rc) + "), auto-reconnecting");
        }
    }

    MqttConfig cfg_{};
    mosquitto* mosq_ = nullptr;
    mutable int failed_attempts_ = 0;
    mutable int messages_skipped_ = 0;
    mutable bool connected_ = false;
    mutable std::chrono::steady_clock::time_point last_reconnect_attempt_{};
};
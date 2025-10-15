#pragma once
#include <regex>
#include <sstream>
#include <iostream>
#include <mosquitto.h>

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
};

inline bool parseMqttConnectionString(const std::string& uri, MqttConfig& cfg) {
    std::regex re(R"(^(mqtts?)://(?:([^:@]+)(?::([^@]+))?@)?([^:/?#]+)(?::(\d+))?(?:/([^?#]*))?(?:\?([^#]*))?$)");
    std::smatch m;
    if (!std::regex_match(uri, m, re)) {
        std::cerr << "Invalid MQTT connection string: " << uri << std::endl;
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
        if (!mosq_) return false;

        if (!cfg_.username.empty()) {
            mosquitto_username_pw_set(
                mosq_, cfg_.username.c_str(),
                cfg_.password.empty() ? nullptr : cfg_.password.c_str()
            );
        }
        if (mosquitto_connect(mosq_, cfg_.host.c_str(), cfg_.port, cfg_.keepalive) != MOSQ_ERR_SUCCESS){
            return false;
        }

        // Start internal network loop (threaded)
        if (mosquitto_loop_start(mosq_) != MOSQ_ERR_SUCCESS){
            return false;
        }
        return true;
    }

    void stop() {
        if (!mosq_) return;
        mosquitto_loop_stop(mosq_, true);
        mosquitto_disconnect(mosq_);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        mosquitto_lib_cleanup();
    }

    [[nodiscard]] bool publish(const std::string& topic, const std::string& payload) const
    {
        if (!cfg_.enabled || !mosq_) return false;
        const int rc = mosquitto_publish(
            mosq_, nullptr, topic.c_str(),
            static_cast<int>(payload.size()),
            payload.data(),
            cfg_.qos, cfg_.retain
        );
        return rc == MOSQ_ERR_SUCCESS;
    }

    [[nodiscard]] const MqttConfig& config() const { return cfg_; }

private:
    MqttConfig cfg_{};
    mosquitto* mosq_ = nullptr;
};
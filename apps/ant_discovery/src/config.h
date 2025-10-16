#pragma once
// Header-only, dependency-light persistence helpers for paired ANT+ channels.
// Stores paired channels (concrete Device#, DeviceType, TxType) in CSV and
// reloads them on startup.
//
// Default path:
// - Linux/macOS: ~/.config/antz/paired_channels.csv (or $XDG_CONFIG_HOME/antz/...)
// - Windows:     %APPDATA%\antz\paired_channels.csv
//
// This header expects the following to exist in the including TU (discovery.cpp):
//   namespace ant {
//     struct Channel {
//       bool use; uint8_t cNum; uint8_t cType; uint16_t dNum; uint8_t dType;
//       uint8_t tType; unsigned short period; uint8_t rfFreq; uint8_t searchTimeout;
//     };
//     extern std::vector<Channel> channels;
//     // Logging + formatting helpers:
//     void info(const std::string&);
//     void warn(const std::string&);
//     void fine(const std::string&);
//     std::string toHexByte(uint16_t); // (overload taking uint8_t is fine too)
//   }

#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include "discovery.hpp"

namespace ant {

  /**
   * @brief Configuration for the Heart Rate Monitor (HRM) search channel.
   *
   * This variable defines the parameters for the HRM channel search mode,
   * including its state, network number, channel type, RF frequency, device type,
   * transmission type, search timeout, data page size, and extended assignment.
   *
   * @note This is a constant configuration and should not be modified at runtime.
   */
  static constexpr Channel HRM_SEARCH_CH = {true, 0x00,  0x00, 0x00, 0x78, 0x00, 8070, 57, 0x012};

  // -----------------------------------------------------------------------------
  // ANT+ Asset Tracker – Pairing Mode
  //
  // This configuration enables ANT+ pairing as defined in the Device Profile
  // "ANT+ Asset Tracker Rev 1.0", Chapter 6: Device Pairing.
  //
  // Key behaviors:
  // - The receiver (e.g., this app or a Fenix watch) opens an ANT channel using:
  //     - Device #:        0 (wildcard)
  //     - Device Type:      0x29 (Asset Tracker)
  //     - Transmission Type: 0 (wildcard, required for pairing)
  //     - Channel Period:   2048 (16 Hz) – mandatory for Asset Tracker
  //     - RF Frequency:     2457 MHz – standard for most ANT+ profiles
  //
  // - No data is transmitted by the receiver during pairing.
  // - The receiver listens passively for Location Page 0x01 messages.
  // - A valid pairing candidate must send:
  //     - Page 0x01 with a valid index, distance and bearing
  //     - Extended data ("Rx trailer") with:
  //         - Device # (2 bytes)
  //         - Device Type (0x29)
  //         - Transmission Type (any, 0x00 preferred)
  //         - Optionally RSSI / Proximity info (flags 0xD0+)
  // - Once a message is received, the receiver may cache the Device # and
  //   Transmission Type for future use (persistent pairing).
  // - To be compatible with future devices, **any Transmission Type returned
  //   is valid** and should be accepted.
  //
  // Reference:
  // ANT+ Asset Tracker Device Profile, Rev 1.0 – Section 6: Device Pairing
  // -----------------------------------------------------------------------------
  static constexpr Channel TRK_SEARCH_CH = {true, 0x01,  0x00, 0x00, 0x29, 0x00, 2048, 57, 0x03};

  inline std::vector<Channel> channels = {
    /*
      HRM_SEARCH_CH,
      TRK_SEARCH_CH,
      {false, 0x02,  0x00, 0x2BB3, 0x78, 0x51, 8070, 57, 0x012},  // Paired HRM
      {false, 0x03,  0x00, 0x024A, 0x29, 0xD5, 2048, 57, 0x06},   // Paired Alpha 10
      {false, 0x04,  0x00, 0x7986, 0x29, 0x65, 2048, 57, 0x03},   // Paired Astro 320
    */
  };

  inline constexpr uint8_t MAX_SEARCH_CH = 2;

  void info(const std::string& message);
  void warn(const std::string& message);
  void fine(const std::string& message);
  std::string toHexByte(uint16_t v);

  // ---- Defaults for paired Tracker channels (no TRK_SEARCH_CH dependency) ----
  // Asset Tracker profile timing: period 2048 (16 Hz), RF 57 (2457 MHz).
  // These are used to template a new paired channel when we first see a device.
  struct PairedDefaults {
    uint8_t  cType         = 0x00;   // slave, bidirectional Rx-only typical
    unsigned short period  = 2048;
    uint8_t  rfFreq        = 57;
    uint8_t  searchTimeout = 0x06;   // snappier reconnects for paired
  };

  inline PairedDefaults& defaults() {
    static PairedDefaults d{};
    return d;
  }

  // Optional override if you want to tweak defaults at runtime.
  inline void setPairedDefaults(const PairedDefaults& d) {
    defaults() = d;
  }

  // ---- Paired store path and override ----

  inline std::string& pairedStorePathRef() {
    static std::string path = []{
    #if defined(_WIN32)
      const char* appdata = std::getenv("APPDATA");
      std::filesystem::path base = appdata ? std::filesystem::path(appdata) : std::filesystem::path(".");
      return (base / "antz" / "paired_channels.csv").string();
    #else
      const char* xdg = std::getenv("XDG_CONFIG_HOME");
      std::filesystem::path base;
      if (xdg && *xdg) {
        base = std::filesystem::path(xdg);
      } else {
        const char* home = std::getenv("HOME");
        base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::path(".");
      }
      return (base / "antz" / "paired_channels.csv").string();
    #endif
    }();
    return path;
  }

  inline const std::string& getPairedStorePath() {
    return pairedStorePathRef();
  }

  inline void setPairedStorePath(const std::string& p) {
    pairedStorePathRef() = p;
  }

  // ---- Helpers ----

  inline void ensureParentDir(const std::string& file) {
    std::filesystem::path p(file);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
  }

  inline std::string channelToCsv(const ant::Channel& ch) {
    std::ostringstream oss;
    // cNum;use;cType;dNum;dType;tType;period;rfFreq;searchTimeout
    oss << static_cast<int>(ch.cNum) << ';'
        << (ch.use ? 1 : 0) << ';'
        << static_cast<int>(ch.cType) << ';'
        << ch.dNum << ';'
        << static_cast<int>(ch.dType) << ';'
        << static_cast<int>(ch.tType) << ';'
        << ch.period << ';'
        << static_cast<int>(ch.rfFreq) << ';'
        << static_cast<int>(ch.searchTimeout);
    return oss.str();
  }

  inline bool parseInt(const std::string& s, int& out) {
    try { out = std::stoi(s); return true; } catch (...) { return false; }
  }
  inline bool parseUshort(const std::string& s, unsigned short& out) {
    try { out = static_cast<unsigned short>(std::stoul(s)); return true; } catch (...) { return false; }
  }

  inline bool csvToChannel(const std::string& line, ant::Channel& ch) {
    std::istringstream iss(line);
    std::string tok;
    int field = 0, iTmp = 0;
    unsigned short usTmp = 0;

    while (std::getline(iss, tok, ';')) {
      switch (field) {
        case 0: if (!parseInt(tok, iTmp)) return false; ch.cNum = static_cast<uint8_t>(iTmp); break;
        case 1: if (!parseInt(tok, iTmp)) return false; ch.use = (iTmp != 0); break;
        case 2: if (!parseInt(tok, iTmp)) return false; ch.cType = static_cast<uint8_t>(iTmp); break;
        case 3: if (!parseUshort(tok, usTmp)) return false; ch.dNum = static_cast<uint16_t>(usTmp); break;
        case 4: if (!parseInt(tok, iTmp)) return false; ch.dType = static_cast<uint8_t>(iTmp); break;
        case 5: if (!parseInt(tok, iTmp)) return false; ch.tType = static_cast<uint8_t>(iTmp); break;
        case 6: if (!parseUshort(tok, usTmp)) return false; ch.period = static_cast<unsigned short>(usTmp); break;
        case 7: if (!parseInt(tok, iTmp)) return false; ch.rfFreq = static_cast<uint8_t>(iTmp); break;
        case 8: if (!parseInt(tok, iTmp)) return false; ch.searchTimeout = static_cast<uint8_t>(iTmp); break;
        default: break;
      }
      ++field;
    }
    return field >= 9;
  }

  inline uint8_t nextFreeChannelNumber() {
    std::set<uint8_t> used;
    for (const auto& c : ant::channels) used.insert(c.cNum);
    for (uint8_t n = MAX_SEARCH_CH; n < 8; ++n) {
      if (!used.contains(n)) {
        return n;
      }
    }
    return ant::channels.empty()
      ? MAX_SEARCH_CH
      : std::min(MAX_SEARCH_CH, static_cast<uint8_t>(ant::channels.back().cNum + 1));
  }

  inline bool channelEqualsId(const ant::Channel& c, const uint16_t dNum, const uint8_t dType, const uint8_t tType) {
    return c.dNum == dNum && c.dType == dType && c.tType == tType;
  }

  inline bool hasChannel(const uint16_t dNum, const uint8_t dType, const uint8_t tType) {
    return std::any_of(
      ant::channels.begin(),
      ant::channels.end(),
      [&](const ant::Channel& c){
        return channelEqualsId(c, dNum, dType, tType);
      }
    );
  }

  // Public API: add/save/load

  // Find channel config by number
  inline const Channel* findChannelByNumber(const uint8_t cNum) {
    const auto it = std::find_if(channels.begin(), channels.end(),
        [&](const Channel& ch){ return ch.cNum == cNum; });
    return (it != channels.end()) ? &(*it) : nullptr;
  }

  static Channel makeDedicatedFromTemplate(const uint8_t cNum, const Channel& tmpl, const ExtendedInfo& ext) {
    Channel ch = tmpl;
    ch.cNum  = cNum;
    ch.use   = true;
    ch.dNum  = ext.deviceId.number;
    ch.dType = ext.deviceId.dType;
    ch.tType = ext.deviceId.tType;
    return ch;
  }

  inline bool savePairedChannels() {
    const auto& path = getPairedStorePath();
    ensureParentDir(path);
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
      ant::warn(std::string("Failed to open paired store for write: ") + path);
      return false;
    }
    // Persist only concrete (non-wildcard) device channels
    for (const auto& ch : ant::channels) {
      if (ch.dNum == 0) continue;
      out << channelToCsv(ch) << "\n";
    }
    out.close();
    ant::info(std::string("Saved paired channels to ") + path);
    return true;
  }

  inline bool loadPairedChannels() {
    const auto& path = getPairedStorePath();
    std::ifstream in(path);
    if (!in.is_open()) {
      ant::warn(std::string("Paired channel store not found: ") + path);
      return false;
    }
    std::string line;
    int loaded = 0;
    channels.clear();
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      ant::Channel ch{};
      if (csvToChannel(line, ch)) {
        if (!hasChannel(ch.dNum, ch.dType, ch.tType)) {
          ant::channels.push_back(ch);
          ++loaded;
        }
      }
    }
    in.close();
    if (loaded > 0) {
      ant::info("Loaded " + std::to_string(loaded) + " paired channel(s) from " + path);
    } else {
      ant::info("No paired channels loaded from " + path);
    }
    return loaded > 0;
  }

  inline std::string makeDeviceKey(const ExtendedInfo& ext) {
    std::ostringstream oss;
    oss << ext.deviceId.number
        << ":" << static_cast<int>(ext.deviceId.dType)
        << ":" << static_cast<int>(ext.deviceId.tType);
    return oss.str();
  }

} // namespace ant::config
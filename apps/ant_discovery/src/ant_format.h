//
// Output fragment construction for the ANT+ profiles this app speaks.
//
// Separated from discovery.cpp because the fragments are pure string building
// over plain structs, and because the alternative — building them inline next
// to the I/O — is how a malformed heart-rate object reached MQTT unnoticed for
// as long as it did. Nothing here touches std::cout, MQTT or a global; the
// verbosity that used to be read from `logLevel` is a parameter.
//
// discovery.cpp adds the I/O. tests/format_test.cpp is the reason this is a
// separate header.
//

#ifndef ANT_FORMAT_H
#define ANT_FORMAT_H

#include "discovery.hpp"
#include "logging.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace ant::format {

inline std::string jsonEscape(const std::string& input) {
    std::ostringstream oss;
    for (const auto& c : input) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1F) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// ISO 8601 UTC.
inline std::string timestamp(const std::chrono::system_clock::time_point& tp) {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    const std::tm* tm = std::gmtime(&t);
    char buf[25];
    std::strftime(buf, sizeof(buf), "%FT%TZ", tm);
    return {buf};
}

inline std::string assetSituation(const AssetSituation s) {
    switch (s) {
        case AssetSituation::Undefined: return "Undefined";
        case AssetSituation::Unknown:   return "Unknown";
        case AssetSituation::Pointed:   return "On Point";
        case AssetSituation::Treed:     return "Treed";
        case AssetSituation::Moving:    return "Moving";
        case AssetSituation::Sitting:   return "Sitting";
        default:                        return "Invalid";
    }
}

// TRK Table 7-9, spec wording. Absent until an identification page arrives,
// which is not the same as type 0.
inline std::string assetType(const std::optional<uint8_t> type) {
    if (!type.has_value()) return "Unknown";
    switch (*type) {
        case 0x00: return "Asset Tracker";
        case 0x01: return "Dog";
        default:   return "Reserved";
    }
}

// Seven decimals is about 1.1 cm. A semicircle is 180/2^31 degrees, about
// 9 mm, so anything coarser discards resolution the wire paid to carry — and
// a default-formatted double gives six significant digits, which at these
// magnitudes is four decimals, or roughly 11 m.
inline std::string coordinate(const double degrees) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(7) << degrees;
    return oss.str();
}

inline std::string fixed(const double v, const int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

// The envelope every fragment is delivered in.
inline std::string wrapPage(const OutputFormat fmt, const char* pageName, const std::string& fragment) {
    std::ostringstream oss;
    switch (fmt) {
        case OutputFormat::JSON: oss << "{" << R"("page":")" << pageName << "\"," << fragment << "}"; break;
        case OutputFormat::CSV:  oss << pageName << "," << fragment; break;
        case OutputFormat::Text: oss << fragment; break;
    }
    return oss.str();
}

inline std::string hrm(const OutputFormat fmt, const HRM& h, const std::string& text, const bool verbose) {
    std::ostringstream oss;
    switch (fmt) {
        case OutputFormat::Text:
            oss << text;
            break;
        case OutputFormat::JSON:
            // HRM Table 7: 0x00 is invalid, not a rate of zero.
            if (h.heartRate.has_value()) oss << R"("heartRate":)" << static_cast<int>(*h.heartRate) << ",";
            else                         oss << R"("heartRate":null,)";
            oss << R"("heartBeatCount":)" << static_cast<int>(h.heartBeatCount) << ",";
            oss << R"("heartBeatEventTime":)" << static_cast<int>(h.heartBeatEventTime);
            if (verbose) {
                oss << R"(,"flags":"0x)" << toHexByte(h.ext.flags) << "\"";
                oss << R"(,"text":")" << jsonEscape(text) << "\"";
            }
            break;
        case OutputFormat::CSV:
            // heartRate,heartBeatCount,heartBeatEventTime,[flags,text]
            if (h.heartRate.has_value()) oss << static_cast<int>(*h.heartRate);
            oss << "," << static_cast<int>(h.heartBeatCount)
                << "," << static_cast<int>(h.heartBeatEventTime);
            if (verbose) {
                oss << ",0x" << toHexByte(h.ext.flags);
                oss << "," << '"' << text << '"';
            }
            break;
    }
    return oss.str();
}

inline std::string device(const OutputFormat fmt, const Device& d, const double age,
                          const std::string& text, const bool verbose) {
    const std::string name = !d.name.fName.empty() ? d.name.fName : d.name.uName;
    std::ostringstream oss;
    switch (fmt) {
        case OutputFormat::Text:
            oss << text << " | age=" << fixed(age, 0) << "s";
            break;
        case OutputFormat::JSON:
            oss << R"("ts":")" << timestamp(d.ts) << "\",";
            oss << R"("name":")" << jsonEscape(name) << "\",";
            oss << R"("index":)" << static_cast<int>(d.index) << ",";
            oss << R"("color":"0x)" << toHexByte(d.color) << "\",";
            oss << R"("id":"0x)" << toHexByte(d.ext.deviceId.number) << "\",";
            oss << R"("deviceType":"0x)" << toHexByte(d.ext.deviceId.dType) << "\",";
            if (d.aType.has_value()) oss << R"("assetType":)" << static_cast<int>(*d.aType) << ",";
            else                     oss << R"("assetType":null,)";
            oss << R"("assetTypeName":")" << assetType(d.aType) << "\",";
            oss << R"("lat":)" << coordinate(d.lat) << ",";
            oss << R"("long":)" << coordinate(d.lon) << ",";
            oss << R"("distance":)" << d.distance << ",";
            oss << R"("heading":)" << fixed(d.headingDegrees, 1) << ",";
            oss << R"("situation":")" << assetSituation(d.situation) << "\",";
            oss << R"("gpsLost":)" << (d.gpsLost ? "true" : "false") << ",";
            oss << R"("commsLost":)" << (d.commsLost ? "true" : "false") << ",";
            oss << R"("lowBattery":)" << (d.lowBattery ? "true" : "false") << ",";
            oss << R"("remove":)" << (d.remove ? "true" : "false") << ",";
            oss << R"("age":)" << fixed(age, 0);
            if (verbose) {
                oss << R"(,"flags":"0x)" << toHexByte(d.ext.flags) << "\"";
                oss << R"(,"text":")" << jsonEscape(text) << "\"";
            }
            break;
        case OutputFormat::CSV:
            // ts,name,index,deviceId,deviceType,assetType,lat,lon,distance,
            // heading,situation,gpsLost,commsLost,lowBattery,remove,age,[flags,text]
            oss << '"' << timestamp(d.ts) << '"' << ","
                << '"' << name << '"' << ","
                << static_cast<int>(d.index) << ","
                << "0x" << toHexByte(d.ext.deviceId.number) << ","
                << "0x" << toHexByte(d.ext.deviceId.dType) << ","
                << (d.aType.has_value() ? std::to_string(static_cast<int>(*d.aType)) : std::string()) << ","
                << coordinate(d.lat) << ","
                << coordinate(d.lon) << ","
                << d.distance << ","
                << fixed(d.headingDegrees, 1) << ","
                << '"' << assetSituation(d.situation) << '"' << ","
                << (d.gpsLost ? 1 : 0) << ","
                << (d.commsLost ? 1 : 0) << ","
                << (d.lowBattery ? 1 : 0) << ","
                << (d.remove ? 1 : 0) << ","
                << fixed(age, 0);
            if (verbose) {
                oss << ",0x" << toHexByte(d.ext.flags);
                oss << "," << '"' << text << '"';
            }
            break;
    }
    return oss.str();
}

} // namespace ant::format

#endif // ANT_FORMAT_H

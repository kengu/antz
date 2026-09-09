//
// Output fragment tests.
//
// The heart-rate object was malformed JSON for as long as it existed and no
// test would have noticed, because construction sat next to the I/O. These
// assert the strings, not the plumbing.
//
#include "ant_format.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

using namespace ant;

static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)
#define CHECK_EQ(got, want) do { ++checks; const std::string g_=(got), w_=(want); if (g_ != w_) { \
    std::printf("FAIL %s:%d\n  got  %s\n  want %s\n", __FILE__, __LINE__, g_.c_str(), w_.c_str()); \
    return 1; } } while (0)

static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// A JSON object is well formed enough for our purposes if quotes pair, braces
// balance, and no comma sits against a brace or another comma. That is exactly
// the class of defect the heart-rate emitter had.
static bool wellFormedJson(const std::string& s) {
    if (s.empty() || s.front() != '{' || s.back() != '}') return false;
    int depth = 0;
    bool inString = false, escaped = false;
    char prev = '\0';
    for (const char c : s) {
        if (inString) {
            if (escaped)        escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"')  inString = false;
            continue;
        }
        if (c == '"') { inString = true; prev = c; continue; }
        if (c == '{') ++depth;
        if (c == '}') { if (--depth < 0) return false; if (prev == ',') return false; }
        if (c == ',' && (prev == ',' || prev == '{' || prev == '\0')) return false;
        if (c == ':' && prev != '"') return false;
        if (c != ' ') prev = c;
    }
    return depth == 0 && !inString;
}

static HRM aHeartRate(const std::optional<uint8_t> bpm) {
    HRM h;
    h.heartRate = bpm;
    h.heartBeatCount = 13;
    h.heartBeatEventTime = 4660;
    h.ext.flags = 0x20;
    return h;
}

// The checker has to reject what the old emitter produced, or it proves nothing.
static int checker_self_test() {
    CHECK(wellFormedJson(R"({"a":1})"));
    CHECK(wellFormedJson(R"({"a":"x,y","b":2})"));
    CHECK(!wellFormedJson(R"({"page":"P","heartRate":"72,})"));      // unterminated
    CHECK(!wellFormedJson(R"({"heartRate":"72,,"flags":"0x20"})"));  // the real bug
    CHECK(!wellFormedJson(R"({"a":1,})"));                           // trailing comma
    CHECK(!wellFormedJson(R"({"a":1)"));                             // unbalanced
    return 0;
}

// The regression this file exists for.
static int hrm_json() {
    const std::string quiet = format::wrapPage(
        OutputFormat::JSON, "HRMPage0", format::hrm(OutputFormat::JSON, aHeartRate(72), "t", false));
    CHECK_EQ(quiet, R"({"page":"HRMPage0","heartRate":72,"heartBeatCount":13,"heartBeatEventTime":4660})");
    CHECK(wellFormedJson(quiet));

    // Verbose. The old emitter produced "heartRate":"72,, here — an unclosed
    // string and a doubled comma.
    const std::string verbose = format::wrapPage(
        OutputFormat::JSON, "HRMPage0", format::hrm(OutputFormat::JSON, aHeartRate(72), "hello", true));
    CHECK(wellFormedJson(verbose));
    CHECK(has(verbose, R"("heartRate":72,)"));
    CHECK(!has(verbose, R"("heartRate":")"));   // never a quoted number
    CHECK(!has(verbose, ",,"));
    CHECK(has(verbose, R"("flags":"0x20")"));
    CHECK(has(verbose, R"("text":"hello")"));

    // HRM Table 7: 0x00 is invalid. Null, not zero, and still well formed.
    const std::string invalid = format::wrapPage(
        OutputFormat::JSON, "HRMPage0", format::hrm(OutputFormat::JSON, aHeartRate(std::nullopt), "t", false));
    CHECK(wellFormedJson(invalid));
    CHECK(has(invalid, R"("heartRate":null)"));
    CHECK(!has(invalid, R"("heartRate":0)"));

    // Quotes in free text must not break the object.
    const std::string quoted = format::wrapPage(
        OutputFormat::JSON, "HRMPage0", format::hrm(OutputFormat::JSON, aHeartRate(72), "a \"b\" c", true));
    CHECK(wellFormedJson(quoted));
    return 0;
}

static int hrm_csv() {
    CHECK_EQ(format::wrapPage(OutputFormat::CSV, "HRMPage0",
                              format::hrm(OutputFormat::CSV, aHeartRate(72), "t", false)),
             "HRMPage0,72,13,4660");
    // An invalid rate leaves the field empty rather than claiming zero.
    CHECK_EQ(format::wrapPage(OutputFormat::CSV, "HRMPage0",
                              format::hrm(OutputFormat::CSV, aHeartRate(std::nullopt), "t", false)),
             "HRMPage0,,13,4660");
    return 0;
}

static Device aDog() {
    Device d;
    d.index = 3;
    d.name.uName = "Rex";
    d.name.fName = "Rex";
    d.color = 96;
    d.aType = uint8_t{1};
    d.lat = 63.4869123;
    d.lon = 10.9609456;
    d.distance = 750;
    d.headingDegrees = 60.46875f;
    d.situation = AssetSituation::Treed;
    d.gpsLost = false;
    d.commsLost = false;
    d.lowBattery = true;
    d.remove = false;
    d.ext.flags = 0x80;
    return d;
}

static int device_json() {
    const Device d = aDog();
    const std::string s = format::wrapPage(
        OutputFormat::JSON, "Location2", format::device(OutputFormat::JSON, d, 12.0, "t", true));
    CHECK(wellFormedJson(s));

    // A semicircle is about 9 mm. Default double formatting gives six
    // significant digits, which at these magnitudes is four decimals — about
    // 11 m — so the emitter must set precision explicitly.
    CHECK(has(s, R"("lat":63.4869123)"));
    CHECK(has(s, R"("long":10.9609456)"));

    // The 0x prefix must not sit in front of a decimal. 96 is 0x60.
    CHECK(has(s, R"("color":"0x60")"));
    CHECK(!has(s, R"("color":"0x96")"));

    CHECK(has(s, R"("assetType":1)"));
    CHECK(has(s, R"("assetTypeName":"Dog")"));
    CHECK(has(s, R"("situation":"Treed")"));
    CHECK(has(s, R"("lowBattery":true)"));
    CHECK(has(s, R"("gpsLost":false)"));
    CHECK(has(s, R"("heading":60.5)"));
    CHECK(has(s, R"("age":12)"));

    // Absent asset type is null, and never the zero that means Asset Tracker.
    Device unknown = aDog();
    unknown.aType.reset();
    const std::string u = format::wrapPage(
        OutputFormat::JSON, "Location2", format::device(OutputFormat::JSON, unknown, 0, "t", false));
    CHECK(wellFormedJson(u));
    CHECK(has(u, R"("assetType":null)"));
    CHECK(has(u, R"("assetTypeName":"Unknown")"));
    CHECK(!has(u, R"("assetType":0)"));

    // A southern, western dog must not come back positive.
    Device south = aDog();
    south.lat = -33.8688000;
    south.lon = -70.1234567;
    const std::string sw = format::wrapPage(
        OutputFormat::JSON, "Location2", format::device(OutputFormat::JSON, south, 0, "t", false));
    CHECK(has(sw, R"("lat":-33.8688000)"));
    CHECK(has(sw, R"("long":-70.1234567)"));
    return 0;
}

static int device_csv() {
    const std::string s = format::wrapPage(
        OutputFormat::CSV, "Location2", format::device(OutputFormat::CSV, aDog(), 12.0, "t", false));
    CHECK(has(s, "Location2,"));
    CHECK(has(s, ",3,"));                 // index
    CHECK(has(s, "63.4869123"));          // full precision here too
    CHECK(has(s, R"("Treed")"));
    return 0;
}

static int escaping() {
    CHECK_EQ(format::jsonEscape(R"(a"b)"), R"(a\"b)");
    CHECK_EQ(format::jsonEscape("a\\b"), R"(a\\b)");
    CHECK_EQ(format::jsonEscape("a\nb"), R"(a\nb)");
    CHECK_EQ(format::jsonEscape("a\tb"), R"(a\tb)");
    CHECK_EQ(format::jsonEscape("plain"), "plain");
    return 0;
}

struct Group { const char* name; int (*run)(); };
static const Group groups[] = {
    { "format.checker",     checker_self_test },
    { "format.hrm_json",    hrm_json    },
    { "format.hrm_csv",     hrm_csv     },
    { "format.device_json", device_json },
    { "format.device_csv",  device_csv  },
    { "format.escaping",    escaping    },
};

int main(const int argc, char** argv) {
    if (argc > 2) { std::printf("usage: %s [group]\n", argv[0]); return 2; }
    if (argc == 2) {
        for (const auto& [name, run] : groups) {
            if (std::strcmp(name, argv[1]) == 0) {
                if (run()) return 1;
                std::printf("ok %s — %d checks\n", name, checks);
                return 0;
            }
        }
        std::printf("no such group: %s\n", argv[1]);
        return 2;
    }
    for (const auto& [name, run] : groups) {
        const int before = checks;
        if (run()) return 1;
        std::printf("ok %-22s %d checks\n", name, checks - before);
    }
    std::printf("ok — %d checks in %zu groups\n", checks, std::size(groups));
    return 0;
}

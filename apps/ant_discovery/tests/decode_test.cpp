//
// Decoding tests for the ANT+ profiles this app speaks.
//
// Links nothing: ant_decode.h is free of the ANT SDK precisely so these can
// run on a build machine with no dongle. Every case cites the table it comes
// from, because the bugs these guard against were all misread tables.
//
#include "ant_decode.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>

using namespace ant::decode;

static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

static bool near(const double a, const double b, const double eps = 1e-6) {
    return std::fabs(a - b) <= eps;
}

// ─── Tracker: status byte, TRK Table 7-4 and 7-5 ───────────────────────────
static int status_byte() {
    // Situation is bits 0:2. Read from 5:7 — as this decoder once did — every
    // state below collapses to Sitting.
    CHECK(situation(0x00) == AssetSituation::Sitting);
    CHECK(situation(0x01) == AssetSituation::Moving);
    CHECK(situation(0x02) == AssetSituation::Pointed);
    CHECK(situation(0x03) == AssetSituation::Treed);
    CHECK(situation(0x04) == AssetSituation::Unknown);

    // 5-7 fit the field and name nothing.
    CHECK(situation(0x05) == AssetSituation::Undefined);
    CHECK(situation(0x07) == AssetSituation::Undefined);

    // Table 7-5: an Asset Tracker asset has no situation, and the whole byte
    // is 0xFF — which must not be read as situation 7 plus every flag set.
    CHECK(situation(0xFF) == AssetSituation::Undefined);

    // Flags sit above the situation field and must not be read out of it.
    CHECK(!gpsLost(0x03) && !commsLost(0x03) && !removeFlag(0x03) && !lowBattery(0x03));
    CHECK(lowBattery(0x08) && !gpsLost(0x08));
    CHECK(gpsLost(0x10)    && !commsLost(0x10));
    CHECK(commsLost(0x20)  && !removeFlag(0x20));
    CHECK(removeFlag(0x40) && !commsLost(0x40));

    // A treed dog with a flat battery is both, and nothing else.
    const uint8_t treedLowBattery = 0x0B;
    CHECK(situation(treedLowBattery) == AssetSituation::Treed);
    CHECK(lowBattery(treedLowBattery));
    CHECK(!gpsLost(treedLowBattery) && !commsLost(treedLowBattery));

    // Bit 7 is reserved and must not disturb the situation.
    CHECK(situation(0x83) == AssetSituation::Treed);
    return 0;
}

// ─── Tracker: asset index, TRK Table 7-3 ───────────────────────────────────
static int asset_index() {
    CHECK(assetIndex(0x00) == 0);
    CHECK(assetIndex(0x03) == 3);
    CHECK(assetIndex(0x1F) == 31);          // five bits, so 31 is the maximum
    CHECK(assetIndex(0xE3) == 3);           // reserved bits 5:7 are set to 0x7
    return 0;
}

// ─── Tracker: units, TRK §8.1 and §8.2 ─────────────────────────────────────
static int units() {
    CHECK(near(semicirclesToDegrees(0), 0.0));

    // §8.1's two worked examples, asserted against the computed value rather
    // than the printed one. Both hex inputs and both decimals in the document
    // are right; both printed results are not, in different ways. Do not
    // "correct" these to match the page.
    //
    //   longitude 0xAB1688E4 = -1424586524 -> -119.40746304
    //     printed as -119.407462: truncated at the sixth decimal, not rounded.
    //   latitude  0x1A6CFB08 =   443349768 ->   37.16114827
    //     printed as 37.16148287: the same digits transposed.
    CHECK(near(semicirclesToDegrees(-1424586524), -119.40746304, 1e-8));
    CHECK(near(semicirclesToDegrees(443349768),     37.16114827, 1e-8));

    // Signedness is the point: read as unsigned, a western longitude comes
    // back as a large positive angle instead of a negative one.
    CHECK(semicirclesToDegrees(-1424586524) < 0.0);

    // Norway is positive in both, which is how an unsigned read hid.
    CHECK(semicirclesToDegrees(757323304) > 0.0);

    // §8.2's worked example: 0x2B is 60 degrees.
    CHECK(near(bradiansToDegrees(0x2B), 60.46875));
    CHECK(near(bradiansToDegrees(0), 0.0));
    CHECK(near(bradiansToDegrees(128), 180.0));
    CHECK(bradiansToDegrees(255) < 360.0);
    return 0;
}

// ─── Heart rate, HRM Table 7 ───────────────────────────────────────────────
static int heart_rate() {
    // Byte 0: page in bits 0:6, toggle in bit 7.
    CHECK(hrmPage(0x00) == 0);
    CHECK(hrmPage(0x04) == 4);
    CHECK(hrmPage(0x84) == 4);              // toggle set must not change it
    CHECK(!hrmToggle(0x04));
    CHECK(hrmToggle(0x84));

    // Byte 7: 1-255 bpm, "If Invalid set to 0x00". Zero is not a measurement
    // of nought and must not reach a consumer as one.
    CHECK(!computedHeartRate(0).has_value());
    CHECK(computedHeartRate(1).value_or(0) == 1);
    CHECK(computedHeartRate(72).value_or(0) == 72);
    CHECK(computedHeartRate(255).value_or(0) == 255);

    // Bytes 4-5: little-endian, 1/1024 s.
    CHECK(heartBeatEventTime(0x00, 0x00) == 0);
    CHECK(heartBeatEventTime(0x34, 0x12) == 0x1234);
    CHECK(heartBeatEventTime(0xFF, 0xFF) == 0xFFFF);
    return 0;
}

// Each group is registered with CTest separately, so a failure names the part
// of the spec that broke and a group can be run on its own. With no argument
// every group runs, which is what a bare ./antz_decode_test does.
struct Group { const char* name; int (*run)(); };

static const Group groups[] = {
    { "tracker.status_byte", status_byte },
    { "tracker.asset_index", asset_index },
    { "tracker.units",       units       },
    { "hrm.page_and_rate",   heart_rate  },
};

int main(const int argc, char** argv) {
    if (argc > 2) {
        std::printf("usage: %s [group]\n", argv[0]);
        return 2;
    }

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

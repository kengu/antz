//
// Decoding tests for the ANT+ profiles this app speaks.
//
// Links nothing: ant_decode.h is free of the ANT SDK precisely so these can
// run on a build machine with no dongle. Every case cites the table it comes
// from, because the bugs these guard against were all misread tables.
//
#include "ant_decode.h"

#include "profiles/common/common_decoder.h"
#include "profiles/tracker/tracker_decoder.h"

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
// ─── Common Data Pages 80, 81 and 82, D00001198 ───────────────────────────────
//
// Not in ant_decode.h: these are common to every ANT+ profile rather than to
// the Tracker, so they live in antz_core beside the profiles. Tested here
// because this is where the decode suite is.
static int common_pages() {
    // Page 80. Garmin is manufacturer 1; model and hardware revision are the
    // maker's own. Bytes 1-2 are reserved and all-ones, and reading them as
    // a field is the mistake the layout invites.
    const uint8_t page80[8] = { 0x50, 0xFF, 0xFF, 0x01, 0x01, 0x00, 0xC7, 0x06 };
    antz_common_manufacturer_t mfg{};
    CHECK(antz::common_decode_manufacturer(page80, 8, &mfg) == 0);
    CHECK(mfg.hw_revision == 1);
    CHECK(mfg.manufacturer_id == 1);
    CHECK(mfg.model_number == 1735);

    // Page 81, with both optional fields present.
    const uint8_t page81[8] = { 0x51, 0xFF, 0x0A, 0x03, 0xD2, 0x02, 0x96, 0x49 };
    antz_common_product_t product{};
    CHECK(antz::common_decode_product(page81, 8, &product) == 0);
    CHECK(product.sw_revision_main == 3);
    CHECK(product.sw_revision_supplemental == 10);
    CHECK(product.sw_revision_supplemental_valid);
    CHECK(product.serial == 1234567890u);
    CHECK(product.serial_valid);

    // All-ones is a refusal, not a value. A decoder testing the number
    // instead of the flag gives every declining device serial 4294967295 —
    // which makes them one device rather than none.
    const uint8_t page81none[8] = { 0x51, 0xFF, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0xFF };
    antz_common_product_t declined{};
    CHECK(antz::common_decode_product(page81none, 8, &declined) == 0);
    CHECK(declined.sw_revision_main == 3);
    CHECK(!declined.sw_revision_supplemental_valid);
    CHECK(!declined.serial_valid);

    // The page number is checked, unlike in the Tracker decoders. These two
    // are reached by dispatch over one shared page space, so routing 0x50 to
    // the product decoder must fail rather than read a manufacturer id as
    // the low half of a serial.
    CHECK(antz::common_decode_product(page80, 8, &product) != 0);
    CHECK(antz::common_decode_manufacturer(page81, 8, &mfg) != 0);

    // Null and short input, as the Tracker decoders handle them.
    CHECK(antz::common_decode_manufacturer(nullptr, 8, &mfg) != 0);
    CHECK(antz::common_decode_manufacturer(page80, 8, nullptr) != 0);
    CHECK(antz::common_decode_manufacturer(page80, 7, &mfg) != 0);
    CHECK(antz::common_decode_product(page81, 7, &product) != 0);

    // Page 82, captured off hardware on 2026-09-24: each handheld's own
    // battery, and each reading matches that unit's chemistry.
    // Alpha 10, internal lithium cell: 3 + 0xC5/256 = 3.77 V, status 3 (ok).
    const uint8_t alpha[8] = { 0x52, 0xFF, 0xFF, 0x65, 0x0A, 0x00, 0xC5, 0xB3 };
    antz_common_battery_t battery{};
    CHECK(antz::common_decode_battery(alpha, 8, &battery) == 0);
    CHECK(!battery.identifier_valid);
    CHECK(battery.voltage_valid);
    CHECK(battery.voltage_v > 3.769f && battery.voltage_v < 3.771f);
    CHECK(battery.status_valid);
    CHECK(battery.status == ANTZ_BATTERY_STATUS_OK);
    // Every field as sent: 0x000A65 ticks at 2 s (bit 7 of byte 7 set),
    // coarse 3 and fractional 0xC5.
    CHECK(battery.operating_time_ticks == 0x000A65u);
    CHECK(battery.operating_time_resolution_s == 2u);
    CHECK(battery.operating_time_s == 0x000A65u * 2u);
    CHECK(battery.voltage_coarse == 3u);
    CHECK(battery.voltage_fractional == 0xC5u);

    // Astro 320, two AA cells: 2 + 0x97/256 = 2.59 V, status 2 (good).
    const uint8_t astro[8] = { 0x52, 0xFF, 0xFF, 0x5D, 0x0A, 0x00, 0x97, 0xA2 };
    CHECK(antz::common_decode_battery(astro, 8, &battery) == 0);
    CHECK(battery.voltage_v > 2.589f && battery.voltage_v < 2.591f);
    CHECK(battery.status == ANTZ_BATTERY_STATUS_GOOD);

    // Coarse volts 0x0F and status 7 are refusals, each on its own: a
    // decoder testing the value would report fifteen volts.
    const uint8_t none[8] = { 0x52, 0xFF, 0x21, 0x00, 0x00, 0x00, 0x00, 0x7F };
    CHECK(antz::common_decode_battery(none, 8, &battery) == 0);
    CHECK(!battery.voltage_valid);
    CHECK(battery.voltage_coarse == 0x0Fu);
    CHECK(battery.voltage_v == 0.0f);
    CHECK(!battery.status_valid);
    CHECK(battery.status == ANTZ_BATTERY_STATUS_INVALID);
    // Bit 7 clear: the operating time counts in 16 s ticks.
    CHECK(battery.operating_time_resolution_s == 16u);
    CHECK(battery.identifier_valid);
    CHECK(battery.battery_count == 1);
    CHECK(battery.battery_identifier == 2);

    CHECK(antz::common_decode_battery(page80, 8, &battery) != 0);
    CHECK(antz::common_decode_battery(alpha, 7, &battery) != 0);
    CHECK(antz::common_decode_battery(nullptr, 8, &battery) != 0);
    return 0;
}

struct Group { const char* name; int (*run)(); };

// ─── Tracker: the handheld's own position, pages 0x04 and 0x05 ─────────────
//
// Undocumented — TRK Rev 1.0 §7.5 reserves 0x04-0x0F and a real Astro
// transmits them anyway. So the assertions below are against **captured
// hardware**, not against a synthesised page: these two payloads came off a
// stationary Astro lying beside a phone whose own GNSS read
// 59.733434, 10.132869 at that moment. The decode lands 9.8 m away, which is
// not a coincidence at that precision.
//
// What is *not* established is deliberately not asserted as meaning: bytes
// 1-3 were 0x00 and 0xFFFF in both samples and are carried out raw.
static int self_position() {
    // Off the wire, byte for byte.
    const uint8_t lat_page[8] = { 0x04, 0x00, 0xff, 0xff, 0xd5, 0x27, 0x7a, 0x2a };
    const uint8_t lon_page[8] = { 0x05, 0x00, 0xff, 0xff, 0x5b, 0xa0, 0x34, 0x07 };

    antz_tracker_self_position_t lat{};
    CHECK(antz::tracker_decode_self_latitude(lat_page, 8, &lat) == 0);
    CHECK(lat.semicircles == 712648661);
    CHECK(near(lat.semicircles * 180.0 / 2147483648.0, 59.733521, 1e-5));

    antz_tracker_self_position_t lon{};
    CHECK(antz::tracker_decode_self_longitude(lon_page, 8, &lon) == 0);
    CHECK(lon.semicircles == 120889435);
    CHECK(near(lon.semicircles * 180.0 / 2147483648.0, 10.132835, 1e-5));

    // The unknowns are carried, not interpreted.
    CHECK(lat.reserved_1 == 0x00 && lat.reserved_23 == 0xffff);
    CHECK(lon.reserved_1 == 0x00 && lon.reserved_23 == 0xffff);

    // Each page is a whole coordinate. Nothing to reassemble, and crossing the
    // decoders is refused rather than silently producing a number.
    CHECK(antz::tracker_decode_self_latitude(lon_page, 8, &lat) != 0);
    CHECK(antz::tracker_decode_self_longitude(lat_page, 8, &lon) != 0);

    // Signed. Read unsigned, a tracker west of Greenwich lands in the wrong
    // hemisphere — which testing in Norway never shows you.
    const uint8_t west[8] = { 0x05, 0x00, 0xff, 0xff, 0xa5, 0x5f, 0xcb, 0xf8 };
    CHECK(antz::tracker_decode_self_longitude(west, 8, &lon) == 0);
    CHECK(lon.semicircles < 0);

    // An unobserved byte 1 is refused. Its meaning is unknown, and reading a
    // variant we have never seen as somebody's position is the guess that
    // produced this profile's four errata.
    const uint8_t odd[8] = { 0x04, 0xe1, 0xff, 0xff, 0xd5, 0x27, 0x7a, 0x2a };
    CHECK(antz::tracker_decode_self_latitude(odd, 8, &lat) != 0);

    // Short buffers and nulls, as everywhere else here.
    CHECK(antz::tracker_decode_self_latitude(lat_page, 7, &lat) != 0);
    CHECK(antz::tracker_decode_self_latitude(nullptr, 8, &lat) != 0);
    CHECK(antz::tracker_decode_self_latitude(lat_page, 8, nullptr) != 0);
    return 0;
}

static const Group groups[] = {
    { "tracker.status_byte", status_byte },
    { "tracker.asset_index", asset_index },
    { "tracker.units",       units       },
    { "hrm.page_and_rate",   heart_rate  },
    { "common.pages_80_81",  common_pages },
    { "tracker.self_position", self_position },
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

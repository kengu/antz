//
// Encoding tests for the ANT+ Asset Tracker pages.
//
// Two kinds, and the second is the reason the encoder is worth having.
//
// The fixed cases pin bytes against the vectors in the streamdog-platform
// repository, which the decoder is held to as well. Those vectors are authored
// from the specification rather than captured off hardware, so agreement with
// them proves this library reads and writes the profile one way — with the
// four Rev 1.0 errata already corrected — and not that a handheld would send
// these bytes.
//
// The round trips are new coverage for the decoder, not only for the encoder.
// The decoder has only ever been tested against fixed points; encode-then-
// decode runs it over the whole parameter space, which is where an off-by-one
// in a mask or a sign lives.
//
#include "profiles/tracker/tracker_encoder.h"
#include "profiles/tracker/tracker_decoder.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace antz;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static bool bytes_eq(const uint8_t* a, const uint8_t* b, const size_t n) {
    return std::memcmp(a, b, n) == 0;
}

static void show(const char* what, const uint8_t* got, const uint8_t* want) {
    std::printf("  %s\n    got  ", what);
    for (int i = 0; i < 8; ++i) std::printf("%02X", got[i]);
    std::printf("\n    want ");
    for (int i = 0; i < 8; ++i) std::printf("%02X", want[i]);
    std::printf("\n");
}

// ─── Fixed cases, against the shared vectors ───────────────────────────────

static void location1_matches_the_vector() {
    // location1-treed-healthy: 01E3EE022B038F70
    const uint8_t want[8] = { 0x01, 0xE3, 0xEE, 0x02, 0x2B, 0x03, 0x8F, 0x70 };

    antz_tracker_location1_t in{};
    in.asset_index      = 3;
    in.distance_m       = 750;
    in.bearing_bradians = 43;
    in.status.situation_valid = true;
    in.status.situation = ANTZ_TRACKER_SITUATION_TREED;
    in.latitude_bits_0_15 = 28815;

    uint8_t got[8] = {};
    CHECK(tracker_encode_location1(&in, got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want, 8));
    if (!bytes_eq(got, want, 8)) show("location1", got, want);
}

static void location2_matches_the_vector() {
    // location2-northern-eastern: 02E3252D1761CB07
    const uint8_t want[8] = { 0x02, 0xE3, 0x25, 0x2D, 0x17, 0x61, 0xCB, 0x07 };

    antz_tracker_location2_t in{};
    in.asset_index           = 3;
    in.latitude_bits_16_31   = 0x2D25;
    in.longitude_semicircles = 130769175;

    uint8_t got[8] = {};
    CHECK(tracker_encode_location2(&in, got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want, 8));
    if (!bytes_eq(got, want, 8)) show("location2", got, want);
}

static void identification1_matches_the_vector() {
    // identification1-colour-and-name: 10E3605265780000
    const uint8_t want[8] = { 0x10, 0xE3, 0x60, 0x52, 0x65, 0x78, 0x00, 0x00 };

    antz_tracker_identification1_t in{};
    in.asset_index = 3;
    in.colour      = 96;
    std::strcpy(in.name, "Rex");

    uint8_t got[8] = {};
    CHECK(tracker_encode_identification1(&in, got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want, 8));
    if (!bytes_eq(got, want, 8)) show("identification1", got, want);
}

static void identification2_matches_the_vector() {
    // identification2-asset-tracker: 11E5007900000000
    const uint8_t want[8] = { 0x11, 0xE5, 0x00, 0x79, 0x00, 0x00, 0x00, 0x00 };

    antz_tracker_identification2_t in{};
    in.asset_index = 5;
    in.asset_type  = 0;
    std::strcpy(in.name, "y");

    uint8_t got[8] = {};
    CHECK(tracker_encode_identification2(&in, got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want, 8));
    if (!bytes_eq(got, want, 8)) show("identification2", got, want);
}

static void the_assetless_pages_match_their_vectors() {
    // no-assets: 03FFFFFFFFFFFFFF, disconnect: 20FFFFFFFFFFFFFF.
    // Reserved bytes are all-ones. Zeroes would decode the same and are not
    // what a handheld sends.
    const uint8_t want_none[8] = { 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    const uint8_t want_bye[8]  = { 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    uint8_t got[8] = {};
    CHECK(tracker_encode_no_assets(got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want_none, 8));

    CHECK(tracker_encode_disconnect(got, sizeof(got)) == 0);
    CHECK(bytes_eq(got, want_bye, 8));
}

static void the_reserved_index_bits_are_set() {
    // TRK Table 7-3. Index 3 is 0xE3 and not 0x03 — the top three bits are
    // reserved and set, and this is the erratum that decoded index 3 as 227
    // when the mask was forgotten. An encoder that zeroed them would produce
    // pages this library reads back correctly and hardware never sent.
    antz_tracker_identification1_t in{};
    in.asset_index = 3;
    in.colour      = 0;
    in.name[0]     = '\0';

    uint8_t got[8] = {};
    CHECK(tracker_encode_identification1(&in, got, sizeof(got)) == 0);
    CHECK(got[1] == 0xE3);
}

// ─── Status byte, TRK Tables 7-4 and 7-5 ───────────────────────────────────

static void the_status_byte_round_trips() {
    // Every situation, every flag combination. 0xFF is excluded as a value
    // the flags can reach, because it is the sentinel — see below.
    for (int situation = 0; situation <= 4; ++situation) {
        for (int flags = 0; flags < 16; ++flags) {
            antz_tracker_status_t in{};
            in.situation_valid = true;
            in.situation = static_cast<antz_tracker_situation_e>(situation);
            in.low_battery = (flags & 1) != 0;
            in.gps_lost    = (flags & 2) != 0;
            in.comms_lost  = (flags & 4) != 0;
            in.remove      = (flags & 8) != 0;

            const uint8_t byte = tracker_encode_status(&in);
            const antz_tracker_status_t back = tracker_decode_status(byte);

            CHECK(back.situation_valid);
            CHECK(back.situation == in.situation);
            CHECK(back.low_battery == in.low_battery);
            CHECK(back.gps_lost == in.gps_lost);
            CHECK(back.comms_lost == in.comms_lost);
            CHECK(back.remove == in.remove);
        }
    }
}

static void the_sentinel_survives_a_round_trip() {
    // An Asset Tracker asset has no situation. The sentinel must come back as
    // itself and not as a byte with four fictional faults in it.
    antz_tracker_status_t in{};
    in.situation_valid = false;
    in.situation = ANTZ_TRACKER_SITUATION_UNDEFINED;
    // Flags set, and deliberately ignored: they are not readable beside the
    // sentinel, so encoding them would invent a byte that is not 0xFF.
    in.low_battery = true;
    in.remove = true;

    const uint8_t byte = tracker_encode_status(&in);
    CHECK(byte == 0xFF);

    const antz_tracker_status_t back = tracker_decode_status(byte);
    CHECK(!back.situation_valid);
    CHECK(back.situation == ANTZ_TRACKER_SITUATION_UNDEFINED);
    CHECK(!back.low_battery);
    CHECK(!back.remove);
}

static void an_unnamed_situation_round_trips_to_undefined() {
    // The decoder collapses 5, 6 and 7 into UNDEFINED, so there is nothing to
    // write back that tells them apart. Encoding must land on one of them and
    // decode to UNDEFINED again rather than on a named situation.
    antz_tracker_status_t in{};
    in.situation_valid = true;
    in.situation = ANTZ_TRACKER_SITUATION_UNDEFINED;

    const uint8_t byte = tracker_encode_status(&in);
    CHECK(byte != 0xFF);

    const antz_tracker_status_t back = tracker_decode_status(byte);
    CHECK(back.situation_valid);
    CHECK(back.situation == ANTZ_TRACKER_SITUATION_UNDEFINED);
}

// ─── Round trips over the parameter space ──────────────────────────────────

static void location1_round_trips() {
    for (uint8_t index = 0; index <= 0x1F; ++index) {
        for (int step = 0; step < 8; ++step) {
            antz_tracker_location1_t in{};
            in.asset_index      = index;
            in.distance_m       = static_cast<uint16_t>(step * 9377);
            in.bearing_bradians = static_cast<uint8_t>(step * 31);
            in.status.situation_valid = true;
            in.status.situation = static_cast<antz_tracker_situation_e>(step % 5);
            in.status.gps_lost  = (step % 2) == 0;
            in.latitude_bits_0_15 = static_cast<uint16_t>(step * 8191);

            uint8_t page[8] = {};
            CHECK(tracker_encode_location1(&in, page, sizeof(page)) == 0);

            antz_tracker_location1_t back{};
            CHECK(tracker_decode_location1(page, sizeof(page), &back) == 0);
            CHECK(back.asset_index == in.asset_index);
            CHECK(back.distance_m == in.distance_m);
            CHECK(back.bearing_bradians == in.bearing_bradians);
            CHECK(back.latitude_bits_0_15 == in.latitude_bits_0_15);
            CHECK(back.status.situation == in.status.situation);
            CHECK(back.status.gps_lost == in.status.gps_lost);
        }
    }
}

static void location2_round_trips_across_both_hemispheres() {
    // Semicircles are signed. Read unsigned, a dog south of the equator or
    // west of Greenwich lands in the wrong hemisphere — which is invisible in
    // Norway and is exactly why this case is here rather than in the fixed
    // vectors, all of which are northern and eastern.
    const int32_t longitudes[] = {
        0, 1, -1, 130769175, -130769175, 2147483647, -2147483647 - 1,
    };
    const int32_t latitudes[] = {
        0, 757428367, -757428367, 2147483647, -2147483647 - 1,
    };

    for (const int32_t longitude : longitudes) {
        for (const int32_t latitude : latitudes) {
            uint16_t low = 0;
            uint16_t high = 0;
            tracker_latitude_split(latitude, &low, &high);
            CHECK(tracker_latitude(low, high) == latitude);

            antz_tracker_location2_t in{};
            in.asset_index           = 3;
            in.latitude_bits_16_31   = high;
            in.longitude_semicircles = longitude;

            uint8_t page[8] = {};
            CHECK(tracker_encode_location2(&in, page, sizeof(page)) == 0);

            antz_tracker_location2_t back{};
            CHECK(tracker_decode_location2(page, sizeof(page), &back) == 0);
            CHECK(back.asset_index == in.asset_index);
            CHECK(back.latitude_bits_16_31 == in.latitude_bits_16_31);
            CHECK(back.longitude_semicircles == in.longitude_semicircles);
        }
    }
}

static void names_round_trip_at_every_length() {
    // Five characters fill the field with no terminator, which is the case a
    // NUL-terminating encoder would get wrong.
    const char* names[] = { "", "R", "Re", "Rex", "Rexx", "Rexxy" };

    for (const char* name : names) {
        antz_tracker_identification1_t in{};
        in.asset_index = 7;
        in.colour      = 96;
        std::strcpy(in.name, name);

        uint8_t page[8] = {};
        CHECK(tracker_encode_identification1(&in, page, sizeof(page)) == 0);

        antz_tracker_identification1_t back{};
        CHECK(tracker_decode_identification1(page, sizeof(page), &back) == 0);
        CHECK(back.asset_index == in.asset_index);
        CHECK(back.colour == in.colour);
        CHECK(std::strcmp(back.name, name) == 0);
    }
}

static void identification2_round_trips_every_asset_type() {
    // Asset Type 0 is a person and 1 is a dog, and zero is a real value
    // rather than "not set". Both must survive.
    for (int type = 0; type < 4; ++type) {
        antz_tracker_identification2_t in{};
        in.asset_index = 5;
        in.asset_type  = static_cast<uint8_t>(type);
        std::strcpy(in.name, "y");

        uint8_t page[8] = {};
        CHECK(tracker_encode_identification2(&in, page, sizeof(page)) == 0);

        antz_tracker_identification2_t back{};
        CHECK(tracker_decode_identification2(page, sizeof(page), &back) == 0);
        CHECK(back.asset_type == in.asset_type);
        CHECK(std::strcmp(back.name, "y") == 0);
    }
}

// ─── Refusals ──────────────────────────────────────────────────────────────

static void what_the_encoder_refuses() {
    uint8_t page[8] = {};

    antz_tracker_location1_t loc{};
    loc.asset_index = 3;
    CHECK(tracker_encode_location1(nullptr, page, sizeof(page)) != 0);
    CHECK(tracker_encode_location1(&loc, nullptr, sizeof(page)) != 0);
    CHECK(tracker_encode_location1(&loc, page, 7) != 0);

    // The index is five bits. Truncating a larger one would file a position
    // under a different asset.
    loc.asset_index = 0x20;
    CHECK(tracker_encode_location1(&loc, page, sizeof(page)) != 0);

    // Six characters do not fit, and truncating would name a dog something
    // nobody chose.
    //
    // memcpy and not strcpy: name is char[6], so six characters leave no room
    // for a terminator and strcpy would overflow the struct before the
    // encoder ever saw it. An unterminated field is also the shape this
    // actually has to refuse — it is what a caller filling the array by hand
    // produces.
    antz_tracker_identification1_t id{};
    id.asset_index = 3;
    std::memcpy(id.name, "Rexxxx", 6);
    CHECK(tracker_encode_identification1(&id, page, sizeof(page)) != 0);

    CHECK(tracker_encode_no_assets(nullptr, 8) != 0);
    CHECK(tracker_encode_disconnect(page, 7) != 0);
}

static void a_refused_encode_leaves_the_buffer_alone() {
    uint8_t page[8];
    std::memset(page, 0xAA, sizeof(page));

    antz_tracker_location1_t loc{};
    loc.asset_index = 0x20;
    CHECK(tracker_encode_location1(&loc, page, sizeof(page)) != 0);
    for (const unsigned char byte : page) CHECK(byte == 0xAA);
}

int main() {
    location1_matches_the_vector();
    location2_matches_the_vector();
    identification1_matches_the_vector();
    identification2_matches_the_vector();
    the_assetless_pages_match_their_vectors();
    the_reserved_index_bits_are_set();

    the_status_byte_round_trips();
    the_sentinel_survives_a_round_trip();
    an_unnamed_situation_round_trips_to_undefined();

    location1_round_trips();
    location2_round_trips_across_both_hemispheres();
    names_round_trip_at_every_length();
    identification2_round_trips_every_asset_type();

    what_the_encoder_refuses();
    a_refused_encode_leaves_the_buffer_alone();

    if (failures == 0) {
        std::printf("ok — %d checks\n", checks);
        return 0;
    }
    std::printf("FAILED — %d of %d checks\n", failures, checks);
    return 1;
}

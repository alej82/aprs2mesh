#include <unity.h>

#include <cstring>

#include "psk_base64.h"
#include "config.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

Config validConfig() {
  Config config = defaultConfig();
  std::strncpy(config.callsign, "EA1ABC-9", sizeof(config.callsign) - 1);
  config.pskLength = 16;
  return config;
}

}  // namespace

static void test_defaults_wait_for_portal() {
  const Config config = defaultConfig();
  TEST_ASSERT_FALSE(config.hasCallsign());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::Callsign),
                          static_cast<uint8_t>(validateConfig(config)));
  TEST_ASSERT_FALSE(config.stripPath);
  TEST_ASSERT_EQUAL_UINT16(kConfigDefaultPerMinute, config.maxPerMinute);
}

static void test_valid_callsigns() {
  TEST_ASSERT_TRUE(validCallsign("EA1ABC"));
  TEST_ASSERT_TRUE(validCallsign("EA1ABC-9"));
  TEST_ASSERT_TRUE(validCallsign("N0CALL-15"));
  TEST_ASSERT_TRUE(validCallsign("12345"));

  TEST_ASSERT_FALSE(validCallsign(""));
  TEST_ASSERT_FALSE(validCallsign(nullptr));
  TEST_ASSERT_FALSE(validCallsign("TOOLONG7"));
  TEST_ASSERT_FALSE(validCallsign("EA1ABC-16"));
  TEST_ASSERT_FALSE(validCallsign("EA1ABC-"));
  TEST_ASSERT_FALSE(validCallsign("EA1ABC-1A"));
  TEST_ASSERT_FALSE(validCallsign("EA1 ABC"));
  TEST_ASSERT_FALSE(validCallsign("EA1_ABC"));
}

static void test_complete_config_is_valid() {
  const Config config = validConfig();
  TEST_ASSERT_TRUE(config.hasCallsign());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::None),
                          static_cast<uint8_t>(validateConfig(config)));
}

static void test_field_errors_are_identified() {
  Config config = validConfig();

  std::memset(config.channelName, 0, sizeof(config.channelName));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::ChannelName),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.pskLength = 33;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::Psk),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.region = RegionCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::Region),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.preset = ModemPresetCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::Preset),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.maxPerMinute = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::RateLimit),
                          static_cast<uint8_t>(validateConfig(config)));

  config.maxPerMinute = kConfigMaxPerMinute + 1;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::RateLimit),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.minIntervalMs = kConfigMaxIntervalMs + 1;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::MinInterval),
                          static_cast<uint8_t>(validateConfig(config)));

  config = validConfig();
  config.bluetoothName[0] = static_cast<char>(0x7F);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::BluetoothName),
                          static_cast<uint8_t>(validateConfig(config)));
}

static void test_unterminated_field_is_rejected() {
  Config config = validConfig();
  std::memset(config.callsign, 'A', sizeof(config.callsign));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigField::Callsign),
                          static_cast<uint8_t>(validateConfig(config)));
}

static void test_base64_matches_meshtastic_keys() {
  // "AQ==" is the default key marker, "1PG7..." is the 16-byte default PSK as
  // the Meshtastic app displays it.
  char encoded[64] = {};
  const uint8_t marker[] = {1};
  TEST_ASSERT_EQUAL_UINT32(4, encodeBase64(marker, sizeof(marker), encoded, sizeof(encoded)));
  TEST_ASSERT_EQUAL_STRING("AQ==", encoded);

  const uint8_t defaultPsk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                                  0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};
  TEST_ASSERT_EQUAL_UINT32(24, encodeBase64(defaultPsk, sizeof(defaultPsk), encoded,
                                            sizeof(encoded)));
  TEST_ASSERT_EQUAL_STRING("1PG7OiApB1nwvP+rz05pAQ==", encoded);

  uint8_t decoded[32] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(decodeBase64(encoded, decoded, sizeof(decoded), length));
  TEST_ASSERT_EQUAL_UINT32(sizeof(defaultPsk), length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(defaultPsk, decoded, sizeof(defaultPsk));
}

static void test_base64_decode_cases() {
  uint8_t out[32] = {};
  size_t length = 0;

  TEST_ASSERT_TRUE(decodeBase64("", out, sizeof(out), length));
  TEST_ASSERT_EQUAL_UINT32(0, length);

  TEST_ASSERT_TRUE(decodeBase64("AQIDBAU=", out, sizeof(out), length));
  TEST_ASSERT_EQUAL_UINT32(5, length);
  TEST_ASSERT_EQUAL_UINT8(1, out[0]);
  TEST_ASSERT_EQUAL_UINT8(5, out[4]);

  TEST_ASSERT_FALSE(decodeBase64("AQ=", out, sizeof(out), length));
  TEST_ASSERT_FALSE(decodeBase64("A===", out, sizeof(out), length));
  TEST_ASSERT_FALSE(decodeBase64("AQ==AQ==", out, sizeof(out), length));
  TEST_ASSERT_FALSE(decodeBase64("AQ*=", out, sizeof(out), length));
  TEST_ASSERT_FALSE(decodeBase64("AQ", out, sizeof(out), length));

  uint8_t tiny[2] = {};
  TEST_ASSERT_FALSE(decodeBase64("AQIDBAU=", tiny, sizeof(tiny), length));

  char small[3] = {};
  TEST_ASSERT_EQUAL_UINT32(0, encodeBase64(tiny, 2, small, sizeof(small)));
}

static void test_base64_round_trip() {
  uint8_t data[32] = {};
  for (size_t i = 0; i < sizeof(data); ++i) {
    data[i] = static_cast<uint8_t>(i * 7 + 3);
  }

  char encoded[64] = {};
  for (size_t length = 0; length <= sizeof(data); ++length) {
    TEST_ASSERT_TRUE(encodeBase64(data, length, encoded, sizeof(encoded)) ==
                     ((length + 2) / 3) * 4);
    uint8_t decoded[32] = {};
    size_t decodedLength = 0;
    TEST_ASSERT_TRUE(decodeBase64(encoded, decoded, sizeof(decoded), decodedLength));
    TEST_ASSERT_EQUAL_UINT32(length, decodedLength);
    if (length > 0) {
      TEST_ASSERT_EQUAL_UINT8_ARRAY(data, decoded, length);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_wait_for_portal);
  RUN_TEST(test_valid_callsigns);
  RUN_TEST(test_complete_config_is_valid);
  RUN_TEST(test_field_errors_are_identified);
  RUN_TEST(test_unterminated_field_is_rejected);
  RUN_TEST(test_base64_matches_meshtastic_keys);
  RUN_TEST(test_base64_decode_cases);
  RUN_TEST(test_base64_round_trip);
  return UNITY_END();
}

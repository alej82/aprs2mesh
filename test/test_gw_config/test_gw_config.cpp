#include <unity.h>

#include <cstring>

#include "gateway_config.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

GatewayConfig validConfig() {
  GatewayConfig config = defaultGatewayConfig();
  std::strncpy(config.callsign, "EA1ABC-10", sizeof(config.callsign) - 1);
  std::strncpy(config.wifiSsid, "casa", sizeof(config.wifiSsid) - 1);
  std::strncpy(config.wifiPassword, "password123", sizeof(config.wifiPassword) - 1);
  std::strncpy(config.channelName, "LongFast", sizeof(config.channelName) - 1);
  return config;
}

GatewayConfig customConfig() {
  GatewayConfig config = validConfig();
  config.modem.preset = ModemPresetCode::Custom;
  config.modem.frequencyHz = 869525000u;
  config.modem.bandwidth = BandwidthCode::Bw62_5;
  config.modem.spreadFactor = 7;
  config.modem.codingRate = CodingRateCode::Cr4_5;
  config.modem.hopLimit = 3;
  return config;
}

GwField validate(const GatewayConfig& config) { return validateGatewayConfig(config); }

}  // namespace

static void test_defaults_wait_for_portal() {
  const GatewayConfig config = defaultGatewayConfig();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Callsign),
                          static_cast<uint8_t>(validate(config)));
  TEST_ASSERT_FALSE(config.gateOpen);
  TEST_ASSERT_EQUAL_STRING("rotate.aprs2.net", config.host);
  TEST_ASSERT_EQUAL_STRING("m/50", config.filter);
  TEST_ASSERT_EQUAL_UINT16(14580, config.port);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ModemPresetCode::LongFast),
                          static_cast<uint8_t>(config.modem.preset));
  TEST_ASSERT_EQUAL_UINT8(kModemDefaultHopLimit, config.modem.hopLimit);
}

static void test_complete_config_is_valid() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::None),
                          static_cast<uint8_t>(validate(validConfig())));
}

static void test_callsign_is_validated() {
  GatewayConfig config = validConfig();
  std::memset(config.callsign, 0, sizeof(config.callsign));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Callsign),
                          static_cast<uint8_t>(validate(config)));

  std::strncpy(config.callsign, "EA1ABCDE", sizeof(config.callsign) - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Callsign),
                          static_cast<uint8_t>(validate(config)));
}

static void test_host_and_port_are_validated() {
  GatewayConfig config = validConfig();
  config.host[0] = '\0';
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Host),
                          static_cast<uint8_t>(validate(config)));

  std::strncpy(config.host, "rotate aprs2", sizeof(config.host) - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Host),
                          static_cast<uint8_t>(validate(config)));

  std::strncpy(config.host, "rotate.aprs2.net", sizeof(config.host) - 1);
  config.port = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Port),
                          static_cast<uint8_t>(validate(config)));
}

static void test_passcode_range() {
  GatewayConfig config = validConfig();
  config.passcode = 32768;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Passcode),
                          static_cast<uint8_t>(validate(config)));
  config.passcode = 32767;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::None),
                          static_cast<uint8_t>(validate(config)));
}

static void test_filter_rejects_line_breaks_and_length() {
  GatewayConfig config = validConfig();
  std::strncpy(config.filter, "m/50\nr/10", sizeof(config.filter) - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Filter),
                          static_cast<uint8_t>(validate(config)));

  char longFilter[100] = {};
  std::memset(longFilter, 'm', sizeof(longFilter));
  TEST_ASSERT_FALSE(validFilterText(longFilter));
  TEST_ASSERT_FALSE(validFilterText("m/50\r"));
  TEST_ASSERT_TRUE(validFilterText("m/50"));

  config.filter[0] = '\0';
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::None),
                          static_cast<uint8_t>(validate(config)));
}

static void test_policy_numbers() {
  GatewayConfig config = validConfig();

  config.flowTtlMinutes = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::FlowTtl),
                          static_cast<uint8_t>(validate(config)));
  config.flowTtlMinutes = 1441;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::FlowTtl),
                          static_cast<uint8_t>(validate(config)));
  config.flowTtlMinutes = 60;

  config.maxFlows = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::MaxFlows),
                          static_cast<uint8_t>(validate(config)));
  config.maxFlows = 9;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::MaxFlows),
                          static_cast<uint8_t>(validate(config)));
  config.maxFlows = 8;

  config.uplinkPerMinute = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::UplinkRate),
                          static_cast<uint8_t>(validate(config)));
  config.uplinkPerMinute = 61;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::UplinkRate),
                          static_cast<uint8_t>(validate(config)));
  config.uplinkPerMinute = 6;

  config.downlinkPerMinute = 61;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::DownlinkRate),
                          static_cast<uint8_t>(validate(config)));
  config.downlinkPerMinute = 6;

  config.minIntervalMs = static_cast<uint16_t>(kGwMaxIntervalMs + 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::MinInterval),
                          static_cast<uint8_t>(validate(config)));
}

static void test_wifi_and_channel_fields() {
  GatewayConfig config = validConfig();
  std::strncpy(config.wifiPassword, "corta", sizeof(config.wifiPassword) - 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::WifiPassword),
                          static_cast<uint8_t>(validate(config)));

  std::strncpy(config.wifiPassword, "password123", sizeof(config.wifiPassword) - 1);
  config.wifiSsid[0] = '\0';
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::WifiSsid),
                          static_cast<uint8_t>(validate(config)));

  config.wifiPassword[0] = '\0';
  config.channelName[0] = '\0';
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::ChannelName),
                          static_cast<uint8_t>(validate(config)));

  std::strncpy(config.channelName, "LongFast", sizeof(config.channelName) - 1);
  config.pskLength = 33;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Psk),
                          static_cast<uint8_t>(validate(config)));

  config.pskLength = 0;
  config.region = RegionCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Region),
                          static_cast<uint8_t>(validate(config)));
  config.region = RegionCode::Eu868;
  config.modem.preset = ModemPresetCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Preset),
                          static_cast<uint8_t>(validate(config)));
}

static void test_custom_modem_is_valid() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::None),
                          static_cast<uint8_t>(validate(customConfig())));
}

static void test_custom_modem_field_errors() {
  GatewayConfig config = customConfig();
  config.modem.frequencyHz = 869400000u;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Frequency),
                          static_cast<uint8_t>(validate(config)));

  config = customConfig();
  config.modem.bandwidth = BandwidthCode::Bw500;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::Bandwidth),
                          static_cast<uint8_t>(validate(config)));

  config = customConfig();
  config.modem.spreadFactor = 6;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::SpreadFactor),
                          static_cast<uint8_t>(validate(config)));

  config = customConfig();
  config.modem.codingRate = CodingRateCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::CodingRate),
                          static_cast<uint8_t>(validate(config)));

  config = customConfig();
  config.modem.hopLimit = 8;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::HopLimit),
                          static_cast<uint8_t>(validate(config)));
}

static void test_fixed_preset_ignores_custom_fields() {
  GatewayConfig config = validConfig();
  config.modem.frequencyHz = 1;
  config.modem.bandwidth = BandwidthCode::Count;
  config.modem.spreadFactor = 0;
  config.modem.codingRate = CodingRateCode::Count;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(GwField::None),
                          static_cast<uint8_t>(validate(config)));
}

static void test_field_text_is_stable() {
  TEST_ASSERT_EQUAL_STRING("callsign", gwFieldText(GwField::Callsign));
  TEST_ASSERT_EQUAL_STRING("filtro", gwFieldText(GwField::Filter));
  TEST_ASSERT_EQUAL_STRING("ok", gwFieldText(GwField::None));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_wait_for_portal);
  RUN_TEST(test_complete_config_is_valid);
  RUN_TEST(test_callsign_is_validated);
  RUN_TEST(test_host_and_port_are_validated);
  RUN_TEST(test_passcode_range);
  RUN_TEST(test_filter_rejects_line_breaks_and_length);
  RUN_TEST(test_policy_numbers);
  RUN_TEST(test_wifi_and_channel_fields);
  RUN_TEST(test_custom_modem_is_valid);
  RUN_TEST(test_custom_modem_field_errors);
  RUN_TEST(test_fixed_preset_ignores_custom_fields);
  RUN_TEST(test_field_text_is_stable);
  return UNITY_END();
}

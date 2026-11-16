#pragma once

#include <cstddef>
#include <cstdint>

#include "modem.h"

namespace aprs2mesh {

inline constexpr size_t kConfigCallsign = 12;
inline constexpr size_t kConfigBluetoothName = 32;
inline constexpr size_t kConfigChannelName = 16;
inline constexpr size_t kConfigPsk = 32;

inline constexpr uint16_t kConfigMaxPerMinute = 60;
inline constexpr uint32_t kConfigMaxIntervalMs = 60000;
inline constexpr uint16_t kConfigDefaultPerMinute = 6;
inline constexpr uint32_t kConfigDefaultIntervalMs = 10000;

enum class ConfigField : uint8_t {
  None = 0,
  Callsign,
  BluetoothName,
  ChannelName,
  Psk,
  Region,
  Preset,
  Frequency,
  Bandwidth,
  SpreadFactor,
  CodingRate,
  HopLimit,
  RateLimit,
  MinInterval,
};

struct Config {
  char callsign[kConfigCallsign] = {};
  char bluetoothName[kConfigBluetoothName] = {};
  char channelName[kConfigChannelName] = {};
  uint8_t psk[kConfigPsk] = {};
  uint8_t pskLength = 0;
  RegionCode region = RegionCode::Eu868;
  ModemConfig modem;
  bool stripPath = false;
  uint16_t maxPerMinute = kConfigDefaultPerMinute;
  uint16_t minIntervalMs = kConfigDefaultIntervalMs;

  bool hasCallsign() const;
};

// Safe defaults: no callsign, so validateConfig() fails and the device starts
// in portal mode instead of transmitting with an invented identity.
Config defaultConfig();

// Returns the first invalid field, or None when the whole config is usable.
ConfigField validateConfig(const Config& config);

// "CALL" or "CALL-SSID" with 1..6 alphanumeric characters and SSID 0..15.
bool validCallsign(const char* callsign);

const char* configFieldText(ConfigField field);

}  // namespace aprs2mesh

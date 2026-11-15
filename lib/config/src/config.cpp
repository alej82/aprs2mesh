#include "config.h"

#include <cstring>

namespace aprs2mesh {

namespace {

// Length of a fixed-size text field, or `maximum` when it is not terminated.
size_t boundedLength(const char* value, size_t maximum) {
  for (size_t i = 0; i < maximum; ++i) {
    if (value[i] == '\0') {
      return i;
    }
  }
  return maximum;
}

bool printable(const char* value, size_t maximum, bool allowEmpty) {
  const size_t length = boundedLength(value, maximum + 1);
  if (length > maximum) {
    return false;
  }
  if (length == 0) {
    return allowEmpty;
  }
  for (size_t i = 0; i < length; ++i) {
    const auto c = static_cast<unsigned char>(value[i]);
    if (c < 0x20 || c > 0x7E) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool validCallsign(const char* callsign) {
  if (callsign == nullptr) {
    return false;
  }

  size_t base = 0;
  while (base < kConfigCallsign && callsign[base] != '\0' && callsign[base] != '-') {
    const char c = callsign[base];
    const bool alphanumeric = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                              (c >= '0' && c <= '9');
    if (!alphanumeric) {
      return false;
    }
    ++base;
  }
  if (base == 0 || base > 6 || base == kConfigCallsign) {
    return false;
  }
  if (callsign[base] == '\0') {
    return true;
  }

  const char* ssidText = callsign + base + 1;
  const size_t ssidLength = boundedLength(ssidText, 3);
  if (ssidLength == 0 || ssidLength > 2) {
    return false;
  }
  unsigned ssid = 0;
  for (size_t i = 0; i < ssidLength; ++i) {
    if (ssidText[i] < '0' || ssidText[i] > '9') {
      return false;
    }
    ssid = ssid * 10 + static_cast<unsigned>(ssidText[i] - '0');
  }
  return ssid <= 15;
}

bool Config::hasCallsign() const { return validCallsign(callsign); }

Config defaultConfig() {
  Config config;
  std::strncpy(config.bluetoothName, "aprs2mesh", sizeof(config.bluetoothName) - 1);
  std::strncpy(config.channelName, "LongFast", sizeof(config.channelName) - 1);
  return config;
}

ConfigField validateConfig(const Config& config) {
  if (boundedLength(config.callsign, sizeof(config.callsign)) >=
          sizeof(config.callsign) ||
      !validCallsign(config.callsign)) {
    return ConfigField::Callsign;
  }
  if (!printable(config.bluetoothName, kConfigBluetoothName - 1, false)) {
    return ConfigField::BluetoothName;
  }
  if (!printable(config.channelName, kConfigChannelName - 1, false)) {
    return ConfigField::ChannelName;
  }
  if (config.pskLength > kConfigPsk) {
    return ConfigField::Psk;
  }
  if (static_cast<uint8_t>(config.region) >= static_cast<uint8_t>(RegionCode::Count)) {
    return ConfigField::Region;
  }
  if (static_cast<uint8_t>(config.preset) >=
      static_cast<uint8_t>(ModemPresetCode::Count)) {
    return ConfigField::Preset;
  }
  if (config.maxPerMinute == 0 || config.maxPerMinute > kConfigMaxPerMinute) {
    return ConfigField::RateLimit;
  }
  if (config.minIntervalMs > kConfigMaxIntervalMs) {
    return ConfigField::MinInterval;
  }
  return ConfigField::None;
}

const char* configFieldText(ConfigField field) {
  switch (field) {
    case ConfigField::None:
      return "ok";
    case ConfigField::Callsign:
      return "callsign";
    case ConfigField::BluetoothName:
      return "bluetooth name";
    case ConfigField::ChannelName:
      return "channel name";
    case ConfigField::Psk:
      return "psk";
    case ConfigField::Region:
      return "region";
    case ConfigField::Preset:
      return "preset";
    case ConfigField::RateLimit:
      return "rate limit";
    case ConfigField::MinInterval:
      return "minimum interval";
  }
  return "unknown";
}

}  // namespace aprs2mesh

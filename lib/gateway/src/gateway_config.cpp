#include "gateway_config.h"

#include <cstring>

namespace aprs2mesh {

namespace {

size_t boundedLength(const char* text, size_t capacity) {
  if (text == nullptr) {
    return 0;
  }
  size_t length = 0;
  while (length < capacity && text[length] != '\0') {
    ++length;
  }
  return length;
}

bool printable(const char* text, size_t capacity, size_t minimum) {
  const size_t length = boundedLength(text, capacity);
  if (length < minimum || length >= capacity) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x20 || c == 0x7F) {
      return false;
    }
  }
  return true;
}

}  // namespace

GatewayConfig defaultGatewayConfig() {
  GatewayConfig config;
  std::strncpy(config.host, "rotate.aprs2.net", sizeof(config.host) - 1);
  std::strncpy(config.filter, "m/50", sizeof(config.filter) - 1);
  return config;
}

bool validFilterText(const char* filter) {
  const size_t length = boundedLength(filter, kGwFilter);
  if (length >= kGwFilter) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(filter[i]);
    // The filter is concatenated into a line protocol: no CR, LF or controls.
    if (c < 0x20 || c > 0x7E) {
      return false;
    }
  }
  return true;
}

bool validHostText(const char* host) {
  const size_t length = boundedLength(host, kGwHost);
  if (length == 0 || length >= kGwHost) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const char c = host[i];
    const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    const bool digit = c >= '0' && c <= '9';
    if (!letter && !digit && c != '.' && c != '-') {
      return false;
    }
  }
  return true;
}

GwField validateGatewayConfig(const GatewayConfig& config) {
  if (!validCallsign(config.callsign)) {
    return GwField::Callsign;
  }
  if (!validHostText(config.host)) {
    return GwField::Host;
  }
  if (config.port == 0) {
    return GwField::Port;
  }
  if (config.passcode > kGwMaxPasscode) {
    return GwField::Passcode;
  }
  if (!validFilterText(config.filter)) {
    return GwField::Filter;
  }
  if (config.flowTtlMinutes == 0 || config.flowTtlMinutes > kGwMaxFlowTtlMinutes) {
    return GwField::FlowTtl;
  }
  if (config.maxFlows == 0 || config.maxFlows > kGwMaxFlows) {
    return GwField::MaxFlows;
  }
  if (config.uplinkPerMinute == 0 || config.uplinkPerMinute > kGwMaxPerMinute) {
    return GwField::UplinkRate;
  }
  if (config.downlinkPerMinute == 0 || config.downlinkPerMinute > kGwMaxPerMinute) {
    return GwField::DownlinkRate;
  }
  if (config.minIntervalMs > kGwMaxIntervalMs) {
    return GwField::MinInterval;
  }
  if (!printable(config.wifiSsid, sizeof(config.wifiSsid), 0)) {
    return GwField::WifiSsid;
  }
  const size_t passwordLength =
      boundedLength(config.wifiPassword, sizeof(config.wifiPassword));
  if (passwordLength >= sizeof(config.wifiPassword)) {
    return GwField::WifiPassword;
  }
  if (passwordLength > 0 && passwordLength < 8) {
    return GwField::WifiPassword;
  }
  if (!printable(config.wifiPassword, sizeof(config.wifiPassword), 0)) {
    return GwField::WifiPassword;
  }
  const size_t ssidLength = boundedLength(config.wifiSsid, sizeof(config.wifiSsid));
  if (passwordLength > 0 && ssidLength == 0) {
    return GwField::WifiSsid;
  }
  if (!printable(config.channelName, sizeof(config.channelName), 1)) {
    return GwField::ChannelName;
  }
  if (config.pskLength > kGwPsk) {
    return GwField::Psk;
  }
  if (static_cast<uint8_t>(config.region) >= static_cast<uint8_t>(RegionCode::Count)) {
    return GwField::Region;
  }
  if (static_cast<uint8_t>(config.preset) >=
      static_cast<uint8_t>(ModemPresetCode::Count)) {
    return GwField::Preset;
  }
  return GwField::None;
}

const char* gwFieldText(GwField field) {
  switch (field) {
    case GwField::None:
      return "ok";
    case GwField::Callsign:
      return "callsign";
    case GwField::Host:
      return "servidor APRS-IS";
    case GwField::Port:
      return "puerto APRS-IS";
    case GwField::Passcode:
      return "passcode";
    case GwField::Filter:
      return "filtro";
    case GwField::FlowTtl:
      return "TTL de flujos";
    case GwField::MaxFlows:
      return "máximo de flujos";
    case GwField::UplinkRate:
      return "límite de subida";
    case GwField::DownlinkRate:
      return "límite de bajada";
    case GwField::MinInterval:
      return "intervalo mínimo";
    case GwField::WifiSsid:
      return "SSID de WiFi";
    case GwField::WifiPassword:
      return "clave de WiFi";
    case GwField::ChannelName:
      return "nombre del canal";
    case GwField::Psk:
      return "PSK del canal";
    case GwField::Region:
      return "región";
    case GwField::Preset:
      return "preset";
  }
  return "?";
}

}  // namespace aprs2mesh

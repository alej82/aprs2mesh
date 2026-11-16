#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"

namespace aprs2mesh {

// Field capacities. They include the terminating NUL.
inline constexpr size_t kGwCallsign = 12;
inline constexpr size_t kGwHost = 48;
inline constexpr size_t kGwFilter = 96;
inline constexpr size_t kGwSsid = 33;
inline constexpr size_t kGwPassword = 64;
inline constexpr size_t kGwChannelName = 16;
inline constexpr size_t kGwPsk = 32;

inline constexpr uint16_t kGwDefaultPort = 14580;
inline constexpr uint16_t kGwDefaultFlowTtlMinutes = 60;
inline constexpr uint8_t kGwDefaultMaxFlows = 8;
inline constexpr uint8_t kGwMaxFlows = 8;
inline constexpr uint16_t kGwDefaultPerMinute = 6;
inline constexpr uint32_t kGwDefaultIntervalMs = 10000;
inline constexpr uint16_t kGwMaxPerMinute = 60;
inline constexpr uint32_t kGwMaxIntervalMs = 60000;
inline constexpr uint16_t kGwMaxFlowTtlMinutes = 1440;
inline constexpr uint16_t kGwMaxPasscode = 32767;

// First invalid field, or None when the whole configuration is usable. The
// order is stable so the portal can point at one field.
enum class GwField : uint8_t {
  None = 0,
  Callsign,
  Host,
  Port,
  Passcode,
  Filter,
  FlowTtl,
  MaxFlows,
  UplinkRate,
  DownlinkRate,
  MinInterval,
  WifiSsid,
  WifiPassword,
  ChannelName,
  Psk,
  Region,
  Preset,
  Frequency,
  Bandwidth,
  SpreadFactor,
  CodingRate,
  HopLimit,
};

// Everything the gateway needs to reach APRS-IS and to join the mesh channel.
// The gate starts closed (task 7.1): provisioning must open it on purpose.
struct GatewayConfig {
  char callsign[kGwCallsign] = {};
  char host[kGwHost] = "rotate.aprs2.net";
  uint16_t port = kGwDefaultPort;
  uint16_t passcode = 0;
  char filter[kGwFilter] = "m/50";
  uint16_t flowTtlMinutes = kGwDefaultFlowTtlMinutes;
  uint8_t maxFlows = kGwDefaultMaxFlows;
  uint16_t uplinkPerMinute = kGwDefaultPerMinute;
  uint16_t downlinkPerMinute = kGwDefaultPerMinute;
  uint16_t minIntervalMs = static_cast<uint16_t>(kGwDefaultIntervalMs);
  bool gateOpen = false;
  char wifiSsid[kGwSsid] = {};
  char wifiPassword[kGwPassword] = {};
  char channelName[kGwChannelName] = {};
  uint8_t psk[kGwPsk] = {};
  uint8_t pskLength = 0;
  RegionCode region = RegionCode::Eu868;
  ModemConfig modem;
};

// No callsign, so validateGatewayConfig() fails and the device starts in
// portal mode instead of opening a session with an invented identity.
GatewayConfig defaultGatewayConfig();

GwField validateGatewayConfig(const GatewayConfig& config);

const char* gwFieldText(GwField field);

// Hostnames and IPv4 literals, without spaces or control characters.
bool validFilterText(const char* filter);
bool validHostText(const char* host);

}  // namespace aprs2mesh

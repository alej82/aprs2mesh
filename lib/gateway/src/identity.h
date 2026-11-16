#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh_proto.h"
#include "modem.h"

namespace aprs2mesh {

// Meshtastic Role.CLIENT_MUTE.
inline constexpr uint8_t kGatewayRoleClientMute = 1;
inline constexpr uint8_t kGatewayHopLimit = 3;
inline constexpr uint32_t kNodeInfoIntervalMs = 30u * 60u * 1000u;
inline constexpr size_t kGatewayShortName = 4;

struct GatewayIdentity {
  char longName[kMaxLongName] = {};
  char shortName[kMaxShortName] = {};
  uint32_t nodeNumber = 0;
  uint8_t role = kGatewayRoleClientMute;
  uint8_t hopLimit = kGatewayHopLimit;
};

// long_name is the gateway callsign; short_name keeps up to four characters of
// its base. The hardware model is left unset: the radio layer owns it.
bool buildGatewayIdentity(const char* callsign, uint32_t nodeNumber,
                          uint8_t hopLimit, GatewayIdentity& out);

// Builds the NodeInfo MeshPacket (Data.portnum = NODEINFO_APP) and schedules a
// periodic announcement. The radio layer only has to transmit the bytes.
class NodeInfoScheduler {
 public:
  void configure(const GatewayIdentity& identity, uint32_t nowMs);
  bool due(uint32_t nowMs) const;
  size_t buildPacket(uint32_t packetId, uint8_t* out, size_t capacity) const;
  void markSent(uint32_t nowMs);
  void announceNow(uint32_t nowMs);

  const GatewayIdentity& identity() const { return identity_; }
  uint32_t nextAt() const { return nextAt_; }

 private:
  GatewayIdentity identity_;
  uint32_t nextAt_ = 0;
  bool announced_ = false;
};

}  // namespace aprs2mesh

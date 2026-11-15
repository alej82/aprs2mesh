#include "identity.h"

#include <cstdio>
#include <cstring>

namespace aprs2mesh {

bool buildGatewayIdentity(const char* callsign, uint32_t nodeNumber,
                          GatewayIdentity& out) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return false;
  }

  GatewayIdentity identity;
  std::strncpy(identity.longName, callsign, sizeof(identity.longName) - 1);

  size_t baseLength = 0;
  while (callsign[baseLength] != '\0' && callsign[baseLength] != '-' &&
         baseLength < kGatewayShortName) {
    char c = callsign[baseLength];
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
    identity.shortName[baseLength] = c;
    ++baseLength;
  }
  identity.shortName[baseLength] = '\0';
  identity.nodeNumber = nodeNumber;
  identity.role = kGatewayRoleClientMute;
  identity.hopLimit = kGatewayHopLimit;

  out = identity;
  return true;
}

void NodeInfoScheduler::configure(const GatewayIdentity& identity, uint32_t nowMs) {
  identity_ = identity;
  nextAt_ = nowMs;
  announced_ = false;
}

bool NodeInfoScheduler::due(uint32_t nowMs) const {
  return !announced_ || static_cast<int32_t>(nowMs - nextAt_) >= 0;
}

size_t NodeInfoScheduler::buildPacket(uint32_t packetId, uint8_t* out,
                                      size_t capacity) const {
  UserInfo user;
  std::snprintf(user.id, sizeof(user.id), "!%08x",
                static_cast<unsigned>(identity_.nodeNumber));
  std::strncpy(user.longName, identity_.longName, sizeof(user.longName) - 1);
  std::strncpy(user.shortName, identity_.shortName, sizeof(user.shortName) - 1);
  user.role = identity_.role;
  user.licensed = false;

  uint8_t userBytes[kMaxTextBytes] = {};
  const size_t userLength = encodeUser(user, userBytes, sizeof(userBytes));
  if (userLength == 0) {
    return 0;
  }

  MeshPacketInfo packet;
  packet.from = identity_.nodeNumber;
  packet.to = kBroadcastAddress;
  packet.id = packetId;
  packet.channel = 0;
  packet.hopLimit = identity_.hopLimit;
  packet.wantAck = false;
  packet.hasDecoded = true;
  packet.data.portnum = kPortNodeInfo;
  std::memcpy(packet.data.payload, userBytes, userLength);
  packet.data.payloadLength = userLength;

  return encodeMeshPacket(packet, out, capacity);
}

void NodeInfoScheduler::markSent(uint32_t nowMs) {
  announced_ = true;
  nextAt_ = nowMs + kNodeInfoIntervalMs;
}

void NodeInfoScheduler::announceNow(uint32_t nowMs) {
  announced_ = false;
  nextAt_ = nowMs;
}

}  // namespace aprs2mesh

#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

inline constexpr uint32_t kPortTextMessage = 1;
inline constexpr uint32_t kPortNodeInfo = 4;
inline constexpr uint32_t kBroadcastAddress = 0xFFFFFFFFu;

inline constexpr size_t kMaxTextBytes = 200;
inline constexpr size_t kMaxUserId = 20;
inline constexpr size_t kMaxLongName = 40;
inline constexpr size_t kMaxShortName = 8;

// Data { portnum, payload } - the submessage that gets encrypted per channel.
struct DataMessage {
  uint32_t portnum = 0;
  uint8_t payload[kMaxTextBytes] = {};
  size_t payloadLength = 0;
};

// Decoded view of a MeshPacket. Only `decoded` payloads carry a portnum;
// `encrypted` ones are reported with hasDecoded = false.
struct MeshPacketInfo {
  uint32_t from = 0;
  uint32_t to = 0;
  uint32_t id = 0;
  uint32_t channel = 0;
  uint32_t hopLimit = 0;
  bool wantAck = false;
  bool hasDecoded = false;
  DataMessage data;
};

struct UserInfo {
  char id[kMaxUserId] = {};
  char longName[kMaxLongName] = {};
  char shortName[kMaxShortName] = {};
  uint32_t hwModel = 0;
  uint32_t role = 0;
  bool licensed = false;
};

// Data submessage.
size_t encodeData(const DataMessage& data, uint8_t* out, size_t capacity);
bool decodeData(const uint8_t* data, size_t length, DataMessage& out);

// Full MeshPacket with a decoded payload variant.
size_t encodeMeshPacket(const MeshPacketInfo& packet, uint8_t* out, size_t capacity);
bool decodeMeshPacket(const uint8_t* data, size_t length, MeshPacketInfo& out);

// User submessage (NodeInfo payload).
size_t encodeUser(const UserInfo& user, uint8_t* out, size_t capacity);
bool decodeUser(const uint8_t* data, size_t length, UserInfo& out);

}  // namespace aprs2mesh

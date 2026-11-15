#include "mesh_frame.h"

#include <cstring>

namespace aprs2mesh {

uint8_t xorHash(const uint8_t* data, size_t length) {
  uint8_t hash = 0;
  for (size_t i = 0; i < length; ++i) {
    hash ^= data[i];
  }
  return hash;
}

uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLength) {
  if (name == nullptr) {
    name = "";
  }
  uint8_t hash = xorHash(reinterpret_cast<const uint8_t*>(name), std::strlen(name));
  if (key != nullptr) {
    hash ^= xorHash(key, keyLength);
  }
  return hash;
}

bool resolveChannelKey(const uint8_t* psk, size_t pskLength, uint8_t* key,
                       size_t keyCapacity, size_t& keyLength) {
  if (key == nullptr || keyCapacity < 16) {
    return false;
  }

  if (pskLength == 0) {
    keyLength = 0;
    return false;  // encryption disabled
  }

  std::memset(key, 0, keyCapacity);

  if (pskLength == 1) {
    if (psk[0] == 0) {
      keyLength = 0;
      return false;  // explicitly disabled
    }
    std::memcpy(key, kDefaultPsk, sizeof(kDefaultPsk));
    key[sizeof(kDefaultPsk) - 1] =
        static_cast<uint8_t>(key[sizeof(kDefaultPsk) - 1] + psk[0] - 1);
    keyLength = sizeof(kDefaultPsk);
    return true;
  }

  if (pskLength > keyCapacity) {
    return false;
  }

  const size_t effective = (pskLength < 16) ? 16 : pskLength;
  std::memcpy(key, psk, pskLength);
  keyLength = effective;
  return true;
}

void buildNonce(uint32_t fromNode, uint64_t packetId, uint8_t nonce[16]) {
  std::memset(nonce, 0, 16);
  for (size_t i = 0; i < 8; ++i) {
    nonce[i] = static_cast<uint8_t>((packetId >> (8 * i)) & 0xFF);
  }
  for (size_t i = 0; i < 4; ++i) {
    nonce[8 + i] = static_cast<uint8_t>((fromNode >> (8 * i)) & 0xFF);
  }
}

namespace {

void putLe32(uint8_t* out, uint32_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

uint32_t getLe32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
         (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

}  // namespace

void encodeHeader(const PacketHeader& header, uint8_t out[kHeaderLength]) {
  putLe32(out, header.to);
  putLe32(out + 4, header.from);
  putLe32(out + 8, header.id);
  uint8_t flags = static_cast<uint8_t>(header.hopLimit & kFlagHopLimitMask);
  if (header.wantAck) {
    flags = static_cast<uint8_t>(flags | kFlagWantAck);
  }
  flags = static_cast<uint8_t>(flags | ((header.hopStart & kFlagHopLimitMask) << kFlagHopStartShift));
  out[12] = flags;
  out[13] = header.channel;
  out[14] = header.nextHop;
  out[15] = header.relayNode;
}

bool decodeHeader(const uint8_t in[kHeaderLength], PacketHeader& out) {
  PacketHeader header;
  header.to = getLe32(in);
  header.from = getLe32(in + 4);
  header.id = getLe32(in + 8);
  const uint8_t flags = in[12];
  header.hopLimit = static_cast<uint8_t>(flags & kFlagHopLimitMask);
  header.wantAck = (flags & kFlagWantAck) != 0;
  header.hopStart = static_cast<uint8_t>((flags & kFlagHopStartMask) >> kFlagHopStartShift);
  header.channel = in[13];
  header.nextHop = in[14];
  header.relayNode = in[15];
  out = header;
  return true;
}

}  // namespace aprs2mesh

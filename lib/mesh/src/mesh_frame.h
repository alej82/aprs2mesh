#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// Meshtastic default PSK (what "AQ==" expands to).
inline constexpr uint8_t kDefaultPsk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29,
                                            0x07, 0x59, 0xf0, 0xbc, 0xff, 0xab,
                                            0xcf, 0x4e, 0x69, 0x01};

inline constexpr size_t kMaxChannelKey = 32;
inline constexpr size_t kHeaderLength = 16;

inline constexpr uint8_t kFlagHopLimitMask = 0x07;
inline constexpr uint8_t kFlagWantAck = 0x08;
inline constexpr uint8_t kFlagViaMqtt = 0x10;
inline constexpr uint8_t kFlagHopStartMask = 0xE0;
inline constexpr uint8_t kFlagHopStartShift = 5;

// XOR of every byte. Used for the channel hash and for quick comparisons.
uint8_t xorHash(const uint8_t* data, size_t length);

// Channel hash: xorHash(name) ^ xorHash(key), as the firmware computes it.
uint8_t channelHash(const char* name, const uint8_t* key, size_t keyLength);

// Applies the firmware's PSK expansion rules:
//   length 0      -> no encryption (returns false)
//   length 1      -> 0 disables, 1..255 expands the default key (last byte
//                    bumped by index - 1)
//   length < 16   -> zero padded to 16
//   length 16/32  -> used as is
bool resolveChannelKey(const uint8_t* psk, size_t pskLength, uint8_t* key,
                       size_t keyCapacity, size_t& keyLength);

// AES-CTR nonce: LE64(packetId) || LE32(fromNode) || zero padding.
void buildNonce(uint32_t fromNode, uint64_t packetId, uint8_t nonce[16]);

// The 16-byte header that precedes the encrypted payload on air.
struct PacketHeader {
  uint32_t to = 0xFFFFFFFFu;
  uint32_t from = 0;
  uint32_t id = 0;
  uint8_t hopLimit = 3;
  uint8_t hopStart = 3;
  bool wantAck = false;
  uint8_t channel = 0;
  uint8_t nextHop = 0;
  uint8_t relayNode = 0;
};

void encodeHeader(const PacketHeader& header, uint8_t out[kHeaderLength]);
bool decodeHeader(const uint8_t in[kHeaderLength], PacketHeader& out);

}  // namespace aprs2mesh

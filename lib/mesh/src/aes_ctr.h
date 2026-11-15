#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// AES block cipher (FIPS-197), encryption direction only. Meshtastic uses
// AES-128 or AES-256 depending on the channel key length.
class Aes {
 public:
  bool setKey(const uint8_t* key, size_t length);
  void encryptBlock(const uint8_t in[16], uint8_t out[16]) const;
  uint8_t rounds() const { return rounds_; }

 private:
  uint8_t roundKeys_[240] = {};
  uint8_t rounds_ = 0;
};

// AES-CTR with a 32-bit big-endian counter in the last four bytes, matching
// the firmware's counter size.
void aesCtr(const uint8_t* key, size_t keyLength, const uint8_t nonce[16],
            uint8_t* data, size_t length);

// Encrypts or decrypts a Meshtastic payload in place (CTR is symmetric).
// The key must already be resolved with resolveChannelKey().
bool encryptPayload(const uint8_t* key, size_t keyLength, uint32_t fromNode,
                    uint64_t packetId, uint8_t* data, size_t length);

}  // namespace aprs2mesh

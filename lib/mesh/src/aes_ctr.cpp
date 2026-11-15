#include "aes_ctr.h"

#include <cstring>

#include "mesh_frame.h"

namespace aprs2mesh {
namespace {

constexpr uint8_t kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

uint8_t xtime(uint8_t value) {
  const uint8_t high = static_cast<uint8_t>(value & 0x80);
  uint8_t result = static_cast<uint8_t>(value << 1);
  if (high != 0) {
    result = static_cast<uint8_t>(result ^ 0x1B);
  }
  return result;
}

void subBytes(uint8_t state[16]) {
  for (size_t i = 0; i < 16; ++i) {
    state[i] = kSbox[state[i]];
  }
}

void shiftRows(uint8_t state[16]) {
  uint8_t shifted[16] = {};
  for (size_t column = 0; column < 4; ++column) {
    for (size_t row = 0; row < 4; ++row) {
      shifted[row + 4 * column] = state[row + 4 * ((column + row) & 3)];
    }
  }
  std::memcpy(state, shifted, sizeof(shifted));
}

void mixColumns(uint8_t state[16]) {
  for (size_t column = 0; column < 4; ++column) {
    uint8_t* a = state + 4 * column;
    const uint8_t all = static_cast<uint8_t>(a[0] ^ a[1] ^ a[2] ^ a[3]);
    const uint8_t a0 = a[0];
    const uint8_t a1 = a[1];
    const uint8_t a2 = a[2];
    const uint8_t a3 = a[3];
    a[0] = static_cast<uint8_t>(a0 ^ all ^ xtime(static_cast<uint8_t>(a0 ^ a1)));
    a[1] = static_cast<uint8_t>(a1 ^ all ^ xtime(static_cast<uint8_t>(a1 ^ a2)));
    a[2] = static_cast<uint8_t>(a2 ^ all ^ xtime(static_cast<uint8_t>(a2 ^ a3)));
    a[3] = static_cast<uint8_t>(a3 ^ all ^ xtime(static_cast<uint8_t>(a3 ^ a0)));
  }
}

void addRoundKey(uint8_t state[16], const uint8_t* roundKeys, size_t round) {
  const uint8_t* key = roundKeys + 16 * round;
  for (size_t i = 0; i < 16; ++i) {
    state[i] = static_cast<uint8_t>(state[i] ^ key[i]);
  }
}

}  // namespace

bool Aes::setKey(const uint8_t* key, size_t length) {
  if (key == nullptr || (length != 16 && length != 24 && length != 32)) {
    return false;
  }

  const size_t nk = length / 4;
  rounds_ = static_cast<uint8_t>(nk + 6);
  const size_t totalWords = 4 * (static_cast<size_t>(rounds_) + 1);
  std::memcpy(roundKeys_, key, length);

  uint8_t rcon = 1;
  for (size_t word = nk; word < totalWords; ++word) {
    uint8_t temp[4] = {};
    std::memcpy(temp, roundKeys_ + 4 * (word - 1), sizeof(temp));

    if (word % nk == 0) {
      const uint8_t first = temp[0];
      temp[0] = static_cast<uint8_t>(kSbox[temp[1]] ^ rcon);
      temp[1] = kSbox[temp[2]];
      temp[2] = kSbox[temp[3]];
      temp[3] = kSbox[first];
      rcon = xtime(rcon);
    } else if (nk > 6 && word % nk == 4) {
      for (size_t i = 0; i < 4; ++i) {
        temp[i] = kSbox[temp[i]];
      }
    }

    for (size_t i = 0; i < 4; ++i) {
      roundKeys_[4 * word + i] =
          static_cast<uint8_t>(roundKeys_[4 * (word - nk) + i] ^ temp[i]);
    }
  }
  return true;
}

void Aes::encryptBlock(const uint8_t in[16], uint8_t out[16]) const {
  uint8_t state[16] = {};
  std::memcpy(state, in, sizeof(state));

  addRoundKey(state, roundKeys_, 0);
  for (uint8_t round = 1; round < rounds_; ++round) {
    subBytes(state);
    shiftRows(state);
    mixColumns(state);
    addRoundKey(state, roundKeys_, round);
  }
  subBytes(state);
  shiftRows(state);
  addRoundKey(state, roundKeys_, rounds_);

  std::memcpy(out, state, sizeof(state));
}

void aesCtr(const uint8_t* key, size_t keyLength, const uint8_t nonce[16],
            uint8_t* data, size_t length) {
  if (key == nullptr || nonce == nullptr || (data == nullptr && length > 0)) {
    return;
  }

  Aes aes;
  if (!aes.setKey(key, keyLength)) {
    return;
  }

  uint8_t counter[16] = {};
  std::memcpy(counter, nonce, sizeof(counter));

  uint8_t keystream[16] = {};
  size_t offset = 0;
  while (offset < length) {
    aes.encryptBlock(counter, keystream);
    const size_t block = (length - offset < 16) ? (length - offset) : 16;
    for (size_t i = 0; i < block; ++i) {
      data[offset + i] = static_cast<uint8_t>(data[offset + i] ^ keystream[i]);
    }
    offset += block;

    for (int i = 15; i >= 12; --i) {
      counter[i] = static_cast<uint8_t>(counter[i] + 1);
      if (counter[i] != 0) {
        break;
      }
    }
  }
}

bool encryptPayload(const uint8_t* key, size_t keyLength, uint32_t fromNode,
                    uint64_t packetId, uint8_t* data, size_t length) {
  if (key == nullptr || data == nullptr || length == 0) {
    return false;
  }
  if (keyLength != 16 && keyLength != 32) {
    return false;
  }

  uint8_t nonce[16] = {};
  buildNonce(fromNode, packetId, nonce);
  aesCtr(key, keyLength, nonce, data, length);
  return true;
}

}  // namespace aprs2mesh

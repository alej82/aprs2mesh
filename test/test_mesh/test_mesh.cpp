#include <unity.h>

#include <cstring>

#include "aes_ctr.h"
#include "mesh_frame.h"
#include "mesh_proto.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

static void fromHex(const char* hex, uint8_t* out, size_t& length) {
  length = 0;
  while (hex[0] != '\0' && hex[1] != '\0') {
    auto nibble = [](char c) -> uint8_t {
      if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
      if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
      return static_cast<uint8_t>(c - 'A' + 10);
    };
    out[length++] = static_cast<uint8_t>((nibble(hex[0]) << 4) | nibble(hex[1]));
    hex += 2;
  }
}

static void test_aes_block_vectors() {
  uint8_t key128[16] = {};
  uint8_t key256[32] = {};
  uint8_t plain[16] = {};
  uint8_t expected[16] = {};
  uint8_t out[16] = {};
  size_t n = 0;

  fromHex("000102030405060708090a0b0c0d0e0f", key128, n);
  TEST_ASSERT_EQUAL_UINT32(16, n);
  fromHex("00112233445566778899aabbccddeeff", plain, n);
  fromHex("69c4e0d86a7b0430d8cdb78070b4c55a", expected, n);

  Aes aes128;
  TEST_ASSERT_TRUE(aes128.setKey(key128, sizeof(key128)));
  TEST_ASSERT_EQUAL_UINT8(10, aes128.rounds());
  aes128.encryptBlock(plain, out);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 16);

  fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", key256, n);
  TEST_ASSERT_EQUAL_UINT32(32, n);
  fromHex("8ea2b7ca516745bfeafc49904b496089", expected, n);

  Aes aes256;
  TEST_ASSERT_TRUE(aes256.setKey(key256, sizeof(key256)));
  TEST_ASSERT_EQUAL_UINT8(14, aes256.rounds());
  aes256.encryptBlock(plain, out);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, 16);
}

static void test_aes_ctr_vectors() {
  uint8_t key[32] = {};
  uint8_t counter[16] = {};
  uint8_t data[32] = {};
  uint8_t expected[32] = {};
  size_t n = 0;

  // NIST SP 800-38A F.5.1 (AES-128-CTR).
  fromHex("2b7e151628aed2a6abf7158809cf4f3c", key, n);
  fromHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", counter, n);
  fromHex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51", data, n);
  fromHex("874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff", expected, n);

  aesCtr(key, 16, counter, data, n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, data, n);

  // NIST SP 800-38A F.5.5 (AES-256-CTR), first block.
  fromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", key, n);
  fromHex("6bc1bee22e409f96e93d7e117393172a", data, n);
  fromHex("601ec313775789a5b7a7f504bbf3d228", expected, n);

  aesCtr(key, 32, counter, data, n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, data, n);
}

static void test_ctr_round_trip_with_meshtastic_nonce() {
  uint8_t key[16] = {};
  size_t n = 0;
  fromHex("2b7e151628aed2a6abf7158809cf4f3c", key, n);

  const uint8_t original[] = "TEXT_MESSAGE_PAYLOAD";
  uint8_t buffer[sizeof(original)] = {};
  std::memcpy(buffer, original, sizeof(original));

  TEST_ASSERT_TRUE(encryptPayload(key, sizeof(key), 0x11223344, 0x0102030405060708,
                                  buffer, sizeof(buffer)));
  TEST_ASSERT_TRUE(std::memcmp(buffer, original, sizeof(original)) != 0);

  TEST_ASSERT_TRUE(encryptPayload(key, sizeof(key), 0x11223344, 0x0102030405060708,
                                  buffer, sizeof(buffer)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(original, buffer, sizeof(original));
}

static void test_nonce_layout() {
  uint8_t nonce[16] = {};
  buildNonce(0x11223344, 0x0102030405060708ULL, nonce);
  const uint8_t expected[16] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
                                0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, nonce, 16);
}

static void test_default_channel_hash() {
  // LongFast with the default PSK is the well-known channel hash 0x08.
  TEST_ASSERT_EQUAL_UINT8(0x08, channelHash("LongFast", kDefaultPsk, sizeof(kDefaultPsk)));
  TEST_ASSERT_EQUAL_UINT8(0x00, channelHash("", nullptr, 0));
  TEST_ASSERT_EQUAL_UINT8('A' ^ 'B', channelHash("AB", nullptr, 0));
}

static void test_resolve_channel_key() {
  uint8_t key[32] = {};
  size_t keyLength = 0;

  const uint8_t defaultMarker[1] = {1};
  TEST_ASSERT_TRUE(resolveChannelKey(defaultMarker, 1, key, sizeof(key), keyLength));
  TEST_ASSERT_EQUAL_UINT32(16, keyLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kDefaultPsk, key, 16);

  const uint8_t disabled[1] = {0};
  TEST_ASSERT_FALSE(resolveChannelKey(disabled, 1, key, sizeof(key), keyLength));
  TEST_ASSERT_EQUAL_UINT32(0, keyLength);

  const uint8_t shortPsk[2] = {0xAA, 0xBB};
  TEST_ASSERT_TRUE(resolveChannelKey(shortPsk, 2, key, sizeof(key), keyLength));
  TEST_ASSERT_EQUAL_UINT32(16, keyLength);
  TEST_ASSERT_EQUAL_UINT8(0xAA, key[0]);
  TEST_ASSERT_EQUAL_UINT8(0xBB, key[1]);
  TEST_ASSERT_EQUAL_UINT8(0x00, key[2]);

  uint8_t longPsk[32] = {};
  TEST_ASSERT_TRUE(resolveChannelKey(longPsk, 32, key, sizeof(key), keyLength));
  TEST_ASSERT_EQUAL_UINT32(32, keyLength);
}

static void test_packet_header_layout() {
  PacketHeader header;
  header.to = 0xFFFFFFFFu;
  header.from = 0x11223344u;
  header.id = 0x01020304u;
  header.hopLimit = 3;
  header.hopStart = 3;
  header.wantAck = true;
  header.channel = 0x08;
  header.nextHop = 0;
  header.relayNode = 0;

  uint8_t encoded[kHeaderLength] = {};
  encodeHeader(header, encoded);

  const uint8_t expected[kHeaderLength] = {
      0xFF, 0xFF, 0xFF, 0xFF,  // to
      0x44, 0x33, 0x22, 0x11,  // from
      0x04, 0x03, 0x02, 0x01,  // id
      0x6B,                    // 3 | wantAck | (3 << 5)
      0x08, 0x00, 0x00,
  };
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, encoded, kHeaderLength);

  PacketHeader decoded;
  TEST_ASSERT_TRUE(decodeHeader(encoded, decoded));
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, decoded.to);
  TEST_ASSERT_EQUAL_UINT32(0x11223344u, decoded.from);
  TEST_ASSERT_EQUAL_UINT32(0x01020304u, decoded.id);
  TEST_ASSERT_EQUAL_UINT8(3, decoded.hopLimit);
  TEST_ASSERT_EQUAL_UINT8(3, decoded.hopStart);
  TEST_ASSERT_TRUE(decoded.wantAck);
  TEST_ASSERT_EQUAL_UINT8(0x08, decoded.channel);
}

static void test_data_encoding_bytes() {
  DataMessage data;
  data.portnum = kPortTextMessage;
  data.payload[0] = 'h';
  data.payload[1] = 'i';
  data.payloadLength = 2;

  uint8_t encoded[32] = {};
  const size_t length = encodeData(data, encoded, sizeof(encoded));
  const uint8_t expected[] = {0x08, 0x01, 0x12, 0x02, 'h', 'i'};
  TEST_ASSERT_EQUAL_UINT32(sizeof(expected), length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, encoded, sizeof(expected));

  DataMessage decoded;
  TEST_ASSERT_TRUE(decodeData(encoded, length, decoded));
  TEST_ASSERT_EQUAL_UINT32(kPortTextMessage, decoded.portnum);
  TEST_ASSERT_EQUAL_UINT32(2, decoded.payloadLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected + 4, decoded.payload, 2);
}

static void test_mesh_packet_encoding_bytes() {
  MeshPacketInfo packet;
  packet.from = 0x11223344u;
  packet.to = kBroadcastAddress;
  packet.id = 0x01020304u;
  packet.channel = 0;
  packet.hopLimit = 3;
  packet.data.portnum = kPortTextMessage;
  packet.data.payload[0] = 'h';
  packet.data.payload[1] = 'i';
  packet.data.payloadLength = 2;

  uint8_t encoded[64] = {};
  const size_t length = encodeMeshPacket(packet, encoded, sizeof(encoded));

  const uint8_t expected[] = {
      0x0D, 0x44, 0x33, 0x22, 0x11,              // from (fixed32)
      0x15, 0xFF, 0xFF, 0xFF, 0xFF,              // to (fixed32)
      0x18, 0x00,                                // channel = 0
      0x22, 0x06, 0x08, 0x01, 0x12, 0x02, 'h', 'i',  // decoded
      0x35, 0x04, 0x03, 0x02, 0x01,              // id (fixed32)
      0x48, 0x03,                                // hop_limit = 3
  };
  TEST_ASSERT_EQUAL_UINT32(sizeof(expected), length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, encoded, sizeof(expected));

  MeshPacketInfo decoded;
  TEST_ASSERT_TRUE(decodeMeshPacket(encoded, length, decoded));
  TEST_ASSERT_TRUE(decoded.hasDecoded);
  TEST_ASSERT_EQUAL_UINT32(0x11223344u, decoded.from);
  TEST_ASSERT_EQUAL_UINT32(kBroadcastAddress, decoded.to);
  TEST_ASSERT_EQUAL_UINT32(0x01020304u, decoded.id);
  TEST_ASSERT_EQUAL_UINT32(3, decoded.hopLimit);
  TEST_ASSERT_EQUAL_UINT32(kPortTextMessage, decoded.data.portnum);
  TEST_ASSERT_EQUAL_UINT32(2, decoded.data.payloadLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const uint8_t*>("hi"), decoded.data.payload, 2);
}

static void test_unknown_fields_are_skipped() {
  uint8_t encoded[] = {
      0x08, 0x01,              // portnum = 1
      0x12, 0x01, 'x',         // payload = "x"
      0xF8, 0x06, 0x2A,        // field 111, varint
      0x85, 0x07, 1, 2, 3, 4,  // field 112, fixed32
  };
  DataMessage decoded;
  TEST_ASSERT_TRUE(decodeData(encoded, sizeof(encoded), decoded));
  TEST_ASSERT_EQUAL_UINT32(kPortTextMessage, decoded.portnum);
  TEST_ASSERT_EQUAL_UINT32(1, decoded.payloadLength);
  TEST_ASSERT_EQUAL_UINT8('x', decoded.payload[0]);
}

static void test_encrypted_packet_has_no_decoded_payload() {
  uint8_t encoded[] = {
      0x0D, 1, 2, 3, 4,  // from
      0x2A, 0x03, 0xAA, 0xBB, 0xCC,  // encrypted (field 5, 3 bytes)
  };
  MeshPacketInfo decoded;
  TEST_ASSERT_TRUE(decodeMeshPacket(encoded, sizeof(encoded), decoded));
  TEST_ASSERT_FALSE(decoded.hasDecoded);
  TEST_ASSERT_EQUAL_UINT32(0x04030201u, decoded.from);
}

static void test_user_round_trip() {
  UserInfo user;
  std::strcpy(user.id, "!a1b2c3d4");
  std::strcpy(user.longName, "EA1ABC-9");
  std::strcpy(user.shortName, "ABC9");
  user.hwModel = 4;  // TBEAM
  user.role = 0;     // CLIENT
  user.licensed = true;

  uint8_t encoded[128] = {};
  const size_t length = encodeUser(user, encoded, sizeof(encoded));
  TEST_ASSERT_TRUE(length > 0);

  UserInfo decoded;
  TEST_ASSERT_TRUE(decodeUser(encoded, length, decoded));
  TEST_ASSERT_EQUAL_STRING("!a1b2c3d4", decoded.id);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9", decoded.longName);
  TEST_ASSERT_EQUAL_STRING("ABC9", decoded.shortName);
  TEST_ASSERT_EQUAL_UINT32(4, decoded.hwModel);
  TEST_ASSERT_TRUE(decoded.licensed);
}

static void test_truncated_and_oversized_inputs() {
  DataMessage data;
  const uint8_t truncatedVarint[] = {0x08, 0x80};
  TEST_ASSERT_FALSE(decodeData(truncatedVarint, sizeof(truncatedVarint), data));

  const uint8_t badLength[] = {0x12, 0x05, 'a', 'b'};
  TEST_ASSERT_FALSE(decodeData(badLength, sizeof(badLength), data));

  data.portnum = kPortTextMessage;
  data.payloadLength = kMaxTextBytes + 1;
  uint8_t out[512] = {};
  TEST_ASSERT_EQUAL_UINT32(0, encodeData(data, out, sizeof(out)));

  MeshPacketInfo packet;
  TEST_ASSERT_FALSE(decodeMeshPacket(truncatedVarint, sizeof(truncatedVarint), packet));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_aes_block_vectors);
  RUN_TEST(test_aes_ctr_vectors);
  RUN_TEST(test_ctr_round_trip_with_meshtastic_nonce);
  RUN_TEST(test_nonce_layout);
  RUN_TEST(test_default_channel_hash);
  RUN_TEST(test_resolve_channel_key);
  RUN_TEST(test_packet_header_layout);
  RUN_TEST(test_data_encoding_bytes);
  RUN_TEST(test_mesh_packet_encoding_bytes);
  RUN_TEST(test_unknown_fields_are_skipped);
  RUN_TEST(test_encrypted_packet_has_no_decoded_payload);
  RUN_TEST(test_user_round_trip);
  RUN_TEST(test_truncated_and_oversized_inputs);
  return UNITY_END();
}

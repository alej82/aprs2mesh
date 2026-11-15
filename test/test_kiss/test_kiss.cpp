#include <unity.h>

#include <cstring>

#include "kiss_codec.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

static void test_classify() {
  TEST_ASSERT_TRUE(kissClassify(0x00) == KissKind::Data);
  TEST_ASSERT_TRUE(kissClassify(0x10) == KissKind::Data);  // port 1, data
  TEST_ASSERT_TRUE(kissClassify(0x01) == KissKind::Command);
  TEST_ASSERT_TRUE(kissClassify(0x06) == KissKind::Command);
  TEST_ASSERT_TRUE(kissClassify(0x0F) == KissKind::Unknown);
  TEST_ASSERT_EQUAL_UINT8(1, kissPort(0x10));
  TEST_ASSERT_EQUAL_UINT8(0, kissType(0x10));
}

static void test_round_trip_with_escapes() {
  const uint8_t payload[] = {0x00, kFend, 0x42, kFesc, 0xFF};
  uint8_t encoded[64] = {};
  const size_t encodedLen = KissEncoder::encode(0x00, payload, sizeof(payload), encoded,
                                                sizeof(encoded));
  TEST_ASSERT_EQUAL_UINT32(KissEncoder::encodedSize(0x00, payload, sizeof(payload)),
                           encodedLen);

  KissDecoder decoder;
  bool complete = false;
  for (size_t i = 0; i < encodedLen; ++i) {
    complete = decoder.feed(encoded[i]);
    if (i + 1 < encodedLen) {
      TEST_ASSERT_FALSE(complete);
    }
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_UINT32(sizeof(payload) + 1, decoder.frameLen());
  TEST_ASSERT_EQUAL_UINT8(0x00, decoder.frame()[0]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoder.frame() + 1, sizeof(payload));
}

static void test_consecutive_and_empty_frames() {
  KissDecoder decoder;
  TEST_ASSERT_FALSE(decoder.feed(kFend));
  TEST_ASSERT_FALSE(decoder.feed(kFend));
  TEST_ASSERT_EQUAL_UINT32(0, decoder.frameLen());

  TEST_ASSERT_FALSE(decoder.feed(0x00));
  TEST_ASSERT_TRUE(decoder.feed(kFend));
  TEST_ASSERT_EQUAL_UINT32(1, decoder.frameLen());
}

static void test_two_frames_in_sequence() {
  const uint8_t a[] = {'A', 'B'};
  const uint8_t b[] = {'C'};
  uint8_t stream[64] = {};
  size_t n = KissEncoder::encode(0x00, a, sizeof(a), stream, sizeof(stream));
  n += KissEncoder::encode(0x12, b, sizeof(b), stream + n, sizeof(stream) - n);

  KissDecoder decoder;
  int frames = 0;
  for (size_t i = 0; i < n; ++i) {
    if (decoder.feed(stream[i])) {
      ++frames;
      if (frames == 1) {
        TEST_ASSERT_EQUAL_UINT32(3, decoder.frameLen());
        TEST_ASSERT_EQUAL_UINT8(0x00, decoder.frame()[0]);
        TEST_ASSERT_EQUAL_UINT8('B', decoder.frame()[2]);
      } else {
        TEST_ASSERT_EQUAL_UINT32(2, decoder.frameLen());
        TEST_ASSERT_EQUAL_UINT8(0x12, decoder.frame()[0]);
        TEST_ASSERT_EQUAL_UINT8('C', decoder.frame()[1]);
      }
    }
  }
  TEST_ASSERT_EQUAL_INT(2, frames);
}

static void test_invalid_escape_drops_frame_and_resyncs() {
  KissDecoder decoder;
  TEST_ASSERT_FALSE(decoder.feed(0x00));
  TEST_ASSERT_FALSE(decoder.feed(kFesc));
  TEST_ASSERT_FALSE(decoder.feed(0x11));  // not TFEND/TFESC
  TEST_ASSERT_FALSE(decoder.feed(kFend));
  TEST_ASSERT_TRUE(decoder.lastFrameDropped());

  const uint8_t payload[] = {'O', 'K'};
  uint8_t encoded[16] = {};
  const size_t n = KissEncoder::encode(0x00, payload, sizeof(payload), encoded, sizeof(encoded));
  bool complete = false;
  for (size_t i = 0; i < n; ++i) {
    complete = decoder.feed(encoded[i]);
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_UINT32(3, decoder.frameLen());
}

static void test_oversized_frame_dropped() {
  KissDecoder decoder;
  for (size_t i = 0; i < kKissMaxFrame + 32; ++i) {
    TEST_ASSERT_FALSE(decoder.feed(0x41));
  }
  TEST_ASSERT_FALSE(decoder.feed(kFend));
  TEST_ASSERT_TRUE(decoder.lastFrameDropped());
  TEST_ASSERT_EQUAL_UINT32(0, decoder.frameLen());
}

static void test_encoder_rejects_small_buffer() {
  const uint8_t payload[] = {1, 2, 3};
  uint8_t out[4] = {};
  TEST_ASSERT_EQUAL_UINT32(0, KissEncoder::encode(0x00, payload, sizeof(payload), out, sizeof(out)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_classify);
  RUN_TEST(test_round_trip_with_escapes);
  RUN_TEST(test_consecutive_and_empty_frames);
  RUN_TEST(test_two_frames_in_sequence);
  RUN_TEST(test_invalid_escape_drops_frame_and_resyncs);
  RUN_TEST(test_oversized_frame_dropped);
  RUN_TEST(test_encoder_rejects_small_buffer);
  return UNITY_END();
}

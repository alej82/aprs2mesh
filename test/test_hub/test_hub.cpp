#include <unity.h>

#include <cstring>

#include "kiss_codec.h"
#include "kiss_hub.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

class FakeSink : public KissSink {
 public:
  size_t write(const uint8_t* data, size_t length) override {
    if (failWrites) {
      return 0;
    }
    if (used + length > sizeof(buffer)) {
      return 0;
    }
    std::memcpy(buffer + used, data, length);
    used += length;
    ++writes;
    return length;
  }

  void reset() {
    used = 0;
    writes = 0;
  }

  uint8_t buffer[1024] = {};
  size_t used = 0;
  size_t writes = 0;
  bool failWrites = false;
};

static size_t encodeInto(uint8_t portAndType, const uint8_t* payload, size_t length,
                         uint8_t* out, size_t capacity) {
  return KissEncoder::encode(portAndType, payload, length, out, capacity);
}

static void test_clients_can_join_and_leave() {
  KissHub hub;
  FakeSink a;
  FakeSink b;
  TEST_ASSERT_TRUE(hub.addClient(&a));
  TEST_ASSERT_TRUE(hub.addClient(&b));
  TEST_ASSERT_FALSE(hub.addClient(&a));  // duplicate
  TEST_ASSERT_EQUAL_UINT32(2, hub.clientCount());

  hub.removeClient(&a);
  TEST_ASSERT_EQUAL_UINT32(1, hub.clientCount());
  hub.removeClient(&b);
  TEST_ASSERT_EQUAL_UINT32(0, hub.clientCount());
}

static void test_client_limit() {
  KissHub hub;
  FakeSink sinks[KissHub::kMaxClients + 1];
  for (size_t i = 0; i < KissHub::kMaxClients; ++i) {
    TEST_ASSERT_TRUE(hub.addClient(&sinks[i]));
  }
  TEST_ASSERT_FALSE(hub.addClient(&sinks[KissHub::kMaxClients]));
}

static void test_data_frame_is_reported_without_type_byte() {
  KissHub hub;
  FakeSink a;
  hub.addClient(&a);

  const uint8_t ax25[] = {0x82, 0xA0, 0x03, 0xF0, 'H', 'I'};
  uint8_t encoded[32] = {};
  const size_t n = encodeInto(0x00, ax25, sizeof(ax25), encoded, sizeof(encoded));

  bool complete = false;
  for (size_t i = 0; i < n; ++i) {
    complete = hub.feed(&a, encoded[i]);
  }
  TEST_ASSERT_TRUE(complete);
  TEST_ASSERT_EQUAL_UINT32(sizeof(ax25), hub.frameLen());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(ax25, hub.frame(), sizeof(ax25));
}

static void test_command_frames_are_swallowed() {
  KissHub hub;
  FakeSink a;
  hub.addClient(&a);

  const uint8_t txdelay[] = {30};
  uint8_t encoded[16] = {};
  const size_t n = encodeInto(0x01, txdelay, sizeof(txdelay), encoded, sizeof(encoded));
  bool complete = false;
  for (size_t i = 0; i < n; ++i) {
    complete = hub.feed(&a, encoded[i]);
  }
  TEST_ASSERT_FALSE(complete);
  TEST_ASSERT_EQUAL_UINT32(0, hub.frameLen());
}

static void test_unknown_client_bytes_are_ignored() {
  KissHub hub;
  FakeSink stranger;
  TEST_ASSERT_FALSE(hub.feed(&stranger, 0x00));
}

static void test_broadcast_reaches_every_client() {
  KissHub hub;
  FakeSink a;
  FakeSink b;
  hub.addClient(&a);
  hub.addClient(&b);

  const uint8_t ax25[] = {0x82, 0xA0, 0x03, 0xF0, 'H', 'I'};
  TEST_ASSERT_EQUAL_UINT32(2, hub.broadcastAx25(ax25, sizeof(ax25)));
  TEST_ASSERT_EQUAL_UINT32(a.used, b.used);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(a.buffer, b.buffer, a.used);
  TEST_ASSERT_EQUAL_UINT8(kFend, a.buffer[0]);
  TEST_ASSERT_EQUAL_UINT8(0x00, a.buffer[1]);
  TEST_ASSERT_EQUAL_UINT8(kFend, a.buffer[a.used - 1]);
}

static void test_interleaved_clients_do_not_corrupt_each_other() {
  KissHub hub;
  FakeSink a;
  FakeSink b;
  hub.addClient(&a);
  hub.addClient(&b);

  const uint8_t frameA[] = {'A', 'A', 'A', 'A'};
  const uint8_t frameB[] = {'B', 'B'};
  uint8_t encodedA[32] = {};
  uint8_t encodedB[32] = {};
  const size_t lenA = encodeInto(0x00, frameA, sizeof(frameA), encodedA, sizeof(encodedA));
  const size_t lenB = encodeInto(0x00, frameB, sizeof(frameB), encodedB, sizeof(encodedB));

  // Half of A, all of B, then the rest of A.
  bool completeA = false;
  for (size_t i = 0; i < lenA / 2; ++i) {
    completeA = hub.feed(&a, encodedA[i]);
  }
  TEST_ASSERT_FALSE(completeA);

  bool completeB = false;
  for (size_t i = 0; i < lenB; ++i) {
    completeB = hub.feed(&b, encodedB[i]);
  }
  TEST_ASSERT_TRUE(completeB);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(frameB, hub.frame(), sizeof(frameB));

  for (size_t i = lenA / 2; i < lenA; ++i) {
    completeA = hub.feed(&a, encodedA[i]);
  }
  TEST_ASSERT_TRUE(completeA);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(frameA, hub.frame(), sizeof(frameA));
}

static void test_failed_clients_are_dropped() {
  KissHub hub;
  FakeSink good;
  FakeSink broken;
  hub.addClient(&good);
  hub.addClient(&broken);

  broken.failWrites = true;
  const uint8_t ax25[] = {0x82, 0xA0, 0x03, 0xF0, 'H', 'I'};
  TEST_ASSERT_EQUAL_UINT32(1, hub.broadcastAx25(ax25, sizeof(ax25)));

  TEST_ASSERT_EQUAL_UINT32(1, hub.dropFailedClients());
  TEST_ASSERT_EQUAL_UINT32(1, hub.clientCount());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_clients_can_join_and_leave);
  RUN_TEST(test_client_limit);
  RUN_TEST(test_data_frame_is_reported_without_type_byte);
  RUN_TEST(test_command_frames_are_swallowed);
  RUN_TEST(test_unknown_client_bytes_are_ignored);
  RUN_TEST(test_broadcast_reaches_every_client);
  RUN_TEST(test_interleaved_clients_do_not_corrupt_each_other);
  RUN_TEST(test_failed_clients_are_dropped);
  return UNITY_END();
}

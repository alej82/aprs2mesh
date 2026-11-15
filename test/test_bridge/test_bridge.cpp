#include <unity.h>

#include <cstring>

#include "ax25.h"
#include "bridge.h"
#include "dedupe.h"
#include "mesh_text.h"
#include "tx_queue.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

char gPublished[kMaxTextBytes + 1] = {};
size_t gPublishedLength = 0;
uint32_t gPublishCalls = 0;

bool fakePublisher(void*, const char* text, size_t length) {
  if (length > kMaxTextBytes) {
    return false;
  }
  std::memcpy(gPublished, text, length);
  gPublished[length] = '\0';
  gPublishedLength = length;
  ++gPublishCalls;
  return true;
}

Ax25Frame sampleFrame() {
  Ax25Frame frame;
  parseAddress("APRS", 4, frame.destination);
  parseAddress("EA1ABC-9", 8, frame.source);
  parseAddress("WIDE1-1", 7, frame.digipeaters[0]);
  parseAddress("WIDE2-1", 7, frame.digipeaters[1]);
  frame.digipeaterCount = 2;
  const char info[] = "!4332.10N/00752.30W-Test";
  frame.infoLength = static_cast<uint16_t>(sizeof(info) - 1);
  std::memcpy(frame.info, info, frame.infoLength);
  return frame;
}

size_t encode(const Ax25Frame& frame, uint8_t* out, size_t capacity) {
  size_t written = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Ax25Status::Ok),
                          static_cast<uint8_t>(
                              buildAx25(frame, out, capacity, written)));
  return written;
}

}  // namespace

static void test_text_prefix() {
  const char text[] = "A#EA1ABC>APRS:x";
  TEST_ASSERT_TRUE(hasTextPrefix(text, sizeof(text) - 1));
  TEST_ASSERT_FALSE(hasTextPrefix("A", 1));
  TEST_ASSERT_FALSE(hasTextPrefix("a#EA1ABC>APRS:x", 15));
  TEST_ASSERT_FALSE(hasTextPrefix(nullptr, 0));

  size_t payloadLength = 0;
  const char* payload = stripMeshTextPrefix(text, sizeof(text) - 1, payloadLength);
  TEST_ASSERT_NOT_NULL(payload);
  TEST_ASSERT_EQUAL_UINT32(13, payloadLength);
  TEST_ASSERT_EQUAL_STRING("EA1ABC>APRS:x", payload);

  TEST_ASSERT_NULL(stripMeshTextPrefix("hola", 4, payloadLength));
}

static void test_build_mesh_text() {
  char out[kMaxTextBytes + 1] = {};
  const char payload[] = "EA1ABC>APRS:hola";
  const size_t length = buildMeshText(payload, sizeof(payload) - 1, out, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(sizeof(payload) - 1 + 2, length);
  TEST_ASSERT_EQUAL_STRING("A#EA1ABC>APRS:hola", out);

  // Exactly at the limit fits, one byte over does not.
  char big[kMaxTextBytes] = {};
  std::memset(big, 'x', sizeof(big) - 2);
  TEST_ASSERT_EQUAL_UINT32(kMaxTextBytes, buildMeshText(big, sizeof(big) - 2, out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, buildMeshText(big, sizeof(big) - 1, out, sizeof(out)));

  char tiny[8] = {};
  TEST_ASSERT_EQUAL_UINT32(0, buildMeshText(payload, sizeof(payload) - 1, tiny, sizeof(tiny)));
  TEST_ASSERT_EQUAL_UINT32(0, buildMeshText(payload, sizeof(payload) - 1, nullptr, sizeof(out)));
}

static void test_classify_text_packet() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PacketDisposition::Accept),
                          static_cast<uint8_t>(classifyTextPacket(
                              true, 0, 0, kPortTextMessage, 5)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PacketDisposition::NotDecoded),
                          static_cast<uint8_t>(classifyTextPacket(
                              false, 0, 0, kPortTextMessage, 5)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PacketDisposition::WrongChannel),
                          static_cast<uint8_t>(classifyTextPacket(
                              true, 1, 0, kPortTextMessage, 5)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PacketDisposition::NotText),
                          static_cast<uint8_t>(classifyTextPacket(
                              true, 0, 0, 3, 5)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PacketDisposition::EmptyText),
                          static_cast<uint8_t>(classifyTextPacket(
                              true, 0, 0, kPortTextMessage, 0)));
}

static void test_dedupe_window() {
  DedupeWindow window(1000);
  TEST_ASSERT_TRUE(window.accept(42, 100));
  TEST_ASSERT_FALSE(window.accept(42, 1099));
  TEST_ASSERT_TRUE(window.contains(42, 1099));
  TEST_ASSERT_FALSE(window.contains(42, 1100));
  TEST_ASSERT_TRUE(window.accept(42, 1100));

  // Unsigned subtraction must survive the millis wrap.
  TEST_ASSERT_TRUE(window.accept(7, 0xFFFFFF00u));
  TEST_ASSERT_TRUE(window.contains(7, 0x00000100u));

  // Filling the table evicts the oldest entry, never blocks new keys.
  for (size_t i = 0; i < DedupeWindow::kCapacity; ++i) {
    TEST_ASSERT_TRUE(window.accept(1000 + i, 2000));
  }
  TEST_ASSERT_TRUE(window.accept(9999, 2000));

  window.clear();
  TEST_ASSERT_FALSE(window.contains(9999, 2000));
}

static void test_tx_queue_rate_limit() {
  TxQueue queue(2, 0);
  TEST_ASSERT_TRUE(queue.push("uno", 3, 0));
  TEST_ASSERT_TRUE(queue.push("dos", 3, 0));
  TEST_ASSERT_FALSE(queue.push("tres", 4, 0));
  TEST_ASSERT_EQUAL_UINT32(1, queue.stats().droppedRate);

  char out[kMaxTextBytes + 1] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(queue.pop(0, out, sizeof(out), length));
  TEST_ASSERT_EQUAL_STRING("uno", out);
  TEST_ASSERT_TRUE(queue.pop(0, out, sizeof(out), length));

  // The two sends are still inside the 60 s window.
  TEST_ASSERT_FALSE(queue.push("cuatro", 6, 59999));
  TEST_ASSERT_TRUE(queue.push("cuatro", 6, 60000));
}

static void test_tx_queue_full() {
  TxQueue queue(60, 0);
  for (size_t i = 0; i < TxQueue::kCapacity; ++i) {
    TEST_ASSERT_TRUE(queue.push("x", 1, 0));
  }
  TEST_ASSERT_FALSE(queue.push("y", 1, 0));
  TEST_ASSERT_EQUAL_UINT32(1, queue.stats().droppedFull);
  TEST_ASSERT_EQUAL_UINT32(TxQueue::kCapacity, queue.size());

  // Reconfiguring keeps queued texts.
  queue.configure(1, 1000);
  TEST_ASSERT_EQUAL_UINT32(TxQueue::kCapacity, queue.size());
  TEST_ASSERT_EQUAL_UINT16(1, queue.maxPerMinute());
}

static void test_tx_queue_spacing() {
  TxQueue queue(60, 10000);
  TEST_ASSERT_TRUE(queue.push("uno", 3, 0));
  TEST_ASSERT_TRUE(queue.push("dos", 3, 0));

  char out[kMaxTextBytes + 1] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(queue.pop(0, out, sizeof(out), length));
  TEST_ASSERT_FALSE(queue.pop(5000, out, sizeof(out), length));
  TEST_ASSERT_TRUE(queue.pop(10000, out, sizeof(out), length));
  TEST_ASSERT_EQUAL_STRING("dos", out);
  TEST_ASSERT_TRUE(queue.empty());
}

static void test_tx_queue_rejects_oversized() {
  TxQueue queue(60, 0);
  char big[kMaxTextBytes + 1] = {};
  TEST_ASSERT_FALSE(queue.push(big, kMaxTextBytes + 1, 0));
  TEST_ASSERT_FALSE(queue.push(nullptr, 1, 0));
  TEST_ASSERT_FALSE(queue.push(big, 0, 0));
}

static void test_bridge_uplink_keeps_path() {
  Bridge bridge(0x1234, fakePublisher, nullptr);
  BridgeSettings settings;
  settings.maxPerMinute = 60;
  settings.minIntervalMs = 0;
  bridge.configure(settings, 0);

  const Ax25Frame frame = sampleFrame();
  uint8_t ax25[Bridge::kMaxAx25] = {};
  const size_t ax25Length = encode(frame, ax25, sizeof(ax25));

  TEST_ASSERT_TRUE(bridge.uplink(ax25, ax25Length, 0));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().uplinkAccepted);
  TEST_ASSERT_EQUAL_UINT32(1, bridge.txQueue().size());

  gPublishedLength = 0;
  TEST_ASSERT_EQUAL_UINT32(1, bridge.drain(0));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().published);
  TEST_ASSERT_EQUAL_STRING(
      "A#EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test", gPublished);

  // Same source and information inside 30 s is a client retry.
  TEST_ASSERT_FALSE(bridge.uplink(ax25, ax25Length, 1000));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().uplinkDuplicate);

  // After the window it is a new frame again.
  TEST_ASSERT_TRUE(bridge.uplink(ax25, ax25Length, 30001));
}

static void test_bridge_uplink_strips_path() {
  Bridge bridge(0x1234, fakePublisher, nullptr);
  BridgeSettings settings;
  settings.stripPath = true;
  settings.maxPerMinute = 60;
  settings.minIntervalMs = 0;
  bridge.configure(settings, 0);

  uint8_t ax25[Bridge::kMaxAx25] = {};
  const size_t ax25Length = encode(sampleFrame(), ax25, sizeof(ax25));
  TEST_ASSERT_TRUE(bridge.uplink(ax25, ax25Length, 0));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.drain(0));
  TEST_ASSERT_EQUAL_STRING("A#EA1ABC-9>APRS:!4332.10N/00752.30W-Test", gPublished);
}

static void test_bridge_uplink_rejects_invalid_and_oversized() {
  Bridge bridge(0x1234, fakePublisher, nullptr);
  BridgeSettings settings;
  settings.maxPerMinute = 60;
  settings.minIntervalMs = 0;
  bridge.configure(settings, 0);

  const uint8_t garbage[] = {1, 2, 3};
  TEST_ASSERT_FALSE(bridge.uplink(garbage, sizeof(garbage), 0));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().uplinkInvalid);

  Ax25Frame frame = sampleFrame();
  frame.digipeaterCount = 0;
  frame.infoLength = static_cast<uint16_t>(kMaxInfo);
  std::memset(frame.info, 'x', frame.infoLength);
  uint8_t ax25[Bridge::kMaxAx25] = {};
  const size_t ax25Length = encode(frame, ax25, sizeof(ax25));

  TEST_ASSERT_FALSE(bridge.uplink(ax25, ax25Length, 0));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().uplinkTooLarge);
  TEST_ASSERT_TRUE(bridge.txQueue().empty());
}

static void test_bridge_downlink_builds_ax25() {
  Bridge bridge(0x1234, fakePublisher, nullptr);
  const char text[] = "A#EA2XYZ>APRS::EA1ABC-9 :hola{13";
  uint8_t ax25[Bridge::kMaxAx25] = {};
  size_t ax25Length = 0;

  TEST_ASSERT_TRUE(bridge.downlink(text, sizeof(text) - 1, 0x5678, 0, ax25,
                                   sizeof(ax25), ax25Length));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().downlinkDelivered);

  Ax25Frame frame;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Ax25Status::Ok),
                          static_cast<uint8_t>(parseAx25(ax25, ax25Length, frame)));
  TEST_ASSERT_EQUAL_STRING("EA2XYZ", frame.source.call);
  TEST_ASSERT_EQUAL_STRING("APRS", frame.destination.call);
  TEST_ASSERT_EQUAL_UINT32(18, frame.infoLength);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(reinterpret_cast<const uint8_t*>(":EA1ABC-9 :hola{13"),
                                frame.info, 18);
}

static void test_bridge_downlink_filters() {
  Bridge bridge(0x1234, fakePublisher, nullptr);
  uint8_t ax25[Bridge::kMaxAx25] = {};
  size_t ax25Length = 0;

  // Our own text coming back through a relay.
  const char own[] = "A#EA1ABC-9>APRS:hola";
  TEST_ASSERT_FALSE(bridge.downlink(own, sizeof(own) - 1, 0x1234, 0, ax25,
                                    sizeof(ax25), ax25Length));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().ownEcho);

  // Plain chat without the prefix.
  TEST_ASSERT_FALSE(bridge.downlink("hola", 4, 7, 0, ax25, sizeof(ax25), ax25Length));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().downlinkIgnored);

  // Prefix but no TNC2 shape.
  const char broken[] = "A#esto no es una trama";
  TEST_ASSERT_FALSE(bridge.downlink(broken, sizeof(broken) - 1, 7, 0, ax25,
                                    sizeof(ax25), ax25Length));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().downlinkInvalid);

  // Larger than the message limit.
  char huge[kMaxTextBytes + 2] = {};
  std::memset(huge, 'x', sizeof(huge) - 1);
  TEST_ASSERT_FALSE(bridge.downlink(huge, sizeof(huge) - 1, 7, 0, ax25,
                                    sizeof(ax25), ax25Length));
  TEST_ASSERT_EQUAL_UINT32(1, bridge.counters().downlinkTooLarge);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_text_prefix);
  RUN_TEST(test_build_mesh_text);
  RUN_TEST(test_classify_text_packet);
  RUN_TEST(test_dedupe_window);
  RUN_TEST(test_tx_queue_rate_limit);
  RUN_TEST(test_tx_queue_full);
  RUN_TEST(test_tx_queue_spacing);
  RUN_TEST(test_tx_queue_rejects_oversized);
  RUN_TEST(test_bridge_uplink_keeps_path);
  RUN_TEST(test_bridge_uplink_strips_path);
  RUN_TEST(test_bridge_uplink_rejects_invalid_and_oversized);
  RUN_TEST(test_bridge_downlink_builds_ax25);
  RUN_TEST(test_bridge_downlink_filters);
  return UNITY_END();
}

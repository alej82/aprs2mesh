#include <unity.h>

#include <cstring>

#include "aprs_line.h"
#include "flow_table.h"
#include "rate_meter.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

AprsLine parseOk(const char* text) {
  AprsLine line;
  const size_t length = std::strlen(text);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AprsLineStatus::Ok),
                          static_cast<uint8_t>(parseAprsLine(text, length, line)));
  return line;
}

}  // namespace

static void test_parse_rf_line() {
  const AprsLine line = parseOk(
      "EA1ABC-9>APRS,WIDE1-1,WIDE2-1:!4332.10N/00752.30W-Test");
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9", line.source);
  TEST_ASSERT_EQUAL_STRING("APRS", line.destination);
  TEST_ASSERT_EQUAL_STRING("WIDE1-1,WIDE2-1", line.path);
  TEST_ASSERT_EQUAL_STRING("!4332.10N/00752.30W-Test", line.info);
  TEST_ASSERT_TRUE(line.hasPath);
}

static void test_parse_is_line_with_long_path_items() {
  const AprsLine line = parseOk(
      "EA1ABC-9>APRS,TCPIP*,qAC,T2SPAIN::EA1ABC-9 :pong{AB1");
  TEST_ASSERT_EQUAL_STRING("TCPIP*,qAC,T2SPAIN", line.path);
  TEST_ASSERT_EQUAL_STRING(":EA1ABC-9 :pong{AB1", line.info);
}

static void test_parse_rejects_malformed_lines() {
  AprsLine line;
  const char* cases[] = {
      "",
      "EA1ABC-9APRS:hello",
      ">APRS:hello",
      "EA1ABC-9>APRS",
      "EA1ABC-9>APRS:",
      "EA1ABC-16>APRS:hello",
      "EA1ABC-9>APRS,BAD!ITEM:hello",
      "EA1ABC-9>APRS,,WIDE1-1:hello",
  };
  for (const char* text : cases) {
    TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(AprsLineStatus::Ok),
                          static_cast<uint8_t>(
                              parseAprsLine(text, std::strlen(text), line)));
  }
}

static void test_injected_line_preserves_path_and_adds_qar() {
  const AprsLine line = parseOk("EA1ABC-9>APRS,WIDE1-1,WIDE2-1:>hello");
  char out[400] = {};
  const size_t length = buildInjectedLine(line, "EA1ABC-10", out, sizeof(out));
  TEST_ASSERT_TRUE(length > 0);
  TEST_ASSERT_EQUAL_STRING(
      "EA1ABC-9>APRS,WIDE1-1,WIDE2-1,qAR,EA1ABC-10:>hello", out);

  const AprsLine bare = parseOk("EA1ABC-9>APRS:>hello");
  const size_t bareLength = buildInjectedLine(bare, "EA1ABC-10", out, sizeof(out));
  TEST_ASSERT_TRUE(bareLength > 0);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,qAR,EA1ABC-10:>hello", out);
}

static void test_downlink_line_uses_tcpip_only() {
  const AprsLine line = parseOk(
      "EA1ABC-9>APRS,TCPIP*,qAC,T2SPAIN::EA1ABC-9 :pong{AB1");
  char out[400] = {};
  const size_t length = buildDownlinkLine(line, out, sizeof(out));
  TEST_ASSERT_TRUE(length > 0);
  TEST_ASSERT_EQUAL_STRING(
      "EA1ABC-9>APRS,TCPIP*::EA1ABC-9 :pong{AB1", out);
}

static void test_parse_message() {
  AprsLine line = parseOk("EA1ABC-9>APRS::EA1ABC-9 :hello{12");
  AprsMessage message;
  TEST_ASSERT_TRUE(parseAprsMessage(line, message));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9", message.addressee);
  TEST_ASSERT_EQUAL_STRING("hello", message.text);
  TEST_ASSERT_TRUE(message.hasMessageId);
  TEST_ASSERT_EQUAL_STRING("12", message.messageId);

  line = parseOk("EA1ABC-9>APRS::EA1ABC-9 :reply{AB1}12");
  TEST_ASSERT_TRUE(parseAprsMessage(line, message));
  TEST_ASSERT_EQUAL_STRING("reply", message.text);
  TEST_ASSERT_EQUAL_STRING("AB1", message.messageId);

  line = parseOk("EA1ABC-9>APRS::EA1ABC-9 :no id here");
  TEST_ASSERT_TRUE(parseAprsMessage(line, message));
  TEST_ASSERT_FALSE(message.hasMessageId);
  TEST_ASSERT_EQUAL_STRING("no id here", message.text);
}

static void test_message_parser_rejects_non_messages() {
  AprsMessage message;
  AprsLine line = parseOk("EA1ABC-9>APRS:!4332.10N/00752.30W-Test");
  TEST_ASSERT_FALSE(parseAprsMessage(line, message));

  line = parseOk("EA1ABC-9>APRS:}EA1XYZ>APRS:hello");
  TEST_ASSERT_FALSE(parseAprsMessage(line, message));

  line = parseOk("EA1ABC-9>APRS::EA1ABC-9");
  TEST_ASSERT_FALSE(parseAprsMessage(line, message));
}

static void test_flows_open_refresh_expire() {
  FlowTable flows;
  flows.configure(60, 8);
  flows.open("EA1ABC-9", 1000);
  TEST_ASSERT_EQUAL_UINT32(1, flows.size());
  TEST_ASSERT_TRUE(flows.active("ea1abc-9", 2000));

  flows.open("EA1ABC-9", 5000);
  TEST_ASSERT_EQUAL_UINT32(1, flows.size());
  TEST_ASSERT_TRUE(flows.active("EA1ABC-9", 5000 + 59u * 60000u));
  TEST_ASSERT_FALSE(flows.active("EA1ABC-9", 5000 + 61u * 60000u));
}

static void test_flows_evict_oldest_when_full() {
  FlowTable flows;
  flows.configure(60, 2);
  flows.open("EA1AAA-1", 1000);
  flows.open("EA1BBB-2", 2000);
  flows.open("EA1CCC-3", 3000);
  TEST_ASSERT_EQUAL_UINT32(2, flows.size());
  TEST_ASSERT_FALSE(flows.active("EA1AAA-1", 3000));
  TEST_ASSERT_TRUE(flows.active("EA1BBB-2", 3000));
  TEST_ASSERT_TRUE(flows.active("EA1CCC-3", 3000));
  TEST_ASSERT_EQUAL_UINT32(1, flows.evictedCount());
}

static void test_flows_prune() {
  FlowTable flows;
  flows.configure(1, 8);
  flows.open("EA1ABC-9", 0);
  flows.prune(59999);
  TEST_ASSERT_EQUAL_UINT32(1, flows.size());
  flows.prune(60000);
  TEST_ASSERT_EQUAL_UINT32(0, flows.size());
}

static void test_rate_meter_window() {
  RateMeter meter(2);
  TEST_ASSERT_TRUE(meter.allow(0));
  TEST_ASSERT_TRUE(meter.allow(1000));
  TEST_ASSERT_FALSE(meter.allow(2000));
  TEST_ASSERT_EQUAL_UINT32(2, meter.used(2000));
  TEST_ASSERT_TRUE(meter.allow(60001));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_rf_line);
  RUN_TEST(test_parse_is_line_with_long_path_items);
  RUN_TEST(test_parse_rejects_malformed_lines);
  RUN_TEST(test_injected_line_preserves_path_and_adds_qar);
  RUN_TEST(test_downlink_line_uses_tcpip_only);
  RUN_TEST(test_parse_message);
  RUN_TEST(test_message_parser_rejects_non_messages);
  RUN_TEST(test_flows_open_refresh_expire);
  RUN_TEST(test_flows_evict_oldest_when_full);
  RUN_TEST(test_flows_prune);
  RUN_TEST(test_rate_meter_window);
  return UNITY_END();
}

#include <unity.h>

#include <cstdio>
#include <cstring>

#include "aprsis_session.h"
#include "rate_meter.h"
#include "uplink_buffer.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

AprsIsSettings sampleSettings() {
  AprsIsSettings settings;
  std::strncpy(settings.callsign, "EA1ABC-10", sizeof(settings.callsign) - 1);
  settings.passcode = 12345;
  std::strncpy(settings.filter, "m/50", sizeof(settings.filter) - 1);
  return settings;
}

void feedLine(AprsIsSession& session, const char* line, uint32_t nowMs) {
  session.onLine(line, std::strlen(line), nowMs);
}

}  // namespace

static void test_login_line_includes_filter() {
  AprsIsSession session;
  session.configure(sampleSettings(), 0);
  session.start(0);
  session.onConnected(0);
  TEST_ASSERT_TRUE(session.loginPending());

  char line[256] = {};
  const size_t length = session.buildLogin(line, sizeof(line));
  TEST_ASSERT_TRUE(length > 0);
  TEST_ASSERT_EQUAL_STRING("user EA1ABC-10 pass 12345 vers APZ2M filter m/50\r\n",
                           line);
}

static void test_login_line_omits_empty_filter() {
  AprsIsSession session;
  AprsIsSettings settings = sampleSettings();
  settings.filter[0] = '\0';
  session.configure(settings, 0);
  session.start(0);
  session.onConnected(0);

  char line[256] = {};
  TEST_ASSERT_TRUE(session.buildLogin(line, sizeof(line)) > 0);
  TEST_ASSERT_EQUAL_STRING("user EA1ABC-10 pass 12345 vers APZ2M\r\n", line);
}

static void test_verified_and_unverified_logins() {
  AprsIsSession session;
  session.configure(sampleSettings(), 0);
  session.start(0);
  session.onConnected(100);
  session.onLoginSent(200);
  TEST_ASSERT_FALSE(session.canInject());

  feedLine(session, "# aprs2mesh keepalive", 250);
  TEST_ASSERT_FALSE(session.canInject());

  feedLine(session,
           "# aprsc 2.1.4 logresp EA1ABC-10 unverified, server T2SPAIN", 300);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SessionState::Unverified),
                          static_cast<uint8_t>(session.state()));
  TEST_ASSERT_FALSE(session.canInject());
  TEST_ASSERT_EQUAL_STRING("login no verificado", session.lastError());

  feedLine(session, "# aprsc 2.1.4 logresp EA1ABC-10 verified, server T2SPAIN",
           400);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SessionState::Verified),
                          static_cast<uint8_t>(session.state()));
  TEST_ASSERT_TRUE(session.canInject());
  TEST_ASSERT_EQUAL_STRING("", session.lastError());
}

static void test_keepalive_schedule() {
  AprsIsSession session;
  session.configure(sampleSettings(), 0);
  session.start(0);
  session.onConnected(0);
  session.onLoginSent(0);
  feedLine(session, "# aprsc logresp EA1ABC-10 verified, server T2SPAIN", 100);

  TEST_ASSERT_FALSE(session.keepaliveDue(19999));
  TEST_ASSERT_TRUE(session.keepaliveDue(20000));

  char line[64] = {};
  const size_t length = session.buildKeepalive(20000, line, sizeof(line));
  TEST_ASSERT_TRUE(length > 0);
  TEST_ASSERT_EQUAL_STRING("# aprs2mesh\r\n", line);
  TEST_ASSERT_FALSE(session.keepaliveDue(20001));
}

static void test_reconnect_backoff_grows_and_caps() {
  AprsIsSession session;
  session.configure(sampleSettings(), 0);
  session.start(0);

  session.onConnected(10);
  session.onLoginSent(11);
  session.onTransportDown(100, "red caída");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SessionState::Disconnected),
                          static_cast<uint8_t>(session.state()));
  TEST_ASSERT_EQUAL_UINT32(5000, session.retryDelayMs());
  TEST_ASSERT_EQUAL_UINT32(5100, session.nextAttemptAt());
  TEST_ASSERT_FALSE(session.connectDue(5099));
  TEST_ASSERT_TRUE(session.connectDue(5100));

  session.onConnected(5100);
  session.onLoginSent(5101);
  session.onTransportDown(5200, "red caída");
  TEST_ASSERT_EQUAL_UINT32(10000, session.retryDelayMs());
  TEST_ASSERT_EQUAL_UINT32(15200, session.nextAttemptAt());

  TEST_ASSERT_EQUAL_UINT32(300000, AprsIsSession::retryDelayFor(10));
  TEST_ASSERT_EQUAL_UINT32(5000, AprsIsSession::retryDelayFor(1));
}

static void test_login_timeout_requests_close() {
  AprsIsSession session;
  session.configure(sampleSettings(), 0);
  session.start(0);
  session.onConnected(0);
  session.onLoginSent(0);

  session.tick(15000);
  TEST_ASSERT_FALSE(session.closeRequested());
  session.tick(15001);
  TEST_ASSERT_TRUE(session.closeRequested());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SessionState::Disconnected),
                          static_cast<uint8_t>(session.state()));
}

static void test_uplink_buffer_ttl() {
  UplinkBuffer buffer;
  TEST_ASSERT_TRUE(buffer.push("EA1ABC-9>APRS:>hi", 17, "EA1ABC-9", 42, 0));
  TEST_ASSERT_EQUAL_UINT32(1, buffer.size());

  UplinkEntry entry;
  TEST_ASSERT_TRUE(buffer.pop(30000, entry));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS:>hi", entry.tnc2);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9", entry.source);
  TEST_ASSERT_EQUAL_UINT32(42, entry.fromNode);

  TEST_ASSERT_TRUE(buffer.push("EA1ABC-9>APRS:>hi", 17, "EA1ABC-9", 42, 1000));
  TEST_ASSERT_FALSE(buffer.pop(31001, entry));
  TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedExpired());
  TEST_ASSERT_EQUAL_UINT32(0, buffer.size());
}

static void test_uplink_buffer_drops_oldest_when_full() {
  UplinkBuffer buffer;
  char text[32] = {};
  for (size_t i = 0; i < kUplinkCapacity; ++i) {
    std::snprintf(text, sizeof(text), "EA1ABC-9>APRS:>%u", static_cast<unsigned>(i));
    TEST_ASSERT_TRUE(buffer.push(text, std::strlen(text), "EA1ABC-9", 1, 0));
  }
  std::snprintf(text, sizeof(text), "EA1ABC-9>APRS:>%u",
                static_cast<unsigned>(kUplinkCapacity));
  TEST_ASSERT_TRUE(buffer.push(text, std::strlen(text), "EA1ABC-9", 1, 0));
  TEST_ASSERT_EQUAL_UINT32(1, buffer.droppedFull());
  TEST_ASSERT_EQUAL_UINT32(kUplinkCapacity, buffer.size());

  UplinkEntry entry;
  TEST_ASSERT_TRUE(buffer.pop(0, entry));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS:>1", entry.tnc2);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_login_line_includes_filter);
  RUN_TEST(test_login_line_omits_empty_filter);
  RUN_TEST(test_verified_and_unverified_logins);
  RUN_TEST(test_keepalive_schedule);
  RUN_TEST(test_reconnect_backoff_grows_and_caps);
  RUN_TEST(test_login_timeout_requests_close);
  RUN_TEST(test_uplink_buffer_ttl);
  RUN_TEST(test_uplink_buffer_drops_oldest_when_full);
  return UNITY_END();
}

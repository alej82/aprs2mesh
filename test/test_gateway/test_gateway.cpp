#include <unity.h>

#include <cstring>

#include "aprs_line.h"
#include "aprsis_session.h"
#include "gateway.h"
#include "identity.h"
#include "mesh_proto.h"
#include "mesh_text.h"

using namespace aprs2mesh;

void setUp() {}
void tearDown() {}

namespace {

struct MonitorCapture {
  char last[400] = {};
  size_t length = 0;
  uint32_t count = 0;
};

void capture(void* context, const char* tnc2, size_t length) {
  MonitorCapture* out = static_cast<MonitorCapture*>(context);
  std::memcpy(out->last, tnc2, length);
  out->last[length] = '\0';
  out->length = length;
  ++out->count;
}

GatewaySettings sampleSettings() {
  GatewaySettings settings;
  std::strncpy(settings.callsign, "EA1ABC-10", sizeof(settings.callsign) - 1);
  settings.gateOpen = true;
  settings.flowTtlMinutes = 60;
  settings.maxFlows = 8;
  settings.uplinkPerMinute = 10;
  settings.downlinkPerMinute = 10;
  settings.minIntervalMs = 0;
  return settings;
}

constexpr uint32_t kNode = 0x12345678u;
const char* kUplink = "A#EA1ABC-9>APRS,WIDE1-1:>hello";
const char* kResponse =
    "EA1ABC-9>APRS,TCPIP*,qAC,T2SPAIN::EA1ABC-9 :pong{AB1";

}  // namespace

static void test_uplink_builds_qar_and_opens_flow() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 7, kUplink, std::strlen(kUplink), 1000));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.bufferedUplinks());
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkAccepted);

  char line[400] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(gateway.nextInjection(1000, line, sizeof(line), length));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1,qAR,EA1ABC-10:>hello", line);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9", gateway.pendingSource());
  TEST_ASSERT_EQUAL_UINT32(42, gateway.pendingFromNode());

  gateway.onInjectionResult(true, 1000);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().injected);
  TEST_ASSERT_EQUAL_UINT32(1, monitor.count);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1:>hello", monitor.last);
  TEST_ASSERT_TRUE(gateway.flows().active("EA1ABC-9", 1000));
}

static void test_downlink_admitted_and_rewritten() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  gateway.onMeshPacket(42, 7, kUplink, std::strlen(kUplink), 1000);
  char line[400] = {};
  size_t length = 0;
  gateway.nextInjection(1000, line, sizeof(line), length);
  gateway.onInjectionResult(true, 1000);

  gateway.onServerPacket(kResponse, std::strlen(kResponse), 2000);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().serverPackets);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkAdmitted);

  char text[kMaxTextBytes + 1] = {};
  TEST_ASSERT_TRUE(gateway.popDownlink(2000, text, sizeof(text), length));
  TEST_ASSERT_EQUAL_STRING(
      "A#EA1ABC-9>APRS,TCPIP*::EA1ABC-9 :pong{AB1", text);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkPublished);
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,TCPIP*::EA1ABC-9 :pong{AB1",
                           monitor.last);
  TEST_ASSERT_EQUAL_UINT32(2, monitor.count);
}

static void test_downlink_rejects_beacons_and_unknown_addressees() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  const char* beacon = "EA1XYZ-1>APRS,TCPIP*:!4332.10N/00752.30W-Test";
  gateway.onServerPacket(beacon, std::strlen(beacon), 1000);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkRejected);

  const char* unknown =
      "EA1XYZ-1>APRS,TCPIP*::EA1ABC-9 :hello{22";
  gateway.onServerPacket(unknown, std::strlen(unknown), 1100);
  TEST_ASSERT_EQUAL_UINT32(2, gateway.counters().downlinkRejected);
  TEST_ASSERT_EQUAL_UINT32(0, gateway.counters().downlinkAdmitted);
  TEST_ASSERT_EQUAL_STRING("sin flujo vigente", gateway.lastReject());
}

static void test_downlink_deduplicates_and_expires_flows() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  GatewaySettings settings = sampleSettings();
  settings.flowTtlMinutes = 1;
  gateway.configure(settings, 0);

  gateway.onMeshPacket(42, 7, kUplink, std::strlen(kUplink), 0);
  char line[400] = {};
  size_t length = 0;
  gateway.nextInjection(0, line, sizeof(line), length);
  gateway.onInjectionResult(true, 0);

  gateway.onServerPacket(kResponse, std::strlen(kResponse), 1000);
  gateway.onServerPacket(kResponse, std::strlen(kResponse), 1001);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkAdmitted);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkDuplicate);

  gateway.onServerPacket(kResponse, std::strlen(kResponse), 61000);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkAdmitted);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkRejected);
}

static void test_own_echo_prefix_and_gate() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  TEST_ASSERT_FALSE(gateway.onMeshPacket(kNode, 1, kUplink, std::strlen(kUplink), 0));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkOwnEcho);

  const char* plain = "hello channel";
  TEST_ASSERT_FALSE(gateway.onMeshPacket(42, 2, plain, std::strlen(plain), 0));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkIgnored);

  const char* invalid = "A#not a tnc2 line";
  TEST_ASSERT_FALSE(gateway.onMeshPacket(42, 3, invalid, std::strlen(invalid), 0));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkInvalid);

  gateway.setGateOpen(false);
  TEST_ASSERT_FALSE(gateway.onMeshPacket(42, 4, kUplink, std::strlen(kUplink), 0));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkGateClosed);
  TEST_ASSERT_EQUAL_STRING("gate cerrado", gateway.lastReject());
}

static void test_uplink_dedupe_and_rate_limit() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  GatewaySettings settings = sampleSettings();
  settings.uplinkPerMinute = 1;
  gateway.configure(settings, 0);

  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 1, kUplink, std::strlen(kUplink), 0));
  TEST_ASSERT_FALSE(gateway.onMeshPacket(42, 2, kUplink, std::strlen(kUplink), 100));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkDuplicate);

  const char* other = "A#EA1AAA-1>APRS:>hola";
  TEST_ASSERT_FALSE(gateway.onMeshPacket(42, 3, other, std::strlen(other), 200));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkRateLimited);
}

static void test_buffer_expires_without_session() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 1, kUplink, std::strlen(kUplink), 0));
  char line[400] = {};
  size_t length = 0;
  TEST_ASSERT_FALSE(gateway.nextInjection(31001, line, sizeof(line), length));
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().uplinkBufferExpired);
  TEST_ASSERT_EQUAL_UINT32(0, gateway.bufferedUplinks());
}

static void test_failed_injection_returns_to_buffer() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 1, kUplink, std::strlen(kUplink), 0));
  char line[400] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(gateway.nextInjection(0, line, sizeof(line), length));
  gateway.onInjectionResult(false, 10);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().injectFailed);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.bufferedUplinks());
  TEST_ASSERT_EQUAL_UINT32(0, gateway.counters().injected);
}

static void test_bulletins_are_rejected_even_with_flow() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  const char* bulletinUplink = "A#BLN1>APRS:>hello";
  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 11, bulletinUplink,
                                        std::strlen(bulletinUplink), 0));
  char line[400] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(gateway.nextInjection(0, line, sizeof(line), length));
  gateway.onInjectionResult(true, 0);
  TEST_ASSERT_TRUE(gateway.flows().active("BLN1", 0));

  const char* bulletin = "EA1XYZ-1>APRS,TCPIP*::BLN1     :boletin";
  gateway.onServerPacket(bulletin, std::strlen(bulletin), 1000);
  TEST_ASSERT_EQUAL_UINT32(1, gateway.counters().downlinkRejected);
  TEST_ASSERT_EQUAL_UINT32(0, gateway.counters().downlinkAdmitted);
  TEST_ASSERT_EQUAL_STRING("boletín", gateway.lastReject());
}

static void test_full_cycle_with_session_and_fake_server() {
  MonitorCapture monitor;
  Gateway gateway(kNode, capture, &monitor);
  gateway.configure(sampleSettings(), 0);

  // Fake APRS-IS: login, verification and one reply, all in memory.
  AprsIsSession session;
  AprsIsSettings sessionSettings;
  std::strncpy(sessionSettings.callsign, "EA1ABC-10",
               sizeof(sessionSettings.callsign) - 1);
  sessionSettings.passcode = 12345;
  std::strncpy(sessionSettings.filter, "m/50", sizeof(sessionSettings.filter) - 1);
  session.configure(sessionSettings, 0);
  session.start(0);
  session.onConnected(0);

  char login[256] = {};
  TEST_ASSERT_TRUE(session.buildLogin(login, sizeof(login)) > 0);
  TEST_ASSERT_NOT_NULL(std::strstr(login, "user EA1ABC-10 pass 12345 vers APZ2M"));
  TEST_ASSERT_NOT_NULL(std::strstr(login, "filter m/50"));
  session.onLoginSent(10);

  const char* logresp = "# aprsc 2.1.4 logresp EA1ABC-10 verified, server T2SPAIN";
  session.onLine(logresp, std::strlen(logresp), 20);
  TEST_ASSERT_TRUE(session.canInject());

  TEST_ASSERT_TRUE(gateway.onMeshPacket(42, 7, kUplink, std::strlen(kUplink), 30));
  char line[400] = {};
  size_t length = 0;
  TEST_ASSERT_TRUE(gateway.nextInjection(30, line, sizeof(line), length));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-9>APRS,WIDE1-1,qAR,EA1ABC-10:>hello", line);
  gateway.onInjectionResult(true, 30);

  session.onLine(kResponse, std::strlen(kResponse), 40);
  gateway.onServerPacket(kResponse, std::strlen(kResponse), 40);

  char text[kMaxTextBytes + 1] = {};
  TEST_ASSERT_TRUE(gateway.popDownlink(40, text, sizeof(text), length));
  TEST_ASSERT_EQUAL_STRING("A#EA1ABC-9>APRS,TCPIP*::EA1ABC-9 :pong{AB1", text);
}

static void test_identity_and_node_info_packet() {
  GatewayIdentity identity;
  TEST_ASSERT_TRUE(buildGatewayIdentity("EA1ABC-10", kNode, identity));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-10", identity.longName);
  TEST_ASSERT_EQUAL_STRING("EA1A", identity.shortName);
  TEST_ASSERT_EQUAL_UINT8(1, identity.role);
  TEST_ASSERT_EQUAL_UINT8(3, identity.hopLimit);

  NodeInfoScheduler scheduler;
  scheduler.configure(identity, 0);
  TEST_ASSERT_TRUE(scheduler.due(0));

  uint8_t packet[300] = {};
  const size_t length = scheduler.buildPacket(0x11223344u, packet, sizeof(packet));
  TEST_ASSERT_TRUE(length > 0);

  MeshPacketInfo info;
  TEST_ASSERT_TRUE(decodeMeshPacket(packet, length, info));
  TEST_ASSERT_EQUAL_UINT32(kNode, info.from);
  TEST_ASSERT_EQUAL_UINT32(kBroadcastAddress, info.to);
  TEST_ASSERT_EQUAL_UINT32(kPortNodeInfo, info.data.portnum);
  TEST_ASSERT_TRUE(info.data.payloadLength > 0);

  UserInfo user;
  TEST_ASSERT_TRUE(decodeUser(info.data.payload, info.data.payloadLength, user));
  TEST_ASSERT_EQUAL_STRING("EA1ABC-10", user.longName);
  TEST_ASSERT_EQUAL_STRING("EA1A", user.shortName);
  TEST_ASSERT_EQUAL_UINT32(1, user.role);
  TEST_ASSERT_EQUAL_STRING("!12345678", user.id);

  scheduler.markSent(0);
  TEST_ASSERT_FALSE(scheduler.due(1000));
  TEST_ASSERT_TRUE(scheduler.due(kNodeInfoIntervalMs));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_uplink_builds_qar_and_opens_flow);
  RUN_TEST(test_downlink_admitted_and_rewritten);
  RUN_TEST(test_downlink_rejects_beacons_and_unknown_addressees);
  RUN_TEST(test_downlink_deduplicates_and_expires_flows);
  RUN_TEST(test_own_echo_prefix_and_gate);
  RUN_TEST(test_uplink_dedupe_and_rate_limit);
  RUN_TEST(test_buffer_expires_without_session);
  RUN_TEST(test_failed_injection_returns_to_buffer);
  RUN_TEST(test_bulletins_are_rejected_even_with_flow);
  RUN_TEST(test_full_cycle_with_session_and_fake_server);
  RUN_TEST(test_identity_and_node_info_packet);
  return UNITY_END();
}

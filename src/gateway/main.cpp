// aprs2mesh-gw - APRS-IS gateway for the mesh channel.
//
// Boot modes: portal when there is no valid configuration or a long press asked
// for it, run mode otherwise. The access point stays up in both modes (AP+STA)
// so the portal can be used while the igate keeps running.
//
// The radio seam is not implemented yet: text reception, text publication and
// NodeInfo only log. RadioLib TX/RX is pending in the tracker change.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_attr.h>

#include <cstring>

#include "aprsis_session.h"
#include "aprsis_transport.h"
#include "config_store.h"
#include "gateway.h"
#include "gateway_config.h"
#include "identity.h"
#include "monitor.h"
#include "monitor_server.h"
#include "portal.h"
#include "version.h"

using namespace aprs2mesh;

namespace {

constexpr uint32_t kStatsIntervalMs = 30000;
constexpr uint32_t kPortalHoldMs = 3000;
// The T-Beam v1.1 has PWR, IO38 and RST buttons; there is no BOOT button.
// IO38 is the user button, active low with the internal pull-up.
constexpr uint8_t kPortalButtonPin = 38;
constexpr uint32_t kForcePortalMagic = 0x47574D31u;  // "GWM1"
constexpr char kAccessPointSsid[] = "aprs2mesh-gw";
constexpr char kAccessPointPassword[] = "aprs2meshgw";

// Same trick as the tracker: the user button (IO38) is read only in run mode,
// and a long press leaves a flag in RTC memory for the next boot.
RTC_NOINIT_ATTR uint32_t gForcePortalFlag;

GatewayConfigStore gStore;
GatewayConfig gConfig;
AprsIsSession gSession;
AprsIsTransport gTransport;
Monitor gMonitor;
MonitorServer gMonitorServer;
NodeInfoScheduler gNodeInfo;
GatewayRuntime gRuntime;

void onMonitorFrame(void*, const char* tnc2, size_t length) {
  uint8_t ax25[400] = {};
  size_t ax25Length = 0;
  if (gMonitor.frameFor(tnc2, length, ax25, sizeof(ax25), ax25Length)) {
    gMonitorServer.broadcast(ax25, ax25Length);
  } else {
    Serial.printf("monitor: texto no convertible: %.*s\n", static_cast<int>(length),
                  tnc2);
  }
}

Gateway gGateway(0, onMonitorFrame, nullptr);
GatewayPortal gPortal(gStore, gGateway, gSession);

bool gLinkRunning = false;
bool gRebootForPortal = false;
uint32_t gLastStats = 0;
uint32_t gPacketId = 1;

// Placeholder for the radio layer: the text is logged with the configured hop
// limit and considered sent, which keeps the downlink queue moving until task
// 2.5 attaches RadioLib.
bool publishToMesh(const char* text, size_t length) {
  Serial.printf("mesh tx (hops %u): %.*s\n",
                static_cast<unsigned>(gGateway.hopLimit()),
                static_cast<int>(length), text);
  return true;
}

void onServerLine(void*, const char* line, size_t length) {
  gGateway.onServerPacket(line, length, millis());
}

bool portalLongPress(uint32_t nowMs) {
  static bool pressed = false;
  static uint32_t pressedAt = 0;

  if (digitalRead(kPortalButtonPin) != LOW) {
    pressed = false;
    return false;
  }
  if (!pressed) {
    pressed = true;
    pressedAt = nowMs;
    return false;
  }
  return nowMs - pressedAt >= kPortalHoldMs;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(kAccessPointSsid, kAccessPointPassword)) {
    Serial.printf("wifi: AP %s at %s\n", kAccessPointSsid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("wifi: AP failed to start");
  }
}

void startLink(uint32_t nodeNumber) {
  if (gConfig.wifiSsid[0] != '\0') {
    WiFi.begin(gConfig.wifiSsid, gConfig.wifiPassword);
    Serial.printf("wifi: connecting to %s\n", gConfig.wifiSsid);
  }

  AprsIsSettings sessionSettings;
  std::strncpy(sessionSettings.callsign, gConfig.callsign,
               sizeof(sessionSettings.callsign) - 1);
  sessionSettings.passcode = gConfig.passcode;
  std::strncpy(sessionSettings.filter, gConfig.filter,
               sizeof(sessionSettings.filter) - 1);
  gSession.configure(sessionSettings, millis());
  gTransport.configure(gConfig.host, gConfig.port);
  gSession.start(millis());

  GatewaySettings settings;
  std::strncpy(settings.callsign, gConfig.callsign, sizeof(settings.callsign) - 1);
  settings.gateOpen = gConfig.gateOpen;
  settings.flowTtlMinutes = gConfig.flowTtlMinutes;
  settings.maxFlows = gConfig.maxFlows;
  settings.uplinkPerMinute = gConfig.uplinkPerMinute;
  settings.downlinkPerMinute = gConfig.downlinkPerMinute;
  settings.minIntervalMs = gConfig.minIntervalMs;
  settings.hopLimit = gConfig.modem.hopLimit;
  gGateway.configure(settings, millis());

  GatewayIdentity identity;
  if (buildGatewayIdentity(gConfig.callsign, nodeNumber, gConfig.modem.hopLimit,
                           identity)) {
    gNodeInfo.configure(identity, millis());
    Serial.printf("identity: %s, short %s, role CLIENT_MUTE, hops %u\n",
                  identity.longName, identity.shortName,
                  static_cast<unsigned>(identity.hopLimit));
  }

  gMonitorServer.begin();
  gLinkRunning = true;
  Serial.printf("mode: run, gateway %s -> %s:%u, monitor :%u\n", gConfig.callsign,
                gConfig.host, static_cast<unsigned>(gConfig.port),
                static_cast<unsigned>(MonitorServer::kPort));
}

void updateRuntime() {
  gRuntime.mode = gLinkRunning ? "operación" : "portal";
  gRuntime.freeHeap = ESP.getFreeHeap();
  gRuntime.uptimeMs = millis();
  WiFi.softAPIP().toString().toCharArray(gRuntime.apIp,
                                         static_cast<unsigned>(sizeof(gRuntime.apIp)));
  WiFi.localIP().toString().toCharArray(gRuntime.staIp,
                                        static_cast<unsigned>(sizeof(gRuntime.staIp)));
  gRuntime.wifiStatus = static_cast<int32_t>(WiFi.status());
  gRuntime.monitorClients =
      static_cast<uint8_t>(gMonitorServer.clientCount());
  gRuntime.monitorDiscarded = gMonitorServer.discardedBytes();
  gRuntime.monitorDropped = gMonitorServer.droppedClients();
  gPortal.setRuntime(gRuntime);
}

void printStats() {
  const GatewayCounters& c = gGateway.counters();
  Serial.printf(
      "stats: session=%s | mesh=%u up_ok=%u inj=%u buf=%u | server=%u down_ok=%u "
      "pub=%u | monitor=%u\n",
      sessionStateText(gSession.state()), static_cast<unsigned>(c.meshTexts),
      static_cast<unsigned>(c.uplinkAccepted), static_cast<unsigned>(c.injected),
      static_cast<unsigned>(gGateway.bufferedUplinks()),
      static_cast<unsigned>(c.serverPackets),
      static_cast<unsigned>(c.downlinkAdmitted),
      static_cast<unsigned>(c.downlinkPublished),
      static_cast<unsigned>(gMonitorServer.clientCount()));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("aprs2mesh gateway %s\n", APRS2MESH_VERSION);

  const uint32_t nodeNumber = static_cast<uint32_t>(ESP.getEfuseMac());
  gGateway.setNodeNumber(nodeNumber);
  startAccessPoint();

  // IO38 is input-only and has no internal pull-up; the T-Beam provides the
  // external pull-up for its user button.
  pinMode(kPortalButtonPin, INPUT);
  delay(20);

  const bool forcedByRtc = gForcePortalFlag == kForcePortalMagic;
  gForcePortalFlag = 0;

  const bool loaded = gStore.load(gConfig);
  const bool forcePortal = forcedByRtc || digitalRead(kPortalButtonPin) == LOW;
  if (!loaded) {
    gConfig = defaultGatewayConfig();
    gRuntime.note = "sin configuración válida";
  }

  gPortal.begin(gConfig);

  if (!loaded || forcePortal) {
    Serial.printf("mode: portal (%s)\n", loaded ? "button" : "no config");
    Serial.printf("free heap: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));
    return;
  }

  startLink(nodeNumber);
  Serial.printf("node: !%08x\n", static_cast<unsigned>(nodeNumber));
  Serial.printf("free heap: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));
}

void loop() {
  const uint32_t now = millis();

  if (gLinkRunning) {
    gTransport.poll(gSession, now, onServerLine, nullptr);

    if (gSession.canInject()) {
      char line[400] = {};
      size_t length = 0;
      while (gGateway.nextInjection(now, line, sizeof(line), length)) {
        char wire[404] = {};
        std::memcpy(wire, line, length);
        wire[length++] = '\r';
        wire[length++] = '\n';
        const bool sent = gTransport.send(wire, length);
        if (sent) {
          Serial.printf("inject: node=%08x callsign=%s\n",
                        static_cast<unsigned>(gGateway.pendingFromNode()),
                        gGateway.pendingSource());
        } else {
          Serial.println("inject: send failed, frame returns to the buffer");
        }
        gGateway.onInjectionResult(sent, now);
        if (!sent) {
          break;
        }
      }
    }

    char text[kMaxTextBytes + 1] = {};
    size_t textLength = 0;
    while (gGateway.popDownlink(now, text, sizeof(text), textLength)) {
      publishToMesh(text, textLength);
    }

    if (gNodeInfo.due(now)) {
      uint8_t packet[256] = {};
      const size_t length =
          gNodeInfo.buildPacket(gPacketId++, packet, sizeof(packet));
      if (length > 0) {
        Serial.printf("mesh nodeinfo: %s (%u bytes)\n",
                      gNodeInfo.identity().longName,
                      static_cast<unsigned>(length));
      }
      gNodeInfo.markSent(now);
    }

    gMonitorServer.poll();

    if (!gRebootForPortal && portalLongPress(now)) {
      gRebootForPortal = true;
      gForcePortalFlag = kForcePortalMagic;
      Serial.println("portal: release the button to reboot into portal mode");
    }
    if (gRebootForPortal && digitalRead(kPortalButtonPin) != LOW) {
      ESP.restart();
    }
  }

  updateRuntime();
  gPortal.poll();

  if (gLinkRunning && now - gLastStats >= kStatsIntervalMs) {
    gLastStats = now;
    printStats();
  }
  delay(2);
}

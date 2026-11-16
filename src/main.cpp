// aprs2mesh - KISS TNC bridge to a Meshtastic channel.
//
// Boot modes: portal when there is no valid configuration or a long press
// asked for it, run mode otherwise. The access point stays up in both modes so
// the status page is reachable without a computer; the KISS services and the
// mesh link only run in run mode.
//
// The radio seam still logs instead of transmitting: RadioLib TX/RX is tasks
// 3.3-3.6 in openspec/changes/add-kiss-mesh-gateway/tasks.md.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_attr.h>

#include <cstring>

#include "bridge.h"
#include "config.h"
#include "config_store.h"
#include "kiss_hub.h"
#include "portal.h"
#include "transport/bt_spp_link.h"
#include "transport/tcp_kiss_server.h"
#include "version.h"

namespace {

constexpr uint32_t kStatsIntervalMs = 30000;
constexpr uint32_t kPortalHoldMs = 3000;
// The T-Beam v1.1 has PWR, IO38 and RST buttons; there is no BOOT button.
// IO38 is the user button, active low with the internal pull-up.
constexpr uint8_t kPortalButtonPin = 38;
constexpr uint32_t kForcePortalMagic = 0x504F5254u;  // "PORT"
constexpr char kAccessPointSsid[] = "aprs2mesh";
constexpr char kAccessPointPassword[] = "aprs2mesh";

// Survives a software restart, so a long press in run mode can reboot straight
// into portal mode. The flag lets the button be read only as a normal input
// during operation, never as a boot strap.
RTC_NOINIT_ATTR uint32_t gForcePortalFlag;

aprs2mesh::KissHub gHub;
aprs2mesh::BtSppLink gBluetooth;
aprs2mesh::TcpKissServer gTcp;
aprs2mesh::ConfigStore gStore;
aprs2mesh::Config gConfig;
aprs2mesh::Portal gPortal(gStore);
aprs2mesh::PortalStatus gStatus;

char gLastError[64] = {};
uint32_t gLastStats = 0;
bool gLinkRunning = false;
bool rebootForPortal = false;

void setError(const char* message) {
  std::strncpy(gLastError, message, sizeof(gLastError) - 1);
  gLastError[sizeof(gLastError) - 1] = '\0';
}

// Placeholder for the radio layer: the queue is drained and each text is
// logged, which makes the pipeline observable before task 3.3 attaches
// RadioLib. Returning true keeps the airtime queue moving.
bool publishToMesh(void*, const char* text, size_t length) {
  Serial.printf("mesh tx: %.*s\n", static_cast<int>(length), text);
  return true;
}

aprs2mesh::Bridge gBridge(0, publishToMesh, nullptr);

bool portalButtonHeld() { return digitalRead(kPortalButtonPin) == LOW; }

// True when the button has been held long enough in run mode. The caller
// restarts into portal mode instead of trying to detect the press at boot.
bool portalLongPress(uint32_t now) {
  static bool pressed = false;
  static uint32_t pressedAt = 0;

  if (digitalRead(kPortalButtonPin) != LOW) {
    pressed = false;
    return false;
  }
  if (!pressed) {
    pressed = true;
    pressedAt = now;
    return false;
  }
  return now - pressedAt >= kPortalHoldMs;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(kAccessPointSsid, kAccessPointPassword)) {
    Serial.printf("wifi: AP %s at %s\n", kAccessPointSsid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    setError("no se pudo levantar el punto de acceso");
    Serial.println("wifi: AP failed to start");
  }
}

void startLink() {
  aprs2mesh::BridgeSettings settings;
  settings.stripPath = gConfig.stripPath;
  settings.maxPerMinute = gConfig.maxPerMinute;
  settings.minIntervalMs = gConfig.minIntervalMs;
  gBridge.configure(settings, millis());

  if (!gBluetooth.begin(gConfig.bluetoothName)) {
    setError("no se pudo iniciar Bluetooth SPP");
    Serial.println("bt: SPP failed to start");
  }
  gTcp.begin();
  Serial.printf("kiss: TCP port %u ready\n", static_cast<unsigned>(gTcp.kPort));
  gLinkRunning = true;
}

void updateStatus() {
  gStatus.mode = gLinkRunning ? "operación" : "portal";
  gStatus.freeHeap = ESP.getFreeHeap();
  gStatus.kissClients = static_cast<uint8_t>(gHub.clientCount());
  gStatus.queueSize = static_cast<uint8_t>(gBridge.txQueue().size());
  gStatus.counters = gBridge.counters();
  gStatus.txStats = gBridge.txQueue().stats();
  gStatus.lastError = gLastError;
  gPortal.setStatus(gStatus);
}

void printCounters() {
  const aprs2mesh::BridgeCounters& c = gBridge.counters();
  const aprs2mesh::TxQueueStats& t = gBridge.txQueue().stats();
  Serial.printf(
      "stats: up ok=%u dup=%u invalid=%u large=%u rejected=%u | down ok=%u "
      "ignored=%u invalid=%u large=%u echo=%u | mesh pub=%u fail=%u sent=%u "
      "drop_rate=%u drop_full=%u queue=%u\n",
      static_cast<unsigned>(c.uplinkAccepted),
      static_cast<unsigned>(c.uplinkDuplicate),
      static_cast<unsigned>(c.uplinkInvalid),
      static_cast<unsigned>(c.uplinkTooLarge),
      static_cast<unsigned>(c.uplinkRejected),
      static_cast<unsigned>(c.downlinkDelivered),
      static_cast<unsigned>(c.downlinkIgnored),
      static_cast<unsigned>(c.downlinkInvalid),
      static_cast<unsigned>(c.downlinkTooLarge),
      static_cast<unsigned>(c.ownEcho), static_cast<unsigned>(c.published),
      static_cast<unsigned>(c.publishFailed), static_cast<unsigned>(t.sent),
      static_cast<unsigned>(t.droppedRate),
      static_cast<unsigned>(t.droppedFull),
      static_cast<unsigned>(gBridge.txQueue().size()));
}

void onAx25(void*, const uint8_t* frame, size_t length) {
  aprs2mesh::Ax25Frame parsed;
  char tnc2[400] = {};

  if (aprs2mesh::parseAx25(frame, length, parsed) != aprs2mesh::Ax25Status::Ok) {
    Serial.printf("kiss rx: invalid AX.25 (%u bytes)\n", static_cast<unsigned>(length));
    return;
  }

  size_t written = 0;
  if (aprs2mesh::formatTnc2(parsed, true, tnc2, sizeof(tnc2), written) !=
      aprs2mesh::Ax25Status::Ok) {
    Serial.println("kiss rx: frame too large to format");
    return;
  }

  Serial.printf("kiss rx: %s\n", tnc2);
  gBridge.uplink(frame, length, millis());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("aprs2mesh %s\n", APRS2MESH_VERSION);

  // Meshtastic derives the node number from the low 32 bits of the MAC.
  gBridge.setNodeNumber(static_cast<uint32_t>(ESP.getEfuseMac()));
  startAccessPoint();

  // IO38 is input-only and has no internal pull-up; the T-Beam provides the
  // external pull-up for its user button.
  pinMode(kPortalButtonPin, INPUT);
  delay(20);

  // A long press in run mode sets the RTC flag and restarts; the flag is
  // consumed here and cleared so the next normal boot goes back to run mode.
  const bool forcedByRtc = gForcePortalFlag == kForcePortalMagic;
  gForcePortalFlag = 0;

  const bool loaded = gStore.load(gConfig);
  const bool forcePortal = forcedByRtc || portalButtonHeld();
  if (!loaded) {
    gConfig = aprs2mesh::defaultConfig();
    setError("sin configuración válida");
  }

  if (!loaded || forcePortal) {
    if (forcePortal && loaded) {
      setError("portal forzado por el botón");
    }
    gPortal.begin(gConfig);
    Serial.printf("mode: portal (%s)\n", loaded ? "button" : "no config");
  } else {
    gPortal.begin(gConfig);
    startLink();
    Serial.printf("mode: run, callsign %s, channel %s\n", gConfig.callsign,
                  gConfig.channelName);
  }

  Serial.printf("node: !%08x\n", static_cast<unsigned>(gBridge.nodeNumber()));
  Serial.printf("free heap: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));
}

void loop() {
  const uint32_t now = millis();

  if (gLinkRunning) {
    gBluetooth.poll(gHub, onAx25, nullptr);
    gTcp.poll(gHub, onAx25, nullptr);
    gHub.dropFailedClients();

    // One text per pass: the queue already spaces transmissions, this only
    // bounds the work done per loop when the radio seam is blocking.
    gBridge.drain(now, 1);
  }

  if (gLinkRunning && !rebootForPortal && portalLongPress(now)) {
    // Set the flag now and wait for the release before restarting, so the next
    // boot consumes the flag with the button in a known state.
    rebootForPortal = true;
    gForcePortalFlag = kForcePortalMagic;
    Serial.println("portal: release the button to reboot into portal mode");
  }
  if (rebootForPortal && digitalRead(kPortalButtonPin) != LOW) {
    ESP.restart();
  }

  updateStatus();
  gPortal.poll();

  if (gLinkRunning && now - gLastStats >= kStatsIntervalMs) {
    gLastStats = now;
    printCounters();
  }
  delay(2);
}

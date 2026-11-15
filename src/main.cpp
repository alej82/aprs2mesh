// aprs2mesh - KISS TNC bridge to a Meshtastic channel.
//
// Current milestone: KISS plumbing (Bluetooth SPP + TCP) feeding the AX.25
// codec. Frames are decoded and logged on the serial console; the mesh link
// uplink/downlink is added by tasks 3.x and 4.x in
// openspec/changes/add-kiss-mesh-gateway/tasks.md.

#include <Arduino.h>
#include <WiFi.h>

#include "ax25.h"
#include "kiss_hub.h"
#include "transport/bt_spp_link.h"
#include "transport/tcp_kiss_server.h"
#include "version.h"

namespace {

aprs2mesh::KissHub gHub;
aprs2mesh::BtSppLink gBluetooth;
aprs2mesh::TcpKissServer gTcp;

// Defaults until the configuration portal (task 5.3) owns them.
constexpr char kDeviceName[] = "aprs2mesh";
constexpr char kAccessPointSsid[] = "aprs2mesh";
constexpr char kAccessPointPassword[] = "aprs2mesh";

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
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("aprs2mesh %s\n", APRS2MESH_VERSION);

  if (!gBluetooth.begin(kDeviceName)) {
    Serial.println("bt: SPP failed to start");
  }

  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(kAccessPointSsid, kAccessPointPassword)) {
    Serial.printf("wifi: AP %s at %s\n", kAccessPointSsid,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("wifi: AP failed to start");
  }

  gTcp.begin();
  Serial.printf("kiss: TCP port %u ready\n", static_cast<unsigned>(gTcp.kPort));
  Serial.printf("free heap: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));
}

void loop() {
  gBluetooth.poll(gHub, onAx25, nullptr);
  gTcp.poll(gHub, onAx25, nullptr);
  gHub.dropFailedClients();
  delay(2);
}

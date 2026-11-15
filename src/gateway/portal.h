#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "aprsis_session.h"
#include "config_store.h"
#include "gateway.h"
#include "gateway_config.h"

namespace aprs2mesh {

// Values main refreshes every loop so the status page never reads a half-made
// snapshot of the WiFi and monitor state.
struct GatewayRuntime {
  const char* mode = "portal";
  const char* note = "";
  uint32_t freeHeap = 0;
  uint32_t uptimeMs = 0;
  char apIp[16] = {};
  char staIp[16] = {};
  int32_t wifiStatus = 0;
  uint8_t monitorClients = 0;
  uint32_t monitorDiscarded = 0;
  uint32_t monitorDropped = 0;
};

class GatewayPortal {
 public:
  GatewayPortal(GatewayConfigStore& store, Gateway& gateway, AprsIsSession& session);

  void begin(const GatewayConfig& active);
  void poll();
  void setRuntime(const GatewayRuntime& runtime) { runtime_ = runtime; }

 private:
  void sendPage();
  void handleSave();
  void handleGate();
  void fail(const String& message);

  GatewayConfigStore& store_;
  Gateway& gateway_;
  AprsIsSession& session_;
  WebServer server_{80};
  GatewayConfig active_;
  GatewayRuntime runtime_;
  String error_;
  bool rebootPending_ = false;
  uint32_t rebootAt_ = 0;
};

}  // namespace aprs2mesh

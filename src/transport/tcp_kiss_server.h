#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "kiss_hub.h"

namespace aprs2mesh {

// KISS over TCP (port 8001). Each accepted connection is a KISS client; bytes
// are decoded per client by KissHub.
class TcpKissServer {
 public:
  static constexpr uint16_t kPort = 8001;
  static constexpr size_t kMaxConnections = KissHub::kMaxClients;

  void begin();
  void poll(KissHub& hub, Ax25Handler handler, void* context);
  size_t clientCount() const;

 private:
  struct Slot : public KissSink {
    WiFiClient client;
    bool registered = false;
    bool failed = false;
    size_t write(const uint8_t* data, size_t length) override;
  };

  Slot* freeSlot();
  void drop(Slot& slot, KissHub& hub);

  WiFiServer server_{kPort};
  Slot slots_[kMaxConnections] = {};
};

}  // namespace aprs2mesh

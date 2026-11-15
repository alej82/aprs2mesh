#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace aprs2mesh {

// Read-only KISS monitor over TCP. Clients receive every frame the gateway
// injects or publishes; anything they write back is discarded and counted.
class MonitorServer {
 public:
  static constexpr uint16_t kPort = 8002;
  static constexpr size_t kMaxClients = 4;

  void begin();
  void poll();
  // Returns the number of clients that accepted the whole frame.
  size_t broadcast(const uint8_t* ax25, size_t length);

  size_t clientCount() const;
  uint32_t discardedBytes() const { return discardedBytes_; }
  uint32_t droppedClients() const { return droppedClients_; }

 private:
  struct Slot {
    WiFiClient client;
    bool used = false;
  };

  void drop(Slot& slot);

  WiFiServer server_{kPort};
  Slot slots_[kMaxClients];
  uint32_t discardedBytes_ = 0;
  uint32_t droppedClients_ = 0;
};

}  // namespace aprs2mesh

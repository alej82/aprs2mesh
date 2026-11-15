#include "monitor_server.h"

#include "kiss_codec.h"

namespace aprs2mesh {

void MonitorServer::begin() {
  server_.begin();
  server_.setNoDelay(true);
}

void MonitorServer::drop(Slot& slot) {
  slot.client.stop();
  slot.client = WiFiClient();
  slot.used = false;
}

size_t MonitorServer::clientCount() const {
  size_t count = 0;
  for (const auto& slot : slots_) {
    if (slot.used) {
      ++count;
    }
  }
  return count;
}

void MonitorServer::poll() {
  for (auto& slot : slots_) {
    if (slot.used && !slot.client.connected()) {
      drop(slot);
    }

    if (!slot.used) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      WiFiClient incoming = server_.accept();
#else
      WiFiClient incoming = server_.available();
#endif
      if (!incoming) {
        continue;
      }
      slot.client = incoming;
      slot.used = true;
      continue;
    }

    // The monitor is read-only: whatever the client sends is thrown away.
    while (slot.client.available() > 0) {
      const int value = slot.client.read();
      if (value < 0) {
        break;
      }
      ++discardedBytes_;
    }
  }
}

size_t MonitorServer::broadcast(const uint8_t* ax25, size_t length) {
  uint8_t frame[kKissMaxFrame * 2 + 4] = {};
  const size_t encoded =
      KissEncoder::encode(/*portAndType=*/0x00, ax25, length, frame, sizeof(frame));
  if (encoded == 0) {
    return 0;
  }

  size_t delivered = 0;
  for (auto& slot : slots_) {
    if (!slot.used) {
      continue;
    }
    if (!slot.client.connected()) {
      drop(slot);
      continue;
    }
    const size_t written = slot.client.write(frame, encoded);
    if (written != encoded) {
      drop(slot);
      ++droppedClients_;
      continue;
    }
    ++delivered;
  }
  return delivered;
}

}  // namespace aprs2mesh

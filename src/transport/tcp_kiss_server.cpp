#include "tcp_kiss_server.h"

namespace aprs2mesh {

size_t TcpKissServer::Slot::write(const uint8_t* data, size_t length) {
  if (!client.connected()) {
    failed = true;
    return 0;
  }
  const size_t written = client.write(data, length);
  if (written != length) {
    failed = true;
  }
  return written;
}

void TcpKissServer::begin() {
  server_.begin();
  server_.setNoDelay(true);
}

size_t TcpKissServer::clientCount() const {
  size_t count = 0;
  for (const auto& slot : slots_) {
    if (slot.registered) {
      ++count;
    }
  }
  return count;
}

void TcpKissServer::drop(Slot& slot, KissHub& hub) {
  if (slot.registered) {
    hub.removeClient(&slot);
  }
  slot.client.stop();
  slot.client = WiFiClient();
  slot.registered = false;
  slot.failed = false;
}

void TcpKissServer::poll(KissHub& hub, Ax25Handler handler, void* context) {
  for (auto& slot : slots_) {
    if (slot.registered && (!slot.client.connected() || slot.failed)) {
      drop(slot, hub);
    }

    if (!slot.registered) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
      WiFiClient incoming = server_.accept();
#else
      WiFiClient incoming = server_.available();
#endif
      if (!incoming) {
        continue;
      }
      slot.client = incoming;
      slot.failed = false;
      slot.registered = hub.addClient(&slot);
      if (!slot.registered) {
        slot.client.stop();
        slot.client = WiFiClient();
      }
      continue;
    }

    while (slot.client.available() > 0) {
      const int value = slot.client.read();
      if (value < 0) {
        break;
      }
      if (hub.feed(&slot, static_cast<uint8_t>(value))) {
        handler(context, hub.frame(), hub.frameLen());
      }
    }
  }
}

}  // namespace aprs2mesh

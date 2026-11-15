#include "kiss_hub.h"

#include <cstring>

namespace aprs2mesh {

bool KissHub::addClient(KissSink* sink) {
  if (sink == nullptr || find(sink) != nullptr) {
    return false;
  }
  for (auto& client : clients_) {
    if (client.sink == nullptr) {
      client = Client{};
      client.sink = sink;
      client.decoder.reset();
      return true;
    }
  }
  return false;
}

void KissHub::removeClient(const KissSink* sink) {
  for (auto& client : clients_) {
    if (client.sink == sink) {
      client = Client{};
      return;
    }
  }
}

size_t KissHub::clientCount() const {
  size_t count = 0;
  for (const auto& client : clients_) {
    if (client.sink != nullptr) {
      ++count;
    }
  }
  return count;
}

KissHub::Client* KissHub::find(const KissSink* sink) {
  for (auto& client : clients_) {
    if (client.sink == sink) {
      return &client;
    }
  }
  return nullptr;
}

bool KissHub::feed(const KissSink* origin, uint8_t byte) {
  Client* client = find(origin);
  if (client == nullptr) {
    return false;
  }
  if (!client->decoder.feed(byte)) {
    return false;
  }

  const uint8_t* raw = client->decoder.frame();
  const size_t rawLen = client->decoder.frameLen();
  if (rawLen < 1 || kissClassify(raw[0]) != KissKind::Data) {
    return false;  // modem commands and unknown types are consumed silently
  }

  frameLen_ = rawLen - 1;
  if (frameLen_ > 0) {
    std::memcpy(frame_, raw + 1, frameLen_);
  }
  return true;
}

size_t KissHub::broadcastAx25(const uint8_t* ax25, size_t length) {
  if (ax25 == nullptr || length == 0) {
    return 0;
  }

  const size_t needed = KissEncoder::encodedSize(0x00, ax25, length);
  if (needed > sizeof(tx_)) {
    return 0;
  }

  const size_t encoded = KissEncoder::encode(0x00, ax25, length, tx_, sizeof(tx_));
  if (encoded == 0) {
    return 0;
  }

  size_t delivered = 0;
  for (auto& client : clients_) {
    if (client.sink == nullptr) {
      continue;
    }
    if (client.sink->write(tx_, encoded) == encoded) {
      ++delivered;
    } else {
      client.failed = true;
    }
  }
  return delivered;
}

size_t KissHub::dropFailedClients() {
  size_t dropped = 0;
  for (auto& client : clients_) {
    if (client.sink != nullptr && client.failed) {
      client = Client{};
      ++dropped;
    }
  }
  return dropped;
}

}  // namespace aprs2mesh

#pragma once

#include <cstddef>
#include <cstdint>

#include "kiss_codec.h"

namespace aprs2mesh {

// Delivers a decoded AX.25 frame. context is the pointer given to poll().
using Ax25Handler = void (*)(void* context, const uint8_t* ax25, size_t length);

// A connected KISS client (Bluetooth SPP, TCP socket, test fake).
class KissSink {
 public:
  virtual ~KissSink() = default;
  // Returns bytes accepted; fewer than length means the client is congested
  // or gone.
  virtual size_t write(const uint8_t* data, size_t length) = 0;
};

// Fan-in/fan-out for KISS clients. Each client owns its decoder so two
// clients can send concurrently without corrupting each other's framing.
// Outgoing frames are encoded once and written to every client in turn,
// which keeps a frame from being interleaved inside a client's stream.
class KissHub {
 public:
  static constexpr size_t kMaxClients = 4;

  bool addClient(KissSink* sink);
  void removeClient(const KissSink* sink);
  size_t clientCount() const;

  // Feeds one byte received from `origin`. Returns true when that client
  // completed a data frame, available through frame()/frameLen(). Command and
  // unknown frame types are consumed and never reported.
  bool feed(const KissSink* origin, uint8_t byte);

  const uint8_t* frame() const { return frame_; }
  size_t frameLen() const { return frameLen_; }

  // Wraps an AX.25 frame in KISS and sends it to every client.
  // Returns the number of clients that accepted the whole frame.
  size_t broadcastAx25(const uint8_t* ax25, size_t length);

  // Drops clients whose sink did not accept a full frame.
  size_t dropFailedClients();

 private:
  struct Client {
    KissSink* sink = nullptr;
    KissDecoder decoder;
    bool failed = false;
  };

  Client* find(const KissSink* sink);

  Client clients_[kMaxClients] = {};
  uint8_t frame_[kKissMaxFrame] = {};
  size_t frameLen_ = 0;
  uint8_t tx_[kKissMaxFrame * 2 + 4] = {};
};

}  // namespace aprs2mesh

#pragma once

#include <cstddef>
#include <cstdint>

#include "ax25.h"
#include "dedupe.h"
#include "mesh_text.h"
#include "tx_queue.h"

namespace aprs2mesh {

struct BridgeSettings {
  bool stripPath = false;
  uint16_t maxPerMinute = 6;
  uint32_t minIntervalMs = 10000;
};

struct BridgeCounters {
  uint32_t uplinkAccepted = 0;
  uint32_t uplinkDuplicate = 0;
  uint32_t uplinkInvalid = 0;
  uint32_t uplinkTooLarge = 0;
  uint32_t uplinkRejected = 0;  // rate limit or full queue
  uint32_t published = 0;
  uint32_t publishFailed = 0;
  uint32_t downlinkDelivered = 0;
  uint32_t downlinkIgnored = 0;  // no A# prefix
  uint32_t downlinkInvalid = 0;
  uint32_t downlinkTooLarge = 0;
  uint32_t ownEcho = 0;
};

// Publishes a prefixed text into the mesh. Returns false when the radio layer
// could not take it; the bridge counts the failure and drops the text.
using MeshTextPublisher = bool (*)(void* context, const char* text, size_t length);

// The whole KISS <-> mesh pipeline, with no Arduino or radio dependency so the
// behaviour is testable on the host and the radio layer is just a seam.
class Bridge {
 public:
  static constexpr size_t kMaxTnc2 = 400;
  static constexpr size_t kMaxAx25 = 400;
  static constexpr uint32_t kUplinkDedupeMs = 30000;

  Bridge(uint32_t nodeNumber, MeshTextPublisher publisher, void* context);

  void configure(const BridgeSettings& settings, uint32_t nowMs);

  // KISS -> AX.25 -> TNC2 (with the full path unless stripping is enabled) ->
  // A# prefix -> airtime queue. Returns true when the frame was queued.
  bool uplink(const uint8_t* ax25, size_t length, uint32_t nowMs);

  // Text from the channel -> prefix check -> TNC2 -> AX.25. Returns true and
  // fills `ax25` when the frame should be handed to the KISS clients.
  bool downlink(const char* text, size_t length, uint32_t fromNode, uint32_t nowMs,
                uint8_t* ax25, size_t capacity, size_t& ax25Length);

  // Publishes up to `maximum` queued texts whose spacing is due.
  size_t drain(uint32_t nowMs, size_t maximum = 1);

  uint32_t nodeNumber() const { return nodeNumber_; }
  // The node number comes from eFuse, which is read during boot, after static
  // initialisation.
  void setNodeNumber(uint32_t nodeNumber) { nodeNumber_ = nodeNumber; }
  const BridgeSettings& settings() const { return settings_; }
  const BridgeCounters& counters() const { return counters_; }
  const TxQueue& txQueue() const { return txQueue_; }

 private:
  uint32_t nodeNumber_;
  MeshTextPublisher publisher_;
  void* context_;
  BridgeSettings settings_;
  BridgeCounters counters_;
  DedupeWindow uplinkDedupe_{kUplinkDedupeMs};
  TxQueue txQueue_{6, 10000};
};

}  // namespace aprs2mesh

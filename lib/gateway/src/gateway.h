#pragma once

#include <cstddef>
#include <cstdint>

#include "aprs_line.h"
#include "dedupe.h"
#include "flow_table.h"
#include "gateway_config.h"
#include "rate_meter.h"
#include "tx_queue.h"
#include "uplink_buffer.h"

namespace aprs2mesh {

struct GatewaySettings {
  char callsign[kGwCallsign] = {};
  bool gateOpen = false;
  uint16_t flowTtlMinutes = 60;
  uint8_t maxFlows = 8;
  uint16_t uplinkPerMinute = 6;
  uint16_t downlinkPerMinute = 6;
  uint32_t minIntervalMs = 10000;
};

// Three groups on purpose: what the mesh offered, what the pinhole admitted and
// what actually left through each side. "Nothing arrived" and "everything was
// rejected" must not look the same to the operator.
struct GatewayCounters {
  uint32_t meshTexts = 0;
  uint32_t uplinkAccepted = 0;
  uint32_t uplinkIgnored = 0;      // no A# prefix
  uint32_t uplinkInvalid = 0;      // prefix present but not a valid TNC2 line
  uint32_t uplinkOwnEcho = 0;
  uint32_t uplinkDuplicate = 0;
  uint32_t uplinkGateClosed = 0;
  uint32_t uplinkRateLimited = 0;
  uint32_t uplinkBufferFull = 0;
  uint32_t uplinkBufferExpired = 0;
  uint32_t injected = 0;
  uint32_t injectFailed = 0;

  uint32_t serverPackets = 0;
  uint32_t downlinkAdmitted = 0;
  uint32_t downlinkRejected = 0;
  uint32_t downlinkDuplicate = 0;
  uint32_t downlinkRateLimited = 0;
  uint32_t downlinkQueueFull = 0;
  uint32_t downlinkTooLarge = 0;
  uint32_t downlinkPublished = 0;
};

// Text-only APRS-IS gateway pipeline. It never builds AX.25: the monitor does
// that outside the hot path. Everything is host-testable; the sockets and the
// radio live in the Arduino layer.
class Gateway {
 public:
  using MonitorEvent = void (*)(void* context, const char* tnc2, size_t length);

  Gateway(uint32_t nodeNumber, MonitorEvent monitor, void* context);

  void configure(const GatewaySettings& settings, uint32_t nowMs);
  void setGateOpen(bool open) { settings_.gateOpen = open; }
  bool gateOpen() const { return settings_.gateOpen; }

  // Mesh reception, already deduplicated by (from, id). Returns true when the
  // frame entered the uplink buffer.
  bool onMeshPacket(uint32_t fromNode, uint32_t packetId, const char* text,
                    size_t length, uint32_t nowMs);

  // Next buffered uplink as an injectable line (path preserved plus qAR).
  bool nextInjection(uint32_t nowMs, char* out, size_t capacity, size_t& length);
  void onInjectionResult(bool sent, uint32_t nowMs);
  bool injectionPending() const { return pending_; }
  const char* pendingSource() const { return pendingSource_; }
  uint32_t pendingFromNode() const { return pendingFromNode_; }

  // One non-comment line from APRS-IS.
  void onServerPacket(const char* line, size_t length, uint32_t nowMs);

  // Pops one admitted downlink, emits it to the monitor and returns the mesh
  // text ("A#...").
  bool popDownlink(uint32_t nowMs, char* out, size_t capacity, size_t& length);

  void pruneFlows(uint32_t nowMs) { flows_.prune(nowMs); }
  size_t bufferedUplinks() const { return buffer_.size(); }

  const GatewayCounters& counters() const { return counters_; }
  const FlowTable& flows() const { return flows_; }
  const TxQueue& downlinkQueue() const { return downlinkQueue_; }
  const char* lastReject() const { return lastReject_; }
  uint32_t lastRejectAt() const { return lastRejectAt_; }
  const char* lastPacket() const { return lastPacket_; }
  uint32_t lastPacketAt() const { return lastPacketAt_; }
  uint32_t nodeNumber() const { return nodeNumber_; }
  void setNodeNumber(uint32_t nodeNumber) { nodeNumber_ = nodeNumber; }

 private:
  void reject(const char* reason, uint32_t nowMs);
  void rememberPacket(const char* line, size_t length, uint32_t nowMs);
  void syncExpired(uint32_t before);
  uint64_t uplinkKey(const char* source, const char* payload, size_t length) const;
  uint64_t downlinkKey(const char* source, const AprsMessage& message) const;

  uint32_t nodeNumber_;
  MonitorEvent monitor_;
  void* context_;
  GatewaySettings settings_;
  GatewayCounters counters_;

  DedupeWindow rxDedupe_{kReceiveDedupeMs};
  DedupeWindow uplinkDedupe_{30000};
  DedupeWindow downlinkDedupe_{30000};
  RateMeter uplinkRate_{6};
  UplinkBuffer buffer_;
  FlowTable flows_;
  TxQueue downlinkQueue_{6, 10000};

  bool pending_ = false;
  char pendingSource_[kAprsAddress] = {};
  uint32_t pendingFromNode_ = 0;
  char pendingTnc2_[kUplinkLineMax] = {};
  uint16_t pendingTnc2Length_ = 0;

  const char* lastReject_ = "";
  uint32_t lastRejectAt_ = 0;
  char lastPacket_[64] = {};
  uint32_t lastPacketAt_ = 0;
};

}  // namespace aprs2mesh

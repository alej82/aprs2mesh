#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh_proto.h"

namespace aprs2mesh {

struct TxQueueStats {
  uint32_t accepted = 0;
  uint32_t sent = 0;
  uint32_t droppedFull = 0;
  uint32_t droppedRate = 0;
};

// Bounded airtime queue. `push` decides admission against the per-minute limit;
// `pop` hands out one text when the minimum spacing allows it, so a burst is
// spread over time instead of being transmitted back to back.
class TxQueue {
 public:
  static constexpr size_t kCapacity = 8;
  static constexpr size_t kWindow = 64;
  static constexpr uint32_t kRateWindowMs = 60000;

  TxQueue(uint16_t maxPerMinute, uint32_t minIntervalMs);

  // Updates the limits without discarding queued texts.
  void configure(uint16_t maxPerMinute, uint32_t minIntervalMs);

  bool push(const char* text, size_t length, uint32_t nowMs);
  bool pop(uint32_t nowMs, char* out, size_t capacity, size_t& length);

  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  uint16_t maxPerMinute() const { return maxPerMinute_; }
  uint32_t minIntervalMs() const { return minIntervalMs_; }
  const TxQueueStats& stats() const { return stats_; }

 private:
  struct Entry {
    char text[kMaxTextBytes + 1] = {};
    uint8_t length = 0;
  };

  bool rateAllows(uint32_t nowMs) const;
  void recordSend(uint32_t nowMs);

  Entry entries_[kCapacity];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;

  uint32_t sendTimes_[kWindow] = {};
  uint8_t sendIndex_ = 0;
  uint8_t sendCount_ = 0;

  uint16_t maxPerMinute_ = 1;
  uint32_t minIntervalMs_ = 0;
  uint32_t nextSendAt_ = 0;
  bool hasSent_ = false;
  TxQueueStats stats_;
};

}  // namespace aprs2mesh

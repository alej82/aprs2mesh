#pragma once

#include <cstddef>
#include <cstdint>

#include "aprs_line.h"

namespace aprs2mesh {

// One station that originated a recent uplink. Downlinks are admitted only for
// destinations present here (the "pinhole").
struct Flow {
  char callsign[kAprsAddress] = {};
  uint32_t openedAt = 0;
  uint32_t lastUplinkAt = 0;
  bool used = false;
};

class FlowTable {
 public:
  static constexpr size_t kMaxCapacity = 8;
  static constexpr uint16_t kDefaultTtlMinutes = 60;

  FlowTable();

  void configure(uint16_t ttlMinutes, size_t capacity);
  // Opens a flow or refreshes an existing one. When the table is full the
  // oldest flow is evicted.
  void open(const char* callsign, uint32_t nowMs);
  // True while the flow exists and its age is below the TTL.
  bool active(const char* callsign, uint32_t nowMs) const;
  void prune(uint32_t nowMs);
  void clear();

  size_t size() const { return count_; }
  size_t capacity() const { return capacity_; }
  uint32_t ttlMs() const { return ttlMs_; }
  uint32_t evictedCount() const { return evicted_; }
  const Flow& at(size_t index) const { return entries_[index]; }
  size_t slotCount() const { return kMaxCapacity; }

 private:
  Flow* find(const char* callsign);
  const Flow* find(const char* callsign) const;

  Flow entries_[kMaxCapacity];
  size_t count_ = 0;
  size_t capacity_ = kMaxCapacity;
  uint32_t ttlMs_ = static_cast<uint32_t>(kDefaultTtlMinutes) * 60000u;
  uint32_t evicted_ = 0;
};

}  // namespace aprs2mesh

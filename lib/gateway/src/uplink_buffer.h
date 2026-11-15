#pragma once

#include <cstddef>
#include <cstdint>

#include "aprs_line.h"

namespace aprs2mesh {

inline constexpr size_t kUplinkCapacity = 8;
inline constexpr uint32_t kUplinkTtlMs = 30000;
inline constexpr size_t kUplinkLineMax = 400;

struct UplinkEntry {
  char tnc2[kUplinkLineMax] = {};
  uint16_t length = 0;
  char source[kAprsAddress] = {};
  uint32_t fromNode = 0;
  uint32_t queuedAt = 0;
};

// Holds uplinks while APRS-IS is unreachable. APRS-IS deduplicates on a 30 s
// window, so re-injecting older traffic is worse than losing it: entries expire
// and are counted instead of accumulating.
class UplinkBuffer {
 public:
  bool push(const char* tnc2, size_t length, const char* source,
            uint32_t fromNode, uint32_t nowMs);
  // Removes expired entries and returns the next live one.
  bool pop(uint32_t nowMs, UplinkEntry& out);
  void discardExpired(uint32_t nowMs);

  size_t size() const { return count_; }
  uint32_t droppedFull() const { return droppedFull_; }
  uint32_t droppedExpired() const { return droppedExpired_; }
  uint32_t ttlMs() const { return kUplinkTtlMs; }

 private:
  UplinkEntry entries_[kUplinkCapacity];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  uint32_t droppedFull_ = 0;
  uint32_t droppedExpired_ = 0;
};

}  // namespace aprs2mesh

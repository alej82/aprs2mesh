#include "uplink_buffer.h"

#include <cstring>

namespace aprs2mesh {

void UplinkBuffer::discardExpired(uint32_t nowMs) {
  while (count_ > 0) {
    const UplinkEntry& entry = entries_[head_];
    if (static_cast<uint32_t>(nowMs - entry.queuedAt) <= kUplinkTtlMs) {
      return;
    }
    head_ = static_cast<uint8_t>((head_ + 1) % kUplinkCapacity);
    --count_;
    ++droppedExpired_;
  }
}

bool UplinkBuffer::push(const char* tnc2, size_t length, const char* source,
                        uint32_t fromNode, uint32_t nowMs) {
  if (tnc2 == nullptr || length == 0 || length >= kUplinkLineMax) {
    return false;
  }
  discardExpired(nowMs);

  if (count_ >= kUplinkCapacity) {
    // Drop the oldest so the most recent traffic survives a long outage.
    head_ = static_cast<uint8_t>((head_ + 1) % kUplinkCapacity);
    --count_;
    ++droppedFull_;
  }

  UplinkEntry& entry = entries_[tail_];
  std::memcpy(entry.tnc2, tnc2, length);
  entry.tnc2[length] = '\0';
  entry.length = static_cast<uint16_t>(length);
  std::memset(entry.source, 0, sizeof(entry.source));
  if (source != nullptr) {
    std::strncpy(entry.source, source, sizeof(entry.source) - 1);
  }
  entry.fromNode = fromNode;
  entry.queuedAt = nowMs;
  tail_ = static_cast<uint8_t>((tail_ + 1) % kUplinkCapacity);
  ++count_;
  return true;
}

bool UplinkBuffer::pop(uint32_t nowMs, UplinkEntry& out) {
  discardExpired(nowMs);
  if (count_ == 0) {
    return false;
  }
  out = entries_[head_];
  head_ = static_cast<uint8_t>((head_ + 1) % kUplinkCapacity);
  --count_;
  return true;
}

}  // namespace aprs2mesh

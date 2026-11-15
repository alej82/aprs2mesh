#include "dedupe.h"

namespace aprs2mesh {

namespace {
constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;
}  // namespace

DedupeWindow::DedupeWindow(uint32_t windowMs) : windowMs_(windowMs) {}

bool DedupeWindow::contains(uint64_t key, uint32_t nowMs) const {
  for (size_t i = 0; i < kCapacity; ++i) {
    const Entry& entry = entries_[i];
    if (!entry.used || entry.key != key) {
      continue;
    }
    // Unsigned subtraction makes the comparison safe across the millis wrap.
    return static_cast<uint32_t>(nowMs - entry.seenAt) < windowMs_;
  }
  return false;
}

bool DedupeWindow::accept(uint64_t key, uint32_t nowMs) {
  for (size_t i = 0; i < kCapacity; ++i) {
    Entry& entry = entries_[i];
    if (!entry.used || entry.key != key) {
      continue;
    }
    if (static_cast<uint32_t>(nowMs - entry.seenAt) < windowMs_) {
      return false;
    }
    entry.seenAt = nowMs;
    return true;
  }

  Entry& slot = entries_[next_];
  slot.used = true;
  slot.key = key;
  slot.seenAt = nowMs;
  next_ = (next_ + 1) % kCapacity;
  return true;
}

void DedupeWindow::clear() {
  for (size_t i = 0; i < kCapacity; ++i) {
    entries_[i] = Entry{};
  }
  next_ = 0;
}

uint64_t hashBytes(uint64_t seed, const void* data, size_t length) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint64_t hash = seed;
  for (size_t i = 0; i < length; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

}  // namespace aprs2mesh

#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// Reception window for (from, id): a mesh flood delivers the same packet
// through several relays within seconds.
inline constexpr uint32_t kReceiveDedupeMs = 60000;

// Fixed-size window keyed by a 64-bit value. It remembers the first time a key
// was seen and rejects it until the window expires, which is what both APRS
// deduplication rules need: a client retry and a mesh flood are the same
// packet arriving twice.
class DedupeWindow {
 public:
  static constexpr size_t kCapacity = 32;

  explicit DedupeWindow(uint32_t windowMs);

  // True when the key was not seen inside the window; records it.
  bool accept(uint64_t key, uint32_t nowMs);

  // True when the key is still inside the window, without recording anything.
  bool contains(uint64_t key, uint32_t nowMs) const;

  void clear();
  uint32_t windowMs() const { return windowMs_; }

 private:
  struct Entry {
    uint64_t key = 0;
    uint32_t seenAt = 0;
    bool used = false;
  };

  Entry entries_[kCapacity];
  uint32_t windowMs_;
  size_t next_ = 0;
};

// FNV-1a over the pieces that identify an uplink frame. A hash keeps the table
// small; collisions only cost an extra dropped frame inside one window.
uint64_t hashBytes(uint64_t seed, const void* data, size_t length);

}  // namespace aprs2mesh

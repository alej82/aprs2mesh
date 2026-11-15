#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// Sliding one-minute counter. Unlike TxQueue this only meters events: it holds
// no payloads, which is what the APRS-IS side needs because the airtime queue
// does not apply to a TCP link.
class RateMeter {
 public:
  static constexpr size_t kWindow = 64;
  static constexpr uint32_t kWindowMs = 60000;

  explicit RateMeter(uint16_t perMinute = 1);

  void configure(uint16_t perMinute);
  // True when the event is allowed; records it. False leaves the window alone.
  bool allow(uint32_t nowMs);
  uint32_t used(uint32_t nowMs) const;
  uint16_t perMinute() const { return perMinute_; }

 private:
  uint32_t times_[kWindow] = {};
  uint8_t index_ = 0;
  uint8_t count_ = 0;
  uint16_t perMinute_ = 1;
};

}  // namespace aprs2mesh

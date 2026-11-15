#include "rate_meter.h"

namespace aprs2mesh {

RateMeter::RateMeter(uint16_t perMinute) { configure(perMinute); }

void RateMeter::configure(uint16_t perMinute) {
  perMinute_ = perMinute == 0 ? 1 : perMinute;
  if (perMinute_ > kWindow) {
    perMinute_ = static_cast<uint16_t>(kWindow);
  }
}

uint32_t RateMeter::used(uint32_t nowMs) const {
  uint32_t inWindow = 0;
  for (uint8_t i = 0; i < count_; ++i) {
    if (static_cast<uint32_t>(nowMs - times_[i]) < kWindowMs) {
      ++inWindow;
    }
  }
  return inWindow;
}

bool RateMeter::allow(uint32_t nowMs) {
  if (used(nowMs) >= perMinute_) {
    return false;
  }
  times_[index_] = nowMs;
  index_ = static_cast<uint8_t>((index_ + 1) % kWindow);
  if (count_ < kWindow) {
    ++count_;
  }
  return true;
}

}  // namespace aprs2mesh

#include "tx_queue.h"

#include <cstring>

namespace aprs2mesh {

TxQueue::TxQueue(uint16_t maxPerMinute, uint32_t minIntervalMs)
    : maxPerMinute_(maxPerMinute), minIntervalMs_(minIntervalMs) {
  configure(maxPerMinute, minIntervalMs);
}

void TxQueue::configure(uint16_t maxPerMinute, uint32_t minIntervalMs) {
  maxPerMinute_ = maxPerMinute;
  if (maxPerMinute_ == 0) {
    maxPerMinute_ = 1;
  }
  if (maxPerMinute_ > kWindow) {
    maxPerMinute_ = static_cast<uint16_t>(kWindow);
  }
  minIntervalMs_ = minIntervalMs;
}

bool TxQueue::rateAllows(uint32_t nowMs) const {
  uint32_t inWindow = 0;
  for (uint8_t i = 0; i < sendCount_; ++i) {
    if (static_cast<uint32_t>(nowMs - sendTimes_[i]) < kRateWindowMs) {
      ++inWindow;
    }
  }
  // Pending texts count against the limit too: accepting more than the window
  // allows would just accumulate frames that cannot be transmitted in time.
  return (inWindow + static_cast<uint32_t>(count_)) < maxPerMinute_;
}

bool TxQueue::push(const char* text, size_t length, uint32_t nowMs) {
  if (text == nullptr || length == 0 || length > kMaxTextBytes) {
    return false;
  }
  if (!rateAllows(nowMs)) {
    ++stats_.droppedRate;
    return false;
  }
  if (count_ >= kCapacity) {
    ++stats_.droppedFull;
    return false;
  }

  Entry& entry = entries_[tail_];
  std::memcpy(entry.text, text, length);
  entry.text[length] = '\0';
  entry.length = static_cast<uint8_t>(length);
  tail_ = static_cast<uint8_t>((tail_ + 1) % kCapacity);
  ++count_;
  ++stats_.accepted;
  return true;
}

bool TxQueue::pop(uint32_t nowMs, char* out, size_t capacity, size_t& length) {
  length = 0;
  if (count_ == 0 || out == nullptr) {
    return false;
  }
  // Signed subtraction keeps the spacing check correct across the millis wrap.
  if (hasSent_ && static_cast<int32_t>(nowMs - nextSendAt_) < 0) {
    return false;
  }

  const Entry& entry = entries_[head_];
  const size_t entryLength = entry.length;
  if (capacity < entryLength + 1) {
    return false;
  }

  std::memcpy(out, entry.text, entryLength);
  out[entryLength] = '\0';
  length = entryLength;
  head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
  --count_;

  nextSendAt_ = nowMs + minIntervalMs_;
  hasSent_ = true;
  recordSend(nowMs);
  ++stats_.sent;
  return true;
}

void TxQueue::recordSend(uint32_t nowMs) {
  sendTimes_[sendIndex_] = nowMs;
  sendIndex_ = static_cast<uint8_t>((sendIndex_ + 1) % kWindow);
  if (sendCount_ < kWindow) {
    ++sendCount_;
  }
}

}  // namespace aprs2mesh

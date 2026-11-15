#include "flow_table.h"

#include <cstring>

namespace aprs2mesh {

namespace {

char toUpper(char c) {
  if (c >= 'a' && c <= 'z') {
    return static_cast<char>(c - 'a' + 'A');
  }
  return c;
}

bool sameCallsign(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    if (toUpper(*left) != toUpper(*right)) {
      return false;
    }
    ++left;
    ++right;
  }
  return *left == '\0' && *right == '\0';
}

}  // namespace

FlowTable::FlowTable() = default;

void FlowTable::configure(uint16_t ttlMinutes, size_t capacity) {
  ttlMs_ = static_cast<uint32_t>(ttlMinutes) * 60000u;
  if (capacity == 0) {
    capacity = 1;
  }
  if (capacity > kMaxCapacity) {
    capacity = kMaxCapacity;
  }
  if (capacity != capacity_) {
    clear();
    capacity_ = capacity;
  }
}

Flow* FlowTable::find(const char* callsign) {
  for (size_t i = 0; i < kMaxCapacity; ++i) {
    if (entries_[i].used && sameCallsign(entries_[i].callsign, callsign)) {
      return &entries_[i];
    }
  }
  return nullptr;
}

const Flow* FlowTable::find(const char* callsign) const {
  for (size_t i = 0; i < kMaxCapacity; ++i) {
    if (entries_[i].used && sameCallsign(entries_[i].callsign, callsign)) {
      return &entries_[i];
    }
  }
  return nullptr;
}

void FlowTable::open(const char* callsign, uint32_t nowMs) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return;
  }

  Flow* existing = find(callsign);
  if (existing != nullptr) {
    existing->lastUplinkAt = nowMs;
    return;
  }

  Flow* slot = nullptr;
  for (size_t i = 0; i < kMaxCapacity && i < capacity_; ++i) {
    if (!entries_[i].used) {
      slot = &entries_[i];
      break;
    }
  }
  if (slot == nullptr) {
    // Table full: evict the flow with the oldest uplink.
    uint32_t oldestAge = 0;
    for (size_t i = 0; i < capacity_; ++i) {
      const uint32_t age = nowMs - entries_[i].lastUplinkAt;
      if (slot == nullptr || age >= oldestAge) {
        slot = &entries_[i];
        oldestAge = age;
      }
    }
    if (slot != nullptr) {
      ++evicted_;
      --count_;
    }
  }

  if (slot == nullptr) {
    return;
  }
  std::memset(slot, 0, sizeof(*slot));
  std::strncpy(slot->callsign, callsign, sizeof(slot->callsign) - 1);
  slot->openedAt = nowMs;
  slot->lastUplinkAt = nowMs;
  slot->used = true;
  ++count_;
}

bool FlowTable::active(const char* callsign, uint32_t nowMs) const {
  if (callsign == nullptr) {
    return false;
  }
  const Flow* flow = find(callsign);
  if (flow == nullptr) {
    return false;
  }
  return static_cast<uint32_t>(nowMs - flow->lastUplinkAt) < ttlMs_;
}

void FlowTable::prune(uint32_t nowMs) {
  for (size_t i = 0; i < kMaxCapacity; ++i) {
    if (entries_[i].used &&
        static_cast<uint32_t>(nowMs - entries_[i].lastUplinkAt) >= ttlMs_) {
      std::memset(&entries_[i], 0, sizeof(entries_[i]));
      --count_;
    }
  }
}

void FlowTable::clear() {
  for (size_t i = 0; i < kMaxCapacity; ++i) {
    std::memset(&entries_[i], 0, sizeof(entries_[i]));
  }
  count_ = 0;
}

}  // namespace aprs2mesh

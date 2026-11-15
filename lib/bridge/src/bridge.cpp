#include "bridge.h"

#include <cstring>

namespace aprs2mesh {

namespace {

// The uplink window keys on what APRSdroid repeats: same source and same
// information field. Building the key from the parsed frame avoids hashing the
// whole TNC2 text twice.
uint64_t uplinkKey(const Ax25Frame& frame) {
  char source[12] = {};
  const size_t sourceLength = formatAddress(frame.source, source, sizeof(source));
  uint64_t key = hashBytes(1469598103934665603ULL, source,
                           sourceLength > 0 ? sourceLength : 0);
  const uint8_t separator = 0xFF;
  key = hashBytes(key, &separator, 1);
  return hashBytes(key, frame.info, frame.infoLength);
}

}  // namespace

Bridge::Bridge(uint32_t nodeNumber, MeshTextPublisher publisher, void* context)
    : nodeNumber_(nodeNumber), publisher_(publisher), context_(context) {}

void Bridge::configure(const BridgeSettings& settings, uint32_t nowMs) {
  (void)nowMs;
  settings_ = settings;
  txQueue_.configure(settings.maxPerMinute, settings.minIntervalMs);
}

bool Bridge::uplink(const uint8_t* ax25Data, size_t length, uint32_t nowMs) {
  Ax25Frame frame;
  if (parseAx25(ax25Data, length, frame) != Ax25Status::Ok) {
    ++counters_.uplinkInvalid;
    return false;
  }

  const uint64_t key = uplinkKey(frame);
  if (uplinkDedupe_.contains(key, nowMs)) {
    ++counters_.uplinkDuplicate;
    return false;
  }

  char tnc2[kMaxTnc2] = {};
  size_t tnc2Length = 0;
  const bool includePath = !settings_.stripPath;
  if (formatTnc2(frame, includePath, tnc2, sizeof(tnc2), tnc2Length) !=
      Ax25Status::Ok) {
    ++counters_.uplinkTooLarge;
    return false;
  }

  char text[kMaxTextBytes + 1] = {};
  const size_t textLength = buildMeshText(tnc2, tnc2Length, text, sizeof(text));
  if (textLength == 0) {
    ++counters_.uplinkTooLarge;
    return false;
  }

  if (!txQueue_.push(text, textLength, nowMs)) {
    ++counters_.uplinkRejected;
    return false;
  }

  // Recorded only after the frame is queued: a frame dropped by the airtime
  // limit is not a duplicate, and the client retry should be able to pass.
  uplinkDedupe_.accept(key, nowMs);
  ++counters_.uplinkAccepted;
  return true;
}

bool Bridge::downlink(const char* text, size_t length, uint32_t fromNode,
                      uint32_t nowMs, uint8_t* ax25, size_t capacity,
                      size_t& ax25Length) {
  (void)nowMs;
  ax25Length = 0;

  if (fromNode == nodeNumber_) {
    ++counters_.ownEcho;
    return false;
  }
  if (text == nullptr || length > kMaxTextBytes) {
    ++counters_.downlinkTooLarge;
    return false;
  }

  size_t payloadLength = 0;
  const char* payload = stripMeshTextPrefix(text, length, payloadLength);
  if (payload == nullptr) {
    ++counters_.downlinkIgnored;
    return false;
  }

  Ax25Frame frame;
  if (parseTnc2(payload, payloadLength, frame) != Ax25Status::Ok) {
    ++counters_.downlinkInvalid;
    return false;
  }

  if (buildAx25(frame, ax25, capacity, ax25Length) != Ax25Status::Ok) {
    ++counters_.downlinkTooLarge;
    return false;
  }

  ++counters_.downlinkDelivered;
  return true;
}

size_t Bridge::drain(uint32_t nowMs, size_t maximum) {
  size_t handled = 0;
  char text[kMaxTextBytes + 1] = {};
  size_t length = 0;

  while (handled < maximum && txQueue_.pop(nowMs, text, sizeof(text), length)) {
    if (publisher_ != nullptr && publisher_(context_, text, length)) {
      ++counters_.published;
    } else {
      ++counters_.publishFailed;
    }
    ++handled;
  }
  return handled;
}

}  // namespace aprs2mesh

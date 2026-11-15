#include "gateway.h"

#include <cstring>

#include "mesh_text.h"

namespace aprs2mesh {

namespace {

constexpr uint64_t kHashSeed = 0xCBF29CE484222325ULL;

}  // namespace

Gateway::Gateway(uint32_t nodeNumber, MonitorEvent monitor, void* context)
    : nodeNumber_(nodeNumber), monitor_(monitor), context_(context) {}

void Gateway::configure(const GatewaySettings& settings, uint32_t) {
  settings_ = settings;
  flows_.configure(settings.flowTtlMinutes, settings.maxFlows);
  uplinkRate_.configure(settings.uplinkPerMinute);
  downlinkQueue_.configure(settings.downlinkPerMinute, settings.minIntervalMs);
}

uint64_t Gateway::uplinkKey(const char* source, const char* payload,
                            size_t length) const {
  const uint64_t key = hashBytes(kHashSeed, payload, length);
  return hashBytes(key, source, std::strlen(source));
}

uint64_t Gateway::downlinkKey(const char* source, const AprsMessage& message) const {
  uint64_t key = hashBytes(kHashSeed, source, std::strlen(source));
  if (message.hasMessageId) {
    key = hashBytes(key, message.messageId, std::strlen(message.messageId));
  }
  return hashBytes(key, message.text, message.textLength);
}

void Gateway::reject(const char* reason, uint32_t nowMs) {
  lastReject_ = reason;
  lastRejectAt_ = nowMs;
}

void Gateway::rememberPacket(const char* line, size_t length, uint32_t nowMs) {
  const size_t copied = length < sizeof(lastPacket_) - 1 ? length : sizeof(lastPacket_) - 1;
  for (size_t i = 0; i < copied; ++i) {
    const unsigned char c = static_cast<unsigned char>(line[i]);
    lastPacket_[i] = (c < 0x20 || c == 0x7F) ? '.' : line[i];
  }
  lastPacket_[copied] = '\0';
  lastPacketAt_ = nowMs;
}

void Gateway::syncExpired(uint32_t before) {
  counters_.uplinkBufferExpired += buffer_.droppedExpired() - before;
}

bool Gateway::onMeshPacket(uint32_t fromNode, uint32_t packetId, const char* text,
                           size_t length, uint32_t nowMs) {
  ++counters_.meshTexts;

  const uint64_t receiveKey =
      (static_cast<uint64_t>(fromNode) << 32) | static_cast<uint64_t>(packetId);
  if (!rxDedupe_.accept(receiveKey, nowMs)) {
    ++counters_.uplinkDuplicate;
    return false;
  }

  if (fromNode == nodeNumber_) {
    ++counters_.uplinkOwnEcho;
    return false;
  }

  if (!hasTextPrefix(text, length)) {
    ++counters_.uplinkIgnored;
    return false;
  }

  size_t payloadLength = 0;
  const char* payload = stripMeshTextPrefix(text, length, payloadLength);
  if (payload == nullptr || payloadLength == 0) {
    ++counters_.uplinkInvalid;
    return false;
  }

  AprsLine line;
  const AprsLineStatus status = parseAprsLine(payload, payloadLength, line);
  if (status != AprsLineStatus::Ok) {
    ++counters_.uplinkInvalid;
    reject(aprsLineStatusText(status), nowMs);
    return false;
  }

  if (!settings_.gateOpen) {
    ++counters_.uplinkGateClosed;
    reject("gate cerrado", nowMs);
    return false;
  }

  if (!uplinkDedupe_.accept(uplinkKey(line.source, payload, payloadLength), nowMs)) {
    ++counters_.uplinkDuplicate;
    reject("duplicado", nowMs);
    return false;
  }

  if (!uplinkRate_.allow(nowMs)) {
    ++counters_.uplinkRateLimited;
    reject("límite de subida", nowMs);
    return false;
  }

  const uint32_t droppedBefore = buffer_.droppedFull();
  if (!buffer_.push(payload, payloadLength, line.source, fromNode, nowMs)) {
    ++counters_.uplinkInvalid;
    return false;
  }
  if (buffer_.droppedFull() > droppedBefore) {
    ++counters_.uplinkBufferFull;
  }

  ++counters_.uplinkAccepted;
  return true;
}

bool Gateway::nextInjection(uint32_t nowMs, char* out, size_t capacity,
                            size_t& length) {
  length = 0;
  if (pending_ || out == nullptr) {
    return false;
  }

  for (;;) {
    const uint32_t expiredBefore = buffer_.droppedExpired();
    UplinkEntry entry;
    if (!buffer_.pop(nowMs, entry)) {
      syncExpired(expiredBefore);
      return false;
    }
    syncExpired(expiredBefore);

    AprsLine line;
    if (parseAprsLine(entry.tnc2, entry.length, line) != AprsLineStatus::Ok) {
      ++counters_.injectFailed;
      continue;
    }

    const size_t built = buildInjectedLine(line, settings_.callsign, out, capacity);
    if (built == 0) {
      ++counters_.injectFailed;
      continue;
    }

    std::memset(pendingSource_, 0, sizeof(pendingSource_));
    std::strncpy(pendingSource_, entry.source, sizeof(pendingSource_) - 1);
    pendingFromNode_ = entry.fromNode;
    std::memcpy(pendingTnc2_, entry.tnc2, entry.length);
    pendingTnc2_[entry.length] = '\0';
    pendingTnc2Length_ = entry.length;
    pending_ = true;
    length = built;
    return true;
  }
}

void Gateway::onInjectionResult(bool sent, uint32_t nowMs) {
  if (!pending_) {
    return;
  }
  if (sent) {
    ++counters_.injected;
    flows_.open(pendingSource_, nowMs);
    if (monitor_ != nullptr) {
      monitor_(context_, pendingTnc2_, pendingTnc2Length_);
    }
  } else {
    ++counters_.injectFailed;
    const uint32_t droppedBefore = buffer_.droppedFull();
    buffer_.push(pendingTnc2_, pendingTnc2Length_, pendingSource_, pendingFromNode_,
                 nowMs);
    if (buffer_.droppedFull() > droppedBefore) {
      ++counters_.uplinkBufferFull;
    }
  }
  pending_ = false;
}

void Gateway::onServerPacket(const char* line, size_t length, uint32_t nowMs) {
  ++counters_.serverPackets;
  rememberPacket(line, length, nowMs);

  AprsLine parsed;
  if (parseAprsLine(line, length, parsed) != AprsLineStatus::Ok) {
    ++counters_.downlinkRejected;
    reject("línea APRS inválida", nowMs);
    return;
  }

  AprsMessage message;
  if (!parseAprsMessage(parsed, message)) {
    ++counters_.downlinkRejected;
    reject("no es un mensaje", nowMs);
    return;
  }

  if (std::strncmp(message.addressee, "BLN", 3) == 0) {
    ++counters_.downlinkRejected;
    reject("boletín", nowMs);
    return;
  }

  if (!flows_.active(message.addressee, nowMs)) {
    ++counters_.downlinkRejected;
    reject("sin flujo vigente", nowMs);
    return;
  }

  if (!downlinkDedupe_.accept(downlinkKey(parsed.source, message), nowMs)) {
    ++counters_.downlinkDuplicate;
    reject("duplicado", nowMs);
    return;
  }

  char rewritten[kUplinkLineMax] = {};
  const size_t rewrittenLength =
      buildDownlinkLine(parsed, rewritten, sizeof(rewritten));
  if (rewrittenLength == 0) {
    ++counters_.downlinkTooLarge;
    reject("paquete demasiado grande", nowMs);
    return;
  }

  char text[kMaxTextBytes + 1] = {};
  const size_t textLength =
      buildMeshText(rewritten, rewrittenLength, text, sizeof(text));
  if (textLength == 0) {
    ++counters_.downlinkTooLarge;
    reject("texto demasiado grande", nowMs);
    return;
  }

  const TxQueueStats before = downlinkQueue_.stats();
  if (!downlinkQueue_.push(text, textLength, nowMs)) {
    const TxQueueStats& after = downlinkQueue_.stats();
    if (after.droppedFull > before.droppedFull) {
      ++counters_.downlinkQueueFull;
      reject("cola de bajada llena", nowMs);
    } else {
      ++counters_.downlinkRateLimited;
      reject("límite de bajada", nowMs);
    }
    return;
  }

  ++counters_.downlinkAdmitted;
}

bool Gateway::popDownlink(uint32_t nowMs, char* out, size_t capacity,
                          size_t& length) {
  if (!downlinkQueue_.pop(nowMs, out, capacity, length)) {
    return false;
  }

  size_t payloadLength = 0;
  const char* payload = stripMeshTextPrefix(out, length, payloadLength);
  if (payload != nullptr && monitor_ != nullptr) {
    monitor_(context_, payload, payloadLength);
  }
  ++counters_.downlinkPublished;
  return true;
}

}  // namespace aprs2mesh

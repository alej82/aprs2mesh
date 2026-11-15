#include "kiss_codec.h"

namespace aprs2mesh {
namespace {

bool needsEscape(uint8_t b) { return b == kFend || b == kFesc; }

size_t escapedLen(uint8_t b) { return needsEscape(b) ? 2u : 1u; }

}  // namespace

KissKind kissClassify(uint8_t portAndType) {
  switch (kissType(portAndType)) {
    case 0:
      return KissKind::Data;
    case 1:  // TXDELAY
    case 2:  // persistence
    case 3:  // slot time
    case 4:  // TXtail
    case 5:  // full duplex
    case 6:  // set hardware
      return KissKind::Command;
    default:
      return KissKind::Unknown;
  }
}

void KissDecoder::reset() {
  len_ = 0;
  escape_ = false;
  corrupt_ = false;
  dropped_ = false;
  ready_ = false;
}

void KissDecoder::endFrame() {
  escape_ = false;
  if (corrupt_ || len_ == 0) {
    dropped_ = corrupt_;
    corrupt_ = false;
    len_ = 0;
    return;
  }
  dropped_ = false;
  ready_ = true;
  // len_ stays as the completed frame length until the next feed().
}

bool KissDecoder::feed(uint8_t byte) {
  if (ready_) {
    ready_ = false;
    len_ = 0;
  }

  if (byte == kFend) {
    const bool hadFrame = (len_ > 0 && !corrupt_);
    endFrame();
    return hadFrame;
  }

  if (len_ >= kKissMaxFrame) {
    corrupt_ = true;
  }

  if (escape_) {
    escape_ = false;
    if (byte == kTfend) {
      byte = kFend;
    } else if (byte == kTfesc) {
      byte = kFesc;
    } else {
      // Invalid escape: the frame is corrupt, resync at the next FEND.
      corrupt_ = true;
      return false;
    }
  } else if (byte == kFesc) {
    escape_ = true;
    return false;
  }

  if (corrupt_ || len_ >= kKissMaxFrame) {
    corrupt_ = true;
    return false;
  }

  buf_[len_++] = byte;
  return false;
}

size_t KissEncoder::encodedSize(uint8_t portAndType, const uint8_t* payload, size_t len) {
  size_t total = 2;  // opening and closing FEND
  total += escapedLen(portAndType);
  for (size_t i = 0; i < len; ++i) {
    total += escapedLen(payload[i]);
  }
  return total;
}

size_t KissEncoder::encode(uint8_t portAndType, const uint8_t* payload, size_t len,
                           uint8_t* out, size_t capacity) {
  if (out == nullptr) {
    return 0;
  }
  const size_t needed = encodedSize(portAndType, payload, len);
  if (capacity < needed) {
    return 0;
  }

  size_t o = 0;
  auto putEscaped = [&](uint8_t b) {
    if (b == kFend) {
      out[o++] = kFesc;
      out[o++] = kTfend;
    } else if (b == kFesc) {
      out[o++] = kFesc;
      out[o++] = kTfesc;
    } else {
      out[o++] = b;
    }
  };

  out[o++] = kFend;
  putEscaped(portAndType);
  for (size_t i = 0; i < len; ++i) {
    putEscaped(payload[i]);
  }
  out[o++] = kFend;
  return o;
}

}  // namespace aprs2mesh

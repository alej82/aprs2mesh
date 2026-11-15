#include "mesh_text.h"

#include <cstring>

namespace aprs2mesh {

bool hasTextPrefix(const char* text, size_t length) {
  if (text == nullptr || length < kTextPrefixLength) {
    return false;
  }
  return std::memcmp(text, kTextPrefix, kTextPrefixLength) == 0;
}

size_t buildMeshText(const char* payload, size_t length, char* out, size_t capacity) {
  if (payload == nullptr || out == nullptr) {
    return 0;
  }
  const size_t total = kTextPrefixLength + length;
  if (total > kMaxTextBytes || total + 1 > capacity) {
    return 0;
  }

  std::memcpy(out, kTextPrefix, kTextPrefixLength);
  if (length > 0) {
    std::memcpy(out + kTextPrefixLength, payload, length);
  }
  out[total] = '\0';
  return total;
}

const char* stripMeshTextPrefix(const char* text, size_t length,
                                size_t& payloadLength) {
  payloadLength = 0;
  if (!hasTextPrefix(text, length)) {
    return nullptr;
  }
  payloadLength = length - kTextPrefixLength;
  return text + kTextPrefixLength;
}

PacketDisposition classifyTextPacket(bool hasDecoded, uint32_t channel,
                                     uint32_t expectedChannel, uint32_t portnum,
                                     size_t payloadLength) {
  if (!hasDecoded) {
    return PacketDisposition::NotDecoded;
  }
  if (channel != expectedChannel) {
    return PacketDisposition::WrongChannel;
  }
  if (portnum != kPortTextMessage) {
    return PacketDisposition::NotText;
  }
  if (payloadLength == 0) {
    return PacketDisposition::EmptyText;
  }
  return PacketDisposition::Accept;
}

const char* packetDispositionText(PacketDisposition disposition) {
  switch (disposition) {
    case PacketDisposition::Accept:
      return "accept";
    case PacketDisposition::NotDecoded:
      return "not decoded";
    case PacketDisposition::WrongChannel:
      return "wrong channel";
    case PacketDisposition::NotText:
      return "not text";
    case PacketDisposition::EmptyText:
      return "empty text";
  }
  return "unknown";
}

}  // namespace aprs2mesh

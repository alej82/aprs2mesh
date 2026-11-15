#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh_proto.h"

namespace aprs2mesh {

// Shared text contract between the tracker and the APRS-IS gateway. It is a
// constant on purpose: a configurable prefix would let both firmwares drift
// apart and fail in silence.
inline constexpr char kTextPrefix[] = "A#";
inline constexpr size_t kTextPrefixLength = sizeof(kTextPrefix) - 1;

// True when the text starts with the APRS prefix.
bool hasTextPrefix(const char* text, size_t length);

// Builds "A#" + payload. Returns the number of bytes written without the
// terminator, or 0 when the result does not fit the message size or `capacity`.
size_t buildMeshText(const char* payload, size_t length, char* out, size_t capacity);

// Returns a pointer inside `text` just past the prefix and sets
// `payloadLength`. Returns nullptr when the prefix is absent.
const char* stripMeshTextPrefix(const char* text, size_t length, size_t& payloadLength);

// Why a channel packet was not handed to the APRS side. The mesh radio layer
// uses this to drop silently instead of decoding garbage.
enum class PacketDisposition : uint8_t {
  Accept = 0,
  NotDecoded,
  WrongChannel,
  NotText,
  EmptyText,
};

PacketDisposition classifyTextPacket(bool hasDecoded, uint32_t channel,
                                     uint32_t expectedChannel, uint32_t portnum,
                                     size_t payloadLength);

const char* packetDispositionText(PacketDisposition disposition);

}  // namespace aprs2mesh

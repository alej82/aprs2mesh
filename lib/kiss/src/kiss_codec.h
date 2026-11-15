#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// KISS special bytes (KISS protocol, Phil Karn).
inline constexpr uint8_t kFend = 0xC0;  // frame delimiter
inline constexpr uint8_t kFesc = 0xDB;  // escape
inline constexpr uint8_t kTfend = 0xDC; // escaped FEND
inline constexpr uint8_t kTfesc = 0xDD; // escaped FESC

// Largest frame we accept. AX.25 APRS frames are far below this; the limit
// exists to bound memory and to reject garbage streams early.
inline constexpr size_t kKissMaxFrame = 512;

enum class KissKind : uint8_t {
  Data,    // type 0: payload is an AX.25 frame
  Command, // types 1..6: modem parameters, consumed silently
  Unknown, // anything else: ignored
};

// First byte of a KISS frame packs port (high nibble) and type (low nibble).
inline uint8_t kissPort(uint8_t portAndType) { return portAndType >> 4; }
inline uint8_t kissType(uint8_t portAndType) { return portAndType & 0x0F; }

KissKind kissClassify(uint8_t portAndType);

// Streaming decoder. Feed bytes as they arrive; when feed() returns true a
// complete frame (including the port/type byte) is available through frame()
// and frameLen(). The caller must consume it before feeding more bytes.
class KissDecoder {
 public:
  void reset();

  bool feed(uint8_t byte);

  const uint8_t* frame() const { return buf_; }
  size_t frameLen() const { return len_; }
  bool lastFrameDropped() const { return dropped_; }

 private:
  void endFrame();

  uint8_t buf_[kKissMaxFrame] = {};
  size_t len_ = 0;
  bool escape_ = false;
  bool corrupt_ = false;
  bool dropped_ = false;
  bool ready_ = false;
};

// Encodes one frame: FEND + escaped(type byte, payload) + FEND.
class KissEncoder {
 public:
  static size_t encodedSize(uint8_t portAndType, const uint8_t* payload, size_t len);

  // Returns bytes written, or 0 if the output buffer is too small.
  static size_t encode(uint8_t portAndType, const uint8_t* payload, size_t len,
                       uint8_t* out, size_t capacity);
};

}  // namespace aprs2mesh

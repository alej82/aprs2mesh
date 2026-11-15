#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

enum class Ax25Status : uint8_t {
  Ok = 0,
  ShortFrame,
  BadAddress,
  BadControl,
  TooManyDigipeaters,
  InfoTooLong,
  BadTnc2,
  BadCallsign,
  BufferTooSmall,
};

const char* ax25StatusText(Ax25Status status);

inline constexpr size_t kMaxDigipeaters = 8;
inline constexpr size_t kMaxInfo = 256;
inline constexpr size_t kMaxCallsign = 6;

// AX.25 address without the SSID suffix. Calls are upper case.
struct Ax25Address {
  char call[kMaxCallsign + 1] = {};
  uint8_t ssid = 0;
};

struct Ax25Frame {
  Ax25Address destination;
  Ax25Address source;
  Ax25Address digipeaters[kMaxDigipeaters];
  uint8_t digipeaterCount = 0;
  uint8_t info[kMaxInfo] = {};
  uint16_t infoLength = 0;
};

// "CALL-SSID", with "-SSID" omitted when the SSID is zero.
size_t formatAddress(const Ax25Address& address, char* out, size_t capacity);
bool parseAddress(const char* text, size_t length, Ax25Address& out);

// AX.25 UI frame (control 0x03, PID 0xF0) -> structured frame.
Ax25Status parseAx25(const uint8_t* data, size_t length, Ax25Frame& out);

// Structured frame -> AX.25 UI frame.
Ax25Status buildAx25(const Ax25Frame& frame, uint8_t* out, size_t capacity,
                     size_t& written);

// Structured frame -> "SRC>DST,PATH:info". The path can be omitted to save
// airtime on the mesh.
Ax25Status formatTnc2(const Ax25Frame& frame, bool includePath, char* out,
                      size_t capacity, size_t& written);

// "SRC>DST,PATH:info" -> structured frame.
Ax25Status parseTnc2(const char* text, size_t length, Ax25Frame& out);

// Drops the digipeater path from a TNC2 string: "A>B,WIDE1-1:x" -> "A>B:x".
Ax25Status stripTnc2Path(const char* text, size_t length, char* out,
                         size_t capacity, size_t& written);

}  // namespace aprs2mesh

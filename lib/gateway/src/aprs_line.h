#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// "CALL-15" plus the terminating NUL.
inline constexpr size_t kAprsAddress = 10;
inline constexpr size_t kAprsMaxPath = 120;
inline constexpr size_t kAprsMaxInfo = 256;
inline constexpr size_t kAprsMaxDigipeaters = 8;
inline constexpr size_t kAprsMaxMessageText = 96;

enum class AprsLineStatus : uint8_t {
  Ok = 0,
  Empty,
  MissingHeader,   // no '>' or empty source
  MissingInfo,     // no ':' or empty information field
  BadSource,
  BadDestination,
  BadPath,
  TooLong,
};

const char* aprsLineStatusText(AprsLineStatus status);

// "SRC>DST[,PATH]:info" split into its parts. The path keeps its original
// text (without the leading comma) because the injection must preserve it.
struct AprsLine {
  char source[kAprsAddress] = {};
  char destination[kAprsAddress] = {};
  char path[kAprsMaxPath] = {};
  char info[kAprsMaxInfo + 1] = {};
  uint16_t infoLength = 0;
  bool hasPath = false;
};

AprsLineStatus parseAprsLine(const char* text, size_t length, AprsLine& out);

// One address: 1..6 alphanumeric characters, optional -SSID (0..15) and an
// optional trailing '*' meaning "already repeated".
bool parseAprsCall(const char* text, size_t length, char* out, size_t capacity,
                   bool& repeated);

// Same syntax but for path items: APRS-IS server constructs such as T2SPAIN
// are longer than an AX.25 callsign.
bool parsePathItem(const char* text, size_t length, char* out, size_t capacity,
                   bool& repeated);

// "SRC>DST,PATH,qAR,GATEWAY:info" (no path when the frame had none).
size_t buildInjectedLine(const AprsLine& line, const char* gatewayCallsign,
                         char* out, size_t capacity);

// "SRC>DST,TCPIP*:info", the form published to the mesh for downlinks.
size_t buildDownlinkLine(const AprsLine& line, char* out, size_t capacity);

struct AprsMessage {
  char addressee[kAprsAddress] = {};
  char text[kAprsMaxMessageText + 1] = {};
  size_t textLength = 0;
  char messageId[6] = {};
  bool hasMessageId = false;
};

// True only for the APRS message format ":ADDRESSEE:text{id". Beacons, objects,
// bulletins and third-party traffic are rejected by construction.
bool parseAprsMessage(const AprsLine& line, AprsMessage& out);

}  // namespace aprs2mesh

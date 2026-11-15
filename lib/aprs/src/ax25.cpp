#include "ax25.h"

#include <cstring>

namespace aprs2mesh {
namespace {

constexpr uint8_t kControlUi = 0x03;
constexpr uint8_t kPidNoLayer3 = 0xF0;
constexpr uint8_t kAddressReservedBits = 0x60;

bool isUpperAlnum(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

char toUpper(char c) {
  if (c >= 'a' && c <= 'z') {
    return static_cast<char>(c - 'a' + 'A');
  }
  return c;
}

bool validCall(const Ax25Address& address) {
  const size_t len = strnlen(address.call, kMaxCallsign + 1);
  if (len == 0 || len > kMaxCallsign) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    if (!isUpperAlnum(static_cast<uint8_t>(toUpper(address.call[i])))) {
      return false;
    }
  }
  return address.ssid <= 15;
}

bool readAddressField(const uint8_t* data, Ax25Address& out, bool& last) {
  char call[kMaxCallsign + 1] = {};
  bool padded = false;
  size_t used = 0;

  for (size_t i = 0; i < kMaxCallsign; ++i) {
    const uint8_t raw = static_cast<uint8_t>(data[i] >> 1);
    if (raw == ' ') {
      padded = true;
      continue;
    }
    if (padded || !isUpperAlnum(raw)) {
      return false;
    }
    call[used++] = static_cast<char>(raw);
  }
  if (used == 0 || used > kMaxCallsign) {
    return false;
  }

  const uint8_t ssidByte = data[6];
  out = Ax25Address{};
  std::memcpy(out.call, call, sizeof(call));
  out.ssid = static_cast<uint8_t>((ssidByte >> 1) & 0x0F);
  last = (ssidByte & 0x01U) != 0;
  return true;
}

bool writeAddressField(uint8_t* out, const Ax25Address& address, bool last) {
  if (!validCall(address)) {
    return false;
  }
  const size_t callLen = strnlen(address.call, kMaxCallsign);
  for (size_t i = 0; i < kMaxCallsign; ++i) {
    const char c = (i < callLen) ? toUpper(address.call[i]) : ' ';
    out[i] = static_cast<uint8_t>(static_cast<uint8_t>(c) << 1);
  }
  const uint8_t ssid = static_cast<uint8_t>(address.ssid & 0x0F);
  out[6] = static_cast<uint8_t>(kAddressReservedBits | (ssid << 1) | (last ? 1 : 0));
  return true;
}

}  // namespace

const char* ax25StatusText(Ax25Status status) {
  switch (status) {
    case Ax25Status::Ok:
      return "ok";
    case Ax25Status::ShortFrame:
      return "short frame";
    case Ax25Status::BadAddress:
      return "bad address";
    case Ax25Status::BadControl:
      return "bad control or pid";
    case Ax25Status::TooManyDigipeaters:
      return "too many digipeaters";
    case Ax25Status::InfoTooLong:
      return "information field too long";
    case Ax25Status::BadTnc2:
      return "bad tnc2 text";
    case Ax25Status::BadCallsign:
      return "bad callsign";
    case Ax25Status::BufferTooSmall:
      return "buffer too small";
  }
  return "unknown";
}

size_t formatAddress(const Ax25Address& address, char* out, size_t capacity) {
  size_t pos = 0;
  for (const char* p = address.call; *p != '\0' && pos < capacity; ++p) {
    out[pos++] = toUpper(*p);
  }
  if (address.ssid != 0) {
    char digits[3] = {};
    size_t n = 0;
    uint8_t value = address.ssid;
    digits[n++] = static_cast<char>('0' + (value % 10));
    value = static_cast<uint8_t>(value / 10);
    if (value != 0) {
      digits[n++] = static_cast<char>('0' + (value % 10));
    }
    if (pos + 1 < capacity) {
      out[pos++] = '-';
    }
    while (n > 0 && pos < capacity) {
      out[pos++] = digits[--n];
    }
  }
  return pos;
}

bool parseAddress(const char* text, size_t length, Ax25Address& out) {
  if (text == nullptr || length == 0 || length > kMaxCallsign + 3) {
    return false;
  }

  size_t dash = length;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] == '-') {
      dash = i;
      break;
    }
  }

  const size_t callLen = dash;
  if (callLen == 0 || callLen > kMaxCallsign) {
    return false;
  }

  Ax25Address parsed;
  for (size_t i = 0; i < callLen; ++i) {
    const char c = toUpper(text[i]);
    if (!isUpperAlnum(static_cast<uint8_t>(c))) {
      return false;
    }
    parsed.call[i] = c;
  }

  if (dash < length) {
    const size_t ssidLen = length - dash - 1;
    if (ssidLen == 0 || ssidLen > 2) {
      return false;
    }
    uint8_t ssid = 0;
    for (size_t i = dash + 1; i < length; ++i) {
      if (text[i] < '0' || text[i] > '9') {
        return false;
      }
      ssid = static_cast<uint8_t>(ssid * 10 + static_cast<uint8_t>(text[i] - '0'));
    }
    if (ssid > 15) {
      return false;
    }
    parsed.ssid = ssid;
  }

  out = parsed;
  return true;
}

Ax25Status parseAx25(const uint8_t* data, size_t length, Ax25Frame& out) {
  if (data == nullptr || length < 14 + 2) {
    return Ax25Status::ShortFrame;
  }

  Ax25Frame frame;
  size_t offset = 0;
  bool last = false;

  if (!readAddressField(data + offset, frame.destination, last)) {
    return Ax25Status::BadAddress;
  }
  offset += 7;

  if (last) {
    // A destination in last position means there is no source: not APRS.
    return Ax25Status::BadAddress;
  }

  if (!readAddressField(data + offset, frame.source, last)) {
    return Ax25Status::BadAddress;
  }
  offset += 7;

  while (!last) {
    if (frame.digipeaterCount >= kMaxDigipeaters) {
      return Ax25Status::TooManyDigipeaters;
    }
    if (offset + 7 + 2 > length) {
      return Ax25Status::ShortFrame;
    }
    if (!readAddressField(data + offset, frame.digipeaters[frame.digipeaterCount], last)) {
      return Ax25Status::BadAddress;
    }
    offset += 7;
    ++frame.digipeaterCount;
  }

  if (offset + 2 > length) {
    return Ax25Status::ShortFrame;
  }
  if (data[offset] != kControlUi || data[offset + 1] != kPidNoLayer3) {
    return Ax25Status::BadControl;
  }
  offset += 2;

  const size_t infoLength = length - offset;
  if (infoLength > kMaxInfo) {
    return Ax25Status::InfoTooLong;
  }
  if (infoLength > 0) {
    std::memcpy(frame.info, data + offset, infoLength);
  }
  frame.infoLength = static_cast<uint16_t>(infoLength);

  out = frame;
  return Ax25Status::Ok;
}

Ax25Status buildAx25(const Ax25Frame& frame, uint8_t* out, size_t capacity,
                     size_t& written) {
  if (out == nullptr) {
    return Ax25Status::BufferTooSmall;
  }
  if (frame.digipeaterCount > kMaxDigipeaters) {
    return Ax25Status::TooManyDigipeaters;
  }
  if (frame.infoLength > kMaxInfo) {
    return Ax25Status::InfoTooLong;
  }
  if (!validCall(frame.destination) || !validCall(frame.source)) {
    return Ax25Status::BadCallsign;
  }
  for (uint8_t i = 0; i < frame.digipeaterCount; ++i) {
    if (!validCall(frame.digipeaters[i])) {
      return Ax25Status::BadCallsign;
    }
  }

  const size_t needed = (2 + frame.digipeaterCount) * 7 + 2 + frame.infoLength;
  if (capacity < needed) {
    return Ax25Status::BufferTooSmall;
  }

  size_t offset = 0;
  if (!writeAddressField(out + offset, frame.destination, false)) {
    return Ax25Status::BadCallsign;
  }
  offset += 7;

  const bool sourceIsLast = frame.digipeaterCount == 0;
  if (!writeAddressField(out + offset, frame.source, sourceIsLast)) {
    return Ax25Status::BadCallsign;
  }
  offset += 7;

  for (uint8_t i = 0; i < frame.digipeaterCount; ++i) {
    const bool last = (i + 1) == frame.digipeaterCount;
    if (!writeAddressField(out + offset, frame.digipeaters[i], last)) {
      return Ax25Status::BadCallsign;
    }
    offset += 7;
  }

  out[offset++] = kControlUi;
  out[offset++] = kPidNoLayer3;
  if (frame.infoLength > 0) {
    std::memcpy(out + offset, frame.info, frame.infoLength);
    offset += frame.infoLength;
  }

  written = offset;
  return Ax25Status::Ok;
}

Ax25Status formatTnc2(const Ax25Frame& frame, bool includePath, char* out,
                      size_t capacity, size_t& written) {
  if (out == nullptr || capacity == 0) {
    return Ax25Status::BufferTooSmall;
  }

  size_t pos = 0;
  auto put = [&](char c) -> bool {
    if (pos + 1 >= capacity) {
      return false;
    }
    out[pos++] = c;
    return true;
  };

  auto putAddress = [&](const Ax25Address& address) -> bool {
    char text[12] = {};
    const size_t len = formatAddress(address, text, sizeof(text));
    for (size_t i = 0; i < len; ++i) {
      if (!put(text[i])) {
        return false;
      }
    }
    return true;
  };

  if (!putAddress(frame.source) || !put('>') || !putAddress(frame.destination)) {
    return Ax25Status::BufferTooSmall;
  }

  if (includePath) {
    for (uint8_t i = 0; i < frame.digipeaterCount; ++i) {
      if (!put(',') || !putAddress(frame.digipeaters[i])) {
        return Ax25Status::BufferTooSmall;
      }
    }
  }

  if (!put(':')) {
    return Ax25Status::BufferTooSmall;
  }

  for (size_t i = 0; i < frame.infoLength; ++i) {
    if (!put(static_cast<char>(frame.info[i]))) {
      return Ax25Status::BufferTooSmall;
    }
  }

  out[pos] = '\0';
  written = pos;
  return Ax25Status::Ok;
}

Ax25Status parseTnc2(const char* text, size_t length, Ax25Frame& out) {
  if (text == nullptr || length == 0) {
    return Ax25Status::BadTnc2;
  }

  size_t colon = length;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] == ':') {
      colon = i;
      break;
    }
  }
  if (colon == length) {
    return Ax25Status::BadTnc2;
  }

  size_t gt = colon;
  for (size_t i = 0; i < colon; ++i) {
    if (text[i] == '>') {
      gt = i;
      break;
    }
  }
  if (gt == colon) {
    return Ax25Status::BadTnc2;
  }

  Ax25Frame frame;
  if (!parseAddress(text, gt, frame.source)) {
    return Ax25Status::BadCallsign;
  }

  size_t segmentStart = gt + 1;
  bool destinationDone = false;
  while (segmentStart <= colon) {
    size_t segmentEnd = segmentStart;
    while (segmentEnd < colon && text[segmentEnd] != ',') {
      ++segmentEnd;
    }

    size_t effectiveEnd = segmentEnd;
    if (effectiveEnd > segmentStart && text[effectiveEnd - 1] == '*') {
      --effectiveEnd;  // has-been-repeated marker
    }

    if (!destinationDone) {
      if (!parseAddress(text + segmentStart, effectiveEnd - segmentStart,
                        frame.destination)) {
        return Ax25Status::BadCallsign;
      }
      destinationDone = true;
    } else {
      if (effectiveEnd == segmentStart) {
        // Empty path segment, ignore.
      } else if (frame.digipeaterCount >= kMaxDigipeaters) {
        return Ax25Status::TooManyDigipeaters;
      } else if (!parseAddress(text + segmentStart, effectiveEnd - segmentStart,
                               frame.digipeaters[frame.digipeaterCount])) {
        return Ax25Status::BadCallsign;
      } else {
        ++frame.digipeaterCount;
      }
    }

    if (segmentEnd >= colon) {
      break;
    }
    segmentStart = segmentEnd + 1;
  }

  if (!destinationDone) {
    return Ax25Status::BadTnc2;
  }

  const size_t infoLength = length - colon - 1;
  if (infoLength > kMaxInfo) {
    return Ax25Status::InfoTooLong;
  }
  if (infoLength > 0) {
    std::memcpy(frame.info, text + colon + 1, infoLength);
  }
  frame.infoLength = static_cast<uint16_t>(infoLength);

  out = frame;
  return Ax25Status::Ok;
}

Ax25Status stripTnc2Path(const char* text, size_t length, char* out,
                         size_t capacity, size_t& written) {
  Ax25Frame frame;
  const Ax25Status status = parseTnc2(text, length, frame);
  if (status != Ax25Status::Ok) {
    return status;
  }
  return formatTnc2(frame, /*includePath=*/false, out, capacity, written);
}

}  // namespace aprs2mesh

#include "aprs_line.h"

#include <cstring>

namespace aprs2mesh {

namespace {

constexpr size_t kMaxLine = 400;

bool isAlnum(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char toUpper(char c) {
  if (c >= 'a' && c <= 'z') {
    return static_cast<char>(c - 'a' + 'A');
  }
  return c;
}

bool parseAddress(const char* text, size_t length, char* out, size_t capacity,
                  bool& repeated, size_t maxBase, bool uppercase) {
  repeated = false;
  if (text == nullptr || length == 0) {
    return false;
  }
  if (text[length - 1] == '*') {
    repeated = true;
    --length;
  }
  if (length == 0) {
    return false;
  }

  size_t dash = length;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] == '-') {
      dash = i;
      break;
    }
  }

  const size_t baseLength = dash;
  if (baseLength == 0 || baseLength > maxBase) {
    return false;
  }

  char buffer[16] = {};
  if (baseLength + 1 > sizeof(buffer)) {
    return false;
  }
  for (size_t i = 0; i < baseLength; ++i) {
    if (!isAlnum(text[i])) {
      return false;
    }
    buffer[i] = uppercase ? toUpper(text[i]) : text[i];
  }

  uint8_t ssid = 0;
  if (dash < length) {
    const size_t digits = length - dash - 1;
    if (digits == 0 || digits > 2) {
      return false;
    }
    for (size_t i = dash + 1; i < length; ++i) {
      if (text[i] < '0' || text[i] > '9') {
        return false;
      }
      ssid = static_cast<uint8_t>(ssid * 10 + static_cast<uint8_t>(text[i] - '0'));
    }
    if (ssid > 15) {
      return false;
    }
    buffer[baseLength] = '-';
    size_t written = baseLength + 1;
    if (ssid >= 10) {
      buffer[written++] = static_cast<char>('0' + ssid / 10);
      buffer[written++] = static_cast<char>('0' + ssid % 10);
    } else {
      buffer[written++] = static_cast<char>('0' + ssid);
    }
    buffer[written] = '\0';
  }

  const size_t writtenLength = std::strlen(buffer);
  if (writtenLength + 1 > capacity) {
    return false;
  }
  std::memcpy(out, buffer, writtenLength + 1);
  return true;
}

bool append(char* out, size_t capacity, size_t& length, const char* text,
            size_t textLength) {
  if (length + textLength + 1 > capacity) {
    return false;
  }
  if (textLength > 0) {
    std::memcpy(out + length, text, textLength);
  }
  length += textLength;
  out[length] = '\0';
  return true;
}

}  // namespace

const char* aprsLineStatusText(AprsLineStatus status) {
  switch (status) {
    case AprsLineStatus::Ok:
      return "ok";
    case AprsLineStatus::Empty:
      return "línea vacía";
    case AprsLineStatus::MissingHeader:
      return "sin cabecera";
    case AprsLineStatus::MissingInfo:
      return "sin información";
    case AprsLineStatus::BadSource:
      return "origen inválido";
    case AprsLineStatus::BadDestination:
      return "destino inválido";
    case AprsLineStatus::BadPath:
      return "path inválido";
    case AprsLineStatus::TooLong:
      return "línea demasiado larga";
  }
  return "?";
}

bool parseAprsCall(const char* text, size_t length, char* out, size_t capacity,
                   bool& repeated) {
  return parseAddress(text, length, out, capacity, repeated, 6, true);
}

bool parsePathItem(const char* text, size_t length, char* out, size_t capacity,
                   bool& repeated) {
  // Path items keep their case: APRS-IS constructs such as qAC are written in
  // lower case and the injection must preserve the received path.
  return parseAddress(text, length, out, capacity, repeated, 9, false);
}

AprsLineStatus parseAprsLine(const char* text, size_t length, AprsLine& out) {
  if (text == nullptr || length == 0) {
    return AprsLineStatus::Empty;
  }
  if (length > kMaxLine) {
    return AprsLineStatus::TooLong;
  }

  size_t header = 0;
  while (header < length && text[header] != '>') {
    ++header;
  }
  if (header == 0 || header >= length) {
    return AprsLineStatus::MissingHeader;
  }

  size_t info = header + 1;
  while (info < length && text[info] != ':') {
    ++info;
  }
  if (info >= length) {
    return AprsLineStatus::MissingInfo;
  }
  if (info + 1 >= length) {
    return AprsLineStatus::MissingInfo;
  }

  AprsLine parsed;
  bool repeated = false;
  if (!parseAprsCall(text, header, parsed.source, sizeof(parsed.source), repeated) ||
      repeated) {
    return AprsLineStatus::BadSource;
  }

  const size_t addresses = header + 1;
  size_t comma = addresses;
  while (comma < info && text[comma] != ',') {
    ++comma;
  }
  if (comma == addresses) {
    return AprsLineStatus::BadDestination;
  }
  if (!parseAprsCall(text + addresses, comma - addresses, parsed.destination,
                     sizeof(parsed.destination), repeated) ||
      repeated) {
    return AprsLineStatus::BadDestination;
  }

  size_t cursor = comma < info ? comma + 1 : info;
  size_t digipeaters = 0;
  size_t pathLength = 0;
  while (cursor < info) {
    size_t end = cursor;
    while (end < info && text[end] != ',') {
      ++end;
    }
    if (end == cursor || digipeaters >= kAprsMaxDigipeaters) {
      return AprsLineStatus::BadPath;
    }
    char item[kAprsAddress] = {};
    if (!parsePathItem(text + cursor, end - cursor, item, sizeof(item), repeated)) {
      return AprsLineStatus::BadPath;
    }
    if (pathLength > 0 && !append(parsed.path, sizeof(parsed.path), pathLength, ",", 1)) {
      return AprsLineStatus::BadPath;
    }
    if (!append(parsed.path, sizeof(parsed.path), pathLength, item, std::strlen(item))) {
      return AprsLineStatus::BadPath;
    }
    if (repeated &&
        !append(parsed.path, sizeof(parsed.path), pathLength, "*", 1)) {
      return AprsLineStatus::BadPath;
    }
    parsed.hasPath = true;
    ++digipeaters;
    cursor = end + 1;
  }

  const size_t infoLength = length - info - 1;
  if (infoLength > kAprsMaxInfo) {
    return AprsLineStatus::TooLong;
  }
  std::memcpy(parsed.info, text + info + 1, infoLength);
  parsed.info[infoLength] = '\0';
  parsed.infoLength = static_cast<uint16_t>(infoLength);

  out = parsed;
  return AprsLineStatus::Ok;
}

size_t buildInjectedLine(const AprsLine& line, const char* gatewayCallsign,
                         char* out, size_t capacity) {
  if (out == nullptr || gatewayCallsign == nullptr) {
    return 0;
  }
  size_t length = 0;
  if (!append(out, capacity, length, line.source, std::strlen(line.source)) ||
      !append(out, capacity, length, ">", 1) ||
      !append(out, capacity, length, line.destination, std::strlen(line.destination))) {
    return 0;
  }
  if (line.hasPath &&
      (!append(out, capacity, length, ",", 1) ||
       !append(out, capacity, length, line.path, std::strlen(line.path)))) {
    return 0;
  }
  if (!append(out, capacity, length, ",qAR,", 5) ||
      !append(out, capacity, length, gatewayCallsign, std::strlen(gatewayCallsign)) ||
      !append(out, capacity, length, ":", 1) ||
      !append(out, capacity, length, line.info, line.infoLength)) {
    return 0;
  }
  return length;
}

size_t buildDownlinkLine(const AprsLine& line, char* out, size_t capacity) {
  if (out == nullptr) {
    return 0;
  }
  size_t length = 0;
  if (!append(out, capacity, length, line.source, std::strlen(line.source)) ||
      !append(out, capacity, length, ">", 1) ||
      !append(out, capacity, length, line.destination, std::strlen(line.destination)) ||
      !append(out, capacity, length, ",TCPIP*:", 8) ||
      !append(out, capacity, length, line.info, line.infoLength)) {
    return 0;
  }
  return length;
}

bool parseAprsMessage(const AprsLine& line, AprsMessage& out) {
  // ":ADDRESSEE:text", where the addressee field is exactly nine characters.
  if (line.infoLength < 12 || line.info[0] != ':' || line.info[10] != ':') {
    return false;
  }

  AprsMessage parsed;
  size_t addresseeLength = 9;
  while (addresseeLength > 0 && line.info[addresseeLength] == ' ') {
    --addresseeLength;
  }
  if (addresseeLength == 0) {
    return false;
  }
  for (size_t i = 0; i < addresseeLength; ++i) {
    if (!isAlnum(line.info[1 + i]) && line.info[1 + i] != '-' && line.info[1 + i] != ' ') {
      return false;
    }
    parsed.addressee[i] = toUpper(line.info[1 + i]);
  }
  parsed.addressee[addresseeLength] = '\0';

  size_t textLength = line.infoLength - 11;
  const char* text = line.info + 11;
  for (size_t i = textLength; i > 0; --i) {
    if (text[i - 1] == '{') {
      const size_t identifierBytes = textLength - i;
      if (identifierBytes == 0) {
        return false;
      }
      size_t identifierLength = 0;
      while (identifierLength < identifierBytes && text[i + identifierLength] != '}') {
        if (!isAlnum(text[i + identifierLength]) || identifierLength >= 5) {
          return false;
        }
        parsed.messageId[identifierLength] = text[i + identifierLength];
        ++identifierLength;
      }
      if (identifierLength == 0) {
        return false;
      }
      parsed.hasMessageId = true;
      textLength = i - 1;
      break;
    }
  }

  if (textLength > kAprsMaxMessageText) {
    return false;
  }
  std::memcpy(parsed.text, text, textLength);
  parsed.text[textLength] = '\0';
  parsed.textLength = textLength;

  out = parsed;
  return true;
}

}  // namespace aprs2mesh

#include "psk_base64.h"

#include <cstring>

namespace aprs2mesh {

namespace {

constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int decodeChar(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

}  // namespace

size_t encodeBase64(const uint8_t* data, size_t length, char* out, size_t capacity) {
  if (out == nullptr || (data == nullptr && length > 0)) {
    return 0;
  }
  const size_t needed = ((length + 2) / 3) * 4;
  if (needed + 1 > capacity) {
    return 0;
  }

  size_t written = 0;
  for (size_t i = 0; i < length; i += 3) {
    const uint32_t first = data[i];
    const uint32_t second = (i + 1 < length) ? data[i + 1] : 0;
    const uint32_t third = (i + 2 < length) ? data[i + 2] : 0;
    const uint32_t triple = (first << 16) | (second << 8) | third;

    out[written++] = kAlphabet[(triple >> 18) & 0x3F];
    out[written++] = kAlphabet[(triple >> 12) & 0x3F];
    out[written++] = (i + 1 < length) ? kAlphabet[(triple >> 6) & 0x3F] : '=';
    out[written++] = (i + 2 < length) ? kAlphabet[triple & 0x3F] : '=';
  }
  out[written] = '\0';
  return written;
}

bool decodeBase64(const char* text, uint8_t* out, size_t capacity, size_t& length) {
  length = 0;
  if (text == nullptr || out == nullptr) {
    return false;
  }

  const size_t inputLength = std::strlen(text);
  if (inputLength == 0) {
    return true;
  }
  if (inputLength % 4 != 0) {
    return false;
  }

  size_t written = 0;
  for (size_t i = 0; i < inputLength; i += 4) {
    int values[4] = {};
    int padding = 0;

    for (size_t j = 0; j < 4; ++j) {
      const char c = text[i + j];
      if (c == '=') {
        // Padding only closes the last group and never covers the first two
        // characters of it.
        if (i + 4 != inputLength || j < 2) {
          return false;
        }
        ++padding;
      } else {
        if (padding > 0) {
          return false;
        }
        const int value = decodeChar(c);
        if (value < 0) {
          return false;
        }
        values[j] = value;
      }
    }
    if (padding == 3) {
      return false;
    }

    const uint32_t triple = (static_cast<uint32_t>(values[0]) << 18) |
                            (static_cast<uint32_t>(values[1]) << 12) |
                            (static_cast<uint32_t>(values[2]) << 6) |
                            static_cast<uint32_t>(values[3]);
    const size_t produced = 3 - static_cast<size_t>(padding);
    if (written + produced > capacity) {
      return false;
    }

    if (padding < 3) {
      out[written++] = static_cast<uint8_t>((triple >> 16) & 0xFFu);
    }
    if (padding < 2) {
      out[written++] = static_cast<uint8_t>((triple >> 8) & 0xFFu);
    }
    if (padding < 1) {
      out[written++] = static_cast<uint8_t>(triple & 0xFFu);
    }
  }

  length = written;
  return true;
}

}  // namespace aprs2mesh

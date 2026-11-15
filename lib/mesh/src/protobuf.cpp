#include "protobuf.h"

#include <cstring>

namespace aprs2mesh::pb {

bool Writer::raw(const uint8_t* data, size_t length) {
  if (!ok_ || length > capacity_ - pos_) {
    ok_ = false;
    return false;
  }
  if (length > 0) {
    std::memcpy(out_ + pos_, data, length);
  }
  pos_ += length;
  return true;
}

bool Writer::rawVarint(uint32_t value) {
  uint8_t buffer[5] = {};
  size_t length = 0;
  while (value > 0x7F) {
    buffer[length++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
    value >>= 7;
  }
  buffer[length++] = static_cast<uint8_t>(value);
  return raw(buffer, length);
}

bool Writer::tag(uint32_t field, uint32_t wire) {
  return rawVarint((field << 3) | wire);
}

bool Writer::varint(uint32_t field, uint32_t value) {
  return tag(field, kWireVarint) && rawVarint(value);
}

bool Writer::fixed32(uint32_t field, uint32_t value) {
  const uint8_t bytes[4] = {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF),
      static_cast<uint8_t>((value >> 16) & 0xFF),
      static_cast<uint8_t>((value >> 24) & 0xFF),
  };
  return tag(field, kWireFixed32) && raw(bytes, sizeof(bytes));
}

bool Writer::bytes(uint32_t field, const uint8_t* data, size_t length) {
  if (!tag(field, kWireLengthDelimited) || !rawVarint(static_cast<uint32_t>(length))) {
    return false;
  }
  return raw(data, length);
}

bool Writer::str(uint32_t field, const char* text) {
  if (text == nullptr) {
    text = "";
  }
  return bytes(field, reinterpret_cast<const uint8_t*>(text), std::strlen(text));
}

bool Reader::next(uint32_t& field, uint32_t& wire) {
  uint32_t key = 0;
  if (!readVarint(key)) {
    return false;
  }
  field = key >> 3;
  wire = key & 0x07;
  return field != 0;
}

bool Reader::readVarint(uint32_t& value) {
  uint32_t result = 0;
  uint32_t shift = 0;
  while (pos_ < length_ && shift < 35) {
    const uint8_t byte = data_[pos_++];
    result |= static_cast<uint32_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) {
      value = result;
      return true;
    }
    shift += 7;
  }
  return false;
}

bool Reader::readFixed32(uint32_t& value) {
  if (length_ - pos_ < 4) {
    return false;
  }
  value = static_cast<uint32_t>(data_[pos_]) |
          (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
          (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
          (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
  pos_ += 4;
  return true;
}

bool Reader::readLengthDelimited(const uint8_t*& data, size_t& length) {
  uint32_t size = 0;
  if (!readVarint(size)) {
    return false;
  }
  if (size > length_ - pos_) {
    return false;
  }
  data = data_ + pos_;
  length = size;
  pos_ += size;
  return true;
}

bool Reader::skip(uint32_t wire) {
  switch (wire) {
    case kWireVarint: {
      uint32_t ignored = 0;
      return readVarint(ignored);
    }
    case kWireFixed64:
      if (length_ - pos_ < 8) {
        return false;
      }
      pos_ += 8;
      return true;
    case kWireLengthDelimited: {
      const uint8_t* ignored = nullptr;
      size_t size = 0;
      return readLengthDelimited(ignored, size);
    }
    case kWireFixed32:
      if (length_ - pos_ < 4) {
        return false;
      }
      pos_ += 4;
      return true;
    default:
      return false;
  }
}

}  // namespace aprs2mesh::pb

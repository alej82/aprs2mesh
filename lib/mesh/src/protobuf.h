#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh::pb {

inline constexpr uint32_t kWireVarint = 0;
inline constexpr uint32_t kWireFixed64 = 1;
inline constexpr uint32_t kWireLengthDelimited = 2;
inline constexpr uint32_t kWireFixed32 = 5;

// Minimal protobuf writer. Every call returns false once the output buffer is
// full; the writer stays failed from then on.
class Writer {
 public:
  Writer(uint8_t* out, size_t capacity) : out_(out), capacity_(capacity) {}

  bool varint(uint32_t field, uint32_t value);
  bool fixed32(uint32_t field, uint32_t value);
  bool bytes(uint32_t field, const uint8_t* data, size_t length);
  bool str(uint32_t field, const char* text);
  bool flag(uint32_t field, bool value) { return varint(field, value ? 1 : 0); }

  size_t size() const { return pos_; }
  bool ok() const { return ok_; }

 private:
  bool tag(uint32_t field, uint32_t wire);
  bool rawVarint(uint32_t value);
  bool raw(const uint8_t* data, size_t length);

  uint8_t* out_;
  size_t capacity_;
  size_t pos_ = 0;
  bool ok_ = true;
};

// Minimal protobuf reader with hard bounds checking.
class Reader {
 public:
  Reader(const uint8_t* data, size_t length) : data_(data), length_(length) {}

  bool next(uint32_t& field, uint32_t& wire);
  bool readVarint(uint32_t& value);
  bool readFixed32(uint32_t& value);
  bool readLengthDelimited(const uint8_t*& data, size_t& length);
  bool skip(uint32_t wire);

  bool eof() const { return pos_ >= length_; }

 private:
  const uint8_t* data_;
  size_t length_;
  size_t pos_ = 0;
};

}  // namespace aprs2mesh::pb

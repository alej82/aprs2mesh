#include "mesh_proto.h"

#include <cstring>

#include "protobuf.h"

namespace aprs2mesh {
namespace {

void copyString(char* destination, size_t capacity, const uint8_t* source, size_t length) {
  if (capacity == 0) {
    return;
  }
  size_t copied = length;
  if (copied > capacity - 1) {
    copied = capacity - 1;
  }
  if (copied > 0) {
    std::memcpy(destination, source, copied);
  }
  destination[copied] = '\0';
}

}  // namespace

size_t encodeData(const DataMessage& data, uint8_t* out, size_t capacity) {
  if (data.payloadLength > kMaxTextBytes) {
    return 0;
  }
  pb::Writer writer(out, capacity);
  writer.varint(/*field=*/1, data.portnum);
  writer.bytes(/*field=*/2, data.payload, data.payloadLength);
  if (!writer.ok()) {
    return 0;
  }
  return writer.size();
}

bool decodeData(const uint8_t* data, size_t length, DataMessage& out) {
  DataMessage decoded;
  pb::Reader reader(data, length);
  while (!reader.eof()) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!reader.next(field, wire)) {
      return false;
    }
    if (field == 1 && wire == pb::kWireVarint) {
      if (!reader.readVarint(decoded.portnum)) {
        return false;
      }
    } else if (field == 2 && wire == pb::kWireLengthDelimited) {
      const uint8_t* payload = nullptr;
      size_t payloadLength = 0;
      if (!reader.readLengthDelimited(payload, payloadLength) || payloadLength > kMaxTextBytes) {
        return false;
      }
      std::memcpy(decoded.payload, payload, payloadLength);
      decoded.payloadLength = payloadLength;
    } else if (!reader.skip(wire)) {
      return false;
    }
  }
  out = decoded;
  return true;
}

size_t encodeMeshPacket(const MeshPacketInfo& packet, uint8_t* out, size_t capacity) {
  uint8_t dataBytes[kMaxTextBytes + 16] = {};
  const size_t dataLength = encodeData(packet.data, dataBytes, sizeof(dataBytes));
  if (dataLength == 0) {
    return 0;
  }

  pb::Writer writer(out, capacity);
  writer.fixed32(/*field=*/1, packet.from);
  writer.fixed32(/*field=*/2, packet.to);
  writer.varint(/*field=*/3, packet.channel);
  writer.bytes(/*field=*/4, dataBytes, dataLength);
  writer.fixed32(/*field=*/6, packet.id);
  writer.varint(/*field=*/9, packet.hopLimit);
  if (packet.wantAck) {
    writer.flag(/*field=*/10, true);
  }
  if (!writer.ok()) {
    return 0;
  }
  return writer.size();
}

bool decodeMeshPacket(const uint8_t* data, size_t length, MeshPacketInfo& out) {
  MeshPacketInfo decoded;
  pb::Reader reader(data, length);
  while (!reader.eof()) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!reader.next(field, wire)) {
      return false;
    }
    switch (field) {
      case 1:
        if (wire != pb::kWireFixed32 || !reader.readFixed32(decoded.from)) {
          return false;
        }
        break;
      case 2:
        if (wire != pb::kWireFixed32 || !reader.readFixed32(decoded.to)) {
          return false;
        }
        break;
      case 3:
        if (wire != pb::kWireVarint || !reader.readVarint(decoded.channel)) {
          return false;
        }
        break;
      case 4: {
        const uint8_t* sub = nullptr;
        size_t subLength = 0;
        if (wire != pb::kWireLengthDelimited ||
            !reader.readLengthDelimited(sub, subLength) ||
            !decodeData(sub, subLength, decoded.data)) {
          return false;
        }
        decoded.hasDecoded = true;
        break;
      }
      case 5:
        if (wire != pb::kWireLengthDelimited) {
          return false;
        }
        decoded.hasDecoded = false;
        decoded.data = DataMessage{};
        if (!reader.skip(wire)) {
          return false;
        }
        break;
      case 6:
        if (wire != pb::kWireFixed32 || !reader.readFixed32(decoded.id)) {
          return false;
        }
        break;
      case 9:
        if (wire != pb::kWireVarint || !reader.readVarint(decoded.hopLimit)) {
          return false;
        }
        break;
      case 10: {
        if (wire != pb::kWireVarint) {
          return false;
        }
        uint32_t value = 0;
        if (!reader.readVarint(value)) {
          return false;
        }
        decoded.wantAck = value != 0;
        break;
      }
      default:
        if (!reader.skip(wire)) {
          return false;
        }
        break;
    }
  }
  out = decoded;
  return true;
}

size_t encodeUser(const UserInfo& user, uint8_t* out, size_t capacity) {
  pb::Writer writer(out, capacity);
  writer.str(/*field=*/1, user.id);
  writer.str(/*field=*/2, user.longName);
  writer.str(/*field=*/3, user.shortName);
  writer.varint(/*field=*/5, user.hwModel);
  if (user.licensed) {
    writer.flag(/*field=*/6, true);
  }
  writer.varint(/*field=*/7, user.role);
  if (!writer.ok()) {
    return 0;
  }
  return writer.size();
}

bool decodeUser(const uint8_t* data, size_t length, UserInfo& out) {
  UserInfo decoded;
  pb::Reader reader(data, length);
  while (!reader.eof()) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!reader.next(field, wire)) {
      return false;
    }
    if (field == 1 || field == 2 || field == 3) {
      const uint8_t* text = nullptr;
      size_t textLength = 0;
      if (wire != pb::kWireLengthDelimited ||
          !reader.readLengthDelimited(text, textLength)) {
        return false;
      }
      if (field == 1) {
        copyString(decoded.id, sizeof(decoded.id), text, textLength);
      } else if (field == 2) {
        copyString(decoded.longName, sizeof(decoded.longName), text, textLength);
      } else {
        copyString(decoded.shortName, sizeof(decoded.shortName), text, textLength);
      }
    } else if (field == 5 && wire == pb::kWireVarint) {
      if (!reader.readVarint(decoded.hwModel)) {
        return false;
      }
    } else if (field == 6 && wire == pb::kWireVarint) {
      uint32_t value = 0;
      if (!reader.readVarint(value)) {
        return false;
      }
      decoded.licensed = value != 0;
    } else if (field == 7 && wire == pb::kWireVarint) {
      if (!reader.readVarint(decoded.role)) {
        return false;
      }
    } else if (!reader.skip(wire)) {
      return false;
    }
  }
  out = decoded;
  return true;
}

}  // namespace aprs2mesh

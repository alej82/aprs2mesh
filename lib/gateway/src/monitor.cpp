#include "monitor.h"

#include "ax25.h"

namespace aprs2mesh {

bool Monitor::frameFor(const char* tnc2, size_t length, uint8_t* ax25,
                       size_t capacity, size_t& ax25Length) {
  ax25Length = 0;
  Ax25Frame frame;
  if (parseTnc2(tnc2, length, frame) != Ax25Status::Ok) {
    ++counters_.invalid;
    return false;
  }
  if (buildAx25(frame, ax25, capacity, ax25Length) != Ax25Status::Ok) {
    ++counters_.invalid;
    return false;
  }
  ++counters_.frames;
  return true;
}

}  // namespace aprs2mesh

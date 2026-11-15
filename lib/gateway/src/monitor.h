#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

struct MonitorCounters {
  uint32_t frames = 0;
  uint32_t invalid = 0;
};

// Converts a TNC2 text into an AX.25 UI frame for a KISS monitor client. This
// is the only place the gateway speaks AX.25: the injection path is pure text.
class Monitor {
 public:
  // False when the text is not a valid TNC2 line; the event is counted.
  bool frameFor(const char* tnc2, size_t length, uint8_t* ax25, size_t capacity,
                size_t& ax25Length);

  const MonitorCounters& counters() const { return counters_; }

 private:
  MonitorCounters counters_;
};

}  // namespace aprs2mesh

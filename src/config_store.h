#pragma once

#include "config.h"

namespace aprs2mesh {

// Persists the whole Config as a single NVS blob. One key means one atomic
// entry for NVS: after a power loss the device reads either the previous blob
// or the new one, never a half-written configuration.
class ConfigStore {
 public:
  // True only when a blob exists, matches the format version and passes
  // validation. A corrupt or older blob is ignored, not repaired.
  bool load(Config& config);
  bool save(const Config& config);
  void erase();
};

}  // namespace aprs2mesh

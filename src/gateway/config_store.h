#pragma once

#include "gateway_config.h"

namespace aprs2mesh {

// Persists the whole GatewayConfig as a single NVS blob: a power loss leaves
// either the previous blob or the new one, never a half-written configuration.
class GatewayConfigStore {
 public:
  // True only when a blob exists, matches the format version and passes
  // validation.
  bool load(GatewayConfig& config);
  bool save(const GatewayConfig& config);
  void erase();
};

}  // namespace aprs2mesh

#include "config_store.h"

#include <Preferences.h>

namespace aprs2mesh {

namespace {

constexpr char kNamespace[] = "aprs2mesh-gw";
constexpr char kKey[] = "config";
constexpr uint32_t kMagic = 0x41324D32u;  // "A2M2"
constexpr uint16_t kFormatVersion = 1;

struct Blob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  GatewayConfig config;
};

}  // namespace

bool GatewayConfigStore::load(GatewayConfig& config) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/true)) {
    return false;
  }

  Blob blob{};
  const size_t stored = preferences.getBytesLength(kKey);
  const bool ok = stored == sizeof(Blob) &&
                  preferences.getBytes(kKey, &blob, sizeof(blob)) == sizeof(blob);
  preferences.end();

  if (!ok || blob.magic != kMagic || blob.version != kFormatVersion ||
      blob.size != sizeof(GatewayConfig)) {
    return false;
  }
  if (validateGatewayConfig(blob.config) != GwField::None) {
    return false;
  }

  config = blob.config;
  return true;
}

bool GatewayConfigStore::save(const GatewayConfig& config) {
  if (validateGatewayConfig(config) != GwField::None) {
    return false;
  }

  Blob blob{};
  blob.magic = kMagic;
  blob.version = kFormatVersion;
  blob.size = static_cast<uint16_t>(sizeof(GatewayConfig));
  blob.config = config;

  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) {
    return false;
  }
  const bool ok =
      preferences.putBytes(kKey, &blob, sizeof(blob)) == sizeof(blob);
  preferences.end();
  return ok;
}

void GatewayConfigStore::erase() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  preferences.remove(kKey);
  preferences.end();
}

}  // namespace aprs2mesh

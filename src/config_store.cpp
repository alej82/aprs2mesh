#include "config_store.h"

#include <Preferences.h>

namespace aprs2mesh {

namespace {

constexpr char kNamespace[] = "aprs2mesh";
constexpr char kKey[] = "config";
constexpr uint32_t kMagic = 0x41324D31u;  // "A2M1"
// Version 2 adds the modem block (CUSTOM frequency/BW/SF/CR) and hop limit.
// No migration is needed: no device has been flashed with version 1 yet.
constexpr uint16_t kFormatVersion = 2;

struct Blob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  Config config;
};

}  // namespace

bool ConfigStore::load(Config& config) {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/true)) {
    return false;
  }

  Blob blob{};
  const size_t stored = preferences.getBytesLength(kKey);
  bool ok = stored == sizeof(Blob) &&
            preferences.getBytes(kKey, &blob, sizeof(blob)) == sizeof(blob);
  preferences.end();

  if (!ok || blob.magic != kMagic || blob.version != kFormatVersion ||
      blob.size != sizeof(Config)) {
    return false;
  }
  if (validateConfig(blob.config) != ConfigField::None) {
    return false;
  }

  config = blob.config;
  return true;
}

bool ConfigStore::save(const Config& config) {
  if (validateConfig(config) != ConfigField::None) {
    return false;
  }

  Blob blob{};
  blob.magic = kMagic;
  blob.version = kFormatVersion;
  blob.size = static_cast<uint16_t>(sizeof(Config));
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

void ConfigStore::erase() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  preferences.remove(kKey);
  preferences.end();
}

}  // namespace aprs2mesh

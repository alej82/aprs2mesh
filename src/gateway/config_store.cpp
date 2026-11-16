#include "config_store.h"

#include <Arduino.h>
#include <Preferences.h>

namespace aprs2mesh {

namespace {

constexpr char kNamespace[] = "aprs2mesh-gw";
constexpr char kKey[] = "config";
constexpr uint32_t kMagic = 0x41324D32u;  // "A2M2"
// Version 2 adds the shared modem block (CUSTOM frequency/BW/SF/CR) and hop
// limit. No migration: no gateway has been flashed with version 1 yet.
constexpr uint16_t kFormatVersion = 2;

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

  if (!ok) {
    Serial.printf("gw config: read failed (stored=%u expected=%u)\n",
                  static_cast<unsigned>(stored),
                  static_cast<unsigned>(sizeof(Blob)));
    return false;
  }
  if (blob.magic != kMagic || blob.version != kFormatVersion ||
      blob.size != sizeof(GatewayConfig)) {
    Serial.printf("gw config: header mismatch magic=%08x version=%u size=%u\n",
                  static_cast<unsigned>(blob.magic),
                  static_cast<unsigned>(blob.version),
                  static_cast<unsigned>(blob.size));
    return false;
  }
  const GwField field = validateGatewayConfig(blob.config);
  if (field != GwField::None) {
    Serial.printf("gw config: invalid field %s\n", gwFieldText(field));
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
    Serial.println("gw config: NVS open for write failed");
    return false;
  }
  const size_t written = preferences.putBytes(kKey, &blob, sizeof(blob));
  preferences.end();
  Serial.printf("gw config: wrote %u/%u bytes\n", static_cast<unsigned>(written),
                static_cast<unsigned>(sizeof(blob)));
  return written == sizeof(blob);
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

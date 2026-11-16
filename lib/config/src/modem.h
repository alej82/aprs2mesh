#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// Internal on-device codes. They are not the Meshtastic protobuf enum values:
// resolveModem() maps a fixed preset to its parameters, and the CUSTOM fields
// map to Meshtastic's use_preset = false + bandwidth/spread_factor/coding_rate
// plus override_frequency.
enum class RegionCode : uint8_t { Eu868 = 0, Us915, Count };
enum class ModemPresetCode : uint8_t {
  LongFast = 0,
  MediumFast,
  ShortFast,
  Custom,
  Count
};
enum class BandwidthCode : uint8_t {
  Bw31_25 = 0,
  Bw41_67,
  Bw62_5,
  Bw125,
  Bw250,
  Bw500,
  Count
};
enum class CodingRateCode : uint8_t { Cr4_5 = 0, Cr4_6, Cr4_7, Cr4_8, Count };

inline constexpr uint8_t kModemMinSpreadFactor = 7;
inline constexpr uint8_t kModemMaxSpreadFactor = 12;
inline constexpr uint8_t kModemMinCodingRate = 5;  // Meshtastic 5..8 = 4/5..4/8
inline constexpr uint8_t kModemMaxCodingRate = 8;
inline constexpr uint8_t kModemDefaultHopLimit = 3;
inline constexpr uint8_t kModemMaxHopLimit = 7;
inline constexpr uint16_t kModemDefaultPreambleSymbols = 16;
// Text payload used for the portal estimate; the mesh header and protobuf add
// a few bytes on air.
inline constexpr size_t kModemAirtimePayloadBytes = 200;

struct ModemConfig {
  ModemPresetCode preset = ModemPresetCode::LongFast;
  // Custom mode only. 0 means the operator never configured a custom value, so
  // the portal can show the defaults requested by the product spec.
  uint32_t frequencyHz = 0;
  BandwidthCode bandwidth = BandwidthCode::Bw250;
  uint8_t spreadFactor = 11;
  CodingRateCode codingRate = CodingRateCode::Cr4_5;
  uint8_t hopLimit = kModemDefaultHopLimit;
};

struct RegionBand {
  uint32_t startHz = 0;
  uint32_t endHz = 0;
  uint8_t dutyCyclePercent = 0;
};

struct ModemParams {
  uint32_t frequencyHz = 0;
  uint32_t bandwidthHz = 0;
  BandwidthCode bandwidth = BandwidthCode::Bw250;
  uint8_t spreadFactor = 0;
  uint8_t codingRate = kModemMinCodingRate;
  CodingRateCode codingRateCode = CodingRateCode::Cr4_5;
  uint8_t hopLimit = kModemDefaultHopLimit;
};

enum class ModemField : uint8_t {
  None = 0,
  Preset,
  Frequency,
  Bandwidth,
  SpreadFactor,
  CodingRate,
  HopLimit,
};

bool regionBand(RegionCode region, RegionBand& out);
bool presetParams(ModemPresetCode preset, BandwidthCode& bandwidth,
                  uint8_t& spreadFactor, CodingRateCode& codingRate);

uint32_t bandwidthHz(BandwidthCode bandwidth);
const char* bandwidthName(BandwidthCode bandwidth);
const char* codingRateName(CodingRateCode codingRate);
const char* modemPresetName(ModemPresetCode preset);

// Validates the modem block. Custom-only fields are ignored for fixed presets.
ModemField validateModemConfig(const ModemConfig& modem, RegionCode region);

// Resolves the frequency and modulation actually used on air. For fixed
// presets the frequency follows Meshtastic's channel-slot calculation from the
// region band and the channel name; for CUSTOM it is taken from the config.
bool resolveModem(const ModemConfig& modem, RegionCode region,
                  const char* channelName, ModemParams& out);

// djb2 hash used by Meshtastic to choose the frequency slot.
uint32_t channelSlotHash(const char* channelName);

// Estimated LoRa airtime for a payload, including preamble, header and CRC.
// Returns 0 for parameters outside the supported ranges.
uint32_t estimateAirtimeMs(const ModemParams& params, size_t payloadBytes,
                           uint16_t preambleSymbols = kModemDefaultPreambleSymbols);

// Floor of the packets per minute that fit in the region duty cycle. Zero
// means "less than one per minute"; the caller can show that explicitly.
uint16_t suggestedMaxPerMinute(const ModemParams& params,
                               uint8_t dutyCyclePercent,
                               size_t payloadBytes);

}  // namespace aprs2mesh

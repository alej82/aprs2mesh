#include "modem.h"

#include <cmath>

namespace aprs2mesh {

namespace {

uint8_t codingRateNumber(CodingRateCode codingRate) {
  return static_cast<uint8_t>(static_cast<uint8_t>(codingRate) +
                              kModemMinCodingRate);
}

}  // namespace

bool regionBand(RegionCode region, RegionBand& out) {
  switch (region) {
    case RegionCode::Eu868:
      out.startHz = 869400000u;
      out.endHz = 869650000u;
      out.dutyCyclePercent = 10;
      return true;
    case RegionCode::Us915:
      out.startHz = 902000000u;
      out.endHz = 928000000u;
      out.dutyCyclePercent = 100;
      return true;
    case RegionCode::Count:
      break;
  }
  return false;
}

bool presetParams(ModemPresetCode preset, BandwidthCode& bandwidth,
                  uint8_t& spreadFactor, CodingRateCode& codingRate) {
  switch (preset) {
    case ModemPresetCode::LongFast:
      bandwidth = BandwidthCode::Bw250;
      spreadFactor = 11;
      codingRate = CodingRateCode::Cr4_5;
      return true;
    case ModemPresetCode::MediumFast:
      bandwidth = BandwidthCode::Bw250;
      spreadFactor = 9;
      codingRate = CodingRateCode::Cr4_5;
      return true;
    case ModemPresetCode::ShortFast:
      bandwidth = BandwidthCode::Bw250;
      spreadFactor = 7;
      codingRate = CodingRateCode::Cr4_5;
      return true;
    case ModemPresetCode::Custom:
    case ModemPresetCode::Count:
      break;
  }
  return false;
}

uint32_t bandwidthHz(BandwidthCode bandwidth) {
  switch (bandwidth) {
    case BandwidthCode::Bw31_25:
      return 31250u;
    case BandwidthCode::Bw41_67:
      return 41670u;
    case BandwidthCode::Bw62_5:
      return 62500u;
    case BandwidthCode::Bw125:
      return 125000u;
    case BandwidthCode::Bw250:
      return 250000u;
    case BandwidthCode::Bw500:
      return 500000u;
    case BandwidthCode::Count:
      break;
  }
  return 0;
}

const char* bandwidthName(BandwidthCode bandwidth) {
  switch (bandwidth) {
    case BandwidthCode::Bw31_25:
      return "31.25";
    case BandwidthCode::Bw41_67:
      return "41.67";
    case BandwidthCode::Bw62_5:
      return "62.5";
    case BandwidthCode::Bw125:
      return "125";
    case BandwidthCode::Bw250:
      return "250";
    case BandwidthCode::Bw500:
      return "500";
    case BandwidthCode::Count:
      break;
  }
  return "?";
}

const char* codingRateName(CodingRateCode codingRate) {
  switch (codingRate) {
    case CodingRateCode::Cr4_5:
      return "4/5";
    case CodingRateCode::Cr4_6:
      return "4/6";
    case CodingRateCode::Cr4_7:
      return "4/7";
    case CodingRateCode::Cr4_8:
      return "4/8";
    case CodingRateCode::Count:
      break;
  }
  return "?";
}

const char* modemPresetName(ModemPresetCode preset) {
  switch (preset) {
    case ModemPresetCode::LongFast:
      return "LONG_FAST";
    case ModemPresetCode::MediumFast:
      return "MEDIUM_FAST";
    case ModemPresetCode::ShortFast:
      return "SHORT_FAST";
    case ModemPresetCode::Custom:
      return "CUSTOM";
    case ModemPresetCode::Count:
      break;
  }
  return "?";
}

ModemField validateModemConfig(const ModemConfig& modem, RegionCode region) {
  if (static_cast<uint8_t>(modem.preset) >=
      static_cast<uint8_t>(ModemPresetCode::Count)) {
    return ModemField::Preset;
  }
  if (modem.hopLimit > kModemMaxHopLimit) {
    return ModemField::HopLimit;
  }

  RegionBand band;
  if (!regionBand(region, band)) {
    return ModemField::Frequency;
  }

  if (modem.preset != ModemPresetCode::Custom) {
    BandwidthCode fixedBandwidth = BandwidthCode::Bw250;
    uint8_t fixedSpreadFactor = 0;
    CodingRateCode fixedCodingRate = CodingRateCode::Cr4_5;
    if (!presetParams(modem.preset, fixedBandwidth, fixedSpreadFactor,
                      fixedCodingRate)) {
      return ModemField::Preset;
    }
    if (bandwidthHz(fixedBandwidth) > band.endHz - band.startHz) {
      return ModemField::Bandwidth;
    }
    return ModemField::None;
  }

  if (static_cast<uint8_t>(modem.bandwidth) >=
      static_cast<uint8_t>(BandwidthCode::Count)) {
    return ModemField::Bandwidth;
  }
  if (modem.spreadFactor < kModemMinSpreadFactor ||
      modem.spreadFactor > kModemMaxSpreadFactor) {
    return ModemField::SpreadFactor;
  }
  if (static_cast<uint8_t>(modem.codingRate) >=
      static_cast<uint8_t>(CodingRateCode::Count)) {
    return ModemField::CodingRate;
  }

  const uint32_t width = bandwidthHz(modem.bandwidth);
  const uint32_t span = band.endHz - band.startHz;
  if (width == 0 || width > span) {
    return ModemField::Bandwidth;
  }
  if (modem.frequencyHz < band.startHz + width / 2 ||
      modem.frequencyHz > band.endHz - width / 2) {
    return ModemField::Frequency;
  }
  return ModemField::None;
}

bool resolveModem(const ModemConfig& modem, RegionCode region,
                  const char* channelName, ModemParams& out) {
  RegionBand band;
  if (!regionBand(region, band)) {
    return false;
  }

  ModemParams params;
  params.hopLimit = modem.hopLimit;

  if (modem.preset == ModemPresetCode::Custom) {
    if (validateModemConfig(modem, region) != ModemField::None) {
      return false;
    }
    params.frequencyHz = modem.frequencyHz;
    params.bandwidth = modem.bandwidth;
    params.bandwidthHz = bandwidthHz(modem.bandwidth);
    params.spreadFactor = modem.spreadFactor;
    params.codingRate = codingRateNumber(modem.codingRate);
    params.codingRateCode = modem.codingRate;
    out = params;
    return true;
  }

  BandwidthCode bandwidth = BandwidthCode::Bw250;
  uint8_t spreadFactor = 0;
  CodingRateCode codingRate = CodingRateCode::Cr4_5;
  if (!presetParams(modem.preset, bandwidth, spreadFactor, codingRate)) {
    return false;
  }

  const uint32_t width = bandwidthHz(bandwidth);
  const uint32_t span = band.endHz - band.startHz;
  if (width == 0 || width > span) {
    return false;
  }
  const uint32_t channels = span / width;
  if (channels == 0) {
    return false;
  }

  const uint32_t slot = channelSlotHash(channelName) % channels;
  params.frequencyHz = band.startHz + width / 2 + slot * width;
  params.bandwidth = bandwidth;
  params.bandwidthHz = width;
  params.spreadFactor = spreadFactor;
  params.codingRate = codingRateNumber(codingRate);
  params.codingRateCode = codingRate;
  out = params;
  return true;
}

uint32_t channelSlotHash(const char* channelName) {
  uint32_t hash = 5381;
  if (channelName == nullptr) {
    return hash;
  }
  for (const char* p = channelName; *p != '\0'; ++p) {
    hash = (hash << 5) + hash + static_cast<uint8_t>(*p);
  }
  return hash;
}

uint32_t estimateAirtimeMs(const ModemParams& params, size_t payloadBytes,
                           uint16_t preambleSymbols) {
  if (params.bandwidthHz == 0 ||
      params.spreadFactor < kModemMinSpreadFactor ||
      params.spreadFactor > kModemMaxSpreadFactor ||
      params.codingRate < kModemMinCodingRate ||
      params.codingRate > kModemMaxCodingRate) {
    return 0;
  }

  const double symbolMs =
      (static_cast<double>(1UL << params.spreadFactor) * 1000.0) /
      static_cast<double>(params.bandwidthHz);
  const bool lowDataRate = symbolMs > 16.0;
  const double numerator = 8.0 * static_cast<double>(payloadBytes) -
                           4.0 * static_cast<double>(params.spreadFactor) +
                           28.0 + 16.0;
  const double denominator =
      4.0 * static_cast<double>(params.spreadFactor - (lowDataRate ? 2 : 0));
  const double payloadSymbols =
      8.0 + std::ceil(numerator / denominator) *
                static_cast<double>(params.codingRate);
  const double totalMs =
      (static_cast<double>(preambleSymbols) + 4.25) * symbolMs +
      payloadSymbols * symbolMs;
  return static_cast<uint32_t>(std::ceil(totalMs));
}

uint16_t suggestedMaxPerMinute(const ModemParams& params,
                               uint8_t dutyCyclePercent,
                               size_t payloadBytes) {
  const uint32_t airtime = estimateAirtimeMs(params, payloadBytes);
  if (airtime == 0 || dutyCyclePercent == 0) {
    return 0;
  }
  const uint64_t budgetMsPerMinute =
      static_cast<uint64_t>(dutyCyclePercent) * 60000u / 100u;
  const uint64_t suggested = budgetMsPerMinute / airtime;
  if (suggested == 0) {
    return 0;
  }
  if (suggested > 65535u) {
    return 65535u;
  }
  return static_cast<uint16_t>(suggested);
}

}  // namespace aprs2mesh

#include "portal.h"

#include <WiFi.h>

#include <cstdio>
#include <cstring>

#include "psk_base64.h"
#include "version.h"

namespace aprs2mesh {

namespace {

String htmlEscape(const char* value) {
  String out;
  if (value == nullptr) {
    return out;
  }
  for (const char* p = value; *p != '\0'; ++p) {
    switch (*p) {
      case '&':
        out += F("&amp;");
        break;
      case '<':
        out += F("&lt;");
        break;
      case '>':
        out += F("&gt;");
        break;
      case '"':
        out += F("&quot;");
        break;
      default:
        out += *p;
        break;
    }
  }
  return out;
}

bool assignText(const String& value, char* out, size_t capacity) {
  if (value.length() >= capacity) {
    return false;
  }
  std::memset(out, 0, capacity);
  if (value.length() > 0) {
    std::memcpy(out, value.c_str(), value.length());
  }
  return true;
}

bool parseNumber(const String& text, long& value) {
  if (text.length() == 0) {
    return false;
  }
  long result = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') {
      return false;
    }
    result = result * 10 + static_cast<long>(c - '0');
    if (result > 1000000L) {
      return false;
    }
  }
  value = result;
  return true;
}

// Frequencies are entered with 1 kHz resolution (three decimals) and stored in
// Hz, matching the tracker portal.
bool parseFrequencyMHz(const String& text, uint32_t& frequencyHz) {
  if (text.length() == 0) {
    return false;
  }

  long whole = 0;
  long fraction = 0;
  size_t fractionDigits = 0;
  bool dot = false;
  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text[i];
    if (c == '.') {
      if (dot) {
        return false;
      }
      dot = true;
      continue;
    }
    if (c < '0' || c > '9') {
      return false;
    }
    if (!dot) {
      whole = whole * 10 + static_cast<long>(c - '0');
      if (whole > 1000) {
        return false;
      }
    } else {
      if (fractionDigits >= 3) {
        return false;
      }
      fraction = fraction * 10 + static_cast<long>(c - '0');
      ++fractionDigits;
    }
  }

  while (fractionDigits < 3) {
    fraction *= 10;
    ++fractionDigits;
  }

  const uint32_t hz = static_cast<uint32_t>(whole) * 1000000u +
                      static_cast<uint32_t>(fraction) * 1000u;
  if (hz == 0) {
    return false;
  }
  frequencyHz = hz;
  return true;
}

String frequencyText(uint32_t frequencyHz) {
  char buffer[16] = {};
  std::snprintf(buffer, sizeof(buffer), "%lu.%03lu",
                static_cast<unsigned long>(frequencyHz / 1000000u),
                static_cast<unsigned long>((frequencyHz / 1000u) % 1000u));
  return String(buffer);
}

ModemConfig customDisplayConfig(const GatewayConfig& active) {
  ModemConfig modem = active.modem;
  if (modem.frequencyHz == 0) {
    ModemParams effective;
    if (resolveModem(active.modem, active.region, active.channelName, effective)) {
      modem.frequencyHz = effective.frequencyHz;
    }
    modem.bandwidth = BandwidthCode::Bw62_5;
    modem.spreadFactor = 7;
    modem.codingRate = CodingRateCode::Cr4_5;
  }
  return modem;
}

String modemFingerprint(const GatewayConfig& active) {
  ModemParams params;
  if (!resolveModem(active.modem, active.region, active.channelName, params)) {
    return String(F("—"));
  }

  String out = frequencyText(params.frequencyHz);
  out += F(" MHz · BW ");
  out += bandwidthName(params.bandwidth);
  out += F(" kHz · SF ");
  out += String(static_cast<unsigned>(params.spreadFactor));
  out += F(" · CR ");
  out += codingRateName(params.codingRateCode);
  out += F(" · saltos ");
  out += String(static_cast<unsigned>(params.hopLimit));
  return out;
}

const char* regionName(RegionCode region) {
  switch (region) {
    case RegionCode::Eu868:
      return "EU868";
    case RegionCode::Us915:
      return "US915";
    case RegionCode::Count:
      break;
  }
  return "?";
}

void appendRow(String& page, const char* label, const String& value) {
  page += F("<tr><td>");
  page += label;
  page += F("</td><td>");
  page += value;
  page += F("</td></tr>");
}

void appendText(String& page, const char* label, const char* name, const char* value,
                const char* hint, size_t maximum) {
  page += F("<label>");
  page += label;
  page += F("</label><input name=\"");
  page += name;
  page += F("\" maxlength=\"");
  page += static_cast<unsigned>(maximum);
  page += F("\" value=\"");
  page += htmlEscape(value);
  page += F("\">");
  if (hint != nullptr) {
    page += F("<div class=\"hint\">");
    page += hint;
    page += F("</div>");
  }
}

void appendNumber(String& page, const char* label, const char* name, uint32_t value,
                  uint32_t minimum, uint32_t maximum) {
  page += F("<label>");
  page += label;
  page += F("</label><input type=\"number\" name=\"");
  page += name;
  page += F("\" min=\"");
  page += static_cast<unsigned>(minimum);
  page += F("\" max=\"");
  page += static_cast<unsigned>(maximum);
  page += F("\" value=\"");
  page += static_cast<unsigned>(value);
  page += F("\">");
}

String flowSummary(Gateway& gateway, uint32_t nowMs) {
  String summary;
  for (size_t i = 0; i < gateway.flows().slotCount(); ++i) {
    const Flow& flow = gateway.flows().at(i);
    if (!flow.used) {
      continue;
    }
    if (summary.length() > 0) {
      summary += F(", ");
    }
    summary += flow.callsign;
    summary += F(" (");
    summary += static_cast<unsigned>((nowMs - flow.lastUplinkAt) / 60000u);
    summary += F(" min)");
  }
  if (summary.length() == 0) {
    summary = "ninguno";
  }
  return summary;
}

}  // namespace

GatewayPortal::GatewayPortal(GatewayConfigStore& store, Gateway& gateway,
                             AprsIsSession& session)
    : store_(store), gateway_(gateway), session_(session) {}

void GatewayPortal::begin(const GatewayConfig& active) {
  active_ = active;
  server_.on("/", HTTP_GET, [this]() { sendPage(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/gate", HTTP_POST, [this]() { handleGate(); });
  server_.begin();
  Serial.println("portal: http://192.168.4.1/");
}

void GatewayPortal::poll() {
  server_.handleClient();
  if (rebootPending_ && static_cast<int32_t>(millis() - rebootAt_) >= 0) {
    Serial.println("portal: restarting with the saved configuration");
    ESP.restart();
  }
}

void GatewayPortal::fail(const String& message) {
  error_ = message;
  sendPage();
}

void GatewayPortal::handleSave() {
  GatewayConfig next = active_;
  error_ = "";

  if (!assignText(server_.arg("callsign"), next.callsign, sizeof(next.callsign)) ||
      !assignText(server_.arg("host"), next.host, sizeof(next.host)) ||
      !assignText(server_.arg("filter"), next.filter, sizeof(next.filter)) ||
      !assignText(server_.arg("wifiSsid"), next.wifiSsid, sizeof(next.wifiSsid)) ||
      !assignText(server_.arg("wifiPassword"), next.wifiPassword,
                  sizeof(next.wifiPassword)) ||
      !assignText(server_.arg("channelName"), next.channelName,
                  sizeof(next.channelName))) {
    fail(F("Algún campo de texto supera la longitud admitida"));
    return;
  }

  if (!validFilterText(next.filter)) {
    fail(F("El filtro no puede contener saltos de línea ni caracteres de control"));
    return;
  }

  const String passcodeText = server_.arg("passcode");
  long passcode = 0;
  if (!parseNumber(passcodeText, passcode) || passcode > 65535L) {
    fail(F("El passcode debe ser un número entre 0 y 32767"));
    return;
  }
  next.passcode = static_cast<uint16_t>(passcode);

  long number = 0;
  if (!parseNumber(server_.arg("port"), number) || number < 1 || number > 65535) {
    fail(F("El puerto APRS-IS debe estar entre 1 y 65535"));
    return;
  }
  next.port = static_cast<uint16_t>(number);

  if (!parseNumber(server_.arg("flowTtlMinutes"), number)) {
    fail(F("El TTL de flujos debe ser numérico"));
    return;
  }
  next.flowTtlMinutes = static_cast<uint16_t>(number > 65535 ? 65535 : number);

  if (!parseNumber(server_.arg("maxFlows"), number)) {
    fail(F("El máximo de flujos debe ser numérico"));
    return;
  }
  next.maxFlows = static_cast<uint8_t>(number > 255 ? 255 : number);

  if (!parseNumber(server_.arg("uplinkPerMinute"), number)) {
    fail(F("El límite de subida debe ser numérico"));
    return;
  }
  next.uplinkPerMinute = static_cast<uint16_t>(number > 65535 ? 65535 : number);

  if (!parseNumber(server_.arg("downlinkPerMinute"), number)) {
    fail(F("El límite de bajada debe ser numérico"));
    return;
  }
  next.downlinkPerMinute = static_cast<uint16_t>(number > 65535 ? 65535 : number);

  if (!parseNumber(server_.arg("minIntervalMs"), number)) {
    fail(F("El intervalo mínimo debe ser numérico"));
    return;
  }
  next.minIntervalMs = static_cast<uint16_t>(number > 65535 ? 65535 : number);

  if (!parseNumber(server_.arg("region"), number) ||
      number < 0 || number > 255) {
    fail(F("Región inválida"));
    return;
  }
  next.region = static_cast<RegionCode>(static_cast<uint8_t>(number));

  if (!parseNumber(server_.arg("preset"), number) || number < 0 || number > 255) {
    fail(F("Preset inválido"));
    return;
  }
  next.modem.preset = static_cast<ModemPresetCode>(static_cast<uint8_t>(number));

  if (!parseNumber(server_.arg("hopLimit"), number) || number < 0 ||
      number > 255) {
    fail(F("Número de saltos inválido"));
    return;
  }
  next.modem.hopLimit = static_cast<uint8_t>(number);

  // Fixed presets preserve the last custom values, exactly like the tracker.
  if (next.modem.preset == ModemPresetCode::Custom) {
    if (!parseFrequencyMHz(server_.arg("frequencyMHz"), next.modem.frequencyHz)) {
      fail(F("Frecuencia inválida"));
      return;
    }
    if (!parseNumber(server_.arg("bandwidth"), number) || number < 0 ||
        number > 255) {
      fail(F("Ancho de banda inválido"));
      return;
    }
    next.modem.bandwidth =
        static_cast<BandwidthCode>(static_cast<uint8_t>(number));
    if (!parseNumber(server_.arg("spreadFactor"), number) || number < 0 ||
        number > 255) {
      fail(F("Factor de dispersión inválido"));
      return;
    }
    next.modem.spreadFactor = static_cast<uint8_t>(number);
    if (!parseNumber(server_.arg("codingRate"), number) || number < 0 ||
        number > 255) {
      fail(F("Tasa de codificación inválida"));
      return;
    }
    next.modem.codingRate =
        static_cast<CodingRateCode>(static_cast<uint8_t>(number));
  }

  std::memset(next.psk, 0, sizeof(next.psk));
  size_t pskLength = 0;
  const String pskText = server_.arg("psk");
  if (pskText.length() > 0 &&
      !decodeBase64(pskText.c_str(), next.psk, sizeof(next.psk), pskLength)) {
    fail(F("La PSK no es base64 válido"));
    return;
  }
  next.pskLength = static_cast<uint8_t>(pskLength);

  next.gateOpen = server_.hasArg("gateOpen");

  const GwField invalid = validateGatewayConfig(next);
  if (invalid != GwField::None) {
    fail(String(F("Campo inválido: ")) + gwFieldText(invalid));
    return;
  }

  if (!store_.save(next)) {
    fail(F("No se pudo guardar la configuración en NVS"));
    return;
  }

  active_ = next;
  rebootPending_ = true;
  rebootAt_ = millis() + 1500;
  sendPage();
}

void GatewayPortal::handleGate() {
  const bool open = server_.arg("open") == "1";
  active_.gateOpen = open;
  // The switch must not wait for a reboot: apply first, persist second.
  gateway_.setGateOpen(open);
  const bool saved = store_.save(active_);
  Serial.printf("gate: %s%s\n", open ? "abierto" : "cerrado",
                saved ? "" : " (no persistido)");

  server_.sendHeader("Location", "/", true);
  server_.send(303, "text/plain", saved ? "ok" : "no persistido");
}

void GatewayPortal::sendPage() {
  const uint32_t nowMs = millis();
  gateway_.pruneFlows(nowMs);

  String page;
  page.reserve(14000);

  page += F(
      "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>aprs2mesh-gw</title><style>"
      "body{font-family:system-ui,sans-serif;margin:0;padding:16px;"
      "background:#121212;color:#eee}h1{font-size:1.4rem;margin:0 0 4px}"
      "h2{font-size:1.1rem;margin:24px 0 8px}"
      "label{display:block;margin:10px 0 4px;font-size:.9rem;color:#bbb}"
      "input,select{width:100%;padding:8px;box-sizing:border-box;"
      "background:#1f1f1f;color:#eee;border:1px solid #444;border-radius:6px}"
      "button{margin-top:16px;width:100%;padding:10px;background:#2e7d32;"
      "color:#fff;border:0;border-radius:6px;font-size:1rem}"
      ".danger{background:#9c2b2b}"
      ".secondary{background:#455a64}"
      ".warn{color:#e0a030}"
      "table{width:100%;border-collapse:collapse;font-size:.9rem}"
      "td{padding:4px 0;border-bottom:1px solid #262626}"
      "td:first-child{color:#aaa}"
      ".hint{font-size:.8rem;color:#888;margin-top:2px}"
      ".err{background:#5c1f1f;padding:10px;border-radius:6px;margin:12px 0}"
      ".ok{background:#1f4d24;padding:10px;border-radius:6px;margin:12px 0}"
      "</style></head><body><h1>aprs2mesh·gateway</h1><div>");
  page += htmlEscape(APRS2MESH_VERSION);
  page += F("</div>");

  if (rebootPending_) {
    page += F("<div class=\"ok\">Configuración guardada. Reiniciando…</div>");
    page += F("</body></html>");
    server_.send(200, "text/html; charset=utf-8", page);
    return;
  }

  if (error_.length() > 0) {
    page += F("<div class=\"err\">");
    page += htmlEscape(error_.c_str());
    page += F("</div>");
  }

  char pskText[64] = {};
  if (active_.pskLength > 0) {
    encodeBase64(active_.psk, active_.pskLength, pskText, sizeof(pskText));
  }

  const ModemConfig custom = customDisplayConfig(active_);
  ModemParams effective;
  const bool effectiveOk =
      resolveModem(active_.modem, active_.region, active_.channelName, effective);
  RegionBand band;
  const bool bandOk = regionBand(active_.region, band);
  uint32_t effectiveAirtime = 0;
  uint16_t suggestedRate = 0;
  if (effectiveOk) {
    effectiveAirtime =
        estimateAirtimeMs(effective, kModemAirtimePayloadBytes);
    if (bandOk) {
      suggestedRate = suggestedMaxPerMinute(
          effective, band.dutyCyclePercent, kModemAirtimePayloadBytes);
    }
  }

  page += F("<h2>Configuración</h2><form method=\"post\" action=\"/save\">");

  appendText(page, "Callsign del gateway", "callsign", active_.callsign,
             "Con SSID propio, por ejemplo -10", sizeof(active_.callsign) - 1);
  appendText(page, "Servidor APRS-IS", "host", active_.host,
             "rotate.aprs2.net", sizeof(active_.host) - 1);
  appendNumber(page, "Puerto APRS-IS", "port", active_.port, 1, 65535);
  appendNumber(page, "Passcode APRS-IS", "passcode", active_.passcode, 0, 32767);
  appendText(page, "Filtro de recepción", "filter", active_.filter,
             "Una sola línea, por ejemplo m/50", sizeof(active_.filter) - 1);

  appendNumber(page, "TTL de flujos (min)", "flowTtlMinutes", active_.flowTtlMinutes,
               1, kGwMaxFlowTtlMinutes);
  appendNumber(page, "Máximo de flujos", "maxFlows", active_.maxFlows, 1, kGwMaxFlows);
  appendNumber(page, "Subidas por minuto", "uplinkPerMinute",
               active_.uplinkPerMinute, 1, kGwMaxPerMinute);
  page += F("<label>Bajadas por minuto (1-60)</label>"
            "<input id=\"downlinkPerMinute\" type=\"number\" "
            "name=\"downlinkPerMinute\" min=\"1\" max=\"60\" value=\"");
  page += static_cast<unsigned>(active_.downlinkPerMinute);
  page += F("\" oninput=\"updateAirtime()\">");
  appendNumber(page, "Intervalo mínimo de bajada (ms)", "minIntervalMs",
               active_.minIntervalMs, 0, kGwMaxIntervalMs);

  page += F("<label><input type=\"checkbox\" name=\"gateOpen\" "
            "style=\"width:auto\"");
  if (active_.gateOpen) {
    page += F(" checked");
  }
  page += F("> Gate abierto al guardar</label>");

  appendText(page, "SSID de WiFi", "wifiSsid", active_.wifiSsid,
             "Vacío para quedarse sólo con el AP", sizeof(active_.wifiSsid) - 1);
  appendText(page, "Clave de WiFi", "wifiPassword", active_.wifiPassword,
             "8 caracteres o más", sizeof(active_.wifiPassword) - 1);
  appendText(page, "Nombre del canal", "channelName", active_.channelName,
             "El mismo que usa el tracker", sizeof(active_.channelName) - 1);

  page += F("<label>Clave del canal (base64, vacío = canal abierto)</label>"
            "<input name=\"psk\" maxlength=\"44\" value=\"");
  page += htmlEscape(pskText);
  page += F("\">");

  page += F("<label>Región</label><select id=\"region\" name=\"region\" "
            "onchange=\"updateAirtime()\">");
  for (uint8_t i = 0; i < static_cast<uint8_t>(RegionCode::Count); ++i) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(i);
    page += F("\"");
    if (static_cast<uint8_t>(active_.region) == i) {
      page += F(" selected");
    }
    page += F(">");
    page += regionName(static_cast<RegionCode>(i));
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label>Preset de módem</label><select id=\"preset\" name=\"preset\" "
            "onchange=\"updateCustom()\">");
  for (uint8_t i = 0; i < static_cast<uint8_t>(ModemPresetCode::Count); ++i) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(i);
    page += F("\"");
    if (static_cast<uint8_t>(active_.modem.preset) == i) {
      page += F(" selected");
    }
    page += F(">");
    page += modemPresetName(static_cast<ModemPresetCode>(i));
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<div id=\"customModem\"><div class=\"hint\">"
            "Sólo se aplica con preset CUSTOM. Debe coincidir con el tracker.</div>");

  page += F("<label>Frecuencia (MHz, precisión 1 kHz)</label>"
            "<input id=\"frequencyMHz\" name=\"frequencyMHz\" type=\"number\" "
            "step=\"0.001\" min=\"0\" max=\"1000\" value=\"");
  page += frequencyText(custom.frequencyHz);
  page += F("\" oninput=\"updateAirtime()\">");

  page += F("<label>Ancho de banda (kHz)</label>"
            "<select id=\"bandwidth\" name=\"bandwidth\" "
            "onchange=\"updateAirtime()\">");
  for (uint8_t i = 0; i < static_cast<uint8_t>(BandwidthCode::Count); ++i) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(i);
    page += F("\"");
    if (static_cast<uint8_t>(custom.bandwidth) == i) {
      page += F(" selected");
    }
    page += F(">");
    page += bandwidthName(static_cast<BandwidthCode>(i));
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label>Factor de dispersión</label>"
            "<select id=\"spreadFactor\" name=\"spreadFactor\" "
            "onchange=\"updateAirtime()\">");
  for (uint8_t sf = kModemMinSpreadFactor; sf <= kModemMaxSpreadFactor; ++sf) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(sf);
    page += F("\"");
    if (custom.spreadFactor == sf) {
      page += F(" selected");
    }
    page += F(">SF ");
    page += static_cast<unsigned>(sf);
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label>Tasa de codificación</label>"
            "<select id=\"codingRate\" name=\"codingRate\" "
            "onchange=\"updateAirtime()\">");
  for (uint8_t i = 0; i < static_cast<uint8_t>(CodingRateCode::Count); ++i) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(i);
    page += F("\"");
    if (static_cast<uint8_t>(custom.codingRate) == i) {
      page += F(" selected");
    }
    page += F(">");
    page += codingRateName(static_cast<CodingRateCode>(i));
    page += F("</option>");
  }
  page += F("</select></div>");

  page += F("<label>Número de saltos (0-7)</label>"
            "<input type=\"number\" name=\"hopLimit\" min=\"0\" max=\"7\" value=\"");
  page += static_cast<unsigned>(active_.modem.hopLimit);
  page += F("\">");

  page += F("<div class=\"hint\" id=\"airtimeHint\">");
  if (effectiveOk) {
    char airtimeText[16] = {};
    std::snprintf(airtimeText, sizeof(airtimeText), "%.2f",
                  static_cast<double>(effectiveAirtime) / 1000.0);
    page += F("Aire estimado: ~");
    page += airtimeText;
    page += F(" s por trama");
    if (suggestedRate > 0) {
      page += F(" · sugerido para bajadas: ");
      page += static_cast<unsigned>(suggestedRate);
      page += F("/min");
    } else {
      page += F(" · menos de 1/min sostenible");
    }
  } else {
    page += F("Aire estimado: —");
  }
  page += F("</div>");
  page += F("<button type=\"button\" id=\"applySuggestion\" class=\"secondary\" "
            "style=\"display:none;width:auto;margin-top:8px\" "
            "onclick=\"applySuggestion()\">Aplicar el límite sugerido</button>");

  page += F("<button type=\"submit\">Guardar y reiniciar</button></form>");

  page += F("<script>"
            "const kCustomPreset=");
  page += static_cast<unsigned>(ModemPresetCode::Custom);
  page += F(";const kRegionDuty=[");
  for (uint8_t i = 0; i < static_cast<uint8_t>(RegionCode::Count); ++i) {
    RegionBand jsBand;
    page += regionBand(static_cast<RegionCode>(i), jsBand)
                ? static_cast<unsigned>(jsBand.dutyCyclePercent)
                : 100u;
    if (i + 1 < static_cast<uint8_t>(RegionCode::Count)) {
      page += F(",");
    }
  }
  page += F("];const kPresetModem=[");
  for (uint8_t i = 0; i < static_cast<uint8_t>(ModemPresetCode::Custom); ++i) {
    BandwidthCode jsBandwidth = BandwidthCode::Bw250;
    uint8_t jsSpreadFactor = 11;
    CodingRateCode jsCodingRate = CodingRateCode::Cr4_5;
    if (presetParams(static_cast<ModemPresetCode>(i), jsBandwidth,
                     jsSpreadFactor, jsCodingRate)) {
      page += F("[");
      page += static_cast<unsigned long>(bandwidthHz(jsBandwidth));
      page += F(",");
      page += static_cast<unsigned>(jsSpreadFactor);
      page += F(",");
      page += static_cast<unsigned>(static_cast<uint8_t>(jsCodingRate) +
                                    kModemMinCodingRate);
      page += F("]");
    }
    if (i + 1 < static_cast<uint8_t>(ModemPresetCode::Custom)) {
      page += F(",");
    }
  }
  page += F("];const kBw=[");
  for (uint8_t i = 0; i < static_cast<uint8_t>(BandwidthCode::Count); ++i) {
    page += static_cast<unsigned long>(
        bandwidthHz(static_cast<BandwidthCode>(i)));
    if (i + 1 < static_cast<uint8_t>(BandwidthCode::Count)) {
      page += F(",");
    }
  }
  page += F("];"
            "function currentModem(){"
            "const p=Number(document.getElementById('preset').value);"
            "if(p===kCustomPreset){return ["
            "kBw[Number(document.getElementById('bandwidth').value)],"
            "Number(document.getElementById('spreadFactor').value),"
            "Number(document.getElementById('codingRate').value)+5];}"
            "const m=kPresetModem[p];return m?m:kPresetModem[0];}"
            "function airtimeMs(bw,sf,cr){"
            "const symbol=(Math.pow(2,sf)*1000)/bw;"
            "const low=symbol>16?1:0;"
            "const num=8*200-4*sf+28+16;"
            "const den=4*(sf-2*low);"
            "const syms=8+Math.ceil(num/den)*cr;"
            "return (16+4.25)*symbol+syms*symbol;}"
            "function updateCustom(){"
            "const isCustom=Number(document.getElementById('preset').value)===kCustomPreset;"
            "document.getElementById('customModem').style.display=isCustom?'block':'none';"
            "updateAirtime();}"
            "function applySuggestion(){"
            "const s=Number(document.getElementById('applySuggestion').dataset.suggested||'0');"
            "if(s>0){document.getElementById('downlinkPerMinute').value=String(s);updateAirtime();}}"
            "function updateAirtime(){"
            "const m=currentModem();"
            "const t=airtimeMs(m[0],m[1],m[2]);"
            "const duty=kRegionDuty[Number(document.getElementById('region').value)]||100;"
            "const suggested=Math.floor((duty*60000)/(100*t));"
            "const hint=document.getElementById('airtimeHint');"
            "const button=document.getElementById('applySuggestion');"
            "let text='Aire estimado: ~'+(t/1000).toFixed(2)+' s por trama';"
            "if(suggested>0){text+=' · sugerido para bajadas: '+suggested+'/min';}"
            "else{text+=' · menos de 1/min sostenible';}"
            "const current=Number(document.getElementById('downlinkPerMinute').value||'0');"
            "if(suggested>0&&current>suggested){text+=' · tu límite actual es más alto';}"
            "hint.textContent=text;"
            "if(suggested>0&&current>suggested){"
            "button.style.display='block';button.dataset.suggested=String(suggested);"
            "}else{button.style.display='none';button.dataset.suggested='0';}}"
            "updateCustom();updateAirtime();"
            "</script>");

  page += F("<h2>Gate</h2><p>Estado actual: ");
  page += gateway_.gateOpen() ? F("abierto") : F("cerrado");
  page += F("</p><form method=\"post\" action=\"/gate\"><input type=\"hidden\" "
            "name=\"open\" value=\"");
  page += gateway_.gateOpen() ? F("0") : F("1");
  page += F("\"><button class=\"danger\" type=\"submit\">");
  page += gateway_.gateOpen() ? F("Cerrar el gate") : F("Abrir el gate");
  page += F("</button></form>");

  const GatewayCounters& c = gateway_.counters();

  page += F("<h2>Estado</h2><table>");
  appendRow(page, "Modo", runtime_.mode);
  if (runtime_.note[0] != '\0') {
    appendRow(page, "Aviso", runtime_.note);
  }
  appendRow(page, "Región / preset",
            String(regionName(active_.region)) + " / " +
                modemPresetName(active_.modem.preset));
  appendRow(page, "Módem", modemFingerprint(active_));
  appendRow(page, "Sesión APRS-IS",
            String(sessionStateText(session_.state())) + " — " +
                session_.lastError());
  appendRow(page, "Reintento en",
            session_.state() == SessionState::Disconnected
                ? String(static_cast<unsigned>(session_.retryDelayMs() / 1000u)) +
                      " s (intento " +
                      String(static_cast<unsigned>(session_.failedAttempts())) + ")"
                : String("—"));
  appendRow(page, "WiFi AP", runtime_.apIp);
  appendRow(page, "WiFi STA",
            String(runtime_.staIp) +
                (runtime_.wifiStatus == WL_CONNECTED ? " (conectada)"
                                                     : " (sin red)"));
  appendRow(page, "Memoria libre",
            String(static_cast<unsigned long>(runtime_.freeHeap)) + " B");
  appendRow(page, "Tiempo encendido",
            String(static_cast<unsigned long>(runtime_.uptimeMs / 1000u)) + " s");
  appendRow(page, "Flujos", flowSummary(gateway_, nowMs));
  appendRow(page, "Último paquete",
            String(gateway_.lastPacket()[0] != '\0' ? gateway_.lastPacket() : "—"));
  appendRow(page, "Subidas (ok/ign/inv/eco/dup)",
            String(static_cast<unsigned long>(c.uplinkAccepted)) + " / " +
                String(static_cast<unsigned long>(c.uplinkIgnored)) + " / " +
                String(static_cast<unsigned long>(c.uplinkInvalid)) + " / " +
                String(static_cast<unsigned long>(c.uplinkOwnEcho)) + " / " +
                String(static_cast<unsigned long>(c.uplinkDuplicate)));
  appendRow(page, "Subidas bloqueadas (gate/tasa/cola)",
            String(static_cast<unsigned long>(c.uplinkGateClosed)) + " / " +
                String(static_cast<unsigned long>(c.uplinkRateLimited)) + " / " +
                String(static_cast<unsigned long>(c.uplinkBufferFull)));
  appendRow(page, "Inyectadas / fallidas",
            String(static_cast<unsigned long>(c.injected)) + " / " +
                String(static_cast<unsigned long>(c.injectFailed)));
  appendRow(page, "Buffer de subida",
            String(static_cast<unsigned>(gateway_.bufferedUplinks())) + " / " +
                String(static_cast<unsigned>(kUplinkCapacity)) + " (" +
                String(static_cast<unsigned long>(c.uplinkBufferExpired)) +
                " expiradas)");
  appendRow(page, "Bajadas (recibidas/admitidas/rechazadas)",
            String(static_cast<unsigned long>(c.serverPackets)) + " / " +
                String(static_cast<unsigned long>(c.downlinkAdmitted)) + " / " +
                String(static_cast<unsigned long>(c.downlinkRejected)));
  appendRow(page, "Bajadas descartadas (dup/tasa/cola/tamaño)",
            String(static_cast<unsigned long>(c.downlinkDuplicate)) + " / " +
                String(static_cast<unsigned long>(c.downlinkRateLimited)) + " / " +
                String(static_cast<unsigned long>(c.downlinkQueueFull)) + " / " +
                String(static_cast<unsigned long>(c.downlinkTooLarge)));
  appendRow(page, "Publicadas en malla",
            String(static_cast<unsigned long>(c.downlinkPublished)) + " (cola " +
                String(static_cast<unsigned>(gateway_.downlinkQueue().size())) +
                ")");
  appendRow(page, "Último rechazo", gateway_.lastReject());
  appendRow(page, "Monitor (clientes/bytes/expulsados)",
            String(static_cast<unsigned>(runtime_.monitorClients)) + " / " +
                String(static_cast<unsigned long>(runtime_.monitorDiscarded)) + " / " +
                String(static_cast<unsigned long>(runtime_.monitorDropped)));
  page += F("</table>");

  page += F("</body></html>");
  server_.send(200, "text/html; charset=utf-8", page);
}

}  // namespace aprs2mesh

#include "portal.h"

#include <cstdio>
#include <cstring>

#include "psk_base64.h"
#include "config_store.h"
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
// Hz, so the resolver never has to deal with floating point round-off.
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

// Values shown in the CUSTOM block. Until CUSTOM is saved for the first time,
// the block shows the product defaults (62.5 kHz / SF7 / CR 4/5) and the
// frequency the link is using right now, so switching to CUSTOM is not a jump
// into an empty form.
ModemConfig customDisplayConfig(const Config& active) {
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

String modemFingerprint(const Config& active) {
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

}  // namespace

Portal::Portal(ConfigStore& store) : store_(store) {}

void Portal::begin(const Config& active) {
  active_ = active;
  server_.on("/", HTTP_GET, [this]() { sendPage(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.begin();
  Serial.println("portal: http://192.168.4.1/");
}

void Portal::poll() {
  server_.handleClient();
  if (rebootPending_ && static_cast<int32_t>(millis() - rebootAt_) >= 0) {
    Serial.println("portal: restarting with the saved configuration");
    ESP.restart();
  }
}

void Portal::setStatus(const PortalStatus& status) { status_ = status; }

void Portal::fail(const String& message) {
  error_ = message;
  sendPage();
}

void Portal::handleSave() {
  Config next = active_;
  error_ = "";

  if (!assignText(server_.arg("callsign"), next.callsign, sizeof(next.callsign)) ||
      !assignText(server_.arg("bluetoothName"), next.bluetoothName,
                  sizeof(next.bluetoothName)) ||
      !assignText(server_.arg("channelName"), next.channelName,
                  sizeof(next.channelName))) {
    fail(F("Campo de texto demasiado largo"));
    return;
  }

  size_t pskLength = 0;
  std::memset(next.psk, 0, sizeof(next.psk));
  const String pskText = server_.arg("psk");
  if (pskText.length() > 0 &&
      !decodeBase64(pskText.c_str(), next.psk, sizeof(next.psk), pskLength)) {
    fail(F("La PSK no es base64 válido"));
    return;
  }
  next.pskLength = static_cast<uint8_t>(pskLength);

  const long region = server_.arg("region").toInt();
  next.region = (region >= 0 && region < static_cast<long>(RegionCode::Count))
                    ? static_cast<RegionCode>(region)
                    : RegionCode::Count;
  const long preset = server_.arg("preset").toInt();
  next.modem.preset =
      (preset >= 0 && preset < static_cast<long>(ModemPresetCode::Count))
          ? static_cast<ModemPresetCode>(preset)
          : ModemPresetCode::Count;

  long number = 0;
  if (!parseNumber(server_.arg("hopLimit"), number) || number < 0 ||
      number > 255) {
    fail(F("Número de saltos inválido"));
    return;
  }
  next.modem.hopLimit = static_cast<uint8_t>(number);

  // Custom fields are only written when CUSTOM is selected. A fixed preset
  // leaves the last custom values untouched, so switching back is lossless.
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

  const long rate = server_.arg("maxPerMinute").toInt();
  if (rate < 0) {
    next.maxPerMinute = 0;
  } else if (rate > 65535) {
    next.maxPerMinute = 65535;
  } else {
    next.maxPerMinute = static_cast<uint16_t>(rate);
  }
  const long interval = server_.arg("minIntervalMs").toInt();
  if (interval < 0) {
    next.minIntervalMs = 0;
  } else if (interval > 65535) {
    next.minIntervalMs = 65535;
  } else {
    next.minIntervalMs = static_cast<uint16_t>(interval);
  }
  next.stripPath = server_.hasArg("stripPath");

  const ConfigField invalid = validateConfig(next);
  if (invalid != ConfigField::None) {
    fail(String(F("Campo inválido: ")) + configFieldText(invalid));
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

void Portal::sendPage() {
  String page;
  page.reserve(12000);

  page += F(
      "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>aprs2mesh</title><style>"
      "body{font-family:system-ui,sans-serif;margin:0;padding:16px;"
      "background:#121212;color:#eee}h1{font-size:1.4rem;margin:0 0 4px}"
      "h2{font-size:1.1rem;margin:24px 0 8px}"
      "label{display:block;margin:10px 0 4px;font-size:.9rem;color:#bbb}"
      "input,select{width:100%;padding:8px;box-sizing:border-box;"
      "background:#1f1f1f;color:#eee;border:1px solid #444;border-radius:6px}"
      "button{margin-top:16px;width:100%;padding:10px;background:#2e7d32;"
      "color:#fff;border:0;border-radius:6px;font-size:1rem}"
      "table{width:100%;border-collapse:collapse;font-size:.9rem}"
      "td{padding:4px 0;border-bottom:1px solid #262626}"
      "td:first-child{color:#aaa}"
      ".hint{font-size:.8rem;color:#888;margin-top:4px}"
      ".warn{color:#e0a030}"
      ".secondary{background:#455a64}"
      ".err{background:#5c1f1f;padding:10px;border-radius:6px;margin:12px 0}"
      ".ok{background:#1f4d24;padding:10px;border-radius:6px;margin:12px 0}"
      "</style></head><body><h1>aprs2mesh</h1><div>");
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

  page += F("<label>Callsign del operador</label>"
            "<input name=\"callsign\" maxlength=\"11\" required value=\"");
  page += htmlEscape(active_.callsign);
  page += F("\">");

  page += F("<label>Nombre Bluetooth</label>"
            "<input name=\"bluetoothName\" maxlength=\"31\" required value=\"");
  page += htmlEscape(active_.bluetoothName);
  page += F("\">");

  page += F("<label>Nombre del canal</label>"
            "<input name=\"channelName\" maxlength=\"15\" required value=\"");
  page += htmlEscape(active_.channelName);
  page += F("\">");

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
            "Sólo se aplica con preset CUSTOM.</div>");

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
      page += F(" · sugerido: ");
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

  page += F("<label>Tramas por minuto (1-60)</label>"
            "<input id=\"maxPerMinute\" type=\"number\" name=\"maxPerMinute\" "
            "min=\"1\" max=\"60\" value=\"");
  page += static_cast<unsigned>(active_.maxPerMinute);
  page += F("\" oninput=\"updateAirtime()\">");

  page += F("<label>Intervalo mínimo entre transmisiones (ms)</label>"
            "<input type=\"number\" name=\"minIntervalMs\" min=\"0\" max=\"60000\" value=\"");
  page += static_cast<unsigned>(active_.minIntervalMs);
  page += F("\">");

  page += F("<label><input type=\"checkbox\" name=\"stripPath\" "
            "style=\"width:auto\"");
  if (active_.stripPath) {
    page += F(" checked");
  }
  page += F("> Recortar el path de digipeaters antes de publicar</label>");

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
            "if(s>0){document.getElementById('maxPerMinute').value=String(s);updateAirtime();}}"
            "function updateAirtime(){"
            "const m=currentModem();"
            "const t=airtimeMs(m[0],m[1],m[2]);"
            "const duty=kRegionDuty[Number(document.getElementById('region').value)]||100;"
            "const suggested=Math.floor((duty*60000)/(100*t));"
            "const hint=document.getElementById('airtimeHint');"
            "const button=document.getElementById('applySuggestion');"
            "let text='Aire estimado: ~'+(t/1000).toFixed(2)+' s por trama';"
            "if(suggested>0){text+=' · sugerido: '+suggested+'/min';}"
            "else{text+=' · menos de 1/min sostenible';}"
            "const current=Number(document.getElementById('maxPerMinute').value||'0');"
            "if(suggested>0&&current>suggested){text+=' · tu límite actual es más alto';}"
            "hint.textContent=text;"
            "if(suggested>0&&current>suggested){"
            "button.style.display='block';button.dataset.suggested=String(suggested);"
            "}else{button.style.display='none';button.dataset.suggested='0';}}"
            "updateCustom();updateAirtime();"
            "</script>");

  const BridgeCounters& c = status_.counters;
  const TxQueueStats& t = status_.txStats;

  page += F("<h2>Estado</h2><table>");
  appendRow(page, "Modo", status_.mode);
  appendRow(page, "Callsign", active_.callsign);
  appendRow(page, "Canal", active_.channelName);
  appendRow(page, "Región / preset",
            String(regionName(active_.region)) + " / " +
                modemPresetName(active_.modem.preset));
  appendRow(page, "Módem", modemFingerprint(active_));
  appendRow(page, "PSK", String(static_cast<unsigned>(active_.pskLength)) + " bytes");
  appendRow(page, "Memoria libre", String(static_cast<unsigned long>(status_.freeHeap)) + " B");
  appendRow(page, "Clientes KISS", String(static_cast<unsigned>(status_.kissClients)));
  appendRow(page, "Subidas (ok/dup/inv/grande/limite)",
            String(static_cast<unsigned long>(c.uplinkAccepted)) + " / " +
                String(static_cast<unsigned long>(c.uplinkDuplicate)) + " / " +
                String(static_cast<unsigned long>(c.uplinkInvalid)) + " / " +
                String(static_cast<unsigned long>(c.uplinkTooLarge)) + " / " +
                String(static_cast<unsigned long>(c.uplinkRejected)));
  appendRow(page, "Bajadas (ok/ign/inv/grande/eco)",
            String(static_cast<unsigned long>(c.downlinkDelivered)) + " / " +
                String(static_cast<unsigned long>(c.downlinkIgnored)) + " / " +
                String(static_cast<unsigned long>(c.downlinkInvalid)) + " / " +
                String(static_cast<unsigned long>(c.downlinkTooLarge)) + " / " +
                String(static_cast<unsigned long>(c.ownEcho)));
  appendRow(page, "Publicadas en malla",
            String(static_cast<unsigned long>(c.published)) + " ok / " +
                String(static_cast<unsigned long>(c.publishFailed)) + " fallidas");
  appendRow(page, "Cola de aire",
            String(static_cast<unsigned>(status_.queueSize)) + " / " +
                String(static_cast<unsigned>(TxQueue::kCapacity)) + " (enviadas " +
                String(static_cast<unsigned long>(t.sent)) + ")");
  appendRow(page, "Descartes de aire",
            String(static_cast<unsigned long>(t.droppedRate)) + " por tasa / " +
                String(static_cast<unsigned long>(t.droppedFull)) + " por cola llena");
  appendRow(page, "Último error", status_.lastError);
  page += F("</table>");

  page += F("</body></html>");
  server_.send(200, "text/html; charset=utf-8", page);
}

}  // namespace aprs2mesh

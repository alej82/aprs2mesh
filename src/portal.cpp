#include "portal.h"

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

const char* presetName(ModemPresetCode preset) {
  switch (preset) {
    case ModemPresetCode::LongFast:
      return "LONG_FAST";
    case ModemPresetCode::MediumFast:
      return "MEDIUM_FAST";
    case ModemPresetCode::ShortFast:
      return "SHORT_FAST";
    case ModemPresetCode::Count:
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
  next.preset = (preset >= 0 && preset < static_cast<long>(ModemPresetCode::Count))
                    ? static_cast<ModemPresetCode>(preset)
                    : ModemPresetCode::Count;

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
  page.reserve(6000);

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

  page += F("<label>Región</label><select name=\"region\">");
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

  page += F("<label>Preset de módem</label><select name=\"preset\">");
  for (uint8_t i = 0; i < static_cast<uint8_t>(ModemPresetCode::Count); ++i) {
    page += F("<option value=\"");
    page += static_cast<unsigned>(i);
    page += F("\"");
    if (static_cast<uint8_t>(active_.preset) == i) {
      page += F(" selected");
    }
    page += F(">");
    page += presetName(static_cast<ModemPresetCode>(i));
    page += F("</option>");
  }
  page += F("</select>");

  page += F("<label>Tramas por minuto (1-60)</label>"
            "<input type=\"number\" name=\"maxPerMinute\" min=\"1\" max=\"60\" value=\"");
  page += static_cast<unsigned>(active_.maxPerMinute);
  page += F("\">");

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

  const BridgeCounters& c = status_.counters;
  const TxQueueStats& t = status_.txStats;

  page += F("<h2>Estado</h2><table>");
  appendRow(page, "Modo", status_.mode);
  appendRow(page, "Callsign", active_.callsign);
  appendRow(page, "Canal", active_.channelName);
  appendRow(page, "Región / preset",
            String(regionName(active_.region)) + " / " + presetName(active_.preset));
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

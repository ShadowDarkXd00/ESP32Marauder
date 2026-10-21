#include "ShadowWebUI.h"

#if defined(SHADOW_CLIENT_ESP32)

#include <WiFi.h>
#include <esp_system.h>
#include "Display.h"

extern Display display_obj;

namespace {
const char SHADOW_WEB_PAGE[] PROGMEM = R"rawliteral(
<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#101820"><title>Shadow Client</title>
<style>
:root{color-scheme:dark;--bg:#101820;--panel:#182630;--line:#29404d;--cyan:#44e2cf;--text:#ecf4f7;--muted:#a8bac4}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px system-ui,sans-serif}
main{max-width:520px;margin:0 auto;padding:26px 18px}.brand{font-size:12px;letter-spacing:.22em;color:var(--cyan);font-weight:800}
h1{margin:8px 0 4px;font-size:28px}.sub{color:var(--muted);margin:0 0 22px}.card{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:18px;margin:14px 0}
label{display:block;font-weight:650;margin-bottom:9px}textarea{width:100%;min-height:110px;resize:vertical;background:#0d171e;border:1px solid var(--line);border-radius:10px;color:var(--text);padding:12px;font:inherit}
.row{display:flex;gap:10px;margin-top:12px}button{flex:1;border:0;border-radius:10px;padding:12px 14px;font:700 15px system-ui;cursor:pointer;background:var(--cyan);color:#08201f}button.secondary{background:#263b47;color:var(--text)}
#status{font-size:14px;color:var(--muted);line-height:1.55}.notice{font-size:12px;color:var(--muted);margin-top:15px}
</style></head><body><main><div class="brand">SHADOW CLIENT</div><h1>Display console</h1>
<p class="sub">Local screen controls · no Wi-Fi attack controls</p><section class="card"><label for="message">Text to show on the TFT</label>
<textarea id="message" maxlength="60" placeholder="Type up to 60 characters"></textarea><div class="row">
<button id="show">Show on display</button><button class="secondary" id="clear">Clear</button></div></section>
<section class="card"><label>Device status</label><div id="status">Connecting…</div><div class="row"><button class="secondary" id="refresh">Refresh status</button></div></section>
<p class="notice">This page is served by your ESP32 over its private access point. Keep the AP password private.</p>
<script>
const statusBox=document.getElementById('status');
async function refresh(){try{const r=await fetch('/api/status',{cache:'no-store'});const s=await r.json();statusBox.textContent=`${s.name} · ${s.ip} · ${s.clients} connected`;}catch(e){statusBox.textContent='Device is not reachable.';}}
document.getElementById('show').onclick=async()=>{const text=document.getElementById('message').value;const body=new URLSearchParams({text});const r=await fetch('/api/display',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!r.ok)alert('Text was not accepted. Use printable characters and stay within 60 characters.');else refresh();};
document.getElementById('clear').onclick=async()=>{await fetch('/api/clear',{method:'POST'});document.getElementById('message').value='';refresh();};
document.getElementById('refresh').onclick=refresh;refresh();setInterval(refresh,5000);
</script></main></body></html>
)rawliteral";

String makePassword() {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  String password;
  password.reserve(12);
  for (uint8_t i = 0; i < 12; ++i) {
    password += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  }
  return password;
}
}  // namespace

ShadowWebUI shadow_web_ui;

void ShadowWebUI::start() {
  if (active_) {
    Serial.println(F("Shadow Client web display is already running."));
    Serial.print(F("Open http://"));
    Serial.println(WiFi.softAPIP());
    return;
  }

  const uint32_t chip_id = static_cast<uint32_t>(ESP.getEfuseMac());
  char ap_name[32];
  snprintf(ap_name, sizeof(ap_name), "ShadowClient-%04X", static_cast<unsigned>(chip_id & 0xFFFF));
  ssid_ = ap_name;
  password_ = makePassword();

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  if (!WiFi.softAP(ssid_.c_str(), password_.c_str())) {
    WiFi.mode(WIFI_STA);
    password_ = "";
    Serial.println(F("Could not start the Shadow Client access point."));
    return;
  }

  server_.on("/", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, "text/html; charset=utf-8", SHADOW_WEB_PAGE);
  });
  server_.on("/api/status", HTTP_GET, [this]() {
    String payload = "{\"name\":\"Shadow Client\",\"ip\":\"";
    payload += WiFi.softAPIP().toString();
    payload += "\",\"clients\":";
    payload += String(WiFi.softAPgetStationNum());
    payload += "}";
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", payload);
  });
  server_.on("/api/display", HTTP_POST, [this]() {
    if (!server_.hasArg("text")) {
      server_.send(400, "text/plain", "Missing text");
      return;
    }
    String text = server_.arg("text");
    if (text.length() > 60) {
      server_.send(413, "text/plain", "Text is too long");
      return;
    }
    for (size_t i = 0; i < text.length(); ++i) {
      const uint8_t c = static_cast<uint8_t>(text[i]);
      if (c < 32 || c > 126) {
        server_.send(400, "text/plain", "Only printable ASCII is allowed");
        return;
      }
    }
    drawText(text);
    server_.send(200, "application/json", "{\"ok\":true}");
  });
  server_.on("/api/clear", HTTP_POST, [this]() {
    drawReady();
    server_.send(200, "application/json", "{\"ok\":true}");
  });
  server_.onNotFound([this]() {
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "");
  });
  server_.begin();
  active_ = true;
  drawReady();

  Serial.println(F("Shadow Client display web controls are ready."));
  Serial.print(F("Wi-Fi network: "));
  Serial.println(ssid_);
  Serial.print(F("Wi-Fi password: "));
  Serial.println(password_);
  Serial.print(F("Open: http://"));
  Serial.println(WiFi.softAPIP());
}

void ShadowWebUI::stop() {
  if (!active_) return;
  server_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  active_ = false;
  password_ = "";
  display_obj.clearScreen();
  Serial.println(F("Shadow Client web display controls stopped."));
}

void ShadowWebUI::loop() {
  if (active_) server_.handleClient();
}

void ShadowWebUI::drawReady() {
  display_obj.tft.fillScreen(TFT_BLACK);
  display_obj.tft.setTextWrap(false, false);
  display_obj.tft.setTextColor(TFT_CYAN, TFT_BLACK);
  display_obj.tft.drawCentreString("SHADOW CLIENT", 80, 14, 2);
  display_obj.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  display_obj.tft.drawCentreString("Display console", 80, 42, 2);
  display_obj.tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display_obj.tft.drawCentreString(WiFi.softAPIP().toString(), 80, 70, 2);
  display_obj.tft.drawCentreString("Web controls ready", 80, 96, 1);
}

void ShadowWebUI::drawText(const String& text) {
  display_obj.tft.fillScreen(TFT_BLACK);
  display_obj.tft.setTextWrap(false, false);
  display_obj.tft.setTextColor(TFT_CYAN, TFT_BLACK);
  display_obj.tft.drawCentreString("SHADOW CLIENT", 80, 6, 2);
  display_obj.tft.drawFastHLine(4, 29, 152, TFT_DARKGREY);
  display_obj.tft.setTextColor(TFT_WHITE, TFT_BLACK);
  const uint8_t chars_per_line = 12;
  uint8_t line = 0;
  for (size_t offset = 0; offset < text.length() && line < 5; offset += chars_per_line, ++line) {
    display_obj.tft.drawString(text.substring(offset, offset + chars_per_line), 4, 36 + line * 17, 2);
  }
  if (text.length() == 0) {
    display_obj.tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display_obj.tft.drawCentreString("Type text in the", 80, 53, 1);
    display_obj.tft.drawCentreString("web display console", 80, 68, 1);
  }
}

void shadowWebStart() { shadow_web_ui.start(); }
void shadowWebStop() { shadow_web_ui.stop(); }
void shadowWebLoop() { shadow_web_ui.loop(); }
bool shadowWebDisplayActive() { return shadow_web_ui.active(); }

#endif

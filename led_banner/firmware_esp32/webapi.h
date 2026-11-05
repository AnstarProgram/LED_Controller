// ============================================================================
//  webapi.h — Wi-Fi сервер ленты:
//   GET  /                мини-редактор (страница, любой браузер/ПК/телефон)
//   POST /api/playlist    [{t,e,s},...] -> сохранить и перезапустить эфир
//   GET  /api/playlist    текущий плейлист JSON
//   GET  /api/status      состояние буфера (для ПК-приложения)
//   POST /api/command     текстовая команда как по Bluetooth (для ПК без BT)
//   GET  /stream?sec=N    поток кадров: ЛЕНТА запрашивает у ПК следующие N секунд
//                         эфира (ответ в том же формате — см. pullFromPC в .ino)
//   GET  /api/config      параметры ленты для приложения
// ============================================================================
#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"

// глобальные объекты объявлены в .ino
extern RingBuffer gRB;
extern Playlist   gPL;
extern Feeder     gFeeder;
extern Banner     gBanner;
extern uint8_t    gEffect;
extern bool       gAutonomous;

// (btRunCommand уже предобъявлен в bt.h, который включается ДО webapi.h в .ino)

static WebServer server(HTTP_PORT);

// --------------------------- HTML мини-редактора ----------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html><html lang="ru"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Новогодняя лента — редактор</title><style>
body{font-family:system-ui;background:#0b1020;color:#e8ecff;margin:0;padding:16px}
h1{font-size:20px}.card{background:#151a30;border:1px solid #2a3158;border-radius:12px;padding:14px;margin:10px 0}
input[type=text]{width:99%;padding:10px;font-size:16px;border-radius:8px;border:1px solid #33407a;background:#0d1330;color:#fff}
select,button{padding:10px 14px;font-size:15px;border-radius:8px;border:1px solid #33407a;background:#1c2547;color:#fff;cursor:pointer}
button:hover{background:#2a3663}.go{background:#146c2e;border-color:#1c8c3c;font-weight:700}
li{margin:6px 0;background:#101636;padding:8px;border-radius:8px;list-style:none}
ul{padding:0}#st{color:#9fd0ff;font-size:13px}.del{background:#7a1c1c;border-color:#a22}</style></head><body>
<h1>&#127876; Новогодняя LED-лента 8x96</h1>
<div class="card"><div id="st">подключение...</div></div>
<div class="card"><b>Написать что вывести на ленту:</b><br><br>
<input id="txt" type="text" placeholder="С Новым Годом, школа! &#10024;" maxlength="200"><br><br>
Эффект: <select id="eff"><option value="0">Белый</option><option value="1">Гирлянда</option>
<option value="2">Радуга</option><option value="3">Снежный</option></select>
Скорость: <select id="spd"><option value="1">медленно</option><option value="2" selected>обычно</option><option value="3">быстро</option></select>
&nbsp;<button class="go" onclick="showNow()">&#9654; Показать сейчас</button>
&nbsp;<button onclick="addItem()">+ В очередь</button>
</div>
<div class="card"><b>Очередь показов</b><ul id="list"></ul>
<button onclick="savePl()">Сохранить на ленту</button>
<button onclick="clearPl()">Очистить всё</button></div>
<script>
let items=[];
async function refresh(){try{
 const r=await fetch('/api/playlist');items=await r.json();render();}catch(e){}}
function render(){const u=document.getElementById('list');u.innerHTML='';
 items.forEach((it,i)=>{const li=document.createElement('li');
 li.textContent=(i+1)+'. '+it.t+'  [эфф.'+it.e+', скор.'+it.s+'] ';
 const b=document.createElement('button');b.className='del';b.textContent='X';
 b.onclick=()=>{items.splice(i,1);render();};li.appendChild(b);u.appendChild(li);});}
function collect(t){return {t:t||document.getElementById('txt').value.trim(),
 e:+document.getElementById('eff').value,s:+document.getElementById('spd').value};}
async function push(arr){await fetch('/api/playlist',{method:'POST',headers:{'Content-Type':'application/json'},
 body:JSON.stringify({items:arr})});refresh();}
async function showNow(){const t=document.getElementById('txt').value.trim();if(!t)return;
 await push([collect(t)]);document.getElementById('txt').value='';}
async function addItem(){const t=document.getElementById('txt').value.trim();if(!t)return;
 items.push(collect(t));render();document.getElementById('txt').value='';}
async function savePl(){await push(items);}
async function clearPl(){items=[];await push([]);}
async function status(){try{const r=await fetch('/api/status');const s=await r.json();
 document.getElementById('st').innerHTML='Состояние: <b>'+s.state+'</b> | буфер: '+s.buf_sec+' сек | '
 +(s.wifi?'Wi-Fi OK ('+s.ip+')':'Wi-Fi НЕ ПОДКЛЮЧЕН')+' | режим: '+(s.autonomous?'автономный (плейлист из памяти)':'поток с ПК');}catch(e){}}
setInterval(status,2000);refresh();status();
</script></body></html>)rawliteral";

// ------------------------------ handlers ------------------------------------
static void handleIndex() { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }

static String jsonEscape(const String& s) {
  String r; for (size_t i = 0; i < s.length(); i++) { char c = s[i];
    if (c == '"' || c == '\\') { r += '\\'; r += c; } else if (c >= 0x20) r += c; }
  return r;
}

static void handlePlaylistGet() {
  String s = "[";
  for (size_t i = 0; i < gPL.items.size(); i++) {
    if (i) s += ",";
    s += "{\"t\":\"" + jsonEscape(gPL.items[i].text) + "\",\"e\":" + String(gPL.items[i].effect)
       + ",\"s\":" + String(gPL.items[i].speed) + "}";
  }
  server.send(200, "application/json; charset=utf-8", s + "]");
}

static void handlePlaylistPost() {
  String body = server.arg("plain");
  // допускаем и {"items":[...]}, и просто [...]
  int io = body.indexOf("\"items\"");
  if (io >= 0) { int b = body.indexOf('[', io); if (b >= 0) body = body.substring(b); }
  if (!gPL.parseJson(body)) { server.send(400, "text/plain", "bad json"); return; }
  gPL.save();
  // мгновенно перезапускаем эфир из новых данных
  gRB.reset();
  gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
  gFeeder.refillNow(gRB.cap / 2);
  server.send(200, "text/plain", "ok");
}

// Команда как по Bluetooth: TEXT:..., EFFECT:..., BRIGHT:... и т.д.
static void handleCommand() {
  String cmd = server.arg("plain");
  if (cmd.isEmpty()) cmd = server.arg("cmd");
  btRunCommand(cmd);
  server.send(200, "text/plain", "ok");
}

static void handleStatus() {
  String st = gRB.state == BUF_PLAYING ? "PLAYING" : gRB.state == BUF_WAIT ? "WAIT_NET"
            : gRB.state == BUF_READY ? "READY" : gRB.state == BUF_ERROR ? "ERROR" : "EMPTY";
  float sec = gRB.available() * gRB.frameMs / 1000.f;
  server.send(200, "application/json",
    "{\"state\":\"" + st + "\",\"buf_sec\":" + String(sec, 1) +
    ",\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
    ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
    ",\"autonomous\":" + String(gAutonomous ? "true" : "false") +
    ",\"paired\":" + String(gPrefs.getBool("paired", false) ? "true" : "false") +
    ",\"scratch\":" + String(gScratch.running ? "true" : "false") +
    ",\"blocks\":" + String((int)gScratch.blocks.size()) +
    ",\"open\":" + String(pinAccept("") ? "true" : "false") +
    ",\"pin\":\"" + (gPairMode ? gPin : String("")) + "\"}");
}

// Поток: отдать следующие N секунд текущего эфира как кадры 8x96 (1 байт/пиксель).
// Используется ПК-приложением, чтобы «видеть» то же, что показывает лента.
static void handleStream() {
  int sec = server.arg("sec").toInt();
  if (sec <= 0) sec = 10;
  size_t n = min((uint32_t)(sec * TARGET_FPS), (uint32_t)MAX_FRAMES);
  WiFiClient c = server.client();
  c.print(String("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n") +
          "X-Frame-Px: " + String(gRB.framePx) + "\r\n" +
          "X-W: " + String(gBanner.w()) + "\r\nX-H: " + String(gBanner.h()) + "\r\n" +
          "X-Fps: " + String(TARGET_FPS) + "\r\n" +
          "X-Frames: " + String(n) + "\r\nConnection: close\r\n\r\n");
  uint32_t pos = gRB.playPos;
  for (size_t i = 0; i < n && c.connected(); i++) {
    const uint8_t* f = (pos < gRB.head) ? gRB.frameAt(pos++) : gRB.lastFrame();
    if (f) c.write(f, gRB.framePx); else break;
    if (i % 10 == 0) delay(1);                     // не душим воспроизведение
  }
  c.stop();
}

// ---- Scratch-программа ленты ----
static void handlePrgGet() { server.send(200, "application/json; charset=utf-8", gScratch.toJson()); }

static void handlePrgPost() {
  String body = server.arg("plain");
  if (!gScratch.parseJson(body)) { server.send(400, "text/plain", "bad json"); return; }
  gScratch.saveProg(); gScratch.restart(); gScratch.running = true;
  server.send(200, "text/plain", "ok:" + String((int)gScratch.blocks.size()));
}

static void handleRun()  { if (!gScratch.blocks.empty()) { gScratch.restart(); gScratch.running = true; server.send(200,"text/plain","ok"); } else server.send(400,"text/plain","no prg"); }
static void handleStop() { gScratch.running = false; gScratch.resumePlaylist(); server.send(200,"text/plain","ok"); }

// Сброс панели: показать PIN-код и включить точку доступа для привязки
static void handleResetPanel() {
  gPrefs.putBool("paired", false);
  pairEnter();
  server.send(200, "application/json", "{\"pin\":\"" + gPin + "\",\"ap\":true}");
}

// Привязка по коду из приложения (когда лента уже в той же Wi-Fi сети):
// POST /api/pair {"code":"1234"}  (пустой код/0000 — вход без пароля)
static void handlePairPost() {
  String body = server.arg("plain");
  int ci = body.indexOf("\"code\"");
  String code;
  if (ci >= 0) { int q1 = body.indexOf('"', ci + 7); int q2 = body.indexOf('"', q1 + 1);
                 if (q1 > 0 && q2 > q1) code = body.substring(q1 + 1, q2); }
  if (pinAccept(code)) {          // открытый режим (PIN 0000) пропускает пустой код
    gPrefs.putBool("paired", true);
    if (gPairMode) pairExit(true);
    server.send(200, "text/plain", "ok-paired");
  } else server.send(403, "text/plain", "bad code");
}

// Список Wi-Fi сетей вокруг (для меню настроек в приложениях)
static void handleNetworks() {
  String s = "[";
  int n = WiFi.scanComplete();
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  s += "]";
  WiFi.scanDelete();
  server.send(200, "application/json; charset=utf-8", s);
}

static void handleScanReq() {
  int r = WiFi.scanNetworks(true /*async*/);
  server.send(200, "text/plain", r == WIFI_SCAN_RUNNING ? "scanning" : "started");
}

static void handleWifi() {
  String body = server.arg("plain");           // "ssid|пароль"
  btRunCommand("WIFI:" + body);
  server.send(200, "text/plain", "wifi saved, reconnecting");
}

static void handleConfig() {
  server.send(200, "application/json",
    "{\"fw\":\"ny-banner-esp32\",\"rows\":8,\"cols\":" + String(gBanner.w()) +
    ",\"fps\":" + String(TARGET_FPS) + ",\"buffer_sec\":" + String(BUFFER_SECONDS) +
    ",\"bt\":\"" BT_DEVICE_NAME "\"}");
}

void webSetup() {
  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/prg", HTTP_GET, handlePrgGet);
  server.on("/api/prg", HTTP_POST, handlePrgPost);
  server.on("/api/run", HTTP_POST, handleRun);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/reset", HTTP_POST, handleResetPanel);
  server.on("/api/pair", HTTP_POST, handlePairPost);
  server.on("/api/networks", HTTP_GET, handleNetworks);
  server.on("/api/scan", HTTP_POST, handleScanReq);
  server.on("/api/playlist", HTTP_GET, handlePlaylistGet);
  server.on("/api/playlist", HTTP_POST, handlePlaylistPost);
  server.on("/api/command", HTTP_POST, handleCommand);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.begin();
}

void webLoop() { server.handleClient(); }

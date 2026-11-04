// ============================================================================
//  scratchvm.h — мини-интерпретатор «Scratch-программ» внутри ленты.
//  Программа = список блоков, каждый блок: {op, a, b}.
//  Приложения (сайт / Android / ПК) собирают блоки и шлют их на ESP32 одним
//  запросом; лента исполняет программу сама и повторяет её по кругу —
//  поэтому всё работает даже без подключённого ПК/телефона.
//
//  Хранение: LittleFS /scratch.json + Preferences (переживает перепрошивку).
//  Команды (Wi-Fi /api/command или Bluetooth):
//     PRG:[{"op":"text","a":"С НОВЫМ ГОДОМ"},{"op":"wait","a":"3"},...]
//     RUN            — запустить сохранённую программу
//     STOP           — остановить и вернуться к обычному плейлисту
//     ISCRATCH       — сбросить панель: показать PIN-код + включить Wi-Fi AP
//                      (делается при первом включении автоматически)
//     GETCODE        — вернуть текущий PIN
//     BRIGHT:n / EFFECT:n / CLEAR / SAVE — как обычно
// ============================================================================
#pragma once
#include <Arduino.h>
#include <LittleFS.h>

#define SCRATCH_FILE "/scratch.json"

// ---- опережающие объявления (используются bt.h и .ino) ----
extern String   gPin;
extern bool     gPairMode;
void pairEnter();
void pairExit(bool keepWifi);
void pairLoop();
class ScratchVM;
extern ScratchVM* gScratchPtr;


enum ScrOp : uint8_t {
  SOP_TEXT = 0,   // a=текст, b=эффект(0..3), c=скорость   -> показать бегущим
  SOP_FX,         // a=salute|snow|tree|garland             -> готовый клип
  SOP_WAIT,       // a=секунды                              -> пауза
  SOP_BLINK,      // a=текст, b=раз                         -> мигать текстом
  SOP_EFFECT,     // a=номер эффекта                        -> сменить цвет
  SOP_BRIGHT,     // a=0..255                               -> яркость
  SOP_CLEAR,      //                                        -> очистить очередь
  SOP_SPEED,      // a=1..4                                 -> скорость текущего
  SOP_NEXT,       //                                        -> следующий клип
  SOP_REPEAT,     // a=раз (повторить ВСЮ программу)        -> цикл
};

struct ScrBlock { ScrOp op; String a; int b; int c; };

extern RingBuffer gRB; extern Playlist gPL; extern Feeder gFeeder;
extern Banner gBanner; extern uint8_t gEffect; extern bool gAutonomous;


class ScratchVM {
public:
  Vector<ScrBlock> blocks;
  size_t pc = 0;
  bool running = false;
  bool repeatAll = true;
  uint32_t waitUntilMs = 0;
  uint32_t stepDueMs = 0;
  uint8_t savedEffect = 1;

  void restart() { pc = 0; stepDueMs = 0; }

  // ---- парсер JSON вида [{"op":"text","a":"...","b":"2"},...] ----
  bool parseJson(const String& s) {
    blocks.clear();
    int i = 0, n = s.length();
    while (true) {
      int o = s.indexOf("{\"op\":", i); if (o < 0) break;
      int q1 = s.indexOf('"', o + 6); if (q1 < 0) break;
      int q2 = q1 + 1; while (q2 < n && !(s[q2] == '"' && s[q2 - 1] != '\\')) q2++;
      String op = s.substring(q1 + 1, q2);
      ScrBlock bl; bl.op = SOP_TEXT; bl.b = 0; bl.c = 2;
      if      (op == "text")   bl.op = SOP_TEXT;
      else if (op == "fx")      bl.op = SOP_FX;
      else if (op == "wait")    bl.op = SOP_WAIT;
      else if (op == "blink")   bl.op = SOP_BLINK;
      else if (op == "effect")  bl.op = SOP_EFFECT;
      else if (op == "bright")  bl.op = SOP_BRIGHT;
      else if (op == "clear")   bl.op = SOP_CLEAR;
      else if (op == "speed")   bl.op = SOP_SPEED;
      else if (op == "next")    bl.op = SOP_NEXT;
      else if (op == "repeat")  bl.op = SOP_REPEAT;
      else { i = q2; continue; }
      bl.a = jsonStrField(s, q2, "a");
      bl.b = jsonNumField(s, q2, "b", 0);
      bl.c = jsonNumField(s, q2, "c", 2);
      blocks.push_back(bl);
      i = q2;
    }
    return !blocks.empty();
  }

  String toJson() const {
    String s = "[";
    for (size_t k = 0; k < blocks.size(); k++) {
      if (k) s += ",";
      s += "{\"op\":\"" + opName(blocks[k].op) + "\",\"a\":\"" + esc(blocks[k].a) +
           "\",\"b\":" + String(blocks[k].b) + ",\"c\":" + String(blocks[k].c) + "}";
    }
    return s + "]";
  }

  static const char* opName(ScrOp o) {
    switch (o) {
      case SOP_TEXT: return "text"; case SOP_FX: return "fx";
      case SOP_WAIT: return "wait"; case SOP_BLINK: return "blink";
      case SOP_EFFECT: return "effect"; case SOP_BRIGHT: return "bright";
      case SOP_CLEAR: return "clear"; case SOP_SPEED: return "speed";
      case SOP_NEXT: return "next"; default: return "repeat";
    }
  }

  bool saveProg() {
    File f = LittleFS.open(SCRATCH_FILE, "w"); if (!f) return false;
    f.print(toJson()); f.close(); return true;
  }
  bool loadProg() {
    File f = LittleFS.open(SCRATCH_FILE, "r"); if (!f) return false;
    String s = f.readString(); f.close(); return parseJson(s);
  }

  // ---- исполнение: вызывается из loop() раз в секунду+ ----
  void tick() {
    if (!running || blocks.empty()) return;
    uint32_t now = millis();
    if (now < waitUntilMs) return;               // идёт пауза блока WAIT
    if (now < stepDueMs) return;                 // не время следующего блока
    execBlock(blocks[pc]);
    pc++;
    if (pc >= blocks.size()) {
      bool rep = false;
      for (auto& b : blocks) if (b.op == SOP_REPEAT) { rep = true; break; }
      if (rep || repeatAll) pc = 0;
      else { running = false; resumePlaylist(); return; }
    }
    stepDueMs = now + BLOCK_MIN_MS;
  }

  void execBlock(const ScrBlock& bl) {
    switch (bl.op) {
      case SOP_TEXT: {
        uint8_t effOld = gEffect;
        if (bl.b >= 0 && bl.b <= 3) gEffect = (uint8_t)bl.b;
        showNow(bl.a, constrain(bl.c, 1, 4));
        (void)effOld;
        stepDueMs = millis() + estRunSecs(bl.a, bl.c) * 1000UL;
        break; }
      case SOP_FX: {
        String f = bl.a; f.toLowerCase();
        const char* t = "S NOVYM GODOM!"; uint8_t e = 2; int sp = 2;
        if (f == "snow")    { t = "SNEG SNESHKII SNESHKII"; e = 3; }
        else if (f == "tree")   { t = "YOLKA LES ELOCHKA"; e = 1; }
        else if (f == "garland"){ t = "SCHOLA POZDRAVLAET"; e = 1; }
        gEffect = e; showNow(t, sp);
        stepDueMs = millis() + estRunSecs(t, sp) * 1000UL;
        break; }
      case SOP_WAIT:
        waitUntilMs = millis() + (uint32_t)constrain(bl.a.toInt(), 0, 600) * 1000UL;
        break;
      case SOP_BLINK: {
        // мигание без задержек в loop(): ставим текст и «погасший» клип
        // как два коротких кадра плейлиста — эфир сам их чередует
        int times = constrain(bl.b, 1, 20);
        gPL.clearAll();
        for (int r = 0; r < times; r++) {
          gPL.add(bl.a, gEffect, 4);
          gPL.add(" ", gEffect, 4);
        }
        gRB.reset();
        gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
        gFeeder.refillNow(gRB.cap / 2);
        stepDueMs = millis() + (uint32_t)times * 900;
        break; }
      case SOP_EFFECT:
        gEffect = constrain(bl.a.toInt(), 0, 3);
        break;
      case SOP_BRIGHT:
        gBanner.setBrightness(constrain(bl.a.toInt(), 0, 255));
        break;
      case SOP_CLEAR:
        gPL.clearAll(); gRB.reset(); break;
      case SOP_SPEED:
        if (!gPL.items.empty()) gPL.items[0].speed = constrain(bl.a.toInt(), 1, 4);
        break;
      case SOP_NEXT:
        gFeeder.itemIdx++; gFeeder.scroll = 0; break;
      case SOP_REPEAT: break; // обработан в tick()
    }
  }

  // мгновенно показать текст (очистив эфир)
  void showNow(const String& text, int speed) {
    gPL.clearAll(); gPL.add(text, gEffect, speed);
    gRB.reset();
    gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
    gFeeder.refillNow(gRB.cap / 2);
  }

  void resumePlaylist() {
    gPL.load();
    gRB.reset();
    gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
    gFeeder.refillNow(gRB.cap / 2);
  }

private:
  static uint32_t estRunSecs(const String& t, int speed) {
    int w = textWidth(t);   // textWidth уже возвращает пиксели (6 px/символ)
    float pxps = max(1, speed) * TARGET_FPS;         // шагов/сек
    return (uint32_t)((w + 96) / pxps) + 2;          // пробег + 2 сек пауза
  }
  // Числовое поле JSON: "b":12 или "b":"12". Пусто/нет -> def.
  static int jsonNumField(const String& s, int from, const char* key, int def) {
    String pat = String("\"") + key + "\":";
    int p = s.indexOf(pat, from); if (p < 0 || p > from + 400) return def;
    { int cb = s.indexOf('}', from); if (cb >= 0 && p > cb) return def; }
    int q = p + pat.length();
    while (q < (int)s.length() && (s[q] == ' ' || s[q] == '"')) q++;
    String num;
    while (q < (int)s.length() && s[q] >= '0' && s[q] <= '9') { num += s[q]; q++; }
    return num.length() ? num.toInt() : def;
  }

  static String jsonStrField(const String& s, int from, const char* key) {
    String pat = String("\"") + key + "\":";
    int p = s.indexOf(pat, from); if (p < 0 || p > from + 400) return "";
    { int cb = s.indexOf('}', from); if (cb >= 0 && p > cb) return ""; }
    int q = s.indexOf('"', p + pat.length()); if (q < 0) return "";
    int r = q + 1; while (r < (int)s.length() && !(s[r] == '"' && s[r - 1] != '\\')) r++;
    String v = s.substring(q + 1, r);
    // unescape
    String out; for (size_t i = 0; i < v.length(); i++) {
      if (v[i] == '\\' && i + 1 < v.length()) { i++; out += v[i]; } else out += v[i]; }
    return out;
  }
  static String esc(const String& s) {
    String r; for (size_t i = 0; i < s.length(); i++) { char c = s[i];
      if (c == '"' || c == '\\') { r += '\\'; r += c; } else if ((uint8_t)c >= 0x20) r += c; }
    return r;
  }
};

#define BLOCK_MIN_MS 120   // минимальная «длительность» блока

// --------------------------------- PIN-код ----------------------------------
// При первом включении (или по команде ISCRATCH / кнопке «Сбросить панель»)
// лента показывает на панелях 4-значный код и поднимает точку доступа
// «NY-BANNER-SETUP», куда приложение подключается и вводит этот код.
#include <esp_wifi.h>
String   gPin;
bool     gPairMode = false;
static uint32_t gPairSince = 0;
static WiFiServer gPairSrv(80);   // порт 80 — страница входа в AP (как у роутера)

String genPin() {
  char buf[5];
  uint32_t r = (uint32_t)(esp_random() % 9000 + 1000);
  snprintf(buf, sizeof(buf), "%04u", (unsigned)r);
  return String(buf);
}

static void drawCentered(const String& line) {
  uint32_t W = gBanner.w(), H = gBanner.h();
  static uint8_t f[8 * 128];
  memset(f, 0, sizeof(f));
  drawTextRow(f, W, H, line, 0, 0);
  for (int pass = 0; pass < 4; pass++) { gBanner.show(f, millis(), 2); delay(200); }
}

void pairShowPage(WiFiClient& c) {
  String body = "<!doctype html><meta charset=utf-8><title>NY-BANNER</title>"
    "<style>body{font-family:system-ui;background:#0b1020;color:#fff;padding:24px;text-align:center}"
    "input{font-size:28px;width:160px;text-align:center;padding:8px;border-radius:8px;border:1px solid #444}"
    "button{font-size:20px;padding:10px 24px;margin-top:12px;border-radius:8px}</style>"
    "<h2>&#127876; Панель готова к настройке</h2>"
    "<p>Введите код, показанный на LED-ленте:</p>"
    "<form method=post action=/pair><input name=code maxlength=4 pattern=[0-9]{4}>"
    "<br><button>Подключить</button></form>";
  c.print(String("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n") +
          "Connection: close\r\n\r\n" + body);
}

static void pairHttpOnce() {
  if (!gPairSrv.hasClient()) return;
  WiFiClient c = gPairSrv.available();
  if (!c) return;
  String req = "", line; uint32_t t0 = millis();
  while (c.connected() && millis() - t0 < 2000) {
    if (!c.available()) { delay(2); continue; }
    char ch = c.read();
    if (ch == '\n') { if (line.isEmpty()) break; req += line + "\n"; line = ""; }
    else line += ch;
  }
  if (req.startsWith("GET /pair") || req.startsWith("GET /")) { drawCentered(gPin); pairShowPage(c); }
  else if (req.startsWith("POST /pair")) {
    // читаем Content-Length и тело
    int cli = req.indexOf("Content-Length:");
    int len = cli >= 0 ? req.substring(cli + 15).toInt() : 0;
    String body = "";
    uint32_t t1 = millis();
    while ((int)body.length() < len && c.connected() && millis() - t1 < 2000) {
      if (c.available()) body += (char)c.read(); else delay(2);
    }
    int ci = body.indexOf("code=");
    String code = ci >= 0 ? body.substring(ci + 5, ci + 9) : "";
    bool ok = (code == gPin);
    String resp = ok ?
      "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
      "<meta charset=utf-8><body style='font-family:system-ui;background:#0b1020;color:#eaf'>"
      "<h2>&#10004; Код принят!</h2><p>Теперь выберите Wi-Fi школы в приложении — "
      "панель запомнит сеть.</p></body>"
      :
      "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
      "<meta charset=utf-8><body style='font-family:system-ui;background:#0b1020;color:#faa'>"
      "<h2>&#10008; Неверный код</h2><p>Посмотрите код на ленте ещё раз (кнопка «Показать код»).</p></body>";
    c.print(resp);
    if (ok) {
      drawCentered("CODE OK");
      pairExit(true);
    }
  }
  c.stop();
}

void pairEnter() {
  gPin = genPin();
  gPairMode = true; gPairSince = millis();
  WiFi.mode(WIFI_AP_STA);
  String apName = String("NY-BANNER-SETUP-") + gPin.substring(3);
  WiFi.softAP(apName.c_str());            // открытая точка для настройки
  gPairSrv.begin();
  drawCentered(gPin);
  Serial.println("PAIR CODE: " + gPin + "  AP: " + apName);
}

void pairExit(bool keepWifi) {
  gPairMode = false;
  gPairSrv.end();
  WiFi.softAPdisconnect(true);
  if (keepWifi) {
    String ssid = gPrefs.getString("ssid", WIFI_SSID);
    String pass = gPrefs.getString("pass", WIFI_PASS);
    WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(), pass.c_str());
  }
}

void pairLoop() {
  if (!gPairMode) return;
  pairHttpOnce();
  if (millis() - gPairSince > 15UL * 60 * 1000) {  // 15 минут — выход в штатный режим
    pairExit(false);
  }
}

// глобальный экземпляр VM + указатель для bt.h/webapi.h
ScratchVM gScratch;
ScratchVM* gScratchPtr = &gScratch;

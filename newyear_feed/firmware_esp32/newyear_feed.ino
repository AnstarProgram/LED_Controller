// ============================================================================
//  newyear_feed.ino — «Новогодняя школа-лента» на ESP32 (WROOM/PSRAM)
//
//  Что умеет:
//   • Показывает плейлист клипов на цветном TFT (RGB565 кадры из .bin файлов).
//   • Управляется по Wi-Fi (HTTP API на :80 + ПК-приложение sender.py).
//   • Фоновая предзагрузка: клип N+1 докачивается ПОКА идёт показ клипа N.
//     За 5 секунд до конца проверяется готовность; если файла нет — на экран
//     выводится ошибка и закачка запускается заново, эфир не гаснет.
//   • Bluetooth LE UART: команды PLAY / PAUSE / NEXT / PREV / RELOAD / STATUS /
//     BRIGHT:<0..255> / MODE:SNOW|RAINBOW|FIRE с телефона или ПК.
//   • NeoPixel-гирлянда (режимы снежинок/радуги/салюта).
//   • audio.h — задел под колонки (I2S), включается одной строкой в config.h.
//
//  Библиотеки (через Library Manager Arduino IDE):
//   TFT_eSPI (Bodmer), Adafruit NeoPixel, FastLED(опц.), ESP32 BLE Arduino.
//  Настройки пинов дисплея — в User_Setup.h библиотеки TFT_eSPI (см. README.md).
// ============================================================================
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "display.h"
#include "storage.h"
#include "downloader.h"
#include "player.h"
#include "leds.h"
#include "audio.h"
#include "bt.h"

Display disp;          // определение extern Display disp из display.h
static WebServer server(HTTP_PORT);
static String pcHost = "";     // адрес ПК (узнаётся из /setup или задаётся ниже)

// --------------------------- команды ----------------------------------------
static bool paused = false;

// прямой переход к клипу (для NEXT/PREV); объявляется до обработчика команд
static bool openClipDirect(int idx) {
    if (idx >= 0 && idx < (int)playlist.clips.size()) {
        if (clipReady(playlist.clips[idx])) {
            player.host = pcHost;
            return player.openClip(idx);
        }
        dl.start(pcHost, playlist.clips[idx]);   // нет файла — качаем
    }
    return false;
}

static void handleCommand(String cmd) {
    cmd.trim(); cmd.toUpperCase();
    if (cmd == "PLAY")  { paused = false; bt.notify("OK PLAY"); }
    else if (cmd == "PAUSE") { paused = true; bt.notify("OK PAUSE"); }
    else if (cmd == "NEXT" || cmd == "PREV") {
        paused = true;
        int n = max(1, (int)playlist.clips.size());
        int step = (cmd == "NEXT") ? 1 : (n - 1);
        int nxt = ((player.cur < 0 ? 0 : player.cur) + step) % n;
        player.closeClip(); player.started = false; player.cur = -1;
        dl.stop();
        if (openClipDirect(nxt)) paused = false;
        bt.notify("OK " + cmd);
    }
    else if (cmd.startsWith("BRIGHT:")) { leds.setBrightness(cmd.substring(7).toInt()); bt.notify("OK BRIGHT"); }
    else if (cmd.startsWith("MODE:")) {
        String m = cmd.substring(5);
        leds.mode = m == "SNOW" ? LED_SNOW : m == "FIRE" ? LED_FIRE :
                    m == "STANDBY" ? LED_STANDBY : LED_RAINBOW;
        bt.notify("OK MODE");
    }
    else if (cmd == "RELOAD") { loadIndex(); player.closeClip(); player.started = false; dl.stop(); bt.notify("OK RELOAD"); }
    else if (cmd == "STATUS") {
        bt.notify(String("clip=") + player.cur + " dl=" + (int)dl.state +
                  " wifi=" + (WiFi.isConnected() ? 1 : 0));
    }
}

// ----------------------------- Wi-Fi -----------------------------------------
static void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    disp.statusLine("WiFi connecting...", C_WARN);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) { delay(500); yield(); }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nIP ленты: %s  (откройте http://%s)\n",
                      WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
        disp.statusLine("WiFi OK " + WiFi.localIP().toString(), C_OK);
    } else {
        disp.statusLine("WiFi FAIL -> AP mode", C_ERR);
        WiFi.softAP("NY-Feed", "newyear123");   // резервная точка доступа
    }
}

// ---------------------------- HTTP API ---------------------------------------
// GET  /                 — статус-страница
// POST /setup?host=192.168.1.50&port=8765      — где лежат файлы на ПК
// GET  /index            — перечитать index.json с ленты
// POST /play /pause /next /reload              — как Bluetooth-команды
// GET  /files/<id>.bin   — (сервер на ESP отдаёт свои файлы для отладки)
static void setupRoutes() {
    server.on("/", HTTP_GET, []() {
        String s = "<h2>NY Feed ESP32</h2><pre>";
        s += "clip: " + String(player.cur) + "\nframes: " + String(player.curFrame) +
             "\ndownload: " + String((int)dl.state) + " " + String(dl.got) + "/" + String(dl.expectBytes) +
             "\nwifi: " + WiFi.RSSI() + " dBm\nBT connected: " + String(bt.connected) + "</pre>";
        s += "<a href='/pause'>pause</a> <a href='/play'>play</a> <a href='/next'>next</a> <a href='/reload'>reload</a>";
        server.send(200, "text/html", s);
    });
    server.on("/setup", HTTP_POST, []() {
        pcHost = server.arg("host");
        player.host = pcHost;
        loadIndex();
        server.send(200, "text/plain", "OK host=" + pcHost + " clips=" + String(playlist.clips.size()));
    });
    server.on("/index", HTTP_GET, []() {
        server.send(200, "application/json", [&]() {
            File f = fsOpen(INDEX_FILE); String j = f ? f.readString() : "{}";
            if (f) f.close(); return j;
        }());
    });
    server.on("/play",  HTTP_POST, []() { handleCommand("PLAY");  server.send(200, "text/plain", "OK"); });
    server.on("/pause", HTTP_POST, []() { handleCommand("PAUSE"); server.send(200, "text/plain", "OK"); });
    server.on("/next",  HTTP_POST, []() { handleCommand("NEXT");  server.send(200, "text/plain", "OK"); });
    server.on("/reload",HTTP_POST, []() { handleCommand("RELOAD");server.send(200, "text/plain", "OK"); });
    server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
    server.begin();
}

// ------------------------------ setup / loop ----------------------------------
void setup() {
    Serial.begin(115200);
    randomSeed(esp_random());

    disp.begin();
    if (!storageBegin()) disp.errorBox("STORAGE", "LittleFS/SD mount failed");
    loadIndex();

    leds.begin();
    audioBegin();

    connectWifi();
    setupRoutes();

    bt.begin(handleCommand);

    // Если pcHost не задан через /setup, попробуйте прописать его здесь:
    // pcHost = "192.168.1.50";
    player.host = pcHost;
}

void loop() {
    server.handleClient();
    if (!paused) player.tick();
    else dl.tick();                 // даже на паузе докачиваем!
    leds.tick();
#ifdef USE_SERVO
    leds.waveServo();
#endif
    delay(2);                       // уступаем Wi-Fi стеку
}

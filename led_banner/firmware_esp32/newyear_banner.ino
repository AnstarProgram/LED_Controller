// ============================================================================
//  newyear_banner.ino — Новогодняя LED-лента школы
//  ESP32 (self-made/«SCP32»-подобная плата с Wi-Fi+BT, microUSB, BOOT/RST)
//  + 3 матричные панели 8x32 (P10/HUB75) => общая лента 8x96.
//
//  Возможности:
//   * Бегущий текст по-русски (свой шрифт 5x7), эффекты цвета: гирлянда/радуга/снег;
//   * RAM-буфер ~5 минут эфира (кадры 8x96 по 10 к/с);
//   * за 5 секунд до конца текущих данных лента сама запрашивает новые у ПК
//     по Wi-Fi (HTTP GET /stream?sec=...); последние 5 сек старых удаляются;
//   * если ПК не ответил за 5 минут — на панели мигает «ОШИБКА СВЯЗИ»,
//     эфир не гаснет (повтор последнего фрагмента), запрос повторяется;
//   * автономный режим: плейлист из памяти ленты (LittleFS) — работает без ПК;
//   * мини-редактор в браузере: http://IP-ленты:8765 (написать текст -> показать);
//   * Bluetooth SPP "NY-BANNER": TEXT:..., ADD:..., EFFECT:..., BRIGHT:... ;
//   * задел под колонки (I2S) — см. audio_stub.h.
//
//  Установка библиотек (Arduino IDE -> Library Manager):
//   1. "SmartLED Matrix Library" (fastechnologie) — драйвер панелей HUB75
//      (если её нет в менеджере — GitHub: fastechnologie/SmartLED-Matrix-Library)
//   2. Плата: "ESP32 Arduino core" (Espressif). Для буфера желательно PSRAM:
//      Tools -> PSRAM: "OPI/QSPI PSRAM" если ваша ESP32-S3 и т.п.; без PSRAM
//      буфер автоматически уменьшится (код сам подберёт размер).
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>

#include "config.h"
#include "ringbuffer.h"
#include "font5x7.h"
#include "banner.h"
#include "playlist.h"
#include "feeder.h"
#include "webapi.h"
#include "bt.h"

// ------------------------------ глобальные объекты --------------------------
RingBuffer gRB;
Playlist   gPL;
Feeder     gFeeder;
Banner     gBanner;
uint8_t    gEffect = 1;          // эффект по умолчанию — гирлянда
bool       gAutonomous = true;   // ленте не нужен ПК, пока сам не попросит данные

// Адрес ПК-приложения (для стрим-режима). Можно менять через BT/веб.
String gPcUrl = "http://192.168.1.100:8765";

#include <WiFiUdp.h>
static WiFiUDP gUdp;               // автопоиск ленты приложениями (порт 8766)

// --------------------------- служебные экраны -------------------------------
static void splash(const String& line1) {
  uint32_t W = gBanner.w(), H = gBanner.h();
  uint8_t* f = (uint8_t*)calloc(W * H, 1);
  if (!f) return;
  drawTextRow(f, W, H, line1, 0, 0);
  for (int pass = 0; pass < 3; pass++) { gBanner.show(f, millis(), 0); delay(250); }
  free(f);
}

static void blinkError() {
  static uint32_t lastBlink = 0;
  static bool on = false;
  if (millis() - lastBlink < 500) return;
  lastBlink = millis(); on = !on;
  uint32_t W = gBanner.w(), H = gBanner.h();
  uint8_t* f = (uint8_t*)calloc(W * H, 1);
  if (!f) return;
  if (on) drawTextRow(f, W, H, "OSHIBKA SVYAZI", 0, 0);
  else { const uint8_t* l = gRB.lastFrame(); if (l) memcpy(f, l, W * H); }
  gBanner.show(f, millis(), 0);
  free(f);
}

// -------------------- запрос новых кадров у ПК по Wi-Fi ---------------------
// Формат ответа /stream: заголовок X-Frames + сырые кадры framePx байт каждый.
static bool pullReadFrames(WiFiClient& cli) {
  // читаем заголовки до пустой строки, ловим X-Frames
  int frames = -1; String line; uint32_t t0 = millis();
  while (cli.connected() && millis() - t0 < 10000) {
    if (!cli.available()) { delay(2); continue; }
    char ch = cli.read();
    if (ch == '\n') {
      if (line.endsWith("\r")) line.remove(line.length() - 1);
      if (line.isEmpty()) break;
      if (line.startsWith("X-Frames:")) frames = line.substring(9).toInt();
      line = "";
    } else line += ch;
  }
  if (frames <= 0) return false;
  std::unique_ptr<uint8_t[]> buf(new uint8_t[gRB.framePx]);
  int got = 0; t0 = millis();
  while (got < frames && cli.connected() && millis() - t0 < FETCH_TIMEOUT_MS) {
    size_t need = gRB.framePx, off = 0;
    while (need && cli.connected()) {
      int r = cli.readBytes((char*)buf.get() + off, need);
      if (r <= 0) break;
      off += r; need -= r;
    }
    if (need) break;
    gRB.appendFrame(buf.get()); got++;
  }
  return got > 0;
}

static bool pullFromPC(int seconds) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String u = gPcUrl; u.trim();
  if (u.startsWith("http://")) u = u.substring(7);
  int sl = u.indexOf('/'); if (sl >= 0) u = u.substring(0, sl);
  String host = u; uint16_t port = 80;
  int cp = u.indexOf(':');
  if (cp >= 0) { host = u.substring(0, cp); port = u.substring(cp + 1).toInt(); }
  WiFiClient cli;
  if (!cli.connect(host.c_str(), port, 3000)) return false;
  cli.print(String("GET /stream?sec=") + String(seconds) +
            " HTTP/1.1\r\nHost: esp32\r\nConnection: close\r\n\r\n");
  bool ok = pullReadFrames(cli);
  cli.stop();
  return ok;
}

// -------------------------------- setup ------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n== NY-BANNER ESP32 start ==");

  LittleFS.begin(true);
  prefsSetup();
  // сохранённые из приложения настройки имеют приоритет над config.h
  String ssid = gPrefs.getString("ssid", WIFI_SSID);
  String pass = gPrefs.getString("pass", WIFI_PASS);
  gPcUrl      = gPrefs.getString("pcurl", gPcUrl);
  gPL.load();
  if (gPL.items.empty()) {                       // первый запуск — приветствие
    gPL.add("SCHOLA POZDRAVLAET", 1, 2);
    gPL.add("S NOVYM GODOM 2027!", 2, 2);
    gPL.save();
  }

  gBanner.begin();
  uint32_t px = gBanner.w() * gBanner.h();       // 96*8 = 768 байт/кадр

  // Пытаемся выделить буфер на 5 минут; при нехватке памяти код сам уменьшит
  // его вдвое (но не меньше 30 секунд) — эфир всё равно не сорвётся.
  if (!gRB.begin(px)) { splash("NEMA PAMIATI"); while (1) delay(100); }

  gFeeder.begin(&gRB, &gPL, gBanner.w(), gBanner.h());

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  splash(String("WiFi: ") + ssid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
  webSetup();                                    // мини-редактор доступен всегда
  btSetup();                                     // Bluetooth даже без Wi-Fi
  gUdp.begin(8766);                              // UDP-ответ на «Найти авто»

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP().toString());
    splash("WiFi OK");
  } else splash("WiFi NET");

  // Наполняем эфир на несколько минут вперёд ДО выхода на панель
  gFeeder.refillNow(gRB.cap / 2);
}

// --------------------------------- loop ------------------------------------
static uint32_t nextFrameMs = 0;
static uint32_t lastSecCheck = 0;

void loop() {
  uint32_t now = millis();

  // 1) Равномерно играем кадры из RAM-буфера (10 к/с)
  if (now >= nextFrameMs) {
    nextFrameMs = now + gRB.frameMs;
    const uint8_t* f = gRB.step();
    if (f) {
      gBanner.show(f, now, gEffect);
    } else if (gRB.state == BUF_ERROR) {
      blinkError();                              // мигалка «ОШИБКА СВЯЗИ»
    } else {
      const uint8_t* l = gRB.lastFrame();        // буфер пуст — держим последний кадр
      if (l) gBanner.show(l, now, gEffect);
    }
  }

  // 2) Раз в секунду: автозапрос данных у ПК и фоновые задачи
  if (now - lastSecCheck > 1000) {
    lastSecCheck = now;
    gRB.tick();                                  // проверка таймаута WAIT -> ERROR

    if (gRB.shouldRequest() && WiFi.status() == WL_CONNECTED) {
      gRB.startFetch();                          // за 5 сек до конца — просим новые
      bool got = pullFromPC(PREBUFFER_SECONDS);  // качаем следующие 10 секунд
      if (got) gRB.state = BUF_READY;            // данные пришли -> удаление старых разрешено
      else if (gAutonomous) gFeeder.pump();      // ПК молчит -> доращиваем из плейлиста
    } else if (gAutonomous) {
      gFeeder.pump();                            // тихо наполняем буфер, ПК не нужен
    }
  }

  // 3) Веб-редактор и Bluetooth
  webLoop();
  btLoop();

  // 4) Автопоиск приложениями: «NYB_DISCOVER» -> отвечаем своим IP
  {
    int ps = gUdp.parsePacket();
    if (ps > 0) {
      char buf[32] = {0};
      gUdp.read(buf, sizeof(buf) - 1);
      if (String(buf).startsWith("NYB_DISCOVER")) {
        gUdp.beginPacket(gUdp.remoteIP(), gUdp.remotePort());
        gUdp.print("NYB@"); gUdp.print(WiFi.localIP().toString());
        gUdp.endPacket();
      }
    }
  }
}

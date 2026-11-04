// ============================================================================
//  bt.h — Bluetooth Classic SPP (Serial Port Profile): «NY-BANNER».
//  С телефона (приложение типа "Bluetooth Terminal", или наше ПК-приложение
//  через BT-адаптер) можно слать текстовые команды без Wi-Fi:
//     TEXT:С Новым Годом!      — показать текст немедленно
//     ADD:Текст|эфф|скор       — добавить в очередь
//     NEXT / PREV              — следующий/предыдущий пункт плейлиста
//     EFFECT:0..3              — сменить эффект цвета
//     SPEED:n                  — скорость прокрутки
//     BRIGHT:0..255            — яркость панелей
//     CLEAR                    — очистить очередь
//     SAVE                     — сохранить плейлист в память ленты
//     STATUS                   — состояние буфера обратно по BT
// ============================================================================
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <BluetoothSerial.h>   // Classic SPP (есть в пакете esp32 для Arduino)

extern RingBuffer gRB;
extern Playlist   gPL;
extern Feeder     gFeeder;
extern Banner     gBanner;
extern uint8_t    gEffect;

static BluetoothSerial SerialBT;
static String btLine;

void btSetup() {
  SerialBT.begin(BT_DEVICE_NAME);
}

// Общая обработка команд — используется и Bluetooth'ом, и /api/command (Wi-Fi)
void btRunCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("TEXT:")) {
    String t = cmd.substring(5);
    gPL.clearAll(); gPL.add(t, gEffect, 2);
    gRB.reset();
    gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
    gFeeder.refillNow(gRB.cap / 2);
    SerialBT.println("OK");
  } else if (cmd.startsWith("ADD:")) {
    String a = cmd.substring(4);
    int e = 0, s = 2;
    int p1 = a.lastIndexOf('|');
    if (p1 > 0) { s = a.substring(p1 + 1).toInt(); a = a.substring(0, p1); }
    int p2 = a.lastIndexOf('|');
    if (p2 > 0) { e = a.substring(p2 + 1).toInt(); a = a.substring(0, p2); }
    gPL.add(a, e, s); gPL.save(); SerialBT.println("OK");
  } else if (cmd == "NEXT") {
    gFeeder.itemIdx++; gFeeder.scroll = 0; SerialBT.println("OK");
  } else if (cmd == "PREV") {
    gFeeder.itemIdx = gFeeder.itemIdx ? gFeeder.itemIdx - 1 : 0;
    gFeeder.scroll = 0; SerialBT.println("OK");
  } else if (cmd.startsWith("EFFECT:")) {
    gEffect = constrain(cmd.substring(7).toInt(), 0, 3); SerialBT.println("OK");
  } else if (cmd.startsWith("SPEED:")) {
    if (!gPL.items.empty()) { gPL.items[0].speed = constrain(cmd.substring(6).toInt(), 1, 4); }
    SerialBT.println("OK");
  } else if (cmd.startsWith("BRIGHT:")) {
    gBanner.setBrightness(constrain(cmd.substring(7).toInt(), 0, 255)); SerialBT.println("OK");
  } else if (cmd == "CLEAR") {
    gPL.clearAll(); gPL.save(); SerialBT.println("OK");
  } else if (cmd == "SAVE") {
    SerialBT.println(gPL.save() ? "OK" : "ERR");
  } else if (cmd == "STATUS") {
    SerialBT.printf("state=%u frames=%u wifi=%d\n", (unsigned)gRB.state,
                    (unsigned)gRB.available(), (int)WiFi.status());
  } else {
    SerialBT.println("ERR: unknown");
  }
}

void btLoop() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n' || c == '\r') { if (btLine.length()) { btRunCommand(btLine); btLine = ""; } }
    else if (btLine.length() < 300) btLine += c;
  }
}

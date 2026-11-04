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
#include <Preferences.h>
#include <BluetoothSerial.h>   // Classic SPP (есть в пакете esp32 для Arduino)
extern Preferences gPrefs;   // определён ниже (нужен scratchvm.h)
#include "scratchvm.h"        // ScratchVM, gScratchPtr, pairEnter/pairExit

extern RingBuffer gRB;
extern Playlist   gPL;
extern Feeder     gFeeder;
extern Banner     gBanner;
extern uint8_t    gEffect;
extern String     gPcUrl;      // адрес ПК-приложения (стрим-режим), определён в .ino

static Preferences gPrefs;     // энергонезависимое хранение Wi-Fi/URL настроек
extern String genPin();        // определено в scratchvm.h (PIN из config.h / памяти)
extern bool pinAccept(const String& code);

void prefsSetup() {
  gPrefs.begin("nybanner", false);
}

static BluetoothSerial SerialBT;
static String btLine;

void btSetup() {
  SerialBT.begin(BT_DEVICE_NAME);
}

// Общая обработка команд — используется и Bluetooth'ом, и /api/command (Wi-Fi)
void btRunCommand(String cmd);   // предобъявление для webapi.h
void btRunCommand(String cmd) {  // реализация
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
  } else if (cmd.startsWith("WIFI:")) {
    // WIFI:имя|пароль — сохранить и переподключиться (из ПК/Android приложения)
    String v = cmd.substring(5);
    int p = v.indexOf('|');
    String ssid = (p >= 0) ? v.substring(0, p) : v;
    String pass = (p >= 0) ? v.substring(p + 1) : "";
    ssid.trim(); pass.trim();
    gPrefs.putString("ssid", ssid); gPrefs.putString("pass", pass);
    gPrefs.putBool("paired", true);   // сеть задана приложением — панель привязана
    SerialBT.println("OK-SAVE-WIFI");
    WiFi.disconnect(); WiFi.begin(ssid.c_str(), pass.c_str());
  } else if (cmd.startsWith("PCURL:")) {
    gPcUrl = cmd.substring(6); gPcUrl.trim();
    gPrefs.putString("pcurl", gPcUrl);
    SerialBT.println("OK-PCURL");
  } else if (cmd.startsWith("FX:")) {
    // FX:salute | FX:snow | FX:tree — новогодние эффекты из блоков Scratch
    String f = cmd.substring(3); f.trim(); f.toLowerCase();
    const char* txt = NULL; uint8_t eff = 2;
    if (f == "salute") { txt = "S NOVYM GODOM!"; eff = 2; }
    else if (f == "snow")  { txt = "SNEG SNESHKII SNESHKII"; eff = 3; }
    else if (f == "tree")  { txt = "YOLKA LES ELOCHKA"; eff = 1; }
    if (txt) {
      gPL.clearAll(); gPL.add(txt, eff, 2);
      gRB.reset();
      gFeeder.itemIdx = 0; gFeeder.scroll = 0; gFeeder.pauseFrames = 0;
      gFeeder.refillNow(gRB.cap / 2);
      SerialBT.println("OK-FX");
    } else SerialBT.println("ERR: unknown fx");
  } else if (cmd.startsWith("PRG:")) {
    // Загрузить Scratch-программу: PRG:[{"op":"text","a":"..."},...]
    String j = cmd.substring(4);
    ScratchVM* vm = gScratchPtr;
    if (vm && vm->parseJson(j)) {
      vm->saveProg(); vm->restart(); vm->running = true;
      SerialBT.println("OK-PRG:" + String((int)vm->blocks.size()));
    } else SerialBT.println("ERR: bad prg");
  } else if (cmd == "RUN") {
    if (gScratchPtr && !gScratchPtr->blocks.empty()) {
      gScratchPtr->restart(); gScratchPtr->running = true; SerialBT.println("OK-RUN");
    } else SerialBT.println("ERR: no prg");
  } else if (cmd == "STOP") {
    if (gScratchPtr) { gScratchPtr->running = false; gScratchPtr->resumePlaylist(); }
    SerialBT.println("OK-STOP");
  } else if (cmd == "ISCRATCH") {
    // Сброс панели: показать PIN + режим привязки
    gPrefs.putBool("paired", false);
    SerialBT.println("OK-PIN");
    pairEnter();
  } else if (cmd == "GETCODE") {
    SerialBT.println("PIN:" + gPin);
  } else if (cmd.startsWith("PIN")) {
    // Сменить PIN-код: «PIN 1234», «PIN:1234» или «PIN 0» (=0000, вход без пароля)
    String arg = cmd.substring(3); arg.trim(); arg.replace(":", ""); arg.trim();
    if (arg.length() >= 1 && arg.toInt() >= 0 && arg.toInt() <= 9999) {
      gPrefs.putInt("pin", arg.toInt());
      gPin = genPin();
      SerialBT.println("OK-PIN:" + gPin);
    } else SerialBT.println("ERR: pin 0..9999");
  } else if (cmd == "PAIROK") {
    gPrefs.putBool("paired", true);
    pairExit(true);
    SerialBT.println("OK-PAIRED");
  } else if (cmd == "REBOOT") {
    SerialBT.println("BYE"); delay(200); ESP.restart();
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

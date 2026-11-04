// ============================================================================
//  feeder.h — «автономный режим»: лента САМА генерирует кадры из плейлиста
//  и наполняет свой RAM-буфер на 5 минут вперёд. ПК для этого НЕ нужен!
//  (ПК-режим streaming тоже поддерживается — см. downloader.h)
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "ringbuffer.h"
#include "playlist.h"
#include "banner.h"

class Feeder {
public:
  RingBuffer* rb;
  Playlist* pl;
  uint32_t W = 96, H = 8;
  size_t itemIdx = 0;        // текущий пункт плейлиста
  uint32_t scroll = 0;       // позиция бегущей строки внутри пункта
  uint32_t pauseFrames = 0;  // пауза между пунктами (кадров)
  uint8_t* frame = nullptr;

  void begin(RingBuffer* r, Playlist* p, uint32_t w, uint32_t h) {
    rb = r; pl = p; W = w; H = h;
    frame = (uint8_t*)malloc(W * H);
  }

  // Вызывается из loop(): если в буфере мало кадров впереди — сгенерировать ещё.
  void pump() {
    if (!frame || pl->items.empty()) return;
    uint32_t need = min(rb->PREBUFFER_FRAMES(), rb->cap / 2);
    while (rb->leadFrames() < need) generateOne();
  }

  void generateOne() {
    memset(frame, 0, W * H);
    const Item& it = pl->items[itemIdx % pl->items.size()];
    if (pauseFrames) {
      pauseFrames--;
    } else {
      drawTextRow(frame, W, H, it.text, scroll, 0);
      scroll += it.speed ? it.speed : 1;
      if (scroll > textWidth(it.text) + W) {          // пробег завершён — следующий пункт
        scroll = 0; itemIdx++; pauseFrames = TARGET_FPS * 2; // 2 сек пауза
      }
    }
    rb->appendFrame(frame);
  }

  // Срочно наполнить буфер (например после редактирования текста).
  // Ограничено по времени, чтобы не блокировать панель надолго.
  void refillNow(uint32_t frames) {
    uint32_t t0 = millis();
    uint32_t done = 0;
    while (done < frames && rb->available() < rb->cap && millis() - t0 < 800) {
      generateOne(); done++;
    }
  }
};

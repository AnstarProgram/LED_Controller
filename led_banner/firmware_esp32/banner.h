// ============================================================================
//  banner.h — панель DMD 8x96 (3 модуля P10 8x32) + рендер ч/б кадра с эффектом цвета.
//  Библиотека: SmartLED Matrix Library (fastechnologie) — ставится из менеджера
//  библиотек Arduino IDE ("SmartLED_MatrixMapper", "SmartLED_ShiftType").
//  Если у вас готовый маппер RFM2169 — раскомментируйте include в config.h и
//  поменяйте класс ниже на RFM2169 (конструкторы совместимы).
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "font5x7.h"

#if PANEL_LAYOUT == PANELS_CHAIN
class Banner {
public:
  // Маппер: цепочка панелей 8x96, каждый пиксель = 1 байт яркости (см. renderFrame)
  SmartLED_MatrixMapper mapper;

  bool begin() {
    mapper.begin(CHAIN_NUM_COLS, CHAIN_NUM_ROWS);   // ширина x высота
    mapper.setBrightness(DEFAULT_BRIGHTNESS);
    return true;
  }
  void setBrightness(uint8_t b) { mapper.setBrightness(b); }

  // Кадр: framePx байт яркости (row-major: y*W+x), эффект цвета на лету.
  // effect: 0 белый, 1 «мигающая гирлянда»(красный/зелёный), 2 радуга-скролл, 3 снег
  void show(const uint8_t* px, uint32_t tms, int effect) {
    const uint32_t W = CHAIN_NUM_COLS, H = CHAIN_NUM_ROWS;
    for (uint32_t y = 0; y < H; y++) {
      for (uint32_t x = 0; x < W; x++) {
        uint8_t v = px[y * W + x];
        uint32_t c = applyEffect(v, x, y, tms, effect);
        mapper.drawPixel(x, y, c);
      }
    }
    mapper.update();
  }

  // Прямая отрисовка произвольного RGB-пикселя (для служебных экранов)
  void dot(uint32_t x, uint32_t y, uint32_t c) { if (x < CHAIN_NUM_COLS && y < CHAIN_NUM_ROWS) mapper.drawPixel(x, y, c); }
  void flush() { mapper.update(); }
  uint32_t w() const { return CHAIN_NUM_COLS; }
  uint32_t h() const { return CHAIN_NUM_ROWS; }

private:
  static uint32_t hsv(uint16_t hue, uint8_t s, uint8_t v);
  uint32_t applyEffect(uint8_t v, uint32_t x, uint32_t y, uint32_t t, int eff) {
    if (!v) return 0;
    switch (eff) {
      case 1: { // гирлянда: красный/зелёный по колонкам, мигание
        bool on = ((t / 300) % 2) == 0;
        bool red = ((x / 4) % 2) == 0;
        uint8_t r = (red && on) ? v : (red ? v / 4 : 0);
        uint8_t g = (!red && on) ? v : (!red ? v / 4 : 0);
        return (r << 16) | (g << 8);
      }
      case 2: { // радуга скроллом
        uint16_t hue = (uint16_t)((x * 8 + t / 20) % 360);
        return hsv(hue, 255, v);
      }
      case 3: { // холодный синий «снежный»
        uint32_t r = v / 4, g = v / 2, b = v;
        return (r << 16) | (g << 8) | b;
      }
      default:
        return (v << 16) | (v << 8) | v;            // белый
    }
  }
};
#else
// Вариант B: три независимых выхода HUB75 (по панели на выход)
class Banner {
public:
  SmartLED_MatrixMapper m[SEP_NUM_PANELS];
  bool begin() {
    for (auto& mm : m) { mm.begin(SEP_COLS, SEP_ROWS); mm.setBrightness(DEFAULT_BRIGHTNESS); }
    return true;
  }
  void setBrightness(uint8_t b) { for (auto& mm : m) mm.setBrightness(b); }
  void show(const uint8_t* px, uint32_t tms, int effect) {
    const uint32_t H = SEP_ROWS, PW = SEP_COLS;
    for (uint32_t y = 0; y < H; y++)
      for (uint32_t p = 0; p < SEP_NUM_PANELS; p++)
        for (uint32_t x = 0; x < PW; x++) {
          uint8_t v = px[y * (PW * SEP_NUM_PANELS) + p * PW + x];
          m[p].drawPixel(x, y, v ? 0xFFFFFFUL * (v / 255.f) : 0); // упрощённо: белый
        }
    for (auto& mm : m) mm.update();
    (void)tms; (void)effect;
  }
  void dot(uint32_t x, uint32_t y, uint32_t c) {
    uint32_t p = x / SEP_COLS; if (p >= SEP_NUM_PANELS) return;
    m[p].drawPixel(x % SEP_COLS, y, c);
  }
  void flush() { for (auto& mm : m) mm.update(); }
  uint32_t w() const { return SEP_COLS * SEP_NUM_PANELS; }
  uint32_t h() const { return SEP_ROWS; }
};
#endif

// HSV -> RGB565-like uint32 (0xRRGGBB)
inline uint32_t Banner::hsv(uint16_t hue, uint8_t s, uint8_t v) {
  float hf = (hue % 360) / 60.f, sf = s / 255.f, vf = v / 255.f;
  int i = (int)hf; float f = hf - i;
  float p = vf * (1 - sf), q = vf * (1 - sf * f), t = vf * (1 - sf * (1 - f));
  float r, g, b;
  switch (i % 6) {
    case 0: r=vf; g=t;  b=p; break; case 1: r=q;  g=vf; b=p; break;
    case 2: r=p;  g=vf; b=t; break; case 3: r=p;  g=q;  b=vf;break;
    case 4: r=t;  g=p;  b=vf;break; default:r=vf; g=p;  b=q; break;
  }
  return ((uint32_t)(r*255)<<16)|((uint32_t)(g*255)<<8)|(uint32_t)(b*255);
}

// ------------------------- Растеризация текста в кадр ------------------------
// Пишет текст в буфер кадра (ч/б яркость). offset — сдвиг бегущей строки.
inline void drawTextRow(uint8_t* px, uint32_t W, uint32_t H, const String& utf8,
                        int32_t offset, uint8_t baselineRow = 0) {
  int32_t x = -offset;
  size_t n = utf8.length();
  size_t i = 0;
  while (i < n) {
    char32_t cp = 0xFFFD;
    uint8_t c = utf8[i];
    // декодирование UTF-8
    if (c < 0x80) { cp = c; i += 1; }
    else if ((c >> 5) == 0x6 && i + 1 < n) { cp = ((c & 0x1F) << 6) | (utf8[i+1] & 0x3F); i += 2; }
    else if ((c >> 4) == 0xE && i + 2 < n) { cp = ((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F); i += 3; }
    else { cp = '?'; i += 1; }

    const uint8_t* g = glyphFor((char16_t)cp);
    if (g) {
      for (int col = 0; col < GLYPH_W; col++) {
        uint8_t bits = g[col];
        int32_t xx = x + col;
        if (xx < 0 || xx >= (int32_t)W) continue;
        for (uint32_t row = 0; row < 7; row++) {
          if (bits & (1 << row)) {
            uint32_t yy = baselineRow + row;
            if (yy < H) px[yy * W + xx] = 255;
          }
        }
      }
    }
    x += CHAR_ADVANCE;
  }
}

// Полная ширина текста в пикселях
inline uint32_t textWidth(const String& utf8) {
  size_t n = utf8.length(), i = 0, w = 0;
  while (i < n) {
    uint8_t c = utf8[i];
    if (c < 0x80) i += 1; else if ((c >> 5) == 0x6) i += 2; else if ((c >> 4) == 0xE) i += 3; else i += 1;
    w += CHAR_ADVANCE;
  }
  return w;
}

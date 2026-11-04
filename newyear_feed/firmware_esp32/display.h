// ============================================================================
//  display.h — обёртка над TFT_eSPI. Картинки клипа = пакеты RGB565, которые
//  приходят с ПК и складываются в PSRAM/heap. Здесь же — русский шрифт для
//  сообщений об ошибках (Cyrillic_Arial из библиотеки TFT_eSPI).
// ============================================================================
#pragma once
#include <TFT_eSPI.h>
#include "config.h"

static TFT_eSPI tft;

// ---- цветовые константы -----------------------------------------------------
#define C_BG      tft.color565(0,   0,   0)
#define C_TITLE   tft.color565(255, 215, 0)     // золотой
#define C_TEXT    tft.color565(230, 230, 255)
#define C_OK      tft.color565(80,  220, 120)
#define C_WARN    tft.color565(255, 170, 40)
#define C_ERR     tft.color565(255, 80,  80)

struct Display {
    int16_t w = 0, h = 0;
    bool ready = false;

    void begin() {
        tft.init();
        tft.setRotation(1);                 // альбомная ориентация
        tft.fillScreen(C_BG);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(C_TEXT, C_BG);
        tft.setTextFont(2);                 // 5x7 — быстрый, латиница
        w = tft.width(); h = tft.height();
        ready = true;
        splash();
    }

    void splash() {
        tft.fillScreen(C_BG);
        drawSnowflakes(8);
        tft.setTextColor(C_TITLE, C_BG);
        // Русский текст требует загруженного Cyrillic-шрифта в LittleFS
        // (файлы fonts/*.vlw создаёт sender.py командой `make-fonts`).
        // Если шрифта нет — показываем латиницу, чтобы не упасть.
        if (tft.loadFont("fonts/Cyrillic_Arial14")) {
            tft.drawString("НОВЫЙ ГОД", w / 2, h / 2 - 20, 4);
            tft.unloadFont();
        } else {
            tft.drawCentreString("NEW YEAR FEED", w / 2, h / 2 - 20, 4);
        }
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawCentreString("Loading feed...", w / 2, h / 2 + 24, 2);
    }

    void clear() { if (ready) tft.fillScreen(C_BG); }

    // Показать один кадр клипа: буфер uint16_t fw*fh в RGB565.
    // Кадры, большие экрана, прореживаются (nearest-neighbour), маленькие — центрируются.
    void pushFrame(const uint16_t *rgb565, int fw, int fh) {
        if (!ready || !rgb565 || fw <= 0 || fh <= 0) return;
        int stepX = max(1, fw / w);                 // прореживание по горизонтали
        int stepY = max(1, fh / h);                 // и по вертикали
        int outW  = fw / stepX, outH = fh / stepY;  // реальные размеры после прореживания
        int x0 = (w - outW) / 2, y0 = (h - outH) / 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        for (int sy = 0; sy < outH && (y0 + sy) < h; ++sy) {
            const uint16_t *src = rgb565 + (size_t)(sy * stepY) * fw;
            tft.pushImage(x0, y0 + sy, min(outW, w - x0), 1, src, stepX);
        }
    }

    void statusLine(const String &s, uint16_t col = C_TEXT) {
        if (!ready) return;
        tft.fillRect(0, h - 16, w, 16, C_BG);
        tft.setTextColor(col, C_BG);
        tft.drawString(s.substring(0, w / 6), 2, h - 15, 2);
    }

    void errorBox(const String &title, const String &detail) {
        if (!ready) return;
        tft.fillScreen(C_BG);
        tft.drawRect(4, 4, w - 8, h - 8, C_ERR);
        tft.setTextColor(C_ERR, C_BG);
        tft.drawCentreString(title, w / 2, 12, 2);
        tft.setTextColor(C_TEXT, C_BG);
        wrapText(detail, 10, 34, w - 20, 2);
    }

    void progress(const String &label, float p /*0..1*/) {
        if (!ready) return;
        int bw = w - 20, bh = 10, bx = 10, by = h - 30;
        tft.fillRect(bx - 1, by - 1, bw + 2, bh + 2, C_BG);
        tft.drawRect(bx, by, bw, bh, C_TEXT);
        tft.fillRect(bx + 1, by + 1, (int)(bw * constrain(p, 0, 1)), bh - 2, C_OK);
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawString(label, 10, by - 14, 2);
    }

    void wrapText(const String &text, int x, int y, int maxWidth, int font) {
        int cy = y;
        for (const String &line : splitLines(text, maxWidth, font)) {
            tft.drawString(line, x, cy, font);
            cy += (font == 2 ? 8 : 16);
        }
    }

    Vector<String> splitLines(const String &text, int maxWidth, int font) {
        Vector<String> out;
        String cur;
        int start = 0;
        while (start <= (int)text.length()) {
            int sp = text.indexOf(' ', start);
            if (sp < 0) sp = text.length();
            String word = text.substring(start, sp);
            String trial = cur.length() ? cur + " " + word : word;
            if (tft.textWidth(trial, font) > maxWidth && cur.length()) {
                out.push_back(cur); cur = word;
            } else cur = trial;
            start = sp + 1;
            if (sp == (int)text.length()) break;
        }
        if (cur.length()) out.push_back(cur);
        return out;
    }

    // Декор: снежинки на чёрном фоне
    void drawSnowflakes(int n) {
        for (int i = 0; i < n; ++i) {
            int x = random(w), y = random(h - 20);
            tft.drawPixel(x, y, C_TEXT);
            tft.drawLine(x - 2, y, x + 2, y, C_TEXT);
            tft.drawLine(x, y - 2, x, y + 2, C_TEXT);
        }
    }
};

extern Display disp;

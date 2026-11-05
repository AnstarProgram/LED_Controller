// ============================================================================
//  leds.h — гирлянда NeoPixel + (опционально) серво-«снежинка».
//  Режимы: SNOW, RAINBOW, FIREWORKS, STEREO-MUSIC (когда появятся колонки).
// ============================================================================
#pragma once
#include <Adafruit_NeoPixel.h>
#ifdef USE_SERVO
#include <ESP32Servo.h>
#endif
#include "config.h"

enum LedMode { LED_SNOW, LED_RAINBOW, LED_FIRE, LED_STANDBY };

struct Leds {
    Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEOPIXEL_TYPE);
    LedMode mode = LED_RAINBOW;
    uint8_t brightness = 180;
    uint32_t tPrev = 0;
    int offset = 0;
#ifdef USE_SERVO
    Servo servo;
#endif

    void begin() {
        strip.begin();
        strip.setBrightness(brightness);
        strip.show();
#ifdef USE_SERVO
        servo.attach(SERVO_PIN);
        servo.write(90);
#endif
    }

    void setBrightness(int p) {
        brightness = constrain(p, 0, 255);
        strip.setBrightness(brightness);
    }

    // Вызывать из loop(): сам решает когда обновлять (экономим CPU на Wi-Fi)
    void tick() {
        if (millis() - tPrev < 40) return;   // ~25 Гц максимум
        tPrev = millis();
        switch (mode) {
            case LED_SNOW:    snow();   break;
            case LED_RAINBOW: rainbow();break;
            case LED_FIRE:    fire();   break;
            default:          standby();break;
        }
        strip.show();
    }

    void standby() { for (int i = 0; i < NEOPIXEL_COUNT; ++i) strip.setPixelColor(i, 0, 0, 20); }

    // Падающие снежинки: белые «волны» бегут вниз по ленте
    void snow() {
        offset++;
        for (int i = 0; i < NEOPIXEL_COUNT; ++i) {
            float v = sin((i + offset) * 0.35f) * 0.5f + 0.5f;
            uint8_t b = v > 0.75f ? 255 : (v > 0.5f ? 60 : 10);
            strip.setPixelColor(i, b, b, min(255, b + 30));
        }
    }

    void rainbow() {
        offset += 3;
        for (int i = 0; i < NEOPIXEL_COUNT; ++i)
            strip.setPixelColor(i, strip.ColorHSV(((uint32_t)i * 65536 / NEOPIXEL_COUNT + offset * 256) & 0xFFFF));
    }

    void fire() {
        for (int i = 0; i < NEOPIXEL_COUNT; ++i) {
            int r = random(150, 255), g = random(0, 120);
            strip.setPixelColor(i, r, g, 0);
        }
    }

    // ЗАДЕЛ НА КОЛОНКИ: когда добавите I2S-микрофон/анализатор звука,
    // вызывайте musicBeat(level) с уровнем 0..255 — лента будет «плясать».
    void musicBeat(uint8_t level) {
        uint32_t c = strip.Color(level, level / 2, 255 - level);
        for (int i = 0; i < NEOPIXEL_COUNT; ++i) strip.setPixelColor(i, c);
    }

#ifdef USE_SERVO
    void waveServo() {
        static int ang = 90, dir = 1;
        ang += dir * 5;
        if (ang > 150 || ang < 30) dir *= -1;
        servo.write(ang);
    }
#endif
};

static Leds leds;

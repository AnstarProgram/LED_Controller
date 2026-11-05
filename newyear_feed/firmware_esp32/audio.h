// ============================================================================
//  audio.h — ЗАДЕЛ ПОД КОЛОНКИ (I2S). Сейчас: тишина + заготовки.
//  Когда купите усилитель PAM8403/MAX98357A + динамик: подключаете к I2S-пинам
//  и раскомментируете AUDIO_ENABLE в config.h. Дальше лента сама будет брать
//  PCM из клипов (.bin содержит аудиофреймы после видео) или стримить музыку
//  с ПК по Wi-Fi (буферизация та же схема: качаем 5 минут, играем).
// ============================================================================
#pragma once
#include <Arduino.h>

#ifdef AUDIO_ENABLE
#include "driver/i2s.h"
#define I2S_NUM_PORT   I2S_NUM_0
#define PIN_I2S_BCLK   27
#define PIN_I2S_LRCK   26
#define PIN_I2S_DOUT   25
#define AUDIO_SAMPLE_RATE 16000

static void audioBegin() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_ALLOC_FLAG_DMA_INHERIT_DEFAULT,
        .dma_buf_count = 8, .dma_buf_len = 256, .use_apll = false };
    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_BCLK, .ws_io_num = PIN_I2S_LRCK,
        .data_out_num = PIN_I2S_DOUT, .data_in_num = I2S_PIN_NO_CHANGE };
    i2s_driver_install(I2S_NUM_PORT, &cfg, 0, nullptr);
    i2s_set_pin(I2S_NUM_PORT, &pins);
}
static void audioWrite(const int16_t *pcm, size_t samples) {
    size_t written;
    i2s_write(I2S_NUM_PORT, pcm, samples * 2, &written, portMAX_DELAY);
}
#else
static inline void audioBegin() {}
static inline void audioWrite(const int16_t *, size_t) {}
#endif

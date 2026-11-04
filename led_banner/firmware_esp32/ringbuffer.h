// ============================================================================
//  ringbuffer.h — кольцевой RAM-буфер кадров ленты (8x96, яркость 1 байт/пиксель)
//
//  Логика «5 минут / 5 секунд», которую вы описали:
//   1. Играем кадры из буфера (до ~5 минут).
//   2. Когда до конца текущих данных остаётся 5 секунд — лента сама просит у
//      ПК новые данные по Wi-Fi (состояние WAIT).
//   3. Последние 5 секунд старых кадров удаляются из буфера, освобождая место.
//   4. Если за FETCH_TIMEOUT_MS (5 мин) от ПК ничего не пришло — состояние
//      ERROR: на панели мигает «ОШИБКА СВЯЗИ», а воспроизводится последний
//      сохранившийся фрагмент (панель не гаснет).
// ============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

#define FRAME_PX_MAX (8 * 128)   // запас под конфигурации 8x96 / 8x128

enum BufState : uint8_t { BUF_EMPTY, BUF_PLAYING, BUF_WAIT, BUF_READY, BUF_ERROR };

class RingBuffer {
public:
  uint8_t* frames = nullptr;
  uint32_t cap = MAX_FRAMES;  // реальное число слотов (подбирается под память)
  uint32_t head = 0;        // следующий слот для записи принятых кадров
  uint32_t tail = 0;        // первый валидный кадр
  uint32_t playPos = 0;     // текущая позиция воспроизведения
  uint32_t oldEnd = 0;      // граница «старого» сегмента на момент запроса
  uint32_t framePx = 0;     // пикселей в кадре
  uint32_t frameMs = 100;   // длительность кадра, мс
  uint32_t fetchStartMs = 0;
  bool     fetching = false;
  BufState state = BUF_EMPTY;

  uint32_t available() const { return head - tail; }

  // Пытаемся выделить буфер на maxFrames кадров; при нехватке памяти уменьшаем
  // вдвое, но не меньше minFrames. Возвращает число реально выделенных слотов.
  uint32_t begin(uint32_t px, uint32_t maxFrames = MAX_FRAMES,
                 uint32_t minFrames = (uint32_t)(30 * TARGET_FPS)) {
    framePx = px;
    frameMs = 1000 / TARGET_FPS;
    uint32_t n = maxFrames;
    while (n >= minFrames) {
      size_t bytes = (size_t)n * px;
      frames = (uint8_t*)ps_malloc(bytes);           // PSRAM если есть...
      if (!frames) frames = (uint8_t*)malloc(bytes); // ...иначе обычная RAM
      if (frames) { cap = n; memset(frames, 0, bytes); break; }
      log_w("Нет памяти под %u кадров (%u КБ), пробуем вдвое меньше", n, (unsigned)(bytes/1024));
      if (n <= minFrames * 2) { n = 0; break; }
      n /= 2;
    }
    if (!frames) { log_e("Буфер не выделен!"); return 0; }
    log_i("RingBuffer: %u кадров (~%.0f c), %u КБ", cap,
          (float)cap * frameMs / 1000.f, (unsigned)((size_t)cap * px / 1024));
    return cap;
  }

  const uint8_t* frameAt(uint32_t n) const { return frames + (size_t)(n % cap) * framePx; }

  // Принять один новый кадр из сети (вызывает downloader/feeder)
  void appendFrame(const uint8_t* px) {
    if (available() >= cap) advanceTail(1); // защита от переполнения
    memcpy((void*)frameAt(head), px, framePx);
    head++;
    if (state == BUF_EMPTY || state == BUF_ERROR) state = BUF_PLAYING;
    if (fetching && leadFrames() >= PREBUFFER_FRAMES()) { fetching = false; state = BUF_READY; }
  }

  uint32_t PREBUFFER_FRAMES() const { return (uint32_t)PREBUFFER_SECONDS * 1000 / frameMs; }
  uint32_t LEAD_FRAMES()      const { return (uint32_t)FETCH_LEAD_SECONDS   * 1000 / frameMs; }
  uint32_t DELETE_FRAMES()    const { return (uint32_t)DELETE_LEAD_SECONDS  * 1000 / frameMs; }

  // Сколько кадров в запасе впереди точки воспроизведения (с учётом кольца)
  uint32_t leadFrames() const { return head >= playPos ? head - playPos : 0; }

  // Пора ли просить новые данные? (за 5 сек до конца старых)
  bool shouldRequest() const {
    if (head == 0) return !fetching;               // буфер пуст — просим сразу
    return leadFrames() <= LEAD_FRAMES() && !fetching;
  }

  void startFetch() {
    fetching = true;
    fetchStartMs = millis();
    oldEnd = head;                                  // фиксируем границу старого сегмента
    if (head > 0) state = BUF_WAIT;
  }

  // Полный сброс эфира (после смены плейлиста)
  void reset() { head = tail = playPos = oldEnd = 0; state = BUF_EMPTY; fetching = false; }

  // Вызывается каждые ~секунду из loop(): таймаут ожидания
  void tick() {
    if (fetching && millis() - fetchStartMs > FETCH_TIMEOUT_MS) {
      fetching = false;
      state = BUF_ERROR;                            // «ОШИБКА» на панели
      log_w("Нет ответа от ПК > %u мс -> ERROR", FETCH_TIMEOUT_MS);
    }
  }

  // Сдвинуть воспроизведение на кадр вперёд. Вернуть NULL, если играть нечего.
  const uint8_t* step() {
    if (playPos >= head) return nullptr;            // очередь пуста
    // Освобождение места: последние 5 секунд старых -> удаляем
    uint32_t leftOld = (oldEnd && playPos < oldEnd) ? (oldEnd - playPos) : 0;
    if (fetching && leftOld && leftOld <= DELETE_FRAMES()) {
      advanceTailTo(playPos + 1);                  // съеденные кадры больше не нужны
    }
    const uint8_t* p = frameAt(playPos);
    playPos++;
    if (state == BUF_ERROR || state == BUF_WAIT) { /* показываем, ждём сеть */ }
    else if (state != BUF_READY) state = BUF_PLAYING;
    return p;
  }

  // Последний доступный кадр (для зацикливания при ошибке связи)
  const uint8_t* lastFrame() {
    if (head == 0) return nullptr;
    return frameAt(head - 1);
  }

private:
  void advanceTail(uint32_t k) { tail += k; if (tail > playPos) playPos = tail; }
  void advanceTailTo(uint32_t pos) { if (pos > tail) tail = pos; }
};

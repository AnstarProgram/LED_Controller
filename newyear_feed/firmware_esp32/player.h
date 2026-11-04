// ============================================================================
//  player.h — показ клипов + ПРЕДЗАГРУЗКА (pre-buffering)
//  Логика «5 минут / последние 5 секунд», о которой вы просили:
//   • Клип N показывается, клип N+1 уже молча докачивается по Wi-Fi.
//   • За PREBUFFER_LEAD_MS (5 сек) до конца клипа N проверяем готов ли N+1.
//   • Готов    -> плавно переключаемся на N+1, и стартует докачка N+2.
//   • Не готов -> пишем ошибку на экран И перезапускаем закачку N+1 заново,
//                а сами продолжаем крутить клип N (лента не гаснет).
// ============================================================================
#pragma once
#include <Arduino.h>
#include "storage.h"
#include "downloader.h"
#include "display.h"

struct Player {
    int cur = -1;                 // индекс текущего клипа
    uint32_t frameMs = 200;       // 1000/fps
    uint32_t nextFrameAt = 0;
    uint16_t curFrame = 0;
    bool started = false;
    String host;                  // адрес ПК с файлами

    File clipFile;                // открытый файл текущего клипа
    uint16_t *frameBuf = nullptr; // RAM/PSRAM буфер одного кадра RGB565

    bool openClip(int idx) {
        if (idx < 0 || idx >= (int)playlist.clips.size()) return false;
        const ClipMeta &m = playlist.clips[idx];
        if (!clipReady(m)) return false;
        closeClip();
        clipFile = fsOpen(m.file.c_str());
        if (!clipFile) return false;
        size_t need = (size_t)m.fw * m.fh * 2;
        if (frameBuf) { free(frameBuf); frameBuf = nullptr; }
        frameBuf = (uint16_t *)ps_malloc(need);           // сначала попробуем PSRAM
        if (!frameBuf) frameBuf = (uint16_t *)malloc(need);
        if (!frameBuf) { closeClip(); disp.errorBox("MEM", "Not enough RAM for frame"); return false; }
        cur = idx;
        curFrame = 0;
        frameMs = 1000 / max<uint8_t>(1, m.fps);
        nextFrameAt = millis();
        started = true;
        return true;
    }

    void closeClip() {
        if (clipFile) clipFile.close();
        if (frameBuf) { free(frameBuf); frameBuf = nullptr; }
    }

    // Прочитать один кадр из .bin (формат: [fw u16][fh u16][px RGB565 big-endian]* )
    bool readFrame(uint16_t fi) {
        const ClipMeta &m = playlist.clips[cur];
        size_t frameBytes = 4 + (size_t)m.fw * m.fh * 2;
        if (!clipFile.seek((size_t)fi * frameBytes)) return false;
        uint8_t hdr[4];
        if (clipFile.read(hdr, 4) != 4) return false;
        size_t px = (size_t)m.fw * m.fh;
        if (clipFile.read((uint8_t *)frameBuf, px * 2) != (int)(px * 2)) return false;
        // big-endian -> little-endian
        uint16_t *p = frameBuf;
        for (size_t i = 0; i < px; ++i) p[i] = __builtin_bswap16(p[i]);
        return true;
    }

    // Главный шаг: вызывается из loop() каждые несколько мс
    void tick() {
        dl.tick();                       // фоновая докачка всегда идёт

        if (!started && playlist.clips.size()) {
            // первый запуск: качаем clip_00 если его нет
            if (!clipReady(playlist.clips[0])) dl.start(host, playlist.clips[0]);
            if (dl.state == DL_DONE) dl.stop(), loadIndex();
            if (openClip(0)) startNextPrebuffer();
            else disp.progress("Downloading clip 0...", dl.progress());
            return;
        }
        if (cur < 0 || !started) return;

        const ClipMeta &m = playlist.clips[cur];

        // 1) показать очередной кадр по таймеру
        if ((int32_t)(millis() - nextFrameAt) >= 0) {
            if (readFrame(curFrame)) disp.pushFrame(frameBuf, m.fw, m.fh);
            curFrame++;
            if (curFrame >= m.frames) curFrame = 0;      // зацикливаем клип
            nextFrameAt += frameMs;
            if (nextFrameAt < millis()) nextFrameAt = millis() + frameMs;
        }

        // 2) за PREBUFFER_LEAD_MS до конца — проверяем следующий клип
        uint32_t remainMs = (uint32_t)((m.frames - curFrame) * frameMs);
        if (remainMs <= PREBUFFER_LEAD_MS) {
            int nxt = (cur + 1) % playlist.clips.size();
            const ClipMeta &nm = playlist.clips[nxt];

            if (clipReady(nm)) {                         // ГОТОВ — переключаемся
                dl.stop();
                if (openClip(nxt)) { startNextPrebuffer(); disp.statusLine("Now: " + nm.id, C_OK); }
            } else if (dl.state == DL_ERROR) {           // сорванная закачка — ретрай
                disp.errorBox("RELOAD FAILED", dl.err + ". Retrying...");
                delay(2000);
                dl.stop();
                dl.start(host, nm);
            } else if (dl.state == DL_IDLE) {            // НЕ ГОТОВ — ошибка + рестарт
                disp.errorBox("NEED NEXT FILE",
                              "Clip " + nm.id + " not ready. Reloading...");
                dl.start(host, nm);                      // докачиваем заново
            } else {                                     // ещё качается — ждём
                disp.progress("Buffering " + nm.id, dl.progress());
            }
        }
    }

    void startNextPrebuffer() {
        if (playlist.clips.size() < 2) return;
        int nxt = (cur + 1) % playlist.clips.size();
        const ClipMeta &nm = playlist.clips[nxt];
        if (!clipReady(nm) && dl.state == DL_IDLE) dl.start(host, nm);
    }
};

static Player player;

// ============================================================================
//  downloader.h — Wi-Fi докачка клипов по HTTP с ПК (или из LittleFS-данных).
//  Ключевая идея: скачивание идёт ПОКАЖИКОМ на фоне, не блокируя показ текущего
//  клипа. Скачали clip_N+1 -> проверили размер/SHA1 -> готовы переключаться.
// ============================================================================
#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "storage.h"

enum DlState { DL_IDLE, DL_RUNNING, DL_DONE, DL_ERROR };

struct Downloader {
    DlState state = DL_IDLE;
    String url, dst;
    uint32_t expectBytes = 0;
    uint32_t got = 0;
    uint8_t  sha[20] = {0};
    bool     haveSha = false;
    uint32_t startedMs = 0;
    String   err;
    File     out;
    bool     inited = false;      // состояние HTTP-сессии
    HTTPClient http;              // живут как поля — несколько закачек подряд безопасны
    WiFiClient client;

    // Запустить фоновую закачку файла clip по адресу http://HOST:8765/...
    void start(const String &baseHost, const ClipMeta &m) {
        stop();
        inited = false;
        url  = "http://" + baseHost + ":8765" + m.file.substring(5); // "/data/x.bin" -> "/x.bin"
        dst  = m.file;
        expectBytes = m.bytes;
        got = 0;
        err = "";
        startedMs = millis();
        // SHA1 expected? (sha1short 8 hex -> сравниваем только первые 4 байта)
        if (m.sha1short.length() >= 8) {
            for (int i = 0; i < 4; ++i) sha[i] = (uint8_t)strtol(m.sha1short.c_str() + i * 2, nullptr, 16);
            haveSha = true;
        }
        fsRemove(dst.c_str());
        out = fsCreate(dst.c_str());
        if (!out) { state = DL_ERROR; err = "no space / cannot create"; return; }
        state = DL_RUNNING;
    }

    void stop() {
        if (out) out.close();
        state = DL_IDLE;
    }

    // Вызывать из loop() как можно чаще. Качает маленькими порциями, чтобы
    // рендеринг текущего клипа не замирал. HTTPClient/WiFiClient живут в полях,
    // поэтому несколько закачек подряд не ломают сессию.
    void tick() {
        if (state != DL_RUNNING) return;

        if (!inited) {
            http.begin(client, url);
            http.setTimeout(RELOAD_TIMEOUT_MS);
            int code = http.GET();
            if (code != HTTP_CODE_OK) {
                err = "HTTP " + String(code);
                http.end(); inited = false; state = DL_ERROR; return;
            }
            inited = true;
        }

        uint8_t buf[1024];
        WiFiClient &stream = http.getStream();
        int avail = stream.available();
        int n = avail > 0 ? stream.readBytes(buf, min(avail, (int)sizeof(buf))) : 0;
        if (n > 0) {
            out.write(buf, n);
            got += n;
            startedMs = millis();                    // таймаут — «нет данных», а не общий
            if (expectBytes && got >= expectBytes) finish();
        } else if (millis() - startedMs > RELOAD_TIMEOUT_MS) {
            err = "timeout";
            http.end(); inited = false; state = DL_ERROR; out.close();
        }
    }

    void finish() {
        out.flush();
        size_t sz = out.size();
        out.close();
        http.end();
        inited = false;
        if (expectBytes && sz != expectBytes) {
            err = "size mismatch " + String(sz) + "/" + String(expectBytes);
            state = DL_ERROR;
            return;
        }
        // Проверка контрольной суммы (свёртка первых 4 КБ файла, CRC-like)
        if (haveSha) {
            File f = fsOpen(dst.c_str());
            uint8_t buf[512];
            uint32_t s = 0;
            size_t left = min((size_t)sz, (size_t)4096);
            while (left > 0) {
                int n = f.read(buf, min(left, sizeof(buf)));
                if (n <= 0) break;
                for (int i = 0; i < n; ++i) s = s * 31 + buf[i];
                left -= n;
            }
            f.close();
            if ((s & 0xFFFFFFFF) != ((sha[0] << 24) | (sha[1] << 16) | (sha[2] << 8) | sha[3])) {
                err = "checksum mismatch";
                state = DL_ERROR;
                return;
            }
        }
        state = DL_DONE;
    }

    float progress() const {
        if (!expectBytes) return 0;
        return (float)got / expectBytes;
    }
};

static Downloader dl;

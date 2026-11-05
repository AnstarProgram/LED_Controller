// ============================================================================
//  storage.h — LittleFS (или SD) + индекс плейлиста index.json
//  Файлы клипов лежат в /data: clip_00.bin ... clip_NN.bin + index.json
// ============================================================================
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#if USE_SD_CARD
#include <SD.h>
#endif
#include "config.h"

struct ClipMeta {
    String id;        // "clip_00"
    String file;      // "/data/clip_00.bin"
    uint16_t fw = 0, fh = 0;   // размер кадра в пикселях
    uint8_t fps = 5;           // кадров в секунду (низкий FPS — экономим память)
    uint16_t frames = 0;       // сколько кадров внутри
    uint32_t bytes = 0;        // ожидаемый размер файла для сверки докачки
    String sha1short;          // первые 8 hex-символов SHA1 (контроль целостности)
};

struct Playlist {
    Vector<ClipMeta> clips;
    bool loaded = false;
};

static Playlist playlist;

static inline File fsOpen(const char *p) {
#if USE_SD_CARD
    return SD.open(p);
#else
    return LittleFS.open(p, "r");
#endif
}
static inline File fsCreate(const char *p) {
#if USE_SD_CARD
    return SD.open(p, FILE_WRITE);
#else
    return LittleFS.open(p, "w");
#endif
}
static inline bool fsExists(const char *p) {
#if USE_SD_CARD
    return SD.exists(p);
#else
    return LittleFS.exists(p);
#endif
}
static inline void fsRemove(const char *p) {
#if USE_SD_CARD
    SD.remove(p);
#else
    LittleFS.remove(p);
#endif
}
static inline size_t fsSize(const char *p) {
    File f = fsOpen(p);
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}

static bool storageBegin() {
#if USE_SD_CARD
    if (!SD.begin(SD_CS_PIN)) return false;
#else
    if (!LittleFS.begin(true)) return false;   // true = форматировать при ошибке
#endif
    return true;
}

// ---- очень простой парсер index.json (без внешних библиотек) ---------------
// Ожидаемый формат:
// {"clips":[{"id":"clip_00","file":"/data/clip_00.bin","fw":160,"fh":120,
//            "fps":5,"frames":900,"bytes":576000,"sha1short":"a1b2c3d4"}]}
static String jsonStr(const String &j, const String &key, int from = 0) {
    int k = j.indexOf("\"" + key + "\"", from);
    if (k < 0) return "";
    int c = j.indexOf(':', k);
    int q1 = j.indexOf('"', c + 1);
    // пропускаем пробелы между ':' и '"', но не заходим в следующее поле
    while (q1 >= 0 && q1 < (int)j.length() && (j[q1] == ' ' || j[q1] == '\n' || j[q1] == '\r' || j[q1] == '\t')) q1++;
    if (q1 < 0 || j[q1] != '"') return "";      // значение — не строка
    int q2 = j.indexOf('"', q1 + 1);
    if (q2 < 0) return "";
    return j.substring(q1 + 1, q2);
}
static long jsonNum(const String &j, const String &key, int from = 0) {
    int k = j.indexOf("\"" + key + "\"", from);
    if (k < 0) return -1;
    int c = j.indexOf(':', k);
    int e = c + 1;
    while (e < (int)j.length() && (isdigit(j[e]) || j[e] == ' ')) e++;
    String n = j.substring(c + 1, e);
    n.trim();
    return n.length() ? n.toInt() : -1;
}

static bool loadIndex() {
    playlist.clips.clear();
    File f = fsOpen(INDEX_FILE);
    if (!f) return false;
    String j = f.readString();
    f.close();

    int pos = 0;
    // ищем начало объекта клипа: "id" (совместимо с {"id"... и { "id"... )
    while ((pos = j.indexOf("\"id\"", pos)) >= 0) {
        int objStart = j.lastIndexOf('{', pos);
        if (objStart < 0) break;
        int end = j.indexOf('}', pos);
        if (end < 0) break;
        String obj = j.substring(objStart, end + 1);
        ClipMeta m;
        m.id        = jsonStr(obj, "id");
        m.file      = jsonStr(obj, "file");
        m.fw        = jsonNum(obj, "fw");
        m.fh        = jsonNum(obj, "fh");
        m.fps       = jsonNum(obj, "fps");
        m.frames    = jsonNum(obj, "frames");
        m.bytes     = jsonNum(obj, "bytes");
        m.sha1short = jsonStr(obj, "sha1short");
        if (m.id.length() && m.file.length()) playlist.clips.push_back(m);
        pos = end + 1;
    }
    playlist.loaded = playlist.clips.size() > 0;
    return playlist.loaded;
}

// Проверка: файл на месте и его размер совпадает с ожиданиями индекса
static bool clipReady(const ClipMeta &m) {
    size_t sz = fsSize(m.file.c_str());
    return sz > 0 && (m.bytes == 0 || sz == m.bytes);
}

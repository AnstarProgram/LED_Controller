// ============================================================================
//  playlist.h — плейлист текстов/эффектов, хранится в LittleFS (/playlist.json)
//  Плейлист можно редактировать:
//    * с ПК по Wi-Fi (главная страница / — мини-редактор),
//    * по Bluetooth (команды TEXT:, ADD:, DEL:, CLEAR:, SAVE:).
//  Каждый пункт = текст + эффект + скорость. Лента сама нарезает бегущую
//  строку на кадры и кладёт их в RAM-буфер (5 минут) без участия ПК.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <LittleFS.h>

struct Item { String text; int effect; int speed; };   // speed: px/cadrs(шаг за кадр)

class Playlist {
public:
  Vector<Item> items;

  bool load() {
    items.clear();
    File f = LittleFS.open(PLAYLIST_FILE, "r");
    if (!f) return false;
    String s = f.readString(); f.close();
    return parseJson(s);
  }

  bool save() {
    File f = LittleFS.open(PLAYLIST_FILE, "w");
    if (!f) return false;
    f.print(toJson()); f.close(); return true;
  }

  void add(const String& text, int effect, int speed) {
    items.push_back({text, constrain(effect,0,3), constrain(speed,1,4)});
  }
  void del(int i) { if (i >= 0 && i < (int)items.size()) items.erase(items.begin()+i); }
  void clearAll() { items.clear(); }

  String toJson() const {
    String s = "[";
    for (size_t i = 0; i < items.size(); i++) {
      if (i) s += ",";
      s += "{\"t\":\"" + esc(items[i].text) + "\",\"e\":" + String(items[i].effect)
         + ",\"s\":" + String(items[i].speed) + "}";
    }
    return s + "]";
  }

  bool parseJson(const String& s) {
    items.clear();
    int i = 0, n = s.length();
    while (i < n) {
      int o = s.indexOf("{\"t\":", i); if (o < 0) break;
      int q1 = s.indexOf('"', o + 5); if (q1 < 0) break;
      int q2 = q1 + 1;
      while (q2 < n) { if (s[q2] == '"' && s[q2-1] != '\\') break; q2++; }
      String t = unesc(s.substring(q1 + 1, q2));
      int e = 0, sp = 1;
      int pe = s.indexOf("\"e\":", q2); if (pe > 0 && pe < q2 + 60) e = s.substring(pe+4).toInt();
      int ps = s.indexOf("\"s\":", q2); if (ps > 0 && ps < q2 + 80) sp = s.substring(ps+4).toInt();
      add(t, e, sp);
      i = q2;
    }
    return !items.empty();
  }

private:
  static String esc(const String& s) {
    String r; for (size_t i = 0; i < s.length(); i++) { char c = s[i];
      if (c=='"'||c=='\\') { r += '\\'; r += c; } else if ((uint8_t)c >= 0x20 || (uint8_t)c==0) r += c; }
    return r;
  }
  static String unesc(const String& s) {
    String r; for (size_t i = 0; i < s.length(); i++) {
      if (s[i]=='\\' && i+1<s.length()) { i++; r += s[i]; } else r += s[i]; }
    return r;
  }
};

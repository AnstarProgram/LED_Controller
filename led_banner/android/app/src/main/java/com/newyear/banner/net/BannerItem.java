package com.newyear.banner.net;

/**
 * Модель одного пункта плейлиста ленты.
 * Совпадает с JSON-форматом ESP32:  {"t":"текст","e":эффект,"s":скорость}
 */
public class BannerItem {
    public String t;   // текст
    public int e;      // эффект: 0 белый, 1 гирлянда, 2 радуга, 3 снежный
    public int s;      // скорость прокрутки 1..4

    public BannerItem() {}
    public BannerItem(String text, int effect, int speed) {
        this.t = text; this.e = effect; this.s = speed;
    }
}

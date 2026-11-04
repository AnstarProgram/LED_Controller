package com.newyear.banner.net;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

/**
 * Клиент Wi-Fi API новогодней ленты (ESP32, порт 8765).
 * Все методы асинхронные — колбэк приходит в main-тред.
 *
 *   GET  /api/status      — состояние буфера
 *   GET  /api/playlist    — плейлист JSON
 *   POST /api/playlist    — задать плейлист [{t,e,s},...]
 *   POST /api/command     — команда как по Bluetooth: TEXT:, EFFECT:, BRIGHT:...
 */
public class BannerClient {

    public interface Cb {
        void onResult(String body);       // HTTP 200
        void onError(Exception e);        // сеть недоступна / таймаут / 4xx-5xx
    }

    private static final MediaType JSON = MediaType.get("application/json; charset=utf-8");
    private static final MediaType TEXT = MediaType.get("text/plain; charset=utf-8");

    private final OkHttpClient http;
    private String host;   // "192.168.1.55" или "ny-banner.local"
    private int port = 8765;

    public BannerClient() {
        http = new OkHttpClient.Builder()
                .connectTimeout(4, TimeUnit.SECONDS)
                .readTimeout(8, TimeUnit.SECONDS)
                .build();
    }

    public void setHost(String h) { this.host = h.trim(); }
    public void setHost(String h, int p) { this.host = h.trim(); this.port = p; }
    public String getHost() { return host == null ? "" : host; }
    public boolean hasHost() { return host != null && !host.isEmpty(); }

    private String url(String path) { return "http://" + host + ":" + port + path; }

    private void go(Request req, final Cb cb) {
        if (!hasHost()) { if (cb != null) cb.onError(new IOException("Лента не выбрана")); return; }
        http.newCall(req).enqueue(new Callback() {
            @Override public void onFailure(Call c, IOException e) {
                if (cb != null) cb.onError(e);
            }
            @Override public void onResponse(Call c, Response r) throws IOException {
                String b = r.body() != null ? r.body().string() : "";
                if (cb == null) return;
                if (r.isSuccessful()) cb.onResult(b);
                else cb.onError(new IOException("HTTP " + r.code()));
            }
        });
    }

    public void status(Cb cb)          { go(new Request.Builder().url(url("/api/status")).build(), cb); }
    public void getPlaylist(Cb cb)     { go(new Request.Builder().url(url("/api/playlist")).build(), cb); }
    public void getConfig(Cb cb)       { go(new Request.Builder().url(url("/api/config")).build(), cb); }

    /** Отправить список пунктов на ленту (полная замена эфира). */
    public void postPlaylist(String jsonItems, Cb cb) {
        go(new Request.Builder().url(url("/api/playlist"))
                .post(RequestBody.create(jsonItems, JSON)).build(), cb);
    }

    /** Текстовая команда: "TEXT:С Новым Годом!", "EFFECT:2", "BRIGHT:200", "SAVE"... */
    public void command(String cmd, Cb cb) {
        go(new Request.Builder().url(url("/api/command"))
                .post(RequestBody.create(cmd, TEXT)).build(), cb);
    }

    /** Показать текст немедленно (обёртка над командой TEXT:). */
    public void showText(String text, Cb cb) { command("TEXT:" + text, cb); }
}

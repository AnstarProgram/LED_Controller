package com.newyear.banner;

import android.app.Application;
import android.content.SharedPreferences;

import com.newyear.banner.net.BannerClient;
import com.newyear.banner.net.BannerDiscovery;
import com.newyear.banner.net.BtBannerLink;

/** Общий синглтон: один HTTP-клиент, один BT-канал, поиск лент, настройки. */
public class App extends Application {

    private static App inst;
    public BannerClient client;
    public BtBannerLink bt;
    public BannerDiscovery discovery;
    SharedPreferences prefs;

    public static App get() { return inst; }

    @Override public void onCreate() {
        super.onCreate();
        inst = this;
        prefs = getSharedPreferences("nybanner", MODE_PRIVATE);
        client = new BannerClient();
        String last = prefs.getString("host", "");
        if (!last.isEmpty()) client.setHost(last);
        discovery = new BannerDiscovery(this);
    }

    public void rememberHost(String host, int port) {
        client.setHost(host, port);
        prefs.edit().putString("host", host).putInt("port", port).apply();
    }

    /** Есть ли активное управление (Wi-Fi или Bluetooth). */
    public boolean hasLink() {
        return client.hasHost() || (bt != null && bt.isConnected());
    }

    /** Отправить команду через Wi-Fi; если Wi-Fi нет — через Bluetooth. */
    public void sendCommand(String cmd, com.newyear.banner.net.BannerClient.Cb cb) {
        if (client.hasHost()) {
            client.command(cmd, new com.newyear.banner.net.BannerClient.Cb() {
                @Override public void onResult(String body) { if (cb != null) cb.onResult(body); }
                @Override public void onError(Exception e) {
                    // Wi-Fi не ответил — пробуем Bluetooth
                    if (bt != null && bt.isConnected()) { bt.send(cmd); if (cb != null) cb.onResult("BT"); }
                    else if (cb != null) cb.onError(e);
                }
            });
        } else if (bt != null && bt.isConnected()) {
            bt.send(cmd);
            if (cb != null) cb.onResult("BT");
        } else if (cb != null) {
            cb.onError(new java.io.IOException("Нет подключения к ленте"));
        }
    }
}

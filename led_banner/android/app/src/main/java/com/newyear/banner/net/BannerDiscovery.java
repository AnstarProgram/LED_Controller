package com.newyear.banner.net;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Looper;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;

/**
 * Автопоиск ленты в Wi-Fi сети.
 * ESP32 каждые несколько секунд шлёт UDP-вещание на порт 8766 строкой:
 *     NYBANNER_DISCOVER ip=192.168.1.55 port=8765 name=NY-BANNER
 * Приложение держит MulticastLock и подставляет найденный адрес.
 */
public class BannerDiscovery {

    public static final int DISCOVERY_PORT = 8766;
    public static final String MAGIC = "NYBANNER_DISCOVER";

    public interface Found { void onFound(String ip, int port, String name); }

    private final Context ctx;
    private volatile boolean running;
    private DatagramSocket sock;
    private WifiManager.MulticastLock mlock;
    private final Handler main = new Handler(Looper.getMainLooper());

    public BannerDiscovery(Context c) { this.ctx = c; }

    public void start(final Found cb) {
        if (running) return;
        running = true;
        try {
            WifiManager wm = (WifiManager) ctx.getApplicationContext()
                    .getSystemService(Context.WIFI_SERVICE);
            if (wm != null) {
                mlock = wm.createMulticastLock("nybanner");
                mlock.setReferenceCounted(true);
                mlock.acquire();
            }
        } catch (Exception ignored) {}

        Thread t = new Thread(() -> {
            byte[] buf = new byte[256];
            try {
                sock = new DatagramSocket(null);
                sock.setReuseAddress(true);
                sock.bind(new java.net.InetSocketAddress(DISCOVERY_PORT));
                sock.setSoTimeout(3000);
                while (running) {
                    DatagramPacket p = new DatagramPacket(buf, buf.length);
                    try {
                        sock.receive(p);
                        String s = new String(p.getData(), 0, p.getLength(), StandardCharsets.UTF_8);
                        if (!s.startsWith(MAGIC)) continue;
                        final String fip = kv(s, "ip=").isEmpty()
                                ? p.getAddress().getHostAddress() : kv(s, "ip=");
                        final String fname = kv(s, "name=");
                        final int fport;
                        try { fport = Integer.parseInt(kv(s, "port=")); }
                        catch (NumberFormatException e) { continue; }
                        main.post(() -> cb.onFound(fip, fport, fname));
                    } catch (java.net.SocketTimeoutException e) {
                        // просто продолжаем слушать
                    }
                }
            } catch (Exception e) {
                // порт занят / нет прав — тихий провал, пользователь введёт IP вручную
            } finally {
                if (sock != null) sock.close();
                if (mlock != null) { try { mlock.release(); } catch (Exception ignored) {} }
            }
        }, "ny-discover");
        t.setDaemon(true);
        t.start();
    }

    public void stop() {
        running = false;
        if (sock != null) sock.close();
    }

    private static String kv(String s, String key) {
        int i = s.indexOf(key);
        if (i < 0) return "";
        int j = s.indexOf(' ', i);
        return (j < 0 ? s.substring(i + key.length()) : s.substring(i + key.length(), j)).trim();
    }
}

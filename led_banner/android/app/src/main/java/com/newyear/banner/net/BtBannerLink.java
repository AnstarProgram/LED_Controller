package com.newyear.banner.net;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Handler;
import android.os.Looper;

import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * Bluetooth Classic SPP — прямое управление лентой без роутера.
 * Имя устройства на ESP32: "NY-BANNER" (см. config.h / BT_DEVICE_NAME).
 * Команды те же, что по Wi-Fi: TEXT:, ADD:, EFFECT:, BRIGHT:, NEXT, SAVE, STATUS...
 */
public class BtBannerLink {

    // Стандартный UUID Serial Port Profile
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private static final String DEVICE_NAME = "NY-BANNER";

    public interface Listener {
        void onBtStatus(String msg);          // подключение/ошибка/обрыв
        void onBtLine(String line);           // ответ ленты (STATUS и "OK")
    }

    private final Handler main = new Handler(Looper.getMainLooper());
    private BluetoothSocket socket;
    private OutputStream out;
    private InputStream in;
    private Thread readerThread;
    private volatile boolean connected;
    private Listener listener;

    public void setListener(Listener l) { this.listener = l; }
    public boolean isConnected() { return connected; }

    /** Найти в сопряжённых устройствах ленту по имени. */
    @SuppressLint("MissingPermission") // вызывающий проверяет BLUETOOTH_CONNECT
    public BluetoothDevice findPaired(BluetoothAdapter adapter) {
        if (adapter == null) return null;
        Set<BluetoothDevice> paired = adapter.getBondedDevices();
        if (paired != null) {
            for (BluetoothDevice d : paired) {
                try {
                    String n = d.getName();
                    if (n != null && n.startsWith(DEVICE_NAME)) return d;
                } catch (SecurityException ignored) {}
            }
        }
        return null;
    }

    public List<String> pairedNames(BluetoothAdapter adapter) {
        List<String> names = new ArrayList<>();
        if (adapter == null) return names;
        try {
            Set<BluetoothDevice> paired = adapter.getBondedDevices();
            if (paired != null) for (BluetoothDevice d : paired) {
                try { names.add(d.getName() + "  [" + d.getAddress() + "]"); } catch (SecurityException ignored) {}
            }
        } catch (SecurityException ignored) {}
        return names;
    }

    private final BluetoothAdapter adapter;

    public BtBannerLink(BluetoothAdapter a) { this.adapter = a; }

    /** Подключиться в фоновом тредe (socket.connect блокирует). */
    @SuppressLint("MissingPermission")
    public void connect(final BluetoothDevice dev) {
        disconnect();
        new Thread(() -> {
            try {
                BluetoothSocket s = dev.createRfcommSocketToServiceRecord(SPP_UUID);
                if (adapter != null) adapter.cancelDiscovery();
                s.connect();
                socket = s;
                out = s.getOutputStream();
                in = s.getInputStream();
                connected = true;
                postStatus("Bluetooth: подключено к " + dev.getName());
                startReader();
            } catch (Exception e) {
                connected = false;
                postStatus("Bluetooth: не удалось подключиться (" + e.getMessage() + ")");
            }
        }, "ny-bt-connect").start();
    }

    private void startReader() {
        readerThread = new Thread(() -> {
            byte[] buf = new byte[256];
            StringBuilder sb = new StringBuilder();
            try {
                while (connected) {
                    int n = in.read(buf);
                    if (n < 0) break;
                    sb.append(new String(buf, 0, n, StandardCharsets.UTF_8));
                    int p;
                    while ((p = sb.indexOf("\n")) >= 0) {
                        String line = sb.substring(0, p).trim();
                        sb.delete(0, p + 1);
                        if (!line.isEmpty() && listener != null) {
                            final String L = line;
                            main.post(() -> listener.onBtLine(L));
                        }
                    }
                }
            } catch (Exception ignored) {
            } finally {
                connected = false;
                postStatus("Bluetooth: соединение закрыто");
            }
        }, "ny-bt-read");
        readerThread.setDaemon(true);
        readerThread.start();
    }

    /** Отправить команду (добавляется \n как терминатор строки). */
    public void send(String cmd) {
        if (!connected || out == null) { postStatus("Bluetooth: не подключено"); return; }
        new Thread(() -> {
            try {
                out.write((cmd + "\n").getBytes(StandardCharsets.UTF_8));
                out.flush();
            } catch (Exception e) {
                connected = false;
                postStatus("Bluetooth: ошибка отправки — " + e.getMessage());
            }
        }).start();
    }

    public void disconnect() {
        connected = false;
        try { if (in != null) in.close(); } catch (Exception ignored) {}
        try { if (out != null) out.close(); } catch (Exception ignored) {}
        try { if (socket != null) socket.close(); } catch (Exception ignored) {}
        in = null; out = null; socket = null;
    }

    private void postStatus(String m) {
        main.post(() -> { if (listener != null) listener.onBtStatus(m); });
    }
}

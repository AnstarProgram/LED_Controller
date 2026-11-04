package com.newyear.banner.net;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/** Ответ /api/status ленты — состояние буфера 5 минут и Wi-Fi. */
public class BannerStatus {
    public String state = "OFFLINE";   // PLAYING / WAIT_NET / READY / ERROR / EMPTY
    public double bufSec;              // сколько секунд эфира уже в памяти ленты
    public boolean wifi;
    public String ip = "";
    public boolean autonomous;         // true — играет свой сохранённый плейлист

    public static BannerStatus parse(String json) {
        BannerStatus st = new BannerStatus();
        try {
            JSONObject o = new JSONObject(json);
            st.state = o.optString("state", "?");
            st.bufSec = o.optDouble("buf_sec", 0);
            st.wifi = o.optBoolean("wifi", false);
            st.ip = o.optString("ip", "");
            st.autonomous = o.optBoolean("autonomous", false);
        } catch (Exception ignored) {}
        return st;
    }

    /** Человекочитаемая строка для статус-бара приложения. */
    public String human() {
        String mode = autonomous ? "автономно" : "поток с сервера";
        return state + " | буфер " + String.format("%.0f c", bufSec) + " | " + mode;
    }

    public static List<BannerItem> parsePlaylist(String json) {
        List<BannerItem> out = new ArrayList<>();
        try {
            JSONArray a = new JSONArray(json);
            for (int i = 0; i < a.length(); i++) {
                JSONObject o = a.getJSONObject(i);
                out.add(new BannerItem(o.optString("t", ""),
                        o.optInt("e", 0), o.optInt("s", 2)));
            }
        } catch (Exception ignored) {}
        return out;
    }

    public static String playlistToJson(List<BannerItem> items) {
        JSONArray a = new JSONArray();
        for (BannerItem it : items) {
            JSONObject o = new JSONObject();
            try {
                o.put("t", it.t); o.put("e", it.e); o.put("s", it.s);
            } catch (Exception ignored) {}
            a.put(o);
        }
        return "{\"items\":" + a + "}";
    }
}

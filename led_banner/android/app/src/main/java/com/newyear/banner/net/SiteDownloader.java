package com.newyear.banner.net;

import android.content.ContentValues;
import android.content.Context;
import android.os.Environment;
import android.provider.MediaStore;

import org.jsoup.Jsoup;
import org.jsoup.nodes.Document;
import org.jsoup.nodes.Element;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

/**
 * «Сайт для скачивания всех этих данных»: приложение умеет открывать
 * страницу с файлами (например, папку GitHub Releases или любой HTTP-листинг),
 * показывать список файлов (.bin прошивки, .json плейлисты, картинки)
 * и скачивать выбранный файл в папку Downloads на телефоне.
 */
public class SiteDownloader {

    public interface Cb {
        void onList(List<String> names, List<String> urls); // нашлись ссылки
        void onError(String msg);
        void onProgress(int percent);                        // -1 = готово
        void onDone(File savedTo, String displayName);
    }

    private final Context ctx;
    private final OkHttpClient http = new OkHttpClient.Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(60, TimeUnit.SECONDS)
            .followRedirects(true)
            .build();
    private volatile boolean cancelled;

    public SiteDownloader(Context c) { this.ctx = c; }
    public void cancel() { cancelled = true; }

    /** Получить список ссылок со страницы url (парсит <a href>). */
    public void fetchPage(final String url, final Cb cb) {
        cancelled = false;
        Request r = new Request.Builder().url(url).build();
        http.newCall(r).enqueue(new Callback() {
            @Override public void onFailure(Call c, java.io.IOException e) {
                fail(cb, "Не удалось открыть страницу: " + e.getMessage());
            }
            @Override public void onResponse(Call c, Response resp) throws java.io.IOException {
                String html = resp.body() != null ? resp.body().string() : "";
                Document doc = Jsoup.parse(html, url);
                List<String> names = new ArrayList<>(), urls = new ArrayList<>();
                for (Element a : doc.select("a[href]")) {
                    String href = a.absUrl("href");
                    if (href == null || href.isEmpty()) continue;
                    if (href.startsWith("javascript:") || href.startsWith("#")) continue;
                    String name = a.text().trim();
                    if (name.isEmpty()) name = href.substring(href.lastIndexOf('/') + 1);
                    // показываем только полезные файлы
                    String low = href.toLowerCase();
                    if (low.matches(".*\\.(bin|json|txt|ino|zip|png|jpg|hex|fw)$")) {
                        names.add(name); urls.add(href);
                    }
                }
                mainPost(() -> cb.onList(names, urls));
            }
        });
    }

    /** Скачать файл в системную папку Downloads (через MediaStore — без прав на Android 10+). */
    public void download(final String url, final String fileName, final Cb cb) {
        cancelled = false;
        Request r = new Request.Builder().url(url).build();
        http.newCall(r).enqueue(new Callback() {
            @Override public void onFailure(Call c, java.io.IOException e) {
                fail(cb, "Ошибка скачивания: " + e.getMessage());
            }
            @Override public void onResponse(Call c, Response resp) throws java.io.IOException {
                if (!resp.isSuccessful() || resp.body() == null) { fail(cb, "HTTP " + resp.code()); return; }
                long total = resp.body().contentLength();
                try (InputStream in = resp.body().byteStream()) {
                    ContentValues cv = new ContentValues();
                    cv.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
                    cv.put(MediaStore.Downloads.MIME_TYPE, "application/octet-stream");
                    android.net.Uri uri = null;
                    File fallback = null;
                    if (android.os.Build.VERSION.SDK_INT >= 29) {
                        cv.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/NYBanner");
                        uri = ctx.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
                    } else {
                        File dir = new File(ctx.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS), "NYBanner");
                        if (!dir.exists()) dir.mkdirs();
                        fallback = new File(dir, fileName);
                    }
                    java.io.OutputStream os = uri != null
                            ? ctx.getContentResolver().openOutputStream(uri)
                            : new FileOutputStream(fallback);
                    byte[] buf = new byte[8192];
                    long done = 0; int n;
                    while ((n = in.read(buf)) > 0) {
                        if (cancelled) { os.close(); fail(cb, "Скачивание отменено"); return; }
                        os.write(buf, 0, n);
                        done += n;
                        if (total > 0) {
                            final int pct = (int) (done * 100 / total);
                            mainPost(() -> cb.onProgress(pct));
                        }
                    }
                    os.close();
                    final File f = fallback; final String nm = fileName;
                    mainPost(() -> cb.onDone(f, nm));
                } catch (Exception e) {
                    fail(cb, "Сохранение не удалось: " + e.getMessage());
                }
            }
        });
    }

    private void fail(Cb cb, String m) { mainPost(() -> cb.onError(m)); }

    private static final android.os.Handler MAIN = new android.os.Handler(android.os.Looper.getMainLooper());
    private static void mainPost(Runnable r) { MAIN.post(r); }
}

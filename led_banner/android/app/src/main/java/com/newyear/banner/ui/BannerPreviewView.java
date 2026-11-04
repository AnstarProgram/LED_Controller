package com.newyear.banner.ui;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

import java.util.List;

import com.newyear.banner.net.BannerItem;

/**
 * Живое превью ленты 8x96: рисует бегущую строку точно так же,
 * как её отрендерит ESP32 (шрифт 5x7 + отступы), с выбором цвета по эффекту.
 */
public class BannerPreviewView extends View {

    private String text = "";
    private int effect = 1;           // 0 белый, 1 гирлянда, 2 радуга, 3 снежный
    private float speedPxPerSec = 16; // ~ скорость ленты
    private float offset = 0;
    private long lastMs = 0;
    private final Paint paint = new Paint();
    private boolean running = false;

    public BannerPreviewView(Context c, AttributeSet a) {
        super(c, a);
        paint.setAntiAlias(false);
    }

    public void setContent(String t, int eff) {
        this.text = (t == null ? "" : t);
        this.effect = eff;
        this.offset = 0;
        invalidate();
    }

    public void start() { running = true; lastMs = 0; postInvalidateOnAnimation(); }
    public void stop()  { running = false; }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        int W = getWidth(), H = getHeight();
        canvas.drawColor(Color.BLACK);
        if (W == 0 || H == 0) return;

        if (running) {
            long now = System.currentTimeMillis();
            if (lastMs != 0) offset += (now - lastMs) / 1000f * speedPxPerSec;
            lastMs = now;
        }

        // Масштаб: превью показывает виртуальные 96x8 пикселей ленты
        float sx = W / 96f, sy = H / 8f;
        float cell = Math.min(sx, sy);
        float ox = (W - 96 * cell) / 2f, oy = (H - 8 * cell) / 2f;

        // фон-сетка (как RGB-матрица)
        paint.setColor(Color.rgb(14, 14, 22));
        for (int x = 0; x < 96; x++)
            for (int y = 0; y < 8; y++)
                canvas.drawRect(ox + x * cell + 0.5f, oy + y * cell + 0.5f,
                        ox + (x + 1) * cell - 0.5f, oy + (y + 1) * cell - 0.5f, paint);

        // текст рисуем в «пиксельном» буфере 8x96 со сдвигом
        long[] buf = new long[96 * 8];   // биты i..i+19 — индексы символов (до 20 совпадений)
        String s = text + "   ";       // хвостовой пробел между циклами
        int totalW = s.length() * 6;   // 5px глиф + 1px межсимвольный
        int shift = ((int) offset % Math.max(totalW, 1));
        for (int cx = 0; cx < 96; cx++) {
            int px = cx + shift;                       // координата в «мире текста»
            int ci = totalW > 0 ? px % totalW : 0;
            if (ci < 0) ci += totalW;
            int chIdx = ci / 6, gx = ci % 6;
            if (chIdx >= s.length() || gx >= 5) continue;
            long glyph = Font5x7.glyph(s.charAt(chIdx));
            if (glyph == 0) continue;                  // нет глифа в шрифте
            for (int row = 0; row < 7; row++) {
                if (((glyph >> (row * 5)) & (1L << (4 - gx))) != 0) {
                    int y = 1 + row;                   // центрируем 7 строк в 8
                    if (y < 8 && chIdx < 20) buf[y * 96 + cx] |= 1L << chIdx;
                }
            }
        }

        long t = System.currentTimeMillis();
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 96; x++) {
                long v = buf[y * 96 + x];
                if (v == 0) continue;
                paint.setColor(colorFor(x, y, Long.numberOfTrailingZeros(v), t));
                canvas.drawRect(ox + x * cell + 0.8f, oy + y * cell + 0.8f,
                        ox + (x + 1) * cell - 0.8f, oy + (y + 1) * cell - 0.8f, paint);
            }
        }

        if (running) postInvalidateOnAnimation();
    }

    /** Цвета соответствуют эффектам на прошивке (banner.h). */
    private int colorFor(int x, int y, int chIdx, long ms) {
        switch (effect) {
            case 0: return Color.WHITE;
            case 1: { // гирлянда — мигающие разноцветные
                int[] pal = {0xFF3B3B, 0xFFD23B, 0x3BFF6E, 0x3BB4FF, 0xB43BFF};
                int k = (int) ((ms / 300) + chIdx) % pal.length;
                return pal[k];
            }
            case 2: { // радуга — градиент по колонкам, бежит
                float hue = ((x + ms / 20f) % 96) / 96f;
                return Color.HSVToColor(new float[]{hue * 360, 1, 1});
            }
            case 3: { // снежный — голубовато-белый мерцающий
                int r = 200 + ((int) ((x * 31 + y * 17 + ms / 500) % 55));
                return Color.rgb(r - 40, r, 255);
            }
            default: return Color.WHITE;
        }
    }

    /** Список пунктов -> показываем первый (лента сейчас играет его). */
    public void setPlaylist(List<BannerItem> items) {
        if (items != null && !items.isEmpty()) setContent(items.get(0).t, items.get(0).e);
    }
}

package com.newyear.banner;

import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import com.newyear.banner.net.BannerClient;
import com.newyear.banner.ui.DevicesFragment;
import com.newyear.banner.ui.EditorFragment;
import com.newyear.banner.ui.FilesFragment;

/**
 * Главный экран: три вкладки — Редактор, Устройства, Файлы.
 */
public class MainActivity extends AppCompatActivity {

    private static final int REQ_PERM = 100;
    @SuppressWarnings("deprecation")
    private int currentTab = -1;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViewById(R.id.tabEditor).setOnClickListener(x -> showTab(0));
        findViewById(R.id.tabDevices).setOnClickListener(x -> showTab(1));
        findViewById(R.id.tabFiles).setOnClickListener(x -> showTab(2));
        if (savedInstanceState == null) showTab(0);

        askRuntimePermissions();
    }

    /** Переключение вкладок без ViewPager — надёжно на всех версиях Android. */
    private void showTab(int i) {
        if (i == currentTab) return;
        currentTab = i;
        Fragment f;
        switch (i) {
            case 0:  f = new EditorFragment();  break;
            case 1:  f = new DevicesFragment(); break;
            default: f = new FilesFragment();   break;
        }
        getSupportFragmentManager().beginTransaction()
                .replace(R.id.pager, f)
                .commit();
        highlightTab(i);
    }

    private void highlightTab(int i) {
        int[] ids = {R.id.tabEditor, R.id.tabDevices, R.id.tabFiles};
        for (int k = 0; k < ids.length; k++) {
            android.view.View v = findViewById(ids[k]);
            v.setBackgroundColor(getResources().getColor(
                    k == i ? R.color.accent : R.color.card, getTheme()));
        }
    }

    /** BLUETOOTH_CONNECT нужен на Android 12+; WRITE_EXTERNAL_STORAGE — на Android 9-. */
    private void askRuntimePermissions() {
        try {
            if (Build.VERSION.SDK_INT >= 31) {
                requestPermissions(new String[]{"android.permission.BLUETOOTH_CONNECT"}, REQ_PERM);
            } else if (Build.VERSION.SDK_INT <= 28) {
                requestPermissions(new String[]{
                        "android.permission.WRITE_EXTERNAL_STORAGE"}, REQ_PERM);
            }
        } catch (SecurityException ignored) {}
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        final String cmd;
        if (id == R.id.menu_next) cmd = "NEXT";
        else if (id == R.id.menu_prev) cmd = "PREV";
        else if (id == R.id.menu_save) cmd = "SAVE";
        else if (id == R.id.menu_reboot) cmd = "REBOOT";
        else cmd = null;
        if (cmd != null) {
            App.get().sendCommand(cmd, new BannerClient.Cb() {
                @Override public void onResult(String body) { toast(getString(R.string.toast_sent) + ": " + cmd); }
                @Override public void onError(Exception e) { toast(getString(R.string.toast_error) + ": " + e.getMessage()); }
            });
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_SHORT).show(); }
}

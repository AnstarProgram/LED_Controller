package com.newyear.banner.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.newyear.banner.App;
import com.newyear.banner.R;
import com.newyear.banner.net.BannerClient;
import com.newyear.banner.net.SiteDownloader;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Вкладка «Файлы»: открываем сайт с файлами (HTTP-листинг / GitHub Releases),
 * выбираем файл -> скачиваем на телефон, а JSON-плейлист можно отправить прямо на ленту.
 */
public class FilesFragment extends Fragment {

    private EditText etUrl;
    private TextView tvDlState;
    private ListView lvFiles;
    private SiteDownloader dl;
    private final List<String> names = new ArrayList<>();
    private final List<String> urls = new ArrayList<>();
    private int selected = -1;

    @Nullable @Override
    public View onCreateView(@NonNull LayoutInflater in, @Nullable ViewGroup g, @Nullable Bundle b) {
        return in.inflate(R.layout.fragment_files, g, false);
    }

    @Override
    public void onViewCreated(@NonNull View v, @Nullable Bundle b) {
        etUrl = v.findViewById(R.id.etUrl);
        tvDlState = v.findViewById(R.id.tvDlState);
        lvFiles = v.findViewById(R.id.lvFiles);
        Button btnOpen = v.findViewById(R.id.btnOpenPage);
        Button btnDown = v.findViewById(R.id.btnDownload);
        Button btnSend = v.findViewById(R.id.btnSendFileToBanner);

        dl = new SiteDownloader(requireContext());
        lvFiles.setAdapter(new ArrayAdapter<>(requireContext(),
                android.R.layout.simple_list_item_1, names));
        lvFiles.setOnItemClickListener((p, view, pos, id) -> selected = pos);

        btnOpen.setOnClickListener(x -> {
            String u = etUrl.getText().toString().trim();
            if (u.isEmpty()) return;
            tvDlState.setText("Открываю страницу…");
            dl.fetchPage(u, new SiteDownloader.Cb() {
                @Override public void onList(List<String> n, List<String> us) {
                    post(() -> {
                        names.clear(); names.addAll(n);
                        urls.clear(); urls.addAll(us);
                        ((ArrayAdapter<?>) lvFiles.getAdapter()).notifyDataSetChanged();
                        tvDlState.setText("Найдено файлов: " + n.size());
                    });
                }
                @Override public void onError(String msg) { post(() -> tvDlState.setText(msg)); }
                @Override public void onProgress(int percent) {
                    post(() -> tvDlState.setText(getString(R.string.dl_progress) + " " + percent + "%"));
                }
                @Override public void onDone(File savedTo, String displayName) {
                    post(() -> tvDlState.setText(getString(R.string.dl_done) + ": " + savedTo.getAbsolutePath()));
                }
            });
        });

        btnDown.setOnClickListener(x -> {
            if (selected < 0 || selected >= urls.size()) { toast("Сначала выберите файл"); return; }
            dl.download(urls.get(selected), names.get(selected), new SiteDownloader.Cb() {
                @Override public void onList(List<String> n, List<String> us) {}
                @Override public void onError(String msg) { post(() -> tvDlState.setText(msg)); }
                @Override public void onProgress(int percent) {
                    post(() -> tvDlState.setText(getString(R.string.dl_progress) + " " + percent + "%"));
                }
                @Override public void onDone(File savedTo, String displayName) {
                    post(() -> tvDlState.setText(getString(R.string.dl_done) + ": " + savedTo.getAbsolutePath()));
                }
            });
        });

        btnSend.setOnClickListener(x -> {
            if (selected < 0) { toast("Сначала выберите файл"); return; }
            String name = names.get(selected);
            if (!name.toLowerCase().endsWith(".json")) {
                toast("На ленту можно отправлять только .json плейлисты");
                return;
            }
            File f = new File(requireContext().getExternalFilesDir(null), name);
            if (!f.exists()) { toast("Файл ещё не скачан — сначала «Скачать»"); return; }
            try {
                byte[] buf = new byte[(int) f.length()];
                java.io.FileInputStream fis = new java.io.FileInputStream(f);
                int n = fis.read(buf); fis.close();
                String json = new String(buf, 0, Math.max(n, 0), "UTF-8");
                App.get().client.postPlaylist(json, new BannerClient.Cb() {
                    @Override public void onResult(String body) { toast(getString(R.string.toast_sent)); }
                    @Override public void onError(Exception e) { toast(getString(R.string.toast_error) + ": " + e.getMessage()); }
                });
            } catch (Exception e) { toast("Ошибка чтения файла: " + e.getMessage()); }
        });
    }

    private void post(Runnable r) {
        if (getActivity() != null) getActivity().runOnUiThread(r);
    }

    private void toast(String s) {
        if (getActivity() != null) Toast.makeText(getActivity(), s, Toast.LENGTH_SHORT).show();
    }
}

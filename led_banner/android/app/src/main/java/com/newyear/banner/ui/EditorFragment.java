package com.newyear.banner.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.newyear.banner.App;
import com.newyear.banner.R;
import com.newyear.banner.net.BannerClient;
import com.newyear.banner.net.BannerItem;
import com.newyear.banner.net.BannerStatus;

import java.util.ArrayList;
import java.util.List;

/**
 * Мини-редактор: написал текст -> «Показать сейчас» или добавил в очередь,
 * затем можно отправить весь плейлист на ленту. Живое превью 1:1 с рендером ESP32.
 */
public class EditorFragment extends Fragment {

    private final List<BannerItem> queue = new ArrayList<>();
    private EditText etText;
    private Spinner spEffect, spSpeed;
    private BannerPreviewView preview;
    private QueueAdapter adapter;

    @Nullable @Override
    public View onCreateView(@NonNull LayoutInflater in, @Nullable ViewGroup g, @Nullable Bundle b) {
        return in.inflate(R.layout.fragment_editor, g, false);
    }

    @Override
    public void onViewCreated(@NonNull View v, @Nullable Bundle b) {
        etText = v.findViewById(R.id.etText);
        spEffect = v.findViewById(R.id.spEffect);
        spSpeed = v.findViewById(R.id.spSpeed);
        preview = v.findViewById(R.id.preview);
        Button btnShow = v.findViewById(R.id.btnShowNow);
        Button btnAdd = v.findViewById(R.id.btnAddQueue);
        Button btnSendPl = v.findViewById(R.id.btnSendPl);
        Button btnClearPl = v.findViewById(R.id.btnClearPl);
        RecyclerView rv = v.findViewById(R.id.rvQueue);

        ArrayAdapter<CharSequence> ad1 = ArrayAdapter.createFromResource(
                requireContext(), R.array.effects, android.R.layout.simple_spinner_item);
        ad1.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spEffect.setAdapter(ad1);
        ArrayAdapter<CharSequence> ad2 = ArrayAdapter.createFromResource(
                requireContext(), R.array.speeds, android.R.layout.simple_spinner_item);
        ad2.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spSpeed.setAdapter(ad2);

        adapter = new QueueAdapter();
        rv.setLayoutManager(new LinearLayoutManager(requireContext()));
        rv.setAdapter(adapter);

        etText.addTextChangedListener(new SimpleTextWatcher(s -> {
            preview.setContent(s, spEffect.getSelectedItemPosition());
        }));
        spEffect.setOnItemSelectedListener(new SimpleSelectListener(i -> {
            preview.setContent(etText.getText().toString(), i);
        }));

        preview.setContent("С Новым годом!", 1);
        preview.start();

        btnShow.setOnClickListener(x -> {
            String t = etText.getText().toString().trim();
            if (t.isEmpty()) return;
            App.get().sendCommand("TEXT:" + t, statusCb());
        });

        btnAdd.setOnClickListener(x -> {
            String t = etText.getText().toString().trim();
            if (t.isEmpty()) return;
            queue.add(new BannerItem(t, spEffect.getSelectedItemPosition(),
                    spSpeed.getSelectedItemPosition() + 1));
            adapter.notifyDataSetChanged();
        });

        btnSendPl.setOnClickListener(x -> {
            if (queue.isEmpty()) { toast(getString(R.string.no_items)); return; }
            App.get().client.postPlaylist(BannerStatus.playlistToJson(queue), statusCb());
        });

        btnClearPl.setOnClickListener(x -> {
            queue.clear();
            adapter.notifyDataSetChanged();
        });
    }

    @Override public void onResume() { super.onResume(); preview.start(); }
    @Override public void onPause()  { preview.stop(); super.onPause(); }

    private BannerClient.Cb statusCb() {
        return new BannerClient.Cb() {
            @Override public void onResult(String body) { toast(getString(R.string.toast_sent)); }
            @Override public void onError(Exception e) { toast(getString(R.string.toast_error) + ": " + e.getMessage()); }
        };
    }

    private void toast(String s) {
        if (getActivity() != null)
            android.widget.Toast.makeText(getActivity(), s, android.widget.Toast.LENGTH_SHORT).show();
    }

    /** Простая адаптер-строка очереди с кнопками ↑ ↓ ✕. */
    private class QueueAdapter extends RecyclerView.Adapter<QueueAdapter.VH> {
        @NonNull @Override
        public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View row = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_queue, parent, false);
            return new VH(row);
        }

        @Override
        public void onBindViewHolder(@NonNull VH h, int pos) {
            BannerItem it = queue.get(pos);
            h.tvNum.setText(String.valueOf(pos + 1));
            String[] eff = getResources().getStringArray(R.array.effects);
            h.tvText.setText(it.t + "  •  " + eff[Math.min(it.e, eff.length - 1)]);
            h.btnUp.setOnClickListener(x -> move(pos, pos - 1));
            h.btnDown.setOnClickListener(x -> move(pos, pos + 1));
            h.btnDel.setOnClickListener(x -> { queue.remove(pos); notifyDataSetChanged(); });
        }

        @Override public int getItemCount() { return queue.size(); }

        private void move(int from, int to) {
            if (to < 0 || to >= queue.size()) return;
            queue.add(to, queue.remove(from));
            notifyDataSetChanged();
        }

        class VH extends RecyclerView.ViewHolder {
            final android.widget.TextView tvNum, tvText;
            final android.widget.ImageButton btnUp, btnDown, btnDel;
            VH(View v) {
                super(v);
                tvNum = v.findViewById(R.id.tvNum);
                tvText = v.findViewById(R.id.tvText);
                btnUp = v.findViewById(R.id.btnUp);
                btnDown = v.findViewById(R.id.btnDown);
                btnDel = v.findViewById(R.id.btnDel);
            }
        }
    }
}

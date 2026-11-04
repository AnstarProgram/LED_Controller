package com.newyear.banner.ui;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.newyear.banner.App;
import com.newyear.banner.R;
import com.newyear.banner.net.BannerClient;
import com.newyear.banner.net.BannerStatus;
import com.newyear.banner.net.BtBannerLink;

/**
 * Вкладка «Устройства»: Wi-Fi (ручной IP + автопоиск UDP) и Bluetooth SPP.
 */
public class DevicesFragment extends Fragment {

    private EditText etIp;
    private LinearLayout deviceList;
    private Spinner spBt;
    private TextView tvBtState;
    private BtBannerLink bt;

    @Nullable @Override
    public View onCreateView(@NonNull LayoutInflater in, @Nullable ViewGroup g, @Nullable Bundle b) {
        return in.inflate(R.layout.fragment_devices, g, false);
    }

    @Override
    public void onViewCreated(@NonNull View v, @Nullable Bundle b) {
        etIp = v.findViewById(R.id.etIp);
        deviceList = v.findViewById(R.id.deviceList);
        spBt = v.findViewById(R.id.spBt);
        tvBtState = v.findViewById(R.id.tvBtState);
        Button btnConnect = v.findViewById(R.id.btnConnect);
        Button btnScan = v.findViewById(R.id.btnScan);
        Button btnBtC = v.findViewById(R.id.btnBtConnect);
        Button btnBtD = v.findViewById(R.id.btnBtDisconnect);

        String last = App.get().client.getHost();
        if (!last.isEmpty()) etIp.setText(last);

        btnConnect.setOnClickListener(x -> {
            String ip = etIp.getText().toString().trim();
            if (ip.isEmpty()) return;
            App.get().rememberHost(ip, 8765);
            checkStatus();
        });

        btnScan.setOnClickListener(x -> startDiscovery());

        // Bluetooth
        BluetoothAdapter adapter = getBtAdapter();
        if (adapter != null) {
            bt = new BtBannerLink(adapter);
            App.get().bt = bt;
            bt.setListener(new BtBannerLink.Listener() {
                @Override public void onBtStatus(String msg) { post(() -> tvBtState.setText(msg)); }
                @Override public void onBtLine(String line) { post(() -> tvBtState.setText("Лента: " + line)); }
            });
            refreshBtList();
        } else {
            tvBtState.setText(getString(R.string.dev_bt_hint));
        }

        btnBtC.setOnClickListener(x -> {
            if (bt == null || adapter == null) return;
            Object sel = spBt.getSelectedItem();
            if (sel == null) { toast("Список пуст"); return; }
            BluetoothAdapter ad = adapter;
            for (android.bluetooth.BluetoothDevice d : ad.getBondedDevices()) {
                if (d.getName() != null && d.getName().equals(sel.toString())) {
                    bt.connect(d);
                    return;
                }
            }
        });
        btnBtD.setOnClickListener(x -> { if (bt != null) bt.disconnect(); });
    }

    @SuppressLint("MissingPermission")
    private void refreshBtList() {
        try {
            BluetoothAdapter ad = getBtAdapter();
            java.util.List<String> names = new java.util.ArrayList<>();
            if (ad != null && ad.getBondedDevices() != null)
                for (android.bluetooth.BluetoothDevice d : ad.getBondedDevices())
                    if (d.getName() != null) names.add(d.getName());
            ArrayAdapter<String> a = new ArrayAdapter<>(requireContext(),
                    android.R.layout.simple_spinner_dropdown_item, names);
            spBt.setAdapter(a);
        } catch (SecurityException e) {
            toast(getString(R.string.perm_needed));
        }
    }

    @Nullable
    private BluetoothAdapter getBtAdapter() {
        try {
            BluetoothManager mgr = (BluetoothManager) requireContext()
                    .getSystemService(Context.BLUETOOTH_SERVICE);
            return mgr == null ? null : mgr.getAdapter();
        } catch (Throwable t) { return null; }
    }

    /** Автопоиск ленты в сети по UDP-вещанию ESP32. */
    private void startDiscovery() {
        deviceList.removeAllViews();
        addDeviceRow("Ищем ленту в Wi-Fi сети…", "");
        App.get().discovery.start((ip, port, name) ->
                post(() -> {
                    deviceList.removeAllViews();
                    addDeviceRow(name + "  (" + ip + ")", ip);
                    etIp.setText(ip);
                    App.get().rememberHost(ip, port);
                    checkStatus();
                }));
    }

    private void addDeviceRow(String label, String ip) {
        TextView tv = new TextView(requireContext());
        tv.setText(label);
        tv.setTextSize(15);
        tv.setPadding(0, 8, 0, 8);
        if (!ip.isEmpty()) tv.setOnClickListener(x -> {
            App.get().rememberHost(ip, 8765);
            checkStatus();
        });
        deviceList.addView(tv);
    }

    private void checkStatus() {
        App.get().client.status(new BannerClient.Cb() {
            @Override public void onResult(String body) {
                BannerStatus st = BannerStatus.parse(body);
                post(() -> toast(st.human()));
            }
            @Override public void onError(Exception e) {
                post(() -> toast(getString(R.string.status_offline) + ": " + e.getMessage()));
            }
        });
    }

    private void post(Runnable r) {
        if (getActivity() != null) getActivity().runOnUiThread(r);
    }

    private void toast(String s) {
        if (getActivity() != null) Toast.makeText(getActivity(), s, Toast.LENGTH_SHORT).show();
    }
}

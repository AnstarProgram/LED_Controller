package com.newyear.banner.ui;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

/** Простейший TextWatcher по лямбде. */
public class SimpleTextWatcher implements android.text.TextWatcher {
    public interface On { void text(String s); }
    private final On on;
    public SimpleTextWatcher(On o) { this.on = o; }
    @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
    @Override public void onTextChanged(CharSequence s, int a, int b, int c) { on.text(s.toString()); }
    @Override public void afterTextChanged(android.text.Editable s) {}
}

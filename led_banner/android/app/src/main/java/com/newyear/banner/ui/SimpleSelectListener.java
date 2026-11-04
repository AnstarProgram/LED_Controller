package com.newyear.banner.ui;

import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;

/** Простейший OnItemSelectedListener по лямбде (позиция выбранного элемента). */
public class SimpleSelectListener implements AdapterView.OnItemSelectedListener {
    public interface On { void selected(int position); }
    private final On on;
    public SimpleSelectListener(On o) { this.on = o; }
    @Override public void onItemSelected(AdapterView<?> parent, View view, int position, long id) { on.selected(position); }
    @Override public void onNothingSelected(AdapterView<?> parent) {}
}

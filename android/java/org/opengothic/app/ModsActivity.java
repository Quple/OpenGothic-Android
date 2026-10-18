package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * Mods before the game: a zip file is added here, a mod is chosen to start with the game.
 * In the game the mods are started, stopped and reset in the menu of the mods.
 */
public final class ModsActivity extends Activity {
    private static final int REQUEST_ADD = 2;

    private ModStore store;
    private LinearLayout list;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        File external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        store = new ModStore(external);
        SystemUi.hideBars(this);
        setContentView(buildUi());
        refresh();
    }

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(32), dp(20), dp(32), dp(20));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff071019, 0xff142e3e, 0xff080d14}));

        TextView title = label("Mods", 30, 0xfff1e7cd);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        page.addView(title);

        TextView hint = label("Mods are JavaScript. A switch chooses the mods that start with the game, "
                + "in the game they are managed with the menu of the mods.", 12, 0xff8fa5b0);
        hint.setPadding(0, 0, 0, dp(10));
        page.addView(hint);

        ScrollView scroll = new ScrollView(this);
        list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(list);
        page.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1.f));

        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setPadding(0, dp(10), 0, 0);

        Button add = button("Add a mod (.zip)", true);
        add.setOnClickListener(v -> pick());
        bar.addView(add, new LinearLayout.LayoutParams(0, dp(48), 1.f));

        Button back = button("Back", false);
        back.setOnClickListener(v -> finish());
        bar.addView(back, new LinearLayout.LayoutParams(0, dp(48), 1.f));

        page.addView(bar);
        return page;
    }

    private void refresh() {
        list.removeAllViews();
        List<ModStore.Mod> mods = store.list();
        if (mods.isEmpty()) {
            TextView none = label("There are no mods yet.\nAdd a zip file with a mod.", 16, 0xffc8d4da);
            none.setPadding(0, dp(20), 0, 0);
            list.addView(none);
            return;
        }
        for (ModStore.Mod mod : mods)
            list.addView(row(mod));
    }

    private View row(ModStore.Mod mod) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(14), dp(10), dp(14), dp(10));

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(0x33172530);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), 0xff425761);
        row.setBackground(shape);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.setMargins(0, 0, 0, dp(8));
        row.setLayoutParams(lp);

        LinearLayout text = new LinearLayout(this);
        text.setOrientation(LinearLayout.VERTICAL);
        String head = mod.name + (mod.version.isEmpty() ? "" : "  v" + mod.version);
        text.addView(label(head, 18, 0xfff1e7cd));
        String info = (mod.author.isEmpty() ? "" : "by " + mod.author + "   ") + mod.file.getName();
        text.addView(label(info, 12, 0xff8fa5b0));
        if (!mod.description.isEmpty())
            text.addView(label(mod.description, 13, 0xffc8d4da));
        row.addView(text, new LinearLayout.LayoutParams(0, -2, 1.f));

        Switch toggle = new Switch(this);
        toggle.setChecked(mod.enabled);
        toggle.setText("Start ");
        toggle.setTextColor(0xffc8d4da);
        toggle.setOnCheckedChangeListener((CompoundButton b, boolean on) -> store.setEnabled(mod, on));
        row.addView(toggle);

        Button remove = button("Remove", false);
        remove.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setTitle("Remove " + mod.name + "?")
                .setMessage("The file of the mod is deleted. Its saved data stays.")
                .setPositiveButton("Remove", (d, w) -> {
                    store.delete(mod);
                    refresh();
                })
                .setNegativeButton("Cancel", null)
                .show());
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(dp(110), dp(44));
        rp.setMargins(dp(14), 0, 0, 0);
        row.addView(remove, rp);
        return row;
    }

    private void pick() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.setType("*/*");
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(pick, REQUEST_ADD);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != REQUEST_ADD || result != RESULT_OK || data == null || data.getData() == null)
            return;
        try {
            ModStore.Mod mod = store.add(getContentResolver(), data.getData());
            Toast.makeText(this, "Added " + mod.name, Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
        }
        refresh();
    }

    // ----------------------------------------------------------------------------------------------------

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private TextView label(String text, int size, int color) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(color);
        return view;
    }

    private Button button(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(16);
        b.setTextColor(primary ? 0xff171c20 : 0xffe0e7e9);

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(primary ? 0xffd8c499 : 0x33172530);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), primary ? 0xffe3d4b2 : 0xff425761);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), shape, null));

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, dp(48));
        lp.setMargins(0, 0, dp(12), 0);
        b.setLayoutParams(lp);
        return b;
    }
}

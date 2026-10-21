package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Imports a ZIP of the user's own Gothic II installation and starts the game.
 * The files are copied to the app-specific storage, no storage permissions are needed.
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_IMPORT = 1;
    private static final int MAX_ENTRIES = 200000;
    private static final long MAX_SIZE = 20L * 1024 * 1024 * 1024;

    /** The games that can be installed next to each other. The folder of Gothic II is the one that older versions had. */
    private static final class GameDef {
        final String folder;
        final String title;
        final String subtitle;

        GameDef(String folder, String title, String subtitle) {
            this.folder = folder;
            this.title = title;
            this.subtitle = subtitle;
        }
    }

    private static final GameDef[] GAMES = {
        new GameDef("Gothic1", "Gothic", "Gothic 1"),
        new GameDef("Gothic2", "Gothic II", "Night of the Raven"),
        new GameDef("Myrtana", "The Chronicles of Myrtana", "Archolos"),
    };

    private TextView status;
    private Button start;
    private Button choose;
    private Button remove;
    private final View[] cards = new View[GAMES.length];
    private final TextView[] cardStates = new TextView[GAMES.length];
    private volatile boolean importing;
    private File external;
    private int selected = 1;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        File external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        this.external = external;
        selected = Math.max(0, Math.min(GAMES.length - 1, getSharedPreferences("launcher", MODE_PRIVATE).getInt("game", 1)));
        SystemUi.hideBars(this);
        setContentView(buildUi());
        refresh();
    }

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.HORIZONTAL);
        page.setPadding(dp(32), dp(24), dp(32), dp(24));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff071019, 0xff142e3e, 0xff080d14}));

        LinearLayout left = column(Gravity.CENTER_VERTICAL);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(left);
        page.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1.3f));

        TextView eyebrow = label("OPEN SOURCE  /  ANDROID", 12, 0xffa4afb4);
        eyebrow.setLetterSpacing(0.16f);
        left.addView(eyebrow);

        TextView title = label("OpenGothic", 42, 0xfff1e7cd);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        left.addView(title);

        TextView subtitle = label("Return to Khorinis", 16, 0xffabbcc4);
        subtitle.setPadding(0, 0, 0, dp(20));
        left.addView(subtitle);

        start = button("Play", true);
        start.setOnClickListener(v -> {
            // the game reads which of the installations it is (it has no command line)
            writeSelection();
            startActivity(new Intent(this, GameActivity.class));
        });
        left.addView(start);

        choose = button("Import game files", false);
        choose.setOnClickListener(v -> pickArchive());
        left.addView(choose);

        remove = button("Remove game files", false);
        remove.setOnClickListener(v -> askRemove());
        left.addView(remove);

        Button mods = button("Mods", false);
        mods.setOnClickListener(v -> startActivity(new Intent(this, ModsActivity.class)));
        left.addView(mods);

        Button steam = button("Steam", false);
        try {
            // the screen of Steam is not in a build that has no libraries (build-apk.sh)
            final Class<?> steamScreen = Class.forName("org.opengothic.app.SteamActivity");
            steam.setOnClickListener(v -> startActivity(new Intent(this, steamScreen)));
        } catch (ClassNotFoundException e) {
            steam.setVisibility(View.GONE);
        }
        left.addView(steam);

        Button help = button("How to start", false);
        help.setOnClickListener(v -> showHelp());
        left.addView(help);

        LinearLayout right = column(Gravity.CENTER_VERTICAL);
        right.setPadding(dp(40), 0, 0, 0);
        page.addView(right, new LinearLayout.LayoutParams(0, -1, 1.f));

        right.addView(label("YOUR GAMES", 12, 0xffa4afb4));
        for (int i = 0; i < GAMES.length; ++i) {
            final int index = i;
            LinearLayout card = column(Gravity.CENTER_VERTICAL);
            card.setPadding(dp(16), dp(10), dp(16), dp(10));
            card.setClickable(true);
            card.setOnClickListener(v -> select(index));

            TextView name = label(GAMES[i].title, 20, 0xffe9e0c8);
            name.setTypeface(Typeface.create("serif", Typeface.NORMAL));
            card.addView(name);
            card.addView(label(GAMES[i].subtitle, 12, 0xff8fa5b0));
            cardStates[i] = label("", 13, 0xff8fa5b0);
            card.addView(cardStates[i]);

            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
            lp.setMargins(0, dp(8), 0, 0);
            right.addView(card, lp);
            cards[i] = card;
        }

        status = label("", 15, 0xffc8d4da);
        status.setPadding(0, dp(16), 0, 0);
        right.addView(status);

        TextView note = label("Touch controls  •  Vulkan\nGraphics and controls can be adjusted in the game menu", 12, 0xff8fa5b0);
        note.setPadding(0, dp(24), 0, 0);
        right.addView(note);
        return page;
    }

    private LinearLayout column(int gravity) {
        LinearLayout ret = new LinearLayout(this);
        ret.setOrientation(LinearLayout.VERTICAL);
        ret.setGravity(gravity);
        return ret;
    }

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
        b.setTextSize(18);
        b.setGravity(Gravity.CENTER_VERTICAL | Gravity.LEFT);
        b.setPadding(dp(18), 0, dp(18), 0);
        b.setTextColor(new ColorStateList(
                new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}},
                new int[] {0xff62727b, primary ? 0xff171c20 : 0xffe0e7e9}));

        GradientDrawable shape = new GradientDrawable();
        shape.setColor(primary ? 0xffd8c499 : 0x33172530);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), primary ? 0xffe3d4b2 : 0xff425761);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), shape, null));

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(52));
        lp.setMargins(0, 0, dp(12), dp(9));
        b.setLayoutParams(lp);
        return b;
    }

    private void showHelp() {
        new AlertDialog.Builder(this)
                .setTitle("Getting started")
                .setMessage("1. Choose the game on the right: Gothic, Gothic II or The Chronicles of Myrtana. "
                        + "Every game has a place of its own, they can all be installed.\n\n"
                        + "2. Make a ZIP of your own installation of that game. "
                        + "It must contain the Data, _work and System folders.\n\n"
                        + "3. Choose Import game files. The files are copied once.\n\n"
                        + "4. Choose Play. Button size, camera sensitivity and graphics are in the settings of the in-game menu.\n\n"
                        + "The game files are not a part of the application.")
                .setPositiveButton("OK", null)
                .show();
    }

    private void pickArchive() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.setType("*/*");
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(pick, REQUEST_IMPORT);
    }

    private static boolean isInstallation(File dir) {
        return new File(dir, "Data").isDirectory()
                && new File(dir, "_work").isDirectory()
                && new File(dir, "System").isDirectory();
    }

    private File rootOf(int game) {
        return new File(external, GAMES[game].folder);
    }

    private void select(int game) {
        if (importing || game == selected)
            return;
        selected = game;
        getSharedPreferences("launcher", MODE_PRIVATE).edit().putInt("game", game).apply();
        refresh();
    }

    /** The name of the folder of the game that starts: the engine looks for it next to the files of the application. */
    private void writeSelection() {
        try (OutputStream out = new FileOutputStream(new File(external, "game.txt"))) {
            out.write(GAMES[selected].folder.getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // the engine takes Gothic II then
        }
    }

    private void askRemove() {
        if (importing)
            return;
        final int game = selected;
        new AlertDialog.Builder(this)
                .setTitle("Remove game files")
                .setMessage("The files of " + GAMES[game].title + " will be deleted from this phone. "
                        + "The saves are kept. You can import the game again later.")
                .setPositiveButton("Remove", (d, w) -> {
                    importing = true;
                    refresh();
                    new Thread(() -> {
                        remove(rootOf(game));
                        runOnUiThread(() -> {
                            importing = false;
                            refresh();
                        });
                    }, "game-remove").start();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void refresh() {
        boolean ready = isInstallation(rootOf(selected));
        start.setEnabled(ready && !importing);
        choose.setEnabled(!ready && !importing);
        choose.setVisibility(ready ? View.GONE : View.VISIBLE);
        remove.setEnabled(!importing);
        remove.setVisibility(ready ? View.VISIBLE : View.GONE);

        for (int i = 0; i < GAMES.length; ++i) {
            boolean installed = isInstallation(rootOf(i));
            cardStates[i].setText(installed ? "Installed" : "Not installed");
            cardStates[i].setTextColor(installed ? 0xff8fd19e : 0xff8fa5b0);

            GradientDrawable shape = new GradientDrawable();
            shape.setColor(i == selected ? 0x44304656 : 0x22172530);
            shape.setCornerRadius(dp(8));
            shape.setStroke(dp(i == selected ? 2 : 1), i == selected ? 0xffd8c499 : 0xff425761);
            cards[i].setBackground(shape);
        }

        String title = GAMES[selected].title;
        status.setText(ready
                ? "Ready to play\n" + title + " is installed."
                : "Add your game\nImport a ZIP archive with the files of " + title + ".");
    }

    @Override
    protected void onResume() {
        super.onResume();
        // a game may have been downloaded from Steam
        if (status != null && external != null)
            refresh();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != REQUEST_IMPORT || result != RESULT_OK || data == null || data.getData() == null)
            return;

        importing = true;
        start.setEnabled(false);
        choose.setEnabled(false);
        status.setText("Importing... Keep the application open.");

        Uri uri = data.getData();
        final File target = rootOf(selected);
        new Thread(() -> importArchive(uri, target), "game-import").start();
    }

    /** Unpacks next to the final folder, which is created only after the whole archive is valid. */
    private void importArchive(Uri uri, File root) {
        File staging = new File(root.getParentFile(), "import-staging");
        try {
            remove(staging);
            if (!staging.mkdirs())
                throw new IOException("Cannot create import directory");
            unzip(uri, staging);

            File source = findInstallation(staging);
            if (source == null)
                throw new IOException("Missing Data, _work or System. ZIP the complete game installation.");
            if (root.exists())
                throw new IOException("The folder of the game already exists; refusing to overwrite it");
            if (!source.renameTo(root))
                throw new IOException("Cannot finalize import");
            remove(staging);
            runOnUiThread(() -> {
                importing = false;
                refresh();
            });
        } catch (Exception error) {
            remove(staging);
            runOnUiThread(() -> {
                importing = false;
                refresh();
                status.setText("Import failed: " + error.getMessage());
            });
        }
    }

    private void unzip(Uri uri, File staging) throws IOException {
        String prefix = staging.getCanonicalPath() + File.separator;
        try (InputStream stream = getContentResolver().openInputStream(uri)) {
            if (stream == null)
                throw new IOException("Cannot open ZIP");
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(stream))) {
                byte[] buffer = new byte[131072];
                long total = 0;
                int count = 0;
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (++count > MAX_ENTRIES)
                        throw new IOException("Too many ZIP entries");
                    File out = new File(staging, entry.getName().replace('\\', '/'));
                    if (!out.getCanonicalPath().startsWith(prefix))
                        throw new IOException("Unsafe ZIP path");

                    if (entry.isDirectory()) {
                        makeDirs(out);
                        continue;
                    }
                    makeDirs(out.getParentFile());
                    try (OutputStream output = new BufferedOutputStream(new FileOutputStream(out))) {
                        int n;
                        while ((n = zip.read(buffer)) != -1) {
                            total += n;
                            if (total > MAX_SIZE)
                                throw new IOException("ZIP exceeds 20 GiB import limit");
                            output.write(buffer, 0, n);
                        }
                    }
                    if (count % 25 == 0) {
                        final long mb = total / (1024 * 1024);
                        runOnUiThread(() -> status.setText("Importing... " + mb + " MiB"));
                    }
                }
            }
        }
    }

    /** The installation is either at the root of the archive or in a single enclosing folder. */
    private static File findInstallation(File staging) throws IOException {
        if (isInstallation(staging))
            return staging;
        File found = null;
        File[] children = staging.listFiles();
        if (children == null)
            return null;
        for (File child : children) {
            if (!isInstallation(child))
                continue;
            if (found != null)
                throw new IOException("ZIP contains multiple installations");
            found = child;
        }
        return found;
    }

    private static void makeDirs(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("Cannot create folder");
    }

    private static void remove(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                remove(c);
        }
        if (f.exists())
            f.delete();
    }
}

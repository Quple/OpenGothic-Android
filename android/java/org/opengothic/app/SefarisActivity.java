package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * The mods of Gothic from https://sefaris.eu (Polish translations and mods): the list, the download and the play. It looks and works like
 * {@link SpineActivity}, and a downloaded mod is played and kept the same way (the store of {@link SpineStore}).
 *
 * Sefaris keeps no files of its own, only links. A file on Google Drive is read by the app itself; for the other links the page is
 * opened in the browser and the player gives the ZIP file (or the folder of an unpacked RAR or 7z archive) back.
 */
public final class SefarisActivity extends Activity {
    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;

    private static final int REQUEST_ZIP = 41;
    private static final int REQUEST_FOLDER = 42;
    private static final int REQUEST_PAGE = 43;

    private static final String[] GAME_FILTER_EN = {"All games", "Gothic", "Gothic II", "Downloaded"};
    private static final String[] GAME_FILTER_PL = {"Wszystkie gry", "Gothic", "Gothic II", "Pobrane"};
    // Sefaris has no field of languages: the tags tell the Polish dubbing and the translations (the team "Bractwo Spolszczenia")
    private static final String[] LANG_FILTER_EN = {"Any language", "Polish dubbing", "Translated to Polish"};
    private static final String[] LANG_FILTER_PL = {"Dowolny język", "Dubbing PL", "Spolszczenie"};
    private static final String[] SORT_EN = {"Newest", "Name"};
    private static final String[] SORT_PL = {"Najnowsze", "Nazwa"};

    private final Handler ui = new Handler(Looper.getMainLooper());
    private File external;
    private SpineStore store;

    private List<SefarisApi.Mod> all = new ArrayList<>();
    private final List<SefarisApi.Mod> shown = new ArrayList<>();
    private Map<Integer, SpineStore.Mod> installed = new HashMap<>();   // by the id in the store
    private Map<Integer, String> blocked = new HashMap<>();

    private String query = "";
    private int gameFilter;
    private int langFilter;
    private int sort;
    private boolean loading;
    private boolean resumed;
    private SefarisApi.Mod picking;   // the mod that a file, a folder or a page of download is chosen for

    private TextView status;
    private TextView gameChip;
    private TextView langChip;
    private TextView sortChip;
    private Adapter adapter;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        external = getExternalFilesDir(null);
        if (external == null) {
            finish();
            return;
        }
        store = new SpineStore(external);
        SystemUi.hideBars(this);
        setContentView(buildUi());
        reload();
        ui.postDelayed(this::tick, 700);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (external == null)
            return;
        resumed = true;
        SystemUi.hideBars(this);
        // the game is over: the files of the mod leave its folder
        if (!SpineActivity.gameRunning(this)) {
            SpineActivation.restore(store.root);
            SefarisReport.afterRun(this);   // what the engine said while the mod ran goes to the report of the mod
        }
        refreshState();
    }

    @Override
    protected void onPause() {
        resumed = false;
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    // ---- the list

    private void reload() {
        if (loading)
            return;
        loading = true;
        status.setText(Texts.t("Loading the list of mods...", "Wczytywanie listy modów..."));
        new Thread(() -> {
            List<SefarisApi.Mod> list = null;
            String error = null;
            try {
                list = SefarisApi.list();
            } catch (IOException e) {
                error = e.getMessage() != null ? e.getMessage() : e.toString();
            }
            final List<SefarisApi.Mod> result = list;
            final String failure = error;
            ui.post(() -> {
                loading = false;
                if (isFinishing())
                    return;
                if (result != null) {
                    all = result;
                    refreshState();
                } else {
                    status.setText(Texts.t("No connection with Sefaris: ", "Brak połączenia z Sefaris: ") + failure);
                    refreshState();
                }
            });
        }, "sefaris-list").start();
    }

    private void refreshState() {
        installed = new HashMap<>();
        for (SpineStore.Mod m : store.installed()) {
            if (m.id >= SefarisApi.ID_BASE)
                installed.put(m.id, m);
        }
        blocked = store.blocked();
        applyFilter();
    }

    private void applyFilter() {
        shown.clear();
        List<SefarisApi.Mod> source = new ArrayList<>(all);
        // a mod that is downloaded stays on the list when the list of the server is not there (no connection)
        Map<Integer, Boolean> known = new HashMap<>();
        for (SefarisApi.Mod m : all)
            known.put(m.storeId(), true);
        for (SpineStore.Mod m : installed.values()) {
            if (!known.containsKey(m.id))
                source.add(SefarisApi.of(m));
        }

        String q = query.trim().toLowerCase(Locale.ROOT);
        for (SefarisApi.Mod m : source) {
            if (gameFilter == 1 && m.game != SefarisApi.GOTHIC)
                continue;
            if (gameFilter == 2 && m.game != SefarisApi.GOTHIC2)
                continue;
            if (gameFilter == 3 && !installed.containsKey(m.storeId()))
                continue;
            if (langFilter == 1 && !m.hasTag("Dubbing PL"))
                continue;
            if (langFilter == 2 && !m.hasTag("Bractwo Spolszczenia"))
                continue;
            if (!q.isEmpty() && !(m.name.toLowerCase(Locale.ROOT).contains(q) || tags(m).toLowerCase(Locale.ROOT).contains(q)))
                continue;
            shown.add(m);
        }
        Collections.sort(shown, (a, b) -> {
            if (sort == 1)
                return a.name.compareToIgnoreCase(b.name);
            int byDate = b.updated.compareTo(a.updated);
            return byDate != 0 ? byDate : a.name.compareToIgnoreCase(b.name);
        });

        gameChip.setText((Texts.polish() ? GAME_FILTER_PL : GAME_FILTER_EN)[gameFilter]);
        langChip.setText((Texts.polish() ? LANG_FILTER_PL : LANG_FILTER_EN)[langFilter]);
        sortChip.setText((Texts.polish() ? SORT_PL : SORT_EN)[sort]);
        if (!loading)
            status.setText(shown.size() + " / " + source.size() + Texts.t(" mods", " modów"));
        if (adapter != null)
            adapter.notifyDataSetChanged();
    }

    private static String tags(SefarisApi.Mod m) {
        StringBuilder b = new StringBuilder();
        for (String t : m.tags) {
            if (b.length() > 0)
                b.append(", ");
            b.append(t);
        }
        return b.toString();
    }

    private void tick() {
        if (isFinishing())
            return;
        if (resumed && report())
            refreshState();   // a mod was installed, or found to need a library
        else if (SefarisService.state != null && adapter != null)
            adapter.notifyDataSetChanged();
        ui.postDelayed(this::tick, 700);
    }

    /** Tells once how the last installation ended. Returns true when it did. */
    private boolean report() {
        SefarisService.State s = SefarisService.state;
        if (s == null || s.running() || s.reported)
            return false;
        s.reported = true;
        if (s.library != null) {
            Toast.makeText(this, Texts.t(s.title + " needs " + s.library + " and cannot run in OpenGothic. Removed.",
                    s.title + " wymaga " + s.library + " i nie działa w OpenGothic. Usunięto."), Toast.LENGTH_LONG).show();
        } else if (s.error != null) {
            new AlertDialog.Builder(this).setTitle(s.title).setMessage(s.error).setPositiveButton("OK", null).show();
        } else if (s.done) {
            String text = Texts.t(s.title + " is ready to play.", s.title + " jest gotowy do gry.");
            if (s.plugins > 0)
                text += Texts.t(" " + s.plugins + " Windows plugins (.dll, Union) were left out: the mod may miss some features.",
                        " Pominięto " + s.plugins + " wtyczek Windows (.dll, Union): mod może działać niepełno.");
            Toast.makeText(this, text, Toast.LENGTH_LONG).show();
        }
        return true;
    }

    // ---- the actions

    /** The button "Download": the page of the mod is loaded (it has the links), then the player chooses the way. */
    private void download(final SefarisApi.Mod m) {
        if (SefarisService.running() != null) {
            Toast.makeText(this, Texts.t("Another mod is being installed.", "Trwa instalacja innego moda."), Toast.LENGTH_SHORT).show();
            return;
        }
        if (SpineActions.gameFor(this, external, m.game) < 0) {
            Toast.makeText(this, Texts.t("Install " + SpineActions.needs(m.game) + " first.", "Najpierw zainstaluj " + SpineActions.needs(m.game) + "."),
                    Toast.LENGTH_LONG).show();
            return;
        }
        withDetails(m, () -> showSources(m));
    }

    /** Loads the page of the mod and then runs the action on the screen; without a connection the action runs anyway. */
    private void withDetails(final SefarisApi.Mod m, final Runnable then) {
        if (m.detailed || m.externalId.isEmpty()) {
            then.run();
            return;
        }
        status.setText(Texts.t("Loading the mod...", "Wczytywanie moda..."));
        new Thread(() -> {
            try {
                SefarisApi.details(m);
            } catch (IOException ignored) {
                // the links are not known then: the player can still give a file
            }
            ui.post(() -> {
                if (isFinishing())
                    return;
                applyFilter();
                then.run();
            });
        }, "sefaris-page").start();
    }

    /** The ways to give the files: Google Drive by the app, the page in the browser, a ZIP file, a folder. */
    private void showSources(final SefarisApi.Mod m) {
        final List<String> labels = new ArrayList<>();
        final List<Runnable> actions = new ArrayList<>();
        final String drive = SefarisApi.driveFile(m);
        if (drive != null) {
            labels.add(Texts.t("Download (Google Drive)", "Pobierz (Google Drive)"));
            actions.add(() -> startInstall(m, SefarisService.SOURCE_URL, SefarisApi.driveUrl(drive), null, null));
        }
        final String mega = SefarisApi.megaLink(m);
        if (mega != null) {
            labels.add(Texts.t("Download (MEGA)", "Pobierz (MEGA)"));
            actions.add(() -> startInstall(m, SefarisService.SOURCE_MEGA, mega, null, null));
        }
        // the other pages (ModDB, MediaFire, folders of Google Drive): the page opens in the browser of the app, and the file that
        // starts to download there is taken by the app
        for (final SefarisApi.Link link : SefarisApi.pages(m)) {
            labels.add(Texts.t("Open the page: ", "Otwórz stronę: ") + SefarisApi.host(link.url));
            actions.add(() -> {
                picking = m;
                startActivityForResult(new Intent(this, SefarisWebActivity.class).putExtra(SefarisWebActivity.EXTRA_PAGE, link.url), REQUEST_PAGE);
            });
        }
        labels.add(Texts.t("I have the file (ZIP, RAR, 7z)", "Mam plik (ZIP, RAR, 7z)"));
        actions.add(() -> pick(m, false));
        labels.add(Texts.t("I have the unpacked folder", "Mam rozpakowany folder"));
        actions.add(() -> pick(m, true));

        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(20), dp(8), dp(20), dp(8));
        TextView hint = label(Texts.t(
                "The app downloads and unpacks ZIP, RAR and 7z archives itself. Installers (.exe) cannot be opened on the phone: run them on a computer and choose the folder.",
                "Aplikacja sama pobierze i rozpakuje archiwa ZIP, RAR i 7z. Instalatorów (.exe) telefon nie otworzy: uruchom je na komputerze i wskaż folder."),
                13, 0);
        hint.setPadding(0, 0, 0, dp(10));
        box.addView(hint);
        final AlertDialog[] dialog = new AlertDialog[1];
        for (int i = 0; i < labels.size(); i++) {
            final Runnable action = actions.get(i);
            Button b = new Button(this);
            b.setText(labels.get(i));
            b.setAllCaps(false);
            b.setOnClickListener(v -> {
                dialog[0].dismiss();
                action.run();
            });
            box.addView(b, new LinearLayout.LayoutParams(-1, -2));
        }
        ScrollView scroll = new ScrollView(this);
        scroll.addView(box);
        dialog[0] = new AlertDialog.Builder(this).setTitle(m.name).setView(scroll)
                .setNegativeButton(Texts.t("Cancel", "Anuluj"), null).create();
        dialog[0].show();
    }

    private void pick(SefarisApi.Mod m, boolean folder) {
        picking = m;
        Intent intent = folder ? new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
                : new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        try {
            startActivityForResult(intent, folder ? REQUEST_FOLDER : REQUEST_ZIP);
        } catch (ActivityNotFoundException e) {
            Toast.makeText(this, Texts.t("There is no file picker on this phone.", "Ten telefon nie ma wyboru plików."), Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        SefarisApi.Mod m = picking;
        picking = null;
        if (m == null || result != RESULT_OK || data == null || data.getData() == null)
            return;
        if (request == REQUEST_ZIP)
            startInstall(m, SefarisService.SOURCE_FILE, null, data.getData(), null);
        else if (request == REQUEST_FOLDER)
            startInstall(m, SefarisService.SOURCE_FOLDER, null, data.getData(), null);
        else if (request == REQUEST_PAGE)
            // the file that the page started to download, with what the browser of the app sent for it
            startInstall(m, SefarisService.SOURCE_URL, data.getDataString(), null, new String[] {
                    str(data, SefarisWebActivity.EXTRA_COOKIE), str(data, SefarisWebActivity.EXTRA_AGENT), str(data, SefarisWebActivity.EXTRA_REFERER)});
    }

    private static String str(Intent intent, String key) {
        String v = intent.getStringExtra(key);
        return v != null ? v : "";
    }

    private void startInstall(SefarisApi.Mod m, String source, String url, Uri uri, String[] web) {
        if (SefarisService.running() != null) {
            Toast.makeText(this, Texts.t("Another mod is being installed.", "Trwa instalacja innego moda."), Toast.LENGTH_SHORT).show();
            return;
        }
        if (Build.VERSION.SDK_INT >= 33)
            requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 1);
        SefarisService.state = null;
        Intent intent = SefarisService.intent(this, m, source, url, uri, web, SefarisReport.hints(m.installation + "\n" + m.overview));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            startForegroundService(intent);
        else
            startService(intent);
        ui.postDelayed(() -> adapter.notifyDataSetChanged(), 300);
    }

    /** The page of the mod: the description and the guide of the authors, with the buttons of the row. */
    private void details(final SefarisApi.Mod m) {
        withDetails(m, () -> {
            StringBuilder html = new StringBuilder(m.overview);
            if (!m.installation.isEmpty())
                html.append("<br>").append(m.installation);
            TextView text = label("", 14, 0);
            text.setText(SefarisApi.text(html.toString()));
            text.setMovementMethod(LinkMovementMethod.getInstance());
            text.setPadding(dp(20), dp(8), dp(20), dp(8));
            ScrollView scroll = new ScrollView(this);
            scroll.addView(text);
            AlertDialog.Builder b = new AlertDialog.Builder(this).setTitle(m.name).setView(scroll)
                    .setNegativeButton(Texts.t("Close", "Zamknij"), null);
            final SpineStore.Mod mod = installed.get(m.storeId());
            if (mod != null) {
                b.setPositiveButton(Texts.t("Play", "Graj"), (d, w) -> SpineActions.play(this, store, mod));
                b.setNeutralButton(Texts.t("Remove", "Usuń"), (d, w) -> SpineActions.remove(this, store, mod, this::refreshState));
            } else if (!blocked.containsKey(m.storeId())) {
                b.setPositiveButton(Texts.t("Download", "Pobierz"), (d, w) -> download(m));
            }
            b.show();
        });
    }

    // ---- the screen

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(24), dp(14), dp(24), dp(14));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0e2130, 0xff060c13}));

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = label("Sefaris", 26, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        top.addView(title);
        status = label("", 13, GREY);
        status.setPadding(dp(14), 0, 0, 0);
        top.addView(status, new LinearLayout.LayoutParams(0, -2, 1.f));
        // the saves of the downloaded mods: the same screen as for Spine
        Button saves = button(Texts.t("Saves", "Zapisy"));
        saves.setOnClickListener(v -> startActivity(new Intent(this, SpineSavesActivity.class)));
        LinearLayout.LayoutParams savesParams = new LinearLayout.LayoutParams(dp(110), dp(40));
        savesParams.setMargins(0, 0, dp(8), 0);
        top.addView(saves, savesParams);
        // what each mod left out, what it needs, what the engine said while it ran: one text for all the mods
        Button reports = button(Texts.t("Report", "Raport"));
        reports.setOnClickListener(v -> ReportDialog.show(this));
        LinearLayout.LayoutParams reportParams = new LinearLayout.LayoutParams(dp(100), dp(40));
        reportParams.setMargins(0, 0, dp(8), 0);
        top.addView(reports, reportParams);
        Button back = button(Texts.t("Back", "Wróć"));
        back.setOnClickListener(v -> finish());
        top.addView(back, new LinearLayout.LayoutParams(dp(90), dp(40)));
        page.addView(top);

        LinearLayout filters = new LinearLayout(this);
        filters.setGravity(Gravity.CENTER_VERTICAL);
        filters.setPadding(0, dp(8), 0, dp(8));
        EditText search = new EditText(this);
        search.setSingleLine(true);
        search.setHint(Texts.t("Search mods", "Szukaj modów"));
        search.setHintTextColor(0xff667a86);
        search.setTextColor(CREAM);
        search.setTextSize(15);
        search.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));
        search.setPadding(dp(12), 0, dp(12), 0);
        search.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {
            }

            @Override
            public void onTextChanged(CharSequence s, int a, int b, int c) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                query = s.toString();
                applyFilter();
            }
        });
        filters.addView(search, new LinearLayout.LayoutParams(0, dp(40), 1.f));

        gameChip = chip();
        gameChip.setOnClickListener(v -> {
            gameFilter = (gameFilter + 1) % 4;
            applyFilter();
        });
        LinearLayout.LayoutParams chipParams = new LinearLayout.LayoutParams(-2, dp(36));
        chipParams.setMargins(dp(8), 0, 0, 0);
        filters.addView(gameChip, chipParams);
        langChip = chip();
        langChip.setOnClickListener(v -> {
            langFilter = (langFilter + 1) % LANG_FILTER_EN.length;
            applyFilter();
        });
        filters.addView(langChip, chipParams);
        sortChip = chip();
        sortChip.setOnClickListener(v -> {
            sort = (sort + 1) % SORT_EN.length;
            applyFilter();
        });
        filters.addView(sortChip, chipParams);
        page.addView(filters);

        ListView list = new ListView(this);
        list.setDivider(null);
        list.setSelector(android.R.color.transparent);
        adapter = new Adapter();
        list.setAdapter(adapter);
        list.setOnItemClickListener((parent, view, position, id) -> details(shown.get(position)));
        page.addView(list, new LinearLayout.LayoutParams(-1, 0, 1.f));
        return page;
    }

    private final class Adapter extends BaseAdapter {
        @Override
        public int getCount() {
            return shown.size();
        }

        @Override
        public Object getItem(int position) {
            return shown.get(position);
        }

        @Override
        public long getItemId(int position) {
            return shown.get(position).id;
        }

        @Override
        public View getView(int position, View convert, ViewGroup parent) {
            final SefarisApi.Mod m = shown.get(position);
            LinearLayout row = new LinearLayout(SefarisActivity.this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(14), dp(8), dp(14), dp(8));
            row.setBackground(round(0x33172530, 0xff425761, dp(1), dp(8)));

            LinearLayout text = new LinearLayout(SefarisActivity.this);
            text.setOrientation(LinearLayout.VERTICAL);
            final SpineStore.Mod mod = installed.get(m.storeId());
            final SefarisService.State s = SefarisService.state;
            boolean active = s != null && s.id == m.storeId() && s.running();
            boolean isBlocked = blocked.containsKey(m.storeId());
            text.addView(label(m.name + (m.version.isEmpty() ? "" : "  v" + m.version), 17, CREAM));

            StringBuilder sub = new StringBuilder(m.game == SefarisApi.GOTHIC ? "Gothic" : "Gothic II");
            if (!m.tags.isEmpty())
                sub.append("  ·  ").append(tags(m));
            if (mod != null && !mod.version.equals(m.version) && !m.version.isEmpty())
                sub.append("  ·  ").append(Texts.t("installed v", "zainstalowano v")).append(mod.version);
            if (active)
                sub = new StringBuilder((s.unpacking ? Texts.t("Unpacking: ", "Rozpakowywanie: ") : "")
                        + (s.percent() >= 0 ? Downloads.size(s.bytes) + " / " + Downloads.size(s.total) : Downloads.size(s.bytes)));
            else if (isBlocked)
                sub = new StringBuilder(Texts.t("Needs " + blocked.get(m.storeId()) + ", OpenGothic cannot run it",
                        "Wymaga " + blocked.get(m.storeId()) + ", OpenGothic go nie uruchomi"));
            TextView subtitle = label(sub.toString(), 12, isBlocked ? 0xffd08a6a : GREY);
            subtitle.setSingleLine(true);
            text.addView(subtitle);
            row.addView(text, new LinearLayout.LayoutParams(0, -2, 1.f));

            Button action = button("");
            action.setFocusable(false);
            boolean enabled = true;
            if (active) {
                action.setText((s.percent() >= 0 ? s.percent() + "%" : Downloads.size(s.bytes)) + "  ✕");
                action.setOnClickListener(v -> s.cancelled = true);
            } else if (mod != null) {
                action.setText(Texts.t("Play", "Graj"));
                action.setBackground(round(0xffd8c499, 0xffe3d4b2, dp(1), dp(8)));
                action.setTextColor(0xff171c20);
                action.setOnClickListener(v -> SpineActions.play(SefarisActivity.this, store, mod));
            } else if (isBlocked) {
                action.setText(Texts.t("Unsupported", "Nieobsługiwany"));
                enabled = false;
            } else if (SpineActions.gameFor(SefarisActivity.this, external, m.game) < 0) {
                action.setText(Texts.t("Needs ", "Wymaga ") + SpineActions.needs(m.game));
                enabled = false;
            } else {
                action.setText(Texts.t("Download", "Pobierz"));
                action.setOnClickListener(v -> download(m));
            }
            action.setEnabled(enabled);
            action.setAlpha(enabled ? 1.f : 0.5f);
            LinearLayout.LayoutParams ap = new LinearLayout.LayoutParams(dp(130), dp(40));
            ap.setMargins(dp(12), 0, 0, 0);
            row.addView(action, ap);

            // the space between the rows: a ListView does not take margins
            FrameLayout wrap = new FrameLayout(SefarisActivity.this);
            wrap.setPadding(0, 0, 0, dp(8));
            wrap.addView(row);
            return wrap;
        }
    }

    // ----------------------------------------------------------------------------------------------------

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private TextView label(String text, int size, int color) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        if (color != 0)
            view.setTextColor(color);   // 0: the color of the theme (the texts of the dialogs)
        return view;
    }

    private GradientDrawable round(int fill, int stroke, int strokeWidth, int radius) {
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(fill);
        shape.setCornerRadius(radius);
        shape.setStroke(strokeWidth, stroke);
        return shape;
    }

    private TextView chip() {
        TextView v = label("", 13, 0xffe0e7e9);
        v.setGravity(Gravity.CENTER);
        v.setPadding(dp(12), 0, dp(12), 0);
        v.setBackground(round(0x33172530, 0xff425761, dp(1), dp(20)));
        return v;
    }

    private Button button(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(14);
        b.setTextColor(0xffe0e7e9);
        b.setPadding(dp(6), 0, dp(6), 0);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), round(0x33172530, 0xff425761, dp(1), dp(8)), null));
        return b;
    }
}

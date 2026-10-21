package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CompletableFuture;

/**
 * The account of Steam: the login (the password, Steam Guard), the session that is kept, the games of the account that this
 * application knows and the download of them to the places of the installations of the launcher.
 */
// The screen does nothing on Android 7 and older (see onCreate): the library needs Android 8
@TargetApi(26)
public final class SteamActivity extends Activity {
    private static final class Game {
        final String name;
        final String detail;
        final int appId;
        final String folder;

        Game(String name, String detail, int appId, String folder) {
            this.name = name;
            this.detail = detail;
            this.appId = appId;
            this.folder = folder;
        }
    }

    // app ids of Steam: Gothic 1 Classic, Gothic II: Gold Edition (with Night of the Raven), The Chronicles of Myrtana: Archolos;
    // the folders are those of the launcher
    private static final Game[] GAMES = {
        new Game("Gothic", "Gothic 1 Classic", 65540, "Gothic1"),
        new Game("Gothic II", "Gold Edition, with Night of the Raven", 39510, "Gothic2"),
        new Game("The Chronicles of Myrtana", "Archolos", 1467450, "Myrtana"),
    };

    private static final int GREEN = 0xff8fd19e;
    private static final int GREY = 0xff8fa5b0;
    private static final int RED = 0xffe29a8c;

    private SteamStore store;
    private LinearLayout form;
    private TextView message;
    private final TextView[] states = new TextView[GAMES.length];
    private final Button[] actions = new Button[GAMES.length];
    private final ProgressBar[] bars = new ProgressBar[GAMES.length];

    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean closed;
    private volatile int generation;        // a login that is not the last one is dropped
    private volatile SteamSession login;    // the connection of the login that goes on
    private Set<Integer> owned;             // null: not known
    private String typedAccount = "";
    private File external;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        external = getExternalFilesDir(null);
        store = new SteamStore(this);
        SystemUi.hideBars(this);
        setContentView(buildUi());

        if (Build.VERSION.SDK_INT < 26 || external == null) {
            form.removeAllViews();
            setMessage("The login to Steam needs Android 8 or newer.", RED);
            form.addView(button("Back", false, v -> finish()));
            return;
        }

        SteamStore.Session session = store.load();
        if (session != null) {
            showAccount(session.account);
            checkGames(session);
        } else {
            showLogin("");
        }
        ui.postDelayed(this::tick, 700);
    }

    @Override
    protected void onDestroy() {
        closed = true;
        ++generation;
        SteamSession s = login;
        if (s != null)
            s.close();
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    // ---- the screens

    private View buildUi() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.HORIZONTAL);
        page.setPadding(dp(32), dp(20), dp(32), dp(20));
        page.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff071019, 0xff142e3e, 0xff080d14}));

        LinearLayout left = column();
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(left);
        page.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1.f));

        TextView title = label("Steam", 30, 0xfff1e7cd);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        left.addView(title);
        TextView hint = label("Sign in to see which of the games you own and to download them. The password goes to Steam only, "
                + "it is not kept: the application keeps the session.", 13, 0xffa4afb4);
        hint.setPadding(0, dp(4), 0, dp(14));
        left.addView(hint);

        form = column();
        left.addView(form);

        message = label("", 14, RED);
        message.setPadding(0, dp(8), 0, 0);
        left.addView(message);

        LinearLayout rightColumn = column();
        ScrollView rightScroll = new ScrollView(this);
        rightScroll.setFillViewport(true);
        rightScroll.addView(rightColumn);
        page.addView(rightScroll, new LinearLayout.LayoutParams(0, -1, 1.2f));
        rightColumn.setGravity(Gravity.CENTER_VERTICAL);
        rightColumn.setPadding(dp(40), 0, 0, 0);

        rightColumn.addView(label("GAMES", 12, 0xffa4afb4));
        for (int i = 0; i < GAMES.length; ++i) {
            final int index = i;
            LinearLayout row = column();
            row.setPadding(0, dp(12), 0, dp(12));

            TextView name = label(GAMES[i].name, 20, 0xffe9e0c8);
            name.setTypeface(Typeface.create("serif", Typeface.NORMAL));
            row.addView(name);
            row.addView(label(GAMES[i].detail, 12, 0xff8fa5b0));
            states[i] = label("", 14, GREY);
            states[i].setPadding(0, dp(2), 0, dp(4));
            row.addView(states[i]);

            bars[i] = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
            bars[i].setMax(1000);
            bars[i].setVisibility(View.GONE);
            row.addView(bars[i], new LinearLayout.LayoutParams(-1, dp(8)));

            actions[i] = button("", false);
            actions[i].setVisibility(View.GONE);
            actions[i].setOnClickListener(v -> onAction(index));
            row.addView(actions[i]);
            rightColumn.addView(row);

            View line = new View(this);
            line.setBackgroundColor(0xff2b3b46);
            rightColumn.addView(line, new LinearLayout.LayoutParams(-1, dp(1)));
        }
        showOwned();
        return page;
    }

    private void showLogin(String error) {
        form.removeAllViews();
        setMessage(error, RED);

        final EditText account = field("Account name", false);
        account.setText(typedAccount);
        form.addView(account);
        final EditText password = field("Password", true);
        form.addView(password);

        final Button go = button("Sign in", true);
        form.addView(go);
        View.OnClickListener signIn = v -> {
            String name = account.getText().toString().trim();
            String pass = password.getText().toString();
            if (name.isEmpty() || pass.isEmpty()) {
                setMessage("Type the account name and the password.", RED);
                return;
            }
            typedAccount = name;
            password.setText("");
            beginLogin(name, pass);
        };
        go.setOnClickListener(signIn);
        password.setOnEditorActionListener((v, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE || action == EditorInfo.IME_ACTION_GO) {
                signIn.onClick(go);
                return true;
            }
            return false;
        });
        form.addView(button("Back", false, v -> finish()));
    }

    private void showBusy(String text) {
        form.removeAllViews();
        setMessage(text, GREY);
        form.addView(button("Cancel", false, v -> cancelLogin()));
    }

    private void cancelLogin() {
        ++generation;
        SteamSession s = login;
        login = null;
        if (s != null)
            new Thread(s::close, "steam-close").start();
        showLogin("");
    }

    private void showGuard(boolean email, String hint, boolean wrong, final CompletableFuture<String> answer) {
        form.removeAllViews();
        String how = email ? "Steam has sent a code by e-mail" + (hint != null && !hint.isEmpty() ? " (" + hint + ")" : "") + ". Type it here."
                : "Type the code of Steam Guard from the Steam app on your phone.";
        if (wrong)
            how = "The code was wrong.\n" + how;
        setMessage(how, wrong ? RED : 0xffc8d4da);

        final EditText code = field("Steam Guard code", false);
        code.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        form.addView(code);
        final Button ok = button("Confirm", true);
        form.addView(ok);
        View.OnClickListener send = v -> {
            String text = code.getText().toString().trim();
            if (text.isEmpty())
                return;
            ok.setEnabled(false);
            setMessage("Checking the code...", GREY);
            answer.complete(text);
        };
        ok.setOnClickListener(send);
        code.setOnEditorActionListener((v, action, event) -> {
            send.onClick(ok);
            return true;
        });
        form.addView(button("Cancel", false, v -> {
            answer.cancel(true);
            cancelLogin();
        }));
    }

    private void showAccount(String account) {
        form.removeAllViews();
        setMessage("", GREY);
        TextView who = label("Signed in as " + (account.isEmpty() ? "your account" : account), 18, 0xffe9e0c8);
        who.setPadding(0, 0, 0, dp(10));
        form.addView(who);
        form.addView(button("Refresh the games", true, v -> {
            SteamStore.Session s = store.load();
            if (s == null)
                showLogin("The session is gone, sign in again.");
            else
                checkGames(s);
        }));
        form.addView(button("Sign out", false, v -> {
            ++generation;
            store.clear();
            owned = null;
            showOwned();
            showLogin("");
        }));
        form.addView(button("Back", false, v -> finish()));
    }

    // ---- the list of the games

    private boolean installed(Game g) {
        return SteamDownloadService.isInstallation(new File(external, g.folder));
    }

    private void showOwned() {
        SteamDownloadService.State dl = SteamDownloadService.state();
        boolean busy = dl != null && !dl.done && dl.error == null;

        for (int i = 0; i < GAMES.length; ++i) {
            Game g = GAMES[i];
            boolean mine = dl != null && dl.appId == g.appId;
            boolean have = external != null && installed(g);

            bars[i].setVisibility(View.GONE);
            actions[i].setVisibility(View.GONE);
            actions[i].setEnabled(true);

            if (mine && busy) {
                states[i].setText(String.format(Locale.US, "%s  %d%%  (%s)", dl.text, Math.round(dl.percent * 100), size(dl.bytes)));
                states[i].setTextColor(0xffe9e0c8);
                bars[i].setVisibility(View.VISIBLE);
                bars[i].setProgress(Math.round(dl.percent * 1000));
                actions[i].setText("Stop");
                actions[i].setVisibility(View.VISIBLE);
                continue;
            }
            if (mine && dl.error != null) {
                states[i].setText(dl.error);
                states[i].setTextColor(RED);
            } else if (have) {
                states[i].setText("Installed");
                states[i].setTextColor(GREEN);
            } else if (owned == null) {
                states[i].setText("Sign in to check");
                states[i].setTextColor(GREY);
            } else if (owned.contains(g.appId)) {
                states[i].setText("Owned");
                states[i].setTextColor(GREEN);
            } else {
                states[i].setText("Not owned");
                states[i].setTextColor(RED);
            }
            if (!have && owned != null && owned.contains(g.appId)) {
                actions[i].setText(mine && dl.error != null ? "Try again" : "Download");
                actions[i].setVisibility(View.VISIBLE);
                actions[i].setEnabled(!busy);
            }
        }
    }

    private void tick() {
        if (closed)
            return;
        SteamDownloadService.State dl = SteamDownloadService.state();
        showOwned();
        if (dl != null && dl.done && !shownDone) {
            shownDone = true;
            setMessage(dl.title + " is installed. Choose it in the launcher and press Play.", GREEN);
        }
        ui.postDelayed(this::tick, 700);
    }

    private boolean shownDone;

    private void onAction(int index) {
        Game g = GAMES[index];
        SteamDownloadService.State dl = SteamDownloadService.state();
        if (dl != null && dl.appId == g.appId && !dl.done && dl.error == null) {
            startService(new Intent(this, SteamDownloadService.class).setAction(SteamDownloadService.ACTION_STOP));
            return;
        }
        if (store.load() == null) {
            showLogin("Sign in first.");
            return;
        }
        if (external.getUsableSpace() < 1500L * 1024 * 1024) {
            setMessage("There is not enough free space on the phone (" + size(external.getUsableSpace()) + ").", RED);
            return;
        }
        if (Build.VERSION.SDK_INT >= 33)
            requestPermissions(new String[] {"android.permission.POST_NOTIFICATIONS"}, 1);

        shownDone = false;
        setMessage("", GREY);
        Intent start = new Intent(this, SteamDownloadService.class)
                .putExtra(SteamDownloadService.EXTRA_APP, g.appId)
                .putExtra(SteamDownloadService.EXTRA_FOLDER, g.folder)
                .putExtra(SteamDownloadService.EXTRA_TITLE, g.name);
        startForegroundService(start);
    }

    private static String size(long bytes) {
        if (bytes >= 1024L * 1024 * 1024)
            return String.format(Locale.US, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        return String.format(Locale.US, "%d MB", bytes / (1024 * 1024));
    }

    // ---- the login

    private void beginLogin(final String account, final String password) {
        final int mine = ++generation;
        showBusy("Connecting to Steam...");
        new Thread(() -> {
            SteamSession steam = null;
            SteamSession.Login result = null;
            Set<Integer> have = null;
            String error = null;
            boolean kept = false;
            try {
                steam = new SteamSession();
                login = steam;
                steam.connect();
                runUi(mine, () -> showBusy("Signing in..."));
                result = steam.login(account, password, Build.MODEL != null ? Build.MODEL : "Android", new SteamSession.Prompt() {
                    @Override
                    public void code(boolean email, String hint, boolean wrong, CompletableFuture<String> answer) {
                        runUi(mine, () -> showGuard(email, hint, wrong, answer));
                    }

                    @Override
                    public void approve() {
                        runUi(mine, () -> showBusy("Approve the login in the Steam app on your phone."));
                    }
                });
                kept = store.save(new SteamStore.Session(result.steamId, result.account, result.refreshToken));
                have = steam.owned(appIds());
            } catch (SteamSession.Failure e) {
                error = e.getMessage();
            } catch (Throwable e) {
                error = "The login failed: " + e;
            } finally {
                login = null;
                if (steam != null)
                    steam.close();
            }
            final SteamSession.Login done = result;
            final Set<Integer> games = have;
            final String failed = error;
            final boolean saved = kept;
            runUi(mine, () -> {
                if (done == null) {
                    showLogin(failed != null ? failed : "The login failed.");
                    return;
                }
                owned = games;
                showAccount(done.account);
                if (failed != null)
                    setMessage(failed, RED);
                else if (!saved)
                    setMessage("The session cannot be kept on this phone, you will sign in again next time.", RED);
                showOwned();
            });
        }, "steam-login").start();
    }

    private void checkGames(final SteamStore.Session session) {
        final int mine = generation;
        setMessage("Checking the games...", GREY);
        new Thread(() -> {
            SteamSession steam = null;
            Set<Integer> have = null;
            String error = null;
            boolean again = false;
            try {
                steam = new SteamSession();
                steam.connect();
                steam.resume(session.account, session.refreshToken);
                have = steam.owned(appIds());
            } catch (SteamSession.Failure e) {
                error = e.getMessage();
                again = e.signIn;
            } catch (Throwable e) {
                error = "The check failed: " + e;
            } finally {
                if (steam != null)
                    steam.close();
            }
            final Set<Integer> games = have;
            final String failed = error;
            final boolean gone = again;
            runUi(mine, () -> {
                if (gone) {
                    store.clear();
                    owned = null;
                    showOwned();
                    showLogin(failed);
                    return;
                }
                if (games != null)
                    owned = games;
                setMessage(failed != null ? failed : "", RED);
                showOwned();
            });
        }, "steam-games").start();
    }

    private static Set<Integer> appIds() {
        Set<Integer> ids = new HashSet<>();
        for (Game g : GAMES)
            ids.add(g.appId);
        return ids;
    }

    private void runUi(final int mine, final Runnable r) {
        ui.post(() -> {
            if (!closed && mine == generation)
                r.run();
        });
    }

    private void setMessage(String text, int color) {
        message.setText(text);
        message.setTextColor(color);
    }

    // ---- the look

    private LinearLayout column() {
        LinearLayout ret = new LinearLayout(this);
        ret.setOrientation(LinearLayout.VERTICAL);
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

    private EditText field(String hint, boolean password) {
        EditText e = new EditText(this);
        e.setHint(hint);
        e.setSingleLine(true);
        e.setTextSize(17);
        e.setTextColor(0xffe9eef0);
        e.setHintTextColor(0xff6c7e88);
        e.setPadding(dp(14), 0, dp(14), 0);
        e.setInputType(password
                ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setImeOptions(password ? EditorInfo.IME_ACTION_DONE : EditorInfo.IME_ACTION_NEXT);
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(0x66101c26);
        shape.setCornerRadius(dp(8));
        shape.setStroke(dp(1), 0xff425761);
        e.setBackground(shape);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(48));
        lp.setMargins(0, 0, dp(12), dp(9));
        e.setLayoutParams(lp);
        return e;
    }

    private Button button(String text, boolean primary, View.OnClickListener click) {
        Button b = button(text, primary);
        b.setOnClickListener(click);
        return b;
    }

    private Button button(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextSize(17);
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

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(48));
        lp.setMargins(0, 0, dp(12), dp(9));
        b.setLayoutParams(lp);
        return b;
    }
}

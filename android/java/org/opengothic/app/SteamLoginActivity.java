package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.Activity;
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
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CompletableFuture;

/**
 * The first screen when nobody is signed in: the account of Steam (the password, Steam Guard). The session is kept, so it
 * is shown once. The result is OK after a login or when the player goes on without an account (the games are imported then).
 */
// The login needs Android 8 because of the library; the launcher does not start this screen on older ones
@TargetApi(26)
public final class SteamLoginActivity extends Activity {
    // app ids of Steam: Gothic 1 Classic, Gothic II: Gold Edition (with Night of the Raven), The Chronicles of Myrtana: Archolos
    static final int[] APPS = {65540, 39510, 1467450};

    private static final int CREAM = 0xfff1e7cd;
    private static final int GREY = 0xff9fb2c0;
    private static final int RED = 0xffe29a8c;

    private SteamStore store;
    private LinearLayout form;
    private TextView message;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean closed;
    private volatile int generation;
    private volatile SteamSession login;
    private String typedAccount = "";

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        store = new SteamStore(this);
        SystemUi.hideBars(this);
        setContentView(buildUi());
        showLogin("");
    }

    @Override
    public void onBackPressed() {
        setResult(RESULT_CANCELED);
        finish();
    }

    @Override
    protected void onDestroy() {
        closed = true;
        ++generation;
        SteamSession s = login;
        if (s != null)
            new Thread(s::close, "steam-close").start();
        ui.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    // ---- the look

    private View buildUi() {
        FrameLayout root = new FrameLayout(this);
        root.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xff050b12, 0xff0f2231, 0xff060c13}));

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        root.addView(scroll, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout holder = new LinearLayout(this);
        holder.setGravity(Gravity.CENTER);
        holder.setPadding(dp(24), dp(16), dp(24), dp(16));
        scroll.addView(holder, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(28), dp(24), dp(28), dp(22));
        GradientDrawable cardShape = new GradientDrawable();
        cardShape.setColor(0xcc0b1823);
        cardShape.setCornerRadius(dp(20));
        cardShape.setStroke(dp(1), 0xff2a4152);
        card.setBackground(cardShape);
        holder.addView(card, new LinearLayout.LayoutParams(dp(420), -2));

        TextView title = label("OpenGothic", 34, CREAM);
        title.setTypeface(Typeface.create("serif", Typeface.NORMAL));
        card.addView(title);

        TextView hint = label(Texts.t("Sign in to Steam to download your games.",
                "Zaloguj się do Steam, aby pobrać swoje gry."), 14, GREY);
        hint.setPadding(0, dp(4), 0, dp(18));
        card.addView(hint);

        form = new LinearLayout(this);
        form.setOrientation(LinearLayout.VERTICAL);
        card.addView(form);

        message = label("", 13, RED);
        message.setPadding(0, dp(10), 0, 0);
        card.addView(message);
        return root;
    }

    private void showLogin(String error) {
        form.removeAllViews();
        setMessage(error, RED);

        final EditText account = field(Texts.t("Account name", "Nazwa konta"), false);
        account.setText(typedAccount);
        form.addView(account);
        final EditText password = field(Texts.t("Password", "Hasło"), true);
        form.addView(password);

        final Button go = button(Texts.t("Sign in", "Zaloguj"), true);
        form.addView(go);
        View.OnClickListener signIn = v -> {
            String name = account.getText().toString().trim();
            String pass = password.getText().toString();
            if (name.isEmpty() || pass.isEmpty()) {
                setMessage(Texts.t("Type the account name and the password.", "Podaj nazwę konta i hasło."), RED);
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

        TextView skip = label(Texts.t("Continue without an account", "Kontynuuj bez konta"), 14, GREY);
        skip.setGravity(Gravity.CENTER);
        skip.setPadding(0, dp(14), 0, dp(4));
        skip.setOnClickListener(v -> {
            getSharedPreferences(SteamBridge.PREFS, MODE_PRIVATE).edit().putBoolean(SteamBridge.SKIPPED, true).apply();
            setResult(RESULT_OK);
            finish();
        });
        form.addView(skip);
    }

    private void showBusy(String text) {
        form.removeAllViews();
        setMessage("", GREY);
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(6), 0, dp(14));
        row.addView(new ProgressBar(this), new LinearLayout.LayoutParams(dp(28), dp(28)));
        TextView t = label(text, 15, 0xffe0e7e9);
        t.setPadding(dp(14), 0, 0, 0);
        row.addView(t);
        form.addView(row);
        form.addView(button(Texts.t("Cancel", "Anuluj"), false, v -> cancelLogin()));
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
        String how = email
                ? Texts.t("Steam has sent a code by e-mail", "Steam wysłał kod e-mailem") + (hint != null && !hint.isEmpty() ? " (" + hint + ")" : "") + "."
                : Texts.t("Type the Steam Guard code from the Steam app.", "Wpisz kod Steam Guard z aplikacji Steam.");
        if (wrong)
            how = Texts.t("The code was wrong.", "Kod był błędny.") + "\n" + how;
        setMessage(how, wrong ? RED : 0xffc8d4da);

        final EditText code = field(Texts.t("Code", "Kod"), false);
        code.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        form.addView(code);
        final Button ok = button(Texts.t("Confirm", "Potwierdź"), true);
        form.addView(ok);
        View.OnClickListener send = v -> {
            String text = code.getText().toString().trim();
            if (text.isEmpty())
                return;
            ok.setEnabled(false);
            setMessage(Texts.t("Checking the code...", "Sprawdzanie kodu..."), GREY);
            answer.complete(text);
        };
        ok.setOnClickListener(send);
        code.setOnEditorActionListener((v, action, event) -> {
            send.onClick(ok);
            return true;
        });
        form.addView(button(Texts.t("Cancel", "Anuluj"), false, v -> {
            answer.cancel(true);
            cancelLogin();
        }));
    }

    // ---- the login

    private void beginLogin(final String account, final String password) {
        final int mine = ++generation;
        showBusy(Texts.t("Connecting to Steam...", "Łączenie ze Steam..."));
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
                runUi(mine, () -> showBusy(Texts.t("Signing in...", "Logowanie...")));
                result = steam.login(account, password, Build.MODEL != null ? Build.MODEL : "Android", new SteamSession.Prompt() {
                    @Override
                    public void code(boolean email, String hint, boolean wrong, CompletableFuture<String> answer) {
                        runUi(mine, () -> showGuard(email, hint, wrong, answer));
                    }

                    @Override
                    public void approve() {
                        runUi(mine, () -> showBusy(Texts.t("Approve the login in the Steam app on your phone.",
                                "Zatwierdź logowanie w aplikacji Steam na telefonie.")));
                    }
                });
                kept = store.save(new SteamStore.Session(result.steamId, result.account, result.refreshToken));
                have = steam.owned(appIds());
                store.saveOwned(have);
            } catch (SteamSession.Failure e) {
                error = e.getMessage();
            } catch (Throwable e) {
                error = "The login failed: " + e;
            } finally {
                login = null;
                if (steam != null)
                    steam.close();
            }
            final boolean ok = result != null && kept;
            final String failed = error;
            runUi(mine, () -> {
                if (ok) {
                    // done: the launcher takes over
                    setResult(RESULT_OK);
                    finish();
                    return;
                }
                showLogin(failed != null ? failed : Texts.t("The session cannot be kept on this phone.",
                        "Nie można zapisać sesji na tym telefonie."));
            });
        }, "steam-login").start();
    }

    private static Set<Integer> appIds() {
        Set<Integer> ids = new HashSet<>();
        for (int a : APPS)
            ids.add(a);
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
        message.setVisibility(text.isEmpty() ? View.GONE : View.VISIBLE);
    }

    // ---- the parts

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
        e.setTextSize(16);
        e.setTextColor(0xffe9eef0);
        e.setHintTextColor(0xff6c7e88);
        e.setPadding(dp(16), 0, dp(16), 0);
        e.setInputType(password
                ? InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
                : InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        e.setImeOptions(password ? EditorInfo.IME_ACTION_DONE : EditorInfo.IME_ACTION_NEXT);
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(0x66101c26);
        shape.setCornerRadius(dp(12));
        shape.setStroke(dp(1), 0xff33495a);
        e.setBackground(shape);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(48));
        lp.setMargins(0, 0, 0, dp(10));
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
        b.setTextSize(16);
        b.setGravity(Gravity.CENTER);
        b.setTextColor(new ColorStateList(
                new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}},
                new int[] {0xff62727b, primary ? 0xff171c20 : 0xffe0e7e9}));
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(primary ? 0xffe3cf9f : 0x33172530);
        shape.setCornerRadius(dp(12));
        shape.setStroke(dp(1), primary ? 0xffefe0b8 : 0xff425761);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x557f9caa), shape, null));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, dp(48));
        lp.setMargins(0, dp(4), 0, dp(6));
        b.setLayoutParams(lp);
        return b;
    }
}

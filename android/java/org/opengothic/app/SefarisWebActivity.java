package org.opengothic.app;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.webkit.CookieManager;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

/**
 * A page of download (ModDB, MediaFire, a folder of Google Drive...) in a browser of the application. The player does what the page
 * asks (a button, a captcha); when the page starts to download a file, the browser stops it and gives the address, the cookies and
 * the agent back, and {@link SefarisInstaller} downloads the file itself. A page like that cannot be read by the app without a
 * browser: ModDB, for one, answers a program with a check of Cloudflare.
 */
public final class SefarisWebActivity extends Activity {
    static final String EXTRA_PAGE = "page";
    static final String EXTRA_COOKIE = "cookie";
    static final String EXTRA_AGENT = "agent";
    static final String EXTRA_REFERER = "referer";

    private WebView web;

    @Override
    @SuppressLint("SetJavaScriptEnabled")
    public void onCreate(Bundle state) {
        super.onCreate(state);
        String page = getIntent().getStringExtra(EXTRA_PAGE);
        if (page == null || !page.startsWith("http")) {
            finish();
            return;
        }
        SystemUi.hideBars(this);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(0xff050b12);

        LinearLayout bar = new LinearLayout(this);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(16), dp(6), dp(16), dp(6));
        TextView hint = new TextView(this);
        hint.setTextColor(0xffe0e7e9);
        hint.setTextSize(13);
        hint.setText(Texts.t("Press the download button of the page. The app takes the file when it starts to download.",
                "Naciśnij przycisk pobierania na stronie. Aplikacja przejmie plik, gdy ruszy pobieranie."));
        bar.addView(hint, new LinearLayout.LayoutParams(0, -2, 1.f));
        Button close = new Button(this);
        close.setAllCaps(false);
        close.setText(Texts.t("Close", "Zamknij"));
        close.setOnClickListener(v -> finish());
        bar.addView(close, new LinearLayout.LayoutParams(dp(110), dp(40)));
        root.addView(bar);

        web = new WebView(this);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);   // the pages need it (the counter before the download, the check of Cloudflare)
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        CookieManager.getInstance().setAcceptCookie(true);
        CookieManager.getInstance().setAcceptThirdPartyCookies(web, true);
        web.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                // pages stay in the browser of the app; the links to other programs (market:, intent:) are not followed
                String scheme = request.getUrl().getScheme();
                return scheme == null || !scheme.startsWith("http");
            }
        });
        web.setDownloadListener((url, agent, disposition, mime, length) -> {
            if (!url.startsWith("http")) {
                Toast.makeText(this, Texts.t("This download cannot be taken by the app. Use the other ways.",
                        "Tego pobrania aplikacja nie przejmie. Użyj innych sposobów."), Toast.LENGTH_LONG).show();
                return;
            }
            String cookie = CookieManager.getInstance().getCookie(url);
            Intent result = new Intent().setData(Uri.parse(url))
                    .putExtra(EXTRA_COOKIE, cookie != null ? cookie : "")
                    .putExtra(EXTRA_AGENT, agent != null ? agent : "")
                    .putExtra(EXTRA_REFERER, web.getUrl() != null ? web.getUrl() : "");
            setResult(RESULT_OK, result);
            finish();
        });
        root.addView(web, new LinearLayout.LayoutParams(-1, 0, 1.f));
        setContentView(root);
        web.loadUrl(page);
    }

    @Override
    public void onBackPressed() {
        if (web != null && web.canGoBack())
            web.goBack();
        else
            super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        if (web != null) {
            web.stopLoading();
            web.destroy();
        }
        super.onDestroy();
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}

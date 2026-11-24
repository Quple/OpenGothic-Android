package org.opengothic.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Typeface;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;

/** The reports of the mods ({@link SefarisReport}) in a window, with the way to hand them to another application. */
final class ReportDialog {
    private ReportDialog() {
    }

    static void show(final Activity a) {
        final File external = a.getExternalFilesDir(null);
        if (external == null)
            return;
        String text = SefarisReport.all(external, 150 * 1024);
        if (text.isEmpty())
            text = Texts.t("There is no report yet. It is written when a mod is installed and when it is played.",
                    "Nie ma jeszcze raportu. Powstaje przy instalacji moda i przy jego uruchomieniu.");
        final String shared = text;
        int pad = Math.round(20 * a.getResources().getDisplayMetrics().density);
        TextView view = new TextView(a);
        view.setText(text);
        view.setTextSize(11);
        view.setTypeface(Typeface.MONOSPACE);
        view.setTextIsSelectable(true);
        view.setPadding(pad, pad / 2, pad, pad / 2);
        ScrollView scroll = new ScrollView(a);
        scroll.addView(view);
        new AlertDialog.Builder(a).setTitle(Texts.t("Reports of the mods", "Raporty mod\u00f3w")).setView(scroll)
                .setPositiveButton(Texts.t("Share", "Udost\u0119pnij"), (d, w) -> {
                    Intent send = new Intent(Intent.ACTION_SEND).setType("text/plain")
                            .putExtra(Intent.EXTRA_SUBJECT, "OpenGothic: mod reports").putExtra(Intent.EXTRA_TEXT, shared);
                    a.startActivity(Intent.createChooser(send, Texts.t("Share the reports", "Udost\u0119pnij raporty")));
                })
                .setNeutralButton(Texts.t("Clear", "Wyczy\u015b\u0107"), (d, w) -> SefarisReport.clear(external))
                .setNegativeButton(Texts.t("Close", "Zamknij"), null).show();
    }
}

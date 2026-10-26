package org.opengothic.app;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

/**
 * The logs of the game: the engine writes log.txt and crash.log to the files of the application, where only the application
 * can read them. The launcher shows the end of them and hands them to another application (a message, a mail).
 */
final class Logs {
    private Logs() {
    }

    /** The last part of a file, from the start of a line. Empty when there is no file. */
    static String tail(File file, int maxBytes) {
        if (!file.isFile())
            return "";
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            long size = in.length();
            long from = Math.max(0, size - maxBytes);
            in.seek(from);
            byte[] data = new byte[(int) (size - from)];
            in.readFully(data);
            String text = new String(data, StandardCharsets.UTF_8);
            if (from > 0) {
                int line = text.indexOf('\n');
                if (line >= 0)
                    text = text.substring(line + 1);
            }
            return text;
        } catch (IOException e) {
            return "";
        }
    }

    /** The log of the last run of the game and the crash, if there was one. */
    static String report(Context c, int maxBytes) {
        File dir = c.getFilesDir();
        StringBuilder b = new StringBuilder();
        b.append("OpenGothic ").append(version(c)).append(", Android ").append(android.os.Build.VERSION.RELEASE)
                .append(" (API ").append(android.os.Build.VERSION.SDK_INT).append("), ")
                .append(android.os.Build.MANUFACTURER).append(' ').append(android.os.Build.MODEL).append('\n');
        File external = c.getExternalFilesDir(null);
        if (external != null) {
            String chosen = tail(new File(external, "game.txt"), 200).trim().replace('\n', ' ');
            if (!chosen.isEmpty())
                b.append("game: ").append(chosen).append('\n');
        }
        // the safe GPU mode of the game: the shader that the driver was compiling when it killed the game
        for (String name : new String[] {"safegpu.lock", "safegpu.mode"}) {
            String text = tail(new File(dir, name), 1000).trim();
            if (!text.isEmpty())
                b.append("\n---- ").append(name).append('\n').append(text).append('\n');
        }
        // the last lines first: that is where the pipeline of the crash is named
        String log = tail(new File(dir, "log.txt"), maxBytes);
        String[] lines = log.split("\n");
        StringBuilder last = new StringBuilder();
        for (int i = Math.max(0, lines.length - 12); i < lines.length; ++i)
            last.append(lines[i]).append('\n');
        b.append("\n---- the last lines of log.txt\n").append(last);
        String crash = tail(new File(dir, "crash.log"), maxBytes / 3);
        if (!crash.isEmpty())
            b.append("\n---- crash.log\n").append(crash).append('\n');
        b.append("\n---- log.txt\n").append(log);
        return b.toString();
    }

    static String version(Context c) {
        try {
            String v = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
            return v != null ? v : "";
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            return "";
        }
    }

    /** Hands the report to another application and keeps a copy where a file manager or adb can reach it. */
    static void share(Activity a) {
        String text = report(a, 60000);
        File external = a.getExternalFilesDir(null);
        if (external != null) {
            try (FileOutputStream out = new FileOutputStream(new File(external, "last-log.txt"))) {
                out.write(report(a, 400000).getBytes(StandardCharsets.UTF_8));
            } catch (IOException ignored) {
                // the copy is only a convenience
            }
        }
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_SUBJECT, "OpenGothic log");
        send.putExtra(Intent.EXTRA_TEXT, text);
        a.startActivity(Intent.createChooser(send, Texts.t("Share the log", "Udostępnij log")));
    }

    /** Is there a log of a crash that is newer than the given time. */
    static boolean crashedSince(Context c, long time) {
        File crash = new File(c.getFilesDir(), "crash.log");
        return crash.isFile() && crash.lastModified() >= time;
    }

    static boolean hasLog(Context c) {
        return new File(c.getFilesDir(), "log.txt").isFile();
    }
}

package org.opengothic.app;

import java.util.ArrayDeque;
import java.util.Locale;

/** What the download of a game from Steam is doing: the service writes it, the screens read it. */
final class Downloads {
    private Downloads() {
    }

    static final class State {
        final int appId;
        final String title;
        volatile String text = "";
        volatile float percent;       // of the whole game, 0..1
        volatile long bytes;          // what is done
        volatile long total;          // the size of the whole game, 0 when it is not known yet
        volatile long speed;          // bytes in a second
        volatile long eta = -1;       // seconds that are left, -1 when it is not known
        volatile boolean done;
        volatile String error;

        private final ArrayDeque<long[]> samples = new ArrayDeque<>();   // time (ms) and bytes, the last seconds

        /** The bytes that are done and the size of the whole game: the speed is the one of the last seconds. */
        synchronized void progress(long doneBytes, long totalBytes) {
            final long now = System.currentTimeMillis();
            bytes = doneBytes;
            total = Math.max(totalBytes, doneBytes);
            percent = total > 0 ? Math.min(1.f, (float) ((double) doneBytes / total)) : 0.f;

            samples.addLast(new long[] {now, doneBytes});
            while (samples.size() > 2 && now - samples.peekFirst()[0] > 8000)
                samples.removeFirst();
            long[] first = samples.peekFirst();
            long span = now - first[0];
            if (span >= 2000 && doneBytes >= first[1]) {
                speed = (doneBytes - first[1]) * 1000 / span;
                eta = speed > 0 && total > doneBytes ? (total - doneBytes) / speed : -1;
            }
        }

        State(int appId, String title) {
            this.appId = appId;
            this.title = title;
        }

        boolean running() {
            return !done && error == null;
        }
    }

    static volatile State state;

    static String size(long bytes) {
        if (bytes >= 1024L * 1024 * 1024)
            return String.format(Locale.US, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        return String.format(Locale.US, "%d MB", bytes / (1024 * 1024));
    }

    static String time(long seconds) {
        if (seconds < 0)
            return "--:--";
        if (seconds >= 3600)
            return String.format(Locale.US, "%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
        return String.format(Locale.US, "%d:%02d", seconds / 60, seconds % 60);
    }

    /** "1.2 GB / 3.4 GB  -  12.3 MB/s  -  3:10" for the card of the game. */
    static String describe(State s) {
        StringBuilder b = new StringBuilder();
        b.append(size(s.bytes));
        if (s.total > 0)
            b.append(" / ").append(size(s.total));
        if (s.speed > 0)
            b.append("  \u00b7  ").append(String.format(Locale.US, "%.1f MB/s", s.speed / (1024.0 * 1024)));
        if (s.eta >= 0)
            b.append("  \u00b7  ").append(time(s.eta));
        return b.toString();
    }

    /** The download that goes on now, or null. */
    static State running() {
        State s = state;
        return s != null && s.running() ? s : null;
    }
}

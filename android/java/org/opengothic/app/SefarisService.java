package org.opengothic.app;

import android.app.DownloadManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.database.Cursor;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.util.Locale;

/**
 * The installation of a mod of Sefaris: a foreground service, so it goes on when the screen is gone. One mod at a time;
 * {@link #state} is what the screen shows. It is the counterpart of {@link SpineDownloadService}.
 */
public final class SefarisService extends Service {
    static final String ACTION_STOP = "org.opengothic.app.SEFARIS_STOP";

    /** Where the files come from. */
    static final String SOURCE_URL = "url";        // a direct address (Google Drive, or from the browser of the app)
    static final String SOURCE_MEGA = "mega";      // a public link of MEGA
    static final String SOURCE_FILE = "file";      // a ZIP, RAR or 7z file that the player chose
    static final String SOURCE_FOLDER = "folder";  // a folder with the unpacked mod

    private static final String EXTRA_ID = "id";
    private static final String EXTRA_EXTERNAL = "external";
    private static final String EXTRA_NAME = "name";
    private static final String EXTRA_VERSION = "version";
    private static final String EXTRA_GAME = "game";
    private static final String EXTRA_SOURCE = "source";
    private static final String EXTRA_URL = "url";
    private static final String EXTRA_COOKIE = "cookie";
    private static final String EXTRA_AGENT = "agent";
    private static final String EXTRA_REFERER = "referer";
    private static final String EXTRA_HINTS = "hints";

    private static final String CHANNEL = "sefaris-download";
    private static final int NOTIFICATION = 10;

    /** What the service is doing: it writes it, the screen reads it. */
    static final class State {
        final int id;                // the id in the store of the downloaded mods
        final String title;
        volatile long bytes;
        volatile long total;
        volatile boolean unpacking;  // the files are being unpacked from an archive (bytes and total are of the archive)
        volatile boolean done;
        volatile boolean cancelled;
        volatile String error;       // what the player reads
        volatile String library;     // the mod needs Ikarus or LeGo
        volatile String blockedIn = "";   // the file and the symbol that gave it away, for the report
        volatile int plugins;        // the .dll plugins that were left out
        volatile boolean reported;   // the player was told how it ended

        State(int id, String title) {
            this.id = id;
            this.title = title;
        }

        boolean running() {
            return !done && error == null && library == null && !cancelled;
        }

        /** -1 when the size is not known. */
        int percent() {
            return total > 0 ? (int) Math.min(99, bytes * 100 / total) : -1;
        }
    }

    static volatile State state;

    static State running() {
        State s = state;
        return s != null && s.running() ? s : null;
    }

    /** The mod and its source go to the service in an intent; a file or a folder of the player is passed with the right to read it. */
    static Intent intent(Context context, SefarisApi.Mod m, String source, String url, Uri uri, String[] web, String hints) {
        Intent intent = new Intent(context, SefarisService.class)
                .putExtra(EXTRA_ID, m.id).putExtra(EXTRA_EXTERNAL, m.externalId).putExtra(EXTRA_NAME, m.name)
                .putExtra(EXTRA_VERSION, m.version).putExtra(EXTRA_GAME, m.game).putExtra(EXTRA_SOURCE, source);
        intent.putExtra(EXTRA_HINTS, hints != null ? hints : "");
        if (url != null)
            intent.putExtra(EXTRA_URL, url);
        if (web != null && web.length == 3)   // what the browser of the app sent: the cookies, the agent, the page
            intent.putExtra(EXTRA_COOKIE, web[0]).putExtra(EXTRA_AGENT, web[1]).putExtra(EXTRA_REFERER, web[2]);
        if (uri != null)
            intent.setData(uri).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        return intent;
    }

    private PowerManager.WakeLock wake;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && ACTION_STOP.equals(intent.getAction())) {
            State s = state;
            if (s != null)
                s.cancelled = true;
            return START_NOT_STICKY;
        }
        if (intent == null || running() != null)
            return START_NOT_STICKY;   // one mod at a time, or the system has restarted the service with nothing to do

        final SefarisApi.Mod m = new SefarisApi.Mod();
        m.id = intent.getIntExtra(EXTRA_ID, 0);
        m.externalId = str(intent, EXTRA_EXTERNAL);
        m.name = str(intent, EXTRA_NAME);
        m.version = str(intent, EXTRA_VERSION);
        m.game = intent.getIntExtra(EXTRA_GAME, SefarisApi.GOTHIC2);
        final String source = str(intent, EXTRA_SOURCE);
        final SefarisInstaller.Remote remote = new SefarisInstaller.Remote();
        remote.cookie = str(intent, EXTRA_COOKIE);
        remote.agent = str(intent, EXTRA_AGENT);
        remote.referer = str(intent, EXTRA_REFERER);
        if (SOURCE_MEGA.equals(str(intent, EXTRA_SOURCE)))
            remote.mega = str(intent, EXTRA_URL);
        else
            remote.url = str(intent, EXTRA_URL);
        final Uri uri = intent.getData();
        final File external = getExternalFilesDir(null);
        if (m.id == 0 || external == null) {
            stopSelf();
            return START_NOT_STICKY;
        }

        final State s = new State(m.storeId(), m.name);
        state = s;
        startForeground(m.name, "...", 0);

        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "opengothic:sefaris");
            wake.acquire(3L * 60 * 60 * 1000);
        }
        final String hints = str(intent, EXTRA_HINTS);
        new Thread(() -> run(s, m, source, remote, uri, external, hints), "sefaris-install").start();
        return START_NOT_STICKY;
    }

    private static String str(Intent intent, String key) {
        String v = intent.getStringExtra(key);
        return v != null ? v : "";
    }

    private void run(final State s, SefarisApi.Mod m, String source, SefarisInstaller.Remote remote, Uri uri, File external, String hints) {
        final long[] lastNotify = {0};
        SpineInstaller.Progress progress = new SpineInstaller.Progress() {
            @Override
            public void update(long bytes, long total, String file) {
                s.unpacking = SefarisInstaller.UNPACK.equals(file);
                s.bytes = bytes;
                s.total = total > 0 ? Math.max(total, bytes) : 0;
                long now = System.currentTimeMillis();
                if (now - lastNotify[0] > 1000) {
                    lastNotify[0] = now;
                    notifyProgress(s);
                }
            }

            @Override
            public boolean cancelled() {
                return s.cancelled;
            }
        };
        SefarisInstaller.Stats stats = new SefarisInstaller.Stats();
        SpineStore.Mod result = null;
        String where = SOURCE_MEGA.equals(source) ? "mega.nz" : SOURCE_URL.equals(source) ? SefarisApi.host(remote.url)
                : SOURCE_FILE.equals(source) ? "file chosen by the player" : "folder chosen by the player";
        stats.hints = hints;
        stats.source = source + " " + where;
        try {
            SpineStore store = new SpineStore(external);
            String language = Locale.getDefault().getLanguage();
            if (SOURCE_URL.equals(source) || SOURCE_MEGA.equals(source)) {
                try {
                    result = SefarisInstaller.fromUrl(store, m, remote, language, progress, stats);
                } catch (SpineInstaller.Cancelled | SpineInstaller.Blocked e) {
                    throw e;
                } catch (IOException e) {
                    // the app could not connect to the server, perhaps the phone lets the system do it (the download manager)
                    if (!SOURCE_URL.equals(source) || !SefarisInstaller.isConnectFailure(e))
                        throw e;
                    Log.w("OpenGothic/Sefaris", "own connection failed, trying the download manager: " + SefarisInstaller.explain(e));
                    try {
                        where += " (the download manager: the own connection failed: " + SefarisInstaller.explain(e) + ")";
                        result = installBySystem(store, m, remote, language, progress, stats, s, external);
                    } catch (SpineInstaller.Cancelled | SpineInstaller.Blocked second) {
                        throw second;
                    } catch (IOException second) {
                        throw new IOException(e.getMessage() + "\n\n" + Texts.t("The download manager: ", "Menedżer pobierania: ") + second.getMessage(), second);
                    }
                }
            }
            else if (SOURCE_FILE.equals(source) && uri != null)
                result = SefarisInstaller.fromFile(store, m, getContentResolver(), uri, language, progress, stats);
            else if (SOURCE_FOLDER.equals(source) && uri != null)
                result = SefarisInstaller.fromFolder(store, m, getContentResolver(), uri, language, progress, stats);
            else
                throw new IOException("Nothing to install");
            s.plugins = stats.plugins;
            s.done = true;
        } catch (SpineInstaller.Blocked b) {
            s.library = b.library;
            s.blockedIn = b.file + (b.marker.isEmpty() ? "" : ", symbol " + b.marker);
        } catch (SpineInstaller.Cancelled c) {
            s.cancelled = true;
        } catch (IOException e) {
            Log.e("OpenGothic/Sefaris", "install of " + m.name + " failed", e);
            s.error = e.getMessage() != null ? e.getMessage() : e.toString();
        } catch (OutOfMemoryError | RuntimeException e) {
            Log.e("OpenGothic/Sefaris", "install of " + m.name + " failed", e);
            s.error = "Failed: " + e;
        } finally {
            if (wake != null && wake.isHeld())
                wake.release();
            // the report of the mod: what was left out, or why it failed
            try {
                SefarisReport.install(SefarisReport.app(this), external, m, source, where, stats, result,
                        s.error != null ? s.error : s.cancelled ? "cancelled by the player" : null, s.library, s.blockedIn);
            } catch (RuntimeException e) {
                Log.w("OpenGothic/Sefaris", "the report was not written: " + e);
            }
            finish(s);
        }
    }

    /**
     * The system download manager gets the file (with the cookies and the agent of the browser of the app) and the app unpacks it.
     * It is the way of the browsers of the phone, so it may work where the connection of the app itself does not.
     */
    private SpineStore.Mod installBySystem(SpineStore store, SefarisApi.Mod m, SefarisInstaller.Remote remote, String language,
                                           SpineInstaller.Progress progress, SefarisInstaller.Stats stats, State s, File external) throws IOException {
        DownloadManager dm = (DownloadManager) getSystemService(Context.DOWNLOAD_SERVICE);
        if (dm == null)
            throw new IOException("There is no download manager");
        String name = "sefaris-" + s.id + ".download";
        File file = new File(external, name);
        SpineStore.delete(file);   // the manager would choose another name for a file that is there
        DownloadManager.Request request = new DownloadManager.Request(Uri.parse(remote.url))
                .setTitle(s.title)
                .setMimeType("application/octet-stream")
                .setAllowedOverMetered(true)
                .setAllowedOverRoaming(true)
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE)
                .setDestinationInExternalFilesDir(this, null, name);
        if (!remote.cookie.isEmpty())
            request.addRequestHeader("Cookie", remote.cookie);
        if (!remote.agent.isEmpty())
            request.addRequestHeader("User-Agent", remote.agent);
        if (!remote.referer.isEmpty())
            request.addRequestHeader("Referer", remote.referer);
        long id = dm.enqueue(request);
        try {
            long lastBytes = -1;
            long lastChange = System.currentTimeMillis();
            while (true) {
                if (s.cancelled)
                    throw new SpineInstaller.Cancelled();
                try (Cursor c = dm.query(new DownloadManager.Query().setFilterById(id))) {
                    if (c == null || !c.moveToFirst())
                        throw new IOException("The download was removed");
                    int status = c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS));
                    long bytes = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
                    long total = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
                    progress.update(Math.max(0, bytes), Math.max(0, total), "");
                    if (status == DownloadManager.STATUS_SUCCESSFUL)
                        break;
                    if (status == DownloadManager.STATUS_FAILED)
                        throw new IOException("code " + c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_REASON)));
                    if (bytes != lastBytes) {
                        lastBytes = bytes;
                        lastChange = System.currentTimeMillis();
                    } else if (System.currentTimeMillis() - lastChange > 90000) {
                        // the manager waits and tries again for ever; the player is told instead
                        throw new IOException(Texts.t("it did not start in 90 seconds", "nie ruszył przez 90 sekund"));
                    }
                }
                try {
                    Thread.sleep(700);
                } catch (InterruptedException e) {
                    throw new SpineInstaller.Cancelled();
                }
            }
            progress.update(file.length(), file.length(), "");
            return SefarisInstaller.fromFile(store, m, getContentResolver(), Uri.fromFile(file), language, progress, stats);
        } finally {
            dm.remove(id);   // it deletes the file too
            SpineStore.delete(file);
        }
    }

    private void notifyProgress(State s) {
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            int percent = s.percent();
            String text = (percent >= 0 ? percent + "%  ·  " : "") + Downloads.size(s.bytes);
            nm.notify(NOTIFICATION, notification(s.title, s.unpacking ? Texts.t("Unpacking  ·  ", "Rozpakowywanie  ·  ") + text : text, percent));
        }
    }

    private void finish(State s) {
        stopForeground(false);
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            String text;
            if (s.library != null)
                text = Texts.t("It needs " + s.library + ", which OpenGothic cannot run. Removed.",
                        "Wymaga " + s.library + ", którego OpenGothic nie uruchomi. Usunięto.");
            else if (s.error != null)
                text = s.error;
            else if (s.cancelled)
                text = Texts.t("Stopped", "Zatrzymano");
            else
                text = Texts.t("Ready to play", "Gotowe do gry");
            Notification.Builder b = builder().setContentTitle(s.title)
                    .setContentText(text)
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setAutoCancel(true)
                    .setOngoing(false);
            nm.notify(NOTIFICATION, b.build());
        }
        stopSelf();
    }

    private void startForeground(String title, String text, int percent) {
        Notification n = notification(title, text, percent);
        if (Build.VERSION.SDK_INT >= 29)
            startForeground(NOTIFICATION, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else
            startForeground(NOTIFICATION, n);
    }

    @SuppressWarnings("deprecation")
    private Notification.Builder builder() {
        // notification channels exist since Android 8, the minimum of the app is lower
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return new Notification.Builder(this);
        NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null && nm.getNotificationChannel(CHANNEL) == null)
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Mods from Sefaris", NotificationManager.IMPORTANCE_LOW));
        return new Notification.Builder(this, CHANNEL);
    }

    private Notification notification(String title, String text, int percent) {
        Intent stop = new Intent(this, SefarisService.class).setAction(ACTION_STOP);
        PendingIntent stopIntent = PendingIntent.getService(this, 0, stop, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder b = builder()
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .addAction(new Notification.Action.Builder(Icon.createWithResource(this, android.R.drawable.ic_menu_close_clear_cancel),
                        Texts.t("Stop", "Zatrzymaj"), stopIntent).build());
        if (percent > 0)
            b.setProgress(100, Math.min(100, percent), false);
        else
            b.setProgress(0, 0, true);
        return b.build();
    }

    @Override
    public void onDestroy() {
        State s = state;
        if (s != null && s.running())
            s.cancelled = true;
        super.onDestroy();
    }
}

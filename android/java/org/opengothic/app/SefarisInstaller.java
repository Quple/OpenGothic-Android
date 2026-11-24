package org.opengothic.app;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PushbackInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Puts a mod of Sefaris into the store of the downloaded mods ({@link SpineStore}), where it is played like a mod of Spine
 * ({@link SpineActivation}). Three sources: a file on Google Drive, a file that the player chose, and a folder with the unpacked mod.
 * A ZIP is unpacked on the way (nothing is kept but the mod); RAR, 7z and the like are opened by {@link SefarisArchive} (libarchive),
 * a Drive file is kept in a file for that, because these formats are read with seeking.
 *
 * The mods of Sefaris are packed for the folder of Gothic on a computer, so the files go to the same paths as in a mod of Spine
 * (Data/X.vdf, System/X.ini, _work/...). The programs of Windows (.dll, .exe: the plugins of Union) are left out.
 */
final class SefarisInstaller {
    private SefarisInstaller() {
    }

    private static final long RESERVE = 200L * 1024 * 1024;

    /** What was left out; the screen tells it. */
    static final class Stats {
        volatile int plugins;      // .dll files, the plugins of Union that run only on Windows
        volatile int installers;   // .exe files: an archive of Sefaris often holds only the installer of the mod
        /** What was left out, with the reason: the report of the mod lists it ({@link SefarisReport}). */
        final List<String> skipped = new java.util.concurrent.CopyOnWriteArrayList<>();
        volatile String hints = "";    // what the guide of the mod says it needs (Union, plugins...), for the report
        volatile String source = "";   // where the files came from, for the report
    }

    private interface Job {
        void run(File dir) throws IOException;
    }

    // ---- the unpacker of the archives that are not ZIP

    /** The mark of the progress that is the unpacking of a file, not its download. */
    static final String UNPACK = "unpack";

    /** Receives the files of an archive one by one; the stream is the data of the file and ends with it. */
    interface Sink {
        void file(String path, InputStream data) throws IOException;
    }

    /** Opens RAR, 7z and the other archives that a ZIP reader cannot: {@link SefarisArchive}, which needs libarchive. */
    interface Unpacker {
        void readFile(File file, Sink sink, SpineInstaller.Progress progress) throws IOException;

        /** The descriptor must be seekable; the size is for the progress only. */
        void readFd(int fd, long size, Sink sink, SpineInstaller.Progress progress) throws IOException;
    }

    /** The unpacker, or null in a build that has no libarchive. It is found by name: the script build-apk.sh leaves it out. */
    private static Unpacker unpacker() {
        try {
            return (Unpacker) Class.forName("org.opengothic.app.SefarisArchive").getDeclaredConstructor().newInstance();
        } catch (ReflectiveOperationException | LinkageError e) {
            return null;
        }
    }

    // ---- the three sources

    /** Where a file on the network is: an address, or a public link of MEGA, and what a browser would send with the request. */
    static final class Remote {
        String url = "";       // a direct address: Google Drive, or what the browser of the app got from a page
        String mega = "";      // a public link of MEGA (a file, or a folder: the biggest archive is taken)
        String cookie = "";
        String agent = "";
        String referer = "";
    }

    /** An open connection to the file: the data (decrypted for MEGA), its size when it is known, and what to close. */
    private static final class Opened {
        InputStream in;
        long total;
        HttpURLConnection connection;
    }

    private static Opened open(Remote remote) throws IOException {
        Opened o = new Opened();
        if (!remote.mega.isEmpty()) {
            SefarisMega.Target t = SefarisMega.resolve(remote.mega);
            HttpURLConnection[] c = new HttpURLConnection[1];
            try {
                o.in = SefarisMega.open(t, c);
            } finally {
                o.connection = c[0];
            }
            o.total = t.size;
            return o;
        }
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(remote.url).openConnection();
        o.connection = c;
        c.setConnectTimeout(20000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", remote.agent.isEmpty() ? "OpenGothic-Android" : remote.agent);
        if (!remote.cookie.isEmpty())
            c.setRequestProperty("Cookie", remote.cookie);
        if (!remote.referer.isEmpty())
            c.setRequestProperty("Referer", remote.referer);
        int code;
        try {
            code = c.getResponseCode();
        } catch (IOException e) {
            // the system says only "Failed to connect to host/ip:port"; the reason (a timeout, a refusal, no route) is in the causes
            throw new IOException(Texts.t("Cannot connect to ", "Nie można połączyć z ") + SefarisApi.host(remote.url) + ": " + explain(e), e);
        }
        if (code != 200)
            throw new IOException(Texts.t("The server answered " + code + ".", "Serwer odpowiedział " + code + "."));
        String type = c.getContentType();
        if (type != null && type.toLowerCase(Locale.ROOT).startsWith("text/"))
            throw new IOException(Texts.t(
                    "The server gave a page, not the file (a limit of downloads, a private file, or a page that has to be opened in a browser).",
                    "Serwer wydał stronę, nie plik (limit pobrań, plik prywatny albo strona, którą trzeba otworzyć w przeglądarce)."));
        o.in = c.getInputStream();
        o.total = c.getContentLengthLong();
        return o;
    }

    /** The messages of an error and of its causes, without repeats: "ConnectException: ... ETIMEDOUT". */
    static String explain(Throwable e) {
        StringBuilder b = new StringBuilder();
        for (Throwable t = e; t != null && b.length() < 400; t = t.getCause() == t ? null : t.getCause()) {
            String m = t.getMessage();
            String part = t.getClass().getSimpleName() + (m != null ? ": " + m : "");
            if (b.indexOf(m != null ? m : part) < 0) {
                if (b.length() > 0)
                    b.append(" <- ");
                b.append(part);
            }
        }
        return b.toString();
    }

    /** The connection to the server could not be made (not an error of the file): the system downloader may still get it. */
    static boolean isConnectFailure(Throwable e) {
        for (Throwable t = e; t != null; t = t.getCause() == t ? null : t.getCause()) {
            if (t instanceof java.net.ConnectException || t instanceof java.net.SocketTimeoutException
                    || t instanceof java.net.UnknownHostException || t instanceof java.net.NoRouteToHostException)
                return true;
        }
        return false;
    }

    /** A file on the network. A ZIP is unpacked on the way; the other archives are kept in a file first. */
    static SpineStore.Mod fromUrl(final SpineStore store, final SefarisApi.Mod m, final Remote remote, String phoneLanguage,
                                  final SpineInstaller.Progress progress, final Stats stats) throws IOException {
        return install(store, m, phoneLanguage, stats, new Job() {
            @Override
            public void run(File dir) throws IOException {
                File staging = new File(dir.getParentFile(), dir.getName() + ".archive");
                Opened o = null;
                try {
                    o = open(remote);
                    final long total = o.total;
                    if (total > 0 && dir.getUsableSpace() < total + RESERVE)
                        throw new IOException(noSpace());
                    try (InputStream in = o.in) {
                        PushbackInputStream data = new PushbackInputStream(counted(in, progress, new long[1], total), 8);
                        String kind = kindOf(data);
                        if (kind.equals("zip")) {
                            unzip(data, dir, stats);
                            return;
                        }
                        Unpacker unpacker = unpackerFor(kind);
                        // the archive and what is unpacked from it are on the disk together
                        if (total > 0 && dir.getUsableSpace() < total * 2 + RESERVE)
                            throw new IOException(noSpace());
                        SpineStore.delete(staging);
                        try (OutputStream out = new FileOutputStream(staging)) {
                            byte[] buf = new byte[131072];
                            int n;
                            while ((n = data.read(buf)) > 0)
                                out.write(buf, 0, n);
                        }
                        unpacker.readFile(staging, sink(dir, stats), progress);
                    }
                } finally {
                    if (o != null && o.connection != null)
                        o.connection.disconnect();
                    SpineStore.delete(staging);
                }
            }
        });
    }
    /** A file that the player chose: ZIP, RAR, 7z. */
    static SpineStore.Mod fromFile(SpineStore store, SefarisApi.Mod m, final ContentResolver cr, final Uri uri, String phoneLanguage,
                                   final SpineInstaller.Progress progress, final Stats stats) throws IOException {
        return install(store, m, phoneLanguage, stats, new Job() {
            @Override
            public void run(File dir) throws IOException {
                String kind;
                try (InputStream in = cr.openInputStream(uri)) {
                    if (in == null)
                        throw new IOException("Cannot open the file");
                    PushbackInputStream data = new PushbackInputStream(counted(in, progress, new long[1], 0), 8);
                    kind = kindOf(data);
                    if (kind.equals("zip")) {
                        unzip(data, dir, stats);
                        return;
                    }
                }
                // the other archives are read from the file itself, which has to be seekable (7z)
                Unpacker unpacker = unpackerFor(kind);
                try (ParcelFileDescriptor fd = cr.openFileDescriptor(uri, "r")) {
                    if (fd == null)
                        throw new IOException("Cannot open the file");
                    unpacker.readFd(fd.getFd(), fd.getStatSize(), sink(dir, stats), progress);
                }
            }
        });
    }

    /** A folder that the player chose, with the unpacked mod. */
    static SpineStore.Mod fromFolder(SpineStore store, SefarisApi.Mod m, final ContentResolver cr, final Uri tree, String phoneLanguage,
                                     final SpineInstaller.Progress progress, final Stats stats) throws IOException {
        return install(store, m, phoneLanguage, stats, new Job() {
            @Override
            public void run(File dir) throws IOException {
                copyTree(cr, tree, DocumentsContract.getTreeDocumentId(tree), dir, "", progress, new long[1], stats, new byte[131072]);
            }
        });
    }

    // ---- the common part

    private static SpineStore.Mod install(SpineStore store, SefarisApi.Mod m, String phoneLanguage, Stats stats, Job job)
            throws IOException {
        // the files of a mod that is played now are in the folder of the game: they are taken out before the mod is replaced
        SpineActivation.restore(store.root);
        File dir = store.modDir(m.storeId());
        SpineStore.delete(dir);
        if (!dir.mkdirs())
            throw new IOException("Cannot create " + dir.getPath());
        try {
            job.run(dir);
            locateRoot(dir);

            SpineStore.Mod mod = new SpineStore.Mod();
            mod.id = m.storeId();
            mod.name = m.name;
            mod.version = m.version;
            mod.gameType = m.game;
            collect(dir, "", mod.files);
            List<String> inis = new ArrayList<>();
            for (String ini : SpineStore.iniFiles(mod.files)) {
                String low = ini.toLowerCase(Locale.ROOT);
                // the ini of the game itself is not the ini of a mod
                if (!low.endsWith("/gothic.ini") && !low.endsWith("/gothicgame.ini"))
                    inis.add(ini);
            }
            mod.ini = SpineStore.pickIni(inis, phoneLanguage);
            if (mod.ini.isEmpty() && stats.installers > 0 && mod.files.isEmpty())
                throw new IOException(Texts.t(
                        "This archive holds only an installer for Windows (.exe). It cannot be opened on the phone: run it on a computer and choose the folder it made.",
                        "To archiwum zawiera tylko instalator dla Windows (.exe). Nie otworzy się na telefonie: uruchom go na komputerze i wskaż folder, który utworzył."));
            if (mod.ini.isEmpty())
                throw new IOException(Texts.t(
                        "There is no ini file in System/ of this mod. It is a patch for the game that needs Union, OpenGothic cannot start it.",
                        "Ten mod nie ma pliku ini w System/. To łatka do gry wymagająca Union, OpenGothic jej nie uruchomi."));
            for (String f : mod.files)
                mod.size += new File(dir, f).length();
            store.write(mod);
            SefarisReport.writeMeta(dir, stats, stats.source);
            mod.dir = dir;
            return mod;
        } catch (SpineInstaller.Blocked b) {
            SpineStore.delete(dir);
            store.block(m.storeId(), b.library);
            throw b;
        } catch (IOException | RuntimeException e) {
            SpineStore.delete(dir);
            throw e;
        }
    }

    private static String noSpace() {
        return Texts.t("There is not enough free space on the phone.", "Brakuje wolnego miejsca w telefonie.");
    }

    /** The first bytes tell what the file is: "zip", "rar", "7z", "exe", or "" for the rest. They are put back. */
    private static String kindOf(PushbackInputStream in) throws IOException {
        byte[] head = new byte[8];
        int n = 0;
        while (n < head.length) {
            int r = in.read(head, n, head.length - n);
            if (r < 0)
                break;
            n += r;
        }
        if (n > 0)
            in.unread(head, 0, n);
        if (n >= 2 && head[0] == 'P' && head[1] == 'K')
            return "zip";
        if (n >= 4 && head[0] == 'R' && head[1] == 'a' && head[2] == 'r')
            return "rar";
        if (n >= 2 && head[0] == '7' && head[1] == 'z')
            return "7z";
        if (n >= 2 && head[0] == 'M' && head[1] == 'Z')
            return "exe";
        return "";
    }

    /** The unpacker for a file that is not a ZIP; an installer of Windows and a build without the library are told to the player. */
    private static Unpacker unpackerFor(String kind) throws IOException {
        if (kind.equals("exe"))
            throw new IOException(Texts.t(
                    "This is an installer for Windows (.exe). It cannot be opened on the phone: run it on a computer and choose the folder it made.",
                    "To instalator dla Windows (.exe). Nie otworzy się na telefonie: uruchom go na komputerze i wskaż folder, który utworzył."));
        Unpacker unpacker = unpacker();
        if (unpacker == null)
            throw new IOException(Texts.t(
                    "This build of the app opens only ZIP files. Unpack the archive with a file manager (for example ZArchiver) and choose the unpacked folder.",
                    "Ta wersja aplikacji otwiera tylko pliki ZIP. Rozpakuj archiwum menedżerem plików (np. ZArchiver) i wskaż rozpakowany folder."));
        return unpacker;
    }

    /** What happens to a file of an archive: the paths are checked, the programs of Windows are left out, the scripts are searched. */
    private static Sink sink(final File dir, final Stats stats) throws IOException {
        final String prefix = dir.getCanonicalPath() + File.separator;
        final byte[] buf = new byte[131072];
        return new Sink() {
            @Override
            public void file(String path, InputStream data) throws IOException {
                String name = path.replace('\\', '/');
                String local = SpineInstaller.localName(name);
                if (local == null) {
                    count(stats, name);
                    return;
                }
                write(data, new File(dir, local), local, prefix, buf);
            }
        };
    }
    private static void unzip(InputStream in, File dir, Stats stats) throws IOException {
        String prefix = dir.getCanonicalPath() + File.separator;
        ZipInputStream zip = new ZipInputStream(in);
        byte[] buf = new byte[131072];
        ZipEntry e;
        while ((e = zip.getNextEntry()) != null) {
            if (e.isDirectory())
                continue;
            String name = e.getName().replace('\\', '/');
            String local = SpineInstaller.localName(name);
            if (local == null) {
                count(stats, name);
                continue;
            }
            write(zip, new File(dir, local), local, prefix, buf);
        }
    }

    private static void copyTree(ContentResolver cr, Uri tree, String docId, File dir, String rel, SpineInstaller.Progress progress,
                                 long[] bytes, Stats stats, byte[] buf) throws IOException {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        List<String[]> entries = new ArrayList<>();
        try (Cursor c = cr.query(children, new String[] {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE},
                null, null, null)) {
            if (c == null)
                throw new IOException("Cannot read the folder");
            while (c.moveToNext()) {
                if (c.getString(0) != null && c.getString(1) != null)
                    entries.add(new String[] {c.getString(0), c.getString(1), c.getString(2)});
            }
        }
        String prefix = dir.getCanonicalPath() + File.separator;
        for (String[] e : entries) {
            if (progress.cancelled())
                throw new SpineInstaller.Cancelled();
            String path = rel.isEmpty() ? e[1] : rel + "/" + e[1];
            if (Document.MIME_TYPE_DIR.equals(e[2])) {
                copyTree(cr, tree, e[0], dir, path, progress, bytes, stats, buf);
                continue;
            }
            String local = SpineInstaller.localName(path);
            if (local == null) {
                count(stats, path);
                continue;
            }
            try (InputStream in = cr.openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree, e[0]))) {
                if (in == null)
                    throw new IOException("Cannot open " + path);
                write(counted(in, progress, bytes, 0), new File(dir, local), local, prefix, buf);
            }
        }
    }

    private static void count(Stats stats, String skipped) {
        String low = skipped.toLowerCase(Locale.ROOT);
        String reason;
        if (low.endsWith(".dll")) {
            stats.plugins++;
            reason = "plugin of Windows (Union)";
        } else if (low.endsWith(".exe")) {
            stats.installers++;
            reason = "program or installer of Windows";
        } else if (low.endsWith(".manifest")) {
            reason = "manifest of a program of Windows";
        } else if (low.contains("..")) {
            reason = "unsafe path";
        } else {
            return;   // hidden files of other systems (._x, .DS_Store) are not worth a line
        }
        if (stats.skipped.size() < 300)
            stats.skipped.add(skipped + " (" + reason + ")");
    }

    /** One file of the mod; the scripts are searched for Ikarus and LeGo while they are written. */
    private static void write(InputStream in, File out, String local, String prefix, byte[] buf) throws IOException {
        if (!out.getCanonicalPath().startsWith(prefix))
            throw new IOException("Unsafe path in the mod");
        File parent = out.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs())
            throw new IOException("Cannot create " + parent.getPath());
        SpineScan scan = !SpineInstaller.ALLOW_LIBRARIES && SpineScan.worthScanning(local) ? new SpineScan() : null;
        try (OutputStream o = new FileOutputStream(out)) {
            int n;
            while ((n = in.read(buf)) > 0) {
                o.write(buf, 0, n);
                if (scan != null) {
                    scan.feed(buf, n);
                    if (scan.found() != null)
                        throw SpineInstaller.blocked(scan.found(), local, scan.marker());
                }
            }
        }
    }

    /**
     * The mods of Sefaris are packed in folders of the author ("Mod 1.5/Patch/System", "Mod 1.5/Patch/Data", and a readme next to
     * them). The folder that has System with an ini file in it is the mod: it takes the place of the whole unpacked tree, so the
     * paths start with System, Data, _work as in the folder of Gothic. Nothing is changed when there is no such folder.
     */
    private static void locateRoot(File dir) throws IOException {
        File root = findRoot(dir);
        if (root == null || root.equals(dir))
            return;
        File moved = new File(dir.getParentFile(), dir.getName() + ".root");
        SpineStore.delete(moved);
        if (!root.renameTo(moved))
            throw new IOException("Cannot move the folder of the mod");
        SpineStore.delete(dir);
        if (!moved.renameTo(dir))
            throw new IOException("Cannot move the folder of the mod");
    }

    /** The nearest folder (a few levels down at most) that has System with an ini file in it. */
    private static File findRoot(File dir) {
        List<File> level = new ArrayList<>();
        level.add(dir);
        for (int depth = 0; depth < 5 && !level.isEmpty(); ++depth) {
            List<File> next = new ArrayList<>();
            for (File d : level) {
                File system = SpineActivation.find(d, "System");
                File[] inside = system != null && system.isDirectory() ? system.listFiles() : null;
                for (int i = 0; inside != null && i < inside.length; ++i) {
                    if (inside[i].isFile() && inside[i].getName().toLowerCase(Locale.ROOT).endsWith(".ini"))
                        return d;
                }
                File[] kids = d.listFiles();
                if (kids == null)
                    continue;
                java.util.Arrays.sort(kids);
                for (File k : kids) {
                    if (k.isDirectory() && !k.getName().startsWith("."))
                        next.add(k);
                }
            }
            level = next;
        }
        return null;
    }
    private static void collect(File dir, String prefix, List<String> out) {
        File[] children = dir.listFiles();
        if (children == null)
            return;
        for (File f : children) {
            if (f.getName().startsWith("."))
                continue;
            if (f.isDirectory())
                collect(f, prefix + f.getName() + "/", out);
            else
                out.add(prefix + f.getName());
        }
    }

    /** The bytes that are read are counted and the cancel is looked at. */
    private static InputStream counted(InputStream in, final SpineInstaller.Progress progress, final long[] bytes, final long total) {
        return new FilterInputStream(in) {
            @Override
            public int read(byte[] b, int off, int len) throws IOException {
                if (progress.cancelled())
                    throw new SpineInstaller.Cancelled();
                int n = super.read(b, off, len);
                if (n > 0) {
                    bytes[0] += n;
                    progress.update(bytes[0], total, "");
                }
                return n;
            }

            @Override
            public int read() throws IOException {
                byte[] one = new byte[1];
                return read(one, 0, 1) < 0 ? -1 : one[0] & 0xff;
            }
        };
    }
}

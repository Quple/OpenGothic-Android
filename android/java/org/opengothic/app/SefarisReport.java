package org.opengothic.app;

import android.content.Context;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Properties;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * The report of every mod of Sefaris: what the install left out, what the mod needs that OpenGothic does not have, what was wrong when
 * it was started and what the engine complained about while it ran. One text file per mod in mod-logs/ of the files of the
 * application, so that the problems of a mod can be read in one place and fixed one by one. The lines that start with
 * <code>TODO</code> are what has to be done for the mod to work fully; <code>WARN</code> and <code>ERROR</code> are what was found.
 * Every mod that is played is reported (Spine and Sefaris), only Sefaris mods have the report of the install. The screen of Sefaris shows
 * all the reports and hands them to another application.
 */
final class SefarisReport {
    private SefarisReport() {
    }

    private static final int LIMIT = 200 * 1024;   // a report that grows over it loses its oldest part

    /** The meta file in the folder of a mod: what the install left out and what the guide of the mod says. */
    static final String META = ".sefaris";

    static File dir(File external) {
        File d = new File(external, "mod-logs");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }

    static File file(File external, int storeId, String name) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < name.length() && b.length() < 40; ++i) {
            char c = name.charAt(i);
            b.append((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_');
        }
        // the mods of Spine are reported too (every mod that is played): their ids are small, the ones of Sefaris start at ID_BASE
        return new File(dir(external), storeId >= SefarisApi.ID_BASE ? b + "-" + (storeId - SefarisApi.ID_BASE) + ".log" : "spine-" + b + "-" + storeId + ".log");
    }

    private static String stamp() {
        return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT).format(new Date());
    }

    static synchronized void append(File external, int storeId, String name, String title, List<String> lines) {
        File f = file(external, storeId, name);
        try (Writer w = new OutputStreamWriter(new FileOutputStream(f, true), StandardCharsets.UTF_8)) {
            w.write("\n=== " + stamp() + "  " + title + " ===\n");
            for (String line : lines)
                w.write(line + "\n");
        } catch (IOException ignored) {
            // a report that cannot be written is not worth stopping the mod for
        }
        if (f.length() > LIMIT) {
            String keep = Logs.tail(f, LIMIT * 3 / 4);
            try (Writer w = new OutputStreamWriter(new FileOutputStream(f), StandardCharsets.UTF_8)) {
                w.write(keep);
            } catch (IOException ignored) {
                // the long report stays
            }
        }
    }

    // ---- the install

    /**
     * The end of an install of a mod of Sefaris, good or bad. {@code mod} is null when it failed.
     * @param where where the files came from (a host, a file name)
     */
    static void install(String app, File external, SefarisApi.Mod m, String source, String where, SefarisInstaller.Stats stats,
                        SpineStore.Mod mod, String failure, String library) {
        install(app, external, m, source, where, stats, mod, failure, library, "");
    }

    /** @param blockedIn the file and the symbol that gave Ikarus or LeGo away ("Data/X.mod, MEM_READINT"), or "" */
    static void install(String app, File external, SefarisApi.Mod m, String source, String where, SefarisInstaller.Stats stats,
                        SpineStore.Mod mod, String failure, String library, String blockedIn) {
        write(app, external, m.storeId(), m.name, m.version, m.game, m.externalId, "INSTALL", source, where, stats.skipped,
                stats.installers, stats.hints, mod, failure, library, blockedIn);
    }

    /** The end of the download (or of the check of the scripts) of a mod of Spine. */
    static void installSpine(String app, File external, SpineApi.Project p, boolean check, List<String> skipped, SpineStore.Mod mod,
                             String failure, String library, String blockedIn) {
        int installers = 0;
        for (String s : skipped) {
            if (s.toLowerCase(Locale.ROOT).contains(".exe"))
                ++installers;
        }
        write(app, external, p.id, p.name, SpineApi.versionOf(p), p.gameType, "", check ? "CHECK" : "INSTALL", check ? "spine (the scripts only)" : "spine",
                "clockwork-origins.com", skipped, installers, "", mod, failure, library, blockedIn);
    }

    private static String gameName(int game) {
        return game == SpineStore.GOTHIC ? "Gothic" : game == SpineStore.GOTHIC2 ? "Gothic II" : "Gothic / Gothic II";
    }

    private static void write(String app, File external, int storeId, String name, String version, int game, String externalId, String title,
                              String source, String where, List<String> skipped, int installers, String hints, SpineStore.Mod mod,
                              String failure, String library, String blockedIn) {
        List<String> r = new ArrayList<>();
        r.add("mod: " + name + " v" + version + ", " + gameName(game) + ", id " + (storeId >= SefarisApi.ID_BASE ? storeId - SefarisApi.ID_BASE : storeId)
                + (externalId.isEmpty() ? "" : " (" + externalId + ")"));
        r.add("app: " + app);
        r.add("source: " + source + (where.isEmpty() ? "" : " " + where));
        if (library != null) {
            r.add("RESULT: BLOCKED, the scripts use " + library + (blockedIn.isEmpty() ? "" : " (" + blockedIn + ")")
                    + " (OpenGothic cannot run it, the mod was removed)");
            r.add("TODO: " + library + " has to be emulated by the engine (docs/IKARUS.md) before this mod can run"
                    + (blockedIn.isEmpty() ? "" : "; the symbol that gave it away: " + blockedIn));
        } else if (failure != null) {
            r.add("RESULT: FAILED: " + failure.replace('\n', ' '));
            if (failure.contains(".exe"))
                r.add("TODO: the mod is an installer of Windows; unpack it on a computer (Inno Setup) and give the folder");
        } else if (mod != null) {
            r.add("RESULT: installed, " + mod.files.size() + " files, " + Downloads.size(mod.size) + ", ini " + mod.ini);
            List<String> inis = SpineStore.iniFiles(mod.files);
            if (inis.size() > 1)
                r.add("INFO: the ini files of the mod: " + inis + " (taken: " + mod.ini + ")");
            List<String> outside = new ArrayList<>();
            for (String f : mod.files) {
                String top = f.contains("/") ? f.substring(0, f.indexOf('/')).toLowerCase(Locale.ROOT) : "";
                if (!top.equals("data") && !top.equals("system") && !top.equals("_work"))
                    outside.add(f);
            }
            if (!outside.isEmpty())
                r.add("INFO: files outside Data, System, _work (they go to the folder of the game as they are): "
                        + outside.subList(0, Math.min(outside.size(), 12)) + (outside.size() > 12 ? " ..." : ""));
        } else if (title.equals("CHECK")) {
            r.add("RESULT: no Ikarus or LeGo in the scripts that were read");
        }
        List<String> plugins = new ArrayList<>();
        for (String s : skipped) {
            r.add("SKIPPED: " + s);
            if (s.toLowerCase(Locale.ROOT).contains(".dll"))
                plugins.add(s.replaceAll(" \\(.*$", ""));
        }
        if (!plugins.isEmpty()) {
            r.add("WARN: " + plugins.size() + " plugin(s) of Windows (Union, .dll) were left out, the mod may miss features: " + plugins);
            r.add("TODO: find out what these plugins do (see GUIDE) and port or emulate the part that the mod needs");
        }
        if (installers > 0 && mod != null)
            r.add("WARN: " + installers + " installer(s) (.exe) in the archive were left out");
        if (!hints.isEmpty()) {
            for (String line : hints.split("\n"))
                r.add("GUIDE: " + line);
        }
        append(external, storeId, name, title, r);
    }
    /** The version of the application and of the phone, the first lines of every report. */
    static String app(Context c) {
        return "OpenGothic " + Logs.version(c) + ", Android " + android.os.Build.VERSION.RELEASE + ", "
                + android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL;
    }

    /** What the guide of the mod says about what it needs, in short lines (Union, plugins, Ikarus, LeGo...). */
    static String hints(String html) {
        if (html == null || html.isEmpty())
            return "";
        String text = html.replaceAll("(?i)<br\\s*/?>|</p>|</li>|</h\\d>|</div>", "\n").replaceAll("<[^>]+>", "")
                .replace("&nbsp;", " ").replace("&amp;", "&").replace("&quot;", "\"").replace("&lt;", "<").replace("&gt;", ">");
        Pattern key = Pattern.compile("union|plugin|wtyczk|ikarus|lego|\\.dll|g1betterinventory|systempack|gd3d11|wymaga|requires|zainstaluj|install",
                Pattern.CASE_INSENSITIVE);
        Set<String> out = new LinkedHashSet<>();
        for (String line : text.split("\n")) {
            String t = line.trim();
            if (t.length() > 3 && key.matcher(t).find())
                out.add(t.length() > 200 ? t.substring(0, 200) + "..." : t);
            if (out.size() >= 10)
                break;
        }
        StringBuilder b = new StringBuilder();
        for (String s : out)
            b.append(b.length() > 0 ? "\n" : "").append(s);
        return b.toString();
    }

    /** What the install left behind for the report of the play: the skipped plugins and the guide. */
    static void writeMeta(File modDir, SefarisInstaller.Stats stats, String source) {
        Properties p = new Properties();
        p.setProperty("source", source);
        StringBuilder skipped = new StringBuilder();
        for (String s : stats.skipped)
            skipped.append(s).append('\n');
        p.setProperty("skipped", skipped.toString());
        p.setProperty("hints", stats.hints);
        try (FileOutputStream out = new FileOutputStream(new File(modDir, META))) {
            p.store(new OutputStreamWriter(out, StandardCharsets.UTF_8), null);
        } catch (IOException ignored) {
            // the report of the play is shorter then
        }
    }

    private static Properties readMeta(File modDir) {
        Properties p = new Properties();
        try (FileInputStream in = new FileInputStream(new File(modDir, META))) {
            p.load(new InputStreamReader(in, StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // an old install, or a mod that came from elsewhere
        }
        return p;
    }

    // ---- the play

    private static File marker(File external) {
        return new File(dir(external), ".run");
    }

    /**
     * Looks at the mod before it is started and writes what is wrong into its report. The game is started anyway: a warning is no reason
     * to stop, and the engine log that is added afterwards ({@link #afterRun}) says what came of it.
     * @return the short warnings for the player (empty when all is well)
     */
    static List<String> beforePlay(Context c, File external, SpineStore.Mod mod) {
        // the run before this one was not reported yet (the screens did not come back): its log is taken now, the next run overwrites it
        afterRun(c, true);
        List<String> report = new ArrayList<>();
        int game = SpineActions.gameFor(c, external, mod.gameType);
        File root = game >= 0 ? SpineActions.rootOf(c, external, game) : null;
        List<String> warn = check(app(c), root, mod, report);
        report.add("PLAY: started" + (warn.isEmpty() ? "" : " with " + warn.size() + " warning(s)"));
        append(external, mod.id, mod.name, "PLAY", report);

        // the end of the run is looked at when the game is over (afterRun)
        Properties run = new Properties();
        run.setProperty("id", Integer.toString(mod.id));
        run.setProperty("name", mod.name);
        run.setProperty("start", Long.toString(System.currentTimeMillis()));
        run.setProperty("dir", mod.dir != null ? mod.dir.getPath() : "");
        run.setProperty("root", root != null ? root.getPath() : "");
        try (FileOutputStream out = new FileOutputStream(marker(external))) {
            run.store(new OutputStreamWriter(out, StandardCharsets.UTF_8), null);
        } catch (IOException ignored) {
            // the engine log is not added then
        }
        return warn;
    }

    /**
     * What is wrong with the mod before it starts: the ini, the archives that the ini lists, what the install left out. The lines go to
     * {@code report}, the short warnings for the player are returned. {@code root} is the folder of the game, null when it is not there.
     */
    static List<String> check(String app, File root, SpineStore.Mod mod, List<String> report) {
        List<String> warn = new ArrayList<>();
        report.add("mod: " + mod.name + " v" + mod.version + ", " + (mod.gameType == SpineStore.GOTHIC ? "Gothic" : "Gothic II") + ", ini " + mod.ini);
        report.add("app: " + app);
        if (root == null) {
            report.add("ERROR: " + SpineActions.needs(mod.gameType) + " is not installed, the mod cannot start");
            warn.add(Texts.t(SpineActions.needs(mod.gameType) + " is not installed", "Brak " + SpineActions.needs(mod.gameType)));
        } else {
            report.add("game folder: " + root.getPath());
        }
        // the ini that the engine starts with, and the archives that it lists
        File ini = SpineActivation.find(new File(mod.dir, "System"), mod.ini);
        if (ini == null || !ini.isFile()) {
            report.add("ERROR: System/" + mod.ini + " is not in the files of the mod (" + mod.dir.getPath() + ")");
            report.add("TODO: reinstall the mod; if it stays, the ini is missing in the archive");
            warn.add(Texts.t("the ini of the mod is missing", "brak pliku ini moda"));
        } else {
            List<String> vdfs = SpineStore.iniVdfs(ini);
            report.add("ini lists " + vdfs.size() + " archive(s): " + vdfs);
            for (String vdf : vdfs) {
                boolean inMod = false;
                for (String f : mod.files) {
                    if (f.toLowerCase(Locale.ROOT).endsWith("/" + vdf) || f.equalsIgnoreCase(vdf))
                        inMod = true;
                }
                boolean inGame = root != null && (hasFile(new File(root, "Data"), vdf) || hasFile(new File(root, "Data/modvdf"), vdf));
                if (!inMod && !inGame) {
                    report.add("WARN: the ini lists " + vdf + ", but it is neither in the mod nor in Data of the game");
                    report.add("TODO: the archive " + vdf + " is missing: another part of the mod (a patch, a voice pack) has to be installed");
                    warn.add(Texts.t(vdf + " is missing", "brak " + vdf));
                }
            }
        }

        // the archives of the mod: these are what the engine mounts
        List<String> archives = new ArrayList<>();
        long archiveBytes = 0;
        for (String f : mod.files) {
            String low = f.toLowerCase(Locale.ROOT);
            if (low.startsWith("data/") && (low.endsWith(".mod") || low.endsWith(".vdf"))) {
                long size = new File(mod.dir, f).length();
                archiveBytes += size;
                archives.add(f.substring(f.lastIndexOf('/') + 1) + " " + Downloads.size(size));
            }
        }
        report.add("INFO: " + archives.size() + " archive(s) in the mod, " + Downloads.size(archiveBytes) + ": " + archives);

        // the game files that the mod replaces for the time of the play
        if (root != null) {
            List<String> replaced = new ArrayList<>();
            for (String f : mod.files) {
                String low = f.toLowerCase(Locale.ROOT);
                if (low.startsWith("data/modvdf/"))
                    continue;
                File target = root;
                boolean found = true;
                for (String part : f.split("/")) {
                    target = SpineActivation.find(target, part);
                    if (target == null) {
                        found = false;
                        break;
                    }
                }
                if (found)
                    replaced.add(f);
            }
            if (!replaced.isEmpty())
                report.add("INFO: " + replaced.size() + " file(s) of the game are replaced while the mod is played (put back after): "
                        + replaced.subList(0, Math.min(replaced.size(), 15)) + (replaced.size() > 15 ? " ..." : ""));
        }

        // what the install left out
        Properties meta = readMeta(mod.dir);
        String skipped = meta.getProperty("skipped", "").trim();
        if (!skipped.isEmpty()) {
            int plugins = 0;
            for (String s : skipped.split("\n")) {
                report.add("SKIPPED at install: " + s);
                if (s.toLowerCase(Locale.ROOT).contains(".dll"))
                    plugins++;
            }
            if (plugins > 0) {
                report.add("WARN: " + plugins + " plugin(s) of Windows (Union) are not there: the mod may miss features or fail where it calls them");
                warn.add(Texts.t(plugins + " Windows plugin(s) skipped", "pomini\u0119to wtyczki Windows: " + plugins));
            }
        }
        String hints = meta.getProperty("hints", "").trim();
        if (!hints.isEmpty()) {
            for (String line : hints.split("\n"))
                report.add("GUIDE: " + line);
        }

        return warn;
    }

    private static boolean hasFile(File dir, String lowerName) {
        File[] children = dir.listFiles();
        if (children == null)
            return false;
        for (File f : children) {
            if (f.getName().equalsIgnoreCase(lowerName))
                return true;
        }
        return false;
    }

    private static final Pattern PROBLEM = Pattern.compile(
            "error|fail|invalid|unable|cannot|can't|couldn't|missing|not found|no such|unbound|unresolved|unsupported|exception|crash|fatal|warn|assert|denied|corrupt",
            Pattern.CASE_INSENSITIVE);

    private static boolean noise(String line) {
        return line.startsWith("[mem]") || line.startsWith("vkCreate") || line.startsWith("[zenkit] Parsing") || line.startsWith("[talk]")
                || line.contains("DmLoader") || line.contains("SystemPack.ini");
    }

    /** Finds out where the files that the engine asked for are: the names of the archives that hold each name (empty: in none). */
    interface Lookup {
        java.util.Map<String, List<String>> find(Set<String> names);
    }

    /**
     * The game was over: the lines of the log of the engine that name a problem go to the report of the mod that was played, with
     * the way the run ended. The log of the engine is of the last run only, so this has to be done before the next one.
     * Safe to call at any time; it does nothing when no mod was started or the game still runs. The work is done off the screen thread:
     * the catalogs of the archives are read.
     */
    static void afterRun(Context c) {
        afterRun(c, false);
    }

    /** @param sync do it here (the next mod is about to start and its run would overwrite the log of the engine) */
    static void afterRun(final Context c, boolean sync) {
        final File external = c.getExternalFilesDir(null);
        if (external == null || !marker(external).isFile() || SpineActivity.gameRunning(c))
            return;
        final Properties run = new Properties();
        try (FileInputStream in = new FileInputStream(marker(external))) {
            run.load(new InputStreamReader(in, StandardCharsets.UTF_8));
        } catch (IOException e) {
            //noinspection ResultOfMethodCallIgnored
            marker(external).delete();
            return;
        }
        //noinspection ResultOfMethodCallIgnored
        marker(external).delete();
        final int id;
        final long start;
        try {
            id = Integer.parseInt(run.getProperty("id", "0"));
            start = Long.parseLong(run.getProperty("start", "0"));
        } catch (NumberFormatException e) {
            return;
        }
        final File files = c.getFilesDir();
        final String dir = run.getProperty("dir", "");
        final String root = run.getProperty("root", "");
        Runnable work = () -> {
            Lookup lookup = names -> SefarisAssets.find(names, SefarisAssets.archives(dir.isEmpty() ? null : new File(dir), root.isEmpty() ? null : new File(root)));
            List<String> r = runLines(new File(files, "log.txt"), new File(files, "crash.log"), new File(files, "lifecycle.txt"), start, lookup);
            append(external, id, run.getProperty("name", "mod"), "RUN ended", r);
        };
        if (sync)
            work.run();
        else
            new Thread(work, "mod-report").start();
    }

    private static final Pattern LOAD = Pattern.compile("unable to load[^\"]*?\"([^\"]+)\"|unable to load[^\"]*?[:\\s]([\\w\\-./\\\\]+)\\s*$");

    private static String shorten(String t) {
        return t.length() > 300 ? t.substring(0, 300) + "..." : t;
    }

    /**
     * What the engine said in the run that began at {@code start}: the problems (once each, with how many times), the way it ended, the
     * crash. The files are those of the engine. {@code lookup} tells for "unable to load X" whether X is in the archives (may be null).
     */
    static List<String> runLines(File log, File crash, File lifecycle, long start, Lookup lookup) {
        List<String> r = new ArrayList<>();
        if (!log.isFile() || log.lastModified() < start) {
            r.add("ERROR: the engine wrote no log.txt: the game did not start (see the lines above: the ini, the archives)");
            r.add("TODO: the mod does not start; check the ini and the archives that it lists");
        } else {
            String[] lines = Logs.tail(log, 400 * 1024).split("\n");
            java.util.Map<String, Integer> problems = new java.util.LinkedHashMap<>();
            List<String> head = new ArrayList<>();
            for (String line : lines) {
                String t = line.trim();
                if (t.isEmpty() || noise(t))
                    continue;
                boolean problem = PROBLEM.matcher(t).find();
                if (head.size() < 6 && problems.isEmpty() && !problem)
                    head.add(shorten(t));
                if (problem) {
                    String key = shorten(t);
                    Integer n = problems.get(key);
                    if (n != null)
                        problems.put(key, n + 1);
                    else if (problems.size() < 150)
                        problems.put(key, 1);
                }
            }
            // the files that the engine could not load: are they in the archives?
            java.util.Map<String, List<String>> where = new java.util.HashMap<>();
            if (lookup != null) {
                Set<String> asked = new LinkedHashSet<>();
                for (String p : problems.keySet()) {
                    String name = loadedName(p);
                    if (name != null && asked.size() < 200)
                        asked.add(name);
                }
                if (!asked.isEmpty()) {
                    try {
                        where = lookup.find(asked);
                    } catch (RuntimeException ignored) {
                        // no verdict then
                    }
                }
            }
            int missing = 0;
            int unloaded = 0;
            for (String h : head)
                r.add("engine: " + h);
            for (java.util.Map.Entry<String, Integer> e : problems.entrySet()) {
                String line = "ENGINE: " + e.getKey() + (e.getValue() > 1 ? "  (x" + e.getValue() + ")" : "");
                String name = loadedName(e.getKey());
                List<String> in = name != null ? where.get(name) : null;
                if (in != null) {
                    if (in.isEmpty()) {
                        line += "  -> NOT in any archive of the mod or of the game";
                        ++missing;
                    } else {
                        line += "  -> is in " + in + ", the engine failed to load it";
                        ++unloaded;
                    }
                }
                r.add(line);
            }
            if (!problems.isEmpty())
                r.add("TODO: the lines ENGINE above are what the engine complained about: each missing function, file or call is a place to fix");
            if (missing > 0)
                r.add("TODO: " + missing + " file(s) that the engine asked for are in no archive: a part of the mod is missing (a patch, a voice pack), "
                        + "or the name in the world or the script is wrong");
            if (unloaded > 0)
                r.add("TODO: " + unloaded + " file(s) are in the archives but did not load: look at the loader of that kind of file in the engine");
            // how it ended: the last lines that are not the noise of the engine
            List<String> end = new ArrayList<>();
            for (int i = lines.length - 1; i >= 0 && end.size() < 6; --i) {
                String t = lines[i].trim();
                if (!t.isEmpty() && !noise(t))
                    end.add(0, "end: " + shorten(t));
            }
            r.addAll(end);
            r.add("run: about " + Math.max(0, (log.lastModified() - start) / 1000) + " s from the start of the mod to the last line of log.txt");
            String life = Logs.tail(lifecycle, 1500).trim();
            if (!life.isEmpty()) {
                String[] l = life.split("\n");
                r.add("lifecycle: " + l[l.length - 1].trim());
            }
        }
        if (crash.isFile() && crash.lastModified() >= start) {
            r.add("ERROR: the game crashed, crash.log:");
            for (String line : Logs.tail(crash, 2500).split("\n"))
                r.add("  " + line);
            r.add("TODO: the crash of the engine with this mod has to be found (crash.log above)");
        }
        return r;
    }

    /** The name of the file in a line "unable to load ...", or null. */
    static String loadedName(String line) {
        java.util.regex.Matcher m = LOAD.matcher(line);
        if (!m.find())
            return null;
        return m.group(1) != null ? m.group(1) : m.group(2);
    }
    // ---- all the reports

    /** The reports of all the mods in one text, the newest file first; for the screen and for sharing. */
    static String all(File external, int maxBytes) {
        File[] files = dir(external).listFiles((d, n) -> n.endsWith(".log"));
        if (files == null || files.length == 0)
            return "";
        Arrays.sort(files, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        StringBuilder b = new StringBuilder();
        for (File f : files) {
            String part = "######## " + f.getName() + "\n" + Logs.tail(f, 60 * 1024) + "\n";
            if (b.length() + part.length() > maxBytes)
                break;
            b.append(part);
        }
        return b.toString();
    }

    static void clear(File external) {
        File[] files = dir(external).listFiles();
        if (files == null)
            return;
        List<File> list = new ArrayList<>(Arrays.asList(files));
        Collections.sort(list);
        for (File f : list) {
            //noinspection ResultOfMethodCallIgnored
            f.delete();
        }
    }
}

package org.opengothic.app;

import android.content.ContentResolver;
import android.net.Uri;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Mods of the game: folders and zip files in mods/ of the files of the application.
 * The engine reads the same folder, and the list of the enabled mods in mods/.data/mods.json (see common/scripting).
 */
final class ModStore {
    static final long MAX_SIZE = 64L * 1024 * 1024;

    static final class Mod {
        String id;
        String name;
        String version = "";
        String author = "";
        String description = "";
        File file;
        boolean enabled;
    }

    private final File root;
    private final File state;

    ModStore(File external) {
        root = new File(external, "mods");
        state = new File(root, ".data/mods.json");
        //noinspection ResultOfMethodCallIgnored
        root.mkdirs();
    }

    File folder() {
        return root;
    }

    List<Mod> list() {
        List<Mod> ret = new ArrayList<>();
        File[] files = root.listFiles();
        if (files == null)
            return ret;

        Set<String> enabled = enabledIds();
        for (File f : files) {
            if (f.getName().startsWith("."))
                continue;
            Mod m = read(f);
            if (m == null)
                continue;
            m.enabled = enabled.contains(m.id);
            ret.add(m);
        }
        Collections.sort(ret, (a, b) -> a.name.compareToIgnoreCase(b.name));
        return ret;
    }

    void setEnabled(Mod mod, boolean on) {
        Set<String> enabled = enabledIds();
        Set<String> disabled = new HashSet<>();
        try {
            JSONArray off = new JSONObject(readText(state)).optJSONArray("disabled");
            for (int i = 0; off != null && i < off.length(); ++i)
                disabled.add(off.getString(i));
        } catch (JSONException | IOException ignored) {
        }

        if (on) {
            enabled.add(mod.id);
            disabled.remove(mod.id);
        } else {
            enabled.remove(mod.id);
            disabled.add(mod.id);
        }

        try {
            JSONObject json = new JSONObject();
            json.put("enabled", new JSONArray(enabled));
            json.put("disabled", new JSONArray(disabled));
            //noinspection ResultOfMethodCallIgnored
            state.getParentFile().mkdirs();
            try (OutputStream out = new FileOutputStream(state)) {
                out.write((json.toString() + "\n").getBytes(StandardCharsets.UTF_8));
            }
            mod.enabled = on;
        } catch (JSONException | IOException ignored) {
        }
    }

    /** Copies a zip file of a mod. Returns the mod, or throws if the file is not a mod. */
    Mod add(ContentResolver resolver, Uri uri) throws IOException {
        File temp = new File(root, ".incoming.zip");
        try {
            try (InputStream in = resolver.openInputStream(uri)) {
                if (in == null)
                    throw new IOException("Cannot open the file");
                try (OutputStream out = new FileOutputStream(temp)) {
                    byte[] buffer = new byte[65536];
                    long total = 0;
                    int n;
                    while ((n = in.read(buffer)) != -1) {
                        total += n;
                        if (total > MAX_SIZE)
                            throw new IOException("A mod can have up to 64 MiB");
                        out.write(buffer, 0, n);
                    }
                }
            }

            Mod mod = read(temp);
            if (mod == null)
                throw new IOException("This is not a mod: there is no manifest.json");

            File target = new File(root, mod.id + ".zip");
            if (target.exists() && !target.delete())
                throw new IOException("Cannot replace the old version");
            if (!temp.renameTo(target))
                throw new IOException("Cannot save the mod");
            mod.file = target;
            mod.enabled = false;
            return mod;
        } finally {
            //noinspection ResultOfMethodCallIgnored
            temp.delete();
        }
    }

    void delete(Mod mod) {
        remove(mod.file);
        setEnabled(mod, false);
    }

    // ----------------------------------------------------------------------------------------------------

    private Set<String> enabledIds() {
        Set<String> ret = new HashSet<>();
        try {
            JSONArray on = new JSONObject(readText(state)).optJSONArray("enabled");
            for (int i = 0; on != null && i < on.length(); ++i)
                ret.add(on.getString(i));
        } catch (JSONException | IOException ignored) {
        }
        return ret;
    }

    /** The mod in a folder or in a zip file, null if there is none. */
    static Mod read(File f) {
        try {
            String json;
            if (f.isDirectory()) {
                File manifest = new File(f, "manifest.json");
                if (!manifest.isFile())
                    return null;
                json = readText(manifest);
            } else if (f.isFile() && f.getName().toLowerCase().endsWith(".zip")) {
                json = readManifestFromZip(f);
            } else {
                return null;
            }
            if (json == null)
                return null;

            JSONObject o = new JSONObject(json);
            Mod m = new Mod();
            String base = f.getName();
            int dot = base.lastIndexOf('.');
            m.id = o.optString("id", dot > 0 && f.isFile() ? base.substring(0, dot) : base);
            m.name = o.optString("name", m.id);
            m.version = o.optString("version", "");
            m.author = o.optString("author", "");
            m.description = o.optString("description", "");
            m.file = f;
            return m.id.isEmpty() ? null : m;
        } catch (IOException | JSONException e) {
            return null;
        }
    }

    /** manifest.json is in the root of the archive or in the only folder of it. */
    private static String readManifestFromZip(File f) throws IOException {
        try (ZipFile zip = new ZipFile(f)) {
            ZipEntry found = zip.getEntry("manifest.json");
            if (found == null) {
                java.util.Enumeration<? extends ZipEntry> all = zip.entries();
                while (all.hasMoreElements()) {
                    ZipEntry e = all.nextElement();
                    String name = e.getName();
                    int slash = name.indexOf('/');
                    if (slash > 0 && name.substring(slash + 1).equals("manifest.json")) {
                        found = e;
                        break;
                    }
                }
            }
            if (found == null)
                return null;
            try (InputStream in = new BufferedInputStream(zip.getInputStream(found))) {
                return readAll(in);
            }
        }
    }

    private static String readText(File f) throws IOException {
        try (InputStream in = new java.io.FileInputStream(f)) {
            return readAll(in);
        }
    }

    private static String readAll(InputStream in) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int n;
        while ((n = in.read(buffer)) != -1)
            out.write(buffer, 0, n);
        return out.toString("UTF-8");
    }

    private static void remove(File f) {
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children)
                remove(c);
        }
        if (f.exists())
            //noinspection ResultOfMethodCallIgnored
            f.delete();
    }
}

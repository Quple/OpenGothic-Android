package org.opengothic.app;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Looks for the files that the engine could not load in the catalogs of the archives of the mod and of the game (VDF, the .mod and
 * .vdf files). It tells the two causes of "unable to load" apart in the report of a mod: the file is in no archive (a part of the mod
 * is missing, or the world asks for a wrong name), or it is in an archive and the engine failed to load it (a bug of a loader).
 *
 * The files of the archives are the compiled ones (a texture TEX, a mesh MRM/MDL, a sound WAV), and the engine says the name of the
 * source (RAKEPLACE_01.TGA, CHESTBIG_XARDAS.MDS), so the names are compared without the extension and without the "-C" of the
 * compiled textures. A sound that has variants (OW_CROW_01.WAV) is found by its name without the number.
 */
final class SefarisAssets {
    private SefarisAssets() {
    }

    private static final int HEADER = 296;
    private static final int ENTRY = 80;
    private static final long TYPE_DIRECTORY = 0x40000000L;
    private static final long MAX_ENTRIES = 2_000_000L;

    /** The name of a file without its extension, "-C" of a compiled texture and the number of a variant; upper case. */
    static String base(String name) {
        String n = name.trim().toUpperCase(Locale.ROOT);
        int slash = Math.max(n.lastIndexOf('/'), n.lastIndexOf('\\'));
        if (slash >= 0)
            n = n.substring(slash + 1);
        int dot = n.lastIndexOf('.');
        if (dot > 0 && n.length() - dot <= 5)
            n = n.substring(0, dot);
        if (n.endsWith("-C"))
            n = n.substring(0, n.length() - 2);
        return n;
    }

    private static String withoutNumber(String base) {
        return base.replaceAll("_\\d+$", "");
    }

    /**
     * @param names the names that the engine asked for
     * @param archives the archives to look into
     * @return for every name the names of the archives that hold it (an empty list: none does)
     */
    static Map<String, List<String>> find(Set<String> names, List<File> archives) {
        Map<String, List<String>> found = new HashMap<>();
        Map<String, List<String>> byBase = new HashMap<>();
        for (String name : names) {
            found.put(name, new ArrayList<String>());
            String b = base(name);
            List<String> group = byBase.get(b);
            if (group == null)
                byBase.put(b, group = new ArrayList<>());
            group.add(name);
        }
        for (File archive : archives) {
            try {
                scan(archive, found, byBase);
            } catch (IOException | RuntimeException ignored) {
                // an archive that cannot be read is not a reason to stop the report
            }
        }
        return found;
    }

    private static void scan(File archive, Map<String, List<String>> found, Map<String, List<String>> byBase) throws IOException {
        try (RandomAccessFile in = new RandomAccessFile(archive, "r")) {
            if (in.length() < HEADER)
                return;
            byte[] header = new byte[HEADER];
            in.readFully(header);
            if (!new String(header, 256, 12, StandardCharsets.ISO_8859_1).equals("PSVDSC_V2.00"))
                return;
            long entries = u32(header, 272);
            long root = u32(header, 288);
            if (entries <= 0 || entries > MAX_ENTRIES || root < HEADER || root + entries * ENTRY > in.length())
                return;
            in.seek(root);
            byte[] catalog = new byte[(int) Math.min(entries * ENTRY, ENTRY * 13000L)];
            long left = entries * ENTRY;
            while (left > 0) {
                int n = (int) Math.min(left, catalog.length);
                in.readFully(catalog, 0, n);
                left -= n;
                for (int i = 0; i + ENTRY <= n; i += ENTRY) {
                    if ((u32(catalog, i + 72) & TYPE_DIRECTORY) != 0)
                        continue;
                    int len = 0;
                    while (len < 64 && catalog[i + len] != 0)
                        ++len;
                    String entry = base(new String(catalog, i, len, StandardCharsets.ISO_8859_1));
                    List<String> asked = byBase.get(entry);
                    if (asked == null)
                        asked = byBase.get(withoutNumber(entry));
                    if (asked == null)
                        continue;
                    for (String name : asked) {
                        List<String> where = found.get(name);
                        if (where.size() < 3 && !where.contains(archive.getName()))
                            where.add(archive.getName());
                    }
                }
            }
        }
    }

    private static long u32(byte[] b, int at) {
        return (b[at] & 0xffL) | (b[at + 1] & 0xffL) << 8 | (b[at + 2] & 0xffL) << 16 | (b[at + 3] & 0xffL) << 24;
    }

    /** The archives of a mod and of the game: the .mod and .vdf files of Data (the mod's ones lie in Data/modvdf when it is not played). */
    static List<File> archives(File modDir, File gameRoot) {
        List<File> ret = new ArrayList<>();
        if (modDir != null)
            collect(new File(modDir, "Data"), 4, ret);
        if (gameRoot != null)
            collect(SpineActivation.find(gameRoot, "Data") != null ? SpineActivation.find(gameRoot, "Data") : new File(gameRoot, "Data"), 4, ret);
        return ret;
    }

    private static void collect(File dir, int depth, List<File> out) {
        File[] children = dir != null ? dir.listFiles() : null;
        if (children == null)
            return;
        for (File f : children) {
            String low = f.getName().toLowerCase(Locale.ROOT);
            if (f.isDirectory() && depth > 0)
                collect(f, depth - 1, out);
            else if (f.isFile() && (low.endsWith(".mod") || low.endsWith(".vdf")))
                out.add(f);
        }
    }
}

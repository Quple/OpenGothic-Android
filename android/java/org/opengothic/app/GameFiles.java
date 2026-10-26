package org.opengothic.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Are the archives of the game whole. A Gothic archive (VDF) starts with "PSVDSC_V2.00"; an interrupted download or a damaged
 * ZIP leaves files of the right size with no content. The engine then finds no world and has nothing to play.
 */
final class GameFiles {
    private GameFiles() {
    }

    private static final byte[] SIGNATURE = "PSVDSC_V2.00".getBytes(java.nio.charset.StandardCharsets.US_ASCII);

    /** The names of the archives in Data that do not start like a VDF: empty when the installation is good. */
    static List<String> brokenArchives(File root) {
        List<String> broken = new ArrayList<>();
        File[] files = new File(root, "Data").listFiles();
        if (files == null)
            return broken;
        for (File f : files) {
            String name = f.getName().toLowerCase(java.util.Locale.ROOT);
            if (!f.isFile() || !name.endsWith(".vdf"))
                continue;
            if (!hasSignature(f))
                broken.add(f.getName());
        }
        return broken;
    }

    private static boolean hasSignature(File f) {
        byte[] head = new byte[SIGNATURE.length];
        try (InputStream in = new FileInputStream(f)) {
            int done = 0;
            while (done < head.length) {
                int n = in.read(head, done, head.length - done);
                if (n < 0)
                    return false;
                done += n;
            }
        } catch (IOException e) {
            return false;
        }
        return java.util.Arrays.equals(head, SIGNATURE);
    }
}

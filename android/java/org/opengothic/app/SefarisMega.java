package org.opengothic.app;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * A public link of MEGA (a file, or a folder of which the biggest archive is taken) turned into a stream of the file. MEGA gives the
 * address of the data through its API, and the data is encrypted: the key is in the link after the "#", and the file is read with
 * AES-128 in the counter mode. Nothing here needs an account. The MEGA API is described at https://mega.io/developers.
 */
final class SefarisMega {
    private SefarisMega() {
    }

    private static final String API = "https://g.api.mega.co.nz/cs";

    // https://mega.nz/file/ID#KEY, https://mega.nz/folder/ID#KEY[/file/ID], and the old forms #!ID!KEY and #F!ID!KEY
    private static final Pattern LINK = Pattern.compile(
            "mega(?:\\.co)?\\.nz/(?:(file|folder)/([A-Za-z0-9_-]{8})#([A-Za-z0-9_-]+)|#(F?)!([A-Za-z0-9_-]{8})!([A-Za-z0-9_-]+))");

    /** A file that can be read now: the address, the size, and the key to decrypt it. */
    static final class Target {
        String url;
        long size;
        String name = "";
        byte[] key;   // AES-128 key
        byte[] iv;    // the start of the counter (16 bytes)
    }

    static boolean isMega(String url) {
        return url != null && LINK.matcher(url).find();
    }

    /** Asks MEGA where the file is. For a folder the biggest archive in it is taken. */
    static Target resolve(String link) throws IOException {
        Matcher m = LINK.matcher(link);
        if (!m.find())
            throw new IOException("Not a link of MEGA");
        boolean folder = "folder".equals(m.group(1)) || "F".equals(m.group(4));
        String id = m.group(2) != null ? m.group(2) : m.group(5);
        String key = m.group(3) != null ? m.group(3) : m.group(6);
        int slash = key.indexOf('/');
        if (slash >= 0)
            key = key.substring(0, slash);
        byte[] raw = base64(key);
        try {
            return folder ? inFolder(id, raw) : single(id, raw);
        } catch (JSONException e) {
            throw new IOException("MEGA gave an answer that the app does not understand");
        }
    }

    // ---- a file

    private static Target single(String id, byte[] raw) throws IOException, JSONException {
        if (raw.length < 32)
            throw new IOException(Texts.t("The link of MEGA is incomplete.", "Link do MEGA jest niepełny."));
        JSONObject o = call("", "{\"a\":\"g\",\"g\":1,\"ssl\":1,\"p\":\"" + id + "\"}");
        return target(o, raw);
    }

    private static Target target(JSONObject o, byte[] nodeKey) throws IOException, JSONException {
        Target t = new Target();
        t.url = o.optString("g", "");
        t.size = o.optLong("s", 0);
        if (t.url.isEmpty())
            throw new IOException(Texts.t("MEGA did not give the address of the file.", "MEGA nie podało adresu pliku."));
        t.key = new byte[16];
        t.iv = new byte[16];
        for (int i = 0; i < 16; ++i)
            t.key[i] = (byte) (nodeKey[i] ^ nodeKey[i + 16]);
        System.arraycopy(nodeKey, 16, t.iv, 0, 8);
        t.name = name(o.optString("at", ""), t.key);
        return t;
    }

    // ---- a folder

    private static Target inFolder(String folderId, byte[] folderKey) throws IOException, JSONException {
        if (folderKey.length < 16)
            throw new IOException(Texts.t("The link of MEGA is incomplete.", "Link do MEGA jest niepełny."));
        JSONObject listing = call("&n=" + folderId, "{\"a\":\"f\",\"c\":1,\"r\":1}");
        JSONArray nodes = listing.optJSONArray("f");
        if (nodes == null)
            throw new IOException(Texts.t("The folder on MEGA is empty.", "Folder w MEGA jest pusty."));

        String bestHandle = null;
        byte[] bestKey = null;
        long bestSize = -1;
        int bestRank = -1;
        for (int i = 0; i < nodes.length(); ++i) {
            JSONObject n = nodes.getJSONObject(i);
            if (n.optInt("t", -1) != 0)
                continue;   // a folder
            Node node = node(n.optString("k", ""), n.optString("a", ""), folderKey);
            if (node == null || node.key.length < 32)
                continue;
            String name = node.name.toLowerCase(Locale.ROOT);
            boolean archive = name.endsWith(".zip") || name.endsWith(".rar") || name.endsWith(".7z");
            // the mod is an archive, in the folder itself (older versions lie in the folders inside), and the biggest of them
            // (the others are patches, voice packs, readmes)
            int rank = (archive ? 2 : 0) + (folderId.equals(n.optString("p", "")) ? 1 : 0);
            long size = n.optLong("s", 0);
            if (bestHandle == null || rank > bestRank || (rank == bestRank && size > bestSize)) {
                bestHandle = n.getString("h");
                bestKey = node.key;
                bestSize = size;
                bestRank = rank;
            }
        }        if (bestHandle == null)
            throw new IOException(Texts.t("There is no file in the folder on MEGA.", "W folderze MEGA nie ma pliku."));
        JSONObject file = call("&n=" + folderId, "{\"a\":\"g\",\"g\":1,\"ssl\":1,\"n\":\"" + bestHandle + "\"}");
        if (file.optString("at", "").isEmpty()) {
            // the attributes of the file come with the listing, not with the address
            for (int i = 0; i < nodes.length(); ++i) {
                if (bestHandle.equals(nodes.getJSONObject(i).optString("h")))
                    file.put("at", nodes.getJSONObject(i).optString("a", ""));
            }
        }
        return target(file, bestKey);
    }

    /** A node of a folder: its key (16 bytes for a folder, 32 for a file) and its name. */
    private static final class Node {
        byte[] key;
        String name = "";
    }

    /**
     * The key of a node of a folder is encrypted with the key of the folder: "handle:key", and more of them separated by "/" when the
     * node is shared in more places. The key that makes the attributes readable is the right one.
     */
    private static Node node(String k, String attributes, byte[] folderKey) {
        Node first = null;
        for (String part : k.split("/")) {
            int colon = part.indexOf(':');
            if (colon < 0)
                continue;
            try {
                byte[] enc = base64(part.substring(colon + 1));
                if (enc.length % 16 != 0 || enc.length == 0)
                    continue;
                Cipher c = Cipher.getInstance("AES/ECB/NoPadding");
                c.init(Cipher.DECRYPT_MODE, new SecretKeySpec(Arrays.copyOf(folderKey, 16), "AES"));
                Node n = new Node();
                n.key = c.doFinal(enc);
                byte[] aes = n.key;
                if (n.key.length >= 32) {
                    aes = new byte[16];
                    for (int i = 0; i < 16; ++i)
                        aes[i] = (byte) (n.key[i] ^ n.key[i + 16]);
                }
                n.name = name(attributes, aes);
                if (!n.name.isEmpty())
                    return n;
                if (first == null)
                    first = n;
            } catch (Exception e) {
                // a key that is not for this folder
            }
        }
        return first;
    }
    // ---- the data

    /** The stream of the file, decrypted on the way. Ask for more only after the end of the first one is read. */
    static InputStream open(Target t, HttpURLConnection[] connection) throws IOException {
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(t.url).openConnection();
        connection[0] = c;
        c.setConnectTimeout(20000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", "OpenGothic-Android");
        int code = c.getResponseCode();
        if (code == 509 || code == 429)
            throw new IOException(Texts.t("MEGA has limited the transfer for now (a limit of data for one address). Try again in some hours.",
                    "MEGA ograniczyło transfer (limit danych dla jednego adresu). Spróbuj za kilka godzin."));
        if (code != 200)
            throw new IOException(Texts.t("MEGA answered " + code + ".", "MEGA odpowiedziało " + code + "."));
        try {
            Cipher cipher = Cipher.getInstance("AES/CTR/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(t.key, "AES"), new IvParameterSpec(t.iv));
            return new javax.crypto.CipherInputStream(c.getInputStream(), cipher);
        } catch (Exception e) {
            throw new IOException("Cannot decrypt: " + e);
        }
    }

    // ---- the API

    private static int sequence = (int) (System.nanoTime() & 0xffffff);

    /** One command of the API; the answer is the object of the first (and only) command. */
    private static JSONObject call(String query, String command) throws IOException, JSONException {
        IOException last = null;
        for (int attempt = 0; attempt < 4; ++attempt) {
            try {
                String answer = post(API + "?id=" + (sequence++) + query, "[" + command + "]");
                answer = answer.trim();
                if (answer.startsWith("[")) {
                    Object first = new JSONArray(answer).get(0);
                    if (first instanceof JSONObject) {
                        JSONObject o = (JSONObject) first;
                        if (o.has("e") && o.optInt("e") < 0)
                            throw error(o.optInt("e"));
                        return o;
                    }
                    if (first instanceof Integer)
                        throw error((Integer) first);
                }
                throw error(Integer.parseInt(answer));   // a number alone is an error code
            } catch (Retry r) {
                last = r;
                try {
                    Thread.sleep(1500L * (attempt + 1));
                } catch (InterruptedException ie) {
                    throw new SpineInstaller.Cancelled();
                }
            } catch (NumberFormatException e) {
                throw new IOException("MEGA gave an answer that the app does not understand");
            }
        }
        throw last;
    }

    /** MEGA asks to try again (-3, -4: the server is busy). */
    private static final class Retry extends IOException {
        Retry(String message) {
            super(message);
        }
    }

    private static IOException error(int code) {
        switch (code) {
            case -3:
            case -4:
                return new Retry("MEGA is busy");
            case -9:
            case -8:
            case -11:
                return new IOException(Texts.t("The file was removed from MEGA, or the link is wrong.", "Plik został usunięty z MEGA albo link jest błędny."));
            case -16:
                return new IOException(Texts.t("The file on MEGA is blocked.", "Plik w MEGA jest zablokowany."));
            case -17:
                return new IOException(Texts.t("MEGA has limited the transfer for now. Try again in some hours.",
                        "MEGA ograniczyło transfer. Spróbuj za kilka godzin."));
            default:
                return new IOException("MEGA error " + code);
        }
    }

    private static String post(String url, String body) throws IOException {
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        try {
            c.setConnectTimeout(20000);
            c.setReadTimeout(30000);
            c.setRequestMethod("POST");
            c.setDoOutput(true);
            c.setRequestProperty("Content-Type", "text/plain;charset=UTF-8");
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            try (OutputStream out = c.getOutputStream()) {
                out.write(body.getBytes(StandardCharsets.UTF_8));
            }
            if (c.getResponseCode() != 200)
                throw new IOException("MEGA answered " + c.getResponseCode());
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0)
                    out.write(buf, 0, n);
                return new String(out.toByteArray(), StandardCharsets.UTF_8);
            }
        } finally {
            c.disconnect();
        }
    }

    // ---- the pieces of the format

    /** The base64 of MEGA: the URL form, without the padding. */
    static byte[] base64(String s) throws IOException {
        s = s.replace('-', '+').replace('_', '/').replace(",", "");
        while (s.length() % 4 != 0)
            s += "=";
        try {
            return android.util.Base64.decode(s, android.util.Base64.DEFAULT);
        } catch (IllegalArgumentException e) {
            throw new IOException(Texts.t("The link of MEGA is damaged.", "Link do MEGA jest uszkodzony."));
        }
    }

    /** The name of the file from its attributes: "MEGA{...}" encrypted with AES-CBC and the key of the file. */
    private static String name(String attributes, byte[] key) {
        try {
            if (attributes.isEmpty())
                return "";
            byte[] enc = base64(attributes);
            if (enc.length % 16 != 0 || enc.length == 0)
                return "";
            Cipher c = Cipher.getInstance("AES/CBC/NoPadding");
            c.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"), new IvParameterSpec(new byte[16]));
            String text = new String(c.doFinal(enc), StandardCharsets.UTF_8);
            if (!text.startsWith("MEGA"))
                return "";
            int end = text.indexOf('\0');
            return new JSONObject(end >= 0 ? text.substring(4, end) : text.substring(4)).optString("n", "");
        } catch (Exception e) {
            return "";
        }
    }
}

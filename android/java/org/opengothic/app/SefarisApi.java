package org.opengothic.app;

import android.text.Html;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The catalog of Gothic mods of https://sefaris.eu (the public API of the page, see https://api.sefaris.eu/swagger). It lists the mods
 * and their pages; it does not host the files: the download links lead to Google Drive, MEGA, ModDB and other places.
 * The mods are kept in {@link SpineStore} under the id {@link #ID_BASE} + the id of Sefaris, so they cannot meet the ids of Spine.
 */
final class SefarisApi {
    private SefarisApi() {
    }

    static final String API = "https://api.sefaris.eu";

    /** The id in the store of the downloaded mods; the ids of Spine are far below it. */
    static final int ID_BASE = 1000000;

    /** The games of Sefaris that OpenGothic can run (the others are Gothic 3, Risen, Gothic Online...). */
    static final int GOTHIC = 0;
    static final int GOTHIC2 = 1;

    static final class Link {
        String type = "";
        String url = "";
    }

    static final class Mod {
        int id;
        String externalId = "";
        String name = "";
        String version = "";
        String updated = "";           // "2026-11-21 19:33:48Z", it sorts as text
        int game;
        List<String> tags = new ArrayList<>();

        // the page of the mod, loaded by details()
        boolean detailed;
        String overview = "";
        String installation = "";
        List<Link> links = new ArrayList<>();

        int storeId() {
            return ID_BASE + id;
        }

        boolean hasTag(String tag) {
            for (String t : tags) {
                if (t.equalsIgnoreCase(tag))
                    return true;
            }
            return false;
        }
    }

    /** The mod that is downloaded, as an entry of the list (when the list of the server is not there). */
    static Mod of(SpineStore.Mod m) {
        Mod ret = new Mod();
        ret.id = m.id - ID_BASE;
        ret.name = m.name;
        ret.version = m.version;
        ret.game = m.gameType;
        return ret;
    }

    /** The mods for Gothic and Gothic II that are released. */
    static List<Mod> list() throws IOException {
        List<Mod> ret = new ArrayList<>();
        try {
            JSONArray items = new JSONObject(get("/Public/Mod")).getJSONArray("items");
            for (int i = 0; i < items.length(); i++) {
                JSONObject o = items.getJSONObject(i);
                int game = o.optInt("game", -1);
                if ((game != GOTHIC && game != GOTHIC2) || o.optBoolean("incoming", false))
                    continue;
                Mod m = new Mod();
                m.id = o.getInt("id");
                m.externalId = o.optString("externalId", "");
                m.name = o.optString("name", "");
                m.version = o.optString("version", "");
                m.updated = o.optString("lastUpdateDate", "");
                m.game = game;
                JSONArray tags = o.optJSONArray("tags");
                for (int t = 0; tags != null && t < tags.length(); t++)
                    m.tags.add(tags.getJSONObject(t).optString("name", ""));
                if (!m.externalId.isEmpty() && !m.name.isEmpty())
                    ret.add(m);
            }
        } catch (JSONException e) {
            throw new IOException("Unexpected answer of sefaris.eu");
        }
        return ret;
    }

    /** Loads the description, the guide and the download links of the mod. */
    static void details(Mod m) throws IOException {
        if (m.detailed || m.externalId.isEmpty())
            return;
        try {
            JSONObject o = new JSONObject(get("/Public/Mod/" + m.externalId));
            m.overview = o.optString("overview", "");
            if (m.overview.isEmpty())
                m.overview = o.optString("description", "");
            m.installation = o.optString("installationGuide", "");
            m.links = new ArrayList<>();
            JSONObject d = o.optJSONObject("details");
            JSONArray links = d != null ? d.optJSONArray("downloadLinks") : null;
            for (int i = 0; links != null && i < links.length(); i++) {
                Link l = new Link();
                l.type = links.getJSONObject(i).optString("type", "");
                l.url = links.getJSONObject(i).optString("url", "");
                if (l.url.startsWith("http"))
                    m.links.add(l);
            }
            m.detailed = true;
        } catch (JSONException e) {
            throw new IOException("Unexpected answer of sefaris.eu");
        }
    }

    // ---- the links

    private static final Pattern DRIVE_FILE = Pattern.compile("drive\\.google\\.com/file/d/([A-Za-z0-9_-]+)");
    private static final Pattern DRIVE_ID = Pattern.compile("[?&]id=([A-Za-z0-9_-]+)");

    /** The id of a file on Google Drive that a link points to (not a folder), or null. */
    static String driveId(String url) {
        if (!url.contains("google.com"))
            return null;
        Matcher m = DRIVE_FILE.matcher(url);
        if (m.find())
            return m.group(1);
        m = DRIVE_ID.matcher(url);
        return m.find() && !url.contains("/folders/") ? m.group(1) : null;
    }

    /** The address that gives the file itself, without the page that asks to confirm a big file. */
    static String driveUrl(String id) {
        return "https://drive.usercontent.google.com/download?id=" + id + "&export=download&confirm=t";
    }

    /** The first link of a Google Drive file of the mod, or null. */
    static String driveFile(Mod m) {
        for (Link l : m.links) {
            String id = driveId(l.url);
            if (id != null)
                return id;
        }
        return null;
    }

    /** The first public link of MEGA of the mod, or null. */
    static String megaLink(Mod m) {
        for (Link l : m.links) {
            if (SefarisMega.isMega(l.url))
                return l.url;
        }
        return null;
    }

    /** The links that only a browser can follow: pages (ModDB, MediaFire), folders of Google Drive. Not the shops, not what is read directly. */
    static List<Link> pages(Mod m) {
        List<Link> ret = new ArrayList<>();
        java.util.Set<String> hosts = new java.util.HashSet<>();
        for (Link l : m.links) {
            if (l.type.equals("steam") || l.type.equals("gog") || driveId(l.url) != null || SefarisMega.isMega(l.url))
                continue;
            if (hosts.add(host(l.url)))
                ret.add(l);
        }
        return ret;
    }

    static String host(String url) {
        try {
            String h = new java.net.URI(url).getHost();
            return h == null ? url : h.startsWith("www.") ? h.substring(4) : h;
        } catch (java.net.URISyntaxException e) {
            return url;
        }
    }

    /** The first link that a person can open to get the files (not the Steam and GOG shops), or null. */
    static String page(Mod m) {
        for (Link l : m.links) {
            if (!l.type.equals("steam") && !l.type.equals("gog"))
                return l.url;
        }
        return null;
    }

    /** The text of a page of the mod without the tags. */
    static CharSequence text(String html) {
        return Html.fromHtml(html, Html.FROM_HTML_MODE_LEGACY);
    }

    // ---- the network

    private static String get(String path) throws IOException {
        Tls.ensure();
        HttpURLConnection c = (HttpURLConnection) new URL(API + path).openConnection();
        try {
            c.setConnectTimeout(20000);
            c.setReadTimeout(30000);
            c.setRequestProperty("User-Agent", "OpenGothic-Android");
            c.setRequestProperty("Accept", "application/json");
            if (c.getResponseCode() != 200)
                throw new IOException("sefaris.eu answered " + c.getResponseCode());
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[16384];
                int n;
                while ((n = in.read(buf)) > 0)
                    out.write(buf, 0, n);
                return new String(out.toByteArray(), StandardCharsets.UTF_8);
            }
        } finally {
            c.disconnect();
        }
    }
}

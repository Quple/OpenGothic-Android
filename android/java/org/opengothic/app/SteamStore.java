package org.opengothic.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.HashSet;
import java.util.Set;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * The session of Steam that is kept between the starts: the account and the refresh token. The password is never kept.
 * The token is encrypted with a key of the Android Keystore, which does not leave it. When the Keystore does not work
 * the token is not kept at all and the player signs in again.
 */
final class SteamStore {
    static final class Session {
        final String steamId;
        final String account;
        final String refreshToken;

        Session(String steamId, String account, String refreshToken) {
            this.steamId = steamId;
            this.account = account;
            this.refreshToken = refreshToken;
        }
    }

    private static final String ALIAS = "opengothic.steam";
    private final SharedPreferences prefs;

    SteamStore(Context context) {
        prefs = context.getSharedPreferences("steam", Context.MODE_PRIVATE);
    }

    Session load() {
        String id = prefs.getString("steamid", null);
        String account = prefs.getString("account", "");
        String iv = prefs.getString("iv", null);
        String token = prefs.getString("token", null);
        if (id == null || iv == null || token == null)
            return null;
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, Base64.decode(iv, Base64.NO_WRAP)));
            String refresh = new String(cipher.doFinal(Base64.decode(token, Base64.NO_WRAP)), StandardCharsets.UTF_8);
            return new Session(id, account, refresh);
        } catch (Exception e) {
            // the key is gone (a new lock of the screen, a restore of the phone): the session is lost
            clear();
            return null;
        }
    }

    /** False when it cannot be kept safely. */
    boolean save(Session s) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true));
            byte[] data = cipher.doFinal(s.refreshToken.getBytes(StandardCharsets.UTF_8));
            prefs.edit()
                    .putString("steamid", s.steamId)
                    .putString("account", s.account)
                    .putString("iv", Base64.encodeToString(cipher.getIV(), Base64.NO_WRAP))
                    .putString("token", Base64.encodeToString(data, Base64.NO_WRAP))
                    .apply();
            return true;
        } catch (Exception e) {
            clear();
            return false;
        }
    }

    /** The apps of the account that the launcher knows, kept so that the library is there at once and without a connection. */
    void saveOwned(Set<Integer> apps) {
        StringBuilder s = new StringBuilder();
        for (int a : apps)
            s.append(s.length() > 0 ? "," : "").append(a);
        prefs.edit().putString("owned", s.toString()).apply();
    }

    /** Null when it was never asked for. */
    Set<Integer> loadOwned() {
        String s = prefs.getString("owned", null);
        if (s == null)
            return null;
        Set<Integer> ret = new HashSet<>();
        for (String part : s.split(",")) {
            try {
                ret.add(Integer.parseInt(part.trim()));
            } catch (NumberFormatException ignored) {
            }
        }
        return ret;
    }

    void clear() {
        prefs.edit().clear().apply();
        try {
            KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
            ks.load(null);
            ks.deleteEntry(ALIAS);
        } catch (Exception ignored) {
        }
    }

    private static SecretKey key(boolean create) throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        if (ks.containsAlias(ALIAS))
            return (SecretKey) ks.getKey(ALIAS, null);
        if (!create)
            throw new IllegalStateException("no key");

        KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        gen.init(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build());
        return gen.generateKey();
    }
}

package org.opengothic.app;

import android.content.Context;
import android.content.Intent;
import android.os.Build;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Set;

/**
 * The launcher reaches the part that needs the libraries of Steam through this class and by the names of the classes only,
 * so a build that has no libraries (build-apk.sh) still works and simply has no Steam.
 */
final class SteamBridge {
    /** The preferences of the launcher and the mark of a player who went on without an account. */
    static final String PREFS = "launcher";
    static final String SKIPPED = "skipped";

    private SteamBridge() {
    }

    /** Steam says no, or cannot be reached. signIn: the session is no good any more. */
    static final class Denied extends Exception {
        final boolean signIn;

        Denied(String message, boolean signIn) {
            super(message);
            this.signIn = signIn;
        }
    }

    static boolean available() {
        if (Build.VERSION.SDK_INT < 26)
            return false;
        try {
            Class.forName("org.opengothic.app.SteamSession", false, SteamBridge.class.getClassLoader());
            return true;
        } catch (ClassNotFoundException | LinkageError e) {
            return false;
        }
    }

    static Intent loginIntent(Context c) {
        return new Intent().setClassName(c, "org.opengothic.app.SteamLoginActivity");
    }

    static Intent downloadIntent(Context c, int appId, String folder, String title, String language) {
        return new Intent().setClassName(c, "org.opengothic.app.SteamDownloadService")
                .putExtra("app", appId)
                .putExtra("folder", folder)
                .putExtra("title", title)
                .putExtra("language", language);
    }

    static Intent stopIntent(Context c) {
        return new Intent().setClassName(c, "org.opengothic.app.SteamDownloadService")
                .setAction("org.opengothic.app.STEAM_STOP");
    }

    /** Which of the apps the account owns. Blocks: not for the main thread. */
    @SuppressWarnings("unchecked")
    static Set<Integer> owned(SteamStore.Session session, Set<Integer> ids) throws Denied {
        try {
            Class<?> k = Class.forName("org.opengothic.app.SteamOwnership");
            Method m = k.getMethod("owned", String.class, String.class, Set.class);
            return (Set<Integer>) m.invoke(null, session.account, session.refreshToken, ids);
        } catch (InvocationTargetException e) {
            Throwable cause = e.getCause();
            if (cause instanceof Denied)
                throw (Denied) cause;
            throw new Denied(String.valueOf(cause), false);
        } catch (ReflectiveOperationException | LinkageError e) {
            throw new Denied("Steam is not in this build.", false);
        }
    }
}

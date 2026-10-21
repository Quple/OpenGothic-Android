package org.opengothic.app;

import java.util.Set;

/** The games of the account, asked for with the session that was kept. Called by name from SteamBridge. */
public final class SteamOwnership {
    private SteamOwnership() {
    }

    public static Set<Integer> owned(String account, String refreshToken, Set<Integer> ids) throws SteamBridge.Denied {
        SteamSession steam = null;
        try {
            steam = new SteamSession();
            steam.connect();
            steam.resume(account, refreshToken);
            return steam.owned(ids);
        } catch (SteamSession.Failure e) {
            throw new SteamBridge.Denied(e.getMessage(), e.signIn);
        } finally {
            if (steam != null)
                steam.close();
        }
    }
}

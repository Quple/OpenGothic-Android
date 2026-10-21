package org.opengothic.app;

import in.dragonbra.javasteam.depotdownloader.DepotDownloader;
import in.dragonbra.javasteam.depotdownloader.IDownloadListener;
import in.dragonbra.javasteam.depotdownloader.data.AppItem;
import in.dragonbra.javasteam.depotdownloader.data.DownloadItem;
import in.dragonbra.javasteam.enums.EResult;
import in.dragonbra.javasteam.steam.authentication.AuthPollResult;
import in.dragonbra.javasteam.steam.authentication.AuthSessionDetails;
import in.dragonbra.javasteam.steam.authentication.AuthenticationException;
import in.dragonbra.javasteam.steam.authentication.IAuthenticator;
import in.dragonbra.javasteam.steam.handlers.steamapps.License;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSProductInfo;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSRequest;
import in.dragonbra.javasteam.steam.handlers.steamapps.SteamApps;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.LicenseListCallback;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.PICSProductInfoCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.LogOnDetails;
import in.dragonbra.javasteam.steam.handlers.steamuser.SteamUser;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOffCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback;
import in.dragonbra.javasteam.steam.steamclient.SteamClient;
import in.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager;
import in.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.callbacks.DisconnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.configuration.SteamConfiguration;
import in.dragonbra.javasteam.types.KeyValue;

import android.util.Log;

import org.bouncycastle.jce.provider.BouncyCastleProvider;

import java.io.Closeable;
import java.io.IOException;
import java.security.SecureRandom;
import java.security.Security;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * One connection to Steam: the login (the password and Steam Guard, or the session that was kept), the games of the account
 * and the download of a game. It is the client of the game itself, as the program of Steam is, not the web site:
 * only a session of this kind is allowed to download the content. Every method blocks, so they belong to a thread of their own.
 * The library is JavaSteam (MIT), it needs Android 8 (java.time).
 */
final class SteamSession implements Closeable {
    private static final String TAG = "OpenGothic/Steam";

    static {
        // JavaSteam asks for the provider "BC" by name (SHA-1, AES of the content). Android has a "BC" of its own with the
        // algorithms taken out and a provider of a name that is taken is not added, so the old one has to go first.
        Security.removeProvider("BC");
        Security.insertProviderAt(new BouncyCastleProvider(), 1);
    }

    /** What the player has to be asked for. The answers are given from any thread. */
    interface Prompt {
        /** The code of Steam Guard from the app of Steam or from the e-mail. Complete it with null to give up. */
        void code(boolean email, String emailHint, boolean previousWasWrong, CompletableFuture<String> answer);

        /** Steam waits until the login is approved in the app of Steam: only to tell the player. */
        void approve();
    }

    interface Progress {
        void status(String text);

        /** A part of the whole game, 0..1, and the bytes that are done. */
        void percent(float done, long bytes);
    }

    static final class Failure extends Exception {
        /** The session has ended or the password is not right: the player has to sign in again. */
        final boolean signIn;

        Failure(String message, boolean signIn) {
            super(message);
            this.signIn = signIn;
        }
    }

    static final class Login {
        String account;
        String refreshToken;
        String steamId;
    }

    private final SteamClient client;
    private final CallbackManager manager;
    private final SteamUser user;
    private final SteamApps apps;
    private final List<Closeable> subscriptions = new ArrayList<>();

    private volatile boolean running = true;
    private volatile boolean closing;
    private Thread pump;

    private volatile CompletableFuture<Void> connected = new CompletableFuture<>();
    private volatile CompletableFuture<LoggedOnCallback> loggedOn = new CompletableFuture<>();
    private volatile CompletableFuture<List<License>> licenses = new CompletableFuture<>();
    private volatile CompletableFuture<Void> picsDone;
    private final Map<Integer, PICSProductInfo> packages = new java.util.concurrent.ConcurrentHashMap<>();
    private volatile DepotDownloader downloader;

    SteamSession() {
        client = new SteamClient(SteamConfiguration.create(builder -> { }));
        manager = new CallbackManager(client);
        user = client.getHandler(SteamUser.class);
        apps = client.getHandler(SteamApps.class);

        subscriptions.add(manager.subscribe(ConnectedCallback.class, c -> connected.complete(null)));
        subscriptions.add(manager.subscribe(DisconnectedCallback.class, this::onDisconnected));
        subscriptions.add(manager.subscribe(LoggedOnCallback.class, c -> loggedOn.complete(c)));
        subscriptions.add(manager.subscribe(LoggedOffCallback.class, c -> {
            // only a token that is no good means a new login; being replaced by another session of the account is not that
            EResult r = c.getResult();
            boolean again = r == EResult.AccessDenied || r == EResult.InvalidPassword || r == EResult.Expired
                    || r == EResult.InvalidLoginAuthCode || r == EResult.AccountDisabled;
            Log.w(TAG, "logged off: " + r);
            failAll(new Failure(again ? "The session has ended, sign in again." : "Steam has ended the session: " + r + ".", again));
        }));
        subscriptions.add(manager.subscribe(LicenseListCallback.class, c -> {
            if (c.getResult() == EResult.OK)
                licenses.complete(c.getLicenseList());
            else
                licenses.completeExceptionally(new Failure("Steam did not give the list of the licenses.", false));
        }));
        subscriptions.add(manager.subscribe(PICSProductInfoCallback.class, this::onProductInfo));

        pump = new Thread(() -> {
            while (running)
                manager.runWaitCallbacks(1000L);
        }, "steam-callbacks");
        pump.setDaemon(true);
        pump.start();
    }

    // ---- the login

    void connect() throws Failure {
        connected = new CompletableFuture<>();
        client.connect();
        await(connected, 30, "Steam does not answer, check the connection.");
    }

    /** The password goes to Steam encrypted, the library does it. Steam Guard is asked for through the prompt. */
    Login login(String account, String password, String deviceName, final Prompt prompt) throws Failure {
        AuthSessionDetails details = new AuthSessionDetails();
        details.username = account;
        details.password = password;
        details.deviceFriendlyName = deviceName;
        details.persistentSession = true;
        details.authenticator = new IAuthenticator() {
            @Override
            public CompletableFuture<String> getDeviceCode(boolean previousCodeWasIncorrect) {
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(false, null, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<String> getEmailCode(String email, boolean previousCodeWasIncorrect) {
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(true, email, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<Boolean> acceptDeviceConfirmation() {
                prompt.approve();
                // the library waits for the approval of the app of Steam
                return CompletableFuture.completedFuture(Boolean.TRUE);
            }
        };

        AuthPollResult result;
        try {
            result = client.getAuthentication().beginAuthSessionViaCredentials(details).get()
                    .pollingWaitForResult().get();
        } catch (AuthenticationException e) {
            throw explain(e);
        } catch (ExecutionException e) {
            throw explain(e.getCause() != null ? e.getCause() : e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Failure("The login was interrupted.", false);
        } catch (CancellationException e) {
            throw new Failure("The login was cancelled.", false);
        }

        Login login = new Login();
        login.account = result.getAccountName();
        login.refreshToken = result.getRefreshToken();
        logOn(login.account, login.refreshToken);
        login.steamId = Long.toString(client.getSteamID().convertToUInt64());
        return login;
    }

    /** The session that was kept: no password and no code. */
    Login resume(String account, String refreshToken) throws Failure {
        logOn(account, refreshToken);
        Login login = new Login();
        login.account = account;
        login.refreshToken = refreshToken;
        login.steamId = Long.toString(client.getSteamID().convertToUInt64());
        return login;
    }

    private void logOn(String account, String refreshToken) throws Failure {
        loggedOn = new CompletableFuture<>();
        licenses = new CompletableFuture<>();
        LogOnDetails details = new LogOnDetails();
        details.setUsername(account);
        details.setAccessToken(refreshToken);
        details.setShouldRememberPassword(true);
        // every session has a number of its own, so the screen and a download can be signed in at the same time
        details.setLoginID(new SecureRandom().nextInt(Integer.MAX_VALUE - 1) + 1);
        user.logOn(details);

        LoggedOnCallback r = await(loggedOn, 30, "Steam did not answer the login.");
        Log.i(TAG, "logon: " + r.getResult());
        if (r.getResult() != EResult.OK) {
            boolean again = r.getResult() == EResult.AccessDenied || r.getResult() == EResult.InvalidPassword
                    || r.getResult() == EResult.Expired || r.getResult() == EResult.InvalidLoginAuthCode;
            throw new Failure(again ? "The session has ended, sign in again." : "Steam refused the login: " + r.getResult() + ".", again);
        }
    }

    // ---- the games

    /** Which of the apps the account owns: the licenses are packages, a package lists its apps. */
    Set<Integer> owned(Set<Integer> wanted) throws Failure {
        List<License> list = await(licenses, 30, "Steam did not give the list of the licenses.");
        List<PICSRequest> requests = new ArrayList<>();
        for (License l : list)
            requests.add(new PICSRequest(l.getPackageID(), l.getAccessToken()));

        Set<Integer> ret = new HashSet<>();
        if (requests.isEmpty())
            return ret;

        // the answer comes in parts, every part is a callback
        packages.clear();
        picsDone = new CompletableFuture<>();
        apps.picsGetProductInfo(Collections.<PICSRequest>emptyList(), requests);
        await(picsDone, 60, "Steam did not give the games of the account.");

        for (PICSProductInfo info : packages.values()) {
            KeyValue ids = info.getKeyValues().get("appids");
            if (ids == null)
                continue;
            for (KeyValue id : ids.getChildren()) {
                Integer app = id.asInteger(0);
                if (wanted.contains(app))
                    ret.add(app);
            }
        }
        return ret;
    }

    private void onProductInfo(PICSProductInfoCallback c) {
        packages.putAll(c.getPackages());
        if (!c.isResponsePending() && picsDone != null)
            picsDone.complete(null);
    }

    // ---- the download

    /**
     * The files of the Windows version of the game go to the folder, in the language that was chosen (the name that Steam
     * gives it, "polish"). An interrupted download goes on from what is there.
     */
    void download(int appId, java.io.File dir, boolean verify, String language, final Progress progress) throws Failure {
        List<License> list = await(licenses, 30, "Steam did not give the list of the licenses.");
        final CompletableFuture<Void> done = new CompletableFuture<>();
        final long[] bytes = {0};
        final Failure[] failed = {null};

        IDownloadListener listener = new IDownloadListener() {
            @Override
            public void onItemAdded(DownloadItem item) {
            }

            @Override
            public void onDownloadStarted(DownloadItem item) {
                progress.status("Downloading...");
            }

            @Override
            public void onDownloadCompleted(DownloadItem item) {
                done.complete(null);
            }

            @Override
            public void onDownloadFailed(DownloadItem item, Throwable error) {
                failed[0] = new Failure("The download failed: " + String.valueOf(error.getMessage()), false);
                done.complete(null);
            }

            @Override
            public void onStatusUpdate(String message) {
                progress.status(message);
            }

            @Override
            public void onFileCompleted(int depotId, String fileName, float depotPercentComplete) {
            }

            @Override
            public void onChunkCompleted(int depotId, float depotPercentComplete, long compressedBytes, long uncompressedBytes) {
                bytes[0] += uncompressedBytes;
                progress.percent(depotPercentComplete, bytes[0]);
            }

            @Override
            public void onDepotCompleted(int depotId, long compressedBytes, long uncompressedBytes) {
            }
        };

        // the arguments: debug, LAN cache, chunks loaded, chunks unpacked and files written at the same time, the Windows build
        DepotDownloader d = new DepotDownloader(client, list, false, false, 8, 2, 1, true, null);
        downloader = d;
        try {
            d.addListener(listener);
            // the game, the files of Windows and every architecture, in the language that the player has chosen:
            // what has no language (the world, the textures) comes with it
            d.add(new AppItem(appId, false, dir.getAbsolutePath(), null, null,
                    false, "windows", true, null, false, language, false,
                    Collections.<Integer>emptyList(), Collections.<Long>emptyList(), verify, false));
            d.finishAdding();
            while (!done.isDone() && running) {
                try {
                    done.get(1, TimeUnit.SECONDS);
                } catch (TimeoutException ignored) {
                    // the download goes on
                } catch (ExecutionException | InterruptedException e) {
                    break;
                }
            }
            if (!running && !done.isDone())
                throw new Failure("The download was stopped.", false);
            if (failed[0] != null)
                throw failed[0];
        } finally {
            downloader = null;
            try {
                d.close();
            } catch (Exception ignored) {
            }
        }
    }

    /** Stops what is going on, the files stay for the next try. */
    void cancel() {
        running = false;
        DepotDownloader d = downloader;
        if (d != null) {
            try {
                d.close();
            } catch (Exception ignored) {
            }
        }
        failAll(new Failure("Stopped.", false));
    }

    boolean isCancelled() {
        return !running;
    }

    @Override
    public void close() {
        closing = true;
        running = false;
        try {
            if (client.isConnected())
                user.logOff();
        } catch (Exception ignored) {
        }
        try {
            client.disconnect();
        } catch (Exception ignored) {
        }
        for (Closeable c : subscriptions) {
            try {
                c.close();
            } catch (IOException ignored) {
            }
        }
        subscriptions.clear();
    }

    // ---- details

    private void onDisconnected(DisconnectedCallback c) {
        if (!closing && !c.isUserInitiated())
            failAll(new Failure("The connection to Steam was lost.", false));
    }

    private void failAll(Failure f) {
        connected.completeExceptionally(f);
        loggedOn.completeExceptionally(f);
        licenses.completeExceptionally(f);
        CompletableFuture<Void> p = picsDone;
        if (p != null)
            p.completeExceptionally(f);
    }

    private <T> T await(CompletableFuture<T> future, int seconds, String timeout) throws Failure {
        try {
            return future.get(seconds, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            throw new Failure(timeout, false);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Failure("Interrupted.", false);
        } catch (ExecutionException e) {
            throw explain(e.getCause() != null ? e.getCause() : e);
        } catch (CancellationException e) {
            throw new Failure("Stopped.", false);
        }
    }

    private static Failure explain(Throwable e) {
        if (e instanceof Failure)
            return (Failure) e;
        if (e instanceof AuthenticationException) {
            EResult r = ((AuthenticationException) e).getResult();
            if (r == EResult.InvalidPassword)
                return new Failure("Wrong account name or password.", false);
            if (r == EResult.RateLimitExceeded || r == EResult.AccountLoginDeniedThrottle)
                return new Failure("Too many tries, wait a while before the next one.", false);
            if (r == EResult.TwoFactorCodeMismatch || r == EResult.InvalidLoginAuthCode)
                return new Failure("The Steam Guard code is wrong.", false);
            return new Failure("Steam refused the login" + (r != null ? ": " + r : "") + ".", false);
        }
        String m = e.getMessage();
        return new Failure(m != null && !m.isEmpty() ? m : e.getClass().getSimpleName(), false);
    }
}

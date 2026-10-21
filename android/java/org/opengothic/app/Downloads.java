package org.opengothic.app;

/** What the download of a game from Steam is doing: the service writes it, the screens read it. */
final class Downloads {
    private Downloads() {
    }

    static final class State {
        final int appId;
        final String title;
        volatile String text = "";
        volatile float percent;
        volatile long bytes;
        volatile boolean done;
        volatile String error;

        State(int appId, String title) {
            this.appId = appId;
            this.title = title;
        }

        boolean running() {
            return !done && error == null;
        }
    }

    static volatile State state;

    /** The download that goes on now, or null. */
    static State running() {
        State s = state;
        return s != null && s.running() ? s : null;
    }
}

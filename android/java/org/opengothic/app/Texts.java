package org.opengothic.app;

import java.util.Locale;

/** The texts of the launcher: Polish when the phone speaks Polish, English otherwise. */
final class Texts {
    private Texts() {
    }

    static boolean polish() {
        return "pl".equals(Locale.getDefault().getLanguage());
    }

    static String t(String en, String pl) {
        return polish() ? pl : en;
    }

    /** The language of the game that fits the language of the phone: the name that Steam gives it. */
    static String steamLanguage() {
        switch (Locale.getDefault().getLanguage()) {
            case "pl": return "polish";
            case "de": return "german";
            case "ru": return "russian";
            case "fr": return "french";
            case "es": return "spanish";
            case "it": return "italian";
            case "cs": return "czech";
            default: return "english";
        }
    }
}

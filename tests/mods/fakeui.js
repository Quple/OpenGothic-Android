// A pretended ui (see common/scripting/game/game_ui.cpp) for the tests of the tabs of mods: it remembers what was
// drawn and presses the buttons and types into the fields when the test says so.
"use strict";

class FakeUi {
    constructor() {
        this.out = [];
        this.pressed = new Set();
        this.typed = {};
        this.tabs = [];
    }

    // --- the test
    clear() { this.out = []; }
    press(label) { this.pressed.add(label); }
    has(text) { return this.out.some(l => l.includes(text)); }

    // --- what the mods use
    tab(title, fn) { this.tabs.push({ title, fn }); }
    text(...a) { this.out.push(a.join(" ")); }
    textWrapped(...a) { this.out.push(a.join(" ")); }
    textDisabled(...a) { this.out.push(a.join(" ")); }
    textColored(c, ...a) { this.out.push(a.join(" ")); }
    bullet(...a) { this.out.push("* " + a.join(" ")); }
    button(label) {
        this.out.push("[" + label + "]");
        return this.pressed.delete(label);
    }
    checkbox(label, v) { this.out.push("(" + label + ")"); return v; }
    inputText(label, value) {
        this.out.push("<" + label + ":" + value + ">");
        return label in this.typed ? this.typed[label] : value;
    }
    sliderInt(l, v) { return v; }
    sliderFloat(l, v) { return v; }
    combo(l, i) { return i; }
    selectable() { return false; }
    sameLine() {}
    separator() {}
    spacing() {}
    indent() {}
    unindent() {}
    setNextWidth() {}
    fontSize() { return 16; }
    width() { return 400; }
    textWidth(t) { return String(t).length * 8; }
    anyActive() { return false; }
    child(id, w, h, fn) { fn(); }
    withId(id, fn) { fn(); }
    withColor(c, fn) { fn(); }
    withScale(s, fn) { fn(); }
}

module.exports = { FakeUi };

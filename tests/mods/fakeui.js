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
        const shown = String(label).split("##")[0];
        this.out.push("[" + shown + "]");
        return this.pressed.delete(shown);
    }
    checkbox(label, v) { this.out.push("(" + label + ")"); return v; }
    inputText(label, value) {
        this.out.push("<" + label + ":" + value + ">");
        return label in this.typed ? this.typed[label] : value;
    }
    sliderInt(l, v) { return v; }
    sliderFloat(l, v) { return v; }
    combo(l, i) { return i; }
    selectable(label) {
        const shown = String(label).split("##")[0];
        this.out.push("{" + shown + "}");
        return this.pressed.delete(shown);
    }
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
    clipped(count, fn) { for (let i = 0; i < count; ++i) fn(i); }
    autoScroll() { this.scrolled = (this.scrolled ?? 0) + 1; }
    withId(id, fn) { fn(); }
    withColor(c, fn) { fn(); }
    withScale(s, fn) { fn(); }
}

module.exports = { FakeUi };

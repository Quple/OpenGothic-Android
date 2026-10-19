// The log of the game in the menu: the lines of log.txt as they come, with a filter. A line of the engine and of the
// scripts, so the errors of Ikarus and LeGo (mem_*: address translation failure, not implemented call [...]) are here.
"use strict";

const Red = [0.95, 0.40, 0.35, 1];
const Gray = [0.55, 0.55, 0.55, 1];
const Max = 4000;   // the lines that are kept

/// Words are separated by spaces and all of them have to be in the line, a word that begins with - must not be
function matcher(filter) {
    const words = filter.toLowerCase().split(/\s+/).filter(w => w.length > 0);
    const must = words.filter(w => !w.startsWith("-")), not = words.filter(w => w.startsWith("-") && w.length > 1).map(w => w.slice(1));
    return (text) => {
        const t = text.toLowerCase();
        return must.every(w => t.includes(w)) && !not.some(w => t.includes(w));
    };
}

class LogView {
    constructor(debug) {
        this.debug = debug;
        this.lines = [];        // { level, text }
        this.serial = 0;
        this.filter = "";
        this.showInfo = true;
        this.showDebug = false;
        this.paused = false;
        this.shown = [];
        this.dirty = true;
    }

    /// New lines of the game, every frame
    poll() {
        if (this.paused) return;
        const r = this.debug.log(this.serial, 400);
        this.serial = r.serial;
        if (r.lines.length === 0) return;
        for (const [level, text] of r.lines) this.lines.push({ level, text });
        if (this.lines.length > Max) this.lines.splice(0, this.lines.length - Max);
        this.dirty = true;
    }

    clear() {
        this.lines = [];
        this.dirty = true;
    }

    visible() {
        if (this.dirty) {
            const ok = matcher(this.filter);
            this.shown = this.lines.filter(l => (l.level === 1 || (l.level === 0 && this.showInfo) || (l.level === 2 && this.showDebug)) && ok(l.text));
            this.dirty = false;
        }
        return this.shown;
    }

    /// The lines that have come after a serial of the log, for the answer of a call
    since(serial, count = 8) {
        const r = this.debug.log(serial, 200);
        return r.lines.slice(-count).map(([level, text]) => ({ level, text }));
    }

    draw(ui, height) {
        const filter = ui.inputText("##logfilter", this.filter);
        if (filter !== this.filter) {
            this.filter = filter;
            this.dirty = true;
        }
        ui.sameLine();
        ui.textDisabled("filter: words, -word");

        const quick = (label, text) => {
            if (ui.button(label)) { this.filter = text; this.dirty = true; }
            ui.sameLine();
        };
        quick("all", "");
        quick("mem_", "mem_");
        quick("not implemented", "not implemented");
        quick("TODO", "todo");
        quick("Ikarus", "ikarus");
        if (ui.button("clear")) this.clear();

        const info = ui.checkbox("info", this.showInfo);
        ui.sameLine();
        const dbg = ui.checkbox("debug", this.showDebug);
        ui.sameLine();
        const pause = ui.checkbox("pause", this.paused);
        if (info !== this.showInfo || dbg !== this.showDebug) this.dirty = true;
        this.showInfo = info; this.showDebug = dbg; this.paused = pause;
        ui.sameLine();
        ui.textDisabled(`${this.visible().length} of ${this.lines.length} lines`);

        const rows = this.visible();
        ui.child("##log", -1, height, () => {
            ui.clipped(rows.length, (i) => {
                const l = rows[i];
                if (l.level === 1) ui.withColor(Red, () => ui.text(l.text));
                else if (l.level === 2) ui.withColor(Gray, () => ui.text(l.text));
                else ui.text(l.text);
            });
            if (!this.paused) ui.autoScroll();
        });
    }
}

module.exports = { LogView, matcher };

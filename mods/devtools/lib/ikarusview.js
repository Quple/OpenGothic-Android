// What the layer for Ikarus and LeGo says: the memory, the movers that the scripts see as zCMover, the hooks that
// nobody answers. The errors of the libraries are in the log (the button mem_ there).
"use strict";

const Gold = [0.82, 0.66, 0.34, 1];
const Green = [0.45, 0.85, 0.45, 1];
const Gray = [0.55, 0.55, 0.55, 1];

class IkarusView {
    constructor(env) {
        this.env = env;          // { debug }
        this.lines = [];
        this.filter = "";
        this.shown = [];
        this.read = false;
        this.versions = "";
    }

    refresh() {
        const text = this.env.debug.ikarus();
        this.lines = text === "" ? [] : text.split("\n");
        const ik = this.env.debug.symbol("Ikarus_Version"), lego = this.env.debug.symbol("LeGo_Version");
        this.versions = [ik ? `Ikarus ${ik.value}` : "", lego ? `LeGo ${lego.value}` : ""].filter(s => s).join(", ");
        this.read = true;
        this.filterLines();
    }

    filterLines() {
        const f = this.filter.toLowerCase();
        this.shown = f === "" ? this.lines : this.lines.filter(l => l.toLowerCase().includes(f));
    }

    draw(ui) {
        if (!this.read) this.refresh();

        ui.withColor(Gold, () => ui.text("Ikarus and LeGo"));
        if (this.versions === "") {
            ui.textWrapped("The scripts of this game do not use Ikarus. Try Chronicles of Myrtana, or another mod that has it.");
        } else {
            ui.withColor(Green, () => ui.text(`This game has ${this.versions}`));
        }
        if (ui.button("Refresh")) this.refresh();
        ui.sameLine();
        ui.setNextWidth(ui.fontSize() * 14);
        const f = ui.inputText("filter##ikfilter", this.filter);
        if (f !== this.filter) {
            this.filter = f;
            this.filterLines();
        }

        ui.child("##ikarus", -1, 0, () => {
            ui.clipped(this.shown.length, (i) => {
                const l = this.shown[i];
                if (l.startsWith("  ")) ui.withColor(Gray, () => ui.text(l)); else ui.text(l);
            });
        });
    }
}

module.exports = { IkarusView };

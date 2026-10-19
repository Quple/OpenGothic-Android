// The functions and the symbols of the scripts of the game (Daedalus): search, call with the arguments, read and write.
"use strict";

const Gold = [0.82, 0.66, 0.34, 1];
const Green = [0.45, 0.85, 0.45, 1];
const Red = [0.95, 0.40, 0.35, 1];
const Who = ["nobody", "the player", "the nearest character"];

/// "func int NAME(int a, string b)" -> [{type, name}]
function parseParams(sig) {
    const m = /\((.*)\)\s*$/.exec(sig ?? "");
    if (!m || m[1].trim() === "") return [];
    return m[1].split(",").map(p => {
        const [type, ...name] = p.trim().split(/\s+/);
        return { type, name: name.join(" ") };
    });
}

class ScriptView {
    constructor(env) {
        this.env = env;          // { debug, player, npcs, log: LogView }
        this.query = "";
        this.found = [];
        this.lastQuery = null;
        this.selected = null;
        this.params = [];
        this.signature = "";
        this.args = [];
        this.self = 0;
        this.other = 0;
        this.result = null;
        this.tail = [];
        this.symQuery = "";
        this.symFound = [];
        this.lastSym = null;
        this.symSelected = null;
        this.symInfo = null;
        this.symValue = "";
        this.symMessage = "";
    }

    pick(mode) {
        if (mode === 1) return this.env.player;
        if (mode === 2) return this.env.npcs.nearby(2500)[0] ?? null;
        return null;
    }

    select(name) {
        this.selected = name;
        this.signature = this.env.debug.signature(name) ?? "";
        this.params = parseParams(this.signature);
        this.args = this.params.map(p => (p.type === "string" ? "" : "0"));
        this.result = null;
    }

    call() {
        const dbg = this.env.debug;
        const serial = this.env.log.serial;
        const args = this.params.map((p, i) => {
            if (p.type === "int" || p.type === "func") return Number(this.args[i]) || 0;
            if (p.type === "float") return Number(this.args[i]) || 0;
            if (p.type === "string") return this.args[i];
            return this.pick(Number(this.args[i]) || 0);   // a character
        });
        this.result = dbg.call(this.selected, args, { self: this.pick(this.self), other: this.pick(this.other) });
        // what the call has said in the log
        this.env.log.poll();
        this.tail = this.env.log.since(serial, 10);
    }

    drawFunctions(ui, height) {
        ui.withColor(Gold, () => ui.text("Functions of the scripts"));
        ui.setNextWidth(-ui.fontSize() * 5);
        this.query = ui.inputText("##fnquery", this.query);
        if (this.query !== this.lastQuery) {
            this.lastQuery = this.query;
            this.found = this.query.length >= 2 ? this.env.debug.functions(this.query, 60) : [];
        }
        ui.sameLine();
        ui.textDisabled("2+ letters");

        ui.child("##fnlist", -1, ui.fontSize() * 7, () => {
            ui.clipped(this.found.length, (i) => {
                if (ui.selectable(this.found[i] + "##f" + i, this.found[i] === this.selected)) this.select(this.found[i]);
            });
        });
        if (!this.selected) return;

        ui.withColor(Green, () => ui.textWrapped(this.signature));
        this.params.forEach((p, i) => {
            ui.withId("arg" + i, () => {
                ui.setNextWidth(ui.fontSize() * 14);
                if (p.type === "int" || p.type === "float" || p.type === "func" || p.type === "string") {
                    this.args[i] = ui.inputText(`${p.name} (${p.type})`, String(this.args[i]), p.type === "string" ? "" : "decimal");
                } else {
                    this.args[i] = ui.combo(`${p.name} (${p.type})`, Number(this.args[i]) || 0, Who);
                }
            });
        });
        ui.setNextWidth(ui.fontSize() * 14);
        this.self = ui.combo("self", this.self, Who);
        ui.setNextWidth(ui.fontSize() * 14);
        this.other = ui.combo("other", this.other, Who);

        if (ui.button("Call", -1, ui.fontSize() * 2.2)) this.call();
        if (this.result) {
            if (this.result.ok) ui.withColor(Green, () => ui.text("ok" + (this.result.result !== undefined ? `, returned ${this.result.result}` : "")));
            else ui.withColor(Red, () => ui.textWrapped(this.result.error));
            for (const l of this.tail) ui.withColor(l.level === 1 ? Red : [0.7, 0.7, 0.7, 1], () => ui.textWrapped(l.text));
        }
    }

    selectSymbol(name) {
        this.symSelected = name;
        this.symInfo = this.env.debug.symbol(name);
        this.symValue = this.symInfo && this.symInfo.value !== undefined ? String(this.symInfo.value) : "";
        this.symMessage = "";
    }

    drawSymbols(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Variables and constants"));
        ui.setNextWidth(-ui.fontSize() * 5);
        this.symQuery = ui.inputText("##symquery", this.symQuery);
        if (this.symQuery !== this.lastSym) {
            this.lastSym = this.symQuery;
            this.symFound = this.symQuery.length >= 2 ? this.env.debug.symbols(this.symQuery, 60) : [];
        }
        ui.sameLine();
        ui.textDisabled("2+ letters");
        ui.child("##symlist", -1, ui.fontSize() * 6, () => {
            ui.clipped(this.symFound.length, (i) => {
                if (ui.selectable(this.symFound[i] + "##s" + i, this.symFound[i] === this.symSelected)) this.selectSymbol(this.symFound[i]);
            });
        });
        if (!this.symSelected || !this.symInfo) return;

        ui.text(`${this.symSelected}: ${this.symInfo.type}${this.symInfo.const ? " (const)" : ""}`);
        ui.setNextWidth(ui.fontSize() * 14);
        this.symValue = ui.inputText("value##symvalue", this.symValue, this.symInfo.type === "string" ? "" : "decimal");
        ui.sameLine();
        if (ui.button("Set")) {
            const v = this.symInfo.type === "string" ? this.symValue : Number(this.symValue);
            this.symMessage = this.env.debug.setSymbol(this.symSelected, v) ? "set" : "cannot be set";
            this.symInfo = this.env.debug.symbol(this.symSelected);
        }
        ui.sameLine();
        if (ui.button("Read")) this.selectSymbol(this.symSelected);
        if (this.symMessage) ui.textDisabled(this.symMessage);
    }

    draw(ui, height) {
        this.drawFunctions(ui, height);
        this.drawSymbols(ui);
    }
}

module.exports = { ScriptView, parseParams };

// The tab Dialogs: the dialogs of the scripts of a mod (Chronicles of Myrtana), their conditions, a simulation of one dialog or of all of
// them, and the report of what the game still lacks for the mod. The work is done by the game (game/dialogtest.cpp): it reads the
// condition of a dialog from the bytecode, sets what it needs (variables, dialogs that were told, status of a quest, items, place,
// time), runs the dialog and lets the queue of its commands go on at once.
//
// NOTE: a simulation changes the state of the game (quests, items, places). Save before, run it on a game that you can throw away.
"use strict";

const Gold = [0.82, 0.66, 0.34, 1];
const Green = [0.45, 0.85, 0.45, 1];
const Red = [0.95, 0.40, 0.35, 1];
const Amber = [0.95, 0.75, 0.30, 1];
const Grey = [0.6, 0.6, 0.6, 1];
const Blue = [0.50, 0.75, 0.95, 1];

const StatusColor = {
    ok: Green,
    issues: Amber,
    error: Red,
    "cond-unsolved": Red,
    "cond-false": Amber,
    "no-npc": Grey,
    "no-world": Grey,
    unknown: Grey,
};

const ReportFile = "dialogtest_report.txt";
const PageSize = 400;

class DialogView {
    constructor(env) {
        this.env = env;           // { debug, ... }
        this.filter = "";
        this.appliedFilter = null;
        this.rows = [];
        this.total = 0;
        this.selected = null;
        this.analysis = "";
        this.result = null;
        this.solve = true;
        this.execute = true;
        this.queue = null;        // the run of all: { names, pos, started, step }
        this.lastRefresh = 0;
        this.message = "";
        this.fnQuery = "";
        this.fnLast = null;
        this.fnFound = [];
        this.fnSelected = null;
        this.fnSelf = "";
        this.fnResult = null;
        this.section = "Dialogs";
        this.perFrame = 3;
    }

    refresh() {
        const r = this.env.debug.dlgList(this.filter, 0, PageSize);
        this.rows = r ? r.rows : [];
        this.total = r ? r.total : 0;
        this.appliedFilter = this.filter;
        this.lastRefresh = Date.now();
    }

    select(row) {
        this.selected = row;
        this.analysis = this.env.debug.dlgAnalyze(row.name) ?? "";
        this.result = null;
    }

    runSelected(solve, execute) {
        if (!this.selected) return;
        this.result = this.env.debug.dlgRun(this.selected.name, solve, execute);
        this.analysis = this.env.debug.dlgAnalyze(this.selected.name) ?? "";
        this.refresh();
    }

    // all the dialogs of the filter, the whole list (the page of the list is only for the eyes). resume: the results of the run that the
    // game did not finish are read from dialogtest_progress.txt, only the dialogs without a result are run (the one that has crashed
    // the game is marked and left)
    startAll(resume) {
        let have = 0;
        if (resume) have = this.env.debug.dlgResume(); else this.env.debug.dlgReset();
        const names = [];
        let skip = 0;
        for (;;) {
            const r = this.env.debug.dlgList(this.filter, skip, 500);
            if (!r || r.rows.length === 0) break;
            for (const row of r.rows) if (!resume || !row.status) names.push(row.name);
            skip += r.rows.length;
            if (skip >= r.total) break;
        }
        this.env.debug.dlgGuard(true);   // the scripts that the dialogs prepare must not end the game between two of them
        this.queue = { names, pos: 0, started: Date.now(), lastRefresh: 0 };
        this.message = resume ? `Resumed: ${have} results read from dialogtest_progress.txt, ${names.length} dialogs to run...` : `Running ${names.length} dialogs...`;
    }

    stopAll(reason) {
        if (!this.queue) return;
        const q = this.queue;
        this.queue = null;
        this.env.debug.dlgGuard(false);
        const ok = this.env.debug.dlgSave(ReportFile);
        this.message = `${reason}: ${q.pos}/${q.names.length} dialogs in ${((Date.now() - q.started) / 1000).toFixed(1)} s. ` +
            (ok ? `The report is in ${ReportFile} (next to log.txt) and in the log.` : "The report could not be written.");
        const report = this.env.debug.dlgReport();
        if (report) for (const line of report.split("\n").slice(0, 80)) console.log(line);
        this.refresh();
    }

    // called every frame: some of the dialogs of the run of all
    tickAll() {
        const q = this.queue;
        if (!q) return;
        for (let i = 0; i < this.perFrame && q.pos < q.names.length; ++i) {
            const r = this.env.debug.dlgRun(q.names[q.pos++], this.solve, this.execute);
            if (r && r.status === "no-world") { this.stopAll("There is no world"); return; }
        }
        if (Date.now() - q.lastRefresh > 1500) {
            q.lastRefresh = Date.now();
            this.refresh();
        }
        if (q.pos >= q.names.length) this.stopAll("Done");
    }

    drawRow(ui, row, i) {
        const color = StatusColor[row.status] ?? (row.known ? Grey : [0.85, 0.85, 0.85, 1]);
        const mark = row.status ? `[${row.status}] ` : "";
        const desc = row.description ? ` "${row.description}"` : "";
        const label = `${row.npc}: ${row.name}${desc}  ${mark}${row.inWorld ? "" : "(npc not in this world) "}${row.known ? "(told) " : ""}${row.important ? "(important)" : ""}##d${i}`;
        ui.withColor(color, () => {
            if (ui.selectable(label, this.selected && this.selected.name === row.name)) this.select(row);
        });
    }

    drawDialogs(ui) {
        ui.setNextWidth(-ui.fontSize() * 8);
        this.filter = ui.inputText("##dlgfilter", this.filter);
        ui.sameLine();
        if (ui.button("Search", ui.fontSize() * 6, 0) || this.appliedFilter === null) this.refresh();
        ui.textDisabled("words must all appear in the dialog, the character or the text; status:error, status:ok, status:none filter by the last run");

        this.solve = ui.checkbox("Solve the conditions##solve", this.solve);
        ui.sameLine();
        this.execute = ui.checkbox("Run the dialog##exec", this.execute);

        if (this.queue) {
            const q = this.queue;
            const width = 30;
            const done = Math.floor(width * q.pos / Math.max(1, q.names.length));
            ui.withColor(Blue, () => ui.text(`[${"#".repeat(done)}${".".repeat(width - done)}] ${q.pos}/${q.names.length}`));
            ui.sameLine();
            if (ui.button("Stop##stopall", ui.fontSize() * 5, 0)) this.stopAll("Stopped");
        } else {
            ui.withColor(Red, () => {
                if (ui.button(`Test all (${this.total})##testall`, ui.fontSize() * 11, ui.fontSize() * 2)) this.startAll(false);
            });
            ui.sameLine();
            if (ui.button("Resume##resume", ui.fontSize() * 6, ui.fontSize() * 2)) this.startAll(true);
            ui.sameLine();
            if (ui.button("Report##report", ui.fontSize() * 6, ui.fontSize() * 2)) {
                const ok = this.env.debug.dlgSave(ReportFile);
                const report = this.env.debug.dlgReport();
                if (report) for (const line of report.split("\n").slice(0, 120)) console.log(line);
                this.message = ok ? `The report is in ${ReportFile} and in the log.` : "The report could not be written.";
            }
            ui.sameLine();
            if (ui.button("Forget results##reset", ui.fontSize() * 9, ui.fontSize() * 2)) { this.env.debug.dlgReset(); this.refresh(); }
        }
        if (this.message) ui.textWrapped(this.message);
        ui.withColor(Gold, () => ui.text(`${this.total} dialogs${this.total > this.rows.length ? ` (the first ${this.rows.length} are listed)` : ""}, ${this.env.debug.dlgCount()} runs`));

        ui.child("##dlglist", -1, ui.fontSize() * 14, () => {
            ui.clipped(this.rows.length, (i) => this.drawRow(ui, this.rows[i], i));
        });

        if (!this.selected) return;
        ui.separator();
        const s = this.selected;
        ui.withColor(Gold, () => ui.textWrapped(`${s.name}  (${s.npc}${s.npcName ? ", " + s.npcName : ""})`));
        if (ui.button("Solve and run##runsel", ui.fontSize() * 11, ui.fontSize() * 2)) this.runSelected(true, true);
        ui.sameLine();
        if (ui.button("Run as it is##runplain", ui.fontSize() * 10, ui.fontSize() * 2)) this.runSelected(false, true);
        ui.sameLine();
        if (ui.button("Analyze##analyze", ui.fontSize() * 7, ui.fontSize() * 2)) this.analysis = this.env.debug.dlgAnalyze(s.name) ?? "";
        ui.sameLine();
        if (ui.button("Go to the character##goto", ui.fontSize() * 12, ui.fontSize() * 2)) {
            const found = this.env.debug.findNpc(s.npcName || s.npc);
            if (found && found.length) this.message = "Use comcheck npc " + (s.npcName || s.npc) + " go";
        }

        if (this.result) {
            const col = StatusColor[this.result.status] ?? Grey;
            ui.withColor(col, () => ui.textWrapped(this.result.text));
        }
        ui.withColor(Blue, () => ui.textWrapped(this.analysis));
    }

    drawScenes(ui) {
        ui.withColor(Gold, () => ui.text("Scenes and events (functions of the scripts)"));
        ui.textDisabled("Run a function as a scene: its commands go through at once. Try: CUTSCENE, TRIALOG, CAMERA, EVT_, _APPLY");
        ui.setNextWidth(-ui.fontSize() * 5);
        this.fnQuery = ui.inputText("##fnq", this.fnQuery);
        if (this.fnQuery !== this.fnLast) {
            this.fnLast = this.fnQuery;
            this.fnFound = this.fnQuery.length >= 3 ? this.env.debug.functions(this.fnQuery, 200) : [];
        }
        ui.sameLine();
        ui.textDisabled("3+ letters");
        ui.child("##fnl", -1, ui.fontSize() * 10, () => {
            ui.clipped(this.fnFound.length, (i) => {
                if (ui.selectable(this.fnFound[i] + "##sf" + i, this.fnFound[i] === this.fnSelected)) { this.fnSelected = this.fnFound[i]; this.fnResult = null; }
            });
        });
        if (!this.fnSelected) return;
        ui.setNextWidth(ui.fontSize() * 16);
        this.fnSelf = ui.inputText("self (an instance, empty: the player)##fnself", this.fnSelf);
        if (ui.button("Run the scene##runfn", ui.fontSize() * 12, ui.fontSize() * 2)) {
            this.fnResult = this.env.debug.dlgRunFn(this.fnSelected, this.fnSelf);
        }
        if (this.fnResult) {
            const col = StatusColor[this.fnResult.status] ?? Grey;
            ui.withColor(col, () => ui.textWrapped(this.fnResult.text));
        }
    }

    draw(ui) {
        for (const name of ["Dialogs", "Scenes"]) {
            ui.withColor(name === this.section ? Gold : [0.85, 0.85, 0.85, 1], () => {
                if (ui.button(name + "##dsec", ui.fontSize() * 7, 0)) this.section = name;
            });
            ui.sameLine();
        }
        ui.withColor(Red, () => ui.text("changes the state of the game: use a throw-away save"));
        ui.separator();
        if (this.section === "Dialogs") this.drawDialogs(ui); else this.drawScenes(ui);
    }
}

module.exports = { DialogView };

// Unit tests of the JavaScript of the developer tools (mods/devtools). They run without the game, with a fake `debug` and a fake `ui`:
//
//   node --test tests/js
//
// The tests that need the game are in mods/devtools/lib/selftests.js (the tab Debug, Tests; the console: comcheck test).
"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");

const lib = (name) => require(path.join(__dirname, "..", "..", "mods", "devtools", "lib", name));

// --- fakes

function fakeConsole() {
    const c = { lines: [], errors: [], commands: {} };
    c.log = (...a) => c.lines.push(a.join(" "));
    c.error = (...a) => c.errors.push(a.join(" "));
    c.registerCommand = (name, fn) => { c.commands[name] = fn; };
    return c;
}

/// A dialog list of `n` dialogs, `known` of them with a status already (a run that was read back from the file of the progress)
function fakeDebug(n, known = 0) {
    const names = Array.from({ length: n }, (_, i) => `DIA_TEST_${String(i).padStart(4, "0")}`);
    const status = new Map();
    for (let i = 0; i < known; ++i) status.set(names[i], "ok");
    const d = {
        calls: [],
        guard: false,
        saved: null,
        resets: 0,
        resumes: 0,
        ran: [],
        dlgList(filter, skip, max) {
            const rows = names.slice(skip, skip + max).map(name => ({ name, npc: "NPC", npcName: "Npc", description: "d", status: status.get(name) || "", known: false, inWorld: true }));
            return { total: names.length, rows };
        },
        dlgReset() { this.resets++; status.clear(); },
        dlgResume() { this.resumes++; return known; },
        dlgGuard(on) { this.guard = on; this.calls.push(["guard", on]); },
        dlgRun(name, solve, execute) {
            this.ran.push(name);
            status.set(name, "ok");
            return { name, status: "ok", text: name, applied: [], unsolved: [], issues: [] };
        },
        dlgSave(file) { this.saved = file; return true; },
        dlgReport() { return "DIALOG TEST REPORT\nruns: " + this.ran.length; },
        dlgCount() { return this.ran.length; },
        dlgAnalyze(name) { return "analysis of " + name; },
        findNpc() { return []; },
        functions() { return []; },
    };
    return d;
}

// --- the filter of the log

test("log filter: all words must be in the line, -word excludes", () => {
    const { matcher } = lib("logview.js");
    const m = matcher("not implemented -wld");
    assert.equal(m("not implemented call [HookEngineI]"), true);
    assert.equal(m("not implemented call [wld_israining]"), false);
    assert.equal(m("something else"), false);
    assert.equal(matcher("")("anything"), true);
    assert.equal(matcher("-")("a - b"), true, "a lone minus is not an exclusion");
});

test("function signature of the script is split into the parameters", () => {
    const { parseParams } = lib("scriptview.js");
    assert.deepEqual(parseParams("func void F()"), []);
    assert.deepEqual(parseParams("func int NAME(int a, string b)"), [{ type: "int", name: "a" }, { type: "string", name: "b" }]);
    assert.deepEqual(parseParams("func void X(C_NPC slf)"), [{ type: "C_NPC", name: "slf" }]);
    assert.deepEqual(parseParams(null), []);
});

// --- the tab Dialogs

test("Test all: fresh start forgets the old results, guards the exit, lists all pages", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(1250);
    const v = new DialogView({ debug });
    v.startAll(false);
    assert.equal(debug.resets, 1);
    assert.equal(debug.resumes, 0);
    assert.equal(debug.guard, true, "the scripts must not end the game during the run");
    assert.equal(v.queue.names.length, 1250, "more than one page of the list (500 per page)");
    assert.equal(new Set(v.queue.names).size, 1250);
});

test("Resume: only the dialogs without a result are run", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(900, 600);
    const v = new DialogView({ debug });
    v.startAll(true);
    assert.equal(debug.resumes, 1);
    assert.equal(debug.resets, 0, "the results of the file must stay");
    assert.equal(v.queue.names.length, 300);
    assert.equal(v.queue.names[0], "DIA_TEST_0600");
    assert.match(v.message, /600 results read/);
});

test("tick of the run: a few dialogs per call, the report and the end", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(10);
    const v = new DialogView({ debug });
    v.perFrame = 3;
    v.startAll(false);
    v.tickAll();
    assert.equal(debug.ran.length, 3);
    assert.ok(v.queue);
    v.tickAll(); v.tickAll();
    assert.equal(debug.ran.length, 9);
    assert.ok(v.queue);
    v.tickAll();
    assert.equal(debug.ran.length, 10);
    assert.equal(v.queue, null, "finished");
    assert.equal(debug.guard, false, "the guard is taken off at the end");
    assert.equal(debug.saved, "dialogtest_report.txt");
    assert.match(v.message, /Done: 10\/10/);
});

test("the run stops when there is no world", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(5);
    debug.dlgRun = (name) => ({ name, status: "no-world", text: "", applied: [], unsolved: [], issues: [] });
    const v = new DialogView({ debug });
    v.startAll(false);
    v.tickAll();
    assert.equal(v.queue, null);
    assert.equal(debug.guard, false);
    assert.match(v.message, /There is no world/);
});

test("Stop: the report is written and the guard is off", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(20);
    const v = new DialogView({ debug });
    v.perFrame = 2;
    v.startAll(false);
    v.tickAll();
    v.stopAll("Stopped");
    assert.equal(v.queue, null);
    assert.equal(debug.guard, false);
    assert.equal(debug.saved, "dialogtest_report.txt");
    assert.match(v.message, /Stopped: 2\/20/);
});

test("tickAll does nothing when no run is going", () => {
    const { DialogView } = lib("dialogview.js");
    const debug = fakeDebug(3);
    const v = new DialogView({ debug });
    v.tickAll();
    assert.equal(debug.ran.length, 0);
});

// --- the console: comcheck dlg

function checks(debug, world = {}) {
    const { registerArcholosChecks } = lib("archoloschecks.js");
    const console_ = fakeConsole();
    registerArcholosChecks({ debug, player: { valid: true }, npcs: {}, world, console: console_, screen: { print() {} } });
    return { console: console_, run: (...args) => console_.commands.comcheck({ args }) };
}

test("comcheck dlg list prints the dialogs", () => {
    const debug = fakeDebug(3);
    const c = checks(debug);
    c.run("dlg", "list");
    assert.equal(c.console.errors.length, 0, c.console.errors.join("\n"));
    assert.match(c.console.lines.join("\n"), /3 dialogs/);
    assert.match(c.console.lines.join("\n"), /DIA_TEST_0000/);
});

test("comcheck dlg analyze and run need a name", () => {
    const c = checks(fakeDebug(1));
    c.run("dlg", "analyze");
    c.run("dlg", "run");
    assert.equal(c.console.errors.length, 2);
    assert.match(c.console.errors[0], /Usage: comcheck dlg analyze/);
    assert.match(c.console.errors[1], /Usage: comcheck dlg run/);
});

test("comcheck dlg run calls the tool with the name", () => {
    const debug = fakeDebug(2);
    const c = checks(debug);
    c.run("dlg", "run", "DIA_TEST_0001");
    assert.deepEqual(debug.ran, ["DIA_TEST_0001"]);
    assert.equal(c.console.errors.length, 0);
});

test("comcheck dlg all refuses a long list and writes the report for a short one", () => {
    let c = checks(fakeDebug(400));
    c.run("dlg", "all");
    assert.match(c.console.errors.join(""), /use the tab Dialogs/);

    const debug = fakeDebug(12);
    c = checks(debug);
    c.run("dlg", "all");
    assert.equal(c.console.errors.length, 0, c.console.errors.join("\n"));
    assert.equal(debug.ran.length, 12);
    assert.equal(debug.saved, "dialogtest_report.txt");
});

test("comcheck dlg with an unknown subcommand says what there is", () => {
    const c = checks(fakeDebug(1));
    c.run("dlg", "bogus");
    assert.match(c.console.errors.join(""), /list\|analyze\|run\|scene\|all\|report\|reset/);
});

test("comcheck npc needs a name and lists or moves a character", () => {
    const debug = fakeDebug(1);
    debug.findNpc = (n) => (n === "Martha" ? ["Martha #48 (100) at 1 2 3: state ZS_X"] : []);
    debug.moveNpc = (n, mode) => (mode === "home" ? "placed at VILLAGE_PUB_04" : null);
    const c = checks(debug, { name: () => "TEST.ZEN" });
    c.run("npc");
    assert.match(c.console.errors.join(""), /Usage: comcheck npc/);
    c.run("npc", "Martha");
    assert.match(c.console.lines.join("\n"), /Martha #48/);
    c.run("npc", "Martha", "home");
    assert.match(c.console.lines.join("\n"), /placed at VILLAGE_PUB_04/);
    c.run("npc", "Nobody");
    assert.match(c.console.lines.join("\n"), /No character with 'Nobody'/);
});

test("comcheck info prints the verdict of the condition", () => {
    const debug = fakeDebug(1);
    debug.infoCondition = (npc, info) => (info === "DIA_X" ? [1, false, "A=1 "] : null);
    const c = checks(debug);
    c.run("info", "Viktor", "DIA_X");
    assert.match(c.console.lines.join("\n"), /condition TRUE, not known yet/);
    assert.match(c.console.lines.join("\n"), /A=1/);
    c.run("info", "Viktor", "DIA_NO");
    assert.match(c.console.errors.join(""), /No such character or info/);
});

// --- the panel

test("the panel has the tabs of the developer tools, Dialogs and Tests among them", () => {
    const { createPanel } = lib("panel.js");
    const debug = fakeDebug(1);
    debug.log = () => ({ serial: 0, lines: [] });
    const panel = createPanel({ ui: {}, debug, player: {}, npcs: {}, world: {} });
    for (const name of ["Log", "Scripts", "World", "Ikarus", "Dialogs", "Tests"])
        assert.ok(panel.views[name], "missing view " + name);
});

// --- the tests that run in the game: the runner and their behaviour with a fake game

function fakeGame() {
    const syms = { ATR_STRENGTH: { type: "int", const: true, index: 5, value: 4 }, HLP_STRCMP: { type: "func", index: 9 }, KAPITEL: { type: "int", const: false, index: 11, value: 2 },
                   MEM_ReadInt: { type: "func", index: 20 }, MEM_Alloc: { type: "func", index: 21 } };
    const memory = new Map();
    let next = 4096;
    const debug = {
        symbol: (n) => syms[n] || null,
        setSymbol: (n, v) => { if (!syms[n] || syms[n].const) return false; syms[n].value = v; return true; },
        functions: (q) => Object.keys(syms).filter(k => k.includes(q.toUpperCase())),
        call(name, args) {
            switch (name) {
                case "HLP_STRCMP": return { ok: true, result: String(args[0]).toLowerCase() === String(args[1]).toLowerCase() ? 1 : 0 };
                case "MEM_Alloc": { const p = next; next += 64; return { ok: true, result: p }; }
                case "MEM_Free": return { ok: true, result: 0 };
                case "MEM_WriteInt": memory.set(args[0], args[1]); return { ok: true, result: 0 };
                case "MEM_ReadInt": return { ok: true, result: memory.get(args[0]) ?? 0 };
                default: return { ok: false, error: "Unknown function " + name };
            }
        },
        points: () => [],
        dlgList: () => ({ total: 0, rows: [] }),
        log: () => ({ serial: 0, lines: [] }),
    };
    const clock = { day: 1, hour: 8, minute: 30 };
    const world = { time: () => ({ ...clock }), setTime: (h, m) => { clock.hour = h; clock.minute = m; }, waypoint: () => null };
    const vars = new Map();
    const player = { valid: true, aiVar: (i) => vars.get(i) ?? 0, setAiVar: (i, v) => vars.set(i, v), itemCount: () => 0, addItem() {}, removeItem() {}, x: 0, y: 0, z: 0, setPosition() {} };
    return { debug, world, player, npcs: { nearby: () => [] } };
}

test("selftests: pass, fail and skip are told apart; the runner selects by area and by a word", () => {
    const { createSelfTests } = lib("selftests.js");
    const suite = createSelfTests(fakeGame());
    assert.ok(suite.tests.length >= 15, "there are tests");
    assert.deepEqual(suite.areas().sort(), ["dialogs", "engine", "ikarus"]);
    assert.equal(suite.select("all").length, suite.tests.length);
    assert.ok(suite.select("ikarus").every(t => t.area === "ikarus"));
    assert.ok(suite.select("memory").length >= 1, "by a word of the name");

    const r = suite.run("all");
    assert.equal(r.fail, 0, "nothing fails with a fake game that is right: " + JSON.stringify(r.results.filter(x => x.status === "fail")));
    assert.ok(r.pass >= 4, "the tests that the fake game can answer pass");
    assert.ok(r.skip >= 5, "the tests that need something else are skipped, not failed");
});

test("selftests: a wrong answer of the game is a failure with the reason", () => {
    const { createSelfTests } = lib("selftests.js");
    const g = fakeGame();
    const original = g.debug.call;
    g.debug.call = (name, args) => (name === "MEM_ReadInt" ? { ok: true, result: 999 } : original.call(g.debug, name, args));
    const suite = createSelfTests(g);
    const r = suite.run("memory");
    assert.equal(r.fail, 1);
    assert.match(r.results[0].message, /expected 1234, got 999/);
});

test("selftests: the variable test puts the value back, also when it fails", () => {
    const { createSelfTests } = lib("selftests.js");
    const g = fakeGame();
    const sym = g.debug.symbol("KAPITEL");
    const oldSet = g.debug.setSymbol;
    g.debug.setSymbol = (n, v) => { const ok = oldSet(n, v); if (v === sym.value && v !== 2) sym.value = v + 100; return ok; };   // a game that breaks the value
    const suite = createSelfTests(g);
    const r = suite.run("variable");
    assert.equal(r.fail, 1);
    assert.equal(sym.value >= 2, true);
});

test("selftests: without a game every test is skipped", () => {
    const { createSelfTests } = lib("selftests.js");
    const g = fakeGame();
    g.player.valid = false;
    const r = createSelfTests(g).run("all");
    assert.equal(r.fail, 0);
    assert.equal(r.pass, 0);
    assert.equal(r.skip, r.results.length);
});

test("comcheck test lists, runs and counts", () => {
    const g = fakeGame();
    const { registerArcholosChecks } = lib("archoloschecks.js");
    const console_ = fakeConsole();
    registerArcholosChecks({ ...g, console: console_, screen: { print() {} } });
    console_.commands.comcheck({ args: ["test"] });
    assert.match(console_.lines[0], /Tests: \d+; areas: /);
    console_.commands.comcheck({ args: ["test", "all"] });
    assert.match(console_.lines.join("\n"), /Tests \(all\): \d+ passed, 0 failed, \d+ skipped/);
    assert.equal(console_.errors.length, 0, console_.errors.join("\n"));
});

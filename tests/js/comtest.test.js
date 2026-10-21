// comtest <case>: one command for each thing that was fixed; the cases with a second step answer by themselves after some seconds.
"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");

const lib = (name) => require(path.join(__dirname, "..", "..", "mods", "devtools", "lib", name));

function fakeConsole() {
    const c = { lines: [], errors: [], commands: {} };
    c.log = (...a) => c.lines.push(a.join(" "));
    c.error = (...a) => c.errors.push(a.join(" "));
    c.registerCommand = (name, fn) => { c.commands[name] = fn; };
    return c;
}

/// a game in which everything that was fixed works (the same fake as the tests of the cases, with a few more functions)
function game() {
    const syms = {};
    let idx = 100;
    for (const n of ["NPC_GETDISTTOWP", "EXITSESSION", "Q102_CHANGEJORNARMOR", "NONE_1_JORN", "ITAR_BAU_L_JORN", "ITAR_BAU_L_JORN_2", "B_MARVIN_USEFAKEBAG_THROW",
                     "MEM_ArrayCreate", "MEM_ArrayInsert", "MEM_ArraySort", "MEM_ArrayRead", "MEM_ArraySize", "MEM_ArrayFree", "AI_TELEPORTALWAYS", "HERO",
                     "NONE_3_EZEKIEL", "B_STARTOTHERROUTINE", "TELEPORTNPCTOWP", "AI_USEITEMTOSTATE", "AI_GOTOWP", "NPC_CLEARAIQUEUE", "ITMI_BRUSH", "AI_SETWALKMODE", "AI_FUNCTION"])
        syms[n] = { type: "func", index: idx++ };
    const log = [];
    const arrays = new Map();
    let arrayId = 1;
    const mover = { name: "Villager", dead: false, x: 0, y: 0, z: 0 };
    const jorn = { name: "Jorn", x: 0, z: 0, armor: "", items: new Map(), addItem(n, c) { this.items.set(n, (this.items.get(n) || 0) + c); },
                   itemCount(n) { return this.items.get(n) || 0; }, setArmor(n) { this.armor = n; } };
    const player = { valid: true, x: 0, y: 0, z: 0, setPosition(x, y, z) { this.x = x; this.y = y; this.z = z; } };
    const martha = Array.from({ length: 5 }, (_, i) => ({ name: `DIA_MARTHA_${i}` }));
    const debug = {
        guard: false,
        symbol: (n) => syms[n] || null,
        points: () => ["FAR_POINT"],
        log: (after) => ({ serial: log.length, lines: log.slice(after).map(t => [1, t]) }),
        dlgGuard(on) { this.guard = on; },
        lastCallOther: () => "Marvin",
        dlgReset() {},
        dlgList: (f) => (f === "Martha" ? { total: 5, rows: martha } : { total: 1, rows: [] }),
        dlgRun: (name) => ({ name, status: "ok", outputs: 2, actions: 3, issues: [], unsolved: [], text: name }),
        findNpc: () => [],
        call(name, args) {
            switch (name) {
                case "NPC_GETDISTTOWP": return { ok: true, result: 0 };
                case "EXITSESSION": if (this.guard) log.push("ExitSession of the scripts is ignored by the dialog test"); return { ok: true, result: 0 };
                case "Q102_CHANGEJORNARMOR": jorn.armor = "ITAR_BAU_L_JORN_2"; jorn.items.set("ITAR_BAU_L_JORN", 0); return { ok: true, result: 0 };
                case "MEM_ArrayCreate": { const id = arrayId++; arrays.set(id, []); return { ok: true, result: id }; }
                case "MEM_ArrayInsert": arrays.get(args[0]).push(args[1]); return { ok: true, result: 0 };
                case "MEM_ArraySort": arrays.get(args[0]).sort((a, b) => a - b); return { ok: true, result: 0 };
                case "MEM_ArraySize": return { ok: true, result: arrays.get(args[0]).length };
                case "MEM_ArrayRead": return { ok: true, result: arrays.get(args[0])[args[1]] };
                case "AI_TELEPORTALWAYS": args[0].x = 7000; args[0].z = 0; return { ok: true, result: 0 };
                default: return { ok: true, result: 0 };
            }
        },
    };
    const npcs = { find: (i) => (i === "NONE_1_JORN" ? jorn : null), spawn: (i) => (i === "NONE_1_JORN" ? jorn : { name: i, x: 0, z: 0 }), nearby: () => [mover] };
    const world = { name: () => "TEST.ZEN", waypoint: (n) => (n === "FAR_POINT" ? { x: 7000, y: 0, z: 0 } : null) };
    return { debug, player, npcs, world };
}

function setup(g = game()) {
    const { registerArcholosChecks } = lib("archoloschecks.js");
    const { createTimers } = lib("timers.js");
    const console_ = fakeConsole();
    let clock = 0;
    const timers = createTimers(() => clock);
    registerArcholosChecks({ ...g, console: console_, screen: { print() {} }, later: timers.later });
    return {
        console: console_, timers,
        run: (...args) => console_.commands.comtest({ args }),
        wait: (ms) => { clock += ms; timers.tick(clock); },
    };
}

test("timers: a thing is done once, after its time, in the order of the time", () => {
    const { createTimers } = lib("timers.js");
    let clock = 0;
    const t = createTimers(() => clock);
    const done = [];
    t.later(2000, () => done.push("b"));
    t.later(500, () => done.push("a"));
    t.tick(400);
    assert.deepEqual(done, []);
    t.tick(600);
    assert.deepEqual(done, ["a"]);
    t.tick(2500);
    assert.deepEqual(done, ["a", "b"]);
    t.tick(9000);
    assert.deepEqual(done, ["a", "b"]);
    assert.equal(t.pending(), 0);
});

test("timers: a failure of one thing does not stop the others", () => {
    const { createTimers } = lib("timers.js");
    const t = createTimers(() => 0);
    const done = [];
    t.later(10, () => { throw new Error("boom"); });
    t.later(10, () => done.push("x"));
    const original = console.error;
    console.error = () => {};
    try { t.tick(20); } finally { console.error = original; }
    assert.deepEqual(done, ["x"]);
});

test("comtest: without a name it lists what there is", () => {
    const c = setup();
    c.run();
    const text = c.console.lines.join("\n");
    for (const w of ["fight", "teleport", "sort", "bag", "tool", "all"]) assert.ok(text.includes(w), "missing " + w);
});

test("comtest needs a game", () => {
    const g = game();
    g.player.valid = false;
    const c = setup(g);
    c.run("sort");
    assert.match(c.console.errors.join(""), /Load a disposable save/);
});

test("comtest sort, bag, tool: one command, the answer at once; fight answers by itself", () => {
    const c = setup();
    for (const name of ["fight", "sort", "bag", "tool"]) c.run(name);
    c.wait(5000);
    const text = c.console.lines.join("\n");
    assert.equal(c.console.errors.length, 0, c.console.errors.join("\n"));
    assert.match(text, /PASS other \(after 3 s\): OTHER in the command was Marvin/, "fight is the case other");
    assert.match(text, /PASS sort: -5, 0, 7/);
    assert.match(text, /PASS slot: /, "bag is the case slot");
    assert.match(text, /PASS tool: 5 of 5 dialogs of Martha run/);
});

test("comtest teleport: it answers by itself when the time has passed", () => {
    const c = setup();
    c.run("teleport");
    assert.match(c.console.lines.join("\n"), /LOOK teleport:/);
    assert.doesNotMatch(c.console.lines.join("\n"), /PASS teleport/, "not before the time");
    c.wait(1000);
    assert.doesNotMatch(c.console.lines.join("\n"), /PASS teleport/);
    c.wait(4000);
    assert.match(c.console.lines.join("\n"), /PASS teleport \(after 4 s\):/);
});

test("comtest teleport reports a failure by itself too", () => {
    const g = game();
    const original = g.debug.call.bind(g.debug);
    g.debug.call = (name, args) => (name === "AI_TELEPORTALWAYS" ? { ok: true, result: 0 } : original(name, args));   // the teleport does nothing
    const c = setup(g);
    c.run("teleport");
    c.wait(6000);
    assert.match(c.console.errors.join("\n"), /FAIL teleport \(after 4 s\):/);
});

test("comtest all: the cases are done one after another, those with time answer later", () => {
    const g = game();
    g.player.x = 0; g.player.z = 0;
    const c = setup(g);
    c.run("all");
    // every case has a tick of its own: only the first one has answered at once
    assert.match(c.console.lines.join("\n"), /SKIP startkm:/);
    assert.doesNotMatch(c.console.lines.join("\n"), /PASS exit:/, "the next one is for the next tick");
    c.wait(400);
    assert.match(c.console.lines.join("\n"), /PASS exit:/);
    for (let i = 0; i < 60; ++i) c.wait(5000);
    const text = c.console.lines.join("\n");
    assert.match(text, /PASS all: done/);
    assert.match(text, /teleport \(after 4 s\)/);
    assert.ok(text.indexOf("LOOK routine:") > text.indexOf("teleport (after 4 s)"), "the next case waits for the one before");
});

test("comtest with a name that does not exist says so, and a case of another mod is skipped", () => {
    const c = setup();
    c.run("bogus");
    assert.match(c.console.errors.join(""), /No such case/);
    const g = game();
    delete g.debug.symbol("Q102_CHANGEJORNARMOR");   // the fake keeps its table: take the symbol out of it
    const c2 = setup(Object.assign(game(), { debug: Object.assign(game().debug, { symbol: (n) => (n === "Q102_CHANGEJORNARMOR" ? null : { type: "func", index: 1 }) }) }));
    c2.run("jorn");
    assert.match(c2.console.lines.join("\n"), /SKIP armor:/);
});

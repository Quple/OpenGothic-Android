// Tests of the cases that were fixed (mods/devtools/lib/fixcases.js) with a fake game: each case passes when the game is right and fails
// when the fixed thing is broken again.
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

function caseGame(opts = {}) {
    const syms = {};
    let idx = 100;
    for (const n of ["NPC_GETDISTTOWP", "EXITSESSION", "Q102_CHANGEJORNARMOR", "NONE_1_JORN", "ITAR_BAU_L_JORN", "ITAR_BAU_L_JORN_2", "B_MARVIN_USEFAKEBAG_THROW",
                     "MEM_ArrayCreate", "MEM_ArrayInsert", "MEM_ArraySort", "MEM_ArrayRead", "MEM_ArraySize", "MEM_ArrayFree", "AI_TELEPORTALWAYS", "HERO",
                     "NONE_3_EZEKIEL", "B_STARTOTHERROUTINE", "TELEPORTNPCTOWP", "AI_USEITEMTOSTATE", "AI_GOTOWP", "NPC_CLEARAIQUEUE", "ITMI_BRUSH", "AI_SETWALKMODE", "AI_FUNCTION"])
        syms[n] = { type: "func", index: idx++ };
    syms.HERO.type = "instance";
    if (opts.noCom) for (const n of ["Q102_CHANGEJORNARMOR", "NONE_1_JORN", "ITAR_BAU_L_JORN"]) delete syms[n];

    const log = [];
    const arrays = new Map();
    let arrayId = 1000;
    const jorn = {
        name: "Jorn", x: 0, y: 0, z: 0, armor: "", items: new Map(), dead: false,
        addItem(n, c) { this.items.set(n, (this.items.get(n) || 0) + c); },
        itemCount(n) { return this.items.get(n) || 0; },
        setArmor(n) { this.armor = n; },
    };
    const player = { valid: true, x: 0, y: 0, z: 0, name: "Marvin", isPlayer: true, setPosition(x, y, z) { this.x = x; this.y = y; this.z = z; } };
    const placed = [];
    const debug = {
        guard: false,
        symbol: (n) => syms[n] || null,
        points: () => ["SHIP_BEGINNING_19", "SHIP_BEGINNING_JUMP", "FAR_POINT", "PART_13_DARRYL_DEAD"],
        log: (after) => ({ serial: log.length, lines: log.slice(after).map(t => [1, t]) }),
        dlgGuard(on) { this.guard = on; },
        lastCallOther: () => (opts.otherEmpty ? "" : "Marvin"),
        dlgReset() {},
        dlgList: (f) => (f === "Martha" ? { total: 2, rows: [{ name: "DIA_MARTHA_A" }, { name: "DIA_MARTHA_B" }] } : { total: opts.noCom ? 0 : 1, rows: [] }),
        dlgRun: (name) => ({ name, status: "ok", outputs: 4, actions: 9, issues: opts.issue ? [opts.issue] : [], unsolved: [], text: name }),
        findNpc: () => ["Ezekiel #3 (10) at 0 0 0: state ZS_SIT_DEPRESSION"],
        call(name, args) {
            switch (name) {
                case "NPC_GETDISTTOWP":
                    return { ok: true, result: opts.wrongDist ? 1234 : Math.round(Math.hypot(player.x + 44396.25, player.y + 2504.3, player.z + 146453.27)) };
                case "EXITSESSION":
                    if (this.guard) log.push("ExitSession of the scripts is ignored by the dialog test");
                    return { ok: true, result: 0 };
                case "Q102_CHANGEJORNARMOR":
                    if (opts.armorBroken) {
                        log.push("[zenkit] Internal Exception: illegal access of unbound member OCNPC.BODY_VISUALNAME");
                        return { ok: true, result: 0 };
                    }
                    jorn.armor = "ITAR_BAU_L_JORN_2";
                    jorn.items.set("ITAR_BAU_L_JORN", 0);
                    return { ok: true, result: 0 };
                case "B_MARVIN_USEFAKEBAG_THROW":
                    if (opts.noSlot) log.push("LeGo: OCNPC__GETSLOTITEM(ZS_LEFTHAND)");
                    return { ok: true, result: 0 };
                case "MEM_ArrayCreate": { const id = arrayId++; arrays.set(id, []); return { ok: true, result: id }; }
                case "MEM_ArrayInsert": arrays.get(args[0]).push(args[1]); return { ok: true, result: 0 };
                case "MEM_ArraySort": if (!opts.noSort) arrays.get(args[0]).sort((a, b) => a - b); return { ok: true, result: 0 };
                case "MEM_ArraySize": return { ok: true, result: arrays.get(args[0]).length };
                case "MEM_ArrayRead": return { ok: true, result: arrays.get(args[0])[args[1]] };
                case "MEM_ArrayFree": return { ok: true, result: 0 };
                case "AI_TELEPORTALWAYS":
                    placed.push(["teleport", args[1]]);
                    if (!opts.teleportBroken) { args[0].x = 7000; args[0].z = 0; }
                    return { ok: true, result: 0 };
                default:
                    placed.push([name, ...args.map(a => (a && a.name) || a)]);
                    return { ok: true, result: 0 };
            }
        },
    };
    const npcs = {
        find: (i) => (i === "NONE_1_JORN" ? jorn : null),
        spawn: (i) => (i === "NONE_1_JORN" ? jorn : { name: i, x: 0, z: 0, itemCount: () => 0, addItem() {}, setArmor() {} }),
        nearby: () => [{ name: "Villager", dead: false, x: 0, y: 0, z: 0 }],
    };
    const world = {
        name: () => "ARCHOLOS_MAINLAND.ZEN",
        waypoint: (n) => ({ FAR_POINT: { x: 7000, y: 0, z: 0 }, SHIP_BEGINNING_JUMP: { x: 0, y: 0, z: 0 }, PART_13_DARRYL_DEAD: { x: 0, y: 0, z: 0 } }[n] || null),
    };
    return { debug, player, npcs, world, jorn, log, placed };
}

test("fix cases: the list has the cases and they all say what they are about", () => {
    const { createFixCases } = lib("fixcases.js");
    const f = createFixCases(caseGame());
    for (const name of ["startkm", "exit", "armor", "slot", "sort", "other", "trialog", "teleport", "routine", "itemstate", "plank", "gestures", "lock", "stuck"])
        assert.ok(f.find(name), "missing case " + name);
    assert.ok(f.cases.every(c => c.about && c.about.length > 20));
});

test("fix cases: the cases that check themselves pass in a game that is right", () => {
    const { createFixCases } = lib("fixcases.js");
    const g = caseGame();
    g.player.x = -44396.25; g.player.y = -2504.3; g.player.z = -146453.27 + 300;
    const f = createFixCases(g);
    const byName = Object.fromEntries(f.runAuto().map(r => [r.name, r]));
    for (const name of ["startkm", "exit", "armor", "slot", "sort", "trialog", "tool"])
        assert.equal(byName[name].status, "pass", `${name}: ${byName[name].message}`);
    assert.equal(g.debug.guard, false, "the guard is taken off after the cases");
});

test("fix cases: each fixed thing fails when it is broken again", () => {
    const { createFixCases } = lib("fixcases.js");
    const expectFail = (opts, name, pattern, setup) => {
        const g = caseGame(opts);
        if (setup) setup(g);
        const r = createFixCases(g).run(name);
        assert.equal(r.status, "fail", `${name} should fail: ${r.message}`);
        assert.match(r.message, pattern);
    };
    expectFail({ wrongDist: true }, "startkm", /way point is/);
    expectFail({ armorBroken: true }, "armor", /still fails/);
    expectFail({ noSlot: true }, "slot", /GETSLOTITEM/);
    expectFail({ noSort: true }, "sort", /got .* wanted/);
    expectFail({ issue: "TRIA_Invite: Der Npc ist nicht in der KI-Glocke" }, "trialog", /TRIA_Invite/);
});

test("fix cases: OTHER of a command of AI_Function is the player; an empty OTHER fails", () => {
    const { createFixCases } = lib("fixcases.js");
    const g = caseGame();
    const f = createFixCases(g);
    assert.equal(f.run("other").status, "look");
    const done = f.check("other");
    assert.equal(done.status, "pass", done.message);
    assert.match(done.message, /OTHER in the command was Marvin/);
    const bad = caseGame({ otherEmpty: true });
    const fb = createFixCases(bad);
    fb.run("fight");
    assert.equal(fb.check("other").status, "fail");
});

test("fix cases: a case of another mod is skipped, not failed", () => {
    const { createFixCases } = lib("fixcases.js");
    const f = createFixCases(caseGame({ noCom: true }));
    assert.equal(f.run("armor").status, "skip");
    assert.equal(f.run("trialog").status, "skip");
    const g = caseGame();
    g.player.valid = false;
    assert.equal(createFixCases(g).run("startkm").status, "skip");
    assert.equal(f.run("no_such_case").status, "fail");
});

test("fix cases: the cases with two steps set up, then check", () => {
    const { createFixCases } = lib("fixcases.js");
    const g = caseGame();
    const f = createFixCases(g);
    assert.equal(f.check("teleport").status, "skip", "the check before the set up is skipped");
    let r = f.run("teleport");
    assert.equal(r.status, "look");
    assert.ok(g.placed.some(p => p[0] === "teleport" && p[1] === "FAR_POINT"));
    assert.equal(f.check("teleport").status, "pass");

    const bad = caseGame({ teleportBroken: true });
    const fb = createFixCases(bad);
    fb.run("teleport");
    assert.equal(fb.check("teleport").status, "fail");

    r = f.run("plank");
    assert.equal(r.status, "look");
    g.player.x = 100; g.player.z = 0;
    assert.equal(f.check("plank").status, "pass");
    g.player.x = 3000;
    assert.equal(f.check("plank").status, "fail");

    const setup = f.run("routine");
    assert.equal(setup.status, "look", setup.message);
    const done = f.check("routine");
    assert.equal(done.status, "pass", done.message);
});

test("fix cases: comcheck fix lists, runs a case and runs all", () => {
    const g = caseGame();
    g.player.x = -44396.25; g.player.y = -2504.3; g.player.z = -146453.27 + 300;
    const { registerArcholosChecks } = lib("archoloschecks.js");
    const c = fakeConsole();
    registerArcholosChecks({ ...g, console: c, screen: { print() {} } });
    c.commands.comcheck({ args: ["fix"] });
    assert.match(c.lines.join("\n"), /startkm: /);
    assert.match(c.lines.join("\n"), /teleport \(two steps\)/);
    c.commands.comcheck({ args: ["fix", "all"] });
    assert.match(c.lines.join("\n"), /PASS startkm/);
    assert.match(c.lines.join("\n"), /Cases: 7 passed, 0 failed/);
    c.commands.comcheck({ args: ["fix", "bogus"] });
    assert.match(c.errors.join(""), /No such case/);
    g.player.x = 0; g.player.y = 0; g.player.z = 0;
    c.commands.comcheck({ args: ["fix", "teleport"] });
    c.commands.comcheck({ args: ["fix", "teleport", "check"] });
    assert.match(c.lines.join("\n"), /PASS teleport/);
});

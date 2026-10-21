// Tests of the game that run in the game: they use the scripts of the loaded mod and the world that is loaded, and say PASS, FAIL or SKIP
// (a test that needs something that this game does not have, for example Ikarus or a world, is skipped, not failed).
//
//   comcheck test            the list of the tests
//   comcheck test all        every test, the result in the console and in the log
//   comcheck test <name>     the tests whose name has the word (or an area: engine, ikarus, dialogs, world)
//
// The tab Tests of the menu (F10) does the same with buttons. The tests change the state of the game and put it back; run them on a
// save that can be thrown away.
"use strict";

class Skip extends Error {}

function createSelfTests(env) {
    const { debug, player, npcs, world } = env;
    const tests = [];

    const test = (area, name, fn) => tests.push({ area, name, fn });
    const skip = (why) => { throw new Skip(why); };
    const ok = (condition, message) => { if (!condition) throw new Error(message || "failed"); };
    const eq = (actual, expected, what) => {
        if (actual !== expected) throw new Error(`${what || "value"}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    };
    const call = (name, args, context) => {
        const r = debug.call(name, args, context);
        if (!r.ok) throw new Error(`${name}: ${r.error}`);
        return r.result;
    };
    const has = (name) => !!debug.symbol(name);
    const needGame = () => { if (!player || !player.valid) skip("load a game first"); };
    const needSymbol = (name) => { if (!has(name)) skip(`the scripts have no ${name}`); };
    const needIkarus = () => { needGame(); needSymbol("MEM_ReadInt"); needSymbol("MEM_Alloc"); };
    const close = (a, b, d) => Math.abs(a - b) <= d;

    // --- the tool itself

    test("engine", "the symbols of the scripts can be read", () => {
        needGame();
        const s = debug.symbol("ATR_STRENGTH");
        if (!s) skip("no ATR_STRENGTH in these scripts");
        eq(s.type, "int", "type");
        ok(s.const, "a constant");
    });

    test("engine", "a variable of the scripts can be written and is put back", () => {
        needGame();
        const name = ["KAPITEL", "Kapitel"].find(has);
        if (!name) skip("no chapter variable");
        const before = debug.symbol(name).value;
        try {
            ok(debug.setSymbol(name, before + 1), "setSymbol refused");
            eq(debug.symbol(name).value, before + 1, "value after the write");
        } finally {
            debug.setSymbol(name, before);
        }
        eq(debug.symbol(name).value, before, "value after the restore");
    });

    test("engine", "a function of the scripts can be found and called", () => {
        needGame();
        needSymbol("HLP_STRCMP");
        ok(debug.functions("HLP_STRCMP", 5).length > 0, "not found by the name");
        eq(call("HLP_STRCMP", ["Abc", "aBC"]), 1, "Hlp_StrCmp is not case sensitive");
        eq(call("HLP_STRCMP", ["Abc", "abd"]), 0, "Hlp_StrCmp of different texts");
    });

    test("engine", "the time of the world can be set and read", () => {
        needGame();
        const before = world.time();
        try {
            world.setTime(13, 37);
            const t = world.time();
            eq(t.hour, 13, "hour");
            eq(t.minute, 37, "minute");
        } finally {
            world.setTime(before.hour, before.minute);
        }
    });

    test("engine", "the way points of the world are found by the name", () => {
        needGame();
        const names = debug.points("", 5);
        if (!names.length) skip("this world has no way points");
        const p = world.waypoint(names[0]);
        ok(p && Number.isFinite(p.x) && Number.isFinite(p.z), "no position for " + names[0]);
    });

    test("engine", "aivar of a character is read and written", () => {
        needGame();
        const before = player.aiVar(90);
        try {
            player.setAiVar(90, 4321);
            eq(player.aiVar(90), 4321, "aivar after the write");
        } finally {
            player.setAiVar(90, before);
        }
    });

    test("engine", "TeleportNpcToWP of the scripts puts the character on the ground at the way point", () => {
        needGame();
        needSymbol("TELEPORTNPCTOWP");
        needSymbol("HERO");
        const names = debug.points("", 40).filter(n => /^[A-Z0-9_]+$/.test(n));
        const name = names.find(n => !n.startsWith("TOT"));
        if (!name) skip("no way point");
        const p = world.waypoint(name);
        const from = { x: player.x, y: player.y, z: player.z };
        try {
            call("TELEPORTNPCTOWP", [debug.symbol("HERO").index, name]);
            ok(close(player.x, p.x, 400) && close(player.z, p.z, 400), `the player is at ${Math.round(player.x)} ${Math.round(player.z)}, not at ${name} ${Math.round(p.x)} ${Math.round(p.z)}`);
            ok(player.y <= p.y + 5, "the player hangs above the way point");
        } finally {
            player.setPosition(from.x, from.y, from.z);
        }
    });

    test("engine", "an item can be added to and removed from the inventory", () => {
        needGame();
        const item = ["ITMI_GOLD", "ItMi_Gold"].find(has);
        if (!item) skip("no gold item");
        const before = player.itemCount(item);
        try {
            player.addItem(item, 7);
            eq(player.itemCount(item), before + 7, "count after adding");
        } finally {
            const now = player.itemCount(item);
            if (now > before) player.removeItem(item, now - before);
        }
        eq(player.itemCount(item), before, "count after the restore");
    });

    test("engine", "a character is found by the name and moved next to the player", () => {
        needGame();
        const near = npcs.nearby(5000).find(n => !n.isPlayer && n.name);
        if (!near) skip("nobody near");
        const found = debug.findNpc(near.name);
        ok(found.length > 0, "findNpc has not found " + near.name);
        const from = { x: near.x, y: near.y, z: near.z };
        try {
            ok(debug.moveNpc(near.name, "here"), "moveNpc");
            ok(close(near.x, player.x, 400) && close(near.z, player.z, 400), "the character is not next to the player");
        } finally {
            near.setPosition(from.x, from.y, from.z);
        }
    });

    // --- Ikarus and LeGo

    test("ikarus", "memory: allocate, write, read, free", () => {
        needIkarus();
        const p = call("MEM_Alloc", [16]);
        ok(p > 0 && p < 0x7fffffff, "a pointer in the positive range, got " + p);
        try {
            call("MEM_WriteInt", [p, 1234]);
            call("MEM_WriteInt", [p + 4, -77]);
            eq(call("MEM_ReadInt", [p]), 1234, "first int");
            eq(call("MEM_ReadInt", [p + 4]), -77, "second int");
        } finally {
            call("MEM_Free", [p]);
        }
    });

    test("ikarus", "an array of numbers is sorted (QSORT_G2 of the engine)", () => {
        needIkarus();
        for (const f of ["MEM_ArrayCreate", "MEM_ArrayInsert", "MEM_ArraySort", "MEM_ArrayRead", "MEM_ArraySize", "MEM_ArrayFree"]) needSymbol(f);
        const arr = call("MEM_ArrayCreate", []);
        const input = [50, -3, 17, 9, 100, 0, 17];
        try {
            for (const n of input) call("MEM_ArrayInsert", [arr, n]);
            call("MEM_ArraySort", [arr]);
            const size = call("MEM_ArraySize", [arr]);
            eq(size, input.length, "size");
            const out = [];
            for (let i = 0; i < size; ++i) out.push(call("MEM_ArrayRead", [arr, i]));
            eq(JSON.stringify(out), JSON.stringify([...input].sort((a, b) => a - b)), "the array after the sort");
        } finally {
            call("MEM_ArrayFree", [arr]);
        }
    });

    test("ikarus", "a pointer of a character is positive and the same every time", () => {
        needIkarus();
        needSymbol("MEM_InstToPtr");
        const a = call("MEM_InstToPtr", [player]);
        const b = call("MEM_InstToPtr", [player]);
        ok(a > 0 && a < 0x7fffffff, "the pointer is not in the positive range: " + a);
        eq(a, b, "the pointer of the same character");
    });

    // --- the dialogs and the tool of the dialogs

    test("dialogs", "the list of dialogs is not empty and has no placeholders", () => {
        needGame();
        const r = debug.dlgList("", 0, 400);
        if (!r || r.total === 0) skip("this game has no dialogs in the scripts");
        ok(r.rows.length > 0, "no rows");
        ok(!r.rows.some(row => /^RESERVED_INFO/.test(row.name)), "RESERVED_INFO is listed");
        ok(!r.rows.some(row => row.npc.startsWith("$")), "a placeholder character is listed");
    });

    test("dialogs", "the texts of dialogs are UTF-8 (no broken letters)", () => {
        needGame();
        const r = debug.dlgList("", 0, 400);
        if (!r || r.total === 0) skip("no dialogs");
        const broken = r.rows.filter(row => /�|[\u0080-\u009F]/.test(row.description));
        eq(broken.length, 0, "dialogs with broken letters");
    });

    test("dialogs", "the filter finds a dialog by a word of the character", () => {
        needGame();
        const all = debug.dlgList("", 0, 5);
        if (!all || all.total === 0) skip("no dialogs");
        const word = all.rows[0].npc;
        const some = debug.dlgList(word, 0, 400);
        ok(some.total > 0 && some.total <= all.total, "the filter by " + word);
        ok(some.rows.every(row => row.npc.toUpperCase().includes(word.toUpperCase()) || row.name.toUpperCase().includes(word.toUpperCase()) ||
                                  row.description.toUpperCase().includes(word.toUpperCase()) || row.npcName.toUpperCase().includes(word.toUpperCase())), "a row that does not match");
    });

    test("dialogs", "a dialog is analyzed and its condition is read from the bytecode", () => {
        needGame();
        const r = debug.dlgList("", 0, 400);
        if (!r || r.total === 0) skip("no dialogs");
        // the first dialog with a condition: the text names the condition function and lists ways
        let text = "";
        for (const row of r.rows) {
            text = debug.dlgAnalyze(row.name);
            if (/condition \S+_CONDITION|condition /.test(text) && /way 1/.test(text)) break;
        }
        ok(/condition/.test(text), "the analysis has no condition: " + text);
    });

    test("dialogs", "a dialog is run, the result is kept and read back from the file of the progress", () => {
        needGame();
        const r = debug.dlgList("", 0, 400);
        if (!r || r.total === 0) skip("no dialogs");
        const row = r.rows.find(x => x.inWorld);
        if (!row) skip("no dialog of a character who is in this world");
        debug.dlgGuard(true);
        try {
            debug.dlgReset();
            const res = debug.dlgRun(row.name, true, false);
            ok(res && typeof res.status === "string" && res.status.length > 0, "no status");
            eq(res.name, row.name, "name");
            eq(debug.dlgCount(), 1, "runs");
            // the file of the progress: a new game of the tool reads the same result
            const read = debug.dlgResume();
            ok(read >= 1, "nothing was read back from the progress file");
            eq(debug.dlgList(row.name, 0, 5).rows[0].status, res.status, "status after the resume");
        } finally {
            debug.dlgGuard(false);
            debug.dlgReset();
        }
    });

    test("dialogs", "the report names what is missing", () => {
        needGame();
        const r = debug.dlgList("", 0, 60);
        if (!r || r.total === 0) skip("no dialogs");
        debug.dlgGuard(true);
        try {
            debug.dlgReset();
            for (const row of r.rows.slice(0, 20)) debug.dlgRun(row.name, true, false);
            const report = debug.dlgReport();
            ok(/DIALOG TEST REPORT/.test(report) && /WHAT IS STILL MISSING/.test(report) && /runs: 20/.test(report), "the report has no sections");
        } finally {
            debug.dlgGuard(false);
            debug.dlgReset();
        }
    });

    test("dialogs", "the condition of a dialog can be read for a character", () => {
        needGame();
        const r = debug.dlgList("", 0, 400);
        if (!r || r.total === 0) skip("no dialogs");
        const row = r.rows.find(x => x.inWorld && x.npcName);
        if (!row) skip("no dialog of a character in this world");
        const v = debug.infoCondition(row.npcName, row.name);
        ok(Array.isArray(v) && v.length >= 2, "no verdict");
    });

    return {
        tests,
        areas: () => [...new Set(tests.map(t => t.area))],
        /// the tests whose name or area has the word; "all" or nothing: every test
        select(word) {
            if (!word || word === "all") return tests;
            const w = word.toLowerCase();
            return tests.filter(t => t.area === w || t.name.toLowerCase().includes(w));
        },
        /// -> { name, area, status: "pass" | "fail" | "skip", message }
        runOne(t) {
            try {
                t.fn();
                return { name: t.name, area: t.area, status: "pass", message: "" };
            } catch (e) {
                if (e instanceof Skip) return { name: t.name, area: t.area, status: "skip", message: e.message };
                return { name: t.name, area: t.area, status: "fail", message: String((e && e.message) || e) };
            }
        },
        run(word) {
            const results = this.select(word).map(t => this.runOne(t));
            const count = (s) => results.filter(r => r.status === s).length;
            return { results, pass: count("pass"), fail: count("fail"), skip: count("skip") };
        },
    };
}

module.exports = { createSelfTests, Skip };

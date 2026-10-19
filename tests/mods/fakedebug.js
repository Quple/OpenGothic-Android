// A pretended `debug`, `player`, `npcs` and `world` (see common/scripting/game/game_debug.cpp) for the test of mods/devtools.
"use strict";

function has(text, filter) {
    return text.toLowerCase().includes(filter.toLowerCase());
}

class FakeDebug {
    constructor() {
        this.lines = [];                 // [level, text]
        this.calls = [];                 // what debug.call got
        this.triggered = [];
        this.godMode = false;
        this.marvinMode = false;
        this.symbols_ = { Ikarus_Version: { type: "int", const: false, value: 1 }, LeGo_Version: { type: "int", const: false, value: 2 }, Foo: { type: "string", const: false, value: "abc" } };
        this.functions_ = {
            B_GIVEINVITEMS: "func void B_GIVEINVITEMS(c_npc giver, c_npc taker, int item, int amount)",
            FOO_ADD: "func int FOO_ADD(int a, string b)",
        };
    }

    say(text, level = 0) { this.lines.push([level, text]); }

    log(after, max) {
        const out = this.lines.slice(after, after + max);
        return { serial: after + out.length, lines: out };
    }

    functions(filter, max) { return Object.keys(this.functions_).filter(n => has(n, filter)).slice(0, max); }
    symbols(filter, max) { return Object.keys(this.symbols_).filter(n => has(n, filter)).slice(0, max); }
    signature(name) { return this.functions_[name] ?? null; }
    call(name, args, who) {
        this.calls.push({ name, args, who });
        this.say("called " + name + " " + JSON.stringify(args));
        if (name === "FOO_ADD") return { ok: true, result: Number(args[0]) + 1 };
        return { ok: false, error: "script error in " + name };
    }
    symbol(name) { return this.symbols_[name] ?? null; }
    setSymbol(name, value) {
        if (!this.symbols_[name]) return false;
        this.symbols_[name].value = value;
        return true;
    }
    ikarus() { return "memory: 3 regions\n  region 1: 100 bytes\nmovers: 2"; }
    triggers(filter, max) { return [["EVT_GATE", "mover", 2], ["EVT_TRAP", "trigger", 0]].filter(t => has(t[0], filter)).slice(0, max); }
    trigger(name, untrigger) { this.triggered.push([name, untrigger]); return true; }
    points(filter, max) { return ["OC1", "OC2", "PSI_START"].filter(n => has(n, filter)).slice(0, max); }
    god(on) { if (on !== undefined) this.godMode = on; return this.godMode; }
    marvin(on) { if (on !== undefined) this.marvinMode = on; return this.marvinMode; }
}

function fakeGame() {
    const p = {
        valid: true, name: "Hero", health: 50, maxHealth: 100, level: 3, x: 0, y: 0, z: 0, rotation: 90,
        at: null, items: {},
        setHealth(h) { this.health = h; },
        teleport(point) { if (point === "NOWHERE") return false; this.at = point; return true; },
        addItem(name, n) { if (name === "NOTHING") return false; this.items[name] = (this.items[name] ?? 0) + n; return true; },
    };
    const spawned = [];
    const npcs = {
        nearby: () => [{ name: "Neighbour" }],
        spawn: (name, x, y, z, a) => {
            if (name === "NOBODY") return null;
            spawned.push({ name, x, y, z, a });
            return { name };
        },
    };
    const world = {
        t: { day: 1, hour: 8, minute: 5 },
        time() { return this.t; },
        setTime(h, m) { this.t = { day: this.t.day, hour: h, minute: m }; },
        name: () => "WORLD.ZEN",
    };
    return { player: p, npcs, world, spawned };
}

module.exports = { FakeDebug, fakeGame };

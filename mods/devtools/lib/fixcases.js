// Checks of the cases of Chronicles of Myrtana that were fixed in the engine: each one sets the case up in the game that is loaded, does what
// the scripts of the mod do, and says PASS or FAIL by what it can read back. What cannot be read back (what the character looks like, what
// the keys do) is told as "LOOK": what you should see.
//
//   comcheck fix                  the list of the cases
//   comcheck fix all              every case that checks itself at once
//   comcheck fix <case>           sets the case up (and checks it, if it can)
//   comcheck fix <case> check     the second step of the cases that need time: after you have closed F10 and waited
//
// They change the state of the game: use a save that can be thrown away. A case that needs a world or characters that are not
// there is skipped.
"use strict";

const { Skip } = require("./selftests.js");

function createFixCases(env) {
    const { debug, player, npcs, world } = env;
    const cases = [];
    const state = {};   // what a case remembers for its second step

    const add = (name, about, def) => cases.push({ name, about, ...def });
    const skip = (why) => { throw new Skip(why); };
    const call = (name, args, context) => {
        const r = debug.call(name, args, context);
        if (!r.ok) throw new Error(`${name}: ${r.error}`);
        return r.result;
    };
    const has = (name) => !!debug.symbol(name);
    const needGame = () => { if (!player || !player.valid) skip("load a game first"); };
    const needSymbols = (...names) => { for (const n of names) if (!has(n)) skip(`the scripts have no ${n} (this is not the mod for this case)`); };
    const dist = (a, b) => Math.hypot(a.x - b.x, a.z - b.z);   // on the ground: the height is not what the cases are about
    const pass = (message) => ({ status: "pass", message });
    const fail = (message) => ({ status: "fail", message });
    const look = (message) => ({ status: "look", message });
    const errorsSince = (serial, regex) => debug.log(serial, 2000).lines.map(l => l[1]).filter(t => regex.test(t));
    const logSerial = () => debug.log(0, 1).serial;

    function character(instance, offset = 250) {
        let n = npcs.find(instance);
        if (n) return n;
        n = npcs.spawn(instance, player.x + offset, player.y + 20, player.z, 0);
        if (!n) skip(`${instance} cannot be put in this world`);
        return n;
    }

    function farPoint(minDistance, notLike = /^(TOT|SPAWN)/) {
        const names = debug.points("", 3000).filter(n => /^[A-Z0-9_]+$/.test(n) && !notLike.test(n));
        for (const name of names) {
            const p = world.waypoint(name);
            if (p && dist(p, player) > minDistance && dist(p, player) < minDistance * 3) return { name, p };
        }
        return null;
    }

    // --- cases that check themselves

    add("startkm", "Npc_GetDistToWP finds the way point, not the start point of the same name (START_KM: the video with the fish after the ship)", {
        auto: true,
        run() {
            needGame();
            needSymbols("NPC_GETDISTTOWP");
            if (world.name().toUpperCase() !== "ARCHOLOS_MAINLAND.ZEN") skip("the case is for ARCHOLOS_MAINLAND.ZEN");
            // the way point START_KM is at the beach, the start point of the same name is at the ship (read from the world)
            const wp = { x: -44396.25, y: -2504.3, z: -146453.27 };
            const got = call("NPC_GETDISTTOWP", [player, "START_KM"]);
            const real = Math.hypot(player.x - wp.x, player.y - wp.y, player.z - wp.z);
            if (Math.abs(got - real) < 15) return pass(`${got} (the real distance to the way point is ${Math.round(real)})`);
            return fail(`Npc_GetDistToWP says ${got}, the way point is ${Math.round(real)} away: the start point or nothing was used`);
        },
    });

    add("exit", "the scripts cannot end the game while the dialogs are tested (ExitSession of the scripts is ignored)", {
        auto: true,
        run() {
            needGame();
            needSymbols("EXITSESSION");
            const serial = logSerial();
            debug.dlgGuard(true);
            try {
                call("EXITSESSION", []);
            } finally {
                debug.dlgGuard(false);
            }
            const told = errorsSince(serial, /ExitSession of the scripts is ignored/).length > 0;
            return told ? pass("the game is still here and the log says that the call was ignored") : fail("the game is here, but the log does not say that ExitSession was ignored");
        },
    });

    add("armor", "NPC_ChangeArmor of CoM works (Jorn gets the wounded outfit after the bloodflies and does not stay naked)", {
        auto: true,
        run() {
            needGame();
            needSymbols("Q102_CHANGEJORNARMOR", "NONE_1_JORN", "ITAR_BAU_L_JORN", "ITAR_BAU_L_JORN_2");
            const jorn = character("NONE_1_JORN");
            const serial = logSerial();
            jorn.addItem("ITAR_BAU_L_JORN", 1);
            jorn.setArmor("ITAR_BAU_L_JORN");
            call("Q102_CHANGEJORNARMOR", []);
            const bad = errorsSince(serial, /unbound member OCNPC\.(BODY|HEAD)_VISUALNAME/);
            if (bad.length) return fail("the script still fails: " + bad[0]);
            const armor = String(jorn.armor || "").toUpperCase();
            if (armor !== "ITAR_BAU_L_JORN_2") return fail(`Jorn wears "${armor || "nothing"}", not ITAR_BAU_L_JORN_2`);
            if (jorn.itemCount("ITAR_BAU_L_JORN") !== 0) return fail("the old outfit is still in his inventory");
            return pass("Jorn wears ITAR_BAU_L_JORN_2, the old outfit is gone") ;
        },
    });

    add("slot", "oCNpc::GetSlotItem and EquipWeapon are answered (the fake bag that Marvin throws)", {
        auto: true,
        run() {
            needGame();
            needSymbols("B_MARVIN_USEFAKEBAG_THROW");
            const serial = logSerial();
            call("B_MARVIN_USEFAKEBAG_THROW", [], { self: player, other: player });
            const bad = errorsSince(serial, /OCNPC__(GETSLOTITEM|EQUIPWEAPON)/);
            return bad.length ? fail(bad[0]) : pass("no unanswered GetSlotItem / EquipWeapon");
        },
    });

    add("sort", "the engine sorts the arrays of numbers (QSORT_G2 of LeGo: the lists of the mod)", {
        auto: true,
        run() {
            needGame();
            needSymbols("MEM_ArrayCreate", "MEM_ArrayInsert", "MEM_ArraySort", "MEM_ArrayRead", "MEM_ArraySize", "MEM_ArrayFree");
            const serial = logSerial();
            const arr = call("MEM_ArrayCreate", []);
            const input = [900, 7, -5, 300, 42, 42, 0];
            try {
                for (const n of input) call("MEM_ArrayInsert", [arr, n]);
                call("MEM_ArraySort", [arr]);
                const out = [];
                for (let i = 0; i < call("MEM_ArraySize", [arr]); ++i) out.push(call("MEM_ArrayRead", [arr, i]));
                const want = [...input].sort((a, b) => a - b);
                if (errorsSince(serial, /QSORT_G2/).length) return fail("the log still says TODO: QSORT_G2");
                return JSON.stringify(out) === JSON.stringify(want) ? pass(out.join(", ")) : fail(`got ${out.join(", ")}, wanted ${want.join(", ")}`);
            } finally {
                call("MEM_ArrayFree", [arr]);
            }
        },
    });

    add("other", "a command from AI_Function has OTHER (the player): the fights of the scenes start (B_ImmediateAttackPlayer, B_Attack)", {
        wait: 3,
        run() {
            needGame();
            needSymbols("AI_FUNCTION", "B_MARVIN_USEFAKEBAG_THROW", "NPC_CLEARAIQUEUE");
            // a real command of the queue, as the scenes of the mod give them: AI_Function(player, function); the function runs when the queue gets
            // to it, with SELF the character and OTHER what the engine leaves there (the player)
            call("NPC_CLEARAIQUEUE", [player]);
            state.other = { serial: logSerial() };
            call("AI_FUNCTION", [player, debug.symbol("B_MARVIN_USEFAKEBAG_THROW").index]);
            return look("A command was queued for the player (AI_Function). Wait 3 seconds with the game running (F10 closed)");
        },
        check() {
            if (!state.other) skip("run comtest fight first");
            const who = debug.lastCallOther();
            if (!who) return fail("the command has not run, or OTHER was empty");
            const bad = errorsSince(state.other.serial, /without an instance set \(in (B_ATTACK|B_IMMEDIATEATTACKPLAYER)\)/);
            if (bad.length) return fail(bad[0]);
            return pass(`OTHER in the command was ${who}`);
        },
    });

    add("trialog", "the trialog of Viktor and Martha in the pub runs (LeGo: the characters are invited, the old trialog does not block it)", {
        auto: true,
        run() {
            needGame();
            if (debug.dlgList("DIA_VIKTOR_MARTHATRIALOG", 0, 1).total === 0) skip("this is not CoM");
            debug.dlgGuard(true);
            try {
                debug.dlgReset();
                const r = debug.dlgRun("DIA_VIKTOR_MARTHATRIALOG", true, true);
                const bad = r.issues.filter(t => /TRIA_Invite|Trialog läuft|Trialog l.uft|TRIA_Start/.test(t));
                if (r.status === "no-npc" || r.status === "no-world") skip(r.status);
                if (bad.length) return fail(bad[0]);
                return r.status === "ok" || r.status === "issues" ? pass(`status ${r.status}, ${r.outputs} lines, ${r.actions} commands`) : fail(`status ${r.status}: ${r.text}`);
            } finally {
                debug.dlgGuard(false);
                debug.dlgReset();
            }
        },
    });

    add("tool", "the tool of the dialogs: Martha's dialogs run, Mob_HasItems and Hlp_IsValidNpc are understood by the solver", {
        auto: true,
        run() {
            needGame();
            const rows = debug.dlgList("Martha", 0, 80);
            if (!rows || rows.total === 0) skip("no dialogs of Martha in these scripts");
            debug.dlgGuard(true);
            try {
                debug.dlgReset();
                let ok = 0, bad = [];
                for (const row of rows.rows) {
                    const r = debug.dlgRun(row.name, true, true);
                    if (r.status === "error" || r.status === "crashed") bad.push(`${row.name}: ${r.status}`);
                    else if (r.status === "ok" || r.status === "issues") ++ok;
                }
                // what the solver learned: a condition that needs a container or a valid character is no longer unknown to it
                const unknown = [];
                for (const name of ["DIA_DAVIS_AFTERCHEST_FIRST", "TRIA_FABIO_Q102_MOLERATS"]) {
                    if (debug.dlgList(name, 0, 1).total === 0) continue;
                    const r = debug.dlgRun(name, true, false);
                    for (const u of r.unsolved) if (/(MOB_HASITEMS|HLP_ISVALIDNPC).*(does not know)/.test(u)) unknown.push(`${name}: ${u}`);
                }
                if (bad.length) return fail(bad.slice(0, 3).join("; "));
                if (unknown.length) return fail(unknown[0]);
                return ok > 0 ? pass(`${ok} of ${rows.rows.length} dialogs of Martha run`) : fail("not one dialog of Martha ran");
            } finally {
                debug.dlgGuard(false);
                debug.dlgReset();
            }
        },
    });

    // --- cases that need time: set up, close F10, wait, check

    add("teleport", "AI_TeleportAlways puts a character at the way point (the scenes of Q103 and Q602)", {
        wait: 4,
        run() {
            needGame();
            needSymbols("AI_TELEPORTALWAYS");
            const npc = npcs.nearby(3000).find(n => !n.dead && n.name);
            if (!npc) skip("stand near a living character");
            const far = farPoint(2500);
            if (!far) skip("no way point 2500..7500 away");
            state.teleport = { npc, wp: far, serial: logSerial() };
            call("AI_TELEPORTALWAYS", [npc, far.name, 1]);
            return look(`${npc.name} has been sent to ${far.name} (${Math.round(dist(far.p, npc))} away). Close F10, wait 4 seconds, then: comcheck fix teleport check`);
        },
        check() {
            const s = state.teleport;
            if (!s) skip("run comcheck fix teleport first");
            const bad = errorsSince(s.serial, /TRAFOOBJTOWORLD|AI_TeleportAlways: the character/);
            if (bad.length) return fail(bad[0]);
            const d = dist(s.wp.p, s.npc);
            return d < 500 ? pass(`${s.npc.name} is ${Math.round(d)} from ${s.wp.name}`) : fail(`${s.npc.name} is ${Math.round(d)} from ${s.wp.name}`);
        },
    });

    add("routine", "B_StartOtherRoutine ends the old routine at once (Ezekiel and Fabio on the beach after the ship: they did not walk back to the ship)", {
        wait: 6,
        run() {
            needGame();
            needSymbols("NONE_3_EZEKIEL", "B_STARTOTHERROUTINE", "TELEPORTNPCTOWP");
            if (!debug.points("PART_13_DARRYL_DEAD", 1).length) skip("this world has no beach of Q101");
            const e = character("NONE_3_EZEKIEL");
            call("NPC_EXCHANGEROUTINE", [e, "Ship"]);
            call("TELEPORTNPCTOWP", [debug.symbol("NONE_3_EZEKIEL").index, "SHIP_EZEKIEL_02"]);
            state.routine = { npc: e, serial: logSerial(), at: Date.now(), phase: 1 };
            // the scene of the mod: a new routine, then the teleport to the beach
            call("B_STARTOTHERROUTINE", [e, "Pray"]);
            call("TELEPORTNPCTOWP", [debug.symbol("NONE_3_EZEKIEL").index, "PART_13_DARRYL_DEAD"]);
            return look("Ezekiel was on the ship (routine Ship) and is now sent to the beach with the routine Pray. Close F10, wait 6 seconds, then: comcheck fix routine check");
        },
        check() {
            const s = state.routine;
            if (!s) skip("run comcheck fix routine first");
            const wp = world.waypoint("PART_13_DARRYL_DEAD");
            const d = wp ? dist(wp, s.npc) : Infinity;
            const line = (debug.findNpc("Ezekiel")[0] || "");
            if (d < 900) return pass(`Ezekiel is ${Math.round(d)} from the beach point; ${line}`);
            return fail(`Ezekiel is ${Math.round(d)} from the beach point (he went back to the ship?); ${line}`);
        },
    });

    add("itemstate", "a character who uses an item (the broom of Martha, the food) can walk away: the animation of the item does not hold him", {
        wait: 10,
        run() {
            needGame();
            const item = ["ITMI_BRUSH", "ITMI_BROOM"].find(has);
            if (!item) skip("no broom in these scripts");
            needSymbols("AI_USEITEMTOSTATE", "AI_GOTOWP", "NPC_CLEARAIQUEUE");
            const npc = npcs.nearby(3000).find(n => !n.dead && n.name);
            if (!npc) skip("stand near a living character");
            const far = farPoint(1200);
            if (!far) skip("no way point 1200..3600 away");
            call("NPC_CLEARAIQUEUE", [npc]);
            call("AI_USEITEMTOSTATE", [npc, debug.symbol(item).index, 1]);
            call("AI_GOTOWP", [npc, far.name]);
            state.itemstate = { npc, wp: far, from: dist(far.p, npc) };
            return look(`${npc.name} takes the broom and is sent to ${far.name} (${Math.round(state.itemstate.from)} away). Close F10, wait 10 seconds, then: comcheck fix itemstate check`);
        },
        check() {
            const s = state.itemstate;
            if (!s) skip("run comcheck fix itemstate first");
            const d = dist(s.wp.p, s.npc);
            return d < s.from * 0.5 ? pass(`${s.npc.name} walked: ${Math.round(s.from)} -> ${Math.round(d)}`) : fail(`${s.npc.name} stands in the animation of the item: ${Math.round(s.from)} -> ${Math.round(d)}; ${debug.findNpc(s.npc.name)[0] || ""}`);
        },
    });

    add("plank", "the player walks onto the plank of the ship by himself (AI_GotoWP without a path on the ship) and jumps", {
        wait: 12,
        run() {
            needGame();
            needSymbols("TELEPORTNPCTOWP", "AI_GOTOWP", "AI_SETWALKMODE", "HERO");
            if (!world.waypoint("SHIP_BEGINNING_JUMP")) skip("this world has no ship of Q101");
            call("TELEPORTNPCTOWP", [debug.symbol("HERO").index, "SHIP_BEGINNING_19"]);
            call("AI_SETWALKMODE", [player, 1]);
            call("AI_GOTOWP", [player, "SHIP_BEGINNING_JUMP"]);
            state.plank = { serial: logSerial() };
            return look("The player is put on the ship and sent to the plank. Close F10, wait 12 seconds, then: comcheck fix plank check");
        },
        check() {
            const s = state.plank;
            if (!s) skip("run comcheck fix plank first");
            const wp = world.waypoint("SHIP_BEGINNING_JUMP");
            const d = dist(wp, player);
            return d < 250 ? pass(`the player is ${Math.round(d)} from the end of the plank`) : fail(`the player is ${Math.round(d)} from the end of the plank`);
        },
    });

    // --- cases that only you can see

    add("error", "an error of the scripts is shown on the whole screen with the call stack (the touch version, or the game with -showerrors)", {
        manual: true,
        run() {
            needGame();
            debug.testError("comtest error: a test of the screen of errors (wrong argument of a function)");
            return look("An exception of the scripts was written to the log as the VM writes it. EXPECTED: a full screen with ERROR OF THE SCRIPTS, the text, " +
                "the call stack and the variables; a tap or a key goes on. The same test again shows nothing: an error is shown once.");
        },
    });

    add("gestures", "a skipped line of a trialog ends the gestures of the hands of everyone and the next speaker does not wait", {
        manual: true,
        run() {
            return look("Start a trialog of three (Viktor and Martha in the pub: comcheck dlg run DIA_VIKTOR_MARTHATRIALOG gives the scene, or play the ship scene). " +
                "Skip every line at once. EXPECTED: the hands of the one who has spoken stop moving at once, the next one speaks without a pause.");
        },
    });

    add("lock", "no inventory, menu, log or status in a cutscene without a dialog (the captain who goes down in the ship)", {
        manual: true,
        run() {
            return look("In the ship scene, while the captain walks downstairs (no dialog), press I, ESC, the log and the status keys. EXPECTED: nothing opens. " +
                "After two minutes of a scene that hangs the keys work again.");
        },
    });

    add("stuck", "Martha does not stand on the counter and walks (comcheck npc Martha shows the state)", {
        manual: true,
        run() {
            return look("Enter the pub of Silbach on a new game after the beach. EXPECTED: Martha is behind the counter at her point, walks, and after the talk with her " +
                "Viktor comes with the trialog. comcheck npc Martha must not say 'plays [S_FOODHUGE_S0 ...]' for minutes.");
        },
    });

    // the names that the commands use for the things that were fixed
    const aliases = { fight: "other", fights: "other", bag: "slot", weapon: "slot", qsort: "sort", tp: "teleport", dialogs: "tool", martha: "tool",
                      beach: "routine", screen: "error", errors: "error", broom: "itemstate", ship: "plank", fish: "startkm", jorn: "armor" };

    return {
        cases,
        aliases,
        names: () => cases.map(c => c.name),
        find: (name) => cases.find(c => c.name === (aliases[name] || name)),
        /// a case with a second step is checked by itself when the time has passed (env.later): the answer comes to report(name, result)
        runFollowed(name, report) {
            const c = this.find(name);
            if (!c) {
                report(name, { status: "fail", message: "no such case: " + name });
                return;
            }
            const r = this.run(c.name);
            report(c.name, r);
            if (c.check && r.status === "look" && env.later)
                env.later((c.wait || 4) * 1000 + 600, () => report(c.name + " (after " + (c.wait || 4) + " s)", this.check(c.name)));
        },
        /// every case that can be set up, one after another: the ones with a second step wait for their time
        runEverything(report) {
            const names = cases.filter(c => !c.manual).map(c => c.name);
            // every case in a tick of its own: a command of the console that runs for long is interrupted by the game
            const next = () => {
                const name = names.shift();
                if (!name) { report("all", { status: "pass", message: "done" }); return; }
                const c = this.find(name);
                const r = this.run(name);
                report(name, r);
                if (c.check && r.status === "look" && env.later)
                    env.later((c.wait || 4) * 1000 + 600, () => { report(name + " (after " + (c.wait || 4) + " s)", this.check(name)); env.later(300, next); });
                else if (env.later)
                    env.later(300, next);
                else
                    next();
            };
            next();
        },
        /// the first step of a case (or its only step)
        run(name) {
            const c = this.find(name);
            if (!c) return { status: "fail", message: "no such case: " + name };
            return this.step(() => c.run());
        },
        check(name) {
            const c = this.find(name);
            if (!c) return { status: "fail", message: "no such case: " + name };
            if (!c.check) return { status: "skip", message: "this case has no second step" };
            return this.step(() => c.check());
        },
        step(fn) {
            try {
                return fn();
            } catch (e) {
                if (e instanceof Skip) return { status: "skip", message: e.message };
                return { status: "fail", message: String((e && e.message) || e) };
            }
        },
        /// every case that checks itself
        runAuto() {
            return cases.filter(c => c.auto).map(c => ({ name: c.name, about: c.about, ...this.run(c.name) }));
        },
    };
}

module.exports = { createFixCases };

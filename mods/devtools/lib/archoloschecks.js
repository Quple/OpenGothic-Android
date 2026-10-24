// Manual checks only: no game state is changed until the user enters comcheck.
"use strict";

const { createSelfTests } = require("./selftests.js");
const { createFixCases } = require("./fixcases.js");

function registerArcholosChecks({ debug, player, npcs, world, console, screen, later }) {
    let sequence = 0;
    let archer = null;
    let archerWorld = "";
    let ammunition = "";
    let previousGod = null;
    let lastShot = null;
    let timeCheck = null;
    let partyNpc = null;
    let finishNpc = null;
    let partyStrength = false;
    let fixCases = null;
    let houseWallChoice = null;
    let itemPreview = 0;

    // comtest <case> and comcheck fix <case>: the cases that were fixed
    function fixCommand(args) {
        const cases = fixCases || (fixCases = createFixCases({ debug, player, npcs, world, later }));
        const show = (name, r) => {
            const line = `${r.status.toUpperCase().padEnd(4)} ${name}: ${r.message}`;
            if (r.status === "fail") console.error(line); else console.log(line);
        };
        if (!args[0]) {
            console.log("comtest <case>: fight (OTHER in the commands of the scenes) | teleport (AI_TeleportAlways) | sort (QSORT_G2) | bag (GetSlotItem, EquipWeapon) | tool (the tool of the dialogs) | all");
            console.log("Every case: " + cases.cases.map(c => c.name + (c.auto ? "" : c.manual ? " (you look)" : " (set up, the answer comes by itself)")).join(", "));
            return;
        }
        if (args[0] === "all") {
            console.log("comtest all: the cases are done one after another, the ones that need time answer after some seconds (keep the game running).");
            cases.runEverything(show);
            return;
        }
        if (!cases.find(args[0])) throw new Error("No such case. comtest lists them.");
        if (args[1] === "check") {
            show(args[0], cases.check(cases.find(args[0]).name));
            return;
        }
        cases.runFollowed(args[0], show);
    }

    function call(name, args, context) {
        const result = debug.call(name, args, context);
        if (!result.ok) throw new Error(`${name}: ${result.error}`);
        return result.result;
    }

    function id(name) {
        const symbol = debug.symbol(name);
        if (!symbol || !Number.isInteger(symbol.index)) throw new Error(`Unknown symbol: ${name}`);
        return symbol.index;
    }

    function message(who, text) {
        const escaped = text.replace(/\\/g, "\\\\").replace(/ /g, "\\_");
        // Coordinates are percentages: 100,100 starts beyond the visible area.
        call("AI_PLAYANI", [who, `CALL SIISI ${escaped} -1 35 FONT_OLD_10_WHITE.TGA 10 ${id("AI_PRINTSCREEN")}`]);
    }

    function requireArcher() {
        if (!archer || !archer.valid || archer.dead || world.name() !== archerWorld ||
            archer.instance !== "NONE_34813481_TREETESTER")
            throw new Error("First run comcheck ranged in this world.");
        return archer;
    }

    function ranged(action) {
        if (action === "clear") {
            if (archer && archer.valid && world.name() === archerWorld && archer.instance === "NONE_34813481_TREETESTER")
                archer.remove();
            archer = null;
            lastShot = null;
            if (previousGod !== null) debug.god(previousGod);
            previousGod = null;
            console.log("Removed the check NPC; restored the previous god-mode setting.");
            return;
        }
        if (action === "aim" || action === "shoot" || action === "stop") {
            const npc = requireArcher();
            if (action === "shoot")
                lastShot = { id: npc.id, shots: npc.rangedShots, ammo: npc.itemCount(ammunition) };
            call("NPC_CLEARAIQUEUE", [npc]);
            if (action === "stop") {
                call("AI_STOPAIM", [npc]);
            } else {
                call(action === "aim" ? "AI_AIMAT" : "AI_SHOOTAT", [npc, player]);
                // Keep normal combat AI from shooting again after the check command.
                call("AI_WAIT", [npc, 120]);
            }
            console.log(`Queued ${action} for check NPC #${npc.id}. Ammo before=${npc.itemCount(ammunition)}. Close F10, observe, then use comcheck ranged status.`);
            return;
        }
        if (action === "status") {
            const npc = requireArcher();
            console.log(`Check NPC #${npc.id}: weaponState=${npc.weaponState}, ranged=${npc.rangedItem}, ammo=${npc.itemCount(ammunition)}, projectiles=${npc.rangedShots}.`);
            if (lastShot && lastShot.id === npc.id) {
                const created = npc.rangedShots - lastShot.shots;
                const spent = lastShot.ammo - npc.itemCount(ammunition);
                console.log(`Last shoot: created=${created}, ammo spent=${spent}. ${created === 1 && spent === 1 ? "PROJECTILE CONFIRMED (hit/visibility not measured)." : "Expected 1 projectile and 1 ammo; close F10 for 3 seconds and check again. A new aim/shoot/stop cancels the previous queue."}`);
            }
            return;
        }
        if (action && action !== "bow" && action !== "crossbow")
            throw new Error("Usage: comcheck ranged [bow|crossbow|aim|shoot|stop|status|clear]");
        if (player.talking) throw new Error("Close the current dialog first.");
        const weapon = action === "bow" ? "ITRW_BOW_L_01" : "ITRW_MIL_CROSSBOW";
        const ammo = action === "bow" ? "ITRW_ARROW" : "ITRW_BOLT";
        const weaponId = id(weapon);
        id(ammo);
        id("NONE_34813481_TREETESTER");
        if (archer && archer.valid && world.name() === archerWorld && archer.instance === "NONE_34813481_TREETESTER")
            archer.remove();
        const p = player.position;
        // Spawn the original animation tester, not a duplicate of a quest character.
        const a = player.rotation * Math.PI / 180;
        archer = npcs.spawn("NONE_34813481_TREETESTER", p.x + Math.cos(a) * 450, p.y,
            p.z + Math.sin(a) * 450, player.rotation + 90);
        if (!archer || !archer.valid) throw new Error("Cannot spawn the check NPC. Stand on open, level ground.");
        archerWorld = world.name();
        lastShot = null;
        ammunition = ammo;
        if (!debug.parkNpc(archer)) throw new Error("Cannot disable the check NPC's routine.");
        call("NPC_CHANGEATTRIBUTE", [archer, debug.symbol("ATR_STRENGTH").value, 200]);
        call("NPC_CHANGEATTRIBUTE", [archer, debug.symbol("ATR_DEXTERITY").value, 200]);
        if (!archer.addItem(ammo, 20)) throw new Error(`Cannot add ${ammo}`);
        call("EQUIPITEM", [archer, weaponId]);
        call("AI_READYRANGEDWEAPON", [archer]);
        if (previousGod === null) previousGod = debug.god();
        debug.god(true);
        console.log(`Check NPC #${archer.id}: ${weapon}, 20 ${ammo}. Player god mode enabled until comcheck ranged clear. Close F10 for 2-3 seconds so the weapon is drawn. Next: comcheck ranged aim | shoot | stop | status | clear.`);
    }

    function smokersScene() {
        if (player.talking) throw new Error("Close the current dialog first.");
        const point = "VILLAGE_POORPLACE_18";
        const pos = world.waypoint(point);
        if (!pos) throw new Error(`This world has no ${point}. Load Archolos' main world first.`);
        const names = ["BAU_13324_SMOKER", "BAU_13323_SMOKER"];
        const infos = ["DIA_13324_SMOKER_OHFUCK", "DIA_13324_SMOKER_GOTCHA", "DIA_13323_SMOKER_GOTCHA"];
        for (const name of names.concat(infos)) id(name);
        const talkState = id("ZS_TALK");
        if (!debug.setSymbol("RNG_TRAP20_WAYPOINT", point))
            throw new Error("Cannot prepare the second smoker's starting point.");
        debug.setSymbol("FMQ001_BLOCKDIALOGUE", 0);
        // The original greeting condition is TRUE; forgetting it is the only dialog requirement.
        for (const info of infos)
            if (!debug.setInfoKnown(info, false)) throw new Error(`Cannot reset ${info}`);
        world.setTime(22, 0);
        for (const flag of ["TRAP20_VANISH1", "TRAP20_VANISH2"])
            if (!debug.setSymbol(flag, 0)) throw new Error(`Cannot reset ${flag}`);
        call("NPC_CLEARAIQUEUE", [player]);
        // Leave the waypoint itself free for the actors.
        const landing = debug.placeOnGround(player, point, 0, 250);
        if (!landing)
            throw new Error(`No safe ground near ${point}; player was not teleported. World=${world.name()}, waypoint=${Math.round(pos.x)},${Math.round(pos.y)},${Math.round(pos.z)}.`);
        call("AI_REMOVEWEAPON", [player]);
        const actors = names.map((name, i) => {
            let npc = npcs.find(name);
            if (npc && npc.dead) { npc.remove(); npc = null; }
            if (!npc) npc = npcs.spawn(name, pos.x + i * 180, pos.y, pos.z, 0);
            if (!npc || !npc.valid) throw new Error(`Cannot prepare ${name}`);
            if (!debug.parkNpc(npc)) throw new Error(`Cannot reset ${name}'s old AI state`);
            call("NPC_EXCHANGEROUTINE", [npc, "START"]);
            call("NPC_CLEARAIQUEUE", [npc]);
            if (!debug.placeOnGround(npc, point, i * 180, 0))
                throw new Error(`No safe ground for ${name} near ${point}`);
            call("AI_STANDUP", [npc]);
            return npc;
        });
        // Enter the original dialog state so its normal cleanup resumes RUN1
        // after the queued AI_Function has changed the routine.
        call("AI_STARTSTATE", [actors[0], talkState, 1, ""], { self: actors[0], other: player });
        console.log(`Ground landing: player x=${Math.round(landing.x)}, y=${Math.round(landing.y)}, z=${Math.round(landing.z)}; waypoint y=${Math.round(pos.y)}; actors y=${actors.map(n => Math.round(n.y)).join(", ")}.`);
        console.log("Prepared Silbach's smokers at VILLAGE_POORPLACE_18, 22:00; dialog history and disappearance flags reset. Close F10. One smoker speaks, the dialog closes, then BOTH flee (AI_Function -> DIA_13324_SMOKER_RUN). Do not chase/start a second dialog until both have started running. Reload your original save afterwards.");
    }

    console.registerCommand("comcheck", ({ args }) => {
        try {
            if (!player.valid) throw new Error("Load a disposable save first.");
            switch (args[0]) {
                case "wait": {
                    const delay = Number(args[1] || 4);
                    if (!Number.isFinite(delay) || delay < 1 || delay > 120)
                        throw new Error("Usage: comcheck wait [seconds, 1..120]");
                    const other = npcs.nearby(1500).find(n => !n.dead && n.health > 0);
                    if (!other) throw new Error("Stand near a living, idle NPC.");
                    call("NPC_CLEARAIQUEUE", [player]);
                    call("NPC_CLEARAIQUEUE", [other]);
                    call("AI_WAIT", [other, delay]);
                    call("AI_WAITTILLEND", [player, other]);
                    call("AI_WAITTILLEND", [other, player]);
                    call("AI_WAITTILLEND", [player, other]);
                    message(player, `COM_WAIT_DONE #${++sequence}`);
                    console.log(`Queued mutual waits with ${other.name}. Close F10; COM_WAIT_DONE should appear after about ${delay} seconds of game time.`);
                    break;
                }
                case "cancel": {
                    const other = npcs.nearby(1500).find(n => !n.dead && n.health > 0);
                    if (!other) throw new Error("Stand near a living, idle NPC.");
                    call("NPC_CLEARAIQUEUE", [player]);
                    call("NPC_CLEARAIQUEUE", [other]);
                    call("AI_WAIT", [other, 30]);
                    call("AI_WAITTILLEND", [player, other]);
                    message(player, `COM_CANCEL_DONE #${++sequence}`);
                    call("NPC_CLEARAIQUEUE", [other]);
                    console.log("Cancelled the NPC queue. Close F10; COM_CANCEL_DONE should appear immediately.");
                    break;
                }
                case "drop": {
                    const item = args[1] || "ITFO_APPLE";
                    const itemId = id(item);
                    if (!player.addItem(item, 1)) throw new Error(`Cannot add ${item}`);
                    call("AI_DROPITEM", [player, itemId]);
                    console.log(`Added one ${item} and queued its drop. Close F10 and inspect the ground and inventory.`);
                    break;
                }
                case "chest": {
                    const tag = args[1] || debug.containers(500)[0];
                    const item = args[2] || "ITFO_APPLE";
                    const rawCount = args[3];
                    const count = Number(rawCount || 3);
                    if (!tag) throw new Error("Stand within 5 metres of a chest, or specify its vob name.");
                    if (!Number.isInteger(count) || count <= 0)
                        throw new Error("Usage: comcheck chest [vob-name] [item-instance] [count]");
                    const itemId = id(item);
                    const before = call("MOB_HASITEMS", [tag, itemId]);
                    call("MOB_CREATEITEMS", [tag, itemId, count]);
                    const after = call("MOB_HASITEMS", [tag, itemId]);
                    console.log(`${tag}: ${item} before=${before}, after=${after}; expected increase=${count}. Open the chest yourself to confirm.`);
                    break;
                }
                case "trader": {
                    // a baker of the village with the dialog "Show me your wares" (it needs only that Q105 is not running)
                    if (player.talking) throw new Error("Close the current dialog first.");
                    const trader = "BAU_719_BAKER";
                    id(trader);
                    id("ITMIS_Q505_RUPERT");
                    id("ITFO_APPLE");
                    const p = player.position;
                    const a = player.rotation * Math.PI / 180;
                    const npc = npcs.spawn(trader, p.x + Math.cos(a) * 250, p.y, p.z + Math.sin(a) * 250, player.rotation + 180);
                    if (!npc || !npc.valid) throw new Error("Cannot spawn the trader. Stand on open, level ground.");
                    if (!debug.parkNpc(npc)) throw new Error("Cannot disable the trader's routine.");
                    npc.addItem("ITMI_GOLD", 1000);
                    player.addItem("ITMIS_Q505_RUPERT", 1);
                    player.addItem("ITFO_APPLE", 3);
                    const opened = player.openTrade(npc);
                    console.log(`Trader ${trader} #${npc.id} stands in front of you. You got ITMIS_Q505_RUPERT (important) and 3 ITFO_APPLE.` +
                        (opened ? " The trade window is open: close F10 and sell the items. " : " Close F10 and talk to him, choose the trade. ") +
                        "The important item must not be sold (the hero shakes his head), the apples can.");
                    break;
                }
                case "scale": {
                    // the collision follows the scale of the model (Mdl_SetModelScale): the giant rats of CoM are scaled down
                    const instance = (args[1] || "GIANT_RAT").toUpperCase();
                    const factor = Number(args[2] || 0.4);
                    if (!(factor > 0.05 && factor < 10)) throw new Error("Usage: comcheck scale [instance] [factor 0.1-5], for example comcheck scale GIANT_RAT 0.4");
                    id(instance);
                    const p = player.position;
                    const a = player.rotation * Math.PI / 180;
                    const left = a + Math.PI / 2;
                    const make = (offset) => npcs.spawn(instance, p.x + Math.cos(a) * 350 + Math.cos(left) * offset, p.y,
                        p.z + Math.sin(a) * 350 + Math.sin(left) * offset, player.rotation + 180);
                    const normal = make(-120);
                    const scaled = make(120);
                    if (!normal || !normal.valid || !scaled || !scaled.valid) throw new Error("Cannot spawn. Stand on open, level ground.");
                    debug.parkNpc(normal);
                    debug.parkNpc(scaled);
                    call("MDL_SETMODELSCALE", [scaled, factor, factor, factor]);
                    call("MDL_SETMODELSCALE", [normal, 1, 1, 1]);
                    console.log(`Two ${instance} stand in front of you: on the left of the screen the normal one, on the right the one at the scale ${factor}. Walk into both: the small one has to be stopped by its small body, not by the body of the normal one.`);
                    break;
                }
                case "state": {
                    // a snapshot to compare before a save and after a load
                    const t = world.time();
                    const p = player.position;
                    const items = ["ITFO_APPLE", "ITMI_GOLD", "ITKE_LOCKPICK", "ITSC_PICKLOCK", "ITMIS_Q505_RUPERT"];
                    const counts = items.map(n => `${n}=${player.itemCount(n)}`).join(" ");
                    console.log(`World ${world.name()}, day ${t.day} ${t.hour}:${String(t.minute).padStart(2, "0")}, position ${Math.round(p.x)}/${Math.round(p.y)}/${Math.round(p.z)}, ` +
                        `health ${player.health}/${player.maxHealth}, mana ${player.mana}/${player.maxMana}, level ${player.level}, xp ${player.experience}, ` +
                        `strength ${player.strength}, dexterity ${player.dexterity}, ${counts}.`);
                    break;
                }
                case "item": {
                    const name = (args[1] || "").toUpperCase();
                    const count = Number(args[2] || 1);
                    if (!name || !Number.isInteger(count) || count <= 0) throw new Error("Usage: comcheck item <INSTANCE> [count], for example comcheck item ITFO_APPLE 5");
                    id(name);
                    if (!player.addItem(name, count)) throw new Error(`Cannot add ${name}`);
                    console.log(`Added ${count} x ${name}. You have ${player.itemCount(name)}.`);
                    break;
                }
                case "clock": {
                    const h = Number(args[1]);
                    const m = Number(args[2] || 0);
                    if (!Number.isInteger(h) || h < 0 || h > 23 || !Number.isInteger(m) || m < 0 || m > 59)
                        throw new Error("Usage: comcheck clock <hour 0-23> [minute 0-59]");
                    world.setTime(h, m);
                    const t = world.time();
                    console.log(`Time set: day ${t.day} ${t.hour}:${String(t.minute).padStart(2, "0")}.`);
                    break;
                }
                case "world": {
                    const worlds = ["ARCHOLOS_MAINLAND", "ARCHOLOS_SEWERS", "ARCHOLOS_SILVERMINE", "ARCHOLOS_VOLFZACKE", "ARCHOLOS_ENDGAME"];
                    if (!args[1] || args[1] === "list") {
                        console.log(`You are in ${world.name()}. Worlds: ${worlds.join(", ")}. comcheck world <name> [waypoint] (comcheck wp <part of the name> shows the waypoints of the world you are in).`);
                        break;
                    }
                    const zen = args[1].toUpperCase().replace(/\.ZEN$/, "") + ".ZEN";
                    if (!worlds.includes(zen.replace(/\.ZEN$/, ""))) throw new Error(`Unknown world, try: ${worlds.join(", ")}`);
                    if (!world.changeLevel(zen, args[2] || "")) throw new Error("Cannot change the world.");
                    console.log(`Going to ${zen}${args[2] ? " at " + args[2] : ""}. Wait for the loading.`);
                    break;
                }
                case "wp": {
                    const found = debug.points(args[1] || "", 25);
                    console.log(found.length ? `Waypoints: ${found.join(", ")}` : "No waypoint found. Give a part of the name, for example comcheck wp PART");
                    break;
                }
                case "tp": {
                    if (!args[1]) throw new Error("Usage: comcheck tp <waypoint>, comcheck wp <part of the name> lists them");
                    if (!world.waypoint(args[1])) throw new Error(`There is no waypoint ${args[1]} in ${world.name()}.`);
                    if (!player.teleport(args[1])) throw new Error("Cannot teleport.");
                    console.log(`Teleported to ${args[1]}.`);
                    break;
                }
                case "heal": {
                    player.heal(9999);
                    call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_MANA").value, 999]);
                    console.log(`Health ${player.health}/${player.maxHealth}, mana ${player.mana}/${player.maxMana}.`);
                    break;
                }
                case "god": {
                    const on = args[1] ? args[1] !== "off" : !debug.god();
                    debug.god(on);
                    console.log(`God mode: ${debug.god() ? "on" : "off"}.`);
                    break;
                }
                case "xp": {
                    const n = Number(args[1]);
                    if (!Number.isInteger(n) || n <= 0) throw new Error("Usage: comcheck xp <points>");
                    call("B_GIVEPLAYERXP", [n]);
                    console.log(`Level ${player.level}, experience ${player.experience}.`);
                    break;
                }
                case "attr": {
                    const names = { strength: "ATR_STRENGTH", dexterity: "ATR_DEXTERITY", health: "ATR_HITPOINTS_MAX", mana: "ATR_MANA_MAX" };
                    const sym = names[(args[1] || "").toLowerCase()];
                    const n = Number(args[2]);
                    if (!sym || !Number.isInteger(n)) throw new Error("Usage: comcheck attr <strength|dexterity|health|mana> <change>, for example comcheck attr strength 50");
                    call("NPC_CHANGEATTRIBUTE", [player, debug.symbol(sym).value, n]);
                    console.log(`${args[1]} changed by ${n}: strength ${player.strength}, dexterity ${player.dexterity}, health ${player.health}/${player.maxHealth}, mana ${player.mana}/${player.maxMana}.`);
                    break;
                }
                case "lock": {
                    // locks: the code of the nearest chest, the talent and the bonus of picking locks (HANDLE_SETPROPERPICKLOCKSTRLEN)
                    const sub = args[1];
                    const talent = () => call("NPC_GETTALENTSKILL", [player, 5]);
                    const bonus = () => debug.symbol("PICKLOCKBONUS").value;
                    const expected = code => call("GETPICKLOCKSTRLEN", [code]);
                    if (sub === "talent") {
                        const level = Number(args[2]);
                        if (!Number.isInteger(level) || level < 0 || level > 3) throw new Error("Usage: comcheck lock talent <0-3>");
                        call("NPC_SETTALENTSKILL", [player, 5, level]);
                        console.log(`Talent of picking locks: ${talent()}. A chest with a code of 8 characters needs ${expected("LRLRLRLR")} of them.`);
                        break;
                    }
                    if (sub === "bonus") {
                        const n = Number(args[2]);
                        if (!Number.isInteger(n) || n < 0 || n > 20) throw new Error("Usage: comcheck lock bonus <0-20>");
                        debug.setSymbol("PICKLOCKBONUS", n);
                        console.log(`PICKLOCKBONUS=${n}. A chest with a code of 8 characters needs ${expected("LRLRLRLR")} of them (talent ${talent()}).`);
                        break;
                    }
                    if (sub === "items") {
                        player.addItem("ITKE_LOCKPICK", 10);
                        player.addItem("ITSC_PICKLOCK", 5);
                        player.addItem("ITRU_PICKLOCK", 1);
                        // the spell takes the mana: with the maximum of 50 and a full bar
                        call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_MANA_MAX").value, 50]);
                        call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_MANA").value, 100]);
                        console.log("You got 10 lockpicks (ITKE_LOCKPICK), 5 scrolls of the lock picking spell (ITSC_PICKLOCK), the rune (ITRU_PICKLOCK) and mana.");
                        break;
                    }
                    if (sub === "off") {
                        const r = debug.setLock("", "");
                        if (!r) throw new Error("No chest nearby. Stand within a few metres of one.");
                        console.log(`${r.tag}: the lock is off.`);
                        break;
                    }
                    // comcheck lock [code]: the nearest chest gets a code and is locked again
                    let code = args[1];
                    if (!code) {
                        code = "";
                        for (let i = 0; i < 8; i++) code += Math.random() < 0.5 ? "L" : "R";
                    }
                    if (!/^[LRlr]+$/.test(code)) throw new Error("The code has only L and R, for example comcheck lock LRLLRLRR");
                    const r = debug.setLock("", code.toUpperCase());
                    if (!r) throw new Error("No chest nearby. Stand within a few metres of one.");
                    console.log(`Chest ${r.tag}: locked, code ${r.code} (${r.code.length} characters). Talent ${talent()}, bonus ${bonus()}: the player has to enter ${expected(r.code)} of them. Close F10 and open it with a lockpick or the spell. comcheck lock items gives the items.`);
                    break;
                }
                case "finish": {
                    // EV_FINISHINGMOVE_HOOK of CoM: finishing off an unconscious NPC_FLAG_IMPORTANT character.
                    // The flag 8 is NPC_FLAG_PROTECTED of the engine: the character is not hurt, so it is set after he is down.
                    if (args[1] === "flag") {
                        if (!finishNpc || !finishNpc.valid) throw new Error("First run comcheck finish.");
                        finishNpc.setFlags(8);   // NPC_FLAG_IMPORTANT
                        console.log("NPC_FLAG_IMPORTANT is set. Close F10, draw the sword, stand next to him and use the key of the finishing move.");
                        break;
                    }
                    const name = "BAU_719_BAKER";
                    id(name);
                    const p = player.position;
                    const a = player.rotation * Math.PI / 180;
                    if (finishNpc && finishNpc.valid) finishNpc.remove();
                    finishNpc = npcs.spawn(name, p.x + Math.cos(a) * 250, p.y, p.z + Math.sin(a) * 250, player.rotation + 180);
                    if (!finishNpc || !finishNpc.valid) throw new Error("Cannot spawn the check NPC. Stand on open, level ground.");
                    if (!debug.parkNpc(finishNpc)) throw new Error("Cannot disable the NPC's routine.");
                    // the baker of the game is NPC_FLAG_IMMORTAL (FLAGS = 2): he would not be hurt
                    finishNpc.setFlags(0);
                    const sword = "ITMW_1H_MIL_SWORD";
                    if (!player.hasItem(sword)) player.addItem(sword, 1);
                    call("EQUIPITEM", [player, id(sword)]);
                    if (!partyStrength) {
                        call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_STRENGTH").value, 100]);
                        partyStrength = true;
                    }
                    console.log("The baker stands in front of you; you have the sword and +100 strength. Close F10 and hit him with the fists (no weapon in the hand) until he is unconscious. Then open F10 and enter comcheck finish flag.");
                    break;
                }
                case "queue": {
                    // a scene that does not go on: the characters near you with a queue of commands, who waits for whom
                    const radius = args[1] ? Number(args[1]) : 6000;
                    const list = debug.queues(radius);
                    if (!list.length) { console.log("No character within " + radius + " has a queue of commands."); break; }
                    console.log(`Characters with commands within ${radius}: ${list.length}`);
                    for (const l of list) console.log("  " + l);
                    break;
                }
                case "fix": {
                    // the cases of Chronicles of Myrtana that were fixed: comcheck fix | all | <case> [check]  (comtest does it with less typing)
                    const cases = fixCases || (fixCases = createFixCases({ debug, player, npcs, world, later }));
                    const show = (name, r) => {
                        const line = `${r.status.toUpperCase().padEnd(4)} ${name}: ${r.message}`;
                        if (r.status === "fail") console.error(line); else console.log(line);
                        };
                    if (!args[1]) {
                        console.log("Cases that were fixed (comtest <case>, or comcheck fix <case>; comcheck fix all runs the ones that check themselves):");
                        for (const c of cases.cases) console.log(`  ${c.name}${c.auto ? "" : c.manual ? " (you look)" : " (two steps)"}: ${c.about}`);
                        break;
                    }
                    if (args[1] === "all") {
                        const results = cases.runAuto();
                        for (const r of results) show(r.name, r);
                        const count = (s) => results.filter(r => r.status === s).length;
                        console.log(`Cases: ${count("pass")} passed, ${count("fail")} failed, ${count("skip")} skipped. The others need two steps or your eyes: comcheck fix`);
                        break;
                    }
                    const c = cases.find(args[1]);
                    if (!c) throw new Error("No such case. comcheck fix lists them.");
                    show(c.name, args[2] === "check" ? cases.check(c.name) : cases.run(c.name));
                    break;
                }
                case "test": {
                    // the tests of the game that run in the game: comcheck test [all|<area>|<word of the name>]
                    const suite = createSelfTests({ debug, player, npcs, world });
                    if (!args[1]) {
                        console.log(`Tests: ${suite.tests.length}; areas: ${suite.areas().join(", ")}. comcheck test all | <area> | <word>`);
                        for (const t of suite.tests) console.log(`  [${t.area}] ${t.name}`);
                        break;
                    }
                    const r = suite.run(args[1]);
                    for (const x of r.results) {
                        const line = `${x.status.toUpperCase().padEnd(4)} [${x.area}] ${x.name}${x.message ? ": " + x.message : ""}`;
                        if (x.status === "fail") console.error(line); else console.log(line);
                        }
                    console.log(`Tests (${args[1]}): ${r.pass} passed, ${r.fail} failed, ${r.skip} skipped.`);
                    break;
                }
                case "dlg": {
                    // the dialogs of the mod: comcheck dlg list [words] | analyze NAME | run NAME [plain] | all [words] | scene FUNCTION [self] | report | reset
                    // (the tab Dialogs of this menu does the same with buttons and a progress)
                    const sub = args[1] || "";
                    const rest = args.slice(2).join(" ");
                    if (sub === "list") {
                        const r = debug.dlgList(rest, 0, 40);
                        console.log(`${r.total} dialogs${r.total > r.rows.length ? ", the first " + r.rows.length : ""}:`);
                        for (const d of r.rows) console.log(`  ${d.npc}: ${d.name} "${d.description}"${d.status ? " [" + d.status + "]" : ""}${d.inWorld ? "" : " (npc not in this world)"}`);
                    } else if (sub === "analyze") {
                        if (!args[2]) throw new Error("Usage: comcheck dlg analyze <DIALOG_INSTANCE>");
                        console.log(debug.dlgAnalyze(args[2]));
                    } else if (sub === "run") {
                        if (!args[2]) throw new Error("Usage: comcheck dlg run <DIALOG_INSTANCE> [plain]");
                        const r = debug.dlgRun(args[2], args[3] !== "plain", true);
                        console.log(r.text);
                    } else if (sub === "scene") {
                        if (!args[2]) throw new Error("Usage: comcheck dlg scene <FUNCTION> [SELF_INSTANCE]");
                        const r = debug.dlgRunFn(args[2], args[3] || "");
                        console.log(r.text);
                    } else if (sub === "all") {
                        // all at once, the game stops for the time: for more than a few hundred use the tab Dialogs
                        const r = debug.dlgList(rest, 0, 500);
                        if (r.total > 300) throw new Error(`${r.total} dialogs: use the tab Dialogs (Test all), or narrow the words.`);
                        debug.dlgReset();
                        for (const d of r.rows) debug.dlgRun(d.name, true, true);
                        const ok = debug.dlgSave("dialogtest_report.txt");
                        const report = debug.dlgReport();
                        for (const line of report.split("\n").slice(0, 60)) console.log(line);
                        console.log(ok ? "The whole report: dialogtest_report.txt (next to log.txt)." : "The report could not be written.");
                    } else if (sub === "report") {
                        const ok = debug.dlgSave("dialogtest_report.txt");
                        for (const line of debug.dlgReport().split("\n").slice(0, 60)) console.log(line);
                        console.log(ok ? "The whole report: dialogtest_report.txt (next to log.txt)." : "The report could not be written.");
                    } else if (sub === "reset") {
                        debug.dlgReset();
                        console.log("The results are forgotten.");
                    } else {
                        throw new Error("Usage: comcheck dlg list|analyze|run|scene|all|report|reset. The tab Dialogs (F10) has buttons and Test all.");
                    }
                    break;
                }
                case "info": {
                    // why a dialog does not appear: comcheck info Viktor DIA_VIKTOR_MARTHATRIALOG
                    if (!args[1] || !args[2]) throw new Error("Usage: comcheck info <npc name part> <INFO_NAME>, for example comcheck info Viktor DIA_VIKTOR_MARTHATRIALOG");
                    const r = debug.infoCondition(args[1], args[2]);
                    if (!r) throw new Error("No such character or info (the condition function is <INFO_NAME>_CONDITION).");
                    console.log(`${args[2]}: condition ${r[0] ? "TRUE" : "FALSE"}, ${r[1] ? "already known" : "not known yet"}. Variables of the condition: ${r[2] || "-"}`);
                    break;
                }
                case "npc": {
                    // where a character is, also one that is far away: for the scenes that move the characters (Ezekiel on the beach of Q101)
                    const name = args[1];
                    if (!name) throw new Error("Usage: comcheck npc <name part> [home|here|go]: lists the characters of the world with the name; home puts the first one at the point of his routine, here next to you, go you next to him. Example: comcheck npc Ezekiel");
                    if (args[2]) {
                        // comcheck npc Martha home | here | go: home puts him at the point of his routine, here next to you, go you next to him
                        const r = debug.moveNpc(name, args[2]);
                        if (!r) throw new Error("No such character or mode (home, here, go).");
                        console.log(r);
                        break;
                    }
                    const list = debug.findNpc(name);
                    if (!list.length) { console.log("No character with '" + name + "' in " + world.name() + "."); break; }
                    for (const l of list) console.log("  " + l);
                    break;
                }
                case "vob": {
                    // CHANGEVOBCOLLISION of CoM: the scripts hide and show the decorations of the quests and switch their collision
                    const name = args[1];
                    if (!name) throw new Error("Usage: comcheck vob <NAME> [go|hide|show|ghost|solid]: go teleports you next to the vob, hide: no visual and no collision, show: both, ghost: visible but walk through, solid: collision only. Example: comcheck vob WOODCUTTERS_VOB_06");
                    const mode = args[2] || "go";
                    let r;
                    if (mode === "hide") r = debug.vobVisual(name, false, false);
                    else if (mode === "show") r = debug.vobVisual(name, true, true);
                    else if (mode === "ghost") r = debug.vobVisual(name, true, false);
                    else if (mode === "solid") r = debug.vobVisual(name, false, true);
                    else r = debug.vobVisual(name);
                    if (!r) throw new Error(`There is no vob ${name} in ${world.name()}.`);
                    if (mode === "go") {
                        player.setPosition(r[1] + 200, r[2] + 100, r[3]);
                        console.log(`Teleported next to ${name} (${r[1].toFixed(0)}, ${r[2].toFixed(0)}, ${r[3].toFixed(0)}). Then: comcheck vob ${name} hide | show | ghost | solid.`);
                    } else {
                        console.log(`${name}: ${mode} applied to ${r[0]} vob(s).`);
                    }
                    break;
                }
                case "faint": {
                    // OCNPC__DROPUNCONSCIOUS_HOOK of CoM: in the fights of the arena (AIVAR[45] = AF_RUNNING), in Q205 and in Q306
                    // the hero keeps the weapon when he falls unconscious, otherwise it falls out of his hand
                    if (args[1] === "wake") {
                        debug.faint(player, false);
                        console.log("The hero gets up.");
                        break;
                    }
                    if (args[1] === "go") {
                        if (!player.weaponItem) throw new Error("The weapon is not in the hand: first run comcheck faint keep or comcheck faint drop and wait 2 seconds with F10 closed.");
                        const kept = player.aiVar(45) === 1;
                        debug.faint(player, true);
                        console.log(kept
                            ? "The hero is unconscious with AIVAR[45]=1. EXPECTED: the sword does NOT lie on the ground, it stays in the inventory (check the inventory after comcheck faint wake)."
                            : "The hero is unconscious with AIVAR[45]=0. EXPECTED: the sword falls to the ground next to him (the normal behaviour of the game). Then comcheck faint wake.");
                        break;
                    }
                    const keep = args[1] !== "drop";
                    const sword = "ITMW_1H_MIL_SWORD";
                    if (!player.hasItem(sword)) player.addItem(sword, 1);
                    call("EQUIPITEM", [player, id(sword)]);
                    if (!partyStrength) {
                        call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_STRENGTH").value, 100]);
                        partyStrength = true;
                    }
                    player.setAiVar(45, keep ? 1 : 0);
                    call("AI_READYMELEEWEAPON", [player]);
                    console.log(keep
                        ? "Mode KEEP: AIVAR[45]=1 (the fight of the arena). Close F10 for 2 seconds (the sword is drawn), open F10 and run comcheck faint go."
                        : "Mode DROP: AIVAR[45]=0 (an ordinary fight). Close F10 for 2 seconds (the sword is drawn), open F10 and run comcheck faint go.");
                    break;
                }
                case "party": {
                    // HIDEFOCUSFORPARTYMEMBERS of CoM: a companion (AIVAR[15]) is not in the focus of a player with a weapon
                    if (args[1] === "off") {
                        if (!partyNpc || !partyNpc.valid) throw new Error("First run comcheck party.");
                        partyNpc.setAiVar(15, 0);
                        console.log("The check NPC is a normal character again: with the weapon in the hand he can be targeted.");
                        break;
                    }
                    const name = "BAU_719_BAKER";
                    id(name);
                    const p = player.position;
                    const a = player.rotation * Math.PI / 180;
                    if (partyNpc && partyNpc.valid) partyNpc.remove();
                    partyNpc = npcs.spawn(name, p.x + Math.cos(a) * 300, p.y, p.z + Math.sin(a) * 300, player.rotation + 180);
                    if (!partyNpc || !partyNpc.valid) throw new Error("Cannot spawn the check NPC. Stand on open, level ground.");
                    if (!debug.parkNpc(partyNpc)) throw new Error("Cannot disable the NPC's routine.");
                    partyNpc.setAiVar(15, 1);
                    // a weapon and the strength to use it
                    const sword = "ITMW_1H_MIL_SWORD";
                    if (!player.hasItem(sword)) player.addItem(sword, 1);
                    call("EQUIPITEM", [player, id(sword)]);
                    if (!partyStrength) {
                        call("NPC_CHANGEATTRIBUTE", [player, debug.symbol("ATR_STRENGTH").value, 100]);
                        partyStrength = true;
                    }
                    console.log("You got the sword ITMW_1H_MIL_SWORD (equipped) and +100 strength. The baker with AIVAR[15]=1 stands in front of you. Close F10. Without a weapon he is in the focus (his name). Draw a weapon: the name must disappear and he must not be targeted. comcheck party off makes him a normal character.");
                    break;
                }
                case "trialog": {
                    const names = ["TRIA_RUNNING", "TRIA_CPTR", "TRIA_SELF", "TRIA_LAST", "Q102_JORNCHOSEN", "Q102_FABIOCHOSEN"];
                    for (const name of names) {
                        const symbol = debug.symbol(name);
                        if (symbol) console.log(`${name}=${symbol.value}`);
                    }
                    console.log("After naturally finishing the Q102 choice (go with Jorn/Fabio), TRIA_RUNNING and TRIA_CPTR must become 0, the camera must return, and ZS_TALK_END must not report an unfinished trialog. Use comcheck queue for any pending actions. This command only reads state; it does not force completion.");
                    break;
                }
                case "crafting": {
                    for (const name of ["CRAFTINGVIEW_ISOPEN", "CRAFTINGVIEWLASTCHOICE", "CV_CURRENTITEM", "CV_CURRENTMODE", "CV_CURRENTAMOUNT", "CRAFTINGVIEW_SELECTEDTAB"]) {
                        const symbol = debug.symbol(name);
                        if (symbol) console.log(`${name}=${symbol.value}`);
                    }
                    console.log("Read-only crafting state. At a real workbench/alchemy table/cooking station select several recipes and quantities, including All. Check the resulting model, ingredients, owned/required counts and properties tab. Leave the menu: the preview and its texts must disappear. Reopen and craft normally; this command does not craft or grant items.");
                    break;
                }
                case "breath":
                case "slowdown": {
                    const npc = args[1] ? npcs.find(args[1]) : player;
                    if (!npc || !npc.valid) throw new Error("No such live NPC in this world.");
                    const state = npc.compatibilityState;
                    if (!state) throw new Error("This command needs the current compatibility build of Gothic2Notr.exe.");
                    console.log(`${npc.instance}: diving=${state.diving}, elapsed=${state.diveElapsedMs} ms, limit=${state.diveLimitMs} ms, remaining=${state.diving ? state.diveLimitMs - state.diveElapsedMs : state.diveLimitMs} ms, requested model scale=${state.requestedModelScale}, HP=${npc.health}/${npc.maxHealth}.`);
                    console.log("Read only: does not cast, grant a potion, refill air or change the quest. Breath: naturally use Dive10, dive, save/load and let the buff expire. Slowdown: naturally cast on this NPC, compare animation/movement/hits, save/load during the effect and check the return to 1.0. Use comcheck slowdown NPC_INSTANCE to inspect its target; inspect the player during Otis Q306.");
                    break;
                }
                case "attitude": {
                    if (!args[1]) throw new Error("Usage: comcheck attitude NPC_INSTANCE (reads only)");
                    const npc = npcs.find(args[1]);
                    if (!npc || !npc.valid) throw new Error("No such live NPC in this world.");
                    const current = call("NPC_GETATTITUDE", [npc, player]);
                    const permanent = call("NPC_GETPERMATTITUDE", [npc, player]);
                    console.log(`${npc.instance}: current attitude=${current}, permanent attitude=${permanent}, party AIVAR[15]=${npc.aiVar(15)}. Expected codes: 0 hostile, 1 angry, 2 neutral, 3 friendly.`);
                    console.log("Read only. Compare before/during/after a natural quest fight and after save/load; a temporary fight must not permanently change the relationship. Check Q205/Q306 and party combat separately.");
                    break;
                }
                case "telekinesis": {
                    for (const name of ["STAGE", "NUMB_OF_STEPS", "STEP_X", "STEP_Y", "STEP_Z"]) {
                        const symbol = debug.symbol(name);
                        if (symbol) console.log(`${name}=${symbol.value}`);
                    }
                    console.log("Read only. Naturally cast Telekinesis on a loose world item 2-10 metres away. It must rise by its height, travel toward you with the original mana cost and drop normally when released/completed. Pick it up once. Repeat with no valid target, too-close item, exhausted mana, interrupted cast and save/load during levitation; no item may remain floating permanently or duplicate. STEP values above are float bit patterns.");
                    break;
                }
                case "map": {
                    const maps = {
                        archolos: "USE_MAP_ARCHOLOS", silbach: "USE_MAP_SILBACH",
                        city: "USE_MAP_CITY", sewers: "USE_MAP_SEWERS"
                    };
                    const choice = args[1] || "archolos";
                    if (choice === "off") {
                        call("SPRITEMAP_DESTROY", []);
                        console.log("Closed the CoM sprite map.");
                        break;
                    }
                    if (!maps[choice]) throw new Error("Usage: comcheck map archolos|silbach|city|sewers|off");
                    call(maps[choice], [], { self: player, other: player });
                    console.log(`Opened ${choice} through its CoM use function. Close F10: the map should be square and the cursor should stay above it and follow position/heading where this map supports the current world. Close normally or use comcheck map off. Also check natural map-item use and quest maps.`);
                    break;
                }
                case "preview": {
                    if (itemPreview) {
                        call("RENDER_REMOVE", [itemPreview]);
                        itemPreview = 0;
                    }
                    if (args[1] === "off") {
                        console.log("Removed the LeGo item preview.");
                        break;
                    }
                    const item = args[1] || "ITMW_1H_MIL_SWORD";
                    call("_RENDER_INIT", []);
                    itemPreview = call("RENDER_ADDITEMPRIO", [id(item), 1024, 1024, 3072, 3072, 0]);
                    if (!itemPreview) throw new Error("Render_AddItemPrio returned no view.");
                    console.log(`Created a LeGo 3D preview of ${item}. Close F10: expect a model in the upper-left area, with no inventory item granted. Use comcheck preview off before saving/loading/changing world. Also inspect the natural crafting window's ingredient and result previews.`);
                    break;
                }
                case "category": {
                    id("CATINV_SETCATEGORY");
                    if (args[1] !== undefined && args[1] !== "status") {
                        const index = Number(args[1]);
                        if (!Number.isInteger(index) || index < 0 || index > 8)
                            throw new Error("Usage: comcheck category 0..8|status");
                        const changed = call("CATINV_SETCATEGORY", [index]);
                        console.log(`CATINV_SETCATEGORY(${index}): changed=${changed}.`);
                    }
                    console.log(`CATINV_ACTIVECATEGORY=${debug.symbol("CATINV_ACTIVECATEGORY").value}. Order: 0 all, 1 weapons, 2 armor, 3 magic, 4 artifacts, 5 food, 6 potions, 7 documents, 8 other. Open inventory, inspect the filter, close and reopen: own inventory should remember it. Open a chest/trade/ransack: the initial category should return. Transfer the last item of a category and check both lists.`);
                    break;
                }
                case "torch": {
                    const action = args[1] || "status";
                    if (!["status", "stow"].includes(action)) throw new Error("Usage: comcheck torch status|stow");
                    const holding = call("TORCH_HEROHOLDINGTORCH", []);
                    if (action === "stow") {
                        if (!holding) throw new Error("No burning torch is detected in the left hand. Equip one normally first.");
                        const before = player.itemCount("ITLSTORCH");
                        call("TORCH_UNEQUIP", []);
                        console.log(`Stowed through CoM TORCH_UNEQUIP: holding=${call("TORCH_HEROHOLDINGTORCH", [])}, unlit torches ${before} -> ${player.itemCount("ITLSTORCH")}. Expected: the flame disappears, holding=0, exactly one unlit torch is returned. No torch should appear on the ground.`);
                    } else {
                        console.log(`CoM detects burning torch in left hand: ${holding ? "YES" : "NO"}; unlit torches=${player.itemCount("ITLSTORCH")}. Equip normally, check again, then use comcheck torch stow. Also verify the normal torch key and save/reload while holding it.`);
                    }
                    break;
                }
                case "spellactive": {
                    const name = args[1] || "SPL_LIGHT";
                    const spell = debug.symbol(name);
                    if (!spell || !Number.isInteger(spell.value)) throw new Error(`Unknown spell constant: ${name}`);
                    const active = call("ISSPELLACTIVE", [player, spell.value]);
                    console.log(`${name}: ${active ? "ACTIVE" : "INACTIVE"}. Compare before casting, while the committed spell effect is visible, and after it expires. Merely equipping a rune must remain INACTIVE. For Q501, cast light in the dark room and check the natural journal/room update.`);
                    break;
                }
                case "musiczone": {
                    for (const name of ["CURRENTMUSICZONE", "CURRENTLEVEL", "INEXTREMO_CANPLAYCONCERT", "INEXTREMO_STARTCONCERT"]) {
                        const symbol = debug.symbol(name);
                        if (symbol) console.log(`${name}=${symbol.value}`);
                    }
                    console.log("Read only. Cross natural overlapping music zones and wait up to five seconds; compare day/night/combat. Check the In Extremo concert at 18-21, then leave and return. Save/load while its zone is enabled and disabled, and after changing worlds. This command does not enable a zone, advance time or start the concert.");
                    break;
                }
                case "cutscene": {
                    for (const name of ["ISINCAMERA", "ISINFINALBOARDS", "CURRENTTRIGGEREDCAMERAPTR", "TRIA_RUNNING", "TRIA_CPTR"]) {
                        const symbol = debug.symbol(name);
                        if (symbol) console.log(`${name}=${symbol.value}`);
                    }
                    console.log("Read only. During a natural CoM camera scene check that status/focus and menu remain blocked, including after two minutes. NPC subtitles moved by the script should use the lower field; final-board lines must hide the speaker name and use the script color. After the scene normal HUD, menu and subtitles return. This command does not start, skip or finish a scene.");
                    break;
                }
                case "scenelock": {
                    const mode = args[1];
                    if (!["load", "save", "both", "off"].includes(mode))
                        throw new Error("Usage: comcheck scenelock load|save|both|off. Use while idle on a disposable save; off enables both operations.");
                    if (mode === "both" || mode === "off") {
                        call("ENABLELOADSAVEGAME", [mode === "off" ? 1 : 0]);
                    } else {
                        call("ENABLELOADSAVEGAME", [1]);
                        call(mode === "load" ? "ENABLELOADGAME" : "ENABLESAVEGAME", [0]);
                    }
                    console.log(`Scene lock: ${mode}. Check save/load buttons and quick-load. A blocked load must keep the current world. Use comcheck scenelock off to enable both again; do not save while a manual lock is active.`);
                    break;
                }
                case "housewall": {
                    // Real CoM function: wallpaper choices 0..11, including Christmas.
                    const key = "HOUSE_CRAFT_WALL_CHANGETEXTURE";
                    id("APPLYHOUSEWALLTEXTURE");
                    const current = debug.symbol(key);
                    if (!current) throw new Error("The CoM wallpaper variable is missing.");
                    const action = args[1] || "status";
                    if (action === "status") {
                        console.log(`House wallpaper=${current.value}. Stand inside your house, then comcheck housewall 1..11, 0 for default, restore to undo. This changes the saved choice; use a disposable save.`);
                        break;
                    }
                    let value;
                    if (action === "restore") {
                        if (houseWallChoice === null) throw new Error("No wallpaper choice was saved by this command in this session.");
                        value = houseWallChoice;
                    } else {
                        value = Number(action);
                        if (!Number.isInteger(value) || value < 0 || value > 11)
                            throw new Error("Usage: comcheck housewall 0..11|status|restore");
                    }
                    if (houseWallChoice === null) houseWallChoice = current.value;
                    debug.setSymbol(key, value);
                    call("APPLYHOUSEWALLTEXTURE", []);
                    console.log(`Applied wallpaper ${value}. Close F10 and inspect the walls; then try another choice and default (0). Save/reload and verify the selected pattern remains. Restore only restores the choice remembered in this JS session.`);
                    break;
                }
                case "bestweapon": {
                    id("NpcEquipBestWeaponByType");
                    const before = `melee=${player.weaponItem}, ranged=${player.rangedItem}`;
                    call("NpcEquipBestWeaponByType", [player, 2]);
                    call("NpcEquipBestWeaponByType", [player, 4]);
                    const after = `melee=${player.weaponItem}, ranged=${player.rangedItem}`;
                    // Repeat the same calls: equipment must stay equipped.
                    call("NpcEquipBestWeaponByType", [player, 2]);
                    call("NpcEquipBestWeaponByType", [player, 4]);
                    console.log(`Before: ${before}; selected: ${after}; repeated: melee=${player.weaponItem}, ranged=${player.rangedItem}. Inspect equipped melee/ranged weapons in inventory: strongest usable weapons should be equipped and remain equipped on repetition. Stand idle with weapons sheathed and use a disposable save.`);
                    break;
                }
                case "inventory": {
                    // LOCKINVENTORY of CoM: with the flag the inventory closes itself right after it was opened
                    const on = args[1] !== "off";
                    debug.setSymbol("PLAYERLOCKINVENTORY", on ? 1 : 0);
                    console.log(on ? "PLAYERLOCKINVENTORY=1. Close F10 and press the inventory key: it must close at once. Use comcheck inventory off."
                                   : "PLAYERLOCKINVENTORY=0. The inventory opens normally.");
                    break;
                }
                case "time": {
                    // SCALETIME of the scripts changes the oCWorldTimer: the time of the world must run faster
                    const now = world.time();
                    const minutes = t => t.day * 1440 + t.hour * 60 + t.minute;
                    if (args[1] === "stop") {
                        call("CHANGETIMESPEED", [100]);
                        if (!timeCheck) throw new Error("First run comcheck time.");
                        const game = minutes(now) - timeCheck.minutes;
                        const real = (Date.now() - timeCheck.real) / 1000;
                        console.log(`Speed 100 again. ${real.toFixed(1)} s of the real time = ${game} game minutes (about ${(game * 60 / real).toFixed(1)} game seconds per second; the normal speed is about 14.5).`);
                        timeCheck = null;
                        break;
                    }
                    timeCheck = { minutes: minutes(now), real: Date.now() };
                    call("CHANGETIMESPEED", [2200]);
                    console.log(`Time speed 2200 started at day ${now.day} ${now.hour}:${String(now.minute).padStart(2, "0")}. Close F10, wait 10 seconds, open it and use comcheck time stop.`);
                    break;
                }
                case "text":
                    message(player, `COM text with spaces: OK #${++sequence}`);
                    console.log("Close F10; the queued external must print COM text with spaces: OK.");
                    break;
                case "ranged":
                    ranged(args[1]);
                    break;
                case "scene":
                    if (args[1] && args[1] !== "smokers") throw new Error("Usage: comcheck scene [smokers]");
                    smokersScene();
                    break;
                default:
                    console.log("Tools for saves: state | scale [instance] [factor] | item <NAME> [n] | clock <h> [m] | world [list|<name> [waypoint]] | wp [part] | tp <waypoint> | heal | god [off] | xp <n> | attr <strength|dexterity|health|mana> <n>.");
                    console.log("New CoM checks: trialog | crafting | cutscene | musiczone | breath [NPC_INSTANCE] | slowdown [NPC_INSTANCE] | attitude NPC_INSTANCE | telekinesis | bestweapon | spellactive [SPL_LIGHT] | torch status|stow | category 0..8|status | map archolos|silbach|city|sewers|off | preview [item]|off | housewall 0..11|status|restore | scenelock load|save|both|off. Details: docs/ARCHOLOS_PROGRESS_2026-10-24.md.");
                    console.log("Manual checks on a disposable save: comcheck wait [seconds] | cancel | drop [item] | chest [vob-name] [item] [count] | text | time [stop] | inventory [off] | trader | party [off] | finish [flag] | faint [keep|drop|go|wake] | vob <NAME> [go|hide|show|ghost|solid] | queue [radius] | lock [code|off|items|talent N|bonus N] | ranged [bow|crossbow|aim|shoot|stop|status|clear] | scene [smokers] | fix [case] [check] | test [all|area] | dlg list|analyze|run|all|report");
                    screen.print("comcheck: see the Console and docs/ARCHOLOS_TESTING.md");
                    break;
            }
        } catch (error) {
            console.error(String(error));
        }
    }, "comcheck - manual checks of Archolos compatibility; use a disposable save");

    console.registerCommand("comtest", ({ args }) => {
        try {
            if (!player.valid) throw new Error("Load a disposable save first.");
            fixCommand(args);
        } catch (error) {
            console.error(String(error));
        }
    }, "comtest <fight|teleport|sort|bag|tool|all> - tests of the cases of Chronicles of Myrtana that were fixed; use a disposable save");
}

module.exports = { registerArcholosChecks };

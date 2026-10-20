// Manual checks only: no game state is changed until the user enters comcheck.
"use strict";

function registerArcholosChecks({ debug, player, npcs, world, console, screen }) {
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
                case "npc": {
                    // where a character is, also one that is far away: for the scenes that move the characters (Ezekiel on the beach of Q101)
                    const name = args[1];
                    if (!name) throw new Error("Usage: comcheck npc <name part> [go]: lists the characters of the world with the name, go teleports you to the first one. Example: comcheck npc Ezekiel");
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
                    console.log("Manual checks on a disposable save: comcheck wait [seconds] | cancel | drop [item] | chest [vob-name] [item] [count] | text | time [stop] | inventory [off] | trader | party [off] | finish [flag] | faint [keep|drop|go|wake] | vob <NAME> [go|hide|show|ghost|solid] | queue [radius] | lock [code|off|items|talent N|bonus N] | ranged [bow|crossbow|aim|shoot|stop|status|clear] | scene [smokers]");
                    screen.print("comcheck: see the Console and docs/ARCHOLOS_TESTING.md");
                    break;
            }
        } catch (error) {
            console.error(String(error));
        }
    }, "comcheck - manual checks of Archolos compatibility; use a disposable save");
}

module.exports = { registerArcholosChecks };

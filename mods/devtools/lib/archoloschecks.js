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
                    console.log("Manual checks on a disposable save: comcheck wait [seconds] | cancel | drop [item] | chest [vob-name] [item] [count] | text | time [stop] | inventory [off] | trader | ranged [bow|crossbow|aim|shoot|stop|status|clear] | scene [smokers]");
                    screen.print("comcheck: see the Console and docs/ARCHOLOS_TESTING.md");
                    break;
            }
        } catch (error) {
            console.error(String(error));
        }
    }, "comcheck - manual checks of Archolos compatibility; use a disposable save");
}

module.exports = { registerArcholosChecks };

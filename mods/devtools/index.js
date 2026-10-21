// Developer tools: the tab Debug in the menu of the mods (F10, on a phone the button Mods of the settings).
// The log of the game, calls of the functions of the scripts, teleports, triggers, spawns, the report of the layer
// for Ikarus and LeGo. Everything is in lib/.
"use strict";

const { createPanel } = require("./lib/panel.js");
const { registerArcholosChecks } = require("./lib/archoloschecks.js");
const { createTimers } = require("./lib/timers.js");

// the second step of the tests that need some seconds of the game (comtest teleport, ...) is done by the ticks of the game
const timers = createTimers();
// (not while the game is paused by the menu: the commands of the queues that a test waits for do not run then)
if (typeof events !== "undefined") events.on("tick", ({ dt }) => { if (typeof debug === "undefined" || !debug.paused()) timers.advance(dt); });

if (typeof debug !== "undefined")
    registerArcholosChecks({ debug, player, npcs, world, console, screen, later: timers.later });

if (typeof ui !== "undefined" && typeof debug !== "undefined") {
    const panel = createPanel({ ui, debug, player, npcs, world });
    ui.tab("Debug", panel.draw);
    // the run of all the dialogs goes on when the menu is closed
    if (typeof events !== "undefined") events.on("tick", () => panel.views.Dialogs.tickAll());
    console.log("Developer tools are ready: the tab Debug");
}

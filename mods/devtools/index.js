// Developer tools: the tab Debug in the menu of the mods (F10, on a phone the button Mods of the settings).
// The log of the game, calls of the functions of the scripts, teleports, triggers, spawns, the report of the layer
// for Ikarus and LeGo. Everything is in lib/.
"use strict";

const { createPanel } = require("./lib/panel.js");
const { registerArcholosChecks } = require("./lib/archoloschecks.js");

if (typeof debug !== "undefined")
    registerArcholosChecks({ debug, player, npcs, world, console, screen });

if (typeof ui !== "undefined" && typeof debug !== "undefined") {
    const panel = createPanel({ ui, debug, player, npcs, world });
    ui.tab("Debug", panel.draw);
    console.log("Developer tools are ready: the tab Debug");
}

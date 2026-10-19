// The tab Debug of the menu of the mods. A row of buttons chooses what is shown; the log goes on in the background,
// so a call from the other sections has its lines when the player looks at the log.
"use strict";

const { LogView } = require("./logview.js");
const { ScriptView } = require("./scriptview.js");
const { WorldView } = require("./worldview.js");
const { IkarusView } = require("./ikarusview.js");

const Gold = [0.82, 0.66, 0.34, 1];
const Sections = ["Log", "Scripts", "World", "Ikarus"];

/// env: { ui, debug, player, npcs, world }
function createPanel(env) {
    const log = new LogView(env.debug);
    const full = { ...env, log };
    const views = {
        Log: log,
        Scripts: new ScriptView(full),
        World: new WorldView(full),
        Ikarus: new IkarusView(full),
    };
    let section = "Log";

    function draw() {
        const ui = env.ui;
        // the log is read every frame, it does not matter which section is open
        log.poll();

        for (const name of Sections) {
            ui.withColor(name === section ? Gold : [0.85, 0.85, 0.85, 1], () => {
                if (ui.button(name + "##section", ui.fontSize() * 6, ui.fontSize() * 2)) section = name;
            });
            ui.sameLine();
        }
        ui.textDisabled(`${log.lines.length} log lines`);
        ui.separator();

        // the log fills the room that is left, the other sections scroll
        if (section === "Log") {
            views.Log.draw(ui, -ui.fontSize() * 0.5);
        } else {
            ui.child("##section", -1, 0, () => views[section].draw(ui));
        }
    }

    return { draw, views, log };
}

module.exports = { createPanel };

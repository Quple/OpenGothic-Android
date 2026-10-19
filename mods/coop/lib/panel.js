// The tab of the co-op in the menu of the mods (F10 on a computer, the button of the mods on a phone): host a game or
// join one, see who is in it. It is drawn with ui, see docs/MODS.md. The keyboard of the screen comes by itself
// when a field of text has the focus.
"use strict";

const Gold = [0.82, 0.66, 0.34, 1];
const Green = [0.45, 0.85, 0.45, 1];
const Amber = [0.95, 0.78, 0.30, 1];
const Orange = [0.95, 0.60, 0.30, 1];

/// coop: createCoop(), ui: the widgets, env.saveName(name) keeps the name between the games
function createPanel(ui, coop, env = {}) {
    let nameField = "";
    let lastName = null;
    let addressField = "";
    let joinPort = "7777";
    let hostPort = "7777";

    function attempt(fn) {
        try {
            fn();
        } catch (e) {
            coop.notify(e.message);
        }
    }

    function name(s) {
        // the name that the mod has is the name in the field, until the player types another one
        if (!ui.anyActive() && s.name !== lastName) {
            nameField = s.name;
            lastName = s.name;
        }
        ui.textDisabled("Your name");
        ui.setNextWidth(-(ui.textWidth("Set") + ui.fontSize() * 2.4));
        nameField = ui.inputText("##coopname", nameField);
        ui.sameLine();
        if (ui.button("Set") && nameField.trim() !== "") {
            coop.setName(nameField.trim());
            env.saveName?.(nameField.trim());
        }
        ui.separator();
    }

    function offline(s, line) {
        ui.withColor(Gold, () => ui.text("Host a game"));
        ui.textWrapped("The other players join with the address of this device, which is shown after the game has started. " +
                       "All the devices have to be in the same network (Wi-Fi), or this one has to be reachable from the Internet.");
        ui.setNextWidth(ui.fontSize() * 6);
        hostPort = ui.inputText("Port##host", hostPort, "decimal");
        ui.sameLine();
        if (ui.button("Host", -1, line))
            attempt(() => coop.host(Number(hostPort || 7777)));

        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Join a game"));
        ui.textDisabled("Address of the host, for example 192.168.0.12");
        ui.setNextWidth(-ui.fontSize() * 7.5);
        addressField = ui.inputText("##coopaddr", addressField);
        ui.sameLine();
        ui.setNextWidth(ui.fontSize() * 5);
        joinPort = ui.inputText("##coopport", joinPort, "decimal");
        if (ui.button("Join", -1, line) && addressField.trim() !== "")
            attempt(() => coop.join(addressField.trim(), Number(joinPort || 7777)));

        if (s.recent.length > 0) {
            ui.spacing();
            ui.textDisabled("Games that you have joined");
            s.recent.forEach((entry, i) => {
                ui.withId("recent" + i, () => {
                    if (!ui.button(entry, -1, line)) return;
                    const at = entry.lastIndexOf(":");
                    addressField = at < 0 ? entry : entry.slice(0, at);
                    joinPort = at < 0 ? "7777" : entry.slice(at + 1);
                    attempt(() => coop.join(addressField, Number(joinPort)));
                });
            });
        }
    }

    function online(s, line) {
        if (s.mode === "host") {
            ui.withColor(Green, () => ui.text(`Hosting on port ${s.port}`));
            if (s.addresses.length === 0) {
                ui.textWrapped("The address of this device is not known: look it up in the settings of the network.");
            } else {
                ui.textWrapped("The others join with:");
                for (const a of s.addresses)
                    ui.withColor(Gold, () => ui.withScale(1.3, () => ui.text(`${a}:${s.port}`)));
            }
        } else if (s.connected) {
            ui.withColor(Green, () => ui.text("You are in the game of the host"));
        } else {
            ui.withColor(Amber, () => ui.text("Connecting..."));
        }

        ui.spacing();
        ui.textDisabled("Players");
        ui.bullet(`${s.name} (you)`);
        for (const p of s.players)
            ui.bullet(p.world ? `${p.name}, ${p.world}` : p.name);

        if (s.mode === "client" && s.connected) {
            ui.textDisabled(`Characters of the host: ${s.puppets}`);
            if (s.mismatch > 0)
                ui.withColor(Orange, () => ui.textWrapped(`${s.mismatch} do not match: both players should start a new game`));
        }

        ui.spacing();
        if (ui.button(s.mode === "host" ? "Stop the game" : "Leave", -1, line))
            attempt(() => coop.leave());
    }

    function draw() {
        const s = coop.info();
        const line = ui.fontSize() * 2.3;

        name(s);
        if (s.mode === "off") offline(s, line); else online(s, line);

        if (s.message) {
            ui.spacing();
            ui.separator();
            ui.textWrapped(s.message);
        }
    }

    return { draw };
}

module.exports = { createPanel };

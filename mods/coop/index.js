// Co-op for OpenGothic. Put the folder "coop" into mods/ and press F10, tab Console:
//   coop host [port]            the game of this player is the host (the port is 7777 by default)
//   coop join <address> [port]  join the game of a host
//   coop leave, coop status, coop say <text>, coop name <name>
//
// The host decides what the characters of the world do; the others see them. Both players must start a new game
// (or load the same save) and be in the same world. See docs/COOP.md.
"use strict";

const { createCoop } = require("./lib/coop.js");

const DefaultPort = 7777;

// the time of the game, as the last frame has told it
let clock = 0;

const env = {
    net,
    player,
    npcs,
    worldName: () => world.name() || null,
    now: () => clock,
    print: (text) => screen.print(text),
    takeShots: () => world.takeShots(),
    shoot: (...shot) => world.shoot(...shot),
    log: (level, text) => (console[level] ?? console.log)(text),
    // the character of another player looks like his own, unless the instance is not known here
    avatarInstance: (instance) => storage.get("avatar", null) ?? instance,
};

const coop = createCoop(env);
const saved = storage.get("name", null);
if (saved) coop.setName(saved);

events.on("net:connect", (e) => coop.onConnect(e));
events.on("net:message", (e) => coop.onMessage(e));
events.on("net:disconnect", (e) => coop.onDisconnect(e));
events.on("net:error", (e) => coop.onError(e));
events.on("world:change", () => coop.onWorldChange());
events.on("tick", ({ dt, time }) => {
    clock = time;
    coop.tick(dt, time);
});
events.on("shutdown", () => coop.leave());

function port(text) {
    const p = text === undefined ? DefaultPort : Number(text);
    if (!Number.isInteger(p) || p < 1 || p > 65535)
        throw new Error(`"${text}" is not a port`);
    return p;
}

console.registerCommand("coop", ({ args, raw }) => {
    const [cmd, ...rest] = args;
    try {
        switch (cmd) {
            case "host":
                coop.host(rest[0] === undefined ? DefaultPort : port(rest[0]));
                break;
            case "join":
                if (rest.length === 0) throw new Error("coop join <address> [port]");
                coop.join(rest[0], port(rest[1]));
                break;
            case "leave":
                coop.leave();
                break;
            case "say":
                if (!coop.chat(raw.replace(/^\s*say\s+/, ""))) console.warn("You are not in a game");
                break;
            case "name":
                if (rest.length === 0) throw new Error("coop name <name>");
                coop.setName(rest.join(" "));
                storage.set("name", rest.join(" "));
                console.log("Your name is " + rest.join(" "));
                break;
            case "avatar":
                // the instance for the characters of the other players, "coop avatar" clears it
                storage.set("avatar", rest[0] ?? null);
                console.log(rest[0] ? `The other players look like ${rest[0]}` : "The other players look like their own characters");
                break;
            case "status":
            case undefined:
                console.log(coop.status());
                break;
            default:
                console.warn("coop host [port] | join <address> [port] | leave | status | say <text> | name <name> | avatar [instance]");
        }
    } catch (e) {
        console.error(e.message);
    }
}, "coop host|join|leave|status|say|name - play with friends");

console.log("Co-op is ready, type: coop host   or   coop join <address>");

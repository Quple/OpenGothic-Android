// The messages of the co-op. A message is a line of JSON, see net.send. Both sides check what comes,
// a bad message is dropped and never reaches the game.
"use strict";

const Visual = require("./visual.js");

const Version = 4;

const Types = {
    hello:   "hello",     // client -> host: who I am
    welcome: "welcome",   // host -> client: your number and the players that are here
    join:    "join",      // host -> clients: a player has come
    leave:   "leave",     // host -> clients: a player has gone
    player:  "p",         // state of a player
    npcs:    "n",         // host -> clients: the characters of the world
    hit:     "hit",       // client -> host: a character was hit
    chat:    "chat",      // text for everybody
    need:    "need",      // client -> host: these characters are not in my world, tell me about them
    spawn:   "spawn",     // host -> client: the characters that the client has asked for
    shot:    "shot",      // an arrow or a bolt has been shot
    hurt:    "hurt",      // host -> client: a character of the world has hit the player of the client
};

function num(v) {
    return typeof v === "number" && Number.isFinite(v);
}

function vec(m) {
    return num(m.x) && num(m.y) && num(m.z) && num(m.a);
}

/// Returns the message or null, if it is not right
function decode(text) {
    let m;
    try {
        m = JSON.parse(text);
    } catch (e) {
        return null;
    }
    if (m === null || typeof m !== "object" || typeof m.t !== "string")
        return null;

    switch (m.t) {
        case Types.hello:
            if (m.v !== Version || typeof m.name !== "string" || typeof m.instance !== "string")
                return null;
            break;
        case Types.welcome:
            if (m.v !== Version || !num(m.id) || !Array.isArray(m.players))
                return null;
            break;
        case Types.join:
            if (!num(m.id) || typeof m.name !== "string" || typeof m.instance !== "string")
                return null;
            break;
        case Types.leave:
            if (!num(m.id))
                return null;
            break;
        case Types.player:
            if (!vec(m))
                return null;
            if (!Visual.validLook(m))
                return null;
            break;
        case Types.npcs:
            if (!Array.isArray(m.e) || (m.r !== undefined && !Array.isArray(m.r)) || (m.d !== undefined && !Array.isArray(m.d)))
                return null;
            break;
        case Types.hit:
            if (!num(m.id))
                return null;
            // damage, or the finishing of an unconscious character
            if (m.f !== 1 && (!num(m.d) || m.d <= 0 || m.d > 100000))
                return null;
            break;
        case Types.chat:
            if (typeof m.text !== "string")
                return null;
            break;
        case Types.need:
            if (!Array.isArray(m.ids) || m.ids.length > 64 || !m.ids.every(num))
                return null;
            break;
        case Types.spawn:
            if (!Array.isArray(m.e) || m.e.length > 64)
                return null;
            break;
        case Types.hurt:
            if (!num(m.d) || m.d <= 0 || m.d > 100000)
                return null;
            break;
        case Types.shot:
            if (!Array.isArray(m.s) || m.s.length !== 7 || typeof m.s[0] !== "string" || m.s[0].length > 64 ||
                !m.s.slice(1).every(num))
                return null;
            break;
        default:
            return null;
    }
    return m;
}

function encode(m) {
    return JSON.stringify(m);
}

/// The numbers are shorter and the messages smaller
function round(v) {
    return Math.round(v * 10) / 10;
}

module.exports = { Version, Types, decode, encode, round };

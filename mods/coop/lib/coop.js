// The session of the co-op: one game is the host, the others are its clients. The host decides what the
// characters of the world do, every game decides what its own player does.
//
// The game and the network come in env, so the same code runs in the game and in the tests:
//   env.net        net of the host of the mods
//   env.player     the player: valid, x, y, z, rotation, health, maxHealth, name, instance
//   env.npcs       npcs of the game: snapshot, byId, spawn
//   env.worldName()  the name of the world, null if there is no game
//   env.avatarInstance(instance)  the instance for the character of another player
//   env.print(text)  a line on the screen
//   env.log(level, text)
"use strict";

const { Version, Types, decode, encode, round } = require("./protocol.js");
const { Avatars } = require("./avatars.js");
const { HostSync, ClientSync } = require("./npcsync.js");
const Visual = require("./visual.js");

const PlayerInterval = 50;    // ms between the states of the player
const NpcInterval    = 100;   // ms between the states of the characters
const MaxPlayers     = 8;
const MaxText        = 200;

function createCoop(env) {
    const net = env.net;
    const log = env.log;

    let mode = "off";          // "off", "host", "client"
    let myId = 0;              // the host is 0
    let hostPeer = 0;          // client: the connection with the host
    let connected = false;     // client: the host has answered
    let myName = "";
    let nextId = 1;
    let nextPlayerSend = 0;
    let nextNpcSend = 0;

    const players = new Map(); // number of the player -> { id, name, instance, peer, state }
    const peers = new Map();   // host: connection -> number of the player

    const look = new Visual.Tracker();   // what the player looks like, told when it changes
    const avatars = new Avatars(env);
    const hostSync = new HostSync(env);
    const clientSync = new ClientSync(env);

    function send(peer, message) {
        return net.send(peer, encode(message));
    }

    function name() {
        return myName || env.player.name || "Player";
    }

    function myState(now) {
        const p = env.player;
        const w = env.worldName();
        if (!p.valid || !w)
            return null;
        const state = {
            t: Types.player,
            x: round(p.x), y: round(p.y), z: round(p.z), a: Math.round(p.rotation),
            hp: p.health, mhp: p.maxHealth, w,
        };
        const ws = p.weaponState ?? 0, wi = p.weaponItem ?? "", ov = p.overlays ?? [], ar = p.armor ?? "";
        const rw = p.rangedItem ?? "";
        const v = look.next(p.animation ? p.animation() : [], ws, wi, ov, ar, rw, now);
        state.ws = ws;
        state.wi = wi;
        state.ar = ar;
        state.rw = rw;
        state.in = p.instance ?? "";
        if (v) {
            state.an = v.an;
            state.ov = v.ov;
        }
        return state;
    }

    function listOf() {
        return [...players.values()].map(p => ({ id: p.id, name: p.name, instance: p.instance }));
    }

    function say(text) {
        env.print(text);
        log("info", text);
    }

    // --- start and stop

    function host(port) {
        if (mode !== "off")
            throw new Error(`already in the co-op (${mode}), use "coop leave" first`);
        const bound = net.listen(port);
        mode = "host";
        myId = 0;
        nextId = 1;
        players.clear();
        peers.clear();
        say(`Co-op: waiting for the players on port ${bound}`);
        return bound;
    }

    function join(address, port) {
        if (mode !== "off")
            throw new Error(`already in the co-op (${mode}), use "coop leave" first`);
        hostPeer = net.connect(address, port);
        mode = "client";
        connected = false;
        say(`Co-op: connecting to ${address}:${port}`);
    }

    function leave() {
        if (mode === "off")
            return;
        if (mode === "client" && hostPeer)
            net.disconnect(hostPeer);
        else
            net.close();
        cleanup();
        say("Co-op: you have left");
    }

    function cleanup() {
        clientSync.clear();
        avatars.clear();
        for (const peer of peers.keys())
            hostSync.removePeer(peer);
        players.clear();
        peers.clear();
        net.close();
        mode = "off";
        hostPeer = 0;
        connected = false;
        myId = 0;
    }

    // --- the host

    function hostAccept(peer, m) {
        if (peers.has(peer))
            return;
        if (players.size + 1 >= MaxPlayers) {
            send(peer, { t: Types.chat, from: "host", text: "the game is full" });
            net.disconnect(peer);
            return;
        }
        const id = nextId++;
        const info = { id, name: String(m.name).slice(0, 24), instance: m.instance, peer, state: null };

        // the player that has come is told about the others, the others about him
        send(peer, {
            t: Types.welcome, v: Version, id,
            players: [{ id: 0, name: name(), instance: env.player.instance ?? "" }, ...listOf()],
        });
        for (const other of peers.keys())
            send(other, { t: Types.join, id, name: info.name, instance: info.instance });

        players.set(id, info);
        peers.set(peer, id);
        hostSync.addPeer(peer);
        say(`Co-op: ${info.name} has joined`);
        }

    function hostMessage(peer, m) {
        if (m.t === Types.hello) {
            hostAccept(peer, m);
            return;
        }
        const id = peers.get(peer);
        if (id === undefined)
            return;   // only the ones that have said hello are heard
        const info = players.get(id);

        switch (m.t) {
            case Types.player:
                // a player that has come to this world gets all the characters again
                if (!info.state || info.state.w !== m.w)
                    hostSync.resetPeer(peer);
                info.state = { x: m.x, y: m.y, z: m.z, a: m.a, hp: m.hp, w: m.w, an: m.an, ws: m.ws, wi: m.wi, ov: m.ov, ar: m.ar, rw: m.rw, in: m.in };
                avatars.update(id, info, info.state);
                for (const other of peers.keys())
                    if (other !== peer) send(other, { ...m, id });
                break;
            case Types.hit:
                if (m.f === 1) {
                    hostSync.finish(m.id);
                } else {
                    const av = avatars.list.get(id);
                    hostSync.hit(m.id, m.d, av ? av.npc : null);
                }
                break;
            case Types.need: {
                // the guest does not have these characters: they were made in the game of the host
                const e = hostSync.describe(m.ids);
                if (e.length > 0)
                    send(peer, { t: Types.spawn, e });
                break;
            }
            case Types.shot:
                // an arrow of a guest flies in the other games too
                env.shoot(...m.s);
                net.broadcast(encode(m), peer);
                break;
            case Types.chat: {
                const text = String(m.text).slice(0, MaxText);
                net.broadcast(encode({ t: Types.chat, from: info.name, text }), peer);
                env.print(`${info.name}: ${text}`);
                break;
            }
            default:
                break;
        }
    }

    function hostLost(peer) {
        const id = peers.get(peer);
        if (id === undefined)
            return;
        const info = players.get(id);
        peers.delete(peer);
        players.delete(id);
        hostSync.removePeer(peer);
        avatars.remove(id);
        net.broadcast(encode({ t: Types.leave, id }));
        say(`Co-op: ${info.name} has left`);
    }

    function centers() {
        const w = env.worldName();
        const ret = [];
        const p = env.player;
        if (p.valid)
            ret.push({ x: p.x, y: p.y, z: p.z });
        for (const info of players.values())
            if (info.state && info.state.w === w)
                ret.push({ x: info.state.x, y: info.state.y, z: info.state.z });
        return ret;
    }

    // --- a client

    function clientMessage(peer, m) {
        if (peer !== hostPeer)
            return;
        switch (m.t) {
            case Types.welcome:
                connected = true;
                myId = m.id;
                players.clear();
                for (const p of m.players)
                    if (p.id !== myId) players.set(p.id, { id: p.id, name: String(p.name), instance: String(p.instance), state: null });
                say(`Co-op: you are in the game, ${players.size} other player(s)`);
                break;
            case Types.join:
                players.set(m.id, { id: m.id, name: m.name.slice(0, 24), instance: m.instance, state: null });
                say(`Co-op: ${m.name} has joined`);
                break;
            case Types.leave: {
                const info = players.get(m.id);
                players.delete(m.id);
                avatars.remove(m.id);
                if (info) say(`Co-op: ${info.name} has left`);
                break;
            }
            case Types.player: {
                const info = players.get(m.id);
                if (!info)
                    break;
                info.state = { x: m.x, y: m.y, z: m.z, a: m.a, hp: m.hp, w: m.w, an: m.an, ws: m.ws, wi: m.wi, ov: m.ov, ar: m.ar, rw: m.rw, in: m.in };
                avatars.update(m.id, info, info.state);
                break;
            }
            case Types.npcs:
                clientSync.apply(m, env.now(), (ids) => send(hostPeer, { t: Types.need, ids }));
                break;
            case Types.spawn:
                clientSync.make(m, env.now());
                break;
            case Types.shot:
                env.shoot(...m.s);
                break;
            case Types.hurt:
                // a character of the world has hit the avatar of this player
                env.player.heal(-m.d);
                break;
            case Types.chat:
                env.print(`${String(m.from).slice(0, 24)}: ${String(m.text).slice(0, MaxText)}`);
                break;
            default:
                break;
        }
    }

    // --- events of the network and of the game

    function onConnect({ peer, incoming }) {
        if (mode === "client" && peer === hostPeer && !incoming) {
            const inst = env.player.instance ?? "PC_HERO";
            send(peer, { t: Types.hello, v: Version, name: name(), instance: inst });
        }
    }

    function onMessage({ peer, data }) {
        if (mode === "off")
            return;
        const m = decode(data);
        if (!m) {
            log("warn", `a bad message from ${peer} is dropped`);
            return;
        }
        if (mode === "host") hostMessage(peer, m); else clientMessage(peer, m);
    }

    function onDisconnect({ peer }) {
        if (mode === "host")
            hostLost(peer);
        else if (mode === "client" && peer === hostPeer) {
            cleanup();
            say("Co-op: the connection with the host is lost");
        }
    }

    function onError({ peer, error }) {
        if (mode === "client" && peer === hostPeer) {
            cleanup();
            say(`Co-op: ${error}`);
        } else {
            log("warn", error);
        }
    }

    /// The world is new: the characters of the old one are gone
    function onWorldChange() {
        clientSync.forget();
        avatars.forget();
        for (const p of players.values()) p.state = null;
        hostSync.reset();
    }

    // the shooter of the arrows of the player, see World::shootBullet
    const PlayerShot = 0xFFFFFFFE;

    /// The arrows and bolts that were shot in this game fly in the others too. A guest tells only the shots of its
    /// player: the characters of the host are shown by the host.
    function pumpShots() {
        if (!env.takeShots)
            return;
        for (const s of env.takeShots()) {
            const msg = { t: Types.shot, s: [s[0], round(s[1]), round(s[2]), round(s[3]), round(s[4]), round(s[5]), round(s[6])] };
            if (mode === "host") {
                if (peers.size > 0) net.broadcast(encode(msg));
            } else if (connected && s[7] === PlayerShot) {
                send(hostPeer, msg);
            }
        }
    }

    function tick(dt, now) {
        if (mode === "off")
            return;
        avatars.tick(dt, (id, d) => {
            // the characters of the host have hit the avatar of a guest: the guest takes the damage
            const info = players.get(id);
            if (mode === "host" && info && info.peer)
                send(info.peer, { t: Types.hurt, d });
        });

        if (now >= nextPlayerSend) {
            nextPlayerSend = now + PlayerInterval;
            const s = myState(now);
            if (s) {
                if (mode === "host") {
                    if (peers.size > 0)
                        net.broadcast(encode({ ...s, id: 0 }));
                } else if (connected) {
                    send(hostPeer, s);
                }
            }
        }

        pumpShots();

        if (mode === "host") {
            if (now >= nextNpcSend) {
                nextNpcSend = now + NpcInterval;
                hostSync.tick(now, centers(), send);
            }
        } else if (connected) {
            clientSync.tick(dt, now, (id, d, finish) => send(hostPeer, finish ? { t: Types.hit, id, f: 1 } : { t: Types.hit, id, d }));
        }
    }

    function chat(text) {
        text = String(text).slice(0, MaxText);
        if (mode === "host") {
            net.broadcast(encode({ t: Types.chat, from: name(), text }));
        } else if (mode === "client" && connected) {
            send(hostPeer, { t: Types.chat, from: name(), text });
        } else {
            return false;
        }
        env.print(`${name()}: ${text}`);
        return true;
    }

    function status() {
        if (mode === "off")
            return "Co-op is off. Use: coop host [port] or coop join <address> [port]";
        const lines = [];
        if (mode === "host")
            lines.push(`host on port ${net.port}, ${players.size} player(s)`);
        else
            lines.push(connected ? "joined the game of the host" : "connecting...");
        for (const p of players.values())
            lines.push(` - ${p.name} (${p.instance})${p.state ? ` in ${p.state.w}` : ""}`);
        if (mode === "client")
            lines.push(` characters of the host: ${clientSync.count}${clientSync.mismatch ? `, ${clientSync.mismatch} do not match` : ""}`);
        return lines.join("\n");
    }

    return {
        host, join, leave, status, chat, tick,
        setName(n) { myName = String(n).slice(0, 24); },
        onConnect, onMessage, onDisconnect, onError, onWorldChange,
        get mode() { return mode; },
        get connected() { return connected; },
        get id() { return myId; },
        get players() { return players; },
        get clientSync() { return clientSync; },
        get avatars() { return avatars; },
    };
}

module.exports = { createCoop };

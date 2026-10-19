// The characters of the world. The game of the host decides what they do, the games of the others show it:
// a character of the host is a puppet in them, which is moved by the messages of the host.
//
// The host also tells which characters exist: a character that is made in the game of the host (a monster that
// comes, a character that a script puts into the world) is made in the other games too, and one that is removed is removed.
"use strict";

const { Types, round } = require("./protocol.js");
const { Follower } = require("./smooth.js");
const Visual = require("./visual.js");

const Radius       = 3500;   // characters farther than this from all of the players are not sent
const Heartbeat    = 1500;   // a character that stands is sent at least this often
const MoveEpsilon  = 2;      // units
const TurnEpsilon  = 4;      // degrees
const StaleAfter   = 4000;   // a puppet that has not been heard of is given back to its own game
const TalkGrace    = 3000;   // ms a character is left to its own game after a talk of the player with it
const AskAgain     = 5000;   // ms before a character that has not come is asked for again
const ModsRange    = 0x80000000;   // numbers from here are of the characters that mods make, they are not the same in the games

/// The host: tells the others what the characters do
class HostSync {
    constructor(env) {
        this.env = env;
        this.known = new Map();   // peer -> Map(character -> what was sent)
        this.away = new Map();    // peer -> Set(characters that were sent and are far now)
    }

    addPeer(peer) {
        this.known.set(peer, new Map());
        this.away.set(peer, new Set());
    }

    removePeer(peer) {
        this.known.delete(peer);
        this.away.delete(peer);
    }

    /// everything is sent to a peer again, e.g. when it has come to this world
    resetPeer(peer) {
        this.known.get(peer)?.clear();
        this.away.get(peer)?.clear();
    }

    /// everything is sent again, e.g. in a new world
    reset() {
        for (const known of this.known.values())
            known.clear();
        for (const away of this.away.values())
            away.clear();
    }

    /// A character that is not in the world, or that a player of the host controls, is not there for the others
    gone(id) {
        const npc = this.env.npcs.byId(id);
        return !npc || !npc.valid || npc.isPlayer;
    }

    /// centers: places of the players, send(peer, message)
    tick(now, centers, send) {
        if (this.known.size === 0 || centers.length === 0)
            return;
        const world = this.env.worldName();
        if (!world)
            return;

        const snap = this.env.npcs.snapshot(Radius, centers);
        const seen = new Set();
        for (const s of snap) seen.add(s[0]);

        for (const [peer, known] of this.known) {
            const changed = [];
            for (const s of snap) {
                const [id, inst, x, y, z, a, hp] = s;
                const anim = s[8] ?? [], ws = s[9] ?? 0, wi = s[10] ?? "", ov = s[11] ?? [], ar = s[12] ?? "";
                const unc = s[13] ? 1 : 0, rw = s[14] ?? "";
                let old = known.get(id);
                if (!old) {
                    old = { x: NaN, y: NaN, z: NaN, a: 0, hp: -1, unc: -1, t: -Infinity, look: new Visual.Tracker() };
                    known.set(id, old);
                }
                const look = old.look.next(anim, ws, wi, ov, ar, rw, now);
                const fresh = look !== null
                    || Math.abs(old.x - x) > MoveEpsilon || Math.abs(old.y - y) > MoveEpsilon || Math.abs(old.z - z) > MoveEpsilon
                    || Math.abs(((old.a - a + 540) % 360) - 180) > TurnEpsilon
                    || old.hp !== hp || old.unc !== unc
                    || now - old.t > Heartbeat;
                if (!fresh) continue;
                old.x = x; old.y = y; old.z = z; old.a = a; old.hp = hp; old.unc = unc; old.t = now;
                changed.push([id, inst, round(x), round(y), round(z), Math.round(a), hp,
                              look ? look.an : 0, ws, wi, look ? look.ov : 0, ar, unc, rw]);
            }

            // a character that is not in the list is far away, or it is not in the world any more
            const gone = [], removed = [];
            const away = this.away.get(peer);
            for (const id of known.keys()) {
                if (seen.has(id)) continue;
                known.delete(id);
                if (this.gone(id)) {
                    removed.push(id);
                } else {
                    gone.push(id);
                    away.add(id);
                }
            }
            // a character that was far away can be removed while it is far away
            for (const id of [...away]) {
                if (seen.has(id)) {
                    away.delete(id);
                } else if (this.gone(id)) {
                    away.delete(id);
                    removed.push(id);
                }
            }
            if (changed.length === 0 && gone.length === 0 && removed.length === 0)
                continue;
            send(peer, { t: Types.npcs, w: world, e: changed, r: gone, d: removed });
        }
    }

    /// A player of the others has finished off an unconscious character
    finish(id) {
        const npc = this.env.npcs.byId(id);
        if (!npc || !npc.valid || npc.puppet || npc.dead || npc.isPlayer)
            return false;
        npc.finish();
        return true;
    }

    /// A character was hit by a player of the others: the host takes the damage, and the character answers
    /// the attacker (the avatar of the player) as it would answer the player of the host
    hit(id, damage, attacker) {
        const npc = this.env.npcs.byId(id);
        if (!npc || !npc.valid || npc.puppet || npc.dead)
            return false;
        if (attacker && attacker.valid)
            npc.hurtBy(attacker, Math.floor(damage));
        else
            npc.heal(-Math.floor(damage));
        return true;
    }

    /// The others ask for the characters that they have not got: what they need to make them
    describe(ids) {
        const ret = [];
        for (const id of ids) {
            if (id >= ModsRange || ret.length >= 64) continue;
            const npc = this.env.npcs.byId(id);
            if (!npc || !npc.valid || npc.puppet || npc.isPlayer) continue;
            ret.push([id, npc.instance, round(npc.x), round(npc.y), round(npc.z), Math.round(npc.rotation), npc.health]);
        }
        return ret;
    }
}

/// The others: show what the host tells
class ClientSync {
    constructor(env) {
        this.env = env;
        this.puppets = new Map();   // id of the character -> { npc, follow, hp, seen }
        this.asked = new Map();     // id of the character -> when it was asked for
        this.busy = new Map();      // id of the character -> until when a talk of the player with it goes on
        this.told = new Set();      // characters that the host has told about, also the ones that are given back
        this.mismatch = 0;
        this.made = 0;
    }

    /// The characters of the message are the characters of the world of this game. need(ids) asks the host for
    /// the characters that are not here.
    apply(msg, now, need) {
        const here = this.env.worldName();
        if (!here || msg.w !== here)
            return;

        const missing = [];
        for (const e of msg.e) {
            if (!Array.isArray(e) || e.length < 7) continue;
            const [id, inst, x, y, z, a, hp] = e;
            const look = { an: e[7] || undefined, ws: e[8], wi: e[9], ov: e[10] || undefined, ar: e[11], rw: e[13] };
            if (!Visual.validLook(look)) continue;

            // the player talks to this character now: its own game runs it, the host is not listened to
            if ((this.busy.get(id) ?? 0) > now) continue;

            let p = this.puppets.get(id);
            if (p && !p.npc.valid) {
                this.puppets.delete(id);
                p = undefined;
            }
            if (!p) {
                const npc = this.env.npcs.byId(id);
                // the character that is the player of this game is the one that the host has left: not a puppet
                if (npc && npc.valid && npc.isPlayer) continue;
                if (!npc || !npc.valid) {
                    // the character is not in this world, it was made in the game of the host
                    const askedAt = this.asked.get(id);
                    if (id < ModsRange && (askedAt === undefined || now - askedAt > AskAgain)) {
                        this.asked.set(id, now);
                        missing.push(id);
                    }
                    continue;
                }
                // the same number must be the same kind of character, otherwise the worlds are not the same
                if (npc.instanceId !== inst) { this.mismatch++; continue; }
                npc.setPuppet(true);
                this.told.add(id);
                p = { npc, follow: new Follower(x, y, z, a), hp, applied: -1, unc: 0, uncApplied: -1, seen: now, look: {} };
                p.follow.jump();
                npc.setPosition(x, y, z);
                npc.setRotation(a);
                this.puppets.set(id, p);
            }
            p.follow.target(x, y, z, a);
            p.hp   = hp;
            p.unc  = e[12] ? 1 : 0;
            p.seen = now;
            Visual.apply(p.npc, look, p.look);
        }
        for (const id of msg.r ?? [])
            this.release(id);
        for (const id of msg.d ?? [])
            this.remove(id);
        if (missing.length > 0)
            need(missing.slice(0, 64));
    }

    /// The host has told how to make the characters that were asked for
    make(msg, now) {
        for (const e of msg.e) {
            if (!Array.isArray(e) || e.length < 7 || typeof e[1] !== "string" || e[1].length > 64) continue;
            const [id, instance, x, y, z, a, hp] = e;
            if (!Number.isFinite(id) || id >= ModsRange || ![x, y, z, a].every(Number.isFinite)) continue;
            if (this.env.npcs.byId(id)) continue;
            const npc = this.env.npcs.spawn(instance, x, y, z, a, { puppet: true, id });
            if (!npc) continue;
            this.made++;
            this.told.add(id);
            // it is a puppet from the start, so it has a record like the others: they are given back together
            this.puppets.set(id, { npc, follow: new Follower(x, y, z, a), hp, applied: -1, unc: 0, uncApplied: -1, seen: now, look: {} });
        }
    }

    /// every frame: smooth movement, health, and the hits of this player are told to the host
    tick(dt, now, hit) {
        // a talk goes on: the character stays in the hands of this game for as long as it lasts, and a little longer
        for (const [id, until] of [...this.busy]) {
            const npc = this.env.npcs.byId(id);
            if (!npc || !npc.valid) {
                this.busy.delete(id);
            } else if (npc.talking) {
                this.busy.set(id, now + TalkGrace);
            } else if (until <= now) {
                this.busy.delete(id);
            }
        }

        for (const [id, p] of [...this.puppets]) {
            const npc = p.npc;
            if (!npc.valid) {
                this.puppets.delete(id);
                continue;
            }
            if (now - p.seen > StaleAfter) {
                this.release(id);
                continue;
            }

            // a player has started to talk to it: it needs its script and its AI for that
            if (npc.takeInteraction()) {
                this.busy.set(id, now + TalkGrace);
                this.release(id);
                continue;
            }

            if (p.hp !== p.applied) {
                npc.setHealth(p.hp);
                p.applied = p.hp;
            }
            if (p.unc !== p.uncApplied) {
                npc.setUnconscious(p.unc === 1);
                p.uncApplied = p.unc;
            }
            if (!npc.dead && p.follow.step(dt, 110, 800)) {
                npc.setPosition(p.follow.x, p.follow.y, p.follow.z);
                npc.setRotation(p.follow.a);
            }
            const damage = npc.takeDamage();
            if (damage > 0)
                hit(id, damage, false);
            if (npc.takeFinish())
                hit(id, 0, true);
        }
    }

    release(id) {
        const p = this.puppets.get(id);
        if (!p) return;
        this.puppets.delete(id);
        if (p.npc.valid) p.npc.setPuppet(false);
    }

    /// The host has removed the character from the world
    remove(id) {
        const p = this.puppets.get(id);
        this.puppets.delete(id);
        const npc = p ? p.npc : this.env.npcs.byId(id);
        // only a character that the host has told about is the character that the host has removed
        if (npc && npc.valid && (p || this.told.has(id)))
            npc.remove();
        this.told.delete(id);
    }

    clear() {
        for (const id of [...this.puppets.keys()])
            this.release(id);
        this.asked.clear();
        this.busy.clear();
        this.told.clear();
    }

    /// The world is new: the old characters are gone and are not touched
    forget() {
        this.puppets.clear();
        this.asked.clear();
        this.busy.clear();
        this.told.clear();
    }

    get count() {
        return this.puppets.size;
    }
}

module.exports = { HostSync, ClientSync, Radius };

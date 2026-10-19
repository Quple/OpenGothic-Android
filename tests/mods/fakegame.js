// A pretend game for the tests of the co-op: characters that behave like the ones of the engine
// (see common/scripting/game/game_npc.cpp and the puppets of common/world/objects/npc.cpp).
"use strict";

class FakeNpc {
    constructor(game, uid, instance, x, y, z, a, hp = 100) {
        this.game = game;
        this.uid = uid;
        this.instanceId = instance;
        this.x = x; this.y = y; this.z = z; this.rotation = a;
        this.health = hp; this.maxHealth = 100;
        this.puppet = false;
        this.dead = false;
        this.removed = false;
        this._damage = 0;
        this.name = "npc" + uid;
        this.id = uid;
        this.instance = "INST_" + instance;
        this.isPlayer = false;
        this.animName = "S_IDLE";
        this.animStart = 0;
        this.ws = 0;
        this.wi = "";
        this.ov = [];
        this.ar = "";
        this.rw = "";
        this.unconscious = false;
        this.talking = false;
        this._interact = false;
        this._finish = false;
    }
    get rangedItem() { return this.rw; }
    /// the weapons that are carried
    setEquipment(melee, ranged) { if (this.puppet) { this.wi = melee; this.rw = ranged; } }
    setUnconscious(on) { if (this.puppet) this.unconscious = on; }
    /// a player has started to talk to it
    interact() { if (this.puppet) this._interact = true; }
    takeInteraction() { const r = this._interact; this._interact = false; return r; }
    /// a player finishes it off
    finishRequest() { if (this.puppet && this.unconscious) this._finish = true; }
    takeFinish() { const r = this._finish; this._finish = false; return r; }
    finish() { this.health = 0; this.dead = true; this.unconscious = false; }
    get overlays() { return this.ov.slice(); }
    get armor() { return this.ar; }
    setOverlays(names) { if (this.puppet) this.ov = names.slice(); }
    setArmor(name) { if (this.puppet) this.ar = name; }
    get weaponState() { return this.ws; }
    get weaponItem() { return this.wi; }
    playAnim(name) { this.animName = name; this.animStart = this.game.time; }
    animation() { return [[this.animName, this.game.time - this.animStart, 0, 1]]; }
    /// as the engine does it: only a puppet plays what the owner says; an empty list gives the walking back
    setAnimation(an) {
        if (!this.puppet || !Array.isArray(an) || an.length === 0) return 0;
        this.animName = an[0][0];
        this.animStart = this.game.time - an[0][1];
        this.animCalls = (this.animCalls ?? 0) + 1;
        return an.length;
    }
    setWeapon(ws, wi) {
        if (!this.puppet) return;
        this.ws = ws;
    }
    get valid() { return !this.removed && this.game.byUid.get(this.uid) === this; }
    setPuppet(on) { this.puppet = on; }
    setPosition(x, y, z) { this.x = x; this.y = y; this.z = z; this.moves = (this.moves ?? 0) + 1; return true; }
    setRotation(a) { this.rotation = a; }
    /// as the engine does it: a puppet takes no damage by itself, the owner puts the health
    setHealth(hp) {
        this.health = Math.max(0, Math.min(this.maxHealth, hp));
        if (this.health === 0) this.dead = true;
    }
    kill() { this.setHealth(0); }
    heal(n) {
        if (n < 0 && this.puppet) { this._damage += -n; return; }
        this.setHealth(this.health + n);
    }
    takeDamage() { const d = this._damage; this._damage = 0; return d; }
    /// as the engine does it: the character answers the attacker
    hurtBy(attacker, d) {
        if (this.puppet || this.dead) return;
        this.lastAttacker = attacker;
        this.setHealth(this.health - d);
    }
    remove() { this.removed = true; this.game.byUid.delete(this.uid); }
}

class FakeGame {
    constructor(worldName) {
        this.worldName = worldName;
        this.time = 0;
        this.byUid = new Map();
        this.modUid = 0x80000000;
        this.spawned = [];
        this.player = {
            valid: true, x: 0, y: 0, z: 0, rotation: 0, health: 100, maxHealth: 100,
            name: "Hero", instance: "PC_HERO", isPlayer: true,
            animName: "S_IDLE", animStart: 0, weaponState: 0, weaponItem: "",
            animation: () => [[this.player.animName, this.time - this.player.animStart, 0, 1]],
            overlays: [], armor: "", rangedItem: "",
            heal: (n) => { this.player.health = Math.max(0, Math.min(100, this.player.health + n)); },
        };
        this.pendingShots = [];
        this.replayed = [];
        this.made = [];
        const game = this;
        this.npcs = {
            byId: (id) => game.byUid.get(id) ?? null,
            spawn: (instance, x, y, z, a, opts) => {
                if (instance === "NO_SUCH") return null;
                const uid = opts && opts.id ? opts.id : game.modUid++;
                if (game.byUid.has(uid)) return null;
                const num = Number(String(instance).replace("INST_", ""));
                const n = new FakeNpc(game, uid, Number.isFinite(num) ? num : 1, x, y, z, a);
                n.instance = instance;
                n.spawnedAsPuppet = !!(opts && opts.puppet);
                n.remotePlayer = !!(opts && opts.player);
                if (n.spawnedAsPuppet) n.puppet = true;
                game.byUid.set(n.uid, n);
                if (opts && opts.id) game.made.push(n); else game.spawned.push(n);
                return n;
            },
            snapshot: (radius, centers) => {
                const out = [];
                for (const n of game.byUid.values()) {
                    if (n.puppet || n.uid >= 0x80000000 || n.isPlayer) continue;
                    const near = (centers ?? []).some(c => Math.hypot(c.x - n.x, c.y - n.y, c.z - n.z) <= radius);
                    if (near) out.push([n.uid, n.instanceId, n.x, n.y, n.z, n.rotation, n.health, n.maxHealth, n.animation(), n.ws, n.wi, n.overlays, n.ar, n.unconscious ? 1 : 0, n.rw]);
                }
                return out;
            },
        };
    }

    /// an arrow of this game, as the engine tells it
    fire(item, x, y, z, vx, vy, vz, origin) {
        this.pendingShots.push([item, x, y, z, vx, vy, vz, origin]);
    }
    takeShots() { const s = this.pendingShots; this.pendingShots = []; return s; }
    shoot(...a) { this.replayed.push(a); return true; }

    add(uid, instance, x, y, z, a = 0, hp = 100) {
        const n = new FakeNpc(this, uid, instance, x, y, z, a, hp);
        this.byUid.set(uid, n);
        return n;
    }

    /// the AI of the characters that nobody drives from outside
    step(dt) {
        this.time += dt;
        for (const n of this.byUid.values()) {
            if (n.puppet || n.dead || n.uid >= 0x80000000 || !n.walk) continue;
            n.x += n.walk.x * dt / 1000;
            n.z += n.walk.z * dt / 1000;
            if (n.animName !== "S_RUNL") n.playAnim("S_RUNL");
        }
    }

    changeWorld(name) {
        this.worldName = name;
        this.byUid.clear();
    }
}

module.exports = { FakeGame, FakeNpc };

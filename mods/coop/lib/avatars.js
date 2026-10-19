// The other players: every one of them is a character in the world of this game, which the mod moves.
"use strict";

const { Follower } = require("./smooth.js");
const Visual = require("./visual.js");

class Avatars {
    /// env: { npcs, worldName(), log() }
    constructor(env) {
        this.env = env;
        this.list = new Map();   // number of the player -> { npc, follow, info, world }
    }

    has(id) {
        return this.list.has(id);
    }

    /// state: {x, y, z, a, w} of the player, info: {name, instance}
    update(id, info, state) {
        const here = this.env.worldName();
        let av = this.list.get(id);

        // players in other worlds are not seen
        if (!here || state.w !== here) {
            if (av) this.remove(id);
            return;
        }
        if (av && !av.npc.valid) {
            this.list.delete(id);
            av = undefined;
        }
        // the player has become another character: the avatar looks like the new one
        if (av && state.in !== undefined && av.instance !== undefined && state.in !== av.instance) {
            this.remove(id);
            av = undefined;
        }
        if (!av) {
            const name = this.env.avatarInstance(state.in || info.instance);
            // a puppet from the start: its script and its AI never run
            const npc = this.env.npcs.spawn(name, state.x, state.y, state.z, state.a, { puppet: true, player: true });
            if (!npc) {
                this.env.log("warn", `cannot make the character of ${info.name}, there is no instance ${name}`);
                return;
            }
            av = { npc, follow: new Follower(state.x, state.y, state.z, state.a), info, look: {}, instance: state.in };
            this.list.set(id, av);
        }
        av.follow.target(state.x, state.y, state.z, state.a);
        av.hp = state.hp;
        Visual.apply(av.npc, state, av.look);
    }

    /// every frame
    /// hurt(player, damage) is told what the characters of the world have done to the avatars
    tick(dt, hurt) {
        for (const [id, av] of this.list) {
            if (!av.npc.valid) {
                this.list.delete(id);
                continue;
            }
            if (av.follow.step(dt, 80, 700)) {
                av.npc.setPosition(av.follow.x, av.follow.y, av.follow.z);
                av.npc.setRotation(av.follow.a);
            }
            // the avatar takes no damage: the game of its player does, so it is told what has hit it
            const damage = av.npc.takeDamage();
            if (damage > 0 && hurt) hurt(id, damage);
            if (av.hp === 0 && !av.npc.dead)
                av.npc.kill();
        }
    }

    positions(world) {
        const ret = [];
        for (const av of this.list.values())
            if (av.npc.valid) ret.push({ x: av.follow.tx, y: av.follow.ty, z: av.follow.tz });
        return ret;
    }

    remove(id) {
        const av = this.list.get(id);
        if (!av) return;
        this.list.delete(id);
        if (av.npc.valid) av.npc.remove();
    }

    clear() {
        for (const id of [...this.list.keys()])
            this.remove(id);
    }

    /// The world is new: the old characters are not there any more and cannot be removed
    forget() {
        this.list.clear();
    }
}

module.exports = { Avatars };

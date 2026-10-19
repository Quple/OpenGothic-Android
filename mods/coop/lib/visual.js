// What a character looks like to the others: the animation that it plays, the weapon in its hand, the overlays
// (mds files of the animations of its talents) and the armor.
// The animation is a list of layers [sequence, ms since it has started, comb, body state]. The others start the same
// sequences at the same point, so a swing of a sword or a step of a walk is in the same phase in every game.
"use strict";

const Resend = 1500;   // ms, how often the time of an animation is told again, so the games do not drift apart

/// The animation tells the same thing while the sequences are the same: only the time of them goes on
function signature(anim, ws, wi, ov, ar, rw) {
    let s = ws + ":" + wi + ":" + rw + ":" + ar + ":" + ov.join(",");
    for (const l of anim)
        s += "|" + l[0] + "," + l[2] + "," + l[3];
    return s;
}

function validAnim(an) {
    if (!Array.isArray(an) || an.length > 8)
        return false;
    for (const l of an) {
        if (!Array.isArray(l) || l.length < 4 || typeof l[0] !== "string" || l[0].length > 64)
            return false;
        if (!Number.isFinite(l[1]) || !Number.isFinite(l[2]) || !Number.isFinite(l[3]))
            return false;
    }
    return true;
}

function validNames(ov) {
    if (!Array.isArray(ov) || ov.length > 32)
        return false;
    return ov.every(n => typeof n === "string" && n.length <= 64);
}

/// The fields of a message that tell the look: an (animation), ws (weapon drawn), wi, rw (weapons carried),
/// ov (overlays), ar (armor), in (instance of a player)
function validLook(m) {
    if (m.an !== undefined && m.an !== 0 && !validAnim(m.an))
        return false;
    if (m.ws !== undefined && !Number.isFinite(m.ws))
        return false;
    if (m.wi !== undefined && (typeof m.wi !== "string" || m.wi.length > 64))
        return false;
    if (m.ov !== undefined && m.ov !== 0 && !validNames(m.ov))
        return false;
    if (m.ar !== undefined && (typeof m.ar !== "string" || m.ar.length > 64))
        return false;
    if (m.rw !== undefined && (typeof m.rw !== "string" || m.rw.length > 64))
        return false;
    if (m.in !== undefined && (typeof m.in !== "string" || m.in.length > 64))
        return false;
    return true;
}

/// Tells when the look of one character has to be sent
class Tracker {
    constructor() {
        this.sig = null;
        this.sent = -Infinity;
    }

    /// Returns {an, ws, wi, ov, ar, rw} to send, or null if there is nothing new: the look has changed or it is time
    /// to tell the time of the animation again
    next(anim, ws, wi, ov, ar, rw, now) {
        const sig = signature(anim, ws, wi, ov, ar, rw);
        const changed = sig !== this.sig;
        const due = now - this.sent >= Resend;
        if (!changed && !due)
            return null;
        this.sig = sig;
        this.sent = now;
        return { an: anim.map(l => [l[0], Math.round(l[1]), l[2], l[3]]), ws, wi, ov, ar, rw };
    }
}

/// The look of a character that is moved by messages. The animation is started only when it has changed or is
/// told again: between the messages the sequences go on by themselves.
function apply(npc, look, applied) {
    // the weapons that are carried are on the body also when they are not drawn
    if (look.wi !== undefined || look.rw !== undefined) {
        const eq = (look.wi ?? "") + "|" + (look.rw ?? "");
        if (eq !== applied.eq) {
            npc.setEquipment(look.wi ?? "", look.rw ?? "");
            applied.eq = eq;
        }
    }
    if (look.ws !== undefined && (look.ws !== applied.ws || look.wi !== applied.wi)) {
        npc.setWeapon(look.ws, look.wi ?? "");
        applied.ws = look.ws;
        applied.wi = look.wi;
    }
    if (look.ar !== undefined && look.ar !== applied.ar) {
        npc.setArmor(look.ar);
        applied.ar = look.ar;
    }
    if (look.ov && validNames(look.ov)) {
        const key = look.ov.join(",");
        if (key !== applied.ov) {
            npc.setOverlays(look.ov);
            applied.ov = key;
        }
    }
    if (look.an && validAnim(look.an))
        npc.setAnimation(look.an);
}

module.exports = { signature, validAnim, validNames, validLook, Tracker, apply, Resend };

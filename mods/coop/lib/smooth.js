// A character that is moved by messages moves in jumps. These functions make the movement smooth.
"use strict";

/// A place that follows its target: the farther it is, the faster it goes. Far away it jumps.
class Follower {
    constructor(x, y, z, a) {
        this.x = x; this.y = y; this.z = z; this.a = a;
        this.tx = x; this.ty = y; this.tz = z; this.ta = a;
    }

    target(x, y, z, a) {
        this.tx = x; this.ty = y; this.tz = z; this.ta = a;
    }

    jump() {
        this.x = this.tx; this.y = this.ty; this.z = this.tz; this.a = this.ta;
    }

    distance() {
        return Math.hypot(this.tx - this.x, this.ty - this.y, this.tz - this.z);
    }

    /// dt in ms, tau is how long the character needs to be near the target. Returns true if it has moved.
    step(dt, tau = 90, jumpAt = 900) {
        if (this.distance() > jumpAt) {
            this.jump();
            return true;
        }
        const k = 1 - Math.exp(-Math.max(0, dt) / tau);
        const ox = this.x, oy = this.y, oz = this.z, oa = this.a;
        this.x += (this.tx - this.x) * k;
        this.y += (this.ty - this.y) * k;
        this.z += (this.tz - this.z) * k;
        this.a += angleDiff(this.a, this.ta) * k;

        // the last bit is not worth the animation of walking
        if (Math.hypot(this.tx - this.x, this.ty - this.y, this.tz - this.z) < 0.5) {
            this.x = this.tx; this.y = this.ty; this.z = this.tz;
        }
        return ox !== this.x || oy !== this.y || oz !== this.z || oa !== this.a;
    }
}

/// The shortest turn from a to b in degrees, from -180 to 180
function angleDiff(a, b) {
    let d = (b - a) % 360;
    if (d > 180) d -= 360;
    if (d < -180) d += 360;
    return d;
}

module.exports = { Follower, angleDiff };

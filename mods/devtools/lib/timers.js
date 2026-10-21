// Things that are done a little later (the second step of a test that needs the game to run for some seconds). The game gives the time
// with every tick (events.on("tick")), the clock is the argument, so the tests can use their own.
"use strict";

function createTimers(clock = () => Date.now()) {
    const waiting = [];
    let now = clock();
    return {
        /// fn is called at the first tick after `ms` milliseconds
        later(ms, fn) {
            waiting.push({ at: now + ms, fn });
        },
        /// the time that has passed while the game was running (ms), without the time of the menu: the world is paused then and the commands of the
        /// queues that a test waits for do not run
        advance(dt) {
            now += dt;
            this.tick(now);
        },
        /// called with the time (ms) at every tick
        tick(time) {
            now = time;
            for (let i = 0; i < waiting.length; ) {
                if (waiting[i].at <= time) {
                    const { fn } = waiting.splice(i, 1)[0];
                    try { fn(); } catch (e) { console.error("timer: " + e); }
                } else {
                    ++i;
                }
            }
        },
        pending: () => waiting.length,
    };
}

module.exports = { createTimers };

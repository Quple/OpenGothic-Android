// Example of a mod. Put the folder "example" into mods/ (or zip it) and press F10 in the game.
// Every mod has a world of its own: globals of one mod are not seen by the others.

const util = require("./lib/util.js");

console.log(`${GothicJS.mod.name} ${GothicJS.mod.version} runs in OpenGothic ${GothicJS.version} (${GothicJS.game}, ${GothicJS.platform})`);

// --- storage: values stay between the games
const runs = storage.get("runs", 0) + 1;
storage.set("runs", runs);
console.log(`This mod was started ${runs} time(s)`);

// --- events
events.on("ready", () => {
    console.log("ready: the script has been run");
});

events.on("world:change", ({ from, to }) => {
    console.log(`The world changed: ${from ?? "none"} -> ${to ?? "none"}`);
    if (to) screen.print(`You are in ${to}`);
});

events.on("player:damage", ({ damage, health, maxHealth }) => {
    console.warn(`Ouch! -${damage}, ${health}/${maxHealth}`);
});

events.on("player:death", () => console.error("The player is dead"));

events.on("key:down", ({ key }) => {
    if (key === "F9") screen.print("Hello from the example mod!");
});

events.on("shutdown", () => {
    console.log("bye, the mod is stopping");
});

// --- commands of the console (F10, tab Console): hello Ernest
console.registerCommand("hello", ({ args }) => {
    console.log(`Hello, ${args[0] ?? "traveller"}!`);
}, "hello [name] - greets");

console.registerCommand("status", () => {
    if (!player.valid) return console.warn("There is no game yet");
    const p = player.position;
    console.log(`${player.name}: level ${player.level}, ${player.health}/${player.maxHealth} HP, ` +
                `at ${util.round(p.x)} ${util.round(p.y)} ${util.round(p.z)} in ${world.name()}`);
}, "status - the player");

console.registerCommand("heal", () => {
    player.heal(player.maxHealth);
    console.log("Healed");
}, "heal - restores the health");

console.registerCommand("give", ({ args }) => {
    // give ITFO_APPLE 3     (the names of the instances differ in Gothic 1 and 2)
    const ok = player.addItem(args[0] ?? "ITFO_APPLE", Number(args[1] ?? 1));
    console.log(ok ? "Done" : "There is no such item");
}, "give <instance> [count] - adds items");

console.registerCommand("near", ({ args }) => {
    const list = npcs.nearby(Number(args[0] ?? 1500));
    console.log(`${list.length} character(s) nearby`);
    for (const n of list.slice(0, 8))
        console.log(` - ${n.name} (${n.instance}) ${n.health}/${n.maxHealth}${n.dead ? " dead" : ""}`);
}, "near [radius] - characters around the player");

console.registerCommand("quest", () => {
    quests.create("EXAMPLE", "Example of a quest", 0);
    quests.addEntry("EXAMPLE", "The example mod has added this entry.");
    quests.setStatus("EXAMPLE", quests.status.RUNNING);
    console.log("Look into the journal");
}, "quest - adds a quest to the journal");

// --- timers: a message every minute
setInterval(() => {
    if (player.valid && player.health > 0 && player.health < player.maxHealth * 0.25)
        screen.print("You are badly hurt, drink a potion!");
}, 60 * 1000);

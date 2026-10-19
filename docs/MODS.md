# Mods in JavaScript

OpenGothic runs mods written in JavaScript ([QuickJS-ng](https://github.com/quickjs-ng/quickjs), ES2023, modules and `async`/`await` work).
Every mod runs in a **context of its own**: it has its own global objects, so mods cannot see or break each other. A mod can be started, stopped and reset in the game without restarting it.

The API is made to be close to GothicJS, but it is a separate implementation.

## Installing

| Platform | Where |
|---|---|
| Desktop | a folder or a `.zip` file in `mods/` in the working directory of the game (it is created at the first start). All mods found there start with the game, unless they are switched off in the menu of the mods. |
| Android | *Mods* in the launcher: **Add a mod (.zip)**, a switch chooses the mods that start with the game. The files are in `Android/data/<app>/files/mods`. |

`F10` in the game opens the **menu of the mods** (Android: *settings → MODS*):

* **Mods**: *Start/Stop*, *Reset* (the context is created again, the files are read again), *Auto* (the mod starts with the game), *Rescan*.
* **Console**: the output of the mods and a line of commands. `help` lists the commands; `mods`, `start <id>`, `stop <id>`, `reset <id>`, `enable <id>`, `disable <id>`, `eval <id> <code>`, `clear` are built in, mods add their own. `mod.command` calls the command of one mod.

Keys: `Tab` changes the tab, `Up/Down` the mod, `Enter` starts or stops it, `R` resets it, `E` switches *Auto*, `Esc` closes the menu.

## A mod

A mod is a folder, or a zip file with the same content (`manifest.json` may be in the root of the archive or in its only folder):

```
my-mod/
  manifest.json
  index.js
  lib/helpers.js
```

```json
{
  "id": "my-mod",
  "name": "My mod",
  "version": "1.0.0",
  "author": "Me",
  "description": "What it does",
  "main": "index.js",
  "type": "script",
  "apiVersion": 1,
  "game": ["G1", "G2"]
}
```

* `id` - letters, digits, `_`, `-`, `.`. Required, unique.
* `main` - the first file. `type` is `"script"` (default) or `"module"` (an ES module, `import "./file.mjs"` works).
* `game` - the mod starts only in these games. Without it the mod is for all of them.
* `apiVersion` - the mod is not started by an older host.

Files are loaded with `require("./lib/helpers.js")` (CommonJS, `.js`/`.json`, `index.js` of a folder) or with `import`. **Paths cannot leave the mod.**

A script that works for longer than 400 ms in one call (or 4 s at the start) is stopped, and the mod gets an error. The memory of all the mods is limited too.

## Globals

### `GothicJS`
`GothicJS.version`, `GothicJS.api`, `GothicJS.game` (`"G1"`/`"G2"`), `GothicJS.platform` (`"android"`, `"windows"`, `"linux"`, `"macos"`), `GothicJS.mod` (`id`, `name`, `version`, `author`).

### `console`
`log`, `info`, `warn`, `error`, `debug` - print to the console of the game.
`console.registerCommand(name, handler, help)` - the handler gets `{args: string[], raw: string}`. `console.unregisterCommand(name)`.

### `events`
`events.on(name, fn)` → id, `events.once(name, fn)`, `events.off(name[, fn|id])`, `events.emit(name, data)` (for the events of the mod itself).

| Event | When | Argument |
|---|---|---|
| `ready` | the script has run | - |
| `shutdown` | the mod stops or is reset | - |
| `tick` | every frame | `{dt, time}` (ms) |
| `world:change` | another world | `{from, to}` |
| `player:damage` | the health of the player fell | `{damage, health, maxHealth}` |
| `player:death` | the player died | - |
| `key:down`, `key:up` | the keyboard | `{key}`, like `"A"`, `"F1"`, `"Space"`, `"Return"`, `"Up"` |

### Timers
`setTimeout(fn, ms, ...args)`, `setInterval(fn, ms, ...args)`, `clearTimeout(id)`, `clearInterval(id)`. Promises and `async` functions work.

### `storage`
Values of a mod that stay between the games (JSON, saved when the mod stops, every 30 s and with `save()`).
`storage.get(key, default)`, `set(key, value)`, `has(key)`, `remove(key)`, `keys()`, `clear()`, `save()`.

### `player` and characters
`player` is a character that always follows the player. `npcs.find()` and `npcs.nearby()` return objects of the same kind. A character object keeps only the number of the character, so it can be stored; `valid` tells whether it is still there.

Properties: `valid`, `id`, `name`, `instance`, `instanceId` (the number of the instance), `level`, `health`, `maxHealth`, `mana`, `maxMana`, `strength`, `dexterity`, `experience`, `guild`, `dead`, `isPlayer`, `x`, `y`, `z`, `position` (`{x, y, z}`), `rotation` (degrees), `puppet`, `weaponState` (0 none, 1 fists, 2 one-handed, 3 two-handed, 4 bow, 5 crossbow, 6 magic), `weaponItem` (the instance of the melee weapon, `""` if none), `rangedItem` (the instance of the ranged weapon), `overlays` (names of the mds files of the animations that the character has), `armor` (the instance of the armor, `""` if none), `unconscious`, `talking` (in a talk with a player).

Methods: `setHealth(n)`, `heal(n)` (negative hurts), `setMana(n)`, `kill()`, `setPosition(x, y, z)`, `setRotation(degrees)`, `teleport(waypoint)`, `addItem(instance, count)`, `removeItem(instance, count)`, `itemCount(instance)`, `hasItem(instance)`, `remove()` (takes the character out of the world, not the player), `animation()` (what the character plays: `[[sequence, ms since it has started, comb, body state], ...]`). The names of instances are the names of the scripts of the game (`ITFO_APPLE` in Gothic 2, `ITFOAPPLE` in Gothic 1).

`id` is the number of the character in the world. It is the same in every game that has loaded the same world, it does not change when other characters are removed, and the characters that mods add (`npcs.spawn`) have numbers of their own, so they do not shift the others. A mod can send it to another game. In another world the number is another character, so forget the characters on `world:change`.

**Puppets.** `setPuppet(true)` gives the character to the mod: its script, its AI and its physics stop, the mod moves it with `setPosition` and `setRotation`, and the animation of walking follows the movement. It is what the co-op uses for the characters of the other players and for the characters of the host. A puppet does not lose health when somebody hits it: `takeDamage()` returns the damage since the last call, so the mod can tell the one who decides. `setHealth(n)` and `kill()` do change the health of a puppet. `setPuppet(false)` gives the character back to the game.

A puppet can play what the owner of the character plays: `setAnimation(layers)` takes what `animation()` has returned in the other game, starts the same sequences at the same point of them (the layers that play already are left alone if their time is near) and returns how many of the sequences this character knows; an empty array gives the animation back to the walking by the movement. `setWeapon(weaponState, weaponItem)` puts the weapon in the hand of the puppet, `setOverlays(names)` gives it the overlays (the others are taken off), `setArmor(instance)` dresses it, `setEquipment(melee, ranged)` gives it the weapons that its owner carries (also the ones that are not drawn), `setUnconscious(on)` makes it unconscious or not.

**Talks and finishing.** A talk needs the script and the AI of the character, so when a player starts to talk to a puppet, `takeInteraction()` returns `true` once: the mod should give the character back (`setPuppet(false)`) and leave it to the game until the talk is over (`talking`). When a player finishes off an unconscious puppet, `takeFinish()` returns `true` once, and the owner of the character does it with `finish()`.

### `npcs`
`npcs.find(instance)` → a character or `null`, `npcs.nearby(radius = 2000)` → an array, the nearest first, `npcs.count()`, `npcs.byId(id)` → a character or `null`, `npcs.spawn(instance, x, y, z, rotation, options)` → a new character or `null` (`options.puppet`: a puppet from the start, its script of insert and its AI never run; `options.id`: the number of the character, to make the one that exists in another game, `null` if the number is in the world already), `npcs.snapshot(radius, centers)` → the state of the characters near the centers (an array of `{x, y, z}`, the player by default): `[id, instanceId, x, y, z, rotation, health, maxHealth, animation, weaponState, weaponItem, overlays, armor, unconscious, rangedItem]` for each, without the player and the puppets.

### `net`
Connections with the games of other players (TCP). Nothing blocks, the answers come as events. A message is a text: a string is sent as it is, other values as JSON. Messages are whole (a big one, up to 4 MB, is not cut in pieces) and keep their order. Every mod has connections of its own, they are closed when the mod stops. On Android the game needs the network, the permission is in the manifest.

`net.listen(port)` → the port (0 takes any free one), throws if the port is busy. `net.connect(host, port)` → the number of the peer, the answer is `net:connect` or `net:error`. `net.send(peer, data)` → `false` if there is no such peer, `net.broadcast(data, exceptPeer)`, `net.disconnect(peer)`, `net.close()`, `net.address(peer)`. `net.listening`, `net.port`, `net.peers` (numbers of the connected peers).

| Event | Argument |
|---|---|
| `net:connect` | `{peer, address, incoming}` (`incoming` is true for a peer that has connected to this game) |
| `net:message` | `{peer, data}` |
| `net:disconnect` | `{peer, reason}` |
| `net:error` | `{peer, error}`, the connection could not be made |

A mod can talk to anybody that it connects to, so it should not take the commands of a stranger without checking them, see `mods/coop/lib/protocol.js`.

### `world`
`world.name()`, `world.time()` → `{day, hour, minute}`, `world.setTime(hour, minute)`, `world.waypoint(name)` → `{x, y, z}` or `null`. `world.takeShots()` → the arrows and bolts that were shot since the last call (the first call starts to watch): `[[item, x, y, z, vx, vy, vz, shooter], ...]`, where `shooter` is the number of the character, `0xFFFFFFFE` for the player. `world.shoot(item, x, y, z, vx, vy, vz)` makes an arrow or a bolt fly as it was shot in another game: it makes no damage here, the one who has shot it takes care of that (spells are not shot this way).

### `quests`
The journal. `quests.create(id, title, section)` (section 0 mission, 1 note), `quests.addEntry(id, text)`, `quests.setStatus(id, status)` with `quests.status.RUNNING | SUCCESS | FAILED | OBSOLETE`.

### `screen`
`screen.print(text)` - a message on the screen.

### `input`
`input.isDown(key)`.

## Writing a mod

1. Copy `mods/example` and change `manifest.json`.
2. Press `F10`, tab *Mods*, *Reset* after every change of the files. The errors are in the *Console* tab.
3. To test the mods without the game, see `tests/mods/README.md`.

The files of the system are split by what they do:

| File | |
|---|---|
| `common/scripting/modhost.*` | the list of the mods, start, stop, reset, events and timers |
| `common/scripting/modsource.cpp` | folders and zip files, `manifest.json` |
| `common/scripting/modconsole.cpp` | the commands of the console |
| `common/scripting/api/api_*.cpp` | one file for every object that every mod has: `GothicJS`, `console`, `events`, timers, `storage`, `require` |
| `common/scripting/game/game_*.cpp` | one file for every object of the game: characters, `player`, `npcs`, `world`, `quests`, `screen`, `input`, the events of the game, the setup |
| `common/ui/modmenu.*` | the menu of F10 |
| `android/java/.../Mod*.java` | the mods in the launcher |

A new object of the game is a new `game_*.cpp` with an `installXxx(JSContext*)` function, added to `game_setup.cpp`.

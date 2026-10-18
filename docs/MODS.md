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

Properties: `valid`, `id`, `name`, `instance`, `level`, `health`, `maxHealth`, `mana`, `maxMana`, `strength`, `dexterity`, `experience`, `guild`, `dead`, `isPlayer`, `x`, `y`, `z`, `position` (`{x, y, z}`).

Methods: `setHealth(n)`, `heal(n)` (negative hurts), `setMana(n)`, `kill()`, `setPosition(x, y, z)`, `teleport(waypoint)`, `addItem(instance, count)`, `removeItem(instance, count)`, `itemCount(instance)`, `hasItem(instance)`. The names of instances are the names of the scripts of the game (`ITFO_APPLE` in Gothic 2, `ITFOAPPLE` in Gothic 1).

### `npcs`
`npcs.find(instance)` → a character or `null`, `npcs.nearby(radius = 2000)` → an array, the nearest first, `npcs.count()`.

### `world`
`world.name()`, `world.time()` → `{day, hour, minute}`, `world.setTime(hour, minute)`, `world.waypoint(name)` → `{x, y, z}` or `null`.

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

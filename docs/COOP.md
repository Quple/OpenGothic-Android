# Co-op

The mod `mods/coop` lets friends play one world together. It is a mod like any other (JavaScript, see MODS.md); the engine only gives it the network (`net`) and the control of characters (puppets, `npcs.spawn`, `npcs.snapshot`).

## How to play

1. Both players have the same game (Gothic 1 or Gothic 2, the same version) and start a **new game**, or load the same save. Both must be in the same world.
2. The first player opens the console (F10, tab Console) and types `coop host`. The game waits on port 7777 (`coop host 9000` takes another port).
3. The other players type `coop join <address of the host>`, for example `coop join 192.168.1.20`.
   - In one network (home, Hamachi, Radmin VPN, ZeroTier) the address is the address of the host in it.
   - Over the Internet the router of the host has to forward the port 7777 (TCP) to the computer of the host.
4. The players see each other. Text for everybody: `coop say hello`.

| Command | |
|---|---|
| `coop host [port]` | be the host |
| `coop join <address> [port]` | join a host |
| `coop leave` | leave, a host ends the game for everybody |
| `coop status` | who is in the game |
| `coop say <text>` | a message for everybody |
| `coop name <name>` | your name (it stays) |
| `coop avatar [instance]` | the instance that the other players look like in your game (by default their own character) |

On Android the commands are typed in the console of the menu of the mods (the button in the settings of the touch UI).

## What is shared

- **The characters of the world.** The game of the host decides what they do: where they go, whether they fight, whether they live. The games of the others show it: a character of the host is a *puppet* in them (its script and AI are off), it plays the animation that it plays in the game of the host, **at the same point of it** (a step of a walk, a swing of a sword, a fall), holds the same weapon, wears the same armor, has the same overlays (animations of the talents), and its health is the health of the host. **A character that appears in the game of the host appears in the others, and one that is removed is removed:** a guest that sees a character that it has not got asks the host for it, and the host tells how to make it. Only the characters near some player are sent (3500 units), the others live their own life in each game.
- **The players.** Everybody sees the others as characters of the world, with their animations, weapons, armor and overlays. They are puppets from the first moment, with no script and no AI. A player in another world is not seen.
- **Arrows and bolts.** A shot flies in all the games: the games of the others show it (it makes no damage there, the shooter takes care of it). Spells are not shot this way yet.
- **Hits.** When a guest hits a character, the damage goes to the host, who takes it. A guest can also finish off an unconscious character (the finishing blow goes to the host). A character that dies in the game of the host dies in the others, and one that is unconscious in the game of the host is unconscious in the others.
- **The players are noticed.** The characters of the host see the avatars of the guests like the player of the host: a monster attacks a guest that comes near, a character answers a hit of a guest, and the damage that the characters of the host make to the avatar of a guest is taken by the game of the guest (its player loses health). The characters that are near a guest are alive on the host (their AI runs), also when they are far from the player of the host.
- **Weapons.** The weapons that a character carries are seen also when they are not drawn (the sword on the belt, the bow on the back), and the one that is drawn is in its hand.
- **Talks.** A character is run by the host, so it cannot talk in the game of a guest, because a talk needs its script. When a guest starts to talk to a character, the character is given to the game of the guest for as long as the talk lasts and for three seconds more; the host is not listened to for it in that time. What the talk does (quests, items, the attitude of the character) is only in the game of the guest. The same goes for a talk of the host: a guest sees the character stand and answer, and can still talk to it.
- **A character that the host controls** (the host has become it, e.g. with the key O of the debug mode) disappears in the games of the others, and comes back when the host is another character.

## What is not shared yet

Items on the ground, chests, doors, dialogs and quests are of each game alone; the host's world is what everybody sees only for the characters. An animation is shown only if the sequence is known to the character of the game that shows it (that is why the overlays are sent too); otherwise the character walks by the movement. Spells are not shown in the other games. The state of the game is not synchronized, so the players should not do things in the world on their own (kill a character in a quest of the story) that the other game would not know about.

## How it works

- `lib/protocol.js` - the messages (JSON, one line each). Every message that comes is checked; a bad one is dropped.
- `lib/coop.js` - the session: the host accepts the guests, tells them who is in the game and relays the states of the players; a guest sends its own state 20 times a second.
- `lib/visual.js` - the animation and the weapon: they are sent when they change, and the time of an animation again every 1.5 s, so the games do not drift apart.
- `lib/npcsync.js` - the host looks at the characters near the players 10 times a second (`npcs.snapshot`) and sends the ones that have changed; a guest takes them as puppets (`npcs.byId`, `setPuppet`) and moves them smoothly.
- `lib/avatars.js` - the characters of the other players (`npcs.spawn` with `puppet: true`).
- The messages `need` and `spawn` make the characters that the host has made, `d` in the message of the characters removes the ones that the host has removed, and `shot` tells the arrows.
- `lib/smooth.js` - the smooth movement.

A character is told by its number in the world (`npc.id`), which is the same in every game that has loaded the same world. The instance of the character is sent with it, and a guest does not take a character whose instance is different: the worlds are not the same (`coop status` counts such characters).

## The tests

`tests/mods/coop_test.cpp` runs a host and a guest in one process over the network of the computer, with a pretended game (`tests/mods/fakegame.js`). See `tests/mods/README.md`.

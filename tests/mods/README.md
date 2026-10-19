# Tests of the JavaScript mods

The host of the mods (common/scripting/modhost.cpp, the network in common/scripting/net) does not depend on the game,
so it is tested alone. Five programs:

| Test | What it checks |
|---|---|
| `modhost_test.cpp` | folders and zip files, manifests, events, timers, storage, commands, failures of mods |
| `net_test.cpp` | `net`: two mods talk over the loopback, a big message, a busy port, a refused connection |
| `builtin_test.cpp` | mods that come with the program, replaced by a mod of the same id in a folder, `net.localAddresses` |
| `coop_test.cpp` | the co-op mod (mods/coop): a host and a guest, the characters of the host in the game of the guest, the players see each other, hits, deaths, a new world, leaving, bad messages |
| `devtools_test.cpp` | the developer tools (mods/devtools): the tab Debug on a pretended `ui` and a pretended `debug` (`fakedebug.js`): the log and its filter, calling a function of the scripts, variables, teleports, triggers, spawns, the report for Ikarus |

`coop_test` also draws the tab of the menu on a pretended `ui` (`fakeui.js`) and presses its buttons: a game is hosted and joined that way.

`coop_test` pretends the game with `fakegame.js`, which behaves like the characters of the engine (puppets take no damage,
the owner of a puppet sets its health). The engine itself (`Npc::setPuppet`, `npcs.snapshot`, ...) needs the data of the game
and is tried in the game.

In the MSYS2 MinGW64 shell, from the root of the repository:

```bash
Q=lib/quickjs; OUT=/tmp/modtest; mkdir -p $OUT
for f in dtoa libregexp libunicode quickjs; do gcc -c -O1 -D_GNU_SOURCE -DQUICKJS_NG_BUILD -I$Q $Q/$f.c -o $OUT/$f.o; done
gcc -c -O1 -Ilib/miniz lib/miniz/miniz.c -o $OUT/miniz.o
SRC="common/scripting/mod*.cpp common/scripting/api/*.cpp common/scripting/net/netlink.cpp"
for t in modhost_test net_test builtin_test coop_test devtools_test; do
  g++ -std=c++20 -Wall -Werror -Icommon -I$Q -Ilib/miniz tests/mods/$t.cpp $SRC $OUT/*.o -lws2_32 -o $OUT/$t || exit 1
done
$OUT/modhost_test && $OUT/net_test && $OUT/builtin_test && $OUT/coop_test . && $OUT/devtools_test .
```

On Linux and Android `-lws2_32` is not needed.

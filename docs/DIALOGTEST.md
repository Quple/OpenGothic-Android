# The tool of the dialogs and scenes (tab Dialogs)

For a mod with a lot of dialogs (Chronicles of Myrtana: thousands of them) it is not possible to play every one to see what is missing in
the game. The tab **Dialogs** of the developer tools (F10, on a phone the button Mods of the settings; mod `mods/devtools`) runs them:

- the list of all dialogs of the scripts (`C_INFO`), with the character, the text, the state of the last run; the filter takes words
  (`status:error`, `status:ok`, `status:none` filter by the last run),
- **Analyze**: the condition of the dialog is read from the bytecode of the scripts (`game/dialogtest.cpp`), every way that returns TRUE
  is listed with what is already true and what is not,
- **Solve and run**: the tool sets what the condition needs (variables, dialogs that were told, status of the quest, items, the place
  of a character, the time of the day), calls the dialog and its choices, and lets the queues of commands go on **at once**
  (the lines, the animations, the functions called with `AI_Function` are checked),
- **Test all**: the same for every dialog of the filter, a few per frame, with a progress; the menu can be closed,
- **Scenes**: any function of the scripts is run the same way (events, cutscenes, `*_APPLY`),
- **Report**: `dialogtest_report.txt` next to `log.txt` and the log: the summary, *what is still missing* (the engine functions that the
  scripts call and the game does not answer, errors of the scripts, missing animations and texts, conditions that the tool could not set;
  the most common first, with an example dialog), and the dialogs that did not work.

The same from the console (F10 or the console of the mods): `comcheck dlg list|analyze|run|scene|all|report|reset`.

**It changes the state of the game** (the quests, the items, the places of the characters). Save first and run it on a game that can be
thrown away. `ExitSession` and `PlayVideo` of the scripts are ignored while the tool runs.

What the solver understands: comparisons of variables with constants, `Npc_KnowsInfo`, `Log_GetStatus`, `Npc_HasItems`,
`Npc_GetDistToWP`, `Npc_GetDistToNpc`, `Wld_IsTime`, `C_NPC.AIVAR[n]`, and the functions of the scripts that return a value (through the ways
of the function). What it does not (it says so in the report as "condition: ..."): `Npc_IsDead`, `Npc_IsInState`, the talents, the attitude,
who the character is (`Hlp_GetInstanceID`), a distance that has to be *large*.

Results of the run of the CoM dialogs are the list of what to do next for the full mod.

## Tests

- `node --test tests/js` — unit tests of the JavaScript of the developer tools (the tab Dialogs, the commands `comcheck`, the runner of
  the tests) with a fake game; they run without the game.
- The tab **Tests** (F10) and `comcheck test [all|<area>|<word>]` — the tests that run in the game (`mods/devtools/lib/selftests.js`):
  the symbols and the functions of the scripts, the time, the way points, TeleportNpcToWP, items, Ikarus (memory, sorting of arrays,
  pointers of characters), the dialogs (the list, UTF-8, the analysis of a condition, a run, the file of the progress, the report). A test
  that needs what the loaded game does not have is skipped. They change the state of the game and put it back.

## The cases that were fixed (`comcheck fix`)

`comcheck fix` lists the cases of Chronicles of Myrtana that were fixed in the engine. A case sets itself up in the game that is loaded, does what the
scripts of the mod do, and says PASS or FAIL by what it can read back (the tab Tests has the same with buttons):

| case | what it checks |
|---|---|
| `startkm` | `Npc_GetDistToWP` finds the way point START_KM, not the start point of the same name (the video with the fish) |
| `exit` | `ExitSession` of the scripts is ignored while the dialogs are tested |
| `armor` | `NPC_ChangeArmor`: Jorn wears the wounded outfit and is not naked |
| `slot` | `GetSlotItem` and `EquipWeapon` of the engine are answered (the fake bag) |
| `sort` | the arrays of numbers are sorted (`QSORT_G2`) |
| `other` | a command of `AI_Function` has OTHER: the fights of the scenes start |
| `trialog` | the trialog of Viktor and Martha runs |
| `teleport`, `routine`, `itemstate`, `plank` | two steps: `comcheck fix <case>`, close F10, wait, `comcheck fix <case> check` |
| `gestures`, `lock`, `stuck` | you look: the command tells what you should see |

`comcheck fix all` runs the cases that check themselves. The tests of these cases without the game: `node --test tests/js`.

## comtest

`comtest <case>` is the short form of `comcheck fix <case>` and answers by itself (the cases with a second step check themselves after some seconds,
the answer comes to the console and the log). The names for the five things that were fixed: `fight` (OTHER in the commands of the scenes),
`teleport` (AI_TeleportAlways), `sort` (QSORT_G2), `bag` (GetSlotItem, EquipWeapon), `tool` (the tool of the dialogs on Martha, Mob_HasItems,
Hlp_IsValidNpc). `comtest all` does every case one after another. `comtest` alone lists them.

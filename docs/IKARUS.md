# Ikarus and LeGo

Current DAT/source audit and remaining gaps: [24 October 2026](ARCHOLOS_AUDIT_2026-10-24.md).
See [the audit tool documentation](../tools/ikarus/README.md) for `--dat`, static
call counts, complete disassembly and semantic-review reports. Counts below are
historical and do not certify current campaign compatibility.

Ikarus and LeGo are libraries of scripts for Gothic 2. They read and write the memory of the original program
(`MEM_ReadInt`, `_^`, `_@`), call its functions by their addresses in `Gothic2.exe` (`CALL__thiscall`) and replace its
code (`HookEngine`). The mods that use them, for example *The Chronicles of Myrtana: Archolos* (CoM), cannot run in
OpenGothic as they are, because there is no such memory.

OpenGothic runs the scripts of the libraries in its own virtual machine and gives them a **pretended 32-bit memory**
(`common/game/compatibility`). Upstream: https://github.com/Try/OpenGothic/issues/231.

| File | What it does |
|---|---|
| `mem32.cpp` | the memory: regions that are allocated by the scripts, pinned to the data of the game, or *typed* (a function is called when a script reads or writes them) |
| `mem32instances.h` | the layouts of the classes of the original engine that scripts read (`oGame`, `zCParser`, `zCMover`, ...), from `EngineClasses_G2` of Ikarus |
| `cpu32.cpp` | a tiny x86 interpreter: Ikarus builds machine code for `CALL__*`, which is run here; the functions of the engine and of Windows are *answered* by C++ (`register_thiscall`, `register_cdecl`, `register_stdcall`) |
| `directmemory.cpp` | the functions of the libraries that are replaced by C++ (`override_function`), the engine functions that are answered, the pinned data |

`HookEngine` is not a patch of the engine. It is a table of the places where the library wants to be called (a frame, an
event of an animation); the ones that OpenGothic can answer with an event are answered, the others are listed in the log
as `not implemented call [HookEngineI]`.

## What a mod needs: the analysis of the sources

`tools/ikarus/analyze.py` reads the scripts of Ikarus, LeGo and a mod (CoM has a decompilation:
https://github.com/lmichaelis/tcom-decompilation) and lists the engine functions that the mod reaches through the
libraries and OpenGothic does not answer for, by how often the mod uses them:

```bash
python tools/ikarus/analyze.py --ikarus <Ikarus> --lego <LeGo> --mod <tcom-decompilation>
```

What it showed for CoM (scripts of CoM: about 5000 functions):

- The mod itself touches the engine objects very little: 579 variables of `zCMover` (only `moverState`, 1156 reads),
  53 of `oCNpc` (22 fields: `attribute`, `protection`, `bitfield`, `focus_vob`, `aivar`, ...), a few lists, views and symbols.
  The rest is **in the libraries**: `AI_Function` (1730 calls), `Trialoge` (`TRIA_*`, about 1300), frame functions
  (`FF_*`, 700), `MEM_SearchVobByName` (583), `PermMem`, `HookEngine` (about 90 places in the mod and in LeGo).
- The engine functions that the libraries need and were not answered: see the output of the tool. After the work below the
  list is short: file functions of Windows (`CreateFile`, ...), the drawing of LeGo (`zCRnd_D3D__*`, `zCView__Render`),
  fonts (`zCFont__GetFontX/Y`), `oCNpc__PutInSlot/RemoveFromSlot/Equip/UnequipItem`, `zCVob::SetVobName`, `zCAICamera::StartDialogCam`.

## Done

- `MEM_SearchVobByName`, `MEM_SearchAllVobsByName`: the movers of the world are mirrored as `zCMover`
  (`Compatibility::zCMover`, 624 bytes, the layout of Gothic 2) and found by the name of the vob. A script that waits for a
  mover reads `moverState` (0 open, 1 opening, 2 closed, 3 closing), which comes from the real mover (`MoveTrigger::moverState`).
  Other vobs are not found yet.
- `MEM_GetGothOpt`, `MEM_SetGothOpt`, `MEM_GothOptExists`, `MEM_GothOptSectionExists` (Gothic.ini, the values are kept),
  `MEM_GetModOpt`, `MEM_ModOptExists`, `MEM_ModOptSectionExists` (the ini of the mod, the parameter `-game`).
- The x86 interpreter has the register of the floating point (`fstp`), so the functions of C that return a float work:
  `sinf`, `acosf`, `atan2f` (LeGo `Sin`, `Acos`, `Atan2f`). `memset` (`MEM_Clear`), the swap of bytes (`MEM_SwapBytes`),
  the box of the message (`MEM_MessageBox` goes to the log).

- Characters and items have a pointer of their own (`MEM_InstToPtr`, `_@`, `InstanceHandles`): the pointer can be kept,
  compared, put in a list and turned back (`MEM_PtrToInst`), the first word is `oCNpc_vtbl` or `oCItem_vtbl`. The members of
  `oCNpc` and `oCItem` (`attribute`, `protection`, `aivar`, `value`, `description`, ...) are registered as the members of
  `C_NPC` and `C_ITEM`, so a script that reads them through such an instance reads the real character. The members of the
  engine that the VM does not have (`focus_vob`, the bitfields, the matrix of the vob) are not there: reading them raises
  an exception of the script, which is in the log with the stack of the script.
- `MEM_World.voblist_npcs` is the list of the characters of the world (made again every second).
- The arguments of the engine functions that the scripts call (`Cpu32::callExternal`) are popped in the order of the
  declaration. They were popped from the last to the first by GCC (the order of the evaluation of the arguments of a
  function is not specified), so every handler with two or more arguments got them reversed: `zCParser::CreateInstance`
  wrote the object over the parser, the views of LeGo got a pointer "2". Found with the log of CoM.
- The elements of an array member of an instance that lives in the pretended memory (`memory_instance`) are read with the
  size of the element (4 bytes, 20 for a string); the size of the class was used before.
- **Hooks** (`HookEngineF`) are events of OpenGothic: the layer keeps the table address -> functions, and the game calls
  `DirectMemory::fireHook` with the registers (`ECX`, `ESI`, ..., the variables of LeGo) and a stack in the pretended memory
  that the functions read (`MEM_ReadInt(ESP+4)`) and may change; what they leave there goes back to the game. Addresses:
  `hookaddresses.h`. The first call of every place is in the log (`DMA: the first call of the hook ...`). Answered:
  change of the level (begin, end), saving and loading, `oCNpcInventory::Insert` (the player takes an item: the quests of CoM),
  `oCNpc::ChangeAttribute` (the hook may change the value), `oCNpc::OnDamage` and the script of the damage (armor penetration,
  the damage that must not kill: `oSDamageDescriptor` is filled), the fall damage, the keys of the game (`oCGame::HandleEvent`),
  the change of the zone of the music (the places that CoM checks).
- **Savegames.** LeGo writes the handles of PermMem to `SCRPTSAVE.SAV` through `CreateFileA`, `WriteFile`, `ReadFile`: these
  functions work on files in memory, which go into the savegame of OpenGothic (the entry `game/dma`). After a load the
  original engine calls `Init_Global` and `Init_<world>`, and the layer does the same (`GameSession`, only for such mods): LeGo
  then reads its file. At a change of the level LeGo is told by the hooks, otherwise it takes the init for a load and loses
  the handles.
- **Views and texts of LeGo** are drawn by OpenGothic (`directmemory_ui.cpp`, `MainWindow::drawDmaOverlay`): the engine
  functions `zCView::zCView/Open/Close/Top/Move/SetSize/InsertBack` keep the views (the position, the size, the texture
  as an object whose name a script can read), the texts are read from the lists of the views and from the print view of
  the screen (`PrintS`, `Print_Ext`), the texts with a time are removed when the time is over, the width and the height of
  a text come from the real fonts (`zCFont::GetFontX/Y`, the screen of the scripts is 800x600). A view with the texture
  `DEFAULT.TGA` is a rectangle of its color. Not done: the color and the alpha of the text (the font has its own color),
  `zCFont::GetFontName`, the animations of `zCView` (`fxOpen`), the children of a view.
- Every exception of a script prints the stack of the script the first two times (log, red): it says who did it.
- `HookEngine` places that are not answered are listed in the tab Debug, Ikarus, not in the log (the log has one line).

## Not done

What of CoM does not work yet (every item is a place where the original engine is hooked or called, see the tab Debug,
Ikarus, for the list of the hooks that nobody answers):

1. **The custom inventory of CoM** (`CATINV_*`, about 30 hooks of `oCItemContainer`), the texts of the items in the
   windows of recipes (`OCITEMCONTAINER__DRAWITEMINFO_HOOK`): the inventory of OpenGothic is used.
2. **The windows of the dialogs** (`CRAFTINGVIEW_*`, `OCINFORMATIONMANAGER__UPDATE_HOOK`, `ZCVIEWDIALOGCHOICE__HANDLEEVENT_HOOK`):
   the crafting and the answers that are typed work as the normal choices of a dialog, without the icons.
3. **The status screen** (`STATUSSCREENSHOW_HOOK`, `UPDATE_MENU_ITEM`): the menu of the original has no items in memory.
4. **Slots and equipment of the engine**: `oCNpc::GetSlotItem` (answers 0), `PutInSlot`, `RemoveFromSlot`, `EquipWeapon`,
   `DropAllInHand`, the quivers (`_EVT_QUIVER_*`), the torch, `OCNPC__DROPUNCONSCIOUS_HOOK`.
5. **Companions and the cutscenes of CoM**: `OCNPC__GETNEXTENEMY_REPLACEHOOK`, `HIDEFOCUSFORPARTYMEMBERS_*`,
   `ZCCSCAMERA__ONTRIGGER`, `OCGAME__UPDATEPLAYERSTATUS_REPLACECHECKINGGLOBALCUTSCENE` (and the two like it),
   `zCAICamera::StartDialogCam`.
6. **Drawing**: the color and the alpha of a text (the font has its own color), the sprites of LeGo (`_SPRITE_DORENDER`),
   `Render`, `Draw3D`, the animations and the children of a `zCView`, the texture of the walls of the houses
   (`zCTexture::Load`).
7. **The picklock strings** (`HANDLE_SETPROPERPICKLOCKSTRLEN`), the important items that must not be dropped or sold
   (`OCNPC__EVDROPVOB_HOOK`, `ITEMIMPORTANTFLAG_CHECKSELLINGITEMS`), `oCLogManager` (`LOG_MOVETOTOP` does nothing).
8. Windows: `KmLib.dll` of CoM (Steam achievements and the music): `LoadLibrary` is answered by a pretended handle, the
   calls into it are dropped.

## How to try CoM

```
Gothic2Notr.exe -g "C:/Program Files (x86)/Steam/steamapps/common/TheChroniclesOfMyrtana" -game:TheChroniclesOfMyrtana.ini
```

On Android there is no command line: when the installation has an ini in `System` that names its own game or VDFs
(`TheChroniclesOfMyrtana.ini`), it is chosen at the start as `-game:` does, and the log says `mod of the installation: ...`.
An installation without such an ini starts the plain Gothic.

The messages of the libraries are in the log (`log.txt` next to the program): `mem_*: address translation failure`
(a script reads a memory that is not mirrored), `not implemented call [...]` (an engine function or a hook that is not
answered). They are the list of what to do next; `tools/ikarus/analyze.py` orders them by the use in the mod.

The same log is in the game: F10 (on a phone the Mods button of the settings), tab *Debug*. The buttons `mem_` and
`not implemented` of the log filter it to these lines, *Scripts* calls a function of the mod to try it (`MEM_SearchVobByName`
users, `INIT_*`, a dialog function), *World* moves the player and opens movers (`EVT_*`), and *Ikarus* shows the report
of the layer (the memory, the movers, the hooks that nobody answers).

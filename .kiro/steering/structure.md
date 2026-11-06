# Project Structure

## Top-level layout
```
common/      Engine + game logic (C++20), compiled into the OpenGothic static lib
game/        Desktop/Android game entry point (main.cpp)
spacer/      The editor (Spacer) and its assets
shader/      GLSL shaders -> GothicShaders target
android/     Android launcher (Java), manifest, touch assets, build scripts, tests
linux/       Linux packaging/platform files
packaging/   Packaging resources
patches/     Patches applied to dependencies
scripts/     Launch/helper scripts (e.g. Gothic2Notr.bat)
tools/       Developer tooling
mods/        Built-in mods copied next to the desktop executable
cmake/       CMake helper modules (embedmods, tempestpatch, zenkitpatch)
lib/         Vendored dependencies as git submodules
docs/        Design notes, compatibility audits, feature docs
```

## `common/` — engine internals
Most work happens here. Subsystems:
- `game/` — game session, scripting glue, AI state, combat/damage, inventory, quests, save serialization. `compatibility/` and `definitions/` hold engine-extension and data definitions.
- `graphics/` — renderer, draw buckets/clusters/commands, meshes (`mesh/`), particles (`pfx/`), sky, lighting, materials, texture quality. Android-specific culling and resolution handling lives here.
- `world/` — world objects, triggers, waypoints/waymatrix, physics bridge (`bullet.*`), focus and AI queue.
- `physics/` — Bullet integration.
- `scripting/` — Daedalus/script API (`api/`, `game/`), JavaScript mod host (`modhost`, `modjs`, quickjs), `net/` for co-op.
- `ui/` — menus, inventory, dialog, save screen, console, and the touch/mobile controls (`mobilecontrolsmodel`, `touchinput`, `touchdraw`), imgui mod panels.
- `sound/`, `dmusic/`, `bink/` — audio and video playback.
- `utils/` — platform helpers, including `androidinput.cpp` (touch icon list and input handling).
- Root files: `gothic.*` (game context), `resources.*`, `mainwindow.*`, `camera.*`, `commandline.*`, `marvin.*` (debug console).

## `android/` — mobile platform layer
- `java/org/opengothic/app/` — launcher Activities and services: `LauncherActivity`, `SteamLoginActivity`, `GameActivity`, `UpdateCheck.java`, `AppUpdate.java`, `SteamSession.java` / `SteamStore` / `SteamDownloadService`, etc.
- `AndroidManifest.xml`, `assets/` (touch icons, embedded via `assets/CMakeLists.txt`), `launcher-assets/` (covers, promo), `build-apk.sh`, `embed-resources.py`, `tests/`.

## Conventions
- **Naming:** C++ source/header files are lowercase, no separators (`gamesession.cpp`, `mobilecontrolsmodel.h`). Each `.cpp` pairs with a same-named `.h`. Java classes are PascalCase, one per file, in `org.opengothic.app`.
- **Formatting (`.editorconfig`):** UTF-8, LF line endings, final newline, no trailing whitespace. 2-space indent everywhere (C/C++, CMake, GLSL, Python, Lua, YAML, Markdown). C/C++ max line length 120.
- **Warnings are errors:** the engine lib builds with `-Wall -Wconversion -Werror` on non-MSVC. Keep new code warning-clean.
- **Platform-specific code** is gated with CMake variables and `#ifdef` (`ANDROID`, `WIN32`, `__APPLE__`). Android-only behavior (culling, fog resolution, touch UI) should stay conditional so desktop/iOS builds are unaffected.
- **Touch controls logic** is kept free of Android dependencies so it can be unit-tested on the host (`android/tests/controls_test.cpp` against `common/ui/mobilecontrolsmodel.cpp`).
- **Dependencies** live only in `lib/` as submodules; don't vendor copies elsewhere. Dependency build tweaks go through the `cmake/*patch.cmake` modules.
- **Docs** for ongoing compatibility work (Archolos, Ikarus/LeGo, mods, Spine, co-op) live in `docs/`; consult them before touching those areas.

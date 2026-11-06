# Product

OpenGothic Android is an Android-focused fork of [OpenGothic](https://github.com/Try/OpenGothic), the open-source reimplementation of the *Gothic* engine (original target: Gothic II: Night of the Raven).

The fork brings the engine to Android as an ARM64 APK and adds a mobile experience on top of the upstream engine:

- **Game library launcher** (Java/Android) for Gothic, Gothic II and The Chronicles of Myrtana: Archolos. Each game installs into its own folder; the active one is written to `game.txt` for the engine.
- **Touch controls and touch UI** drawn and handled by the engine (movement stick, drag-to-rotate camera, context-sensitive action buttons, touch menus, on-screen keyboard for saves).
- **Game-file import** from a ZIP, an on-device folder, or ADB. No game assets or scripts are bundled; players supply their own legally owned installation.
- **Optional Steam downloads** via JavaSteam (requires Android 8+).
- **JavaScript mods** imported from ZIPs and managed in the launcher or in-game.
- **Mobile performance/quality settings** (button size, 3D resolution, shadows, SSAO, FPS limit, deadzone, sensitivity) stored in the ini `[MOBILE]` section.

## Requirements and constraints

- ARM64 (`arm64-v8a`) device, Android 7.0 (API 24)+, Vulkan 1.1. Steam needs Android 8.0+.
- Ray tracing and mesh shading are disabled in the Android build.
- Android surface destruction (backgrounding, lock) closes the app, so saving before switching apps matters.
- Windows engine extensions (Union, Ninja, AST, Gothic DX11 Renderer) are not supported.

The engine is MIT licensed. Desktop/iOS targets from upstream still build from the same tree; mobile is the focus of this fork.

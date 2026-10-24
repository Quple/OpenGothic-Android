# OpenGothic Android

An Android-focused fork of [OpenGothic](https://github.com/Try/OpenGothic), the open-source reimplementation of Gothic II: Night of the Raven.

This fork brings the engine to Android with an ARM64 APK, a game library launcher, touch controls, game-file import and optional Steam downloads. Development focuses on the mobile experience and compatibility with Gothic, Gothic II and The Chronicles of Myrtana: Archolos. Compatibility work is ongoing; a game appearing in the launcher does not guarantee that every quest or mod works correctly.

[Download APK](https://github.com/borsuczyna/OpenGothic-Android/releases/tag/release) · [Report an issue](https://github.com/borsuczyna/OpenGothic-Android/issues) · [Android documentation](android/README.md)

## Requirements

- A 64-bit ARM Android device (`arm64-v8a`).
- Android 7.0 (API 24) or newer and Vulkan 1.1 support. Steam integration requires Android 8.0 or newer.
- A legally owned copy of the game you want to play. **The APK does not include game assets or scripts.** For Gothic II, use the Night of the Raven / Gold Edition installation.
- Enough free storage for the game files and, when importing, the ZIP archive.

## Install and play

1. Download the APK from this fork's [release page](https://github.com/borsuczyna/OpenGothic-Android/releases/tag/release) and install it on your Android device. Allow APK installation from the browser or file manager when Android prompts you.
2. Open OpenGothic. Sign in to Steam to download games available to your account, or choose **Continue without an account** to import your own installation.
3. Select Gothic, Gothic II or The Chronicles of Myrtana in the library:
   - **Download** retrieves the game files from Steam; choose the language before downloading.
   - **Import** opens a ZIP archive containing an existing game installation.
4. Once the files are ready, tap **Play** on the game's card.

For ZIP import, put `Data`, `_work` and `System` at the archive root, or inside a single enclosing folder:

```text
Gothic2.zip
├── Data/
├── _work/
└── System/
```

The launcher stores each game separately in the app's files (`Gothic1`, `Gothic2`, `Myrtana`). ZIP import uses Android's file picker and does not require broad storage access. See [manual installation with ADB](android/README.md#game-files) for another way to copy game data.

## Android features

- Touch controls: a left movement stick, camera rotation by dragging the right side, and context-sensitive action buttons.
- Touch menus, inventory categories and buttons for using, dropping and transferring items.
- Mobile settings for button size, rendering resolution, shadows, SSAO, FPS limit, joystick deadzone and camera sensitivity.
- Save management with an on-screen keyboard, renaming, overwriting and deleting saves.
- A launcher with separate game installations, ZIP import, optional Steam downloads and update notifications.
- JavaScript mods that can be imported from ZIP files and managed in the launcher or in-game menu.

The mobile defaults are 50% 3D rendering resolution, 256-pixel shadow maps, SSAO off and a 30 FPS limit. Ray tracing and mesh shading are disabled in the Android build.

## Compatibility and limitations

The project is under active development. Performance and rendering depend on the device and its Vulkan driver. Save before switching apps or locking the screen: destruction of the Android rendering surface closes the game. Uninstalling the app removes its game files, saves and settings.

Gothic II: Night of the Raven is the engine's original target. This fork also includes launcher entries and compatibility work for Gothic and Archolos. Ikarus and LeGo support is incomplete; see the [compatibility notes](docs/IKARUS.md) and [Archolos testing notes](docs/ARCHOLOS_TESTING.md) for details.

Windows engine extensions such as Union, Ninja, AST and the Gothic DX11 Renderer are not supported on Android.

For this fork's JavaScript mod format and API, see [Mods in JavaScript](docs/MODS.md). The launcher's **Mods → Add a mod (.zip)** action installs these JavaScript mods.

## Build the Android APK

The Android CI configuration uses:

- JDK 17 and Gradle 8.9.
- CMake 3.22.1 and Ninja.
- Android SDK platform 35, build-tools 35.0.0 and NDK 27.0.12077973.
- A host Vulkan SDK with `glslangValidator`.

Set `JAVA_HOME`, `ANDROID_HOME` and `VULKAN_SDK`. Add Gradle, Ninja and the host Vulkan SDK's `glslangValidator` to `PATH`. Vulkan headers come from the host SDK; Android links the NDK's Vulkan loader.

```sh
git clone --recurse-submodules https://github.com/borsuczyna/OpenGothic-Android.git
cd OpenGothic-Android
cmake -S . -B build/android -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android --target OpenGothic-apk
```

Replace `/path/to/ndk` with your NDK installation directory. For an existing checkout, initialize dependencies with `git submodule update --init --recursive`.

The release APK is written to:

```text
build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debuggable build. The alternative [`android/build-apk.sh`](android/build-apk.sh) script packages an APK without Gradle and omits Steam integration. Detailed build instructions, ADB commands and log locations are in [android/README.md](android/README.md).

## Bug reports

Report Android issues in [this fork's issue tracker](https://github.com/borsuczyna/OpenGothic-Android/issues). Include the app version, device model, Android version, game or mod, steps to reproduce and relevant logs. For a debug APK, retrieve the engine log with:

```sh
adb shell run-as org.opengothic.mobile cat files/log.txt
```

## Upstream and license

The engine is based on [Try/OpenGothic](https://github.com/Try/OpenGothic). Credit goes to the original project and its contributors. For desktop releases and documentation, visit the upstream repository; Android releases for this fork are linked above.

OpenGothic is distributed under the [MIT License](LICENSE). Original game assets remain the property of their respective owners and must be supplied separately.

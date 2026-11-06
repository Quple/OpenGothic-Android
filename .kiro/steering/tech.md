# Tech Stack

## Languages
- **C++20** — the engine and all game logic (`common/`, `game/`, `spacer/`). `set(CMAKE_CXX_STANDARD 20)`.
- **Java** — the Android launcher and platform glue (`android/java/org/opengothic/app`).
- **GLSL** — shaders in `shader/`, compiled with `glslangValidator`.
- **Python / shell** — build and asset helper scripts (`android/embed-resources.py`, `android/build-apk.sh`).

## Build system
CMake (minimum 3.16) with Ninja. The root `CMakeLists.txt` drives everything. Key targets:
- `OpenGothic` — static library with the engine sources (`common/*`).
- `Gothic2Notr` — the game executable (desktop) or shared lib (Android, via `add_android_apk`).
- `Spacer` — the editor.
- `OpenGothic-apk` — the Android APK (defined only when `ANDROID`).
- `GothicShaders`, `TouchAssets`, `SpacerAssets` — generated/asset targets.

Platform is selected by CMake variables: `ANDROID`, `WIN32`, `APPLE`, `IOS`, `UNIX`, `MSVC`. Touch UI is `OPENGOTHIC_TOUCH_UI` (always on for Android, optional on desktop where the mouse emulates touch).

## Dependencies
Vendored as git submodules under `lib/` (init with `git submodule update --init --recursive`):
- **Tempest** — graphics/windowing engine (Vulkan backend, Android input).
- **bullet3** — physics (Bullet 2 API, `BulletDynamics`, `BulletCollision`, `LinearMath`).
- **ZenKit** — Gothic asset loading.
- **dmusic** + **TinySoundFont** — DirectMusic / MIDI soundfont playback.
- **quickjs** (quickjs-ng) — JavaScript engine for mods.
- **imgui** — mod menu UI, drawn through Tempest's painter.
- **rapidjson**, **miniz** (zip), **edd-dbg** (Windows crashlog).

Android-only runtime libraries come through the Gradle `DEPENDENCIES` list in `CMakeLists.txt` (JavaSteam, BouncyCastle, Kotlin stdlib/coroutines, protobuf, OkHttp/Okio, xz, zstd-jni). `build-apk.sh` builds without Gradle and omits Steam.

## Android toolchain
JDK 17, Gradle 8.9, CMake 3.22.1, Ninja, Android SDK platform 35, build-tools 35.0.0, NDK 27.0.12077973. Set `JAVA_HOME`, `ANDROID_HOME`, `VULKAN_SDK`; put Gradle, Ninja and `glslangValidator` on `PATH`. Vulkan headers come from the host SDK; Android links the NDK's Vulkan loader.

## Common commands

Android APK (release):
```sh
cmake -S . -B build/android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release
cmake --build build/android --target OpenGothic-apk
```
Output: `build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk`. Use `-DCMAKE_BUILD_TYPE=Debug` for a debuggable APK. `android/build-apk.sh` builds a signed APK without Gradle.

Desktop build (no Android tools needed):
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
Debug desktop builds enable ASan/LSan (not on Android).

Install and inspect on device:
```sh
adb install -r build/android/OpenGothic-apk/build/outputs/apk/release/OpenGothic-apk-release.apk
adb logcat -s app AndroidRuntime DEBUG
adb shell run-as org.opengothic.mobile cat files/log.txt   # debug APK log
```

Controls state-machine test (runs on the host, no Android deps):
```sh
c++ -std=c++17 -I common android/tests/controls_test.cpp common/ui/mobilecontrolsmodel.cpp -o controls_test && ./controls_test
```

## Versioning
Bump `VERSION_CODE` and `VERSION_NAME` in `add_android_apk` (root `CMakeLists.txt`) with every release; the launcher's `UpdateCheck.java` compares `VERSION_NAME` against the GitHub release name.

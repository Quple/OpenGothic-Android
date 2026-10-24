# Rendering regressions

`rendering_test.cpp` checks visibility percentages (including Myrtana's indexed menu and vanilla multipliers), distance boundaries for large objects, conservative bounds under scale/shear, best-fit cluster reuse, unchanged bone matrices, and 10,000 worker completion handshakes. Early notifications and destruction with pending work are included. Transfers have zero bytes, so these CPU tests need neither game assets nor a GPU; GPU entry points abort if accidentally called.

After building the desktop engine, run from the repository root in a MinGW64 environment. Keep assertions enabled. On Windows the test executable lives beside the engine's runtime DLLs:

```sh
c++ -std=c++20 -O1 -UNDEBUG -DNOMINMAX -Icommon -Ilib/Tempest/Engine/include -Ilib/ZenKit/include -Ilib/bullet3/src tests/graphics/rendering_test.cpp common/graphics/drawclusters.cpp common/graphics/instancestorage.cpp common/utils/workers.cpp build-plain/lib/libTempest.dll.a -o build-plain/opengothic/rendering_test.exe
./build-plain/opengothic/rendering_test.exe
```

The compute visibility shader must also compile and pass `spirv-val` for its shadow, main (`MAIN_VIEW`, `HIZ`), and depth-occluder (`MAIN_VIEW`, `HIZ`, `LARGE`) variants. Visual quality and FPS still need testing on an Android device.

`quality_test.cpp` checks world texture quality quantization, box filtering, compressed DDS mip removal and decoding, native progressive collapse, unchanged surface area on a subdivided square, invalid maps/cycles, and packed LOD index/vertex ranges. Optional arguments are native MRM files for an asset audit. It uses CPU image decoding and mesh packing, with no GPU:

```sh
c++ -std=c++20 -O1 -UNDEBUG -DNOMINMAX -ffunction-sections -fdata-sections -Icommon -Ilib/Tempest/Engine/include -Ilib/ZenKit/include -Ilib/bullet3/src tests/graphics/quality_test.cpp common/graphics/mesh/submesh/packedmesh.cpp common/game/compatibility/phoenix.cpp build-plain/lib/libTempest.dll.a build-plain/lib/libzenkit.a build-plain/lib/libsquish.a -Wl,--gc-sections -o build-plain/opengothic/quality_test.exe
./build-plain/opengothic/quality_test.exe lib/ZenKit/tests/samples/mesh0.mrm
```

In PowerShell quote `'-Wl,--gc-sections'` as one argument. The C++/GLSL cluster layout is now 48 bytes (the two LOD ranges start at byte 32); the visibility push constants are 32 bytes. Validate the cluster patch shader as well as all visibility variants after changing either layout.

`lod_gpu_test.cpp` runs the actual shadow visibility compute shader with Vulkan validation enabled. It checks near/middle/far LOD selection, quality and radius thresholds, camera-relative distance, terrain exclusion, empty material sections and distance culling, including emitted meshlet IDs and indirect counts. It requires a Vulkan device and compiled visibility shader:

```sh
c++ -std=c++20 -O1 -UNDEBUG -DNOMINMAX -Icommon -Ilib/Tempest/Engine/include -Ilib/ZenKit/include -Ilib/bullet3/src tests/graphics/lod_gpu_test.cpp build-plain/lib/libTempest.dll.a -o build-plain/opengothic/lod_gpu_test.exe
./build-plain/opengothic/lod_gpu_test.exe build-plain/shader/sprv/visibility_pass.comp.sprv
```

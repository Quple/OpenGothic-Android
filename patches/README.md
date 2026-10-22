# Local patches of the submodules

`lib/ZenKit` is the upstream GothicKit/ZenKit, OpenGothic needs three changes that are not in it yet:

- `DaedalusVm::invoke_function` - a call of a function with the arguments already on the stack (`AI_Function`, `debug.call`);
- `DaedalusVm::pop_int` accepts an instance and returns the index of its symbol, as the original engine does (the destructor
  `Bar_Delete(var int)` of LeGo is called with an instance);
- `INpc::compat_focus_vob` / `compat_interact_mob` - the members of oCNpc that C_NPC does not have.

CMake applies the patch by itself (`cmake/zenkitpatch.cmake`, before ZenKit is added): a clean clone, the Android build and the CI need nothing more. A tree that has it already is left alone. By hand (the patch fails when it was applied already):

```bash
git -C lib/ZenKit apply --ignore-whitespace ../../patches/zenkit-opengothic.patch
```

Make the patch again after a change in the submodule: `git -C lib/ZenKit diff > patches/zenkit-opengothic.patch`.

## Tempest

`lib/Tempest` is the upstream Try/Tempest. `patches/tempest-opengothic.patch` is about the creation of the Vulkan pipelines, a crash on Mali (Pixel 9, `hardened_malloc: detected write after free` in `vkCreateComputePipelines`):

- `VCompPipeline` destroyed the pipeline layout when the driver refused the pipeline, but the layout belongs to `VPsoLayoutCache` and the other pipelines use it: it was freed and handed out again;
- no pipeline derivatives (`ALLOW_DERIVATIVES`, `DERIVATIVE_BIT`): the bindless variants (update-after-bind) were derived from a base pipeline, Arm advises against it and it brings nothing;
- `VDevice::pipelineSync`: one pipeline is compiled at a time (the compilation of the shaders runs on a thread of its own while the others create objects).

CMake applies it (`cmake/tempestpatch.cmake`, before Tempest is added), as for ZenKit. By hand: `git -C lib/Tempest apply --ignore-whitespace ../../patches/tempest-opengothic.patch`; make it again with `git -C lib/Tempest diff > patches/tempest-opengothic.patch`.

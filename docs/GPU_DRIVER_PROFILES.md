# Mobile GPU and driver profiles

EmuCoreX resolves mobile GS behavior in four independent layers:

1. API: OpenGL ES or Vulkan.
2. GPU identity: Adreno, Mali, or PowerVR, then architecture, exact model, and core count where available.
3. Driver identity: proprietary Qualcomm/ARM/Imagination, Mesa Turnip/PanVK/PowerVR, or ANGLE.
4. Driver release: parsed ordered version, legacy Mali hash, exact raw version, Android SDK, and relevant Vulkan limits.

This ordering is intentional. A workaround for Qualcomm's proprietary Vulkan stack must not be inherited by Turnip merely because both report an Adreno device. Unknown devices stay in a conservative family fallback and never inherit an invented exact model.

## Files

- `GSGPUProfileAdreno.cpp`, `GSGPUProfileMali.cpp`, and `GSGPUProfilePowerVR.cpp` identify hardware and select GS resource-pool tuning.
- `GSGPUDriverProfile.cpp` resolves driver bugs and required actions.
- `gpu-driver-database.json` is the private local source ledger for every runtime rule and is intentionally gitignored.
- `scripts/validate_gpu_driver_database.py` checks that the JSON rule IDs and flag names match the C++ implementation.

## Integration policy

`active` means the corresponding GS fallback is enforced today. `partial` means detection is active and only the actions listed in `activeIntegrations` are enforced. `catalogued` means the condition is detected and exposed to GS, but is deliberately not wired into a renderer path which has no tested equivalent yet.

This distinction prevents a correct upstream observation from becoming an unsafe global hack. For example, a PowerVR clear-load-op issue is tracked immediately, but it should only alter PCSX2 render-pass construction after the replacement path preserves GS load/store semantics and passes regression tests.

Currently enforced renderer actions include:

- orphaned OpenGL streaming/upload buffers on proprietary Mali, Adreno, and PowerVR drivers;
- single-threaded OpenGL shader compilation on Android driver stacks; Vulkan GS pipeline compilation is already serialized;
- component-wise vector bitwise operations in Mali TFX/convert shaders;
- boolean-negation rewriting in proprietary Adreno OpenGL TFX shaders;
- constant-index selection for the affected legacy Mali uniform-matrix access;
- isolated bitwise-negation temporaries in proprietary PowerVR OpenGL TFX shaders;
- descriptor-set fallback for proprietary Mali and PowerVR Vulkan;
- disabling proprietary Adreno provoking vertex;
- D24S8 depth selection for proprietary Adreno, with a capability-checked D32S8 fallback;
- copy/texture-barrier feedback fallback for proprietary Adreno instead of subpass framebuffer fetch;
- depth/stencil load-store preservation on proprietary Adreno 5xx;
- blend-based preservation of fully masked RGB on Adreno 5xx depth-tested draws;
- reusable linear-image staging before Qualcomm image-to-buffer readback;
- materializing lazy clears before PowerVR render passes instead of using a clear load operation;
- manual framebuffer-blit mip generation for tall PowerVR OpenGL textures;
- legacy PowerVR swapchain-width alignment at the pinned driver cutoff;
- GENERAL-layout round-trip before clear passes on the exact affected legacy Mali driver;
- disabling attachment-feedback-loop layout on proprietary Mali and PowerVR;
- coherent Vulkan readback memory on proprietary Mali and Adreno;
- classic render passes, concrete framebuffers, static topology, and monolithic pipelines where
  affected drivers forbid dynamic rendering, imageless framebuffers, extended dynamic state, or GPL;
- the Vulkan backend uses a normal 0..1 viewport depth range, so affected Qualcomm drivers never
  receive the reversed-range operation;
- packed 4444/565/1555 host formats affected on PowerVR are absent from the Vulkan format mapping;
- arithmetic forcing of the remaining direct Mali r32-r39 uniform-to-builtin vertex load;
- primitive restart remains disabled in the GS pipelines, and Vulkan barriers use explicit subresource counts;
- Mali-G57 FIFO present fallback on OpenGL ES.

All decisions and their pinned upstream revisions are stored in the private JSON database. The research copies under `oldcore/research` are disposable and are not build inputs. Neither the database nor the research copies are published.

## Validation levels

The automated release gate is:

1. database validation, including a one-to-one check between every `active` rule and all of its declared workarounds;
2. native profile boundary tests on Android, covering exact models, unknown-family fallbacks, proprietary/Mesa separation, and first-fixed driver releases;
3. representative OpenGL and Vulkan TFX shader compilation with the affected rewrite paths enabled;
4. a complete Android debug APK build.

Physical-device certification is a separate gate because an emulator or SwiftShader cannot reproduce proprietary mobile driver bugs. The minimum hardware matrix is:

- Adreno 5xx, 6xx, 7xx, and a current generation, using both Qualcomm proprietary Vulkan/OpenGL and Turnip where available;
- Mali Midgard, Bifrost, Valhall, and a current Immortalis generation, keeping proprietary ARM and PanVK results separate;
- PowerVR Rogue/GE and a newer B-series device, with the raw driver version recorded;
- at least one Android version below and above every SDK-bounded rule that is still reachable.

For each device, record renderer, driver strings and raw Vulkan version, cold shader-cache boot, GS dump or game scene, frame checksum/screenshot, present mode, and readback result. A rule can be code-complete without claiming physical certification until that row has been run on real hardware.

## Updating the database

1. Pin the upstream commit and exact source file.
2. Add or refine a stable C++ rule ID; never key behavior only on a marketing SoC name.
3. Mirror the condition, flags, confidence, and source in the JSON database.
4. Add boundary tests for the affected model or driver release, including the first known-fixed release.
5. Run:

   `python scripts/validate_gpu_driver_database.py`

6. Build the Android debug APK and run the available unit tests.

Do not remove a workaround based only on a newer device working. The rule should be narrowed to a verified fixed driver boundary, while unknown or unparsable proprietary releases retain the conservative path.

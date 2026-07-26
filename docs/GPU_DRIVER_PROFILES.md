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

`active` means the corresponding GS fallback is enforced today. `partial` means detection is active and only the actions listed in `activeIntegrations` are enforced. `catalogued` means the condition is detected for diagnostics, but is deliberately not wired into a renderer path which has no tested PCSX2 equivalent yet.

This distinction prevents a correct observation in another renderer from becoming an unsafe global hack. A bug flag describes a driver. A workaround describes a tested action in this GS backend; the two are not interchangeable.

The runtime policy is capability-first:

- persistent OpenGL buffer storage remains the first choice whenever the context exposes it;
- native shader operators remain in unaffected shader variants;
- optional framebuffer-fetch paths remain controlled by their capability and user setting;
- no driver rule may silently replace a fast path with extra copies, barriers, render passes, or serialized compilation without a model or driver-version boundary;
- unknown devices keep correctness-sensitive shader fixes, but do not inherit speculative performance workarounds.

Currently enforced renderer actions include:

- component-wise vector bitwise operations in Mali TFX/convert shaders;
- boolean-negation rewriting in proprietary Adreno OpenGL TFX shaders;
- constant-index selection for the affected legacy Mali uniform-matrix access;
- isolated bitwise-negation temporaries before the fixed PowerVR 1.8@4693462 driver;
- descriptor-set fallback for proprietary Mali and PowerVR Vulkan;
- disabling proprietary Adreno provoking vertex;
- high-precision D32S8 depth on capable devices, with D24S8 used only as a format-capability fallback;
- blend-based preservation of fully masked RGB on Adreno 5xx depth-tested draws;
- materializing lazy clears before PowerVR render passes only on the affected 1.7–1.9 driver range;
- manual framebuffer-blit mip generation only for tall legacy PowerVR SGX textures;
- legacy PowerVR swapchain-width alignment at the pinned driver cutoff;
- disabling attachment-feedback-loop layout on proprietary Mali and PowerVR;
- coherent Vulkan readback memory on proprietary Mali and Adreno;
- Mali-G57 FIFO present fallback on proprietary OpenGL ES and Vulkan drivers.

Mobile resource profiles only bound pool size and lifetime. They do not lower the user's texture-preloading
setting or disable the same-frame texture-allocation fast path. A catalogued slow readback operation also
does not justify inserting an extra GPU image copy without a device- and driver-bounded measurement.

Known depth/stencil-discard, dynamic-rendering, imageless-framebuffer, extended-dynamic-state,
primitive-topology, GPL, 16-bit-format, primitive-restart, reversed-depth, and Android host-precompile issues remain catalogued
where the current GS backend either does not use the affected operation or has no equivalent fallback.
They intentionally do not disable unrelated fast paths.

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

# HelEngine Apple

Shared C++20/Objective-C++ backend for macOS and iOS using SDL3 and Metal. Source checkout: `C:\dev\helworks\helengine-apple`; approved native checkout: `/Users/distro/dev/helworks/helengine-apple`. Work is on the real `main` checkout, with no worktrees or source exports.

The native diagnostic currently builds on Apple Silicon with Xcode 26.2. Actual Metal tests on Apple M2 verify texture upload, ordered alpha blending, GPU completion/resource retention, scissor clipping and per-corner rounded borders through offscreen pixel readback. Lifecycle tests cover suspension, zero-sized viewports and unavailable drawable pacing. macOS Debug and Release have passed; iOS device and simulator Debug have compiled unsigned against SDK 26.2. Unsigned compilation does not prove device deployment or execution.

The authored shader contributor compiles HLSL using the engine's shaderc frontend as an intermediate compiler, then translates SPIR-V to MSL with SPIRV-Cross and builds a real Metal library with the selected Apple SDK. Metal remains the only Apple runtime renderer. Material cooking preserves the engine binary format, authored resources and fixed float32 GPU constant layouts. Target registration uses two thin assemblies sharing `helengine.apple.builder` so the existing assembly-only loader selects one unambiguous target per assembly.

The real generated macOS Core and DemoDisc gameplay execute the complete 15-scene catalog. Apple M2 Debug 32/32 offscreen validation passed all 14 rendering scenes, using hardware indexed visibility for the ten GPU scenes and 1,200 real update/draw frames for each CPU path tracer. All four tracers completed one sample per pixel with zero non-finite samples. The startup menu passed 300 frames with 321 draw calls, all 44 unique authored item IDs, keyboard edges and real SDL virtual-controller navigation. No scene buttons were filtered. These checks establish actual rendering and lifecycle behavior; they do not certify visual parity or performance.

The native 3D provider consumes authored indexed submeshes, shader reflection, material constants/textures, forward lights, shadow atlas and point shadow cubes. Controlled Core draws with simulation held fixed verify that disabling shadows changes receiver pixels in both authored shadow scenes and re-enabling them exactly restores the image. GPU fixtures verify projected/radial shadow depth, cube faces, matrix packing, material pixels and counterclockwise front-face culling. Project-specific assertions remain in tests; production startup consumes the shared manifests.

The iOS player uses SDL callbacks, portable touch/gamepad snapshots and lifecycle pacing with the same Core/Metal providers. Actual generated Core device and arm64 simulator unsigned linking passed against SDK 26.2, preserving canonical generated Core/package inputs across SDK changes; no physical-device or simulator execution is claimed. Profiler builds still fail explicitly until generated scopes, Tracy integration and an actual capture are verified. Resource arrays, deferred/HDR/normal-map rendering, multiple queued 3D cameras and authored render targets remain explicitly unsupported. The full generated Core validation baseline is Debug 32/32; other numeric policies and Release player parity remain unverified.

## Reproduce native validation

Dependencies are pinned in `C:\dev\helworks\helengine-apple\toolchain.lock.json` and `/Users/distro/dev/helworks/helengine-apple/toolchain.lock.json`: CMake 3.31.12, SDL3 3.2.28, Xcode SDK 26.2, Metal Toolchain 17C7003j, shaderc v2025.3 and SPIRV-Cross vulkan-sdk-1.4.328.1. CMake and SDL archives are SHA-256 checked; compiler source commits and their transitive DEPS are fixed. The engine SDK is .NET 9.0.308 with roll-forward disabled.

```sh
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target macos --profile Debug --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target macos --profile Release --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target ios --profile Debug --sdk device --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target ios --profile Debug --sdk simulator --diagnostic
```

Builds overwrite `/tmp/helengine/macos` or `/tmp/helengine/ios`; switching the iOS SDK refreshes CMake metadata with `--fresh` while preserving canonical package/cache/managed inputs. Build/test logs and the current validation receipt live there. Persistent tools live in `/Users/distro/dev/helworks/tools`. No desktop window, screenshot or simulator launch is needed for these checks.

For the managed builders and authored shader test:

```sh
export HELENGINE_APPLE_ROOT=/Users/distro/dev/helworks/helengine-apple
export DYLD_LIBRARY_PATH=/Users/distro/dev/helworks/tools/shaderc/lib
/Users/distro/.dotnet/dotnet run --project /Users/distro/dev/helworks/helengine-apple/tests/builder/helengine.apple.builder.tests.csproj --artifacts-path /tmp/helengine/macos/managed -- --shader
```

The macOS builder assembly is `/tmp/helengine/macos/managed/bin/helengine.macos.builder/debug/helengine.macos.builder.dll`; iOS is `/tmp/helengine/macos/managed/bin/helengine.ios.builder/debug/helengine.ios.builder.dll`. Both require the adjacent shared dependencies. `HELENGINE_APPLE_SDK=iphonesimulator` selects simulator libraries for iOS shader compilation.

CMake player inputs are `HELENGINE_CORE_CPP_ROOT`, `HELENGINE_CODE_ROOT`, `HELENGINE_CONTENT_ROOT` and `HELENGINE_NATIVE_PROFILE=Debug|Release|Profiler`. Missing or incomplete core integration is fatal. The explicit `HELENGINE_DIAGNOSTIC_ONLY=ON` option is only for native SDK/GPU diagnostics.

For the current real macOS Core/menu receipt, run `/Users/distro/dev/helworks/helengine-apple/scripts/validate_core.py --core-root <actual-generated-core> --code-root <actual-generated-code> --content-root /tmp/helengine/macos/package --demodisc`. It overwrites `/tmp/helengine/macos/core-validation.json` and verifies unknown scene rejection and teardown as well as GPU output and navigation. The actual player is `/tmp/helengine/macos/helengine_macos.app/Contents/MacOS/helengine_macos`; use `--content /tmp/helengine/macos/package`, with `--offscreen --frames 300` for windowless execution.

Run `/Users/distro/dev/helworks/helengine-apple/scripts/validate_demodisc_rendering.py --content-root /tmp/helengine/macos/package` for the complete rendering receipt at `/tmp/helengine/macos/rendering-validation.json`, and `/Users/distro/dev/helworks/helengine-apple/scripts/validate_shadows.py --content-root /tmp/helengine/macos/package` for `/tmp/helengine/macos/shadow-validation.json`. Build the `demo_disc_trace_tests` and `shadow_scene_tests` CMake targets first. `/Users/distro/dev/helworks/helengine-apple/scripts/validate_ios_core.py` accepts the corresponding iOS core/code/content roots, links both unsigned SDK variants in the same output root, and records `/tmp/helengine/ios/core-validation.json` without launching either application.

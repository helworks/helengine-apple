# HelEngine Apple

Shared C++20/Objective-C++ backend for macOS and iOS using SDL3 and Metal. Source checkout: `C:\dev\helworks\helengine-apple`; approved native checkout: `/Users/distro/dev/helworks/helengine-apple`. Work is on the real `main` checkout, with no worktrees or source exports.

The native diagnostic currently builds on Apple Silicon with Xcode 26.2. Actual Metal tests on Apple M2 verify texture upload, ordered alpha blending, GPU completion/resource retention, scissor clipping and per-corner rounded borders through offscreen pixel readback. Lifecycle tests cover suspension, zero-sized viewports and unavailable drawable pacing. macOS Debug and Release have passed; iOS device and simulator Debug have compiled unsigned against SDK 26.2. Unsigned compilation does not prove device deployment or execution.

The authored shader contributor compiles HLSL using the engine's shaderc frontend as an intermediate compiler, then translates SPIR-V to MSL with SPIRV-Cross and builds a real Metal library with the selected Apple SDK. Metal remains the only Apple runtime renderer. Material cooking preserves the engine binary format, authored resources and fixed float32 GPU constant layouts. Target registration uses two thin assemblies sharing `helengine.apple.builder` so the existing assembly-only loader selects one unambiguous target per assembly.

Generated-core startup, the menu bridge and complete DemoDisc execution are still being integrated. The diagnostic is explicitly labelled and cannot be returned as a game player build. All 44 menu entries and IDs must be preserved when integrated. The 14 rendering scenes have not been validated. Profiler builds currently fail explicitly until generated scopes, Tracy integration and an actual capture are verified.

## Reproduce native validation

Dependencies are pinned in `C:\dev\helworks\helengine-apple\toolchain.lock.json` and `/Users/distro/dev/helworks/helengine-apple/toolchain.lock.json`: CMake 3.31.12, SDL3 3.2.28, Xcode SDK 26.2, Metal Toolchain 17C7003j, shaderc v2025.3 and SPIRV-Cross vulkan-sdk-1.4.328.1. CMake and SDL archives are SHA-256 checked; compiler source commits and their transitive DEPS are fixed. The engine SDK is .NET 9.0.308 with roll-forward disabled.

```sh
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target macos --profile Debug --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target macos --profile Release --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target ios --profile Debug --sdk device --diagnostic
python3 /Users/distro/dev/helworks/helengine-apple/scripts/build_native.py --target ios --profile Debug --sdk simulator --diagnostic
```

Builds overwrite `/tmp/helengine/macos` or `/tmp/helengine/ios`; switching the iOS SDK replaces the previous native iOS output. Build/test logs and the current validation receipt live there. Persistent tools live in `/Users/distro/dev/helworks/tools`. No desktop window, screenshot or simulator launch is needed for these checks.

For the managed builders and authored shader test:

```sh
export HELENGINE_APPLE_ROOT=/Users/distro/dev/helworks/helengine-apple
export DYLD_LIBRARY_PATH=/Users/distro/dev/helworks/tools/shaderc/lib
/Users/distro/.dotnet/dotnet run --project /Users/distro/dev/helworks/helengine-apple/tests/builder/helengine.apple.builder.tests.csproj --artifacts-path /tmp/helengine/macos/managed -- --shader
```

The macOS builder assembly is `/tmp/helengine/macos/managed/bin/helengine.macos.builder/debug/helengine.macos.builder.dll`; iOS is `/tmp/helengine/macos/managed/bin/helengine.ios.builder/debug/helengine.ios.builder.dll`. Both require the adjacent shared dependencies. `HELENGINE_APPLE_SDK=iphonesimulator` selects simulator libraries for iOS shader compilation.

CMake player inputs are `HELENGINE_CORE_CPP_ROOT`, `HELENGINE_CODE_ROOT`, `HELENGINE_CONTENT_ROOT` and `HELENGINE_NATIVE_PROFILE=Debug|Release|Profiler`. Missing or incomplete core integration is fatal. The explicit `HELENGINE_DIAGNOSTIC_ONLY=ON` option is only for native SDK/GPU diagnostics.

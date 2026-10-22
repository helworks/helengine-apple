using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.apple.builder;

/// <summary>Publishes the common Metal runtime contract with distinct macOS and iOS target identities.</summary>
public static class ApplePlatformDefinitionFactory {
    /// <summary>Creates target metadata without claiming that native diagnostics are complete game players.</summary>
    public static PlatformDefinition Create(string target) {
        if (target != "macos" && target != "ios") { throw new ArgumentException("Apple target must be macos or ios.", nameof(target)); }
        return new PlatformDefinition(
            target, target == "macos" ? "macOS Metal" : "iOS Metal",
            [
                new("debug", "Debug", "Native symbols and diagnostics", "metal", "default", []),
                new("release", "Release", "Optimized native runtime", "metal", "default", []),
                new("profiler", "Profiler", "Requires generated scopes and a verified Tracy capture; currently rejected", "metal", "default", [])
            ],
            [new("metal", "Metal", "Shared native Apple renderer", [])],
            [], [], [],
            [new("default", "Native C++20", "Little-endian ARM64 Clang with the engine row-vector convention", PlatformCodegenLanguage.Cpp, PlatformSerializationEndianness.LittleEndian,
                [
                    new(PlatformCodegenSettingIds.EnabledFeatures, "Enabled Runtime Features", PlatformSettingKind.Text, "host_file_system", true, []),
                    new("load-native-runtime-metadata", "Load Native Runtime Metadata", PlatformSettingKind.Boolean, "true", true, []),
                    new("codegen-use-rtti", "Use Compiler RTTI", PlatformSettingKind.Boolean, "true", true, []),
                    new("codegen-use-exceptions", "Use C++ Exceptions", PlatformSettingKind.Boolean, "true", true, [])
                ], new PlatformNumericCapabilities(helengine.NumericPrecisionPolicy.Default, [new(16,16), new(32,32), new(64,64)]))],
            [new("loose-files", "Read-only bundled content", PlatformStorageProfileKind.LooseFiles, target + "-loose-files", false)],
            [new(target + "-app-bundle", "Apple app bundle", PlatformMediaLayoutKind.InstallTree, false, false)],
            runtimeGenerationContract: new RuntimeGenerationContract(RuntimeMaterialResolutionMode.RawShaderBacked, true, PackagedPathPolicy.ContentRelativeOnly));
    }
}

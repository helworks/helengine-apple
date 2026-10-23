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
                new("debug", "Debug", "Native symbols and diagnostics", "metal", "default", [], new Dictionary<string,string>(StringComparer.Ordinal) { ["codegen-generated-function-profiling"] = "false" }),
                new("release", "Release", "Optimized native runtime", "metal", "default", [], new Dictionary<string,string>(StringComparer.Ordinal) { ["codegen-generated-function-profiling"] = "false" }),
                new("profiler", "Profiler", "Generated scopes and native Tracy CPU samples; macOS capture validation required", "metal", "default", [], new Dictionary<string,string>(StringComparer.Ordinal) {
                    ["codegen-generated-function-profiling"] = "true",
                    ["codegen-generated-function-profiling-maintained-symbol-prefixes"] = "helengine.Core.;helengine.SceneManager.;helengine.RenderCommandListBuilder2D."
                })
            ],
            [new("metal", "Metal", "Shared native Apple renderer", [
                new("default-width","Default Window Width",PlatformSettingKind.Text,"1280",true,[]),
                new("default-height","Default Window Height",PlatformSettingKind.Text,"720",true,[]),
                .. (target == "ios" ? new PlatformSettingDefinition[] { new("screen-orientation", "Screen Orientation", PlatformSettingKind.Choice, "unspecified", true, ["unspecified", "portrait", "landscape"]) } : [])
            ])],
            [], [AppleMaterialSchema.Create()], [],
            [new("default", "Native C++20", "Little-endian ARM64 Clang with the engine row-vector convention", PlatformCodegenLanguage.Cpp, PlatformSerializationEndianness.LittleEndian,
                [
                    new("generated-math-convention", "Generated Math Convention", PlatformSettingKind.Text, "engine-row-vector", true, []),
                    new("pointer-size-bytes", "Pointer Size Bytes", PlatformSettingKind.Text, "8", true, []),
                    new("codegen-generated-function-profiling", "Generated Function Profiling", PlatformSettingKind.Boolean, "false", true, []),
                    new("codegen-generated-function-profiling-maintained-symbol-prefixes", "Maintained Profiling Symbol Prefixes", PlatformSettingKind.Text, "helengine.Core.;helengine.SceneManager.;helengine.RenderCommandListBuilder2D.", true, []),
                    new(PlatformCodegenSettingIds.EnabledFeatures, "Enabled Runtime Features", PlatformSettingKind.Text, "host_file_system", true, []),
                    new("load-native-runtime-metadata", "Load Native Runtime Metadata", PlatformSettingKind.Boolean, "true", true, []),
                    new("codegen-use-rtti", "Use Compiler RTTI", PlatformSettingKind.Boolean, "true", true, []),
                    new("codegen-use-exceptions", "Use C++ Exceptions", PlatformSettingKind.Boolean, "true", true, [])
                ], new PlatformNumericCapabilities(helengine.NumericPrecisionPolicy.Default, [new(16,16), new(32,32), new(64,64)]))],
            [new("loose-files", "Read-only bundled content", PlatformStorageProfileKind.LooseFiles, target + "-loose-files", false)],
            [new(target + "-app-bundle", "Apple app bundle", PlatformMediaLayoutKind.InstallTree, false, false)],
            runtimeGenerationContract: new RuntimeGenerationContract(RuntimeMaterialResolutionMode.RawShaderBacked, true, PackagedPathPolicy.ContentRelativeOnly,
                portableInputPreprocessorSymbols: target == "macos" ? ["DESKTOP_PLATFORM"] : []));
    }
}

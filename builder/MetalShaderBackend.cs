using System.Diagnostics;
using System.Text.Json;
using helengine;

namespace helengine.apple.builder;

/// <summary>Compiles authored HLSL through the engine shaderc frontend, pinned SPIRV-Cross and the selected Apple Metal SDK.</summary>
public sealed class MetalShaderBackend : ShaderBackendBase, IShaderBackendCachePolicy {
    /// <summary>Serializes compiler scratch use because platform outputs are overwritten in a single fixed location.</summary>
    static readonly object CompilerLock = new();
    /// <summary>Identifies whether the selected library targets macOS or iOS.</summary>
    readonly string PlatformTarget;
    /// <summary>Shares the exact engine HLSL IO, descriptor binding and SPIR-V environment configuration.</summary>
    readonly AppleSpirvFrontend Frontend = new();

    /// <summary>Creates a target-specific compiler while retaining a single shared Metal implementation.</summary>
    public MetalShaderBackend(string target) : base(ShaderCompileTarget.Metal, new ShaderBackendCapabilities(new ShaderModel(4,0), new ShaderModel(6,7), [ShaderStage.Vertex, ShaderStage.Pixel], false), "Metal") {
        if (target != "macos" && target != "ios") { throw new ArgumentException("Metal platform must be macos or ios.", nameof(target)); }
        PlatformTarget = target;
    }

    /// <summary>Disables a cache keyed only by Metal target, which cannot distinguish macOS, iOS device and simulator library ABIs.</summary>
    public bool UseSharedCache => false;

    /// <summary>Emits a real MTLB library and retains the SPIR-V reflection which maps preserved bindings to native Metal slots.</summary>
    protected override byte[] CompileBytecode(ShaderCompileRequest request, IShaderIncludeResolver includeResolver, out ShaderCompileDiagnostic[] diagnostics) {
        if (!OperatingSystem.IsMacOS()) { throw new PlatformNotSupportedException("Authored Metal shader compilation requires the real Apple SDK on macOS."); }
        if (request.Options.BindingPolicy.DefaultSpace != 0 || request.Options.BindingPolicy.ConstantBufferShift != 0) { throw new NotSupportedException("The shared shaderc frontend and Metal adapter require descriptor space zero and zero constant-buffer shift."); }
        lock (CompilerLock) {
            string tools = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "dev/helworks/tools");
            string cross = Path.Combine(tools, "spirv-cross/bin/spirv-cross");
            if (!File.Exists(cross)) { throw new FileNotFoundException("Install locked SPIRV-Cross with scripts/bootstrap_shader_tools.py.", cross); }
            string translator = "/tmp/helengine/macos/helengine_msl_translate";
            if (!File.Exists(translator)) { throw new FileNotFoundException("Build the native macOS diagnostic to provide the engine-to-Metal resource binding translator.",translator); }
            string root = Path.Combine("/tmp/helengine", PlatformTarget, "shader-compiler");
            Directory.CreateDirectory(root);
            string spirv = Path.Combine(root, "source.spv");
            string metal = Path.Combine(root, "source.metal");
            string reflection = Path.Combine(root, "reflection.json");
            string air = Path.Combine(root, "source.air");
            string library = Path.Combine(root, "source.metallib");
            File.WriteAllBytes(spirv, Frontend.CompileIntermediate(request, includeResolver));
            Run(translator,[spirv,metal,request.Stage == ShaderStage.Vertex ? "vert" : "frag",request.EntryPoint,PlatformTarget,
                request.Options.BindingPolicy.ConstantBufferShift.ToString(System.Globalization.CultureInfo.InvariantCulture),request.Options.BindingPolicy.TextureShift.ToString(System.Globalization.CultureInfo.InvariantCulture),request.Options.BindingPolicy.SamplerShift.ToString(System.Globalization.CultureInfo.InvariantCulture)],root);
            Run(cross, [spirv, "--reflect", "--output", reflection], root);
            using (JsonDocument reflected = JsonDocument.Parse(File.ReadAllText(reflection))) {
                if (!reflected.RootElement.TryGetProperty("entryPoints", out JsonElement entryPoints) || entryPoints.GetArrayLength() == 0) { throw new InvalidOperationException("Metal intermediate reflection did not report a shader entry point."); }
            }
            string sdk = PlatformTarget == "macos" ? "macosx" : "iphoneos";
            string selectedSdk = Environment.GetEnvironmentVariable("HELENGINE_APPLE_SDK");
            if (!string.IsNullOrWhiteSpace(selectedSdk)) {
                if (PlatformTarget != "ios" || (selectedSdk != "iphoneos" && selectedSdk != "iphonesimulator")) { throw new InvalidOperationException("HELENGINE_APPLE_SDK may select iphoneos or iphonesimulator only for iOS builds."); }
                sdk = selectedSdk;
            }
            string sdkVersion = Run("/usr/bin/xcrun", ["--sdk", sdk, "--show-sdk-version"], root).Trim();
            if (sdkVersion != "26.2") { throw new InvalidOperationException("Selected Apple shader SDK differs from the validated toolchain lock: " + sdkVersion); }
            List<string> compile = ["--sdk", sdk, "metal", PlatformTarget == "macos" ? "-std=macos-metal2.3" : "-std=ios-metal2.3", request.Options.Optimize ? "-O3" : "-O0", "-c", metal, "-o", air];
            if (request.Options.GenerateDebugInfo) { compile.Add("-gline-tables-only"); }
            if (request.Options.TreatWarningsAsErrors) { compile.Add("-Werror"); }
            Run("/usr/bin/xcrun", compile, root);
            Run("/usr/bin/xcrun", ["--sdk", sdk, "metallib", air, "-o", library], root);
            byte[] result = File.ReadAllBytes(library);
            if (result.Length < 4 || result[0] != 'M' || result[1] != 'T' || result[2] != 'L' || result[3] != 'B') { throw new InvalidOperationException("Apple compiler output is not a Metal shader library."); }
            diagnostics = [];
            return result;
        }
    }

    /// <summary>Runs a native shader tool with structured arguments and captures both streams without hiding compiler errors.</summary>
    static string Run(string executable, IReadOnlyList<string> arguments, string root) {
        ProcessStartInfo start = new(executable) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, WorkingDirectory = root };
        foreach (string argument in arguments) { start.ArgumentList.Add(argument); }
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Could not start shader tool: " + executable);
        Task<string> stdout = process.StandardOutput.ReadToEndAsync();
        Task<string> stderr = process.StandardError.ReadToEndAsync();
        process.WaitForExit();
        string log = stdout.GetAwaiter().GetResult() + stderr.GetAwaiter().GetResult();
        File.WriteAllText(Path.Combine(root, "compiler.log"), log);
        if (process.ExitCode != 0) { throw new InvalidOperationException($"Metal shader tool failed ({process.ExitCode}): {executable}\n{log}"); }
        return stdout.GetAwaiter().GetResult();
    }
}

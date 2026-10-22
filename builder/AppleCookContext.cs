using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using helengine.baseplatform.Builders;

namespace helengine.apple.builder;

/// <summary>Captures native SDK identity once and validates its immutable use throughout one Apple build.</summary>
public static class AppleCookContext {
    /// <summary>Identifies the SDK revision verified by the pinned native toolchain.</summary>
    const string LockedSdkVersion = "26.2";
    /// <summary>Identifies loaded managed compiler producers without probing mutable SDK or native tool configuration.</summary>
    static string ManagedCompilerIdentity => "apple-metal-compiler-v1:" + typeof(MetalShaderBackend).Assembly.ManifestModule.ModuleVersionId.ToString("N") + ":" + typeof(helengine.vulkan.VulkanShaderBackend).Assembly.ManifestModule.ModuleVersionId.ToString("N") + ":" + typeof(helengine.ShaderBackendBase).Assembly.ManifestModule.ModuleVersionId.ToString("N") + ":";

    /// <summary>Reads the environment and real SDK version once before any cache lookup or authored shader compilation.</summary>
    public static PlatformCookContext Capture(string target) {
        string sdk = AppleSdkSelection.Resolve(target, Environment.GetEnvironmentVariable("HELENGINE_APPLE_SDK"));
        if (!OperatingSystem.IsMacOS()) { throw new PlatformNotSupportedException("Capturing the Apple cook SDK requires the actual macOS host and installed Xcode SDK."); }
        string version = RunCaptureTool(["--sdk", sdk, "--show-sdk-version"]);
        if (version != LockedSdkVersion) { throw new InvalidOperationException("Selected Apple SDK " + sdk + " differs from locked SDK " + LockedSdkVersion + ": " + version); }
        string metalVersion = RunCaptureTool(["--sdk", sdk, "metal", "--version"]).Split('\n', StringSplitOptions.RemoveEmptyEntries).First().Trim();
        string tools = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "dev/helworks/tools");
        string identity = CreateCompilerIdentity(metalVersion, HashFile("/tmp/helengine/macos/helengine_msl_translate"), HashFile(Path.Combine(tools, "spirv-cross/bin/spirv-cross")), HashFile(Path.Combine(tools, "shaderc/lib/libshaderc_shared.dylib")));
        return new(target, new PlatformNativeSdkSelection("apple-metal", sdk, version, identity));
    }

    /// <summary>Builds a stable identity from already captured native producer facts plus the loaded managed compiler implementations.</summary>
    public static string CreateCompilerIdentity(string metalVersion, string translatorHash, string crossHash, string shadercHash) {
        ArgumentException.ThrowIfNullOrWhiteSpace(metalVersion);
        foreach (string hash in new[] { translatorHash, crossHash, shadercHash }) {
            if (hash == null || hash.Length != 64 || !hash.All(Uri.IsHexDigit)) { throw new ArgumentException("Captured native compiler hashes must contain exactly 64 hexadecimal characters."); }
        }
        string inputs = JsonSerializer.Serialize(new { MetalVersion = metalVersion, Translator = translatorHash, CrossReflection = crossHash, Shaderc = shadercHash });
        return ManagedCompilerIdentity + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(inputs))).ToLowerInvariant();
    }

    /// <summary>Hashes the actual native tool file consumed by the shader pipeline, including locally rebuilt translator implementations.</summary>
    static string HashFile(string path) {
        using FileStream stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }

    /// <summary>Queries the selected installed toolchain only during capture and preserves command failures and timeouts.</summary>
    static string RunCaptureTool(IReadOnlyList<string> arguments) {
        ProcessStartInfo start = new("/usr/bin/xcrun") { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (string argument in arguments) { start.ArgumentList.Add(argument); }
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Could not query the selected Apple SDK.");
        Task<string> output = process.StandardOutput.ReadToEndAsync();
        Task<string> error = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(30000)) { process.Kill(true); throw new TimeoutException("Timed out querying the selected Apple toolchain."); }
        string result = output.GetAwaiter().GetResult().Trim();
        string diagnostic = error.GetAwaiter().GetResult();
        if (process.ExitCode != 0) { throw new InvalidOperationException("Cannot capture Apple toolchain identity: " + diagnostic); }
        return result;
    }

    /// <summary>Requires the captured target, SDK revision and compiler to match without consulting mutable environment state.</summary>
    public static PlatformNativeSdkSelection Require(string target, PlatformCookContext context) {
        if (context == null) { throw new ArgumentNullException(nameof(context), "Apple cooking and native compilation require a captured PlatformCookContext."); }
        PlatformNativeSdkSelection sdk = context.NativeSdk;
        if (context.PlatformId != target || sdk == null) { throw new InvalidOperationException("Apple target " + target + " requires its captured native SDK context."); }
        bool nameMatches = target == "macos" ? sdk.Name == "macosx" : target == "ios" && (sdk.Name == "iphoneos" || sdk.Name == "iphonesimulator");
        string prefix = ManagedCompilerIdentity;
        bool identityMatches = sdk.CompilerIdentity.StartsWith(prefix, StringComparison.Ordinal) && sdk.CompilerIdentity.Length == prefix.Length + 64 && sdk.CompilerIdentity[prefix.Length..].All(Uri.IsHexDigit);
        if (sdk.ToolchainId != "apple-metal" || sdk.Version != LockedSdkVersion || !identityMatches || !nameMatches) {
            throw new InvalidOperationException("Apple target " + target + " requires a compatible captured Metal SDK/context and the current shared compiler identity.");
        }
        return sdk;
    }
}

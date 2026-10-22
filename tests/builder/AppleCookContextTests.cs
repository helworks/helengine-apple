using helengine.apple.builder;
using helengine.baseplatform.Builders;
using helengine;

/// <summary>Requires one captured immutable SDK selection for shader compilation, cache identity and native staging.</summary>
public static class AppleCookContextTests {
    /// <summary>Creates an explicit SDK value fixture; it does not probe or claim execution on the selected device.</summary>
    public static PlatformCookContext Create(string target, string sdk) {
        return new(target, new PlatformNativeSdkSelection("apple-metal", sdk, "26.2", AppleCookContext.CreateCompilerIdentity("explicit-test-metal-version", new string('0',64), new string('0',64), new string('0',64))));
    }

    /// <summary>Proves SDK changes affect the cache while environment mutation cannot alter an already captured backend/native selection.</summary>
    public static void Run() {
        PlatformCookContext device = Create("ios", "iphoneos");
        PlatformCookContext simulator = Create("ios", "iphonesimulator");
        if (device.CacheIdentity == simulator.CacheIdentity) { throw new Exception("Different native shader SDKs share a cook cache identity."); }
        PlatformCookContext compilerChanged = new("ios", new PlatformNativeSdkSelection("apple-metal", "iphoneos", "26.2", AppleCookContext.CreateCompilerIdentity("different-metal-version", new string('0',64), new string('0',64), new string('0',64))));
        if (device.CacheIdentity == compilerChanged.CacheIdentity) { throw new Exception("A changed native compiler version retained the old cook cache identity."); }
        PlatformCookContext translatorChanged = new("ios", new PlatformNativeSdkSelection("apple-metal", "iphoneos", "26.2", AppleCookContext.CreateCompilerIdentity("explicit-test-metal-version", new string('1',64), new string('0',64), new string('0',64))));
        if (device.CacheIdentity == translatorChanged.CacheIdentity) { throw new Exception("A changed native translator retained the old cook cache identity."); }
        string original = Environment.GetEnvironmentVariable("HELENGINE_APPLE_SDK");
        try {
            Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", "iphonesimulator");
            MetalShaderBackend backend = new("ios", device);
            if (!ReferenceEquals(backend.CookContext, device) || AppleCookContext.Require("ios", backend.CookContext).Name != "iphoneos") { throw new Exception("Shader/native selection reread mutable environment instead of the captured SDK."); }
            Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", "iphoneos");
            if (AppleCookContext.Require("ios", simulator).Name != "iphonesimulator") { throw new Exception("The captured simulator context changed after an environment update."); }
        } finally { Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", original); }
        bool missingRejected = false;
        try { new MetalShaderBackend("ios", null); }
        catch (ArgumentNullException) { missingRejected = true; }
        if (!missingRejected) { throw new Exception("Missing native shader SDK context was accepted."); }
        foreach (PlatformCookContext invalid in new[] {
            new PlatformCookContext("ios"), Create("macos", "macosx"), Create("ios", "macosx"),
            new PlatformCookContext("ios", new PlatformNativeSdkSelection("other", "iphoneos", "26.2", "other")),
            new PlatformCookContext("ios", new PlatformNativeSdkSelection("apple-metal", "iphoneos", "26.2", "other")),
            new PlatformCookContext("ios", new PlatformNativeSdkSelection("apple-metal", "iphoneos", "27.0", device.NativeSdk.CompilerIdentity))
        }) {
            bool rejected = false;
            try { new MetalShaderBackend("ios", invalid); }
            catch (InvalidOperationException) { rejected = true; }
            if (!rejected) { throw new Exception("Incompatible Apple cook context was accepted."); }
        }
    }

    /// <summary>Compiles real debug Metal libraries after mutating the environment and checks the captured SDK's binary provenance.</summary>
    public static void RunNativeFrozenSdk() {
        string original = Environment.GetEnvironmentVariable("HELENGINE_APPLE_SDK");
        try {
            foreach (string sdk in new[] { "iphoneos", "iphonesimulator" }) {
                Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", sdk);
                PlatformCookContext context = AppleCookContext.Capture("ios");
                Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", sdk == "iphoneos" ? "iphonesimulator" : "iphoneos");
                MetalShaderBackend backend = new("ios", context);
                ShaderCompileRequest request = new(new ShaderSourceInfo("captured-sdk.hlsl", "float4 PSMain() : SV_Target { return float4(1,0,0,1); }"), "PSMain", "PSMain", ShaderStage.Pixel, ShaderCompileTarget.Metal, new ShaderModel(5,0), "default", [], new ShaderCompileOptions(new ShaderBindingPolicy(0,0,100,200,300), false, true, true));
                var result = backend.Compile(request, new ShaderFilesystemIncludeResolver(Directory.GetCurrentDirectory()));
                if (!result.Success) { throw new Exception("Actual captured-SDK shader compilation failed."); }
                string binary = System.Text.Encoding.ASCII.GetString(result.Binary.Bytecode);
                string expected = sdk == "iphoneos" ? "iPhoneOS26.2.sdk" : "iPhoneSimulator26.2.sdk";
                string unexpected = sdk == "iphoneos" ? "iPhoneSimulator26.2.sdk" : "iPhoneOS26.2.sdk";
                if (!binary.Contains(expected, StringComparison.Ordinal) || binary.Contains(unexpected, StringComparison.Ordinal)) { throw new Exception("Metal compilation reread SDK environment after immutable context capture: " + sdk); }
                Console.WriteLine(sdk + ": actual Metal compilation retained captured SDK after environment mutation");
            }
        } finally { Environment.SetEnvironmentVariable("HELENGINE_APPLE_SDK", original); }
    }
}

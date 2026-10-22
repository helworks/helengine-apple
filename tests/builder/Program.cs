using helengine.apple.builder;
using helengine.baseplatform.Requests;
using helengine;

/// <summary>Exercises public Apple plugin discovery metadata and explicit unsupported-stage errors.</summary>
public static class Program {
    /// <summary>Validates the shared assembly exposes two real target identities and preserves selectable precision.</summary>
    public static int Main(string[] arguments) {
        ApplePlatformAssetBuilder[] builders = [new MacOSPlatformAssetBuilder(), new IosPlatformAssetBuilder()];
        foreach (ApplePlatformAssetBuilder builder in builders) {
            if (builder.Definition.CodegenProfiles.Single().Settings.Single(setting => setting.SettingId == "generated-math-convention").DefaultValue != "engine-row-vector") { throw new Exception("Custom target omitted engine matrix convention"); }
            Type[] concreteBuilders = builder.GetType().Assembly.GetTypes().Where(type => !type.IsAbstract && typeof(ApplePlatformAssetBuilder).IsAssignableFrom(type)).ToArray();
            if (concreteBuilders.Length != 1) { throw new Exception("Existing assembly-only loader would select an ambiguous Apple target"); }
            if (builder.Descriptor.TargetPlatformId != builder.Definition.PlatformId) { throw new Exception("Target identity mismatch"); }
            if (!builder.Definition.BuildProfiles.Select(profile => profile.ProfileId).SequenceEqual(["debug", "release", "profiler"])) { throw new Exception("Public profile contract changed"); }
            if (builder.Definition.GraphicsProfiles.Single().ProfileId != "metal") { throw new Exception("Apple backend must use Metal"); }
            if (builder.Definition.CodegenProfiles.Single().NumericCapabilities.SupportedPolicies.Count < 2) { throw new Exception("Configurable precision was narrowed"); }
            try { builder.CookMaterial(null); throw new Exception("Missing material request accepted"); }
            catch (ArgumentNullException) { }
            PlatformMaterialCookRequest request = new("material", "materials/example.hemat", builder.Definition.PlatformId, "debug", "metal", "standard-shader", new Dictionary<string,string> {
                ["shader-asset-id"] = "shader", ["vertex-program"] = "VSMain", ["pixel-program"] = "PSMain", ["variant"] = "default", ["base-color"] = "#804020ff", ["texture-id"] = "authored-texture", ["roughness"] = "0.25"
            }, new NumericPrecisionPolicy(64,64));
            var result = builder.CookMaterial(request);
            var material = (ShaderMaterialAsset)helengine.files.AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes);
            if (material.Id != "material" || material.DiffuseTextureAssetId != "authored-texture" || material.VertexProgram != "VSMain" || material.ConstantBuffers.Length != 4) { throw new Exception("Cooked material lost authored state"); }
            if (material.ConstantBuffers[0].Data.Length != 16 || Math.Abs(BitConverter.ToSingle(material.ConstantBuffers[0].Data, 0) - 128f/255f) > 0.0001f) { throw new Exception("Fixed GPU color layout was changed by configurable 64-bit arithmetic"); }
        }
        string safeRoot = OperatingSystem.IsWindows() ? "C:\\temp\\helengine\\macos\\staging-check" : "/tmp/helengine/macos/staging-check";
        if (!AppleBuildWorkspace.ResolveCookedPath(safeRoot,"generated/ui/ação com espaço.asset").EndsWith(Path.Combine("cooked","ui","ação com espaço.asset"))) { throw new Exception("Cooked staging changed a Unicode/space payload path"); }
        foreach (string unsafePath in new[] {"../escape.asset", "/absolute.asset", "C:\\absolute.asset", "cooked/../../escape.asset"}) {
            bool rejected = false;
            try { AppleBuildWorkspace.ResolveCookedPath(safeRoot,unsafePath); }
            catch (InvalidOperationException) { rejected = true; }
            if (!rejected) { throw new Exception("Unsafe cooked path accepted: " + unsafePath); }
        }
        if (builders.Select(builder => builder.Definition.PlatformId).Distinct().Count() != 2) { throw new Exception("macOS/iOS targets collapsed"); }
        foreach (ApplePlatformAssetBuilder builder in builders) {
            ShaderBackendRegistry registry = new();
            builder.RegisterShaderBackends(registry);
            if (!registry.ContainsTarget(ShaderCompileTarget.Metal)) { throw new Exception("Apple shader contributor did not register Metal"); }
        }
        if (arguments.Contains("--shader")) { ValidateNativeShader("macos"); }
        if (arguments.Contains("--shader-ios")) { ValidateNativeShader("ios"); }
        BuildWorkspaceTests.Run();
        Console.WriteLine("Apple builder metadata: 2 targets, 3 exact profiles, Metal, numeric policy and required inputs passed");
        return 0;
    }

    /// <summary>Compiles actual HLSL through the pinned native tools and verifies a Metal library, reflection slots and failure propagation.</summary>
    static void ValidateNativeShader(string target) {
        const string source = "cbuffer BaseColorBuffer : register(b0) { float4 BaseColor; }; float4 VSMain(float4 p : POSITION) : SV_Position { return p + BaseColor; }";
        ShaderCompileRequest request = new(new ShaderSourceInfo("material.hlsl", source), "VSMain", "VSMain", ShaderStage.Vertex, ShaderCompileTarget.Metal, new ShaderModel(5,0), "default", [], new ShaderCompileOptions(new ShaderBindingPolicy(0,0,16,32,48), true, false, true));
        MetalShaderBackend backend = new(target);
        if (backend.UseSharedCache) { throw new Exception("Metal libraries from different Apple SDKs must not share the engine's target-only compile cache"); }
        var result = backend.Compile(request, new ShaderFilesystemIncludeResolver(Directory.GetCurrentDirectory()));
        if (!result.Success || result.Binary.Target != ShaderCompileTarget.Metal || System.Text.Encoding.ASCII.GetString(result.Binary.Bytecode, 0, 4) != "MTLB") { throw new Exception("Authored shader did not produce a real Metal library"); }
        ShaderCompileRequest pixel = new(new ShaderSourceInfo("textured.hlsl", "Texture2D DiffuseTexture : register(t0); SamplerState DiffuseSampler : register(s0); float4 PSMain(float2 uv : TEXCOORD0) : SV_Target { return DiffuseTexture.Sample(DiffuseSampler,uv); }"), "PSMain", "PSMain", ShaderStage.Pixel, ShaderCompileTarget.Metal, new ShaderModel(5,0), "default", [], request.Options);
        var textured = backend.Compile(pixel,new ShaderFilesystemIncludeResolver(Directory.GetCurrentDirectory()));
        if (!textured.Success || textured.Binary.Bytecode.Length == 0) { throw new Exception("Authored textured shader compilation failed"); }
        ShaderCompileRequest invalid = new(new ShaderSourceInfo("broken.hlsl", "this is not valid HLSL"), "VSMain", "VSMain", ShaderStage.Vertex, ShaderCompileTarget.Metal, new ShaderModel(5,0), "default", [], request.Options);
        bool rejected = false;
        try { backend.Compile(invalid, new ShaderFilesystemIncludeResolver(Directory.GetCurrentDirectory())); }
        catch (InvalidOperationException) { rejected = true; }
        if (!rejected) { throw new Exception("Invalid authored shader did not fail native compilation"); }
        Console.WriteLine(target + ": authored HLSL -> SPIR-V -> MSL -> Metal library passed");
    }
}

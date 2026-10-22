using helengine.apple.builder;
using helengine.baseplatform.Requests;
using helengine;

/// <summary>Exercises public Apple plugin discovery metadata and explicit unsupported-stage errors.</summary>
public static class Program {
    /// <summary>Validates the shared assembly exposes two real target identities and preserves selectable precision.</summary>
    public static int Main(string[] arguments) {
        ApplePlatformAssetBuilder[] builders = [new MacOSPlatformAssetBuilder(), new IosPlatformAssetBuilder()];
        foreach (ApplePlatformAssetBuilder builder in builders) {
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
        if (builders.Select(builder => builder.Definition.PlatformId).Distinct().Count() != 2) { throw new Exception("macOS/iOS targets collapsed"); }
        foreach (ApplePlatformAssetBuilder builder in builders) {
            ShaderBackendRegistry registry = new();
            builder.RegisterShaderBackends(registry);
            if (!registry.ContainsTarget(ShaderCompileTarget.Metal)) { throw new Exception("Apple shader contributor did not register Metal"); }
        }
        if (arguments.Contains("--shader")) { ValidateNativeShader(); }
        Console.WriteLine("Apple builder metadata: 2 targets, 3 exact profiles, Metal, numeric policy and required inputs passed");
        return 0;
    }

    /// <summary>Compiles actual HLSL through the pinned native tools and verifies a Metal library, reflection slots and failure propagation.</summary>
    static void ValidateNativeShader() {
        const string source = "cbuffer BaseColorBuffer : register(b0) { float4 BaseColor; }; float4 VSMain(float4 p : POSITION) : SV_Position { return p + BaseColor; }";
        ShaderCompileRequest request = new(new ShaderSourceInfo("material.hlsl", source), "VSMain", "VSMain", ShaderStage.Vertex, ShaderCompileTarget.Metal, new ShaderModel(5,0), "default", [], new ShaderCompileOptions(new ShaderBindingPolicy(0,0,16,32,48), true, false, true));
        MetalShaderBackend backend = new("macos");
        var result = backend.Compile(request, new ShaderFilesystemIncludeResolver(Directory.GetCurrentDirectory()));
        if (!result.Success || result.Binary.Target != ShaderCompileTarget.Metal || System.Text.Encoding.ASCII.GetString(result.Binary.Bytecode, 0, 4) != "MTLB") { throw new Exception("Authored shader did not produce a real Metal library"); }
        Console.WriteLine("Authored HLSL -> SPIR-V -> MSL -> Metal library passed");
    }
}

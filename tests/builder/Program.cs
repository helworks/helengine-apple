using helengine.apple.builder;

/// <summary>Exercises public Apple plugin discovery metadata and explicit unsupported-stage errors.</summary>
public static class Program {
    /// <summary>Validates the shared assembly exposes two real target identities and preserves selectable precision.</summary>
    public static int Main() {
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
        }
        if (builders.Select(builder => builder.Definition.PlatformId).Distinct().Count() != 2) { throw new Exception("macOS/iOS targets collapsed"); }
        Console.WriteLine("Apple builder metadata: 2 targets, 3 exact profiles, Metal, numeric policy and required inputs passed");
        return 0;
    }
}

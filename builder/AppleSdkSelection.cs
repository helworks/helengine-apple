namespace helengine.apple.builder;

/// <summary>Keeps native application and authored shader compilation on the same selected Apple SDK.</summary>
public static class AppleSdkSelection {
    /// <summary>Resolves approved defaults and rejects SDK overrides incompatible with the requested platform.</summary>
    public static string Resolve(string target, string configuredSdk) {
        if (target != "macos" && target != "ios") { throw new ArgumentException("Apple target must be macos or ios.", nameof(target)); }
        if (string.IsNullOrWhiteSpace(configuredSdk)) { return target == "macos" ? "macosx" : "iphoneos"; }
        if (target != "ios" || (configuredSdk != "iphoneos" && configuredSdk != "iphonesimulator")) { throw new InvalidOperationException("HELENGINE_APPLE_SDK may select iphoneos or iphonesimulator only for iOS builds."); }
        return configuredSdk;
    }
}

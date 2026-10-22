namespace helengine.apple.builder;

/// <summary>Registers macOS from a thin assembly while sharing the renderer and builder implementation with iOS.</summary>
public sealed class MacOSPlatformAssetBuilder : ApplePlatformAssetBuilder {
    /// <summary>Creates macOS ARM64 metadata for native Metal builds.</summary>
    public MacOSPlatformAssetBuilder() : base("macos") { }
}

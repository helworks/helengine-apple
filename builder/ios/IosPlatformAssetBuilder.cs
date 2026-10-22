namespace helengine.apple.builder;

/// <summary>Registers iOS from a thin assembly without representing unsigned compilation as device deployment.</summary>
public sealed class IosPlatformAssetBuilder : ApplePlatformAssetBuilder {
    /// <summary>Creates iOS ARM64 metadata for the shared native Metal renderer.</summary>
    public IosPlatformAssetBuilder() : base("ios") { }
}

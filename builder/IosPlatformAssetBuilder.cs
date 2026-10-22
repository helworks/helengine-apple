namespace helengine.apple.builder;

/// <summary>Registers the iOS target without representing unsigned compilation as device deployment.</summary>
public sealed class IosPlatformAssetBuilder : ApplePlatformAssetBuilder {
    /// <summary>Creates iOS ARM64 metadata for the shared native Metal renderer.</summary>
    public IosPlatformAssetBuilder() : base("ios") { }
}

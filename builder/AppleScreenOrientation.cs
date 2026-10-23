namespace helengine.apple.builder;

/// <summary>Validates the authored iOS orientation independently of desktop window dimensions.</summary>
public static class AppleScreenOrientation {
    /// <summary>Preserves existing free rotation when absent and rejects values the iOS host cannot advertise consistently.</summary>
    /// <param name="graphicsOptions">Resolved graphics-profile choices from the platform build request.</param>
    /// <returns>The exact orientation selection forwarded to native bundle configuration.</returns>
    public static string Resolve(IReadOnlyDictionary<string,string> graphicsOptions) {
        if (!graphicsOptions.TryGetValue("screen-orientation", out string orientation)) return "unspecified";
        return orientation switch {
            "unspecified" or "portrait" or "landscape" => orientation,
            _ => throw new InvalidOperationException("iOS screen-orientation must be unspecified, portrait or landscape.")
        };
    }
}

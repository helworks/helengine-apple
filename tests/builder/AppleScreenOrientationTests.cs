using helengine.apple.builder;

/// <summary>Checks explicit iOS orientation selection without deriving behavior from display dimensions.</summary>
public static class AppleScreenOrientationTests {
    /// <summary>Preserves rotation by default and accepts only the declared authored choices.</summary>
    public static void Run() {
        var setting = ApplePlatformDefinitionFactory.Create("ios").GraphicsProfiles.Single().Settings.Single(value => value.SettingId == "screen-orientation");
        if (setting.DefaultValue != "unspecified" || !setting.AllowedValues.SequenceEqual(["unspecified", "portrait", "landscape"])) { throw new Exception("iOS orientation choices or compatible default changed."); }
        if (ApplePlatformDefinitionFactory.Create("macos").GraphicsProfiles.Single().Settings.Any(value => value.SettingId == "screen-orientation")) { throw new Exception("iOS orientation option leaked into macOS."); }
        if (AppleScreenOrientation.Resolve(new Dictionary<string,string>()) != "unspecified") { throw new Exception("Existing iOS projects must retain free rotation."); }
        foreach (string orientation in new[] { "unspecified", "portrait", "landscape" }) {
            if (AppleScreenOrientation.Resolve(new Dictionary<string,string> { ["screen-orientation"] = orientation, ["default-width"] = "1280", ["default-height"] = "720" }) != orientation) { throw new Exception("iOS ignored explicit orientation."); }
        }
        bool rejected = false;
        try { AppleScreenOrientation.Resolve(new Dictionary<string,string> { ["screen-orientation"] = "sideways" }); }
        catch (InvalidOperationException) { rejected = true; }
        if (!rejected) { throw new Exception("Unknown iOS orientation must fail before native configuration."); }
    }
}

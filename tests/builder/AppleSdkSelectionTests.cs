using helengine.apple.builder;

/// <summary>Verifies shader compilation and native bundle staging select the same real Apple SDK.</summary>
public static class AppleSdkSelectionTests {
    /// <summary>Preserves approved defaults, accepts both iOS SDKs and rejects incompatible overrides before native tools run.</summary>
    public static void Run() {
        if (AppleSdkSelection.Resolve("macos", null) != "macosx" || AppleSdkSelection.Resolve("ios", null) != "iphoneos") { throw new Exception("Apple SDK defaults changed."); }
        foreach (string sdk in new[] { "iphoneos", "iphonesimulator" }) {
            if (AppleSdkSelection.Resolve("ios", sdk) != sdk) { throw new Exception("Explicit iOS SDK was not preserved."); }
        }
        foreach (string target in new[] { "macos", "ios" }) {
            bool rejected = false;
            try { AppleSdkSelection.Resolve(target, "macosx"); }
            catch (InvalidOperationException) { rejected = true; }
            if (!rejected) { throw new Exception("Invalid Apple SDK override was accepted for " + target); }
        }
    }
}

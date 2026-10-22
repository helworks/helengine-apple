using helengine.apple.builder;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

/// <summary>Rejects missing or externally staged incompatible SDK provenance before Apple staging creates any output.</summary>
public sealed class AppleCookContextStagingTests : IPlatformBuildProgressReporter, IPlatformBuildDiagnosticReporter {
    /// <summary>Exercises the public native builder boundary with valid request context and mismatched cooked manifests.</summary>
    public static void Run() {
        string root = Path.Combine(OperatingSystem.IsWindows() ? "C:/temp/helengine/ios" : "/tmp/helengine/ios", "context-reject-test");
        if (Directory.Exists(root)) { throw new Exception("Context rejection fixture must not overwrite existing files: " + root); }
        PlatformCookContext device = AppleCookContextTests.Create("ios", "iphoneos");
        AppleCookContextStagingTests reporter = new();
        foreach (PlatformCookContext manifestContext in new[] { null, AppleCookContextTests.Create("ios", "iphonesimulator") }) {
            PlatformBuildManifest manifest = new(1,"project","1.0.0","1.0.0","ios","1.0.0","",[],[],[],[],[],new("ios-loose-files",[]),[],PlatformBuildRuntimeFeatureManifest.Empty);
            manifest.CookContext = manifestContext;
            PlatformBuildRequest request = new(device, manifest, [new("ios","ios","metal","debug")], [new("debug","Debug",new("metal","rgba8","pcm","hele",PlatformSerializationEndianness.LittleEndian))], root, root, "debug", "metal", "default", new Dictionary<string,string>(), new Dictionary<string,string>(), new Dictionary<string,string>(), executionScope: PlatformBuildExecutionScope.AssetsOnly);
            Exception failure = null;
            try { new IosPlatformAssetBuilder().BuildAsync(request, reporter, reporter, CancellationToken.None).GetAwaiter().GetResult(); }
            catch (Exception error) { failure = error; }
            if (failure is not InvalidOperationException || !failure.Message.Contains("manifest", StringComparison.OrdinalIgnoreCase)) { throw new Exception("Apple accepted missing or mismatched cooked SDK provenance.", failure); }
            if (Directory.Exists(root)) { throw new Exception("Invalid SDK provenance started staging before rejection."); }
        }
    }
    /// <summary>Consumes progress only if a regression starts staging before context validation.</summary>
    public void Report(PlatformBuildProgressUpdate update) { }
    /// <summary>Preserves unexpected native diagnostics as test failures.</summary>
    public void Report(PlatformBuildDiagnostic diagnostic) { throw new InvalidOperationException(diagnostic.Message); }
}

using helengine.apple.builder;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Profiles;

/// <summary>Exercises real graph artifact staging, including assets which are neither scenes nor loose assets.</summary>
public sealed class BuildWorkspaceTests : IPlatformBuildProgressReporter, IPlatformBuildDiagnosticReporter {
    /// <summary>Rejects silently dropped reflected shader/font artifacts and verifies missing files fail.</summary>
    public static void Run() {
        string root = OperatingSystem.IsWindows() ? "C:\\temp\\helengine\\macos\\staging-test" : "/tmp/helengine/macos/staging-test";
        string staged = Path.Combine(root,"staged");
        string output = Path.Combine(root,"output");
        Directory.CreateDirectory(Path.Combine(staged,"generated","fonts"));
        File.WriteAllText(Path.Combine(staged,"generated","fonts","ação com espaço.font"),"real cooked artifact bytes");
        PlatformBuildManifest manifest = new(1,"project","1.0.0","1.0.0","macos","1.0.0","",[],[],
            [new("generated/fonts/ação com espaço.font","font-artifact","hash","font","default")],[],[],new("macos-loose-files",[]),[],PlatformBuildRuntimeFeatureManifest.Empty);
        manifest.CookContext = AppleCookContextTests.Create("macos", "macosx");
        PlatformBuildRequest request = new(AppleCookContextTests.Create("macos", "macosx"),manifest,[new("macos","macos","macos","debug")],[new("debug","Debug",new("metal","rgba8","pcm","hele",PlatformSerializationEndianness.LittleEndian))],output,root,"debug","metal","default",new Dictionary<string,string>(),new Dictionary<string,string>(),new Dictionary<string,string>(),executionScope: PlatformBuildExecutionScope.AssetsOnly);
        string original = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(staged);
            BuildWorkspaceTests reporter = new();
            var report = new MacOSPlatformAssetBuilder().BuildAsync(request,reporter,reporter,CancellationToken.None).GetAwaiter().GetResult();
            if (!report.Succeeded || !File.Exists(Path.Combine(output,"cooked","fonts","ação com espaço.font"))) { throw new Exception("Graph-only cooked font artifact was silently omitted"); }
            File.Delete(Path.Combine(staged,"generated","fonts","ação com espaço.font"));
            bool missingRejected = false;
            try { new MacOSPlatformAssetBuilder().BuildAsync(request,reporter,reporter,CancellationToken.None).GetAwaiter().GetResult(); }
            catch (FileNotFoundException) { missingRejected = true; }
            if (!missingRejected) { throw new Exception("Missing graph artifact did not fail staging"); }
        } finally {
            Directory.SetCurrentDirectory(original);
            Directory.Delete(root,true);
        }
    }

    /// <summary>Consumes progress updates while exercising the real staging code.</summary>
    public void Report(PlatformBuildProgressUpdate update) { }
    /// <summary>Turns unexpected staging diagnostics into test failures.</summary>
    public void Report(PlatformBuildDiagnostic diagnostic) { throw new Exception(diagnostic.Message); }
}

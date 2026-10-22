using System.Diagnostics;
using System.Text.Json;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;

namespace helengine.apple.builder;

/// <summary>Stages actual graph payloads and invokes the real Apple CMake player build without substituting a diagnostic.</summary>
public static class AppleBuildWorkspace {
    /// <summary>Validates ABI and target inputs, preserves all scene IDs, stages assets, and propagates native compiler failures.</summary>
    public static async Task<PlatformBuildReport> BuildAsync(string target, PlatformBuildRequest request, IPlatformBuildProgressReporter progress, IPlatformBuildDiagnosticReporter diagnostics, CancellationToken cancellationToken) {
        if (request == null) { throw new ArgumentNullException(nameof(request)); }
        else if (progress == null) { throw new ArgumentNullException(nameof(progress)); }
        else if (diagnostics == null) { throw new ArgumentNullException(nameof(diagnostics)); }
        PlatformDefinition definition = ApplePlatformDefinitionFactory.Create(target);
        definition.CodegenProfiles.Single().NumericCapabilities.RequireSupported(request.NumericPolicy);
        if (request.Manifest.PlatformName != target) { throw new InvalidOperationException("Apple build manifest target differs from the selected builder."); }
        if (request.NumericPolicy.Identity != request.Manifest.NumericPolicy.Identity) { throw new InvalidOperationException("Apple request and content manifest have different numeric ABI policies."); }
        foreach (var module in request.Manifest.CodeModules) { module.RequireNumericPolicy(request.NumericPolicy); }
        string profile = request.SelectedBuildProfileId switch {
            "debug" => "Debug", "release" => "Release", "profiler" => "Profiler", _ => throw new ArgumentException("Apple build profile must be debug, release or profiler.")
        };
        if (profile == "Profiler" && target == "ios") { throw new NotSupportedException("iOS Profiler requires a verified native runtime capture."); }
        cancellationToken.ThrowIfCancellationRequested();
        string staged = Directory.GetCurrentDirectory();
        string output = Path.GetFullPath(request.OutputRoot);
        Directory.CreateDirectory(output);
        if (Directory.Exists(Path.Combine(staged, "cooked"))) { CopyTree(Path.Combine(staged, "cooked"), Path.Combine(output, "cooked"), cancellationToken); }
        List<PlatformBuildItemOutcome> scenes = [];
        List<PlatformBuildItemOutcome> assets = [];
        foreach (var scene in request.Manifest.Scenes) {
            CopyPayload(staged, output, scene.SourceIdentity, cancellationToken);
            scenes.Add(new(scene.SceneId, PlatformBuildItemOutcomeKind.Succeeded));
        }
        foreach (var asset in request.Manifest.LooseAssets) {
            CopyPayload(staged, output, asset.SourceIdentity, cancellationToken);
            assets.Add(new(asset.AssetId, PlatformBuildItemOutcomeKind.Succeeded));
        }
        foreach (var artifact in request.Manifest.CookedArtifacts) {
            CopyPayload(staged,output,artifact.RelativePath,cancellationToken);
        }
        foreach (var work in request.Manifest.PlatformCookWorkItems) {
            if (work.TargetPlatformId != target) { throw new InvalidOperationException("Apple cook work item belongs to a different target: " + work.WorkItemId); }
            string source = Path.GetFullPath(work.SourceAssetPath.Replace('\\', Path.DirectorySeparatorChar), staged);
            string destination = ResolveCookedPath(output, work.OutputRelativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(source, destination, true);
        }
        string manifestPath = Path.Combine(output, "build-manifest.json");
        await File.WriteAllTextAsync(manifestPath, JsonSerializer.Serialize(request.Manifest, new JsonSerializerOptions { WriteIndented = true }), cancellationToken);
        progress.Report(new PlatformBuildProgressUpdate("Stage Payloads", target, scenes.Count + assets.Count, scenes.Count + assets.Count, "Staged all requested scenes and assets; no backend filtering was applied."));
        if (request.ExecutionScope == PlatformBuildExecutionScope.AssetsOnly) { return new(true, [], [.. scenes], [.. assets]); }
        if (!OperatingSystem.IsMacOS()) { throw new PlatformNotSupportedException($"{target}: payload staging completed, but native Apple compilation requires a real macOS host with the selected Xcode SDK."); }
        if (!Directory.Exists(request.GeneratedCoreCppRootPath)) { throw new DirectoryNotFoundException("Generated Apple core C++ is required: " + request.GeneratedCoreCppRootPath); }
        string code = Path.Combine(staged, "code");
        if (!Directory.Exists(code)) { throw new DirectoryNotFoundException("Generated project C++ is required: " + code); }
        string repository = ResolveRepositoryRoot();
        string native = Path.Combine("/tmp/helengine", target);
        string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string cmake = Path.Combine(home, "dev/helworks/tools/cmake-3.31.12-macos-universal/CMake.app/Contents/bin/cmake");
        List<string> configure = ["-S", repository, "-B", native, "-DHELENGINE_DIAGNOSTIC_ONLY=OFF", "-DHELENGINE_CORE_CPP_ROOT=" + Path.GetFullPath(request.GeneratedCoreCppRootPath), "-DHELENGINE_CODE_ROOT=" + code, "-DHELENGINE_CONTENT_ROOT=" + output, "-DHELENGINE_NATIVE_PROFILE=" + profile, "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_BUILD_TYPE=" + profile];
        foreach (string dimension in new[] { "width", "height" }) {
            if (!request.SelectedGraphicsOptionValues.TryGetValue("default-" + dimension, out string value) || !int.TryParse(value, out int pixels) || pixels < 1 || pixels > 16384) { throw new InvalidOperationException("Apple selected graphics option default-" + dimension + " must contain a pixel count between 1 and 16384."); }
            configure.Add("-DHELENGINE_APPLE_DEFAULT_" + dimension.ToUpperInvariant() + "=" + pixels.ToString(System.Globalization.CultureInfo.InvariantCulture));
        }
        if (target == "ios") { configure.AddRange(["-G", "Xcode", "-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_OSX_SYSROOT=iphoneos", "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0", "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO"]); }
        await RunAsync(cmake, configure, native, "player-configure.log", cancellationToken);
        await RunAsync(cmake, ["--build", native, "--config", profile, "-j", "6"], native, "player-build.log", cancellationToken);
        string sourceBundle = Path.Combine(native, target == "ios" ? profile + "-iphoneos" : "", "helengine_" + target + ".app");
        if (!Directory.Exists(sourceBundle)) { throw new DirectoryNotFoundException("The native build did not produce its actual application bundle: " + sourceBundle); }
        string bundle = Path.GetFullPath(Path.Combine(output, "helengine_" + target + ".app"));
        if (!bundle.StartsWith(output + Path.DirectorySeparatorChar, StringComparison.Ordinal)) { throw new InvalidOperationException("Apple application bundle escaped the selected output root."); }
        if (Directory.Exists(bundle)) { Directory.Delete(bundle, true); }
        CopyBundleTree(sourceBundle, bundle, cancellationToken);
        string resources = target == "macos" ? Path.Combine(bundle, "Contents", "Resources") : bundle;
        CopyBundleTree(Path.Combine(output, "cooked"), Path.Combine(resources, "cooked"), cancellationToken);
        return new(true, [], [.. scenes], [.. assets]);
    }

    /// <summary>Resolves a runtime-relative payload path and rejects absolute, drive, traversal and output-root escape forms.</summary>
    public static string ResolveCookedPath(string outputRoot, string sourceIdentity) {
        if (string.IsNullOrWhiteSpace(sourceIdentity)) { throw new ArgumentException("A nonempty payload identity is required.", nameof(sourceIdentity)); }
        string relative = sourceIdentity.Replace('\\', '/');
        if (relative.StartsWith('/') || relative.Contains(':') || relative.Split('/').Any(segment => segment is ".." or ".")) { throw new InvalidOperationException("Cooked payload path must remain relative to the bundle: " + sourceIdentity); }
        if (relative.StartsWith("generated/", StringComparison.Ordinal)) { relative = relative[10..]; }
        if (!relative.StartsWith("cooked/", StringComparison.Ordinal)) { relative = "cooked/" + relative; }
        string root = Path.GetFullPath(outputRoot) + Path.DirectorySeparatorChar;
        string result = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        if (!result.StartsWith(root, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)) { throw new InvalidOperationException("Cooked payload escapes output root: " + sourceIdentity); }
        return result;
    }

    /// <summary>Copies a graph-declared scene or asset with its runtime path preserved.</summary>
    static void CopyPayload(string staged, string output, string sourceIdentity, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        string normalized = sourceIdentity.Replace('\\', Path.DirectorySeparatorChar);
        string source = Path.GetFullPath(normalized, staged);
        if (!File.Exists(source)) { throw new FileNotFoundException("Apple staged payload was not found: " + sourceIdentity, source); }
        string destination = ResolveCookedPath(output, Path.IsPathRooted(normalized) ? Path.GetFileName(normalized) : sourceIdentity);
        Directory.CreateDirectory(Path.GetDirectoryName(destination));
        if (source != destination) { File.Copy(source, destination, true); }
    }

    /// <summary>Copies the actual staged cooked tree while validating every destination, including Unicode and space-containing paths.</summary>
    static void CopyTree(string source, string destination, CancellationToken cancellationToken) {
        Stack<string> pending = new();
        pending.Push(source);
        while (pending.Count > 0) {
            foreach (string entry in Directory.EnumerateFileSystemEntries(pending.Pop())) {
                cancellationToken.ThrowIfCancellationRequested();
                FileAttributes attributes = File.GetAttributes(entry);
                if ((attributes & FileAttributes.ReparsePoint) != 0) { throw new InvalidOperationException("Cooked payload symlinks are not supported: " + entry); }
                if ((attributes & FileAttributes.Directory) != 0) { pending.Push(entry); continue; }
                string relative = Path.GetRelativePath(source, entry);
                string target = ResolveCookedPath(Path.GetDirectoryName(destination), "cooked/" + relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                if (Path.GetFullPath(entry) != target) { File.Copy(entry, target, true); }
            }
        }
    }

    /// <summary>Copies a verified native bundle or cooked subtree to its exact portable package location, refusing symlink indirection.</summary>
    static void CopyBundleTree(string source, string destination, CancellationToken cancellationToken) {
        if (!Directory.Exists(source)) { throw new DirectoryNotFoundException("Required Apple application payload is absent: " + source); }
        Directory.CreateDirectory(destination);
        foreach (string entry in Directory.EnumerateFileSystemEntries(source)) {
            cancellationToken.ThrowIfCancellationRequested();
            FileAttributes attributes = File.GetAttributes(entry);
            if ((attributes & FileAttributes.ReparsePoint) != 0) { throw new InvalidOperationException("Apple package payload symlinks are not supported: " + entry); }
            string target = Path.Combine(destination, Path.GetFileName(entry));
            if ((attributes & FileAttributes.Directory) != 0) { CopyBundleTree(entry, target, cancellationToken); }
            else { File.Copy(entry, target, true); }
        }
    }

    /// <summary>Finds this builder's real Apple git checkout from an explicit root or assembly ancestors.</summary>
    static string ResolveRepositoryRoot() {
        string configured = Environment.GetEnvironmentVariable("HELENGINE_APPLE_ROOT");
        DirectoryInfo directory = new(string.IsNullOrWhiteSpace(configured) ? AppContext.BaseDirectory : Path.GetFullPath(configured));
        while (directory != null) {
            if (File.Exists(Path.Combine(directory.FullName, "CMakeLists.txt")) && Directory.Exists(Path.Combine(directory.FullName, ".git"))) { return directory.FullName; }
            directory = directory.Parent;
        }
        throw new DirectoryNotFoundException("Set HELENGINE_APPLE_ROOT to the actual Apple checkout containing CMakeLists.txt and .git.");
    }

    /// <summary>Executes structured compiler arguments, captures both streams, kills cancelled subprocesses, and preserves native failures.</summary>
    static async Task RunAsync(string executable, IReadOnlyList<string> arguments, string outputRoot, string logName, CancellationToken cancellationToken) {
        Directory.CreateDirectory(outputRoot);
        ProcessStartInfo start = new(executable) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, WorkingDirectory = outputRoot };
        foreach (string argument in arguments) { start.ArgumentList.Add(argument); }
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Unable to start native tool: " + executable);
        Task<string> stdout = process.StandardOutput.ReadToEndAsync(cancellationToken);
        Task<string> stderr = process.StandardError.ReadToEndAsync(cancellationToken);
        try { await process.WaitForExitAsync(cancellationToken); }
        catch (OperationCanceledException) { if (!process.HasExited) { process.Kill(true); } throw; }
        string log = await stdout + await stderr;
        await File.WriteAllTextAsync(Path.Combine(outputRoot, logName), log, cancellationToken);
        if (process.ExitCode != 0) { throw new InvalidOperationException($"Apple native tool exited {process.ExitCode}: {executable}\n{log[Math.Max(0, log.Length - 4000)..]}"); }
    }
}

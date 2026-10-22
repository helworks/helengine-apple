using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.apple.builder;

/// <summary>Shares Apple metadata while rejecting unimplemented cook/runtime stages with actionable errors.</summary>
public abstract class ApplePlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>Creates the target-specific descriptor exposed by the shared Apple builder assembly.</summary>
    protected ApplePlatformAssetBuilder(string target) {
        Definition = ApplePlatformDefinitionFactory.Create(target);
        Descriptor = new PlatformBuilderDescriptor("helengine.apple.builder." + target, "1.0.0", target,
            new EngineCompatibilityRange("1.0.0", "999.0.0"), new ManifestCompatibilityRange(1,1), [target], ["debug", "release", "profiler"]);
    }

    /// <summary>Gets the stable target identity used during plugin discovery.</summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>Gets target profiles, precision capabilities and the shared Metal graphics contract.</summary>
    public PlatformDefinition Definition { get; }

    /// <summary>Rejects missing inputs and incomplete engine shader translation rather than emitting incompatible material bytes.</summary>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) { throw new ArgumentNullException(nameof(request)); }
        throw new NotSupportedException($"{Definition.PlatformId}: engine shader payload translation and reflection to Metal is not integrated. Native diagnostic shaders do not replace authored materials.");
    }

    /// <summary>Rejects an incomplete generated-core player explicitly; native renderer validation is built through scripts/build_native.py.</summary>
    public Task<PlatformBuildReport> BuildAsync(PlatformBuildRequest request, IPlatformBuildProgressReporter progressReporter, IPlatformBuildDiagnosticReporter diagnosticReporter, CancellationToken cancellationToken) {
        if (request == null) { throw new ArgumentNullException(nameof(request)); }
        cancellationToken.ThrowIfCancellationRequested();
        return Task.FromException<PlatformBuildReport>(new NotSupportedException($"{Definition.PlatformId}: generated-core startup, content and renderer bridge integration is incomplete; no game player has been built. Use the explicitly labelled native diagnostic for SDK/GPU validation."));
    }
}

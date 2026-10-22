using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.apple.builder;

/// <summary>Shares Apple metadata while rejecting unimplemented cook/runtime stages with actionable errors.</summary>
public abstract class ApplePlatformAssetBuilder : IPlatformAssetBuilder, helengine.IShaderBackendRegistryContributor, helengine.IContextualShaderBackendRegistryContributor, IPlatformCookContextProvider {
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

    /// <summary>Rejects legacy registration which cannot carry the immutable SDK/cache identity.</summary>
    public void RegisterShaderBackends(helengine.ShaderBackendRegistry shaderBackendRegistry) {
        if (shaderBackendRegistry == null) { throw new ArgumentNullException(nameof(shaderBackendRegistry)); }
        throw new InvalidOperationException("Apple shader registration requires the captured PlatformCookContext overload.");
    }

    /// <summary>Captures the effective SDK and shared compiler identity exactly once for the caller's build.</summary>
    public PlatformCookContext CaptureCookContext() {
        return AppleCookContext.Capture(Definition.PlatformId);
    }

    /// <summary>Registers a compiler bound to the same immutable SDK used by scene caching and native staging.</summary>
    public void RegisterShaderBackends(helengine.ShaderBackendRegistry shaderBackendRegistry, PlatformCookContext context) {
        if (shaderBackendRegistry == null) { throw new ArgumentNullException(nameof(shaderBackendRegistry)); }
        shaderBackendRegistry.Register(new MetalShaderBackend(Definition.PlatformId, context));
    }

    /// <summary>Serializes the engine material format and preserves authored shader, texture and fixed GPU buffer identities.</summary>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) { throw new ArgumentNullException(nameof(request)); }
        if (request.TargetPlatformId != Definition.PlatformId || request.SelectedGraphicsProfileId != "metal" || request.SchemaId != "standard-shader") { throw new ArgumentException("Apple material target, Metal graphics profile and standard-shader schema must match the selected builder.", nameof(request)); }
        Definition.CodegenProfiles.Single().NumericCapabilities.RequireSupported(request.NumericPolicy);
        return AppleMaterialCooker.Cook(request);
    }

    /// <summary>Stages real graph payloads and invokes the native compiler with exact target, profile and numeric ABI contracts.</summary>
    public Task<PlatformBuildReport> BuildAsync(PlatformBuildRequest request, IPlatformBuildProgressReporter progressReporter, IPlatformBuildDiagnosticReporter diagnosticReporter, CancellationToken cancellationToken) {
        if (request == null) { throw new ArgumentNullException(nameof(request)); }
        cancellationToken.ThrowIfCancellationRequested();
        return AppleBuildWorkspace.BuildAsync(Definition.PlatformId, request, progressReporter, diagnosticReporter, cancellationToken);
    }
}

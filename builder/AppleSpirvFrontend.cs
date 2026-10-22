using helengine;

namespace helengine.apple.builder;

/// <summary>Reuses the engine's shaderc binding and HLSL configuration solely as an intermediate compiler for Metal.</summary>
public sealed class AppleSpirvFrontend : helengine.vulkan.VulkanShaderBackend {
    /// <summary>Compiles Metal-selected source defines to intermediate SPIR-V without creating any Vulkan runtime objects.</summary>
    public byte[] CompileIntermediate(ShaderCompileRequest request, IShaderIncludeResolver includes) {
        return base.CompileBytecode(request, includes, out _);
    }
}

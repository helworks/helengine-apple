#pragma once
#include "RenderManager3D.hpp"
#include "apple/metal_renderer.hpp"
#include "IShaderRenderManager3D.hpp"
#include "ShaderCompileTarget.hpp"
#include "ShaderStage.hpp"
#include "apple/core_material_resource.hpp"
#include <unordered_map>
#include <unordered_set>

class ShaderProgramAsset;
class ShaderBinaryAsset;

namespace helengine::apple {
class CoreRenderManager2D;
class CoreRuntimeModel;
/// Coordinates real generated camera queues and the Metal device while rejecting incomplete native resource contracts.
class CoreRenderManager3D final : public ::RenderManager3D, public ::IShaderRenderManager3D {
    /// Borrows the shared Metal renderer, whose device outlives Core.
    MetalRenderer& Renderer;
    /// Borrows the generated sprite, text and rounded-rectangle adapter.
    CoreRenderManager2D& Bridge;
    /// Counts actual packets submitted by the last generated Core.Draw call.
    int32_t LastDrawCalls = 0;
    /// Tracks every model wrapper whose native uploads are owned by this provider.
    std::unordered_set<CoreRuntimeModel*> Models;
    /// Owns copied shader metadata and native pipelines keyed by generated root material identity.
    std::unordered_map<::RuntimeMaterial*,CoreMaterialResource> Materials;
    /// Tracks generated root wrappers independently from invalidated native shader caches.
    std::unordered_set<::RuntimeMaterial*> OwnedMaterials;
    /// Defers model destruction until the native frame has completed.
    std::unordered_set<CoreRuntimeModel*> ReleasedModels;
    /// Defers material wrapper and pipeline destruction until native frame completion.
    std::unordered_set<::RuntimeMaterial*> ReleasedMaterials;
    /// Resolves a unique exact cooked shader program, never substituting a diagnostic shader.
    static ::ShaderProgramAsset* RequireProgram(::ShaderAsset* shader,HeCppString name,::ShaderStage stage);
    /// Resolves the exact target/stage/variant library matching the authored material.
    static ::ShaderBinaryAsset* RequireBinary(::ShaderAsset* shader,HeCppString name,::ShaderStage stage,HeCppString variant);
    /// Copies reflected stage bindings so no borrowed shader asset data escapes the loader call.
    static void CopyBindings(CoreMaterialResource& target,::ShaderProgramAsset* program,uint32_t stage);
public:
    /// Binds both generated rendering interfaces to the same native device.
    CoreRenderManager3D(MetalRenderer& renderer,CoreRenderManager2D& bridge);
    /// Uploads validated source attributes and preserves exact authored triangle submesh ranges.
    ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;
    /// Uses the shared real shader/material deserializer and existing texture dependency loader.
    ::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* content,HeCppString path) override;
    /// Creates a generated shader material backed by exact cooked Metal program libraries.
    ::RuntimeMaterial* BuildMaterialFromRaw(::ShaderMaterialAsset* material,::ShaderAsset* shader) override;
    /// Identifies Metal as the only runtime shader target for both Apple platforms.
    ::ShaderCompileTarget get_ShaderCompileTarget() override { return ::ShaderCompileTarget::Metal; }
    /// Invalidates only pipelines for the requested authored shader at a completed GPU boundary.
    void InvalidateShaderResources(HeCppString id,::ShaderAsset* shader) override;
    /// Transfers generated model ownership into deferred native release.
    void ReleaseModel(::RuntimeModel* model) override;
    /// Transfers material wrapper ownership into deferred native release.
    void ReleaseMaterial(::RuntimeMaterial* material) override;
    /// Finishes deferred resource disposal after synchronous native frame completion.
    void FlushReleasedAssets() override;
    /// Releases all remaining provider-owned assets after Core scene shutdown.
    void Dispose() override;
    /// Visits enabled cameras in authored order and submits their real generated 2D commands.
    void Draw() override;
    /// Reports genuine submitted packets for diagnostics and offscreen assertions.
    int32_t get_LastDrawCallCount() override { return LastDrawCalls; }
};
}

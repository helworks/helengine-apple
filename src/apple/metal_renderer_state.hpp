#pragma once
#include "apple/metal_renderer.hpp"
#include "apple/draw_packet.hpp"
#include "apple/mesh_resource.hpp"
#include "apple/material_resource.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <unordered_map>

namespace helengine::apple {
    /// Retains the Objective-C resources used by the public C++ renderer.
    class MetalRenderer::State final {
    public:
        /// Selected hardware device; nil is rejected during renderer construction.
        id<MTLDevice> Device;
        /// Ordered command queue shared by all frames and uploads.
        id<MTLCommandQueue> Queue;
        /// Compiled blend-enabled textured primitive pipeline.
        id<MTLRenderPipelineState> Pipeline;
        /// Compiled signed-distance pipeline for rounded shapes and borders.
        id<MTLRenderPipelineState> RoundedPipeline;
        /// Indexed position/normal/UV pipeline with a depth attachment.
        id<MTLRenderPipelineState> MeshPipeline;
        /// Native R32Float shadow-depth pipeline for actual projected and radial cube caster passes.
        id<MTLRenderPipelineState> ShadowPipeline;
        /// Depth-compatible primitive pipeline used when 2D overlays share a mixed frame.
        id<MTLRenderPipelineState> PrimitiveDepthPipeline;
        /// Depth-compatible rounded pipeline used for mixed-frame overlays.
        id<MTLRenderPipelineState> RoundedDepthPipeline;
        /// Less-than comparison with writes enabled for real indexed geometry.
        id<MTLDepthStencilState> MeshDepthState;
        /// Disables depth testing and writes for ordered 2D overlays.
        id<MTLDepthStencilState> OverlayDepthState;
        /// Shared standard linear clamp sampler for the engine's shader-backed material contract.
        id<MTLSamplerState> MaterialSampler;
        /// Point sampling for actual float shadow targets, independent of diffuse texture filtering.
        id<MTLSamplerState> ShadowSampler;
        /// Uploaded resources retained until their explicit release.
        std::unordered_map<std::uint64_t, id<MTLTexture>> Textures;
        /// Uploaded indexed meshes retained until safe explicit release.
        std::unordered_map<std::uint64_t,MeshResource> Meshes;
        /// Actual authored pipelines retained under explicit renderer ownership.
        std::unordered_map<std::uint64_t,MaterialResource> Materials;
        /// Draws waiting for the next explicit frame submission.
        std::vector<DrawPacket> Draws;
        /// Monotonic texture identity; zero is never a valid texture.
        std::uint64_t NextTexture = 1;
        /// Independent monotonic mesh identity namespace.
        std::uint64_t NextMesh = 1;
        /// Independent monotonic identity namespace for authored material pipelines.
        std::uint64_t NextMaterial = 1;
        /// Current clip copied into every packet before encoding starts.
        std::optional<ScissorRect> Clip;
        /// Camera viewport copied into each queued indexed packet.
        std::optional<ScissorRect> MeshViewport;
        /// Camera-selected clear color, retained until the next explicit selection.
        std::array<float,4> ClearColor {0,0,0,1};
        /// Actual depth-tested indexed samples counted by Metal in the last completed main pass.
        uint64_t LastVisibleMeshSamples = 0;
        /// Initializes device, command queue and precompiled Metal shader library.
        State();
        /// Loads exact cooked library bytes into the hardware device without retaining caller-owned memory.
        id<MTLLibrary> LoadLibrary(std::span<const std::uint8_t> bytes);
        /// Extracts Metal's native stage binding requirements for pre-submission material validation.
        /// MTLArgument remains the supported reflection ABI on the declared iOS 15 deployment target.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        void ReflectArguments(MaterialResource& material,NSArray<MTLArgument*>* arguments,std::uint32_t stage);
#pragma clang diagnostic pop
        /// Encodes ordered draws and waits for GPU completion before callers can recycle resources.
        void Render(id<MTLTexture> target, id<CAMetalDrawable> drawable);
    };
}

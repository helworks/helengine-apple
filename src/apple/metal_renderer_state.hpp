#pragma once
#include "apple/metal_renderer.hpp"
#include "apple/draw_packet.hpp"
#include "apple/mesh_resource.hpp"
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
        /// Depth-compatible primitive pipeline used when 2D overlays share a mixed frame.
        id<MTLRenderPipelineState> PrimitiveDepthPipeline;
        /// Depth-compatible rounded pipeline used for mixed-frame overlays.
        id<MTLRenderPipelineState> RoundedDepthPipeline;
        /// Less-than comparison with writes enabled for real indexed geometry.
        id<MTLDepthStencilState> MeshDepthState;
        /// Disables depth testing and writes for ordered 2D overlays.
        id<MTLDepthStencilState> OverlayDepthState;
        /// Uploaded resources retained until their explicit release.
        std::unordered_map<std::uint64_t, id<MTLTexture>> Textures;
        /// Uploaded indexed meshes retained until safe explicit release.
        std::unordered_map<std::uint64_t,MeshResource> Meshes;
        /// Draws waiting for the next explicit frame submission.
        std::vector<DrawPacket> Draws;
        /// Monotonic texture identity; zero is never a valid texture.
        std::uint64_t NextTexture = 1;
        /// Independent monotonic mesh identity namespace.
        std::uint64_t NextMesh = 1;
        /// Current clip copied into every packet before encoding starts.
        std::optional<ScissorRect> Clip;
        /// Initializes device, command queue and precompiled Metal shader library.
        State();
        /// Encodes ordered draws and waits for GPU completion before callers can recycle resources.
        void Render(id<MTLTexture> target, id<CAMetalDrawable> drawable);
    };
}

#pragma once
#include "apple/metal_renderer.hpp"
#include "apple/draw_packet.hpp"
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
        /// Uploaded resources retained until their explicit release.
        std::unordered_map<std::uint64_t, id<MTLTexture>> Textures;
        /// Draws waiting for the next explicit frame submission.
        std::vector<DrawPacket> Draws;
        /// Monotonic texture identity; zero is never a valid texture.
        std::uint64_t NextTexture = 1;
        /// Initializes device, command queue and precompiled Metal shader library.
        State();
        /// Encodes ordered draws and waits for GPU completion before callers can recycle resources.
        void Render(id<MTLTexture> target, id<CAMetalDrawable> drawable);
    };
}

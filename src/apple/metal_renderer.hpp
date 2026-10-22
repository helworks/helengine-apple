#pragma once
#include "apple/vertex.hpp"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace helengine::apple {
    /// Owns the shared macOS/iOS Metal device and synchronous submissions with explicit GPU completion.
    class MetalRenderer final {
        /// Hides SDK Objective-C objects from the C++ host and generated core.
        class State;
        /// Retains the device, pipelines, textures and command queue until submitted work completes.
        std::unique_ptr<State> Resources;
    public:
        /// Initializes a real Metal device and loads the SDK-compiled shader library.
        MetalRenderer();
        /// Completes resource teardown after the last submitted command buffer.
        ~MetalRenderer();
        /// Prevents duplicate ownership of native GPU resources.
        MetalRenderer(const MetalRenderer&) = delete;
        /// Prevents duplicate ownership of native GPU resources.
        MetalRenderer& operator=(const MetalRenderer&) = delete;
        /// Returns the selected physical Metal device name for validation evidence.
        std::string DeviceName() const;
        /// Uploads validated tightly packed RGBA8 texels and returns an owned resource identity.
        std::uint64_t UploadTexture(std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba);
        /// Releases a known texture only after synchronous frame completion.
        void ReleaseTexture(std::uint64_t texture);
        /// Queues triangles in submission order; absent textures and malformed geometry fail explicitly.
        void Submit(std::span<const Vertex> vertices, std::uint64_t texture);
        /// Presents queued primitives to an SDL-provided CAMetalLayer; false means a temporarily missing drawable.
        bool Present(void* layer, std::uint32_t width, std::uint32_t height);
        /// Renders known textured/alpha primitives offscreen, validates pixels and returns their RGBA8 readback.
        std::vector<std::uint8_t> ValidateOffscreen();
        /// Discards transient CPU submissions when a frame is cancelled by lifecycle events.
        void DiscardFrame();
    };
}

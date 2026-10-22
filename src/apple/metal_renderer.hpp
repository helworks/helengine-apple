#pragma once
#include "apple/vertex.hpp"
#include "apple/scissor_rect.hpp"
#include <array>
#include <optional>
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
        /// Updates an exact RGBA8 region after validating source row pitch and destination bounds.
        void UpdateTexture(std::uint64_t texture, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::size_t rowPitch);
        /// Queues triangles in submission order; absent textures and malformed geometry fail explicitly.
        void Submit(std::span<const Vertex> vertices, std::uint64_t texture);
        /// Sets a per-draw physical pixel clip which is snapshotted when each primitive is queued.
        void SetClip(std::optional<ScissorRect> rectangle);
        /// Queues a signed-distance rounded rectangle including per-corner mask and authored border color.
        void SubmitRoundedRect(std::span<const Vertex> vertices, float width, float height, float radius, float border, std::array<float,4> borderColor, std::uint32_t corners);
        /// Completes queued GPU work into an RGBA8 CPU readback without creating any window or drawable.
        std::vector<std::uint8_t> RenderOffscreen(std::uint32_t width, std::uint32_t height);
        /// Presents queued primitives to an SDL-provided CAMetalLayer; false means a temporarily missing drawable.
        bool Present(void* layer, std::uint32_t width, std::uint32_t height);
        /// Renders known textured/alpha primitives offscreen, validates pixels and returns their RGBA8 readback.
        std::vector<std::uint8_t> ValidateOffscreen();
        /// Discards transient CPU submissions when a frame is cancelled by lifecycle events.
        void DiscardFrame();
        /// Returns whether a generated resource release must wait for this frame's GPU submission.
        bool HasPendingDraws() const;
    };
}

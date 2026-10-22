#pragma once
#include "apple/vertex.hpp"
#include "apple/scissor_rect.hpp"
#include "apple/mesh_vertex.hpp"
#include "apple/mesh_draw_uniform.hpp"
#include "apple/material_buffer.hpp"
#include "apple/material_texture.hpp"
#include "apple/shadow_draw.hpp"
#include "apple/material_pipeline_state.hpp"
#include <string_view>
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
        /// Uploads all six RGBA8 faces in Metal's +X,-X,+Y,-Y,+Z,-Z order with exact payload validation.
        std::uint64_t UploadCubeTexture(std::uint32_t side,std::span<const std::uint8_t> rgbaFaces);
        /// Allocates a sampleable R32Float shadow target and clears every real face to neutral depth one.
        std::uint64_t CreateShadowTexture(std::uint32_t width,std::uint32_t height,bool cube);
        /// Renders actual indexed casters into one shadow face with a Depth32 attachment and synchronous completion.
        void RenderShadowMeshes(std::uint64_t target,std::uint32_t face,std::span<const ShadowDraw> draws,std::optional<ScissorRect> atlasRegion={});
        /// Reads completed float shadow texels directly for GPU depth/face-isolation tests.
        std::vector<float> ReadShadowTexture(std::uint64_t target,std::uint32_t face) const;
        /// Releases a known texture only after synchronous frame completion.
        void ReleaseTexture(std::uint64_t texture);
        /// Updates an exact RGBA8 region after validating source row pitch and destination bounds.
        void UpdateTexture(std::uint64_t texture, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> rgba, std::size_t rowPitch);
        /// Queues triangles in submission order; absent textures and malformed geometry fail explicitly.
        void Submit(std::span<const Vertex> vertices, std::uint64_t texture);
        /// Sets a per-draw physical pixel clip which is snapshotted when each primitive is queued.
        void SetClip(std::optional<ScissorRect> rectangle);
        /// Uploads validated indexed position/normal/UV triangles and owns their buffers until explicit release.
        std::uint64_t UploadMesh(std::span<const MeshVertex> vertices,std::span<const std::uint32_t> indices);
        /// Queues an indexed depth-tested draw with a required texture and fixed row-vector matrix payload.
        void SubmitMesh(std::uint64_t mesh,std::uint64_t texture,const MeshDrawUniform& uniforms);
        /// Releases a known mesh after every queued draw using it has completed.
        void ReleaseMesh(std::uint64_t mesh);
        /// Builds a real authored pipeline from cooked Metal libraries and rejects unsupported vertex/resource contracts.
        std::uint64_t UploadMaterial(std::span<const std::uint8_t> vertexLibrary,std::string_view vertexEntry,std::span<const std::uint8_t> fragmentLibrary,std::string_view fragmentEntry,MaterialPipelineState state={});
        /// Queues an authored material only after validating all reflected constants and texture resources.
        void SubmitMaterialMesh(std::uint64_t mesh,std::uint64_t material,std::span<const MaterialBuffer> buffers,std::span<const MaterialTexture> textures);
        /// Releases a known authored pipeline after all of its queued draws have completed.
        void ReleaseMaterial(std::uint64_t material);
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
        /// Returns the exact queued packet count before presentation consumes the frame.
        std::size_t GetQueuedDrawCount() const;
        /// Uses the active generated camera's explicit clear color for the next native frame.
        void SetClearColor(std::array<float,4> color);
        /// Snapshots a physical camera viewport for indexed packets; ordered 2D overlays retain full-target NDC coordinates.
        void SetMeshViewport(std::optional<ScissorRect> viewport);
        /// Exposes exact active native reflection to skip cooked bindings eliminated by shader optimization.
        bool IsMaterialBindingActive(std::uint64_t material,std::uint32_t kind,std::uint32_t slot,std::uint32_t stage) const;
    };
}

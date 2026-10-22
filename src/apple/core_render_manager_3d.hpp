#pragma once
#include "RenderManager3D.hpp"
#include "apple/metal_renderer.hpp"

namespace helengine::apple {
class CoreRenderManager2D;
/// Coordinates real generated camera queues and the Metal device while rejecting incomplete native resource contracts.
class CoreRenderManager3D final : public ::RenderManager3D {
    /// Borrows the shared Metal renderer, whose device outlives Core.
    MetalRenderer& Renderer;
    /// Borrows the generated sprite, text and rounded-rectangle adapter.
    CoreRenderManager2D& Bridge;
    /// Counts actual packets submitted by the last generated Core.Draw call.
    int32_t LastDrawCalls = 0;
public:
    /// Binds both generated rendering interfaces to the same native device.
    CoreRenderManager3D(MetalRenderer& renderer,CoreRenderManager2D& bridge);
    /// Rejects native model loading until its complete authored material contract is connected.
    ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;
    /// Visits enabled cameras in authored order and submits their real generated 2D commands.
    void Draw() override;
    /// Reports genuine submitted packets for diagnostics and offscreen assertions.
    int32_t get_LastDrawCallCount() override { return LastDrawCalls; }
};
}

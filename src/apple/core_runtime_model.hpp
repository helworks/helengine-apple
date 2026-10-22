#pragma once
#include "RuntimeModel.hpp"
#include "apple/metal_renderer.hpp"
#include <vector>
#include <stdexcept>

namespace helengine::apple {
/// Owns exact native indexed draws for the generated model's ordered authored submesh ranges.
class CoreRuntimeModel final : public ::RuntimeModel {
    /// Borrows the renderer until scene ownership releases this wrapper.
    MetalRenderer& Renderer;
    /// Prevents repeated native and generated disposal.
    bool Disposed = false;
public:
    /// Retains one native mesh identity per generated submesh in the same order.
    std::vector<uint64_t> Meshes;
    /// Establishes the same-device lifetime relationship before any native upload.
    explicit CoreRuntimeModel(MetalRenderer& renderer) : Renderer(renderer) { }
    /// Releases every uploaded submesh at a GPU-safe boundary and then generated-owned model data.
    void Dispose() override {
        if (Disposed) return;
        if (Renderer.HasPendingDraws()) throw std::logic_error("Generated Metal model disposal requires a completed frame");
        for (auto mesh : Meshes) Renderer.ReleaseMesh(mesh);
        Meshes.clear(); Disposed=true;
        ::RuntimeModel::Dispose();
    }
};
}

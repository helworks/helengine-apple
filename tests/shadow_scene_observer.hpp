#pragma once
#include "apple/core_player_observer.hpp"
#include "apple/metal_renderer.hpp"
#include "Core.hpp"
#include "RenderManager3D.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace helengine::apple::tests {
/// Proves actual shadow receiver pixels change when authored shadow execution is disabled, with simulation held fixed.
class ShadowSceneObserver final : public CorePlayerObserver {
    /// Records center-region pixels affected by real shadow sampling rather than HUD animation.
    uint64_t ChangedPixels=0;
    /// Records deterministic restoration of the original GPU image after re-enabling the same authored shadows.
    bool Restored=false;
public:
    /// Leaves the real authored simulation and input path intact.
    void BeforeUpdate(::Core&,SdlInputBackend&,uint32_t) override { }
    /// Leaves generated component state untouched before rendering.
    void AfterUpdate(::Core&,SdlInputBackend&,uint32_t) override { }
    /// Performs exactly two additional draws with no Core.Update between them, isolating actual shadow execution.
    void AfterDraw(::Core& core,MetalRenderer& renderer,std::span<const uint8_t> original,uint32_t width,uint32_t height,uint32_t frame) override {
        if (frame!=2) return;
        auto* manager=core.get_RenderManager3D();
        if (!manager->get_ShadowsEnabled()) throw std::runtime_error("Shadow receiver validation requires the authored enabled shadow policy");
        manager->set_ShadowsEnabled(false); core.Draw();
        const auto unshadowed=renderer.RenderOffscreen(width,height);
        manager->set_ShadowsEnabled(true); core.Draw();
        const auto restored=renderer.RenderOffscreen(width,height);
        Restored=restored.size()==original.size() && std::equal(restored.begin(),restored.end(),original.begin());
        for (uint32_t y=height/4;y<height*3/4;++y) for (uint32_t x=width/4;x<width*3/4;++x) {
            const auto index=(static_cast<size_t>(y)*width+x)*4;
            if (unshadowed[index]!=original[index] || unshadowed[index+1]!=original[index+1] || unshadowed[index+2]!=original[index+2]) ++ChangedPixels;
        }
    }
    /// Requires observable receiver shading and exact restoration to rule out disabled or fabricated shadow resources.
    void ValidateComplete(::Core&) override {
        std::cout << "shadow_changed_center_pixels=" << ChangedPixels << " shadow_restored=" << Restored << " extra_controlled_draws=2\n";
        if (ChangedPixels<64 || !Restored) throw std::runtime_error("Actual authored shadow receiver output did not change and restore deterministically");
    }
};
}

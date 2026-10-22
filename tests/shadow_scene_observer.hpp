#pragma once
#include "apple/core_player_observer.hpp"
#include "apple/metal_renderer.hpp"
#include "apple/core_render_manager_3d.hpp"
#include "Core.hpp"
#include "RenderManager3D.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "Component.hpp"
#include "LightComponent.hpp"
#include "IDrawable3D.hpp"
#include "RuntimeMaterial.hpp"
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
        auto* native=dynamic_cast<CoreRenderManager3D*>(manager);
        const auto depths=renderer.ReadShadowTexture(native->GetShadowAtlasTexture(),0);
        const auto written=std::count_if(depths.begin(),depths.end(),[](float value) { return value<0.9999f; });
        int lights=0,receivers=0;
        auto* entities=core.get_ObjectManager()->get_Entities();
        for (int32_t index=0;index<entities->get_Count();++index) {
            auto* components=(*entities)[index]->get_Components();
            for (int32_t component=0;component<components->get_Count();++component) if (auto* light=dynamic_cast<::LightComponent*>((*components)[component])) if (light->get_ShadowsEnabled()) ++lights;
        }
        auto* drawables=core.get_ObjectManager()->get_Drawables3D();
        for (int32_t index=0;index<drawables->get_Count();++index) {
            auto* materials=(*drawables)[index]->get_Materials();
            for (int32_t material=0;material<materials->Length;++material) if ((*materials)[material]->get_ReceivesShadows()) ++receivers;
        }
        std::cout << "shadow_atlas_written_texels=" << written << " authored_shadow_lights=" << lights << " authored_shadow_receivers=" << receivers << '\n';
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

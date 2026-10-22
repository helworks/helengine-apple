#include "apple/core_render_manager_3d.hpp"
#include "apple/core_render_manager_2d.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "ICamera.hpp"
#include "Entity.hpp"
#include "ModelAsset.hpp"
#include "IRenderQueue3D.hpp"
#include <algorithm>
#include <stdexcept>

namespace helengine::apple {
CoreRenderManager3D::CoreRenderManager3D(MetalRenderer& renderer,CoreRenderManager2D& bridge) : Renderer(renderer),Bridge(bridge) { }
void CoreRenderManager3D::Draw() {
    if (!get_OwnerCore() || !get_OwnerCore()->get_ObjectManager()) throw std::logic_error("Apple camera dispatch requires initialized generated Core/ObjectManager");
    std::vector<::ICamera*> cameras;
    auto* registered=get_OwnerCore()->get_ObjectManager()->get_Cameras();
    if (registered) for (int32_t index=0;index<registered->get_Count();++index) {
        auto* camera=(*registered)[index];
        if (camera && camera->get_Parent() && camera->get_Parent()->get_IsHierarchyEnabled() && IsCameraVisibleInWindow(camera,get_MainWindowHandle())) cameras.push_back(camera);
    }
    std::stable_sort(cameras.begin(),cameras.end(),[](auto* left,auto* right) { return left->get_CameraDrawOrder()<right->get_CameraDrawOrder(); });
    for (auto* camera : cameras) if (camera->get_RenderQueue3D() && camera->get_RenderQueue3D()->get_Count()) throw std::runtime_error("Generated camera requires the incomplete Apple model/material adapter");
    for (auto* camera : cameras) {
        auto settings=camera->get_ClearSettings();
        if (settings.ClearColorEnabled) { auto color=settings.ClearColor; Renderer.SetClearColor({color.X.ToSingle(),color.Y.ToSingle(),color.Z.ToSingle(),color.W.ToSingle()}); break; }
    }
    const auto extent=get_MainWindowSize();
    if (extent.X<=0 || extent.Y<=0) throw std::logic_error("Apple camera dispatch requires a positive physical drawable extent");
    for (auto* camera : cameras) Bridge.RenderCamera(camera,static_cast<uint32_t>(extent.X),static_cast<uint32_t>(extent.Y));
    LastDrawCalls=static_cast<int32_t>(Renderer.GetQueuedDrawCount());
}
}

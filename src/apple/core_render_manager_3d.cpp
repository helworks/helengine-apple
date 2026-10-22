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
    if (Disposed || !get_OwnerCore() || !get_OwnerCore()->get_ObjectManager()) throw std::logic_error("Apple camera dispatch requires initialized generated Core/ObjectManager and a live renderer");
    std::vector<::ICamera*> cameras;
    auto* registered=get_OwnerCore()->get_ObjectManager()->get_Cameras();
    if (registered) for (int32_t index=0;index<registered->get_Count();++index) {
        auto* camera=(*registered)[index];
        if (camera && camera->get_Parent() && camera->get_Parent()->get_IsHierarchyEnabled() && IsCameraVisibleInWindow(camera,get_MainWindowHandle())) cameras.push_back(camera);
    }
    std::stable_sort(cameras.begin(),cameras.end(),[](auto* left,auto* right) { return left->get_CameraDrawOrder()<right->get_CameraDrawOrder(); });
    for (auto* camera : cameras) {
        auto settings=camera->get_ClearSettings();
        if (settings.ClearColorEnabled) { auto color=settings.ClearColor; Renderer.SetClearColor({color.X.ToSingle(),color.Y.ToSingle(),color.Z.ToSingle(),color.W.ToSingle()}); break; }
    }
    const auto extent=get_MainWindowSize();
    if (extent.X<=0 || extent.Y<=0) throw std::logic_error("Apple camera dispatch requires a positive physical drawable extent");
    for (auto* camera : cameras) {
        auto* queue=camera->get_RenderQueue3D();
        if (queue && queue->get_Count()) {
            if (Renderer.HasPendingDraws()) throw std::runtime_error("Apple multiple-camera 3D shadow passes require separate retained frame attachments");
            if (camera->get_RenderTarget()) throw std::runtime_error("Apple generated camera RenderTarget attachments are not integrated");
            auto viewport=camera->get_Viewport();
            float x=viewport.X.ToSingle(),y=viewport.Y.ToSingle(),width=viewport.Z.ToSingle(),height=viewport.W.ToSingle();
            if (width>0 && height>0 && width<=1 && height<=1) { x*=extent.X; y*=extent.Y; width*=extent.X; height*=extent.Y; }
            if (width<=0 || height<=0) { x=y=0; width=extent.X; height=extent.Y; }
            if (x<0 || y<0 || width<1 || height<1 || x+width>extent.X || y+height>extent.Y) throw std::invalid_argument("Apple indexed camera viewport must remain inside the physical target");
            auto position=camera->get_Parent()->get_Position(); auto orientation=camera->get_Parent()->get_Orientation();
            auto forward=::float4::RotateVector(::float3(0,0,-1),orientation),up=::float4::RotateVector(::float3(0,1,0),orientation),target=position+forward;
            ::float4x4 view,projection;
            ::float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view);
            ::float4x4::CreatePerspectiveFieldOfView__out4(camera->get_FieldOfView(),width/height,camera->get_NearPlaneDistance(),camera->get_FarPlaneDistance(),projection);
            ::float4x4::Multiply__ref0_ref1_out2(view,projection,CurrentViewProjection); CurrentCameraPosition=position; CurrentCamera=camera;
            try {
                PrepareCameraLights(camera); PrepareCameraShadows(camera);
                const ScissorRect rectangle{static_cast<uint32_t>(x),static_cast<uint32_t>(y),static_cast<uint32_t>(width),static_cast<uint32_t>(height)};
                Renderer.SetMeshViewport(rectangle); Renderer.SetClip(rectangle); queue->VisitOrdered(this);
                CurrentCamera=nullptr; Renderer.SetMeshViewport(std::nullopt); Renderer.SetClip(std::nullopt);
            } catch (...) { CurrentCamera=nullptr; Renderer.DiscardFrame(); throw; }
        }
        Bridge.RenderCamera(camera,static_cast<uint32_t>(extent.X),static_cast<uint32_t>(extent.Y));
    }
    LastDrawCalls=static_cast<int32_t>(Renderer.GetQueuedDrawCount());
}
}

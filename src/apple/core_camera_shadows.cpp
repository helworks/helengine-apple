#include "apple/core_render_manager_3d.hpp"
#include "apple/core_runtime_model.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "IDrawable3D.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeSubmesh.hpp"
#include "ICamera.hpp"
#include "LightComponent.hpp"
#include "DirectionalLightComponent.hpp"
#include "PointLightComponent.hpp"
#include "SpotLightComponent.hpp"
#include "LightDirectionUtility.hpp"
#include "RealMath.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace helengine::apple {
::float4x4 CoreRenderManager3D::BuildShadowViewProjection(::ICamera* camera,::LightComponent* light,int face) const {
    if (!camera || !camera->get_Parent() || !light || !light->get_Parent()) throw std::invalid_argument("Apple shadow transforms require attached camera and light entities");
    ::float4x4 view,projection;
    if (auto* point=dynamic_cast<::PointLightComponent*>(light)) {
        if (face<0 || face>=6 || point->get_Range()<=0.1f) throw std::invalid_argument("Apple point shadow requires valid face and range beyond its 0.1 near plane");
        const std::array<::float3,6> directions={::float3(1,0,0),::float3(-1,0,0),::float3(0,1,0),::float3(0,-1,0),::float3(0,0,1),::float3(0,0,-1)};
        const std::array<::float3,6> ups={::float3(0,-1,0),::float3(0,-1,0),::float3(0,0,1),::float3(0,0,-1),::float3(0,1,0),::float3(0,1,0)};
        auto position=point->get_Parent()->get_Position(),target=position+directions[face],up=ups[face];
        ::float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view);
        ::float4x4::CreatePerspectiveFieldOfView__out4(std::numbers::pi_v<float>*0.5f,1.0f,0.1f,point->get_Range(),projection);
    } else if (auto* directional=dynamic_cast<::DirectionalLightComponent*>(light)) {
        const auto rotated=::LightDirectionUtility::GetEntityForwardDirection(light->get_Parent());
        const auto direction=::float3::Normalize(::float3(-rotated.X,-rotated.Y,-rotated.Z));
        const float distance=std::max(1.0f,directional->get_ShadowDistance());
        const auto forward=::float4::RotateVector(::float3(0,0,-1),camera->get_Parent()->get_Orientation());
        auto target=camera->get_Parent()->get_Position()+forward*(distance*0.5f);
        auto position=target+direction*distance;
        auto up=::RealMath::Abs(::float3::Dot(direction,::float3(0,1,0)))>0.99f ? ::float3(0,0,1) : ::float3(0,1,0);
        ::float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view);
        const float half=distance*0.5f;
        ::float4x4::CreateOrthographicOffCenter__out6(-half,half,-half,half,0.1f,distance*2,projection);
    } else if (auto* spot=dynamic_cast<::SpotLightComponent*>(light)) {
        const auto direction=::float3::Normalize(::LightDirectionUtility::GetEntityForwardDirection(light->get_Parent()));
        auto position=light->get_Parent()->get_Position(),target=position+direction;
        auto up=::RealMath::Abs(::float3::Dot(direction,::float3(0,1,0)))>0.99f ? ::float3(0,0,1) : ::float3(0,1,0);
        ::float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view);
        const float fov=std::min(spot->get_OuterConeAngleDegrees()*std::numbers::pi_v<float>/90.0f,std::numbers::pi_v<float>-0.0001f);
        const float range=std::max(0.1f,spot->get_Range());
        ::float4x4::CreatePerspectiveFieldOfView__out4(fov,1.0f,0.1f,range+std::max(2.0f,range*0.15f),projection);
    } else throw std::invalid_argument("Apple shadow transform received an unsupported light kind");
    ::float4x4 result; ::float4x4::Multiply__ref0_ref1_out2(view,projection,result);
    return result;
}

std::vector<ShadowDraw> CoreRenderManager3D::BuildShadowCasters(::LightComponent* light,::float4x4 viewProjection) const {
    std::vector<ShadowDraw> result;
    auto* drawables=get_OwnerCore()->get_ObjectManager()->get_Drawables3D();
    if (!drawables) return result;
    for (int32_t index=0;index<drawables->get_Count();++index) {
        auto* drawable=(*drawables)[index];
        if (!drawable || !drawable->get_Parent() || !drawable->get_Parent()->get_IsHierarchyEnabled() || drawable->get_Parent()->get_IsRenderHierarchySuppressed()) continue;
        if (!(drawable->get_Parent()->get_LayerMask()&light->get_Parent()->get_LayerMask()&CurrentCamera->get_LayerMask())) continue;
        auto* model=dynamic_cast<CoreRuntimeModel*>(drawable->get_Model());
        if (!model || !Models.contains(model) || ReleasedModels.contains(model)) throw std::invalid_argument("Apple shadow caster references a foreign or released native model");
        auto* materials=drawable->get_Materials();
        if (!materials || materials->Length<=0 || (materials->Length!=1 && static_cast<size_t>(materials->Length)<model->Meshes.size())) throw std::invalid_argument("Apple shadow caster requires its authored submesh materials");
        const auto world=drawable->get_Parent()->get_WorldTransformMatrix();
        ::float4x4 transform; ::float4x4::Multiply__ref0_ref1_out2(world,viewProjection,transform);
        for (size_t submesh=0;submesh<model->Meshes.size();++submesh) {
            auto* material=(*materials)[materials->Length==1 ? 0 : static_cast<int32_t>(submesh)];
            if (!material) throw std::invalid_argument("Apple shadow caster material is null");
            if (!material->get_CastsShadows()) continue;
            ShadowDraw draw{}; draw.Mesh=model->Meshes[submesh];
            CopyShadowMatrix(draw.Uniforms.WorldViewProjection,transform); CopyShadowMatrix(draw.Uniforms.World,world);
            if (auto* point=dynamic_cast<::PointLightComponent*>(light)) {
                const auto position=point->get_Parent()->get_Position();
                draw.Uniforms.PointLight[0]=position.X.ToSingle(); draw.Uniforms.PointLight[1]=position.Y.ToSingle(); draw.Uniforms.PointLight[2]=position.Z.ToSingle(); draw.Uniforms.PointLight[3]=point->get_Range();
            }
            result.push_back(draw);
        }
    }
    return result;
}

void CoreRenderManager3D::PrepareCameraShadows(::ICamera* camera) {
    if (!ShadowAtlas) ShadowAtlas=Renderer.CreateShadowTexture(2048,2048,false);
    for (auto& cube : PointShadows) if (!cube) cube=Renderer.CreateShadowTexture(512,512,true);
    std::array<float,100> values {};
    std::vector<size_t> selected;
    size_t nonPoint=0;
    if (get_ShadowsEnabled()) for (size_t index=0;index<CurrentLights.size();++index) {
        auto* light=CurrentLights[index];
        if (light->get_ShadowsEnabled() && light->get_ShadowStrength()>0 && light->get_ShadowMapMode()!=::ShadowMapMode::Disabled) {
            selected.push_back(index); if (light->get_LightType()!=::LightType::Point) ++nonPoint;
        }
    }
    const uint32_t grid=nonPoint>1 ? 2 : 1,tile=2048/grid;
    values[0]=nonPoint ? 1.0f : 0.0f; values[1]=values[2]=1.0f/2048; values[3]=static_cast<float>(nonPoint);
    Renderer.RenderShadowMeshes(ShadowAtlas,0,{});
    size_t atlasIndex=0,pointIndex=0;
    for (size_t index : selected) {
        auto* light=CurrentLights[index]; const size_t offset=4+index*24;
        values[offset+4]=1; values[offset+5]=light->get_ShadowStrength();
        if (light->get_LightType()==::LightType::Point) {
            values[offset+6]=2; values[offset+7]=static_cast<float>(pointIndex);
            for (int face=0;face<6;++face) {
                const auto projection=BuildShadowViewProjection(camera,light,face);
                Renderer.RenderShadowMeshes(PointShadows[pointIndex],face,BuildShadowCasters(light,projection));
            }
            ++pointIndex;
        } else {
            const ScissorRect region{static_cast<uint32_t>(atlasIndex%grid)*tile,static_cast<uint32_t>(atlasIndex/grid)*tile,tile,tile};
            values[offset]=static_cast<float>(region.X)/2048; values[offset+1]=static_cast<float>(region.Y)/2048; values[offset+2]=values[offset+3]=static_cast<float>(tile)/2048;
            values[offset+6]=1;
            const auto projection=BuildShadowViewProjection(camera,light,-1);
            std::vector<uint8_t> matrix; AppendShaderMatrix(matrix,projection); std::memcpy(values.data()+offset+8,matrix.data(),64);
            Renderer.RenderShadowMeshes(ShadowAtlas,0,BuildShadowCasters(light,projection),region);
            ++atlasIndex;
        }
    }
    CurrentShadows.resize(sizeof(values)); std::memcpy(CurrentShadows.data(),values.data(),sizeof(values));
}
}

#include "apple/core_render_manager_3d.hpp"
#include "apple/core_render_manager_2d.hpp"
#include "apple/core_runtime_model.hpp"
#include "ShaderRuntimeMaterial.hpp"
#include "MaterialLayout.hpp"
#include "MaterialPropertyBlock.hpp"
#include "IDrawable3D.hpp"
#include "Entity.hpp"
#include <cstring>
#include <memory>
#include <stdexcept>

namespace helengine::apple {
void CoreRenderManager3D::AppendShaderMatrix(std::vector<uint8_t>& output,::float4x4 matrix) {
    const float values[]={matrix.M11.ToSingle(),matrix.M21.ToSingle(),matrix.M31.ToSingle(),matrix.M41.ToSingle(),matrix.M12.ToSingle(),matrix.M22.ToSingle(),matrix.M32.ToSingle(),matrix.M42.ToSingle(),matrix.M13.ToSingle(),matrix.M23.ToSingle(),matrix.M33.ToSingle(),matrix.M43.ToSingle(),matrix.M14.ToSingle(),matrix.M24.ToSingle(),matrix.M34.ToSingle(),matrix.M44.ToSingle()};
    const auto* bytes=reinterpret_cast<const uint8_t*>(values); output.insert(output.end(),bytes,bytes+sizeof(values));
}
void CoreRenderManager3D::CopyShadowMatrix(float* output,::float4x4 matrix) {
    const float values[]={matrix.M11.ToSingle(),matrix.M12.ToSingle(),matrix.M13.ToSingle(),matrix.M14.ToSingle(),matrix.M21.ToSingle(),matrix.M22.ToSingle(),matrix.M23.ToSingle(),matrix.M24.ToSingle(),matrix.M31.ToSingle(),matrix.M32.ToSingle(),matrix.M33.ToSingle(),matrix.M34.ToSingle(),matrix.M41.ToSingle(),matrix.M42.ToSingle(),matrix.M43.ToSingle(),matrix.M44.ToSingle()};
    std::memcpy(output,values,sizeof(values));
}
::RuntimeTexture* CoreRenderManager3D::ResolveMaterialTexture(::ShaderRuntimeMaterial* material,const std::string& name) const {
    while (material) {
        const auto index=material->get_Layout()->FindTextureBindingIndex(name);
        if (index>=0) if (auto* texture=material->get_Properties()->GetTexture(index)) return texture;
        material=dynamic_cast<::ShaderRuntimeMaterial*>(material->get_ParentMaterial());
    }
    return nullptr;
}
std::vector<uint8_t> CoreRenderManager3D::BuildUniform(::ShaderRuntimeMaterial* material,const CoreShaderBinding& binding,::float4x4 world) {
    if (binding.Name=="TransformBuffer") {
        if (binding.Size!=224) throw std::invalid_argument("Apple built-in transform requires its exact authored 224-byte GPU layout");
        ::float4x4 wvp,normal;
        ::float4x4::Multiply__ref0_ref1_out2(world,CurrentViewProjection,wvp); ::float4x4::InverseTranspose__ref0_out1(world,normal);
        std::vector<uint8_t> bytes; bytes.reserve(224);
        AppendShaderMatrix(bytes,world); AppendShaderMatrix(bytes,wvp); AppendShaderMatrix(bytes,normal);
        const float tail[]={CurrentCameraPosition.X.ToSingle(),CurrentCameraPosition.Y.ToSingle(),CurrentCameraPosition.Z.ToSingle(),0,material->get_ReceivesShadows()?1.0f:0.0f,material->get_SupportsEmissive()?1.0f:0.0f,0,0};
        const auto* source=reinterpret_cast<const uint8_t*>(tail); bytes.insert(bytes.end(),source,source+sizeof(tail));
        return bytes;
    }
    if (binding.Name=="ForwardLightBuffer") {
        if (binding.Size!=288 || CurrentForwardLights.size()!=288) throw std::invalid_argument("Apple forward lights require their exact authored 288-byte GPU layout");
        return CurrentForwardLights;
    }
    if (binding.Name=="ShadowBuffer") {
        if (binding.Size!=400 || CurrentShadows.size()!=400) throw std::invalid_argument("Apple shadows require their exact authored 400-byte GPU layout");
        return CurrentShadows;
    }
    Array<uint8_t>* data=nullptr;
    if (!material->TryResolveConstantBufferData__out1(binding.Name,data) || !data) throw std::invalid_argument("Apple material is missing inherited constant bytes: "+binding.Name);
    std::unique_ptr<Array<uint8_t>> owned(data);
    if (data->Length<binding.Size) throw std::invalid_argument("Apple authored material constant bytes are shorter than reflected layout: "+binding.Name);
    return std::vector<uint8_t>(data->Data,data->Data+data->Length);
}
void CoreRenderManager3D::Visit(::IDrawable3D* drawable) {
    if (!CurrentCamera) throw std::logic_error("Apple drawable visitation requires a selected generated camera");
    if (!drawable || !drawable->get_Parent() || !drawable->get_Parent()->get_IsHierarchyEnabled() || drawable->get_Parent()->get_IsRenderHierarchySuppressed() || !drawable->get_Model()) return;
    auto* model=dynamic_cast<CoreRuntimeModel*>(drawable->get_Model());
    if (!model || !Models.contains(model) || ReleasedModels.contains(model)) throw std::invalid_argument("Apple drawable references a released or foreign native model");
    auto* materials=drawable->get_Materials(); auto* ranges=model->get_Submeshes();
    if (!materials || materials->Length<=0 || (materials->Length!=1 && materials->Length<ranges->Length) || model->Meshes.size()!=static_cast<size_t>(ranges->Length)) throw std::invalid_argument("Apple drawable requires a matching authored material for every submesh");
    const auto world=drawable->get_Parent()->get_WorldTransformMatrix();
    for (int32_t index=0;index<ranges->Length;++index) {
        auto* material=dynamic_cast<::ShaderRuntimeMaterial*>((*materials)[materials->Length==1 ? 0 : index]);
        if (!material) throw std::invalid_argument("Apple indexed drawing requires an authored shader runtime material");
        const auto found=Materials.find(material->ResolveRootMaterial());
        if (found==Materials.end()) throw std::invalid_argument("Apple drawable material has no live authored Metal pipeline");
        std::vector<MaterialBuffer> buffers;
        std::vector<MaterialTexture> textures;
        for (const auto& binding : found->second.Bindings) {
            if (binding.Type==::ShaderResourceType::ConstantBuffer) {
                if (binding.Slot>=30) throw std::invalid_argument("Apple constant binding overlaps reserved mesh vertex slot 30");
                if (Renderer.IsMaterialBindingActive(found->second.Identity,0,binding.Slot,binding.Stage)) buffers.push_back({static_cast<uint32_t>(binding.Slot),binding.Stage,BuildUniform(material,binding,world)});
            } else if (binding.Type==::ShaderResourceType::Texture2D || binding.Type==::ShaderResourceType::TextureCube) {
                if (binding.Slot<100) throw std::invalid_argument("Apple sampled binding is below the explicit shared policy texture shift 100");
                const auto slot=static_cast<uint32_t>(binding.Slot-100);
                if (!Renderer.IsMaterialBindingActive(found->second.Identity,1,slot,binding.Stage)) continue;
                uint64_t texture=0;
                if (binding.Name=="shadowAtlasTexture") texture=ShadowAtlas;
                else if (binding.Name=="pointShadowTexture0") texture=PointShadows[0];
                else if (binding.Name=="pointShadowTexture1") texture=PointShadows[1];
                else if (binding.Name=="pointShadowTexture2") texture=PointShadows[2];
                else if (binding.Name=="pointShadowTexture3") texture=PointShadows[3];
                else {
                    auto* value=ResolveMaterialTexture(material,binding.Name);
                    if (!value) throw std::invalid_argument("Apple material is missing inherited texture: "+binding.Name);
                    texture=Bridge.ResolveTexture(value);
                }
                if (!texture) throw std::logic_error("Apple generated shadow binding has no real target");
                textures.push_back({slot,binding.Stage,texture});
            } else if (binding.Type!=::ShaderResourceType::Sampler) throw std::invalid_argument("Apple authored draw requires an unsupported resource kind: "+binding.Name);
        }
        Renderer.SubmitMaterialMesh(model->Meshes[index],found->second.Identity,buffers,textures);
    }
}
}

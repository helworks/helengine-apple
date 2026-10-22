#include "apple/core_render_manager_3d.hpp"
#include "apple/core_render_manager_2d.hpp"
#include "apple/core_runtime_model.hpp"
#include "apple/generated_disposable_deleter.hpp"
#include "ModelAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "RuntimeSubmesh.hpp"
#include "ModelPrimitiveTopology.hpp"
#include "ShaderRuntimeMaterialLoader.hpp"
#include "ShaderRuntimeMaterial.hpp"
#include "MaterialLayoutBuilder.hpp"
#include "StandardMaterialTextureBindingDefaults.hpp"
#include "ShaderMaterialAsset.hpp"
#include "ShaderAsset.hpp"
#include "ShaderProgramAsset.hpp"
#include "ShaderBinaryAsset.hpp"
#include "ShaderBindingAsset.hpp"
#include <memory>
#include <numeric>
#include <stdexcept>

namespace helengine::apple {
::RuntimeModel* CoreRenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
    if (!data || !data->Positions || !data->Normals || !data->TexCoords) throw std::invalid_argument("Apple model upload requires position, normal and UV arrays");
    const auto count=data->Positions->Length;
    if (count<=0 || data->Normals->Length!=count || data->TexCoords->Length!=count) throw std::invalid_argument("Apple model attributes must have equal nonempty lengths");
    std::vector<MeshVertex> vertices;
    vertices.reserve(count);
    for (int32_t index=0;index<count;++index) {
        const auto& p=(*data->Positions)[index]; const auto& n=(*data->Normals)[index]; const auto& uv=(*data->TexCoords)[index];
        vertices.push_back({{p.X.ToSingle(),p.Y.ToSingle(),p.Z.ToSingle()},{n.X.ToSingle(),n.Y.ToSingle(),n.Z.ToSingle()},{uv.X.ToSingle(),uv.Y.ToSingle()}});
    }
    std::vector<uint32_t> indices;
    if (data->Indices32 && data->Indices32->Length) indices.assign(data->Indices32->Data,data->Indices32->Data+data->Indices32->Length);
    else if (data->Indices16 && data->Indices16->Length) indices.assign(data->Indices16->Data,data->Indices16->Data+data->Indices16->Length);
    else { indices.resize(count); std::iota(indices.begin(),indices.end(),0u); }
    GeneratedDisposableOwner<CoreRuntimeModel> result(new CoreRuntimeModel(Renderer));
    result->set_Id(data->get_Id()); result->SetBounds(data->BoundsMin,data->BoundsMax);
    std::unique_ptr<Array<::RuntimeSubmesh*>> submeshes(::ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
    result->SetSubmeshes(submeshes.get());
    for (int32_t index=0;index<submeshes->Length;++index) {
        auto* range=(*submeshes)[index];
        const auto first=range->get_IndexStart(),length=range->get_IndexCount();
        if (range->get_PrimitiveTopology()!=::ModelPrimitiveTopology::TriangleList) throw std::invalid_argument("Apple native models currently require explicit triangle-list topology");
        if (first<0 || length<=0 || length%3 || static_cast<size_t>(first)>indices.size() || static_cast<size_t>(length)>indices.size()-first) throw std::invalid_argument("Apple model submesh is outside its source index range");
        result->Meshes.push_back(Renderer.UploadMesh(vertices,std::span<const uint32_t>(indices.data()+first,length)));
    }
    if (result->Meshes.empty()) throw std::invalid_argument("Apple model requires at least one validated submesh");
    Models.insert(result.get());
    return result.release();
}

::ShaderProgramAsset* CoreRenderManager3D::RequireProgram(::ShaderAsset* shader,HeCppString name,::ShaderStage stage) {
    ::ShaderProgramAsset* result=nullptr;
    if (shader->Programs) for (int32_t index=0;index<shader->Programs->Length;++index) {
        auto* program=(*shader->Programs)[index];
        if (program && program->Name==name && program->Stage==stage) {
            if (result) throw std::invalid_argument("Cooked Metal shader contains ambiguous program identity");
            result=program;
        }
    }
    if (!result || result->EntryPoint.empty()) throw std::invalid_argument("Cooked Metal shader is missing the exact authored program: "+name.raw());
    return result;
}

::ShaderBinaryAsset* CoreRenderManager3D::RequireBinary(::ShaderAsset* shader,HeCppString name,::ShaderStage stage,HeCppString variant) {
    ::ShaderBinaryAsset* result=nullptr;
    if (shader->Binaries) for (int32_t index=0;index<shader->Binaries->Length;++index) {
        auto* binary=(*shader->Binaries)[index];
        if (binary && binary->ProgramName==name && binary->Stage==stage && binary->Variant==variant && binary->TargetName=="metal") {
            if (result) throw std::invalid_argument("Cooked Metal shader contains ambiguous program/variant library");
            result=binary;
        }
    }
    if (!result || !result->Bytecode || result->Bytecode->Length<=0) throw std::invalid_argument("Cooked Metal shader is missing exact program/variant library: "+name.raw()+"/"+variant.raw());
    return result;
}

void CoreRenderManager3D::CopyBindings(CoreMaterialResource& target,::ShaderProgramAsset* program,uint32_t stage) {
    if (!program->Bindings) return;
    for (int32_t index=0;index<program->Bindings->Length;++index) {
        auto* binding=(*program->Bindings)[index];
        if (!binding || binding->Name.empty() || binding->Set!=0 || binding->Slot<0) throw std::invalid_argument("Cooked Metal program has invalid named descriptor-space-zero reflection");
        target.Bindings.push_back({binding->Name.raw(),binding->Type,binding->Slot,binding->Size,stage});
    }
}

::RuntimeMaterial* CoreRenderManager3D::BuildMaterialFromRawAsset(::ContentManager* content,HeCppString path) {
    return ::ShaderRuntimeMaterialLoader::BuildMaterialFromRawAsset(this,content,path);
}

::RuntimeMaterial* CoreRenderManager3D::BuildMaterialFromRaw(::ShaderMaterialAsset* material,::ShaderAsset* shader) {
    if (!material || !shader) throw std::invalid_argument("Apple authored material requires both material and shader assets");
    if (material->get_Id().empty() || material->ShaderAssetId.empty() || material->ShaderAssetId!=shader->get_Id() || shader->TargetName!="metal") throw std::invalid_argument("Apple material requires matching nonempty identities and actual Metal shader target");
    auto* vertex=RequireProgram(shader,material->VertexProgram,::ShaderStage::Vertex);
    auto* fragment=RequireProgram(shader,material->PixelProgram,::ShaderStage::Pixel);
    auto* vertexBinary=RequireBinary(shader,material->VertexProgram,::ShaderStage::Vertex,material->Variant);
    auto* fragmentBinary=RequireBinary(shader,material->PixelProgram,::ShaderStage::Pixel,material->Variant);
    GeneratedDisposableOwner<::ShaderRuntimeMaterial> result(new ::ShaderRuntimeMaterial());
    result->set_Id(material->get_Id());
    result->set_SupportsNormalMapping(!material->NormalTextureAssetId.empty()); result->set_SupportsEmissive(!material->EmissiveTextureAssetId.empty());
    result->set_CastsShadows(material->CastsShadows); result->set_ReceivesShadows(material->ReceivesShadows);
    result->SetLayout(::MaterialLayoutBuilder::Build(material,shader));
    result->SetRenderState(material->RenderState); result->ApplyConstantBufferDefaults(material->ConstantBuffers);
    ::StandardMaterialTextureBindingDefaults::Apply(result.get(),&Bridge);
    CoreMaterialResource resource;
    resource.ShaderId=shader->get_Id().raw();
    CopyBindings(resource,vertex,1); CopyBindings(resource,fragment,2);
    resource.Identity=Renderer.UploadMaterial(std::span<const uint8_t>(vertexBinary->Bytecode->Data,vertexBinary->Bytecode->Length),vertex->EntryPoint.raw(),std::span<const uint8_t>(fragmentBinary->Bytecode->Data,fragmentBinary->Bytecode->Length),fragment->EntryPoint.raw());
    Materials.emplace(result.get(),std::move(resource));
    OwnedMaterials.insert(result.get());
    return result.release();
}

void CoreRenderManager3D::InvalidateShaderResources(HeCppString id,::ShaderAsset* shader) {
    if (id.empty() || !shader || shader->get_Id()!=id) throw std::invalid_argument("Apple shader invalidation requires matching authored shader identity");
    if (Renderer.HasPendingDraws()) throw std::logic_error("Apple shader invalidation requires a completed native frame");
    for (auto entry=Materials.begin();entry!=Materials.end();) {
        if (entry->second.ShaderId==id.raw()) { Renderer.ReleaseMaterial(entry->second.Identity); entry=Materials.erase(entry); }
        else ++entry;
    }
}

void CoreRenderManager3D::ReleaseModel(::RuntimeModel* model) {
    auto* native=dynamic_cast<CoreRuntimeModel*>(model);
    if (!native || !Models.contains(native) || !ReleasedModels.insert(native).second) throw std::invalid_argument("Apple model release requires a live, unreleased same-device model");
}

void CoreRenderManager3D::ReleaseMaterial(::RuntimeMaterial* material) {
    if (!material || !ReleasedMaterials.insert(material).second) throw std::invalid_argument("Apple material release requires a nonnull unreleased wrapper");
}

void CoreRenderManager3D::FlushReleasedAssets() {
    if (Renderer.HasPendingDraws()) return;
    for (auto* model : ReleasedModels) { Models.erase(model); model->Dispose(); delete model; }
    ReleasedModels.clear();
    for (auto* material : ReleasedMaterials) {
        auto native=Materials.find(material);
        if (native!=Materials.end()) { Renderer.ReleaseMaterial(native->second.Identity); Materials.erase(native); }
        OwnedMaterials.erase(material);
        material->Dispose(); delete material;
    }
    ReleasedMaterials.clear();
}

void CoreRenderManager3D::Dispose() {
    if (Renderer.HasPendingDraws()) throw std::logic_error("Apple generated provider shutdown requires a completed native frame");
    FlushReleasedAssets();
    for (auto* model : Models) { model->Dispose(); delete model; }
    Models.clear();
    for (auto& entry : Materials) Renderer.ReleaseMaterial(entry.second.Identity);
    Materials.clear();
    for (auto* material : OwnedMaterials) { material->Dispose(); delete material; }
    OwnedMaterials.clear();
    ::RenderManager3D::Dispose();
}
}

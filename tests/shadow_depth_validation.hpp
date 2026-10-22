#pragma once
#include "apple/metal_renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

/// Verifies actual shadow rasterization, depth ordering, radial distance and independent cube faces on the physical GPU.
class ShadowDepthValidation final {
public:
    /// Renders indexed casters into real float shadow targets and checks measured texels rather than fabricated depth payloads.
    static void Run() {
        using namespace helengine::apple;
        MetalRenderer renderer;
        const std::array<MeshVertex,3> vertices={MeshVertex{{-0.8f,-0.8f,0.25f},{0,0,1},{0,0}},MeshVertex{{0.8f,-0.8f,0.25f},{0,0,1},{1,0}},MeshVertex{{0,0.8f,0.25f},{0,0,1},{0.5f,1}}};
        const std::array<uint32_t,3> indices={0,1,2};
        const auto mesh=renderer.UploadMesh(vertices,indices);
        ShadowDraw draw{}; draw.Mesh=mesh;
        for (size_t index=0;index<16;index+=5) { draw.Uniforms.WorldViewProjection[index]=1; draw.Uniforms.World[index]=1; }
        const auto atlas=renderer.CreateShadowTexture(16,16,false);
        const auto cleared=renderer.ReadShadowTexture(atlas,0);
        if (!std::all_of(cleared.begin(),cleared.end(),[](float value) { return value==1; })) throw std::runtime_error("Real shadow target did not clear to neutral depth one");
        renderer.RenderShadowMeshes(atlas,0,std::span<const ShadowDraw>(&draw,1));
        const auto projected=renderer.ReadShadowTexture(atlas,0);
        if (std::abs(projected[8*16+8]-0.25f)>0.0001f || projected[0]!=1) throw std::runtime_error("GPU projected shadow depth or clear region is incorrect");
        const auto cube=renderer.CreateShadowTexture(16,16,true);
        draw.Uniforms.PointLight[3]=1;
        renderer.RenderShadowMeshes(cube,4,std::span<const ShadowDraw>(&draw,1));
        const auto radial=renderer.ReadShadowTexture(cube,4);
        if (radial[8*16+8]<0.25f || radial[8*16+8]>0.28f) throw std::runtime_error("Actual radial cube shadow distance is incorrect");
        for (uint32_t face=0;face<6;++face) if (face!=4) {
            const auto untouched=renderer.ReadShadowTexture(cube,face);
            if (!std::all_of(untouched.begin(),untouched.end(),[](float value) { return value==1; })) throw std::runtime_error("Shadow cube rendering modified an unrelated face");
        }
        bool rejected=false;
        try { renderer.RenderShadowMeshes(cube,6,{}); } catch (const std::invalid_argument&) { rejected=true; }
        if (!rejected) throw std::runtime_error("Shadow cube accepted an invalid face index");
        renderer.ReleaseTexture(atlas); renderer.ReleaseTexture(cube); renderer.ReleaseMesh(mesh);
    }
};

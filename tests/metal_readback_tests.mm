#include "apple/metal_renderer.hpp"
#include "shadow_depth_validation.hpp"
#include <iostream>
#include <stdexcept>
#include <array>

/// Runs real Metal rendering and verifies ordered quads, texture sampling, alpha and explicit resource errors.
int main() {
    try {
        helengine::apple::MetalRenderer renderer;
        const auto pixels = renderer.ValidateOffscreen();
        if (pixels.size() != 64 * 64 * 4) throw std::runtime_error("unexpected readback size");
        const std::array<std::uint8_t,4> white = {255,255,255,255};
        const auto texture = renderer.UploadTexture(1,1,white);
        const std::array<helengine::apple::Vertex,6> quad = {{{{-1,-1},{0,1},{1,1,1,1}},{{1,-1},{1,1},{1,1,1,1}},{{1,1},{1,0},{1,1,1,1}},{{-1,-1},{0,1},{1,1,1,1}},{{1,1},{1,0},{1,1,1,1}},{{-1,1},{0,0},{1,1,1,1}}}};
        renderer.SetClip(helengine::apple::ScissorRect{0,0,32,64});
        renderer.Submit(quad,texture);
        renderer.SetClip(std::nullopt);
        auto clipped = renderer.RenderOffscreen(64,64);
        if (clipped[(32*64+16)*4] != 255 || clipped[(32*64+48)*4] != 0) throw std::runtime_error("Metal scissor clipping failed");
        renderer.SubmitRoundedRect(quad,64,64,20,4,{1,0,0,1},15);
        auto rounded = renderer.RenderOffscreen(64,64);
        if (rounded[0] != 0 || rounded[(32*64+32)*4] != 255 || rounded[(32*64+32)*4+1] != 255) throw std::runtime_error("Metal rounded rectangle coverage failed");
        renderer.ReleaseTexture(texture);
        const std::array<helengine::apple::MeshVertex,4> meshVertices = {{{{-1,-1,0.8f},{0,0,1},{0,1}},{{1,-1,0.8f},{0,0,1},{1,1}},{{1,1,0.8f},{0,0,1},{1,0}},{{-1,1,0.8f},{0,0,1},{0,0}}}};
        const std::array<std::uint32_t,6> indices = {0,1,2,0,2,3};
        const auto mesh = renderer.UploadMesh(meshVertices,indices);
        const auto materialTexture = renderer.UploadTexture(1,1,white);
        helengine::apple::MeshDrawUniform far{};
        far.Transform[0]=far.Transform[5]=far.Transform[10]=far.Transform[15]=1;
        far.Color[0]=far.Color[3]=1;
        auto near = far;
        near.Transform[0]=near.Transform[5]=0.5f;
        near.Transform[14]=-0.6f;
        near.Color[0]=0; near.Color[2]=1;
        renderer.SubmitMesh(mesh,materialTexture,far);
        renderer.SubmitMesh(mesh,materialTexture,near);
        renderer.SubmitMesh(mesh,materialTexture,far);
        auto depth = renderer.RenderOffscreen(64,64);
        if (depth[(32*64+32)*4+2] != 255 || depth[(32*64+32)*4] != 0 || depth[0] != 255) throw std::runtime_error("Indexed Metal mesh depth ordering failed");
        auto translated = near;
        translated.Transform[0]=translated.Transform[5]=0.25f;
        translated.Transform[12]=0.5f;
        renderer.SubmitMesh(mesh,materialTexture,translated);
        auto moved = renderer.RenderOffscreen(64,64);
        if (moved[(32*64+48)*4+2] != 255 || moved[(32*64+16)*4+2] != 0) throw std::runtime_error("Metal row-vector matrix payload translation failed");
        renderer.ReleaseMesh(mesh);
        renderer.ReleaseTexture(materialTexture);
        ShadowDepthValidation::Run();
        std::cout << "Metal offscreen readback and projected/radial cube shadow depths passed on " << renderer.DeviceName() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Metal offscreen validation failed: " << error.what() << '\n';
        return 1;
    }
}

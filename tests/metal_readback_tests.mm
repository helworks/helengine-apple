#include "apple/metal_renderer.hpp"
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
        std::cout << "Metal offscreen readback passed on " << renderer.DeviceName() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Metal offscreen validation failed: " << error.what() << '\n';
        return 1;
    }
}

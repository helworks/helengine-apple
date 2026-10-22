#include "apple/metal_renderer.hpp"
#include <iostream>
#include <stdexcept>

/// Runs real Metal rendering and verifies ordered quads, texture sampling, alpha and explicit resource errors.
int main() {
    try {
        helengine::apple::MetalRenderer renderer;
        const auto pixels = renderer.ValidateOffscreen();
        if (pixels.size() != 64 * 64 * 4) throw std::runtime_error("unexpected readback size");
        std::cout << "Metal offscreen readback passed on " << renderer.DeviceName() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Metal offscreen validation failed: " << error.what() << '\n';
        return 1;
    }
}

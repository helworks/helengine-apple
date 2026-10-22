#pragma once
#include <cstdint>

namespace helengine::apple {
    /// Physical pixel scissor rectangle applied after the engine's viewport and nested clip intersection.
    struct ScissorRect final {
        /// Left pixel coordinate relative to the render target.
        std::uint32_t X;
        /// Top pixel coordinate relative to the render target.
        std::uint32_t Y;
        /// Horizontal extent in physical pixels; zero suppresses the draw.
        std::uint32_t Width;
        /// Vertical extent in physical pixels; zero suppresses the draw.
        std::uint32_t Height;
    };
}

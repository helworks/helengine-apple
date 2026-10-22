#pragma once
#include <cstdint>

namespace helengine::apple {
    /// Fixed 48-byte Metal SDF payload; configurable engine values convert at this explicit GPU boundary.
    struct RoundedRectUniform final {
        /// Width, height, radius and border thickness in physical pixels.
        float Parameters[4] = {};
        /// Straight-alpha border color.
        float BorderColor[4] = {};
        /// Corner mask in element zero, followed by padding shared with the shader uint4.
        std::uint32_t Flags[4] = {};
    };
    static_assert(sizeof(RoundedRectUniform) == 48);
}

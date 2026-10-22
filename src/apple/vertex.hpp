#pragma once
#include <cstddef>

namespace helengine::apple {
    /// Fixed float GPU payload; this does not change configurable engine-side arithmetic precision.
    struct Vertex final {
        /// Normalized device position supplied after the engine-to-GPU projection boundary.
        float Position[2];
        /// Normalized texture coordinates for the material's RGBA8 texture.
        float Uv[2];
        /// Straight-alpha linear color multiplied by the sampled texel.
        float Color[4];
    };
    static_assert(sizeof(Vertex) == 32);
    static_assert(offsetof(Vertex, Uv) == 8);
    static_assert(offsetof(Vertex, Color) == 16);
}

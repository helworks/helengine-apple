#pragma once
#include <cstdint>

namespace helengine::apple {
    /// Maps one engine material texture to a reflected native Metal stage/index.
    struct MaterialTexture final {
        /// Native Metal texture index after subtracting the engine texture base.
        std::uint32_t Slot;
        /// Vertex bit 1 and fragment bit 2 select the stages which consume the texture.
        std::uint32_t Stages;
        /// Required uploaded texture identity; zero never selects an implicit fallback asset.
        std::uint64_t Texture;
    };
}

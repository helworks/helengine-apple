#pragma once
#include <cstddef>
#include <cstdint>

namespace helengine::apple {
    /// Records active native pipeline reflection requirements before any authored material can draw.
    struct MaterialRequirement final {
        /// Buffer type zero, texture type one and sampler type two correspond to distinct Metal namespaces.
        std::uint32_t Kind;
        /// Required native resource index.
        std::uint32_t Slot;
        /// Vertex bit 1 or fragment bit 2 identify the reflected stage.
        std::uint32_t Stage;
        /// Minimum constant-buffer byte size reported by Metal; zero for textures and samplers.
        std::size_t Bytes;
        /// Distinguishes separate cube sampling from 2D sampling before native resource binding.
        bool Cube = false;
    };
}

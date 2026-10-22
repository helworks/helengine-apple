#pragma once
#include "apple/vertex.hpp"
#include <cstdint>
#include <vector>

namespace helengine::apple {
    /// Retains CPU vertex data and the validated texture identity for one ordered draw.
    struct DrawPacket final {
        /// Fixed GPU payload copied at submission so caller-owned spans may expire safely.
        std::vector<Vertex> Vertices;
        /// Texture identity looked up again while encoding to detect premature releases.
        std::uint64_t Texture;
    };
}

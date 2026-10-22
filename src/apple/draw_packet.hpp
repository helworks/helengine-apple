#pragma once
#include "apple/vertex.hpp"
#include "apple/scissor_rect.hpp"
#include "apple/rounded_rect_uniform.hpp"
#include "apple/mesh_draw_uniform.hpp"
#include <optional>
#include <cstdint>
#include <vector>

namespace helengine::apple {
    /// Retains CPU vertex data and the validated texture identity for one ordered draw.
    struct DrawPacket final {
        /// Fixed GPU payload copied at submission so caller-owned spans may expire safely.
        std::vector<Vertex> Vertices;
        /// Texture identity looked up again while encoding to detect premature releases.
        std::uint64_t Texture;
        /// Clip inherited at submission time, unaffected by later stack operations.
        std::optional<ScissorRect> Clip;
        /// Selects the signed-distance pipeline instead of sampling a texture.
        bool Rounded = false;
        /// Per-draw shape dimensions, border and enabled corners.
        RoundedRectUniform Shape;
        /// Nonzero mesh identity selects indexed depth-tested drawing instead of transient 2D vertices.
        std::uint64_t Mesh = 0;
        /// Per-draw transform and tint for the native mesh validation material.
        MeshDrawUniform MeshUniforms;
    };
}

#pragma once
#include "apple/vertex.hpp"
#include "apple/scissor_rect.hpp"
#include "apple/rounded_rect_uniform.hpp"
#include "apple/mesh_draw_uniform.hpp"
#include "apple/material_buffer.hpp"
#include "apple/material_texture.hpp"
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
        /// Physical camera viewport used by indexed NDC geometry; empty selects the full target.
        std::optional<ScissorRect> MeshViewport;
        /// Selects the signed-distance pipeline instead of sampling a texture.
        bool Rounded = false;
        /// Per-draw shape dimensions, border and enabled corners.
        RoundedRectUniform Shape;
        /// Nonzero mesh identity selects indexed depth-tested drawing instead of transient 2D vertices.
        std::uint64_t Mesh = 0;
        /// Per-draw transform and tint for the native mesh validation material.
        MeshDrawUniform MeshUniforms;
        /// Nonzero identity selects an actual authored vertex/fragment pipeline instead of the native validation material.
        std::uint64_t Material = 0;
        /// Owned constant snapshots for the authored draw's reflected stages and indices.
        std::vector<MaterialBuffer> MaterialBuffers;
        /// Explicit required texture bindings retained until GPU completion.
        std::vector<MaterialTexture> MaterialTextures;
    };
}

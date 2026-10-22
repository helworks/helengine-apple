#pragma once
#include "apple/shadow_draw_uniform.hpp"
#include <cstdint>

namespace helengine::apple {
/// Describes one validated indexed caster for an actual native shadow render pass.
struct ShadowDraw final {
    /// Same-device uploaded triangle mesh identity.
    uint64_t Mesh;
    /// Exact light transform and radial-depth metadata for this caster.
    ShadowDrawUniform Uniforms;
};
}

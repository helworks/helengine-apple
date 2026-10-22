#pragma once
#include <cstdint>

namespace helengine::apple {
/// Fixed-function GPU material state copied from the existing generated render-state contract.
struct MaterialPipelineState final {
    /// Zero selects opaque output; one selects straight-alpha blending.
    uint32_t BlendMode = 1;
    /// Zero renders both windings; one culls front faces; two culls back faces.
    uint32_t CullMode = 0;
    /// Enables less-than depth comparison when requested by the authored material.
    bool DepthTest = true;
    /// Writes depth independently of whether comparison is enabled.
    bool DepthWrite = true;
};
}

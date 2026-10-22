#pragma once
#include "apple/core_shader_binding.hpp"
#include <vector>

namespace helengine::apple {
/// Associates a generated root material with its actual cooked native pipeline and copied reflection.
struct CoreMaterialResource final {
    /// Native Metal pipeline identity whose libraries have already been validated by the device.
    uint64_t Identity;
    /// Source shader identity used for exact invalidation matching.
    std::string ShaderId;
    /// Ordered stage-specific resource contracts retained independently of temporary shader asset ownership.
    std::vector<CoreShaderBinding> Bindings;
};
}

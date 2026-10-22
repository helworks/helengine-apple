#pragma once
#include "apple/material_requirement.hpp"
#import <Metal/Metal.h>
#include <vector>

namespace helengine::apple {
    /// Retains an actual authored Metal pipeline and its validated active resource requirements.
    struct MaterialResource final {
        /// Pipeline created from the cooked vertex and fragment Metal libraries.
        id<MTLRenderPipelineState> Pipeline;
        /// Native stage/index and size requirements used to reject missing material resources before GPU execution.
        std::vector<MaterialRequirement> Required;
    };
}

#pragma once

namespace helengine::apple {
    /// Fixed float32 GPU constants for native indexed/depth validation; authored materials retain their own reflected layouts.
    struct MeshDrawUniform final {
        /// Row-major engine matrix bytes; Metal reads their transpose and multiplies a column vector to preserve engine row-vector transforms.
        float Transform[16] = {};
        /// Straight-alpha material tint multiplied by the explicitly bound diffuse texture.
        float Color[4] = {};
    };
    static_assert(sizeof(MeshDrawUniform)==80);
}

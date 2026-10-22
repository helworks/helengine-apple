#pragma once
#include <cstddef>

namespace helengine::apple {
    /// Fixed 32-byte GPU mesh vertex matching the engine position/normal/UV payload boundary.
    struct MeshVertex final {
        /// Local-space XYZ position converted from configured engine storage at upload.
        float Position[3];
        /// Local-space normal used by the native validation material.
        float Normal[3];
        /// Normalized diffuse texture coordinates.
        float Uv[2];
    };
    static_assert(sizeof(MeshVertex)==32);
    static_assert(offsetof(MeshVertex,Normal)==12);
    static_assert(offsetof(MeshVertex,Uv)==24);
}

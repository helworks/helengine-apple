#pragma once
#import <Metal/Metal.h>
#include <cstdint>

namespace helengine::apple {
    /// Owns uploaded Metal mesh buffers independently of the caller's CPU vertex/index spans.
    struct MeshResource final {
        /// Interleaved fixed-layout position/normal/UV buffer.
        id<MTLBuffer> Vertices;
        /// Validated 32-bit triangle index buffer.
        id<MTLBuffer> Indices;
        /// Number of indices encoded by each draw.
        std::uint32_t IndexCount;
    };
}

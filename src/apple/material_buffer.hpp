#pragma once
#include <cstdint>
#include <vector>

namespace helengine::apple {
    /// Owns a reflected constant buffer snapshot for one authored material draw.
    struct MaterialBuffer final {
        /// Native Metal buffer index after subtracting the engine constant-buffer base.
        std::uint32_t Slot;
        /// Vertex bit 1 and fragment bit 2 select the stages which consume this payload.
        std::uint32_t Stages;
        /// Exact authored/reflected constant bytes copied before the caller may recycle its material state.
        std::vector<std::uint8_t> Bytes;
    };
}

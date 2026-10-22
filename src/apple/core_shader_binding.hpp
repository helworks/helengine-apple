#pragma once
#include "ShaderResourceType.hpp"
#include <cstdint>
#include <string>

namespace helengine::apple {
/// Preserves cooked shader reflection after the content loader releases temporary asset wrappers.
struct CoreShaderBinding final {
    /// Authored shader binding name used to resolve material property overrides.
    std::string Name;
    /// Explicit reflected resource kind; unsupported native kinds remain errors.
    ::ShaderResourceType Type;
    /// Engine unified slot before the documented default binding-policy kind shift is removed.
    int32_t Slot;
    /// Reflected constant-buffer byte count, independent of configurable engine math storage.
    int32_t Size;
    /// Exact vertex or fragment stage mask consumed by the native renderer.
    uint32_t Stage;
};
}

#pragma once
#include <cstdint>

class Core;
namespace helengine::apple {
class SdlInputBackend;
/// Receives optional deterministic test observations without coupling the generic player to a project's component types.
class CorePlayerObserver {
public:
    /// Allows test-specific retained state to be destroyed through this interface.
    virtual ~CorePlayerObserver() = default;
    /// Injects only explicit test events before the real Core.Update captures its native input provider.
    virtual void BeforeUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) = 0;
    /// Checks authored state after Core.Update, preserving the actual gameplay/input path.
    virtual void AfterUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) = 0;
    /// Validates the entire completed run before normal Core teardown.
    virtual void ValidateComplete(::Core& core) = 0;
};
}

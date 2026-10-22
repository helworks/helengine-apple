#pragma once
#include "apple/apple_run_options.hpp"
#include <memory>
#include <vector>

class RuntimeSceneCatalog;
class StandardPlatformInputConfiguration;
class StandardPlatformActionBinding;
namespace helengine::apple {
class CorePlayerObserver;
/// Runs the actual generated Core with packaged manifests and the shared Metal renderer.
class ApplePlayer final {
public:
    /// Loads the requested packaged scene and validates genuine GPU work in explicit offscreen mode.
    static int Run(const AppleRunOptions& options,CorePlayerObserver* observer=nullptr);
    /// Resolves startup by its manifest path rather than incidental scene catalog ordering.
    static std::string ResolveSceneId(::RuntimeSceneCatalog* catalog,const std::string& requested,const std::string& startupPath);
private:
    /// Creates the generated runtime catalog, respecting its cloned entry ownership contract.
    static ::RuntimeSceneCatalog* CreateSceneCatalog();
    /// Creates standard bindings whose borrowed pointers remain alive for the entire Core lifetime.
    static ::StandardPlatformInputConfiguration* CreateStandardInput(std::vector<std::unique_ptr<::StandardPlatformActionBinding>>& owners);
};
}

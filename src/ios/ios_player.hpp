#pragma once
#include <SDL3/SDL.h>
#include <memory>

namespace helengine::apple {
/// Owns the real generated Core inside SDL's UIKit-compatible application callback lifecycle.
class IosPlayer final {
    /// Keeps generated providers, native presentation resources and borrowed manifest data alive through Core shutdown.
    class State;
    /// Owns application state until SDL_AppQuit completes explicit teardown.
    std::unique_ptr<State> Resources;
public:
    /// Creates an empty callback host without initializing SDL or presenting a window.
    IosPlayer();
    /// Completes idempotent shutdown if SDL initialization or a later callback failed.
    ~IosPlayer();
    /// Creates the SDL Metal window, real providers and startup scene from packaged manifests.
    void Initialize();
    /// Dispatches native input and suspends GPU work before UIKit backgrounds the application.
    void HandleEvent(const SDL_Event& event);
    /// Advances one foreground frame, pacing unavailable drawable retries without a blocking run loop.
    SDL_AppResult Iterate();
    /// Discards pending GPU work, disposes Core once and then releases its borrowed providers.
    void Shutdown() noexcept;
};
}

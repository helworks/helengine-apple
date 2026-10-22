#include "apple/frame_lifecycle.hpp"
#include <stdexcept>

/// Exercises suspended, zero-sized and drawable-unavailable frames without creating a desktop window.
int main() {
    helengine::apple::FrameLifecycle state;
    if (state.CanRender()) throw std::runtime_error("unconfigured viewport rendered");
    state.Resize(640, 480);
    if (!state.CanRender()) throw std::runtime_error("valid viewport did not render");
    state.Suspend();
    if (state.CanRender()) throw std::runtime_error("suspended application rendered");
    state.Resume();
    state.Resize(0, 480);
    if (state.CanRender()) throw std::runtime_error("zero-width drawable rendered");
    state.Resize(640, 480);
    state.DrawableUnavailable();
    if (!state.ShouldWait()) throw std::runtime_error("drawable retry would busy-loop");
    state.DrawableAvailable();
    if (state.ShouldWait()) throw std::runtime_error("drawable availability did not clear retry state");
    return 0;
}

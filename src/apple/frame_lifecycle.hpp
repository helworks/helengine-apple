#pragma once
#include <cstdint>

namespace helengine::apple {
    /// Tracks whether a native frame has a usable pixel viewport and is permitted by application lifecycle.
    class FrameLifecycle final {
        /// Current drawable width in pixels, separate from logical SDL coordinates.
        std::uint32_t Width = 0;
        /// Current drawable height in pixels.
        std::uint32_t Height = 0;
        /// Whether the operating system has suspended presentation.
        bool Suspended = false;
        /// Whether the latest drawable acquisition requires a paced retry.
        bool WaitingForDrawable = false;
    public:
        /// Sets the current physical drawable extent; zero-sized viewports cannot render.
        void Resize(std::uint32_t width, std::uint32_t height) { Width = width; Height = height; }
        /// Stops submissions before the application enters the background.
        void Suspend() { Suspended = true; }
        /// Permits presentation again after foreground activation.
        void Resume() { Suspended = false; }
        /// Records that CAMetalLayer temporarily could not provide a drawable.
        void DrawableUnavailable() { WaitingForDrawable = true; }
        /// Clears a paced retry after obtaining a real drawable.
        void DrawableAvailable() { WaitingForDrawable = false; }
        /// Returns whether resource dimensions and lifecycle permit drawable acquisition.
        bool CanRender() const { return !Suspended && Width > 0 && Height > 0; }
        /// Requests an event wait when rendering cannot currently make progress.
        bool ShouldWait() const { return !CanRender() || WaitingForDrawable; }
    };
}

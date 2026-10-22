#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_metal.h>
#include "apple/frame_lifecycle.hpp"
#include "apple/metal_renderer.hpp"
#include <memory>
#include <exception>

namespace helengine::apple {
    /// Owns SDL lifecycle, presentation view and the explicitly labelled native diagnostic renderer.
    class DiagnosticApplication final {
    public:
        /// SDL-managed window; physical pixel dimensions are queried separately from logical size.
        SDL_Window* Window = nullptr;
        /// SDL-managed Metal view destroyed before its parent window.
        SDL_MetalView View = nullptr;
        /// Shared renderer owns all GPU resources.
        std::unique_ptr<MetalRenderer> Renderer;
        /// Lifecycle guard prevents background or zero-sized presentation.
        FrameLifecycle Lifecycle;
        /// Releases the Metal view before SDL window teardown.
        ~DiagnosticApplication() {
            Renderer.reset();
            if (View) SDL_Metal_DestroyView(View);
            if (Window) SDL_DestroyWindow(Window);
        }
        /// Handles lifecycle and resizes using pixel dimensions instead of UI coordinate dimensions.
        void HandleEvent(const SDL_Event& event) {
            if (event.type == SDL_EVENT_WILL_ENTER_BACKGROUND) { Lifecycle.Suspend(); Renderer->DiscardFrame(); }
            if (event.type == SDL_EVENT_DID_ENTER_FOREGROUND) Lifecycle.Resume();
            int width = 0;
            int height = 0;
            if (!SDL_GetWindowSizeInPixels(Window, &width, &height)) throw std::runtime_error(SDL_GetError());
            Lifecycle.Resize(width > 0 ? width : 0, height > 0 ? height : 0);
        }
        /// Presents only available drawable frames and yields CPU while inactive.
        void Iterate() {
            if (Lifecycle.ShouldWait()) SDL_Delay(20);
            if (!Lifecycle.CanRender()) return;
            int width = 0;
            int height = 0;
            if (!SDL_GetWindowSizeInPixels(Window, &width, &height)) throw std::runtime_error(SDL_GetError());
            if (Renderer->Present(SDL_Metal_GetLayer(View), width > 0 ? width : 0, height > 0 ? height : 0)) Lifecycle.DrawableAvailable();
            else Lifecycle.DrawableUnavailable();
        }
    };
}

/// Creates an explicit diagnostic host; it cannot be mistaken for the generated game player.
SDL_AppResult SDL_AppInit(void** appstate, int, char**) {
    try {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) throw std::runtime_error(SDL_GetError());
        auto app = std::make_unique<helengine::apple::DiagnosticApplication>();
        app->Window = SDL_CreateWindow("HelEngine Apple native diagnostic", 960, 540, SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!app->Window) throw std::runtime_error(SDL_GetError());
        app->View = SDL_Metal_CreateView(app->Window);
        if (!app->View) throw std::runtime_error(SDL_GetError());
        app->Renderer = std::make_unique<helengine::apple::MetalRenderer>();
        SDL_Event initial{};
        app->HandleEvent(initial);
        *appstate = app.release();
        return SDL_APP_CONTINUE;
    } catch (const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Apple initialization: %s", error.what()); return SDL_APP_FAILURE; }
}

/// Routes SDL events and propagates native failures to the application exit result.
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    try { static_cast<helengine::apple::DiagnosticApplication*>(appstate)->HandleEvent(*event); return SDL_APP_CONTINUE; }
    catch (const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Apple event: %s", error.what()); return SDL_APP_FAILURE; }
}

/// Runs the paced frame loop without silently ignoring GPU errors.
SDL_AppResult SDL_AppIterate(void* appstate) {
    try { static_cast<helengine::apple::DiagnosticApplication*>(appstate)->Iterate(); return SDL_APP_CONTINUE; }
    catch (const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Apple frame: %s", error.what()); return SDL_APP_FAILURE; }
}

/// Tears down resources while SDL is still initialized.
void SDL_AppQuit(void* appstate, SDL_AppResult) { delete static_cast<helengine::apple::DiagnosticApplication*>(appstate); SDL_Quit(); }

#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <stdexcept>
#include <utility>

namespace helengine::apple {
/// Owns an SDL Metal view and keeps logical window coordinates separate from drawable pixels.
class AppleSdlHost final {
    /// Retains the SDL application window until its native Metal view is destroyed.
    SDL_Window* Window = nullptr;
    /// Retains the CAMetalLayer adapter required by the renderer.
    SDL_MetalView View = nullptr;
public:
    /// Creates a normal SDL application window only when interactive execution was explicitly selected.
    AppleSdlHost(int width,int height) {
        if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        Window=SDL_CreateWindow("HelEngine Apple",width,height,SDL_WINDOW_METAL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!Window) { SDL_Quit(); throw std::runtime_error(SDL_GetError()); }
        View=SDL_Metal_CreateView(Window);
        if (!View) { SDL_DestroyWindow(Window); Window=nullptr; SDL_Quit(); throw std::runtime_error(SDL_GetError()); }
    }
    /// Destroys native view ownership before the SDL window and subsystem.
    ~AppleSdlHost() { if (View) SDL_Metal_DestroyView(View); if (Window) SDL_DestroyWindow(Window); SDL_Quit(); }
    /// Exposes only the selected application's own Metal layer for presentation.
    void* GetLayer() const { return SDL_Metal_GetLayer(View); }
    /// Reads physical drawable dimensions without sampling desktop content.
    std::pair<int,int> GetDrawableSize() const { int width=0,height=0; if (!SDL_GetWindowSizeInPixels(Window,&width,&height)) throw std::runtime_error(SDL_GetError()); return {width,height}; }
};
}

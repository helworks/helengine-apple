#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include "ios_player.hpp"
#include "ios_input_backend.hpp"
#include "apple/frame_lifecycle.hpp"
#include "apple/generated_disposable_deleter.hpp"
#include "apple/core_render_manager_2d.hpp"
#include "apple/core_render_manager_3d.hpp"
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "PlatformInfo.hpp"
#include "PlatformSettingsConfiguration.hpp"
#include "HostFileSystemContentStreamSource.hpp"
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "SceneManager.hpp"
#include "SceneLoadMode.hpp"
#include "GeneratedRuntimeModuleRegistration.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "InputControlId.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#import <Foundation/Foundation.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace helengine::apple {
/// Retains every provider borrowed by generated Core and every native object needed by its teardown.
class IosPlayer::State final {
public:
    /// Owns the UIKit-backed SDL window until Metal and generated providers have been released.
    SDL_Window* Window=nullptr;
    /// Owns the SDL CAMetalLayer bridge for the lifetime of presentation.
    SDL_MetalView View=nullptr;
    /// Owns the shared Metal device and synchronous frame queues.
    std::unique_ptr<MetalRenderer> Renderer;
    /// Owns the generated sprite and text resource provider.
    std::unique_ptr<CoreRenderManager2D> Render2D;
    /// Owns real mesh/material/shadow resources and borrows the shared 2D bridge.
    std::unique_ptr<CoreRenderManager3D> Render3D;
    /// Owns portable input snapshots until Core no longer retains current/previous frames.
    std::unique_ptr<IosInputBackend> Input;
    /// Owns the packaged content source, explicitly rooted in the iOS bundle.
    std::unique_ptr<::HostFileSystemContentStreamSource> Content;
    /// Owns the cloned generated runtime scene catalog.
    GeneratedDisposableOwner<::RuntimeSceneCatalog> Catalog;
    /// Owns bindings borrowed by the generated input configuration and Core input system.
    std::vector<std::unique_ptr<::StandardPlatformActionBinding>> BindingOwners;
    /// Owns the copied binding container while retaining borrowed binding objects above.
    GeneratedDisposableOwner<::StandardPlatformInputConfiguration> StandardInput;
    /// Owns the actual selected platform settings from the generated manifest.
    GeneratedDisposableOwner<::PlatformSettingsConfiguration> Settings;
    /// Retains initialization options because generated Core borrows this object.
    std::unique_ptr<::CoreInitializationOptions> Initialization;
    /// Retains platform identity until generated Core teardown.
    std::unique_ptr<::PlatformInfo> Platform;
    /// Owns generated runtime state with explicit callback disposal before wrapper destruction.
    std::unique_ptr<::Core> Engine;
    /// Ensures partial Initialize failures still invoke Core.Dispose exactly once.
    bool NeedsCoreShutdown=false;
    /// Gates submissions while backgrounded or without usable drawable dimensions.
    FrameLifecycle Lifecycle;
    /// Retains the renderer's current nonzero viewport for resize dispatch.
    int Width=HELENGINE_APPLE_DEFAULT_WIDTH,Height=HELENGINE_APPLE_DEFAULT_HEIGHT;
    /// Excludes suspended wall time from the first resumed simulation frame.
    bool HasPreviousTime=false;
    /// Retains the last foreground update timestamp.
    std::chrono::steady_clock::time_point PreviousTime;
    /// Reports secondary teardown failures while preserving callback failure and generated exception ownership.
    template<class T> static void DisposeSafely(T* value,const char* name) noexcept {
        if(!value) return;
        try { value->Dispose(); }
        catch(const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS %s shutdown: %s",name,error.what()); }
        catch(const std::exception* error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS %s shutdown: %s",name,error?error->what():"null generated exception"); delete error; }
        catch(...) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS %s shutdown: unknown exception",name); }
    }
    /// Creates a catalog whose generated constructor clones temporary entries.
    static ::RuntimeSceneCatalog* CreateCatalog() {
        size_t count=0; const auto* entries=he_runtime_scene_catalog_entries(&count);
        if(!entries||!count||count>INT32_MAX) throw std::runtime_error("iOS requires a nonempty generated scene catalog.");
        auto array=std::make_unique<Array<::RuntimeSceneCatalogEntry*>>(static_cast<int32_t>(count));
        std::vector<GeneratedDisposableOwner<::RuntimeSceneCatalogEntry>> owners;
        for(size_t i=0;i<count;++i) {
            if(!entries[i].SceneId||!entries[i].CookedRelativePath) throw std::runtime_error("iOS generated scene catalog contains an invalid entry.");
            owners.emplace_back(new ::RuntimeSceneCatalogEntry(entries[i].SceneId,entries[i].CookedRelativePath)); (*array)[static_cast<int32_t>(i)]=owners.back().get();
        }
        return new ::RuntimeSceneCatalog(array.get());
    }
    /// Creates standard action configuration without releasing the bindings borrowed by its copied list.
    void CreateStandardInput() {
        size_t count=0; const auto* entries=he_runtime_standard_platform_action_entries(&count);
        if(count&&!entries) throw std::runtime_error("iOS generated standard input entries are missing.");
        List<::StandardPlatformActionBinding*> bindings;
        for(size_t i=0;i<count;++i) { const auto& entry=entries[i]; BindingOwners.emplace_back(new ::StandardPlatformActionBinding(static_cast<::StandardPlatformAction>(entry.ActionId),::InputControlId(static_cast<::InputDeviceKind>(entry.DeviceKind),static_cast<::InputControlKind>(entry.ControlKind),entry.DeviceIndex,entry.ControlIndex))); bindings.Add(BindingOwners.back().get()); }
        StandardInput.reset(new ::StandardPlatformInputConfiguration(&bindings));
    }
    /// Resolves the exact authored startup path and rejects missing or ambiguous packaged scenes.
    std::string StartupScene() const {
        const char* startup=he_get_runtime_startup_scene_relative_path();
        if(!startup||!*startup) throw std::runtime_error("iOS generated startup scene path is empty.");
        std::string selected; auto* entries=Catalog->get_Entries();
        for(int32_t i=0;i<entries->get_Length();++i) { auto* entry=(*entries)[i]; if(entry->get_CookedRelativePath()==startup) { if(!selected.empty()) throw std::runtime_error("iOS generated startup scene path is ambiguous."); selected=entry->get_SceneId().raw(); } }
        if(selected.empty()) throw std::runtime_error("iOS startup scene is absent from the packaged catalog."); return selected;
    }
    /// Queries physical pixels and cancels contacts when resize invalidates their coordinate space.
    void RefreshDrawable() {
        int width=0,height=0;
        if(!SDL_GetWindowSizeInPixels(Window,&width,&height)) throw std::runtime_error(std::string("iOS drawable extent: ")+SDL_GetError());
        Lifecycle.Resize(static_cast<uint32_t>(std::max(width,0)),static_cast<uint32_t>(std::max(height,0))); Input->SetDrawableSize(std::max(width,0),std::max(height,0));
        if(width>0&&height>0&&(width!=Width||height!=Height)) { Width=width; Height=height; Render3D->OnWindowResize(1,width,height); HasPreviousTime=false; }
    }
};
IosPlayer::IosPlayer():Resources(std::make_unique<State>()) {}
IosPlayer::~IosPlayer() { Shutdown(); }
void IosPlayer::Initialize() {
    auto& s=*Resources;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0"); SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS,"0");
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMEPAD)) throw std::runtime_error(std::string("iOS SDL_Init: ")+SDL_GetError());
    s.Window=SDL_CreateWindow("HelEngine",s.Width,s.Height,SDL_WINDOW_METAL|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE);
    if(!s.Window) throw std::runtime_error(std::string("iOS SDL_CreateWindow: ")+SDL_GetError());
    s.View=SDL_Metal_CreateView(s.Window); if(!s.View) throw std::runtime_error(std::string("iOS SDL_Metal_CreateView: ")+SDL_GetError());
    NSString* root=NSBundle.mainBundle.resourcePath; if(!root||!root.UTF8String) throw std::runtime_error("iOS application bundle has no content resource root.");
    s.Renderer=std::make_unique<MetalRenderer>(); s.Render2D=std::make_unique<CoreRenderManager2D>(*s.Renderer);
    s.Render2D->SetTextCommandReuseMemoryBudgetBytes(he_get_runtime_render2d_text_command_reuse_memory_budget_bytes());
    s.Render3D=std::make_unique<CoreRenderManager3D>(*s.Renderer,*s.Render2D); s.Input=std::make_unique<IosInputBackend>(s.Window);
    s.Content=std::make_unique<::HostFileSystemContentStreamSource>(std::string(root.UTF8String)); s.Catalog.reset(State::CreateCatalog()); s.CreateStandardInput(); s.Settings.reset(he_create_runtime_platform_settings());
    s.Initialization=std::make_unique<::CoreInitializationOptions>(); auto& options=*s.Initialization;
    options.ContentStreamSource=s.Content.get(); options.SceneCatalog=s.Catalog.get(); options.StandardPlatformInputConfiguration=s.StandardInput.get(); options.PlatformSettingsConfiguration=s.Settings.get(); options.SceneLoadingBudgetMilliseconds=he_get_runtime_scene_loading_budget_milliseconds();
    s.Platform=std::make_unique<::PlatformInfo>(he_get_runtime_platform_name(),he_get_runtime_platform_version()); s.Engine=std::make_unique<::Core>(&options); s.NeedsCoreShutdown=true;
    s.Render3D->AddWindow(1,s.Width,s.Height); s.Render3D->set_ShadowsEnabled(he_get_runtime_shadows_enabled()); s.RefreshDrawable();
    s.Engine->Initialize(s.Render3D.get(),s.Render2D.get(),s.Input.get(),s.Platform.get(),&options); RegisterGeneratedRuntimeModules(s.Engine.get());
    s.Engine->get_SceneManager()->LoadScene(s.StartupScene(),::SceneLoadMode::Single); s.Engine->get_SceneManager()->CommitPendingOperationsAtFrameBoundary();
}
void IosPlayer::HandleEvent(const SDL_Event& event) {
    auto& s=*Resources; s.Input->HandleEvent(event);
    if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND||event.type==SDL_EVENT_DID_ENTER_BACKGROUND||event.type==SDL_EVENT_WINDOW_MINIMIZED) { s.Lifecycle.Suspend(); s.Input->Suspend(); s.Renderer->DiscardFrame(); s.HasPreviousTime=false; }
    else if(event.type==SDL_EVENT_DID_ENTER_FOREGROUND||event.type==SDL_EVENT_WINDOW_RESTORED) { s.Lifecycle.Resume(); s.Input->Resume(); s.HasPreviousTime=false; s.RefreshDrawable(); }
    else if(event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) s.RefreshDrawable();
}
SDL_AppResult IosPlayer::Iterate() {
    auto& s=*Resources; s.RefreshDrawable();
    if(!s.Lifecycle.CanRender()) { s.HasPreviousTime=false; SDL_Delay(16); return SDL_APP_CONTINUE; }
    if(s.Lifecycle.ShouldWait()) SDL_Delay(16);
    const auto now=std::chrono::steady_clock::now(); const double elapsed=s.HasPreviousTime?std::chrono::duration<double>(now-s.PreviousTime).count():0.0; s.PreviousTime=now; s.HasPreviousTime=true;
    const auto updateStart=std::chrono::steady_clock::now(); s.Engine->Update(elapsed); const auto drawStart=std::chrono::steady_clock::now(); s.Engine->Draw();
    if(s.Renderer->Present(SDL_Metal_GetLayer(s.View),s.Width,s.Height)) s.Lifecycle.DrawableAvailable(); else s.Lifecycle.DrawableUnavailable();
    const auto end=std::chrono::steady_clock::now(); s.Engine->SetHostFrameTimings(std::chrono::duration<double,std::milli>(drawStart-updateStart).count(),std::chrono::duration<double,std::milli>(end-drawStart).count());
    s.Render3D->FlushReleasedAssets(); s.Render2D->FlushReleasedTextures(); return SDL_APP_CONTINUE;
}
void IosPlayer::Shutdown() noexcept {
    if(!Resources) return; auto& s=*Resources;
    if(s.Renderer) s.Renderer->DiscardFrame();
    const bool coreOwns3D=s.Engine&&s.Engine->get_RenderManager3D()==s.Render3D.get();
    const bool coreOwns2D=s.Engine&&s.Engine->get_RenderManager2D()==s.Render2D.get();
    if(s.NeedsCoreShutdown) {
        s.NeedsCoreShutdown=false;
        State::DisposeSafely(s.Engine.get(),"Core");
    }
    if(!coreOwns3D) State::DisposeSafely(s.Render3D.get(),"unattached Render3D");
    if(!coreOwns2D) State::DisposeSafely(s.Render2D.get(),"unattached Render2D");
    s.Engine.reset(); s.Initialization.reset(); s.Platform.reset(); s.Settings.reset(); s.StandardInput.reset(); s.BindingOwners.clear(); s.Catalog.reset(); s.Content.reset(); s.Input.reset(); s.Render3D.reset(); s.Render2D.reset(); s.Renderer.reset();
    if(s.View) { SDL_Metal_DestroyView(s.View); s.View=nullptr; } if(s.Window) { SDL_DestroyWindow(s.Window); s.Window=nullptr; }
}
}
/// Initializes the real callback host; SDL still invokes AppQuit after partial initialization failure.
SDL_AppResult SDL_AppInit(void** state,int,char**) {
    try { auto host=std::make_unique<helengine::apple::IosPlayer>(); *state=host.release(); static_cast<helengine::apple::IosPlayer*>(*state)->Initialize(); return SDL_APP_CONTINUE; }
    catch(const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS initialization: %s",error.what()); }
    catch(const std::exception* error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS initialization: %s",error?error->what():"null generated exception"); delete error; }
    catch(...) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS initialization: unknown exception"); } return SDL_APP_FAILURE;
}
/// Dispatches operating-system callbacks without running a nested UIKit event loop.
SDL_AppResult SDL_AppEvent(void* state,SDL_Event* event) {
    if(event->type==SDL_EVENT_QUIT||event->type==SDL_EVENT_TERMINATING) return SDL_APP_SUCCESS;
    try { static_cast<helengine::apple::IosPlayer*>(state)->HandleEvent(*event); return SDL_APP_CONTINUE; }
    catch(const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS event: %s",error.what()); }
    catch(const std::exception* error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS event: %s",error?error->what():"null generated exception"); delete error; }
    catch(...) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS event: unknown exception"); } return SDL_APP_FAILURE;
}
/// Advances one UIKit-approved iteration and converts generated exception ownership into SDL failure.
SDL_AppResult SDL_AppIterate(void* state) {
    try { return static_cast<helengine::apple::IosPlayer*>(state)->Iterate(); }
    catch(const std::exception& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS frame: %s",error.what()); }
    catch(const std::exception* error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS frame: %s",error?error->what():"null generated exception"); delete error; }
    catch(...) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"iOS frame: unknown exception"); } return SDL_APP_FAILURE;
}
/// Tears down Core and Metal before SDL destroys UIKit-backed platform services.
void SDL_AppQuit(void* state,SDL_AppResult) { delete static_cast<helengine::apple::IosPlayer*>(state); SDL_Quit(); }

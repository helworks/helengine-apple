#include "apple/apple_player.hpp"
#include "apple/generated_disposable_deleter.hpp"
#include "apple/core_render_manager_2d.hpp"
#include "apple/core_render_manager_3d.hpp"
#include "apple/sdl_input_backend.hpp"
#include "apple/apple_sdl_host.hpp"
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "apple/core_player_observer.hpp"
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
#include "InputDeviceKind.hpp"
#include "InputControlKind.hpp"
#include "StandardPlatformAction.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace helengine::apple {
::RuntimeSceneCatalog* ApplePlayer::CreateSceneCatalog() {
    size_t count=0; const auto* entries=he_runtime_scene_catalog_entries(&count);
    if (!entries || !count || count>INT32_MAX) { throw std::runtime_error("Native Apple player requires a nonempty generated runtime scene catalog."); }
    auto catalog=std::make_unique<Array<::RuntimeSceneCatalogEntry*>>(static_cast<int32_t>(count));
    std::vector<GeneratedDisposableOwner<::RuntimeSceneCatalogEntry>> temporaryOwners;
    for (size_t index=0;index<count;++index) {
        if (!entries[index].SceneId || !entries[index].CookedRelativePath) { throw std::runtime_error("Generated scene catalog contains an invalid entry."); }
        temporaryOwners.emplace_back(new ::RuntimeSceneCatalogEntry(entries[index].SceneId,entries[index].CookedRelativePath));
        (*catalog)[static_cast<int32_t>(index)]=temporaryOwners.back().get();
    }
    return new ::RuntimeSceneCatalog(catalog.get());
}

::StandardPlatformInputConfiguration* ApplePlayer::CreateStandardInput(std::vector<std::unique_ptr<::StandardPlatformActionBinding>>& bindingOwners) {
    size_t count=0; const auto* entries=he_runtime_standard_platform_action_entries(&count);
    auto bindings=std::make_unique<List<::StandardPlatformActionBinding*>>();
    for (size_t index=0;index<count;++index) {
        const auto& entry=entries[index];
        bindingOwners.emplace_back(new ::StandardPlatformActionBinding(static_cast<::StandardPlatformAction>(entry.ActionId),::InputControlId(static_cast<::InputDeviceKind>(entry.DeviceKind),static_cast<::InputControlKind>(entry.ControlKind),entry.DeviceIndex,entry.ControlIndex)));
        bindings->Add(bindingOwners.back().get());
    }
    return new ::StandardPlatformInputConfiguration(bindings.get());
}

std::string ApplePlayer::ResolveSceneId(::RuntimeSceneCatalog* catalog,const std::string& requestedSceneId,const std::string& startupPath) {
    if (!catalog) { throw std::invalid_argument("Apple scene selection requires an initialized runtime catalog."); }
    if (!requestedSceneId.empty()) { ::RuntimeSceneCatalogEntry* entry=nullptr; if (!catalog->TryGetEntry__out1(requestedSceneId,entry)) { throw std::runtime_error("Requested scene is absent from the actual packaged catalog: " + requestedSceneId); } return entry->get_SceneId().raw(); }
    if (startupPath.empty()) { throw std::runtime_error("Generated Apple startup manifest has no cooked scene path."); }
    std::string selected;
    auto* entries=catalog->get_Entries();
    for (int32_t index=0;index<entries->get_Length();++index) { auto* entry=(*entries)[index]; if (entry->get_CookedRelativePath()==startupPath) { if (!selected.empty()) { throw std::runtime_error("Generated startup scene path is ambiguous in the packaged catalog: " + startupPath); } selected=entry->get_SceneId().raw(); } }
    if (selected.empty()) { throw std::runtime_error("Generated startup scene is absent from the packaged catalog: " + startupPath); }
    return selected;
}

int ApplePlayer::Run(const AppleRunOptions& options,CorePlayerObserver* observer) {
    std::unique_ptr<AppleSdlHost> window;
    if (!options.Offscreen) window=std::make_unique<AppleSdlHost>(HELENGINE_APPLE_DEFAULT_WIDTH,HELENGINE_APPLE_DEFAULT_HEIGHT);
    MetalRenderer renderer;
    CoreRenderManager2D render2d(renderer);
    render2d.SetTextCommandReuseMemoryBudgetBytes(he_get_runtime_render2d_text_command_reuse_memory_budget_bytes());
    CoreRenderManager3D render3d(renderer,render2d);
    SdlInputBackend input(options.Offscreen);
    auto content=std::make_unique<::HostFileSystemContentStreamSource>(options.ContentRoot);
    GeneratedDisposableOwner<::RuntimeSceneCatalog> catalog(CreateSceneCatalog());
    std::vector<std::unique_ptr<::StandardPlatformActionBinding>> bindingOwners;
    GeneratedDisposableOwner<::StandardPlatformInputConfiguration> standardInput(CreateStandardInput(bindingOwners));
    GeneratedDisposableOwner<::PlatformSettingsConfiguration> settings(he_create_runtime_platform_settings());
    ::CoreInitializationOptions initialization;
    initialization.ContentStreamSource=content.get(); initialization.SceneCatalog=catalog.get(); initialization.StandardPlatformInputConfiguration=standardInput.get();
    initialization.SceneLoadingBudgetMilliseconds=he_get_runtime_scene_loading_budget_milliseconds(); initialization.PlatformSettingsConfiguration=settings.get();
    ::Core core(&initialization);
    ::PlatformInfo platform(he_get_runtime_platform_name(),he_get_runtime_platform_version());
    int width=HELENGINE_APPLE_DEFAULT_WIDTH,height=HELENGINE_APPLE_DEFAULT_HEIGHT;
    if (window) { const auto extent=window->GetDrawableSize(); width=extent.first; height=extent.second; }
    render3d.AddWindow(1,width,height); render3d.set_ShadowsEnabled(he_get_runtime_shadows_enabled());
    bool coreNeedsShutdown=false;
    try {
        coreNeedsShutdown=true;
        core.Initialize(&render3d,&render2d,&input,&platform,&initialization);
        RegisterGeneratedRuntimeModules(&core);
        const std::string scene=ResolveSceneId(catalog.get(),options.SceneId,he_get_runtime_startup_scene_relative_path());
        std::cout << "core=generated scene=" << scene << " catalog_count=" << catalog->get_Entries()->get_Length() << " device=" << renderer.DeviceName() << '\n';
        core.get_SceneManager()->LoadScene(scene,::SceneLoadMode::Single);
        core.get_SceneManager()->CommitPendingOperationsAtFrameBoundary();
        bool running=true,suspended=false;
        uint32_t frames=0;
        std::vector<uint8_t> pixels;
        auto previous=std::chrono::steady_clock::now();
        while (running && (!options.Frames || frames<options.Frames)) {
            if (window) {
                SDL_Event event {};
                while (SDL_PollEvent(&event)) {
                    input.HandleEvent(event);
                    if (event.type==SDL_EVENT_QUIT) running=false;
                    else if (event.type==SDL_EVENT_WILL_ENTER_BACKGROUND || event.type==SDL_EVENT_WINDOW_MINIMIZED) { suspended=true; renderer.DiscardFrame(); }
                    else if (event.type==SDL_EVENT_DID_ENTER_FOREGROUND || event.type==SDL_EVENT_WINDOW_RESTORED) suspended=false;
                }
                const auto extent=window->GetDrawableSize();
                if (suspended || extent.first<=0 || extent.second<=0) { SDL_Delay(16); previous=std::chrono::steady_clock::now(); continue; }
                if (extent.first!=width || extent.second!=height) { width=extent.first; height=extent.second; render3d.OnWindowResize(1,width,height); }
            }
            const auto now=std::chrono::steady_clock::now();
            const double elapsed=options.Offscreen ? 1.0/60.0 : std::chrono::duration<double>(now-previous).count(); previous=now;
            if (observer) observer->BeforeUpdate(core,input,frames);
            const auto updateStart=std::chrono::steady_clock::now(); core.Update(elapsed);
            if (observer) observer->AfterUpdate(core,input,frames);
            const auto drawStart=std::chrono::steady_clock::now(); core.Draw();
            if (window) { if (!renderer.Present(window->GetLayer(),width,height)) SDL_Delay(16); }
            else pixels=renderer.RenderOffscreen(width,height);
            const auto drawEnd=std::chrono::steady_clock::now();
            core.SetHostFrameTimings(std::chrono::duration<double,std::milli>(drawStart-updateStart).count(),std::chrono::duration<double,std::milli>(drawEnd-drawStart).count());
            render2d.FlushReleasedTextures(); ++frames;
        }
        uint64_t checksum=1469598103934665603ULL;
        for (uint8_t byte : pixels) checksum=(checksum^byte)*1099511628211ULL;
        bool nonuniform=false;
        for (size_t index=4;index+3<pixels.size();index+=4) if (pixels[index]!=pixels[0] || pixels[index+1]!=pixels[1] || pixels[index+2]!=pixels[2] || pixels[index+3]!=pixels[3]) { nonuniform=true; break; }
        std::cout << "core_frames=" << frames << " draw_calls=" << render3d.get_LastDrawCallCount() << " mesh_visible_samples=" << renderer.GetLastVisibleMeshSamples() << " rgba_bytes=" << pixels.size() << " checksum=" << checksum << " nonuniform=" << nonuniform << '\n';
        if (options.Offscreen && (!frames || render3d.get_LastDrawCallCount()<=0 || !nonuniform)) throw std::runtime_error("Generated-core offscreen run produced no drawable content; menu validation failed");
        if (observer) observer->ValidateComplete(core);
        coreNeedsShutdown=false; core.Dispose();
        return 0;
    } catch (...) {
        renderer.DiscardFrame();
        if (coreNeedsShutdown) {
            coreNeedsShutdown=false;
            try { core.Dispose(); }
            catch (const std::exception& error) { std::cerr << "Core shutdown also failed: " << error.what() << '\n'; }
            catch (const std::exception* error) { std::cerr << "Core shutdown also failed: " << (error ? error->what() : "null generated exception") << '\n'; delete error; }
            catch (...) { std::cerr << "Core shutdown also failed with an unknown exception\n"; }
        }
        throw;
    }
}
}


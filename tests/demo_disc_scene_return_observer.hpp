#pragma once
#include "apple/core_player_observer.hpp"
#include "apple/sdl_input_backend.hpp"
#include "apple/sdl_replay_gamepad.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "Component.hpp"
#include "MenuComponent.hpp"
#include "MenuItemComponent.hpp"
#include <iostream>
#include <stdexcept>

namespace helengine::apple::tests {
/// Verifies authored scene-to-menu transitions through the real SDL input capture and generated gameplay.
class DemoDiscSceneReturnObserver final : public CorePlayerObserver {
    /// Selects the physical input path explicitly requested by the test executable.
    bool Gamepad;
    /// Owns the explicit SDL virtual controller while Core captures its real state.
    std::unique_ptr<SdlReplayGamepad> VirtualGamepad;
    /// Records the real frame at which the initialized authored menu first appeared.
    int32_t ReturnedAtFrame=-1;
    /// Counts native input snapshots through the completed transition.
    uint64_t Captures=0;
    /// Counts frames rendered before the return press, excluding the menu afterward.
    uint32_t SceneDraws=0;
public:
    /// Chooses keyboard Escape or the controller East edge without changing any scene state.
    explicit DemoDiscSceneReturnObserver(bool gamepad) : Gamepad(gamepad) { }
    /// Emits only native input events; generated authored components own the actual scene transition.
    void BeforeUpdate(::Core&,SdlInputBackend& input,uint32_t frame) override {
        input.set_ReceiveInputInBackground(true);
        if (Gamepad && frame==0) { VirtualGamepad=std::make_unique<SdlReplayGamepad>(); input.AttachReplayGamepad(VirtualGamepad->GetId()); }
        if (frame!=10 && frame!=11) return;
        SDL_Event event{};
        if (Gamepad) {
            event.type=frame==10 ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            event.gbutton.button=SDL_GAMEPAD_BUTTON_EAST; VirtualGamepad->Apply(event);
        } else {
            event.type=frame==10 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.scancode=SDL_SCANCODE_ESCAPE; input.HandleEvent(event);
        }
    }
    /// Observes the loaded authored menu rather than inferring a transition from changing framebuffer pixels.
    void AfterUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) override {
        Captures=input.GetCapturedFrameCount();
        auto* entities=core.get_ObjectManager()->get_Entities();
        for (int32_t index=0;index<entities->get_Count();++index) {
            auto* components=(*entities)[index]->get_Components();
            for (int32_t component=0;component<components->get_Count();++component) {
                auto* menu=dynamic_cast<::MenuComponent*>((*components)[component]);
                if (!menu || !menu->get_IsInitialized()) continue;
                if (frame<10) throw std::runtime_error("Return validation unexpectedly started in the menu rather than its requested rendering scene");
                if (ReturnedAtFrame<0) ReturnedAtFrame=static_cast<int32_t>(frame);
            }
        }
    }
    /// Requires actual scene draws before pressing Return while leaving all renderer work unchanged.
    void AfterDraw(::Core& core,MetalRenderer&,std::span<const uint8_t>,uint32_t,uint32_t,uint32_t frame) override {
        if (frame<10 && core.get_RenderManager3D()->get_LastDrawCallCount()>0) ++SceneDraws;
    }
    /// Requires the complete authored menu after the input-driven transition and normal input capture throughout.
    void ValidateComplete(::Core& core) override {
        uint32_t items=0;
        auto* entities=core.get_ObjectManager()->get_Entities();
        for (int32_t index=0;index<entities->get_Count();++index) {
            auto* components=(*entities)[index]->get_Components();
            for (int32_t component=0;component<components->get_Count();++component) if (dynamic_cast<::MenuItemComponent*>((*components)[component])) ++items;
        }
        std::cout << "return_device=" << (Gamepad ? "gamepad" : "keyboard") << " returned_frame=" << ReturnedAtFrame << " returned_menu_items=" << items << " scene_draw_frames=" << SceneDraws << " return_input_captures=" << Captures << '\n';
        if (ReturnedAtFrame<10 || items!=44 || SceneDraws!=10 || Captures<120) throw std::runtime_error("Authored scene did not return to the full menu through the selected real native input path");
    }
};
}

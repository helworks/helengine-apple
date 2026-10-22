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
#include <unordered_set>

namespace helengine::apple::tests {
/// Validates DemoDisc's own 44 authored menu items and navigation through the generic player's real SDL/Core input path.
class DemoDiscMenuObserver final : public CorePlayerObserver {
    /// Retains the initial authored selection for a down/up round trip.
    std::string InitialSelection;
    /// Records a genuine gameplay selection change, independent of animated background pixels.
    bool MovedDown = false;
    /// Records a genuine return to the original selection through a second key event.
    bool MovedUp = false;
    /// Retains the observed native input capture count at completion.
    uint64_t Captures = 0;
    /// Owns only the explicitly attached SDL virtual device; normal capture still reads its real SDL state.
    std::unique_ptr<SdlReplayGamepad> VirtualGamepad;
    /// Stores the starting panel before the virtual controller performs Accept/Back navigation.
    std::string InitialPanel;
    /// Stores the first scene item so a real D-pad edge can be checked semantically.
    std::string FirstSceneItem;
    /// Records actual Accept, D-pad and Return transitions through generated gameplay.
    bool GamepadAccepted=false,GamepadMoved=false,GamepadReturned=false;
    /// Finds exactly one baked project menu without changing its hierarchy or state.
    static ::MenuComponent* FindMenu(::Core& core) {
        ::MenuComponent* result=nullptr;
        auto* entities=core.get_ObjectManager()->get_Entities();
        for (int32_t index=0;index<entities->get_Count();++index) {
            auto* components=(*entities)[index]->get_Components();
            for (int32_t component=0;component<components->get_Count();++component) {
                auto* menu=dynamic_cast<::MenuComponent*>((*components)[component]);
                if (menu) { if (result) throw std::runtime_error("DemoDisc validation found multiple menu roots"); result=menu; }
            }
        }
        if (!result || !result->get_IsInitialized()) throw std::runtime_error("DemoDisc baked menu is not initialized");
        return result;
    }
public:
    /// Replays physical key events explicitly, never reading desktop input or calling menu actions directly.
    void BeforeUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) override {
        input.set_ReceiveInputInBackground(true);
        if (frame==0) { VirtualGamepad=std::make_unique<SdlReplayGamepad>(); input.AttachReplayGamepad(VirtualGamepad->GetId()); }
        if (frame==20) InitialSelection=FindMenu(core)->get_SelectedItemId().raw();
        if (frame==20 || frame==21 || frame==40 || frame==41) {
            SDL_Event event {};
            event.type=frame==20 || frame==40 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.scancode=frame<40 ? SDL_SCANCODE_DOWN : SDL_SCANCODE_UP;
            input.HandleEvent(event);
        }
        if (frame==60) InitialPanel=FindMenu(core)->get_ActivePanelId().raw();
        if (frame==80) FirstSceneItem=FindMenu(core)->get_SelectedItemId().raw();
        if (frame==60 || frame==61 || frame==80 || frame==81 || frame==100 || frame==101) {
            SDL_Event event{};
            event.type=frame==60 || frame==80 || frame==100 ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            event.gbutton.button=frame<80 ? SDL_GAMEPAD_BUTTON_SOUTH : (frame<100 ? SDL_GAMEPAD_BUTTON_DPAD_DOWN : SDL_GAMEPAD_BUTTON_EAST);
            VirtualGamepad->Apply(event);
        }
    }
    /// Observes the state produced by actual generated MenuComponent.Update after native CaptureFrame.
    void AfterUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) override {
        Captures=input.GetCapturedFrameCount();
        if (frame==20) { MovedDown=FindMenu(core)->get_SelectedItemId().raw()!=InitialSelection; if (!MovedDown) throw std::runtime_error("SDL Down replay did not change the authored menu selection"); }
        if (frame==40) { MovedUp=FindMenu(core)->get_SelectedItemId().raw()==InitialSelection; if (!MovedUp) throw std::runtime_error("SDL Up replay did not restore the authored menu selection"); }
        if (frame==60) { GamepadAccepted=FindMenu(core)->get_ActivePanelId().raw()!=InitialPanel; if (!GamepadAccepted) throw std::runtime_error("Actual SDL virtual South press did not enter the authored scene panel"); }
        if (frame==80) { GamepadMoved=FindMenu(core)->get_SelectedItemId().raw()!=FirstSceneItem; if (!GamepadMoved) throw std::runtime_error("Actual SDL virtual D-pad edge did not change the scene selection"); }
        if (frame==100) { GamepadReturned=FindMenu(core)->get_ActivePanelId().raw()==InitialPanel; if (!GamepadReturned) throw std::runtime_error("Actual SDL virtual East press did not navigate back to the initial panel"); }
    }
    /// Requires every baked item, stable nonempty identities, 300 real captures and both navigation transitions.
    void ValidateComplete(::Core& core) override {
        uint32_t items=0;
        std::unordered_set<std::string> identities;
        auto* entities=core.get_ObjectManager()->get_Entities();
        for (int32_t index=0;index<entities->get_Count();++index) {
            auto* components=(*entities)[index]->get_Components();
            for (int32_t component=0;component<components->get_Count();++component) {
                auto* item=dynamic_cast<::MenuItemComponent*>((*components)[component]);
                if (!item) continue;
                ++items;
                const auto panel=item->get_PanelId().raw(),id=item->get_ItemId().raw();
                if (panel.empty() || id.empty() || !identities.insert(panel+"/"+id).second) throw std::runtime_error("DemoDisc item identity is empty or duplicated");
            }
        }
        std::cout << "menu_items=" << items << " unique_item_ids=" << identities.size() << " input_captures=" << Captures << " down=" << MovedDown << " up=" << MovedUp << " gamepad_accept=" << GamepadAccepted << " gamepad_dpad=" << GamepadMoved << " gamepad_back=" << GamepadReturned << '\n';
        if (items!=44 || Captures<300 || !MovedDown || !MovedUp || !GamepadAccepted || !GamepadMoved || !GamepadReturned) throw std::runtime_error("DemoDisc full menu/input validation failed");
    }
};
}

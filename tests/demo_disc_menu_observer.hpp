#pragma once
#include "apple/core_player_observer.hpp"
#include "apple/sdl_input_backend.hpp"
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
        if (frame==20) InitialSelection=FindMenu(core)->get_SelectedItemId().raw();
        if (frame==20 || frame==21 || frame==40 || frame==41) {
            SDL_Event event {};
            event.type=frame==20 || frame==40 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.scancode=frame<40 ? SDL_SCANCODE_DOWN : SDL_SCANCODE_UP;
            input.HandleEvent(event);
        }
    }
    /// Observes the state produced by actual generated MenuComponent.Update after native CaptureFrame.
    void AfterUpdate(::Core& core,SdlInputBackend& input,uint32_t frame) override {
        Captures=input.GetCapturedFrameCount();
        if (frame==20) { MovedDown=FindMenu(core)->get_SelectedItemId().raw()!=InitialSelection; if (!MovedDown) throw std::runtime_error("SDL Down replay did not change the authored menu selection"); }
        if (frame==40) { MovedUp=FindMenu(core)->get_SelectedItemId().raw()==InitialSelection; if (!MovedUp) throw std::runtime_error("SDL Up replay did not restore the authored menu selection"); }
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
        std::cout << "menu_items=" << items << " unique_item_ids=" << identities.size() << " input_captures=" << Captures << " down=" << MovedDown << " up=" << MovedUp << '\n';
        if (items!=44 || Captures<300 || !MovedDown || !MovedUp) throw std::runtime_error("DemoDisc full menu/input validation failed");
    }
};
}

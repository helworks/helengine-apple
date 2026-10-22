#include "ios/ios_input_backend.hpp"
#include "apple/frame_lifecycle.hpp"
#include "InputPointerButton.hpp"
#include "InputGamepadButton.hpp"
#include <iostream>
#include <stdexcept>

/// Fails with a contextual message instead of depending on release-disabled assertions.
static void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
/// Produces actual SDL touch events in normalized coordinates, including full-width native IDs.
static SDL_Event Touch(SDL_EventType type,SDL_TouchID device,SDL_FingerID finger,float x,float y) {
    SDL_Event event{}; event.type=type; event.tfinger.touchID=device; event.tfinger.fingerID=finger; event.tfinger.x=x; event.tfinger.y=y; event.tfinger.pressure=.75f; return event;
}
/// Exercises the portable iOS contract against real generated input types without opening a window.
int main() {
    try {
        if (!SDL_Init(SDL_INIT_EVENTS|SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError());
        {
            helengine::apple::IosInputBackend input;
            input.SetDrawableSize(200,100);
            input.HandleEvent(Touch(SDL_EVENT_FINGER_DOWN,1,1,.5f,.25f));
            input.HandleEvent(Touch(SDL_EVENT_FINGER_MOTION,1,1,.6f,.5f));
            input.HandleEvent(Touch(SDL_EVENT_FINGER_UP,1,1,.6f,.5f));
            auto tap=input.CaptureFrame();
            Require(tap.TouchCount==3,"A complete tap between captures must retain Begin/Move/End.");
            Require((*tap.Touches)[0].X==100 && (*tap.Touches)[0].Y==25,"Touch must use physical drawable pixels.");
            Require((*tap.Touches)[1].DeltaX==20 && (*tap.Touches)[1].DeltaY==25,"Touch motion must retain pixel deltas.");
            Require((*tap.Touches)[2].Phase==InputTouchPhase::Ended,"Release must retain the portable Ended phase.");
            input.HandleEvent(Touch(SDL_EVENT_FINGER_DOWN,1,1,.1f,.1f));
            input.HandleEvent(Touch(SDL_EVENT_FINGER_DOWN,1,0x100000001ULL,.2f,.2f));
            auto current=input.CaptureFrame();
            Require(current.TouchCount==2 && (*current.Touches)[0].Id!=(*current.Touches)[1].Id,"64-bit native fingers must not alias after narrowing.");
            Require(tap.TouchCount==3 && (*tap.Touches)[0].X==100,"Previous touch snapshot must remain immutable.");
            input.Suspend(); input.Resume();
            auto canceled=input.CaptureFrame();
            Require(canceled.TouchCount==2 && (*canceled.Touches)[0].Phase==InputTouchPhase::Canceled,"Background transition must cancel every active contact.");
            Require(!canceled.Pointer.IsButtonDown(InputPointerButton::Primary),"Suspension cannot leave primary pointer pressed.");
            input.SetDrawableSize(0,0); input.HandleEvent(Touch(SDL_EVENT_FINGER_DOWN,1,2,.5f,.5f));
            Require(input.CaptureFrame().TouchCount==0,"Zero drawable extent must not invent touch coordinates.");
            input.SetDrawableSize(200,100); input.HandleEvent(Touch(SDL_EVENT_FINGER_DOWN,1,2,1,1));
            auto edge=input.CaptureFrame(); Require((*edge.Touches)[0].X==199 && (*edge.Touches)[0].Y==99,"Normalized edges must remain inside physical bounds.");
            auto stationary=input.CaptureFrame(); Require(stationary.TouchCount==1 && (*stationary.Touches)[0].Phase==InputTouchPhase::Stationary && (*stationary.Touches)[0].DeltaX==0,"Held contacts must have a stationary zero-delta sample.");
            input.SetDrawableSize(100,200); auto resized=input.CaptureFrame(); Require(resized.TouchCount==1 && (*resized.Touches)[0].Phase==InputTouchPhase::Canceled,"Orientation changes must cancel the old physical coordinate space.");
            SDL_VirtualJoystickDesc descriptor{}; SDL_INIT_INTERFACE(&descriptor); descriptor.type=SDL_JOYSTICK_TYPE_GAMEPAD; descriptor.naxes=SDL_GAMEPAD_AXIS_COUNT; descriptor.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
            auto id=SDL_AttachVirtualJoystick(&descriptor); Require(id!=0,"Virtual gamepad creation failed.");
            auto* joystick=SDL_OpenJoystick(id); Require(joystick!=nullptr,"Virtual gamepad joystick open failed."); input.AttachReplayGamepad(id);
            SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true); SDL_UpdateJoysticks();
            auto pressed=input.CaptureFrame(); Require((*pressed.Gamepads)[0].IsButtonDown(InputGamepadButton::South),"Virtual controller press must reach portable state.");
            SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,false); SDL_UpdateJoysticks();
            auto released=input.CaptureFrame(); Require(!(*released.Gamepads)[0].IsButtonDown(InputGamepadButton::South) && (*pressed.Gamepads)[0].IsButtonDown(InputGamepadButton::South),"Current controller capture must not overwrite previous state.");
            SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTY,-32768); SDL_UpdateJoysticks(); auto axis=input.CaptureFrame(); Require((*axis.Gamepads)[0].LeftStickY==32767,"Portable Y-up stick conversion must saturate the signed minimum.");
            input.Suspend(); Require(!(*input.CaptureFrame().Gamepads)[0].Connected,"Background snapshots must suppress controller input."); input.Resume();
            SDL_CloseJoystick(joystick); SDL_DetachVirtualJoystick(id);
        }
        helengine::apple::FrameLifecycle lifecycle; lifecycle.Resize(200,100); Require(lifecycle.CanRender(),"Foreground drawable must render."); lifecycle.Suspend(); Require(!lifecycle.CanRender(),"Background must stop GPU frames."); lifecycle.Resume(); lifecycle.Resize(0,100); Require(lifecycle.ShouldWait(),"Zero drawable must pace retries."); lifecycle.Resize(200,100); lifecycle.DrawableUnavailable(); Require(lifecycle.ShouldWait(),"Missing CAMetalDrawable must pace retries."); lifecycle.DrawableAvailable(); Require(!lifecycle.ShouldWait(),"Successful drawable must resume frames.");
        SDL_Quit(); std::cout<<"iOS portable input/lifecycle: passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; SDL_Quit(); return 1; }
}

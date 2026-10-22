#include "ios_input_backend.hpp"
#include "InputPointerButton.hpp"
#include "InputGamepadButton.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace helengine::apple {
IosInputBackend::IosInputBackend(SDL_Window* window):Window(window) {
    if (!Window) return;
    int count=0; auto* ids=SDL_GetGamepads(&count);
    if (!ids) throw std::runtime_error(std::string("iOS SDL_GetGamepads: ")+SDL_GetError());
    try { for (int i=0;i<count;++i) OpenGamepad(ids[i]); }
    catch (...) { SDL_free(ids); for (auto* pad:Gamepads) if(pad) SDL_CloseGamepad(pad); throw; }
    SDL_free(ids);
}
IosInputBackend::~IosInputBackend() { for(auto* pad:Gamepads) if(pad) SDL_CloseGamepad(pad); }
void IosInputBackend::OpenGamepad(SDL_JoystickID id) {
    for(auto* pad:Gamepads) if(pad && SDL_GetGamepadID(pad)==id) return;
    for(auto& pad:Gamepads) if(!pad) { pad=SDL_OpenGamepad(id); if(!pad) throw std::runtime_error(std::string("iOS SDL_OpenGamepad: ")+SDL_GetError()); return; }
}
void IosInputBackend::AttachReplayGamepad(SDL_JoystickID id) {
    if(!SDL_IsJoystickVirtual(id)||!SDL_IsGamepad(id)) throw std::invalid_argument("iOS replay requires an explicit virtual gamepad.");
    OpenGamepad(id);
}
void IosInputBackend::Append(const Contact& c,::InputTouchPhase phase,int dx,int dy) {
    ::InputTouchState sample; sample.Id=c.Id; sample.Phase=phase; sample.X=c.X; sample.Y=c.Y; sample.DeltaX=dx; sample.DeltaY=dy; sample.Pressure=c.Pressure; Pending.push_back(sample);
}
void IosInputBackend::CancelContacts() { for(const auto& c:Contacts) Append(c,::InputTouchPhase::Canceled); Contacts.clear(); PreviousPrimary=0; }
void IosInputBackend::SetDrawableSize(int width,int height) {
    if(width<0||height<0) throw std::invalid_argument("iOS drawable dimensions cannot be negative.");
    if(width!=Width||height!=Height) CancelContacts();
    Width=width; Height=height;
}
void IosInputBackend::Suspend() { Suspended=true; CancelContacts(); }
void IosInputBackend::Resume() { Suspended=false; }
void IosInputBackend::HandleEvent(const SDL_Event& event) {
    if(event.type==SDL_EVENT_GAMEPAD_ADDED) { if(Window) OpenGamepad(event.gdevice.which); return; }
    if(event.type==SDL_EVENT_GAMEPAD_REMOVED) { for(auto& pad:Gamepads) if(pad&&SDL_GetGamepadID(pad)==event.gdevice.which) { SDL_CloseGamepad(pad); pad=nullptr; } return; }
    if(event.type!=SDL_EVENT_FINGER_DOWN&&event.type!=SDL_EVENT_FINGER_UP&&event.type!=SDL_EVENT_FINGER_MOTION&&event.type!=SDL_EVENT_FINGER_CANCELED) return;
    const auto& touch=event.tfinger;
    if(Window&&touch.windowID!=SDL_GetWindowID(Window)) return;
    if(Suspended||Width==0||Height==0) return;
    if(!std::isfinite(touch.x)||!std::isfinite(touch.y)||!std::isfinite(touch.pressure)||touch.pressure<0) throw std::invalid_argument("iOS touch contains invalid coordinates or pressure.");
    const int x=static_cast<int>(std::clamp(std::floor(static_cast<double>(touch.x)*Width),0.0,static_cast<double>(Width-1)));
    const int y=static_cast<int>(std::clamp(std::floor(static_cast<double>(touch.y)*Height),0.0,static_cast<double>(Height-1)));
    auto found=std::find_if(Contacts.begin(),Contacts.end(),[&](const Contact& c){return c.Device==touch.touchID&&c.Finger==touch.fingerID;});
    if(event.type==SDL_EVENT_FINGER_DOWN) {
        if(found!=Contacts.end()) { Append(*found,::InputTouchPhase::Canceled); Contacts.erase(found); }
        if(NextContactId>INT32_MAX) throw std::overflow_error("iOS portable touch identity space exhausted.");
        Contacts.push_back({touch.touchID,touch.fingerID,static_cast<int32_t>(NextContactId++),x,y,touch.pressure}); Append(Contacts.back(),::InputTouchPhase::Began); return;
    }
    if(found==Contacts.end()) return; // SDL can deliver stale terminal events after background cancellation.
    const int dx=x-found->X,dy=y-found->Y; found->X=x; found->Y=y; found->Pressure=touch.pressure;
    Append(*found,event.type==SDL_EVENT_FINGER_MOTION ? ::InputTouchPhase::Moved : event.type==SDL_EVENT_FINGER_UP ? ::InputTouchPhase::Ended : ::InputTouchPhase::Canceled,dx,dy);
    if(event.type!=SDL_EVENT_FINGER_MOTION) Contacts.erase(found);
}
::InputFrameState IosInputBackend::CaptureFrame() {
    ::InputFrameState frame;
    for(const auto& c:Contacts) if(std::none_of(Pending.begin(),Pending.end(),[&](const auto& sample){return sample.Id==c.Id;})) Append(c,::InputTouchPhase::Stationary);
    if(Pending.size()>INT32_MAX) throw std::overflow_error("iOS touch event batch exceeds the generated array contract.");
    auto& touches=TouchFrames[NextFrame]; touches=std::make_unique<Array<::InputTouchState>>(static_cast<int32_t>(Pending.size()));
    for(size_t i=0;i<Pending.size();++i) (*touches)[static_cast<int32_t>(i)]=Pending[i];
    frame.Touches=touches.get(); frame.TouchCount=static_cast<int32_t>(Pending.size()); Pending.clear();
    frame.Pointer.IsTouch=true; frame.Pointer.Connected=!Suspended&&Width>0&&Height>0;
    if(!Contacts.empty()) { const auto& c=Contacts.front(); if(c.Id==PreviousPrimary) { frame.Pointer.DeltaX=c.X-PointerX; frame.Pointer.DeltaY=c.Y-PointerY; } PointerX=c.X; PointerY=c.Y; PreviousPrimary=c.Id; frame.Pointer.SetButtonDown(::InputPointerButton::Primary,true); }
    else PreviousPrimary=0;
    frame.Pointer.X=PointerX; frame.Pointer.Y=PointerY;
    auto& pads=GamepadFrames[NextFrame];
    for(size_t i=0;i<Gamepads.size();++i) {
        auto& p=pads[static_cast<int32_t>(i)]; p=::InputGamepadState(); auto* pad=Gamepads[i]; if(!pad||Suspended||!SDL_GamepadConnected(pad)) continue;
        p.Connected=true; p.LeftStickX=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX); p.LeftStickY=static_cast<int16_t>(std::clamp(-static_cast<int>(SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTY)),-32768,32767));
        p.RightStickX=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHTX); p.RightStickY=static_cast<int16_t>(std::clamp(-static_cast<int>(SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHTY)),-32768,32767));
        p.LeftTrigger=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER); p.RightTrigger=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        const std::pair<::InputGamepadButton,SDL_GamepadButton> buttons[]={{::InputGamepadButton::South,SDL_GAMEPAD_BUTTON_SOUTH},{::InputGamepadButton::East,SDL_GAMEPAD_BUTTON_EAST},{::InputGamepadButton::West,SDL_GAMEPAD_BUTTON_WEST},{::InputGamepadButton::North,SDL_GAMEPAD_BUTTON_NORTH},{::InputGamepadButton::DPadUp,SDL_GAMEPAD_BUTTON_DPAD_UP},{::InputGamepadButton::DPadDown,SDL_GAMEPAD_BUTTON_DPAD_DOWN},{::InputGamepadButton::DPadLeft,SDL_GAMEPAD_BUTTON_DPAD_LEFT},{::InputGamepadButton::DPadRight,SDL_GAMEPAD_BUTTON_DPAD_RIGHT},{::InputGamepadButton::Start,SDL_GAMEPAD_BUTTON_START},{::InputGamepadButton::Select,SDL_GAMEPAD_BUTTON_BACK},{::InputGamepadButton::LeftShoulder,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},{::InputGamepadButton::RightShoulder,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},{::InputGamepadButton::LeftStick,SDL_GAMEPAD_BUTTON_LEFT_STICK},{::InputGamepadButton::RightStick,SDL_GAMEPAD_BUTTON_RIGHT_STICK},{::InputGamepadButton::Home,SDL_GAMEPAD_BUTTON_GUIDE}};
        for(const auto& button:buttons) p.SetButtonDown(button.first,SDL_GetGamepadButton(pad,button.second));
        p.SetButtonDown(::InputGamepadButton::LeftTrigger,p.LeftTrigger>16383); p.SetButtonDown(::InputGamepadButton::RightTrigger,p.RightTrigger>16383);
    }
    frame.Gamepads=&pads; frame.GamepadCount=4; NextFrame=1-NextFrame; return frame;
}
}

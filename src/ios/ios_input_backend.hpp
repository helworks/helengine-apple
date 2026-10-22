#pragma once
#include "IInputBackend.hpp"
#include "InputTouchState.hpp"
#include "InputGamepadState.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <vector>

namespace helengine::apple {
/// Converts UIKit-backed SDL touch events and gamepads into the generated portable input contract.
class IosInputBackend final : public ::IInputBackend {
    /// Retains complete native contact identity until its terminal event.
    struct Contact { SDL_TouchID Device; SDL_FingerID Finger; int32_t Id; int32_t X,Y; float Pressure; };
    /// Borrows the application window; null permits only explicit isolated event injection.
    SDL_Window* Window;
    /// Stores active contacts in primary-pointer order.
    std::vector<Contact> Contacts;
    /// Retains every ordered transition, including taps completed between captures.
    std::vector<::InputTouchState> Pending;
    /// Owns alternating touch arrays so previous capture references remain immutable.
    std::array<std::unique_ptr<Array<::InputTouchState>>,2> TouchFrames;
    /// Owns alternating stable gamepad snapshots.
    std::array<Array<::InputGamepadState>,2> GamepadFrames {Array<::InputGamepadState>(4),Array<::InputGamepadState>(4)};
    /// Owns native controller handles in stable slots.
    std::array<SDL_Gamepad*,4> Gamepads {};
    /// Selects the reusable older-than-previous snapshot.
    size_t NextFrame=0;
    /// Allocates collision-free portable identities without narrowing SDL's 64-bit IDs.
    int64_t NextContactId=1;
    /// Stores physical drawable dimensions; zero suspends coordinate materialization.
    int Width=0,Height=0;
    /// Suppresses new input while the application is suspended.
    bool Suspended=false;
    /// Retains the last primary coordinate when the last touch ends.
    int32_t PointerX=0,PointerY=0,PreviousPrimary=0;
    /// Opens an attached gamepad once into its first free stable slot.
    void OpenGamepad(SDL_JoystickID id);
    /// Cancels all active touch identities without converting cancellation into activation.
    void CancelContacts();
    /// Appends a physical portable touch transition.
    void Append(const Contact& contact,::InputTouchPhase phase,int dx=0,int dy=0);
public:
    /// Enumerates production controllers only when a real application window is supplied.
    explicit IosInputBackend(SDL_Window* window=nullptr);
    /// Closes controller handles after the generated input system has stopped borrowing snapshots.
    ~IosInputBackend() override;
    /// Prevents duplicate ownership of SDL controller handles and generated snapshot buffers.
    IosInputBackend(const IosInputBackend&)=delete;
    /// Prevents assignment from duplicating SDL controller ownership.
    IosInputBackend& operator=(const IosInputBackend&)=delete;
    /// Updates physical extent and cancels contacts when a drawable becomes unavailable.
    void SetDrawableSize(int width,int height);
    /// Materializes ordered touch and gamepad lifecycle events for this window.
    void HandleEvent(const SDL_Event& event);
    /// Cancels current contacts and suppresses controller polling in background.
    void Suspend();
    /// Permits new foreground contacts without reviving canceled identities.
    void Resume();
    /// Opens only explicitly supplied virtual gamepads for deterministic noninteractive tests.
    void AttachReplayGamepad(SDL_JoystickID id);
    /// Returns real portable state while preserving the immediately previous snapshot.
    ::InputFrameState CaptureFrame() override;
};
}

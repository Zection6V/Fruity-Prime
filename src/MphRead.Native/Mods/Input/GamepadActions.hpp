#pragma once

#include "GamepadState.hpp"
#include "PadAction.hpp"

#include <cstdint>

namespace MphRead
{
    enum class BeamType : std::int8_t;
}

namespace MphRead::Entities
{
    class Keybind;
    class PlayerControls;
}

namespace MphRead::Mods::Input
{
    class GamepadActions final
    {
    public:
        [[nodiscard]] std::uint64_t Pressed() const noexcept { return _pressed; }
        [[nodiscard]] bool WheelOpen() const noexcept { return _wheelOpen; }
        [[nodiscard]] bool Down(PadAction action) const noexcept;
        [[nodiscard]] bool WasPressed(PadAction action) const noexcept;
        [[nodiscard]] bool Take(PadAction action) noexcept;
        void CloseWheel() noexcept { _wheelOpen = false; }
        void Reset() noexcept;
        void Update(GamepadButtons buttons);

        // null for a weapon with no bind of its own.
        [[nodiscard]] static Entities::Keybind* WeaponBind(Entities::PlayerControls& controls, BeamType weapon);

    private:
        std::uint64_t _down = 0;
        GamepadButtons _suppressed = GamepadButtons::None;
        std::uint64_t _pressed = 0;
        bool _wheelOpen = false;
    };
}

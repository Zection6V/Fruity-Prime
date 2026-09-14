#pragma once

#include "GamepadState.hpp"

#include <utility>

namespace MphRead::Entities
{
    class Keybind;
    class PlayerEntity;
}

namespace MphRead::Mods::Input
{
    class GamepadInput final
    {
    public:
        GamepadInput() = delete;
        GamepadInput(const GamepadInput&) = delete;
        GamepadInput& operator=(const GamepadInput&) = delete;

        static GamepadState State;

        [[nodiscard]] static bool Active() noexcept;
        [[nodiscard]] static bool InUse();

        [[nodiscard]] static float AimDeltaX() noexcept;
        [[nodiscard]] static float AimDeltaY() noexcept;

        static void BeginFrame();
        [[nodiscard]] static bool TakeMenuPress();
        [[nodiscard]] static bool TakeChatPress();
        [[nodiscard]] static bool TakePress(GamepadButtons buttons);
        static void Apply(Entities::PlayerEntity* player);

    private:
        static GamepadButtons _previous;
        static GamepadButtons _pressed;
        static float _aimDeltaX;
        static float _aimDeltaY;

        static constexpr float TurnRate = 3.5F;
        static constexpr float TriggerThreshold = 0.65F;
        static constexpr float WalkThreshold = 0.5F;

        [[nodiscard]] static std::pair<float, float> ApplyDeadZone(float x, float y);
        static void Hold(Entities::Keybind& bind, GamepadButtons buttons);
        static void Hold(Entities::Keybind& bind, bool down);
        static void Hold(Entities::Keybind& bind, bool down, bool pressed);
    };
}

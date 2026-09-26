#pragma once

#include "GamepadManager.hpp"
#include "GamepadState.hpp"

#include <cstdint>
#include <utility>

namespace MphRead::Entities
{
    class Keybind;
    class PlayerControls;
    class PlayerEntity;
}

namespace MphRead::Mods::Input
{
    enum class GamepadContext : std::int32_t;
    class GamepadActions;
    class GamepadEdges;

    class GamepadInput final
    {
    public:
        GamepadInput() = delete;

        [[nodiscard]] static GamepadState State();
        [[nodiscard]] static const GamepadSnapshot& FrameSnapshot() noexcept { return _frameSnapshot; }
        [[nodiscard]] static bool WheelHeld();
        [[nodiscard]] static std::pair<float, float> AimStick();
        [[nodiscard]] static bool Active();
        [[nodiscard]] static bool InUse();
        [[nodiscard]] static float AimDeltaX() noexcept { return _aimDeltaX; }
        [[nodiscard]] static float AimDeltaY() noexcept { return _aimDeltaY; }

        static void BeginFrame();
        [[nodiscard]] static bool TakeMenuPress();
        [[nodiscard]] static bool TakeChatPress();
        [[nodiscard]] static bool TakePress(GamepadButtons buttons);
        static void Apply(Entities::PlayerEntity* player);
        static void ApplyBindings(Entities::PlayerControls& controls);
        static void Hold(Entities::Keybind& bind, bool down, bool pressed);

    private:
        static void Hold(Entities::Keybind& bind, bool down);
        [[nodiscard]] static GamepadEdges& Edges();
        [[nodiscard]] static GamepadActions& Actions();

        static constexpr float TurnRate = 3.5F;
        static constexpr float WalkThreshold = 0.5F;

        inline static GamepadState _frame{};
        inline static GamepadSnapshot _frameSnapshot{};
        inline static GamepadContext _context{};
        inline static GamepadButtons _blocked = GamepadButtons::None;
        inline static std::int64_t _revision = -1;
        inline static std::int64_t _contextRevision = -1;
        inline static std::int64_t _bindingsRevision = -1;
        inline static GamepadButtons _pressed = GamepadButtons::None;
        inline static float _aimDeltaX = 0;
        inline static float _aimDeltaY = 0;
    };
}

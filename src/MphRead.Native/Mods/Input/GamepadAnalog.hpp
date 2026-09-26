#pragma once

#include "GamepadState.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Input
{
    enum class GamepadCurve : std::int32_t { Linear, Classic, Precision, Dynamic };

    // GamepadCurve.ToString(), Enum.TryParse and Enum.IsDefined.
    [[nodiscard]] std::string ToString(GamepadCurve value);
    [[nodiscard]] bool TryParse(std::string_view text, GamepadCurve& value);
    [[nodiscard]] constexpr bool IsDefined(GamepadCurve value) noexcept
    {
        return static_cast<std::int32_t>(value) >= 0 && static_cast<std::int32_t>(value) <= 3;
    }

    class GamepadAnalog final
    {
    public:
        GamepadAnalog() = delete;

        [[nodiscard]] static float Finite(float value, float min = -1, float max = 1) noexcept;
        [[nodiscard]] static std::pair<float, float> ApplyRadialDeadZone(float x, float y,
            float inner, float outer = 0) noexcept;
        [[nodiscard]] static float ApplyResponseCurve(float value, GamepadCurve curve) noexcept;
        [[nodiscard]] static bool Trigger(float value, bool held, float press = 0.60F) noexcept;
        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> QuantizeMovement(float x, float y,
            float threshold = 0.5F) noexcept;
    };

    class GamepadEventState final
    {
    public:
        GamepadButtons KeyButtons = GamepadButtons::None;
        GamepadState Motion{};

        [[nodiscard]] GamepadState Snapshot() const;
        void Key(GamepadButtons button, bool down) noexcept;
        void Clear();
    };
}

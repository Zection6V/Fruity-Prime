#include "GamepadAnalog.hpp"

#include "../../NativeRuntime/System/Enum.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr ::MphRead::NativeRuntime::EnumNameEntry CurveNames[] = {
            {0, "Linear"}, {1, "Classic"}, {2, "Precision"}, {3, "Dynamic"}};
    }

    std::string ToString(GamepadCurve value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, CurveNames, std::size(CurveNames), false);
    }

    bool TryParse(std::string_view text, GamepadCurve& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(text, false, CurveNames, std::size(CurveNames), value);
    }

    float GamepadAnalog::Finite(float value, float min, float max) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, min, max) : 0;
    }

    std::pair<float, float> GamepadAnalog::ApplyRadialDeadZone(float x, float y, float inner, float outer) noexcept
    {
        x = Finite(x);
        y = Finite(y);
        inner = Finite(inner, 0, 0.9F);
        outer = Finite(outer, 0, std::min(0.5F, 0.99F - inner));
        const float length = std::sqrt(x * x + y * y);
        if (length <= inner || length == 0)
        {
            return {0.0F, 0.0F};
        }
        const float magnitude = std::clamp((length - inner) / (1 - inner - outer), 0.0F, 1.0F);
        return {x / length * magnitude, y / length * magnitude};
    }

    float GamepadAnalog::ApplyResponseCurve(float value, GamepadCurve curve) noexcept
    {
        float exponent = 2;
        switch (curve)
        {
        case GamepadCurve::Linear: exponent = 1; break;
        case GamepadCurve::Precision: exponent = 2.4F; break;
        case GamepadCurve::Dynamic: exponent = 1.5F; break;
        default: break;
        }
        return std::copysign(std::pow(std::abs(Finite(value)), exponent), value);
    }

    bool GamepadAnalog::Trigger(float value, bool held, float press) noexcept
    {
        press = Finite(press, 0.05F, 0.95F);
        const float release = std::max(0.01F, press - 0.15F);
        return Finite(value, 0, 1) >= (held ? release : press);
    }

    std::pair<std::int32_t, std::int32_t> GamepadAnalog::QuantizeMovement(float x, float y, float threshold) noexcept
    {
        if (x * x + y * y < threshold * threshold || (x == 0 && y == 0))
        {
            return {0, 0};
        }
        const std::int32_t sector = (static_cast<std::int32_t>(std::nearbyint(
            std::atan2(y, x) / (std::numbers::pi_v<float> / 4))) + 8) % 8;
        switch (sector)
        {
        case 0: return {1, 0};
        case 1: return {1, 1};
        case 2: return {0, 1};
        case 3: return {-1, 1};
        case 4: return {-1, 0};
        case 5: return {-1, -1};
        case 6: return {0, -1};
        default: return {1, -1};
        }
    }

    GamepadState GamepadEventState::Snapshot() const
    {
        GamepadState state = Motion;
        state.Buttons |= KeyButtons;
        return state;
    }

    void GamepadEventState::Key(GamepadButtons button, bool down) noexcept
    {
        if (down)
        {
            KeyButtons |= button;
        }
        else
        {
            KeyButtons &= ~button;
        }
    }

    void GamepadEventState::Clear()
    {
        KeyButtons = GamepadButtons::None;
        Motion = {};
    }
}

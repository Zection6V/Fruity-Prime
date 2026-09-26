#pragma once

#include "GamepadAnalog.hpp"
#include "GamepadState.hpp"
#include "StickCalibration.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::Mods::Input
{
    enum class GamepadFamily : std::int32_t;

    class GamepadOptionState final
    {
    public:
        GamepadOptionState();

        StickCalibration LeftCalibration = StickCalibration::Default();
        StickCalibration RightCalibration = StickCalibration::Default();
        float LeftInner = 0.2F;
        float RightInner = 0.2F;
        float LeftOuter = 0.0F;
        float RightOuter = 0.0F;
        float LookX = 1;
        float LookY = 1;
        float TriggerThreshold = 0.60F;
        float ActivityThreshold = 0.35F;
        bool InvertX = false;
        bool InvertY = false;
        bool Southpaw = false;
        bool Vibration = true;
        float VibrationStrength = 0.65F;
        GamepadCurve Curve = GamepadCurve::Classic;
        GamepadFamily GlyphStyle;
        float ScopedX = 1;
        float ScopedY = 1;
        float WheelThreshold = .45F;
        bool WheelToggle = false;
        GamepadButtons BindingModifier = GamepadButtons::None;
        float LeftTriggerMin = 0.0F;
        float RightTriggerMin = 0.0F;
        float LeftTriggerMax = 1;
        float RightTriggerMax = 1;

        [[nodiscard]] std::array<std::int32_t, 6>& WheelOrder() noexcept { return _wheelOrder; }
        void SetWheelSlot(std::int32_t position, std::int32_t slot);
        void Load(const std::vector<std::string>& lines);
        void Write(std::vector<std::string>& lines) const;
        [[nodiscard]] std::shared_ptr<GamepadOptionState> Clone() const;
        void Reset();

    private:
        std::array<std::int32_t, 6> _wheelOrder{0, 1, 2, 3, 4, 5};
    };
}

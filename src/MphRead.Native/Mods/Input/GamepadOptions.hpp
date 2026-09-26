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
    class GamepadOptionState;

    // The options of whichever controller is in force.
    class GamepadOptions final
    {
    public:
        GamepadOptions() = delete;

        [[nodiscard]] static float LeftInner();
        static void LeftInner(float value);
        [[nodiscard]] static float RightInner();
        static void RightInner(float value);
        [[nodiscard]] static float LeftOuter();
        static void LeftOuter(float value);
        [[nodiscard]] static float RightOuter();
        static void RightOuter(float value);
        [[nodiscard]] static float LookX();
        static void LookX(float value);
        [[nodiscard]] static float LookY();
        static void LookY(float value);
        [[nodiscard]] static float TriggerThreshold();
        static void TriggerThreshold(float value);
        [[nodiscard]] static float ActivityThreshold();
        static void ActivityThreshold(float value);
        [[nodiscard]] static bool InvertX();
        static void InvertX(bool value);
        [[nodiscard]] static bool InvertY();
        static void InvertY(bool value);
        [[nodiscard]] static bool Southpaw();
        static void Southpaw(bool value);
        [[nodiscard]] static bool Vibration();
        static void Vibration(bool value);
        [[nodiscard]] static float VibrationStrength();
        static void VibrationStrength(float value);
        [[nodiscard]] static GamepadCurve Curve();
        static void Curve(GamepadCurve value);
        [[nodiscard]] static GamepadFamily GlyphStyle();
        static void GlyphStyle(GamepadFamily value);
        [[nodiscard]] static float ScopedX();
        static void ScopedX(float value);
        [[nodiscard]] static float ScopedY();
        static void ScopedY(float value);
        [[nodiscard]] static float WheelThreshold();
        static void WheelThreshold(float value);
        [[nodiscard]] static bool WheelToggle();
        static void WheelToggle(bool value);
        [[nodiscard]] static GamepadButtons BindingModifier();
        static void BindingModifier(GamepadButtons value);
        [[nodiscard]] static float LeftTriggerMin();
        static void LeftTriggerMin(float value);
        [[nodiscard]] static float RightTriggerMin();
        static void RightTriggerMin(float value);
        [[nodiscard]] static float LeftTriggerMax();
        static void LeftTriggerMax(float value);
        [[nodiscard]] static float RightTriggerMax();
        static void RightTriggerMax(float value);
        [[nodiscard]] static StickCalibration LeftCalibration();
        static void LeftCalibration(StickCalibration value);
        [[nodiscard]] static StickCalibration RightCalibration();
        static void RightCalibration(StickCalibration value);
        [[nodiscard]] static std::array<std::int32_t, 6>& WheelOrder();
        static void SetWheelSlot(std::int32_t position, std::int32_t slot);
        static void Load(const std::vector<std::string>& lines);
        static void Write(std::vector<std::string>& lines);
        static void Reset();
    };
}

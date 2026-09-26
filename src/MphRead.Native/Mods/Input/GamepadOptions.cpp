#include "GamepadOptions.hpp"

#include "ControllerLayoutState.hpp"
#include "GamepadOptionState.hpp"
#include "GamepadRuntimeConfig.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        [[nodiscard]] std::shared_ptr<GamepadOptionState> State()
        {
            return GamepadRuntimeConfig::Current()->Options();
        }
    }

    float GamepadOptions::LeftInner() { return State()->LeftInner; }
    void GamepadOptions::LeftInner(float value) { State()->LeftInner = value; }
    float GamepadOptions::RightInner() { return State()->RightInner; }
    void GamepadOptions::RightInner(float value) { State()->RightInner = value; }
    float GamepadOptions::LeftOuter() { return State()->LeftOuter; }
    void GamepadOptions::LeftOuter(float value) { State()->LeftOuter = value; }
    float GamepadOptions::RightOuter() { return State()->RightOuter; }
    void GamepadOptions::RightOuter(float value) { State()->RightOuter = value; }
    float GamepadOptions::LookX() { return State()->LookX; }
    void GamepadOptions::LookX(float value) { State()->LookX = value; }
    float GamepadOptions::LookY() { return State()->LookY; }
    void GamepadOptions::LookY(float value) { State()->LookY = value; }
    float GamepadOptions::TriggerThreshold() { return State()->TriggerThreshold; }
    void GamepadOptions::TriggerThreshold(float value) { State()->TriggerThreshold = value; }
    float GamepadOptions::ActivityThreshold() { return State()->ActivityThreshold; }
    void GamepadOptions::ActivityThreshold(float value) { State()->ActivityThreshold = value; }
    bool GamepadOptions::InvertX() { return State()->InvertX; }
    void GamepadOptions::InvertX(bool value) { State()->InvertX = value; }
    bool GamepadOptions::InvertY() { return State()->InvertY; }
    void GamepadOptions::InvertY(bool value) { State()->InvertY = value; }
    bool GamepadOptions::Southpaw() { return State()->Southpaw; }
    void GamepadOptions::Southpaw(bool value) { GamepadRuntimeConfig::Current()->Layout()->Southpaw(value); }
    bool GamepadOptions::Vibration() { return State()->Vibration; }
    void GamepadOptions::Vibration(bool value) { State()->Vibration = value; }
    float GamepadOptions::VibrationStrength() { return State()->VibrationStrength; }
    void GamepadOptions::VibrationStrength(float value) { State()->VibrationStrength = value; }
    GamepadCurve GamepadOptions::Curve() { return State()->Curve; }
    void GamepadOptions::Curve(GamepadCurve value) { State()->Curve = value; }
    GamepadFamily GamepadOptions::GlyphStyle() { return State()->GlyphStyle; }
    void GamepadOptions::GlyphStyle(GamepadFamily value) { State()->GlyphStyle = value; }
    float GamepadOptions::ScopedX() { return State()->ScopedX; }
    void GamepadOptions::ScopedX(float value) { State()->ScopedX = value; }
    float GamepadOptions::ScopedY() { return State()->ScopedY; }
    void GamepadOptions::ScopedY(float value) { State()->ScopedY = value; }
    float GamepadOptions::WheelThreshold() { return State()->WheelThreshold; }
    void GamepadOptions::WheelThreshold(float value) { State()->WheelThreshold = value; }
    bool GamepadOptions::WheelToggle() { return State()->WheelToggle; }
    void GamepadOptions::WheelToggle(bool value) { State()->WheelToggle = value; }
    GamepadButtons GamepadOptions::BindingModifier() { return State()->BindingModifier; }
    void GamepadOptions::BindingModifier(GamepadButtons value) { State()->BindingModifier = value; }
    float GamepadOptions::LeftTriggerMin() { return State()->LeftTriggerMin; }
    void GamepadOptions::LeftTriggerMin(float value) { State()->LeftTriggerMin = value; }
    float GamepadOptions::RightTriggerMin() { return State()->RightTriggerMin; }
    void GamepadOptions::RightTriggerMin(float value) { State()->RightTriggerMin = value; }
    float GamepadOptions::LeftTriggerMax() { return State()->LeftTriggerMax; }
    void GamepadOptions::LeftTriggerMax(float value) { State()->LeftTriggerMax = value; }
    float GamepadOptions::RightTriggerMax() { return State()->RightTriggerMax; }
    void GamepadOptions::RightTriggerMax(float value) { State()->RightTriggerMax = value; }
    StickCalibration GamepadOptions::LeftCalibration() { return State()->LeftCalibration; }
    void GamepadOptions::LeftCalibration(StickCalibration value) { State()->LeftCalibration = value; }
    StickCalibration GamepadOptions::RightCalibration() { return State()->RightCalibration; }
    void GamepadOptions::RightCalibration(StickCalibration value) { State()->RightCalibration = value; }

    std::array<std::int32_t, 6>& GamepadOptions::WheelOrder()
    {
        // The array lives in the state object the config keeps alive.
        return State()->WheelOrder();
    }

    void GamepadOptions::SetWheelSlot(std::int32_t position, std::int32_t slot) { State()->SetWheelSlot(position, slot); }
    void GamepadOptions::Load(const std::vector<std::string>& lines) { State()->Load(lines); }
    void GamepadOptions::Write(std::vector<std::string>& lines) { State()->Write(lines); }
    void GamepadOptions::Reset() { State()->Reset(); }
}

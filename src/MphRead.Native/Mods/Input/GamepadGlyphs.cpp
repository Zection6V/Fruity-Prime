#include "GamepadGlyphs.hpp"

#include "GamepadManager.hpp"
#include "GamepadOptions.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"

namespace MphRead::Mods::Input
{
    GamepadFamily GamepadGlyphs::Detect(const std::string& name, const std::optional<std::string>& guid,
        std::int32_t vendorId)
    {
        const std::string vendor = guid.has_value() && guid->size() >= 12
            ? ::MphRead::NativeRuntime::ToLowerInvariant(guid->substr(8, 4)) : "";
        if (vendorId == 0x054c || vendor == "4c05")
        {
            return GamepadFamily::PlayStation;
        }
        if (vendorId == 0x045e || vendor == "5e04")
        {
            return GamepadFamily::Xbox;
        }
        if (vendorId == 0x057e || vendor == "7e05")
        {
            return GamepadFamily::Nintendo;
        }
        const std::string n = ::MphRead::NativeRuntime::ToLowerInvariant(name);
        const auto has = [&n](const char* text) { return n.find(text) != std::string::npos; };
        if (has("xbox") || has("x-box") || has("xinput"))
        {
            return GamepadFamily::Xbox;
        }
        if (has("playstation") || has("dualsense") || has("dualshock") || has("sony") || has("ps4") || has("ps5"))
        {
            return GamepadFamily::PlayStation;
        }
        if (has("nintendo") || has("switch") || has("joy-con"))
        {
            return GamepadFamily::Nintendo;
        }
        return GamepadFamily::Generic;
    }

    std::string GamepadGlyphs::Resolve(GamepadButtons button, std::optional<GamepadFamily> family)
    {
        GamepadFamily style{};
        if (family.has_value())
        {
            style = *family;
        }
        else if (GamepadOptions::GlyphStyle() == GamepadFamily::Unknown)
        {
            const std::optional<GamepadDeviceSnapshot> active = GamepadManager::ActiveDevice();
            style = active.has_value() ? active->Family : GamepadFamily::Generic;
        }
        else
        {
            style = GamepadOptions::GlyphStyle();
        }
        if (style == GamepadFamily::PlayStation)
        {
            switch (button)
            {
            case GamepadButtons::A: return "Cross";
            case GamepadButtons::B: return "Circle";
            case GamepadButtons::X: return "Square";
            case GamepadButtons::Y: return "Triangle";
            case GamepadButtons::LeftBumper: return "L1";
            case GamepadButtons::RightBumper: return "R1";
            case GamepadButtons::LeftTrigger: return "L2";
            case GamepadButtons::RightTrigger: return "R2";
            case GamepadButtons::Back: return "Share";
            case GamepadButtons::Start: return "Options";
            default: break;
            }
        }
        if (style == GamepadFamily::Nintendo)
        {
            switch (button)
            {
            case GamepadButtons::A: return "B";
            case GamepadButtons::B: return "A";
            case GamepadButtons::X: return "Y";
            case GamepadButtons::Y: return "X";
            case GamepadButtons::LeftBumper: return "L";
            case GamepadButtons::RightBumper: return "R";
            case GamepadButtons::LeftTrigger: return "ZL";
            case GamepadButtons::RightTrigger: return "ZR";
            case GamepadButtons::Back: return "-";
            case GamepadButtons::Start: return "+";
            default: break;
            }
        }
        switch (button)
        {
        case GamepadButtons::LeftBumper: return "LB";
        case GamepadButtons::RightBumper: return "RB";
        case GamepadButtons::LeftTrigger: return "LT";
        case GamepadButtons::RightTrigger: return "RT";
        case GamepadButtons::LeftThumb: return "Left stick";
        case GamepadButtons::RightThumb: return "Right stick";
        case GamepadButtons::DpadUp: return "D-pad up";
        case GamepadButtons::DpadDown: return "D-pad down";
        case GamepadButtons::DpadLeft: return "D-pad left";
        case GamepadButtons::DpadRight: return "D-pad right";
        case GamepadButtons::None: return "unbound";
        default: return ToString(button);
        }
    }
}

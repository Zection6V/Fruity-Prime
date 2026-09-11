#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace MphRead::Mods::Input
{
    enum class TouchControl : std::int32_t
    {
        Shoot = 0,
        Jump = 1,
        Morph = 2,
        ScanVisor = 3,
        Scan = 4,
        Missile = 5,
        WeaponMenu = 6,
        Zoom = 7,
        Pause = 8,
        Scoreboard = 9,
        Chat = 10
    };

    struct TouchControlOrderEntry
    {
        TouchControl Control;
        std::string Label;
    };

    class TouchSettings final
    {
    public:
        static bool ButtonsVisible;
        static TouchControlOrderEntry (&Order)[11];

        static bool IsEnabled(TouchControl control);
        static void SetEnabled(TouchControl control, bool enabled);
        static bool Shown(TouchControl control);
        static void Reset();
        static std::string SettingKey(TouchControl control);

        inline static constexpr std::string_view ButtonsSettingKey = "touch_buttons";

        static bool ReadSetting(
            std::optional<std::string_view> key,
            std::optional<std::string_view> value
        );
        static void WriteSettings(std::vector<std::string>& lines);

    private:
        TouchSettings() = delete;
        ~TouchSettings() = delete;
        TouchSettings(const TouchSettings&) = delete;
        TouchSettings& operator=(const TouchSettings&) = delete;
        TouchSettings(TouchSettings&&) = delete;
        TouchSettings& operator=(TouchSettings&&) = delete;

        static std::unordered_set<TouchControl> _hidden;
        static TouchControlOrderEntry _order[11];
    };
}

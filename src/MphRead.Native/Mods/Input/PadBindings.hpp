#pragma once

#include "GamepadState.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Input
{
    enum class PadAction : std::int32_t
    {
        Shoot = 0,
        Zoom = 1,
        Jump = 2,
        Morph = 3,
        Scan = 4,
        ScanVisor = 5,
        Scoreboard = 6,
        NextWeapon = 7,
        PrevWeapon = 8,
        Missile = 9,
        PowerBeam = 10,
        Menu = 11,
        Chat = 12
    };

    class PadBindings final
    {
    public:
        static const std::array<PadAction, 13>& Actions() noexcept;

        static GamepadButtons Get(PadAction action);
        static void Set(PadAction action, GamepadButtons buttons);
        static GamepadButtons Default(PadAction action);
        static void Reset();
        static std::string Name(PadAction action);
        static std::string Describe(GamepadButtons buttons);
        static std::string ButtonName(GamepadButtons button);
        static std::string SettingKey(PadAction action);
        static bool TryLoad(
            std::optional<std::string_view> key,
            std::optional<std::string_view> value
        );

    private:
        PadBindings() = delete;
        ~PadBindings() = delete;
        PadBindings(const PadBindings&) = delete;
        PadBindings& operator=(const PadBindings&) = delete;
        PadBindings(PadBindings&&) = delete;
        PadBindings& operator=(PadBindings&&) = delete;

        static std::array<GamepadButtons, 13> _defaults;
        static std::array<GamepadButtons, 13> _current;
        static std::array<PadAction, 13> _actions;
    };
}

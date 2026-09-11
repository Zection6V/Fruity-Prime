#pragma once

#include "GamepadState.hpp"

#include <array>
#include <cstdint>
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
        [[nodiscard]] static const std::array<PadAction, 13>& Actions() noexcept;

        [[nodiscard]] static GamepadButtons Get(PadAction action);
        static void Set(PadAction action, GamepadButtons buttons);
        [[nodiscard]] static GamepadButtons Default(PadAction action);
        static void Reset();
        [[nodiscard]] static std::string Name(PadAction action);
        [[nodiscard]] static std::string Describe(GamepadButtons buttons);
        [[nodiscard]] static std::string ButtonName(GamepadButtons button);
        [[nodiscard]] static std::string SettingKey(PadAction action);
        static bool TryLoad(std::string_view key, std::string_view value);

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

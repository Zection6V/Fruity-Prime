#pragma once

#include "GamepadLayout.hpp"
#include "GamepadState.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Input
{
    enum class GamepadFamily : std::int32_t;
    enum class GamepadCapabilities : std::int32_t;

    class GamepadDesktop final
    {
    public:
        GamepadDesktop() = delete;

        static void DeviceChanged(std::int32_t index);
        static void Poll();

    private:
        struct Slot
        {
            std::optional<std::string> Id{};
            std::int32_t Generation = 0;
            float LeftFloor = 0;
            float RightFloor = 0;
            std::string Name = "gamepad";
            std::string Mapping{};
            bool Mapped = false;
            bool XInput = false;
            GamepadFamily Family{};
            GamepadLayout Layout{};
            GamepadCapabilities Capabilities{};
        };

        static void PollUnsafe();
        [[nodiscard]] static bool TryRead(std::int32_t slot);
        [[nodiscard]] static bool TryReadRaw(std::int32_t slot);
        static void Add(GamepadButtons& into, const std::array<std::uint8_t, 15>& buttons, std::int32_t index,
            GamepadButtons flag) noexcept;

        static constexpr std::int32_t ButtonA = 0;
        static constexpr std::int32_t ButtonB = 1;
        static constexpr std::int32_t ButtonX = 2;
        static constexpr std::int32_t ButtonY = 3;
        static constexpr std::int32_t ButtonLeftBumper = 4;
        static constexpr std::int32_t ButtonRightBumper = 5;
        static constexpr std::int32_t ButtonBack = 6;
        static constexpr std::int32_t ButtonStart = 7;
        static constexpr std::int32_t ButtonLeftThumb = 9;
        static constexpr std::int32_t ButtonRightThumb = 10;
        static constexpr std::int32_t ButtonDpadUp = 11;
        static constexpr std::int32_t ButtonDpadRight = 12;
        static constexpr std::int32_t ButtonDpadDown = 13;
        static constexpr std::int32_t ButtonDpadLeft = 14;
        static constexpr std::int32_t AxisLeftX = 0;
        static constexpr std::int32_t AxisLeftY = 1;
        static constexpr std::int32_t AxisRightX = 2;
        static constexpr std::int32_t AxisRightY = 3;
        static constexpr std::int32_t AxisLeftTrigger = 4;
        static constexpr std::int32_t AxisRightTrigger = 5;

        static std::array<Slot, 16> Slots;
        inline static bool _unavailable = false;
    };
}

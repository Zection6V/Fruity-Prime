#pragma once

#include "GamepadState.hpp"
#include "../../NativeRuntime/System/Numerics.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Mods::Input
{
    enum class GamepadFamily : std::int32_t;
    enum class GamepadCapabilities : std::int32_t;

    struct GamepadAuxState
    {
        System::Numerics::Vector3 Gyro{};
        System::Numerics::Vector3 Accelerometer{};
        System::Numerics::Vector2 Touch0{};
        System::Numerics::Vector2 Touch1{};
        bool TouchpadPressed = false;

        friend bool operator==(const GamepadAuxState&, const GamepadAuxState&) = default;
    };

    struct GamepadDeviceSnapshot
    {
        std::string DeviceId{};
        std::string Name{};
        std::string ProfileKey{};
        GamepadFamily Family{};
        GamepadCapabilities Capabilities{};
        bool IsMapped = false;
        std::string Mapping{};
        GamepadState State{};
        GamepadState RawState{};
        GamepadAuxState AuxState{};
        std::int64_t Revision = 0;

        friend bool operator==(const GamepadDeviceSnapshot&, const GamepadDeviceSnapshot&) = default;
    };
}

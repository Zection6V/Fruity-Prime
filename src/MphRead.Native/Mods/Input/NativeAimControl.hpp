#pragma once
#include <cstdint>

namespace MphRead::Mods::Input::NativeAim
{
    struct Control final
    {
        std::uint16_t Flags = 0x28; // Exact + unconditional Dual direction
        std::uint8_t Flag84E = 0;
        std::uint32_t Left = 1, Right = 2, Up = 4, Down = 8;
        std::uint32_t AimAction = 16, EnableAction = 32, Movement = 0xF00;
        // Degrees per native gameplay tick. Preserve intent without Q12 truncation.
        float MaxX = 8.0F, MaxY = 8.0F;
        // EU1.1 stock preset 020BDFA0+0x94/+0x98 -> Player+3F8/+3FC.
        float TouchScaleX = -2211.0F / 4096, TouchScaleY = -1138.0F / 4096;
        std::uint32_t AutoPitchTimer = 0, AutoPitchLimit = 0;
        float AutoPitchScale = 0.1F;
        // DS rules on the 60 Hz simulation (the Classic setting) unless set:
        // then Touch is the 30 Hz native sample and Dual waits for the native
        // tick, exactly as the ROM. A diagnostic (-nativeaim) and nothing else.
        bool RomCadence = false;
    };
}

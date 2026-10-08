#pragma once
#include <cstdint>

namespace MphRead::Mods::Input::NativeAim
{
    struct Control final
    {
        // 0x02 Touch aim, 0x04 vertical needs Enable, 0x08 unconditional Dual,
        // 0x20 Exact, 0x40 no AutoPitch. The default is Exact + unconditional.
        std::uint16_t Flags = 0x28;
        std::uint8_t Flag84E = 0;
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

#pragma once
#include <array>
#include <cstdint>

namespace MphRead::Mods::Input::NativeAim
{
    struct InputSlot final
    {
        // Global slots and Player shadow are separate values. Zeroing a copied
        // shadow cannot mutate its source; no host-global input router is added.
        std::array<std::uint8_t, 0x48> Bytes{};
        constexpr std::uint16_t Read(unsigned offset) const noexcept
        { return static_cast<std::uint16_t>(Bytes[offset] | (Bytes[offset + 1] << 8)); }
        constexpr void Write(unsigned offset, std::uint16_t value) noexcept
        { Bytes[offset] = static_cast<std::uint8_t>(value); Bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8); }
        constexpr void Clear() noexcept { Bytes.fill(0); }
        // Producer gate at 02029C48 is independent of the Biped Dual skip gate.
        constexpr void Produce(std::uint16_t held, bool flag84c, std::uint8_t flag84e) noexcept
        {
            if (!flag84c && flag84e == 0) return;
            held &= 0xFFFU;
            const auto previous = Read(0);
            const auto press = static_cast<std::uint16_t>(held & ~previous);
            Write(0, held); Write(2, previous); Write(4, press);
            Write(8, static_cast<std::uint16_t>(previous & ~held));
            Write(0xA, 0); Write(0xC, 0);
            // The ROM scans keys in order. A later simultaneous new press sees
            // invalidation by the earlier key, rather than precomputed windows.
            for (unsigned key = 0; key < 12; ++key)
            {
                const auto bit = static_cast<std::uint16_t>(1U << key);
                if ((press & bit) != 0)
                {
                    const auto counter = Bytes[0xE + key];
                    for (unsigned other = 0; other < 12; ++other)
                        if (other != key) Bytes[0xE + other] = 0xFF;
                    if (counter >= 4 && counter <= 7) Write(0xA, static_cast<std::uint16_t>(Read(0xA) | bit));
                    if (counter >= 3 && counter <= 7) Write(0xC, static_cast<std::uint16_t>(Read(0xC) | bit));
                    Bytes[0xE + key] = 0;
                }
                else if (Bytes[0xE + key] != 0xFF) ++Bytes[0xE + key];
            }
        }
    };
    static_assert(sizeof(InputSlot) == 0x48);

    constexpr bool TestAction(const InputSlot& slot, std::uint32_t spec) noexcept
    {
        unsigned offset = 0, touchBit = 0;
        if (spec & 0x40000U) { offset = 4; touchBit = 2; }
        else if (spec & 0x100000U) { offset = 8; touchBit = 4; }
        else if (spec & 0x80000U) { offset = 0xA; touchBit = 5; }
        return (slot.Read(offset) & spec) != 0
            || ((spec & 0x10000U) != 0 && (slot.Bytes[0x34] & (1U << touchBit)) != 0);
    }

}

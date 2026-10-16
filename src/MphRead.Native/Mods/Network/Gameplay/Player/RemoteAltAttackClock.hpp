#pragma once

#include "Mods/Network/Domain/NetworkCapacity.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace MphRead::Mods::Network
{
    // A remote player's alt-attack hold, counted on the owner's own frames.
    //
    // A charged alt attack (Vhoscythe's startup) used to be counted on every
    // other machine from the held bit's arrival: the press arriving late and
    // the release on time made the hold shorter there than on the owner's
    // screen, and a release right at the threshold spun for the owner and
    // never for anybody else. The relayed intent already carries the owner's
    // frame number and how many frames ago the press was, so the count is
    // the owner's: press frame = intent frame - press age, and now = intent
    // frame + the steps since this machine first applied that intent frame.
    // Counting from the first application rather than from arrival is the
    // same on every role: a dedicated server drains its socket before it
    // advances the frame, a client after, and arrival age differs by one.
    // No new data is sent.
    class RemoteAltAttackClock final
    {
    public:
        struct Sample final
        {
            std::uint16_t Generation = 0;
            std::uint16_t Life = 0;
            std::uint32_t IntentFrame = 0;
            // This machine's simulation frame at this step.
            std::uint32_t LocalFrame = 0;
            bool Pressed = false;
            std::int32_t PressAge = 0;
            bool Held = false;
        };

        RemoteAltAttackClock() = delete;

        // Owner frames since the press at this step, while the owner holds
        // it; nothing when it is not held or the press was never seen.
        [[nodiscard]] static std::optional<std::uint32_t> Observe(std::int32_t slot, const Sample& sample) noexcept;
        static void ForgetSlot(std::int32_t slot) noexcept;
        static void Reset() noexcept;

    private:
        struct Hold final
        {
            bool Active = false;
            std::uint16_t Generation = 0;
            std::uint16_t Life = 0;
            std::uint32_t PressFrame = 0;
            std::uint32_t IntentFrame = 0;
            std::uint32_t AppliedAt = 0;
        };
        static std::array<Hold, NetworkSlotCapacity> _holds;
    };
}

#pragma once

#include <cstdint>

namespace MphRead::Mods::Network
{
    // One power-up of one player as a snapshot reports it: the authority's
    // grant sequence (0: never granted) and the frames it has left.
    struct PowerUpReport
    {
        std::uint8_t Sequence = 0;
        std::uint16_t Remaining = 0;
    };

    // On the wire, sixteen bits a power-up: the sequence's five above the
    // remaining frames' eleven (PowerUpFrames, 1800, fits in 2047).
    inline constexpr std::uint16_t RemainingMask = 0x07FF;

    [[nodiscard]] constexpr std::uint16_t PackReport(PowerUpReport report) noexcept
    {
        const std::uint16_t remaining = report.Remaining > RemainingMask ? RemainingMask : report.Remaining;
        return static_cast<std::uint16_t>(((report.Sequence & 0x1FU) << 11) | remaining);
    }

    [[nodiscard]] constexpr PowerUpReport UnpackReport(std::uint16_t packed) noexcept
    {
        return PowerUpReport{ static_cast<std::uint8_t>(packed >> 11),
            static_cast<std::uint16_t>(packed & RemainingMask) };
    }

    enum class PowerUpStep : std::uint8_t
    {
        // Nothing to change: the two copies count down together.
        Keep,
        // A grant this copy has not shown: start it, as a pickup would.
        Start,
        // The same effect, retimed to the authority's without fuss.
        Retime,
        // The authority's has ended (it ran out, its holder died).
        End
    };

    // How a client takes one snapshot's word about one power-up. A copy's
    // own timer counts down in step with the authority's, so it is only ever
    // corrected, never assigned each snapshot: the effects' sounds key off
    // exact frames, and a timer jumped past one never plays it.
    //
    // `seen` is the sequence this copy last took; `awaiting` is a prediction
    // of its own still waiting to be confirmed, which a snapshot that has not
    // caught up with it must not end.
    [[nodiscard]] constexpr PowerUpStep ReconcilePowerUp(PowerUpReport reported, std::uint8_t seen,
        std::uint16_t local, bool awaiting) noexcept
    {
        constexpr std::uint16_t Tolerance = 8;
        if (reported.Sequence != seen && reported.Remaining > 0)
        {
            if (local == 0)
            {
                return PowerUpStep::Start;
            }
            const std::uint16_t gap = local > reported.Remaining
                ? static_cast<std::uint16_t>(local - reported.Remaining)
                : static_cast<std::uint16_t>(reported.Remaining - local);
            return gap > Tolerance ? PowerUpStep::Retime : PowerUpStep::Keep;
        }
        if (reported.Remaining == 0 && local > 0 && !awaiting)
        {
            return PowerUpStep::End;
        }
        return PowerUpStep::Keep;
    }
}

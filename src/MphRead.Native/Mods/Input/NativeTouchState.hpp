#pragma once

#include <array>
#include <cstdint>

namespace MphRead::Mods::Input
{
    // The DS touch state the Morph Ball reads, as EU1.1 02029778 produces it
    // into the input slot: current contact (+0x34 bit0), continued contact
    // (+0x34 bit3), the last four signed deltas (+0x38..+0x46) and their SUM
    // (+0x2A/+0x2C). A sum, not an average: the `lsl #2; asr #2` there only
    // keeps the result in 30 bits before the halfword store.
    //
    // This type is the producer and nothing else. Where a contact comes from
    // is TouchInputAdapter's business; what it does to the ball is
    // Entities::MorphBallTouchRules's.
    struct NativeTouchState
    {
        // Everything the Morph Ball reads (+0x34 bits 0/3, +0x2A, +0x2C):
        // what a player's owner reports for the authority to run the same
        // branches on. The history and positions behind it stay local.
        struct Reported
        {
            bool Down = false;
            bool Continued = false;
            std::int16_t Delta4X = 0;
            std::int16_t Delta4Y = 0;

            friend bool operator==(const Reported&, const Reported&) = default;
        };

        bool Down = false;
        bool PreviousDown = false;
        bool Continued = false;

        std::uint8_t X = 0;
        std::uint8_t Y = 0;
        std::uint8_t PreviousX = 0;
        std::uint8_t PreviousY = 0;

        std::array<std::int16_t, 4> DeltaHistoryX{};
        std::array<std::int16_t, 4> DeltaHistoryY{};

        std::int16_t Delta4X = 0;
        std::int16_t Delta4Y = 0;

        // One producer tick from an absolute DS position (clamped to 0-255 /
        // 0-191); the delta is current minus previous.
        void Update(bool down, std::int32_t dsX, std::int32_t dsY) noexcept;

        // One producer tick from a stylus known only by how far it moved: the
        // delta goes into the history as given, clamped to what two DS
        // coordinates can differ by, so no screen edge ever eats one.
        void UpdateRelative(bool down, std::int32_t dx, std::int32_t dy) noexcept;

        void Clear() noexcept;

        [[nodiscard]] Reported Report() const noexcept { return {Down, Continued, Delta4X, Delta4Y}; }
        // Take a reported state as this one: the gameplay fields are set and
        // the local history is dropped, since it describes nobody's stylus.
        void Assign(const Reported& reported) noexcept;

    private:
        // The contact bits and the clear path every tick shares. True when
        // this tick is a continued contact and a delta should be pushed.
        [[nodiscard]] bool BeginTick(bool down) noexcept;
        void ClearHistory() noexcept;
        void Push(std::int16_t dx, std::int16_t dy) noexcept;
    };

    // s16 sum of four s16 samples, wrapped the way `strh` stores it.
    [[nodiscard]] std::int16_t Sum4AsS16(const std::array<std::int16_t, 4>& samples) noexcept;
}

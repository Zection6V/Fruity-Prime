#include "Mods/Network/Gameplay/Player/RemoteAltAttackClock.hpp"

namespace MphRead::Mods::Network
{
    std::array<RemoteAltAttackClock::Hold, NetworkSlotCapacity> RemoteAltAttackClock::_holds{};

    std::optional<std::uint32_t> RemoteAltAttackClock::Observe(std::int32_t slot, const Sample& sample) noexcept
    {
        if (slot < 0 || static_cast<std::size_t>(slot) >= _holds.size())
        {
            return std::nullopt;
        }
        Hold& hold = _holds[static_cast<std::size_t>(slot)];
        if (hold.Active && (hold.Generation != sample.Generation || hold.Life != sample.Life))
        {
            hold = {};
        }
        if (sample.Pressed)
        {
            const auto age = static_cast<std::uint32_t>(sample.PressAge < 0 ? 0 : sample.PressAge);
            hold = {true, sample.Generation, sample.Life, sample.IntentFrame - age,
                sample.IntentFrame, sample.LocalFrame};
        }
        if (!sample.Held)
        {
            hold.Active = false;
            return std::nullopt;
        }
        if (!hold.Active)
        {
            return std::nullopt;
        }
        if (sample.IntentFrame != hold.IntentFrame)
        {
            hold.IntentFrame = sample.IntentFrame;
            hold.AppliedAt = sample.LocalFrame;
        }
        const std::uint32_t ownerNow = sample.IntentFrame + (sample.LocalFrame - hold.AppliedAt);
        // A press stamped after the intent that carries it cannot happen; a
        // wrapped difference that large is treated as no time held.
        const std::uint32_t elapsed = ownerNow - hold.PressFrame;
        return elapsed > 0x7FFFFFFFU ? 0U : elapsed;
    }

    void RemoteAltAttackClock::ForgetSlot(std::int32_t slot) noexcept
    {
        if (slot >= 0 && static_cast<std::size_t>(slot) < _holds.size())
        {
            _holds[static_cast<std::size_t>(slot)] = {};
        }
    }

    void RemoteAltAttackClock::Reset() noexcept
    {
        _holds.fill({});
    }
}

#include "PowerUpLedger.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    PowerUpLedger::Grant PowerUpLedger::Begin(std::int32_t slot, PowerUp kind, std::uint32_t start,
        std::uint16_t duration) noexcept
    {
        if (!Valid(slot))
        {
            return Grant{};
        }
        Record& record = _records[static_cast<std::size_t>(slot)][IndexOf(kind)];
        record.Sequence = NextGrantSequence(record.Sequence);
        Grant& running = record.Grants[record.Newest];
        if (running.Sequence != 0 && running.Expire > start)
        {
            // Picked up again while it ran: the old one ends where this begins.
            running.Expire = std::max(running.Start, start);
        }
        record.Newest = (record.Newest + 1) % History;
        record.Grants[record.Newest] = Grant{ record.Sequence, start, start + duration };
        return record.Grants[record.Newest];
    }

    void PowerUpLedger::End(std::int32_t slot, PowerUp kind, std::uint32_t frame) noexcept
    {
        if (!Valid(slot))
        {
            return;
        }
        Record& record = _records[static_cast<std::size_t>(slot)][IndexOf(kind)];
        Grant& running = record.Grants[record.Newest];
        if (running.Sequence != 0 && running.Expire > frame)
        {
            running.Expire = std::max(running.Start, frame);
        }
    }

    PowerUpLedger::Grant PowerUpLedger::Current(std::int32_t slot, PowerUp kind) const noexcept
    {
        if (!Valid(slot))
        {
            return Grant{};
        }
        const Record& record = _records[static_cast<std::size_t>(slot)][IndexOf(kind)];
        return record.Grants[record.Newest];
    }

    bool PowerUpLedger::RanAt(std::int32_t slot, PowerUp kind, std::uint32_t frame, std::uint32_t late) const noexcept
    {
        if (!Valid(slot))
        {
            return false;
        }
        const Record& record = _records[static_cast<std::size_t>(slot)][IndexOf(kind)];
        return std::any_of(record.Grants.begin(), record.Grants.end(), [&](const Grant& grant)
        {
            return grant.Sequence != 0 && grant.Start <= frame && frame < grant.Expire + late;
        });
    }

    void PowerUpLedger::Forget(std::int32_t slot) noexcept
    {
        if (Valid(slot))
        {
            _records[static_cast<std::size_t>(slot)] = {};
        }
    }

    void PowerUpLedger::Reset() noexcept
    {
        _records = {};
    }
}

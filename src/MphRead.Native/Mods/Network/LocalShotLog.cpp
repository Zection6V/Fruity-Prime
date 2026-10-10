#include "LocalShotLog.hpp"

#include <algorithm>

namespace MphRead::Mods::Network
{
    void LocalShotLog::Record(std::uint32_t frame, ::MphRead::BeamType weapon) noexcept
    {
        _sequence = std::max(1U, _sequence + 1U);
        if (_length == _history.size())
        {
            std::rotate(_history.begin(), _history.begin() + 1, _history.end());
            --_length;
        }
        _history[_length++] = {_sequence, frame, static_cast<std::uint8_t>(weapon)};
    }

    void LocalShotLog::Fill(IntentPacket& intent, std::uint32_t frame) const noexcept
    {
        intent.ShotHistory = _history;
        intent.ShotHistoryLength = _length;
        if (intent.HasShot && _length > 0 && _history[_length - 1].Frame == frame)
        {
            intent.ShotSequence = _history[_length - 1].Sequence;
            intent.ShotWeaponId = _history[_length - 1].WeaponId;
        }
    }

    void LocalShotLog::Reset() noexcept
    {
        _history = {};
        _length = 0;
    }
}

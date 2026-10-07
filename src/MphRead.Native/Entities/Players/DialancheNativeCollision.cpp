#include "DialancheNativeCollision.hpp"

namespace MphRead::Entities
{
    void DialancheNativeCollision::Reset(OpenTK::Mathematics::Vector3 position) noexcept
    {
        _initial = {position, position};
        _previous = {};
        _latest = {};
    }

    void DialancheNativeCollision::Record(std::uint64_t tick, OpenTK::Mathematics::Vector3 left,
        OpenTK::Mathematics::Vector3 right) noexcept
    {
        if (_latest.Valid && tick < _latest.Tick) return;
        // Recomputing a visual pose on the same tick must not evict the older
        // pose all this tick's consumers use.
        if (!_latest.Valid || tick != _latest.Tick) _previous = _latest;
        _latest = {true, tick, {left, right}};
    }

    DialancheNativeCollision::Pose DialancheNativeCollision::PoseForHit(std::uint64_t tick) const noexcept
    {
        if (_latest.Valid && _latest.Tick < tick) return _latest.Value;
        if (_previous.Valid && _previous.Tick < tick) return _previous.Value;
        return _initial;
    }
}

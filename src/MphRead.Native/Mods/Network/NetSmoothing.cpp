#include "NetSmoothing.hpp"

#include "NetLifecycleTracker.hpp"
#include "NetLog.hpp"
#include "NetPlayerLifecycle.hpp"
#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "NetTimingDiagnostics.hpp"

#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <cmath>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using OpenTK::Mathematics::Vector3;

    bool NetSmoothing::_enabled = true;
    std::int32_t NetSmoothing::_delay = NetSmoothing::MinDelayFrames;
    std::int32_t NetSmoothing::_sinceGrew = 0;
    double NetSmoothing::_readFrame = 0;
    bool NetSmoothing::_running = false;
    std::int32_t NetSmoothing::_sinceStarved = 0;
    std::int64_t NetSmoothing::_starved = 0;
    std::int64_t NetSmoothing::_interpolated = 0;
    std::int64_t NetSmoothing::_held = 0;
    std::int64_t NetSmoothing::_snaps = 0;
    std::int64_t NetSmoothing::_steps = 0;
    double NetSmoothing::_stepSum = 0;
    float NetSmoothing::_worstStep = 0;
    std::int64_t NetSmoothing::_stalledFrames = 0;
    std::int32_t NetSmoothing::_worstStall = 0;
    std::array<std::int32_t, NetSmoothing::Slots> NetSmoothing::_stallRun{};
    NetSmoothing::Grid<std::uint16_t> NetSmoothing::_life{};
    NetSmoothing::Grid<std::uint16_t> NetSmoothing::_generation{};
    NetSmoothing::Grid<Vector3> NetSmoothing::_position{};
    NetSmoothing::Grid<bool> NetSmoothing::_altForm{};
    NetSmoothing::Grid<bool> NetSmoothing::_live{};
    std::array<std::uint32_t, NetSmoothing::HistoryFrames> NetSmoothing::_stamp{};
    std::uint32_t NetSmoothing::_newest = 0;
    std::array<Vector3, NetSmoothing::Slots> NetSmoothing::_lastSampled{};
    std::array<bool, NetSmoothing::Slots> NetSmoothing::_sampledSeen{};

    namespace
    {
        template <typename T, std::size_t Rows, std::size_t Columns>
        void ClearGrid(std::array<std::array<T, Columns>, Rows>& grid)
        {
            for (auto& row : grid)
            {
                row.fill(T{});
            }
        }
    }

    void NetSmoothing::ResetSlot(std::int32_t slot)
    {
        if (slot < 0 || slot >= Slots)
        {
            return;
        }
        const auto s = static_cast<std::size_t>(slot);
        for (std::size_t i = 0; i < HistoryFrames; i++)
        {
            _live[s][i] = false;
            _life[s][i] = 0;
            _generation[s][i] = 0;
        }
        _sampledSeen[s] = false;
        _stallRun[s] = 0;
    }

    bool NetSmoothing::Active()
    {
        return _enabled && NetSession::Active() && !NetSession::IsAuthority() && !NetSession::IsHost()
            && _running;
    }

    void NetSmoothing::Record(std::uint32_t frame, std::span<const PlayerState> states)
    {
        if (!_enabled || frame == 0)
        {
            return;
        }
        // Only an explicit stream change resets this clock.
        if (_running && !NetLifecycleTracker::Newer(frame, _newest))
        {
            return;
        }
        const auto index = static_cast<std::size_t>(frame % HistoryFrames);
        _stamp[index] = frame;
        for (std::size_t i = 0; i < Slots; i++)
        {
            _live[i][index] = false;
        }
        for (const PlayerState& state : states)
        {
            const std::int32_t slot = state.SlotIndex;
            if (slot < 0 || slot >= Slots)
            {
                continue;
            }
            const auto s = static_cast<std::size_t>(slot);
            const bool inPlay = (state.Flags & PlayerState::FlagActive) != 0
                && (state.Flags & PlayerState::FlagSpawned) != 0
                && state.Health > 0;
            _life[s][index] = state.LifeId;
            _generation[s][index] = state.SlotGeneration;
            _live[s][index] = inPlay;
            _position[s][index] = state.Position;
            _altForm[s][index] = (state.Flags & PlayerState::FlagAltForm) != 0;
        }
        if (!_running || frame > _newest)
        {
            _newest = frame;
        }
        if (!_running)
        {
            _running = true;
            _readFrame = frame > static_cast<std::uint32_t>(_delay)
                ? static_cast<double>(frame) - _delay : static_cast<double>(frame);
        }
    }

    void NetSmoothing::Restart(std::uint32_t frame)
    {
        _stamp.fill(0);
        ClearGrid(_live);
        _sampledSeen.fill(false);
        _stallRun.fill(0);
        _newest = 0;
        _running = false;
        _sinceStarved = 0;
        _sinceGrew = GrowCooldownFrames;
        NetLog::Event("playout clock re-based on frame " + std::to_string(frame));
    }

    void NetSmoothing::Tick()
    {
        if (!_enabled || !NetSession::Active() || !_running)
        {
            return;
        }
        _readFrame += 1.0;
        const double target = static_cast<double>(_newest) - _delay;
        const double error = target - _readFrame;
        if (std::abs(error) > SnapError)
        {
            // A stall, a rejoin, a rotation, a counter that restarted. Walking
            // across ten frames of error would be four seconds of everybody
            // moving at the wrong speed, which is a worse thing to look at
            // than one jump.
            _readFrame = target;
            _snaps++;
            NetTimingDiagnostics::Correction();
        }
        else
        {
            _readFrame += error * Correction;
        }
        if (_readFrame > _newest)
        {
            // Past everything that has arrived. The read point is pinned rather
            // than allowed to run on, because the alternative is extrapolating
            // -- and a guessed position puts a player through a wall and then
            // snaps them out of it.
            _readFrame = _newest;
            _starved++;
            _sinceStarved = 0;
            if (_delay < MaxDelayFrames && _sinceGrew >= GrowCooldownFrames)
            {
                _delay++;
                _sinceGrew = 0;
            }
        }
        else if (++_sinceStarved > ShrinkAfterFrames && _delay > MinDelayFrames)
        {
            _delay--;
            _sinceStarved = 0;
        }
        _sinceGrew++;
    }

    bool NetSmoothing::Sample(std::int32_t slot, Vector3& position, bool& altForm)
    {
        position = Vector3::Zero;
        altForm = false;
        if (!Active() || slot < 0 || slot >= Slots)
        {
            return false;
        }
        const auto lower = static_cast<std::uint32_t>(std::floor(_readFrame));
        const auto fraction = static_cast<float>(_readFrame - lower);
        Vector3 a{};
        bool altA = false;
        if (!Lookup(slot, lower, a, altA))
        {
            return false;
        }
        altForm = altA;
        Vector3 b{};
        bool altB = false;
        if (fraction <= 0.0001F || !Lookup(slot, lower + 1, b, altB) || altA != altB)
        {
            // Nothing on the far side, or the player changed form between the
            // two -- a biped's position and a morph ball's are measured from
            // different centres, so blending them slides the model half a
            // body. Hold the near one.
            position = a;
            _held++;
            NoteStep(slot, position);
            return true;
        }
        const Vector3 travel = b - a;
        if (travel.LengthSquared() > SnapDistance * SnapDistance)
        {
            // A teleporter, a respawn, a jump pad's launch frame -- not
            // something to slide across. The near side, not the far one:
            // NetUnlagged.Reconcile makes the same refusal at the other end and
            // falls back to the near side, so both worlds stay the same world.
            position = a;
            _held++;
            NoteStep(slot, position);
            return true;
        }
        position = a + OpenTK::Mathematics::Multiply(travel, fraction);
        _interpolated++;
        NoteStep(slot, position);
        return true;
    }

    bool NetSmoothing::Lookup(std::int32_t slot, std::uint32_t frame, Vector3& position, bool& altForm)
    {
        position = Vector3::Zero;
        altForm = false;
        if (frame == 0 || frame > _newest)
        {
            return false;
        }
        const auto index = static_cast<std::size_t>(frame % HistoryFrames);
        const auto s = static_cast<std::size_t>(slot);
        if (_stamp[index] != frame || !_live[s][index]
            || !NetPlayerLifecycle::Matches(slot, _generation[s][index], _life[s][index]))
        {
            return false;
        }
        position = _position[s][index];
        altForm = _altForm[s][index];
        return std::isfinite(position.X) && std::isfinite(position.Y) && std::isfinite(position.Z);
    }

    void NetSmoothing::NoteStep(std::int32_t slot, Vector3 position)
    {
        const auto s = static_cast<std::size_t>(slot);
        if (_sampledSeen[s])
        {
            const float step = OpenTK::Mathematics::Length(position - _lastSampled[s]);
            if (std::isfinite(step))
            {
                _steps++;
                _stepSum += step;
                if (step > _worstStep)
                {
                    _worstStep = step;
                }
                if (step < 0.0005F)
                {
                    _stalledFrames++;
                    _stallRun[s]++;
                    if (_stallRun[s] > _worstStall)
                    {
                        _worstStall = _stallRun[s];
                    }
                }
                else
                {
                    _stallRun[s] = 0;
                }
            }
        }
        _lastSampled[s] = position;
        _sampledSeen[s] = true;
    }

    bool NetSmoothing::AckPoint(std::uint32_t& frame, std::uint8_t& subFrame)
    {
        frame = 0;
        subFrame = 0;
        if (!Active())
        {
            return false;
        }
        const auto lower = static_cast<std::uint32_t>(std::floor(_readFrame));
        if (lower == 0)
        {
            return false;
        }
        frame = lower;
        subFrame = static_cast<std::uint8_t>(Runtime::MathClamp(
            Runtime::ConvertToInt32Net9((_readFrame - lower) * 256.0), 0, 255));
        return true;
    }

    void NetSmoothing::Reset()
    {
        _stamp.fill(0);
        ClearGrid(_live);
        _sampledSeen.fill(false);
        _stallRun.fill(0);
        _newest = 0;
        _readFrame = 0;
        _running = false;
        _sinceStarved = 0;
        _sinceGrew = GrowCooldownFrames;
        _delay = MinDelayFrames;
        _starved = 0;
        _interpolated = 0;
        _held = 0;
        _snaps = 0;
        _steps = 0;
        _stepSum = 0;
        _worstStep = 0;
        _stalledFrames = 0;
        _worstStall = 0;
    }

    void NetSmoothing::NoteRoomChanged()
    {
        _stamp.fill(0);
        ClearGrid(_live);
        _sampledSeen.fill(false);
        _stallRun.fill(0);
        _newest = 0;
        _readFrame = 0;
        _running = false;
        _sinceGrew = GrowCooldownFrames;
    }

    std::optional<std::string> NetSmoothing::Describe()
    {
        if (_steps == 0)
        {
            return std::nullopt;
        }
        const double mean = _stepSum / static_cast<double>(_steps);
        const double stalled = 100.0 * static_cast<double>(_stalledFrames) / static_cast<double>(_steps);
        return "puppet smoothing: " + (_enabled ? "on, " + std::to_string(_delay) + " frames of buffer"
                : std::string("off")) + ", "
            + std::to_string(_interpolated) + " interpolated / " + std::to_string(_held) + " held, "
            + std::to_string(_starved) + " starved, "
            + std::to_string(_snaps) + " clock snaps; steps mean " + Runtime::ToString(mean, "F4")
            + " units, worst " + Runtime::ToString(_worstStep, "F3") + ", "
            + Runtime::ToString(stalled, "F1") + "% of frames still (longest run "
            + std::to_string(_worstStall) + ")";
    }
}

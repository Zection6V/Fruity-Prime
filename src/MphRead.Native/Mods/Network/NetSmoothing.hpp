#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace MphRead::Mods::Network
{
    struct PlayerState;

    // Remote players read off a playout clock held a few frames behind the
    // newest snapshot, rather than snapped to whichever snapshot arrived last.
    class NetSmoothing final
    {
    public:
        NetSmoothing() = delete;

        [[nodiscard]] static bool Enabled() noexcept { return _enabled; }
        static void Enabled(bool value) noexcept { _enabled = value; }

        static constexpr std::int32_t MinDelayFrames = 2;
        static constexpr std::int32_t MaxDelayFrames = 8;

        [[nodiscard]] static std::int32_t Delay() noexcept { return _delay; }

        [[nodiscard]] static std::int64_t Starved() noexcept { return _starved; }
        [[nodiscard]] static std::int64_t Interpolated() noexcept { return _interpolated; }
        [[nodiscard]] static std::int64_t Held() noexcept { return _held; }
        [[nodiscard]] static std::int64_t Snaps() noexcept { return _snaps; }
        [[nodiscard]] static std::int64_t Steps() noexcept { return _steps; }
        [[nodiscard]] static double StepSum() noexcept { return _stepSum; }
        [[nodiscard]] static float WorstStep() noexcept { return _worstStep; }
        [[nodiscard]] static std::int64_t StalledFrames() noexcept { return _stalledFrames; }
        [[nodiscard]] static std::int32_t WorstStall() noexcept { return _worstStall; }

        static void ResetSlot(std::int32_t slot);
        [[nodiscard]] static bool Active();
        static void Record(std::uint32_t frame, std::span<const PlayerState> states);
        static void Tick();
        [[nodiscard]] static bool Sample(std::int32_t slot, OpenTK::Mathematics::Vector3& position, bool& altForm);
        [[nodiscard]] static bool AckPoint(std::uint32_t& frame, std::uint8_t& subFrame);
        static void Reset();
        static void NoteRoomChanged();
        [[nodiscard]] static std::optional<std::string> Describe();

    private:
        static constexpr std::int32_t Slots = 8;
        static constexpr std::int32_t HistoryFrames = 64;
        static constexpr std::int32_t ShrinkAfterFrames = 240;
        static constexpr std::int32_t GrowCooldownFrames = 30;
        static constexpr double Correction = 0.05;
        static constexpr double SnapError = 10.0;
        static constexpr float SnapDistance = 4.0F;

        static void Restart(std::uint32_t frame);
        [[nodiscard]] static bool Lookup(std::int32_t slot, std::uint32_t frame,
            OpenTK::Mathematics::Vector3& position, bool& altForm);
        static void NoteStep(std::int32_t slot, OpenTK::Mathematics::Vector3 position);

        template <typename T>
        using Grid = std::array<std::array<T, HistoryFrames>, Slots>;

        static bool _enabled;
        static std::int32_t _delay;
        static std::int32_t _sinceGrew;
        static double _readFrame;
        static bool _running;
        static std::int32_t _sinceStarved;
        static std::int64_t _starved;
        static std::int64_t _interpolated;
        static std::int64_t _held;
        static std::int64_t _snaps;
        static std::int64_t _steps;
        static double _stepSum;
        static float _worstStep;
        static std::int64_t _stalledFrames;
        static std::int32_t _worstStall;
        static std::array<std::int32_t, Slots> _stallRun;
        // The ring: one stamp per frame covers every slot, because a snapshot
        // carries all of them at once.
        static Grid<std::uint16_t> _life;
        static Grid<std::uint16_t> _generation;
        static Grid<OpenTK::Mathematics::Vector3> _position;
        static Grid<bool> _altForm;
        static Grid<bool> _live;
        static std::array<std::uint32_t, HistoryFrames> _stamp;
        static std::uint32_t _newest;
        static std::array<OpenTK::Mathematics::Vector3, Slots> _lastSampled;
        static std::array<bool, Slots> _sampledSeen;
    };
}

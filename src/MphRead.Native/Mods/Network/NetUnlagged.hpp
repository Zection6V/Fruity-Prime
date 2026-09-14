#pragma once

#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace MphRead::Mods::Network
{
    class NetUnlagged final
    {
    public:
        NetUnlagged() = delete;

        static constexpr std::int32_t HistoryFrames = 64;
        static constexpr std::int32_t MaxRewindFrames = 24;

        [[nodiscard]] static bool Enabled() noexcept { return _enabled; }
        static void SetEnabled(bool value) noexcept { _enabled = value; }

        [[nodiscard]] static std::int64_t ShotsCompensated() noexcept
        {
            return _shotsCompensated;
        }
        [[nodiscard]] static std::int64_t FramesRewound() noexcept
        {
            return _framesRewound;
        }
        [[nodiscard]] static std::int32_t WorstRewind() noexcept
        {
            return _worstRewind;
        }
        [[nodiscard]] static std::int64_t CatchUpSteps() noexcept
        {
            return _catchUpSteps;
        }
        [[nodiscard]] static std::int64_t CatchUpHits() noexcept
        {
            return _catchUpHits;
        }
        [[nodiscard]] static std::int64_t HistoryMisses() noexcept
        {
            return _historyMisses;
        }

        static void Reset();
        static void Record(std::uint32_t frame);
        static void BeginShot(Entities::PlayerEntity& shooter);
        static void Restore();
        static void EndShot(Entities::PlayerEntity& shooter);
        [[nodiscard]] static std::string Describe();

    private:
        static constexpr std::int32_t Slots = Entities::PlayerEntity::SlotCapacity;

        [[nodiscard]] static std::int32_t RewindFor(std::int32_t slot);
        [[nodiscard]] static bool Simulating();
        [[nodiscard]] static bool Reconcile(std::int32_t exceptSlot, std::uint32_t frame);

        inline static bool _enabled = true;

        inline static std::int64_t _shotsCompensated = 0;
        inline static std::int64_t _framesRewound = 0;
        inline static std::int32_t _worstRewind = 0;
        inline static std::int64_t _catchUpSteps = 0;
        inline static std::int64_t _catchUpHits = 0;
        inline static std::int64_t _historyMisses = 0;

        inline static std::array<std::array<OpenTK::Mathematics::Vector3, HistoryFrames>, Slots>
            _position{};
        inline static std::array<std::array<bool, HistoryFrames>, Slots> _altForm{};
        inline static std::array<std::array<bool, HistoryFrames>, Slots> _inPlay{};
        inline static std::array<std::uint32_t, HistoryFrames> _stamp{};
        inline static std::uint32_t _newest = 0;

        inline static std::array<OpenTK::Mathematics::Vector3, Slots> _restore{};
        inline static std::array<bool, Slots> _moved{};
        inline static bool _reconciled = false;

        inline static std::vector<bool> _beamsBefore = std::vector<bool>(16);
        inline static Entities::PlayerEntity* _shooter = nullptr;
        inline static std::int32_t _rewind = 0;
        inline static bool _inProgress = false;
    };
}

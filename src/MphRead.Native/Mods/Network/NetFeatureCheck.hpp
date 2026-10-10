#pragma once


#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    class EntityBase;
}

namespace MphRead::Mods::Network
{
    enum class TestPhase : std::int32_t;

    class NetFeatureCheck final
    {
    public:
        NetFeatureCheck();
        NetFeatureCheck(const NetFeatureCheck&) = delete;
        NetFeatureCheck& operator=(const NetFeatureCheck&) = delete;
        NetFeatureCheck(NetFeatureCheck&&) = delete;
        NetFeatureCheck& operator=(NetFeatureCheck&&) = delete;
        ~NetFeatureCheck();

        void Reset();
        void Observe(MphRead::Scene& scene);
        [[nodiscard]] bool Report(std::int32_t& failures);
        void SampleScoreboard(std::int32_t serverSecond);

    private:
        class Record;

        // One measured feature: how it is read from a record and, for the
        // cross-check, how much of it counts as having happened. The single
        // source of every threshold -- printed with the report, so
        // tools/netcheck/compare-reports.py reads them rather than copies them.
        struct Feature final
        {
            std::string Name{};
            double (*Get)(const Record&) = nullptr;
            // 0: recorded, never checked.
            double Needed = 0;
            std::string Unit{};
            // Either side doing it is enough, and either answer is fine: a
            // diagnostic, never a failure.
            bool Pairwise = false;
            // The hunters it can happen to; null for all.
            bool (*Applies)(MphRead::Hunter) noexcept = nullptr;
        };

        static constexpr float TeleportStep = 9.0F;
        static constexpr std::int32_t LaunchGraceFrames = 30;

        [[nodiscard]] Record& RecordAt(std::int32_t slot);
        [[nodiscard]] const Record& RecordAt(std::int32_t slot) const;
        void IncrementPhase(TestPhase phase);
        void Count(std::span<std::int32_t> counts, Entities::EntityBase* owner);
        [[nodiscard]] std::string Scoreboard(const std::string& prefix) const;
        [[nodiscard]] std::int32_t ReportOne(
            const Record& mine, const Record& other, const std::string& them) const;
        [[nodiscard]] static bool LaysBombs(MphRead::Hunter hunter) noexcept;
        [[nodiscard]] static bool IsWeavel(MphRead::Hunter hunter) noexcept;
        [[nodiscard]] static double Height(const Record& record) noexcept;

        static std::array<Feature, 23> _features;

        std::vector<std::unique_ptr<Record>> _records{};
        std::int32_t _itemSamples = 0;
        std::int64_t _itemTotal = 0;
        std::int32_t _itemsNow = 0;
        std::int32_t _itemsPickedUp = 0;
        std::int32_t _lastItemCount = -1;
        std::vector<std::pair<TestPhase, std::int32_t>> _phaseFrames{};
        std::int32_t _localSlot = 0;
        std::map<std::int32_t, std::string> _boards{};

    public:
        std::map<std::int32_t, std::string>& Boards;
    };
}

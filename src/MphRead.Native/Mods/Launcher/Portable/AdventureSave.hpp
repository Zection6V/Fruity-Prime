#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead
{
    class StorySave;
}

namespace MphRead::Mods::Launcher
{
    class AdventureSave final
    {
    public:
        AdventureSave() = delete;

        static constexpr std::int32_t SlotCount = 3;

        struct SlotInfo
        {
            struct Init
            {
                std::uint8_t Slot = 0;
                bool Used = false;
                std::optional<std::string> Area{};
                std::int32_t Octoliths = 0;
                std::int32_t Health = 0;
                std::int32_t HealthMax = 0;
            };

            const std::uint8_t Slot = 0;
            const bool Used = false;
            const std::optional<std::string> Area{};
            const std::int32_t Octoliths = 0;
            const std::int32_t Health = 0;
            const std::int32_t HealthMax = 0;

            SlotInfo() = default;
            explicit SlotInfo(const Init& init);
            SlotInfo(const SlotInfo&) = default;
            SlotInfo& operator=(const SlotInfo& other);

            [[nodiscard]] std::string Describe() const;
        };

        [[nodiscard]] static SlotInfo Read(std::uint8_t slot);
        [[nodiscard]] static std::vector<SlotInfo> ReadAll();
        [[nodiscard]] static std::string Begin(std::uint8_t slot, bool newGame);
        [[nodiscard]] static std::string StartRoom(std::shared_ptr<MphRead::StorySave> save);

    private:
        static constexpr std::int32_t _newGameRoomId = 45;
        static const std::array<std::string_view, 9> _areaNames;

        [[nodiscard]] static std::string AreaName(std::shared_ptr<MphRead::StorySave> save);
    };
}

#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class PlayerEntity
    {
    public:
        [[nodiscard]] std::int32_t ModScoreColumn1() const;
        [[nodiscard]] std::int32_t ModScoreColumn2() const;

        void ModDrawPingHeader(float posY);
        void ModDrawPingRow(float posY, std::int32_t slot);

    private:
        static constexpr float _pingColumnX = 236.0F;

        [[nodiscard]] static ColorRgba PingColor(std::int32_t ping);
    };
}

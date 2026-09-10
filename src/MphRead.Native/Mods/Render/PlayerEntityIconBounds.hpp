#pragma once

#include <cstdint>
#include <span>

namespace MphRead::Entities
{
    struct IconBounds
    {
        const std::int32_t MinX;
        const std::int32_t MinY;
        const std::int32_t MaxX;
        const std::int32_t MaxY;

        IconBounds();
        IconBounds(std::int32_t minX, std::int32_t minY, std::int32_t maxX, std::int32_t maxY);

        [[nodiscard]] std::int32_t Width() const;
        [[nodiscard]] std::int32_t Height() const;
        [[nodiscard]] float CentreX() const;
        [[nodiscard]] float CentreY() const;
    };

    class PlayerEntity
    {
    public:
        PlayerEntity() = delete;

        static IconBounds ModIconBounds(std::span<const std::uint8_t> data,
            std::int32_t frame, std::int32_t width, std::int32_t height);
    };
}

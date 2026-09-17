#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::Utility
{
    class BinaryWriter;
    class CollisionDataEditor;
}

namespace MphRead::Mods::MapGen
{
    class MapCollisionPacker final
    {
    public:
        [[nodiscard]] static std::vector<std::uint8_t> Pack(
            const std::shared_ptr<const std::vector<
                std::shared_ptr<MphRead::Utility::CollisionDataEditor>>>& data);

        MapCollisionPacker() = delete;
        MapCollisionPacker(const MapCollisionPacker&) = delete;
        MapCollisionPacker& operator=(const MapCollisionPacker&) = delete;

    private:
        static constexpr float CellSize = 4.0F;

        [[nodiscard]] static std::int32_t CellIndex(
            float value, float origin, std::int32_t parts) noexcept;
        static void Align(MphRead::Utility::BinaryWriter& writer);
    };
}

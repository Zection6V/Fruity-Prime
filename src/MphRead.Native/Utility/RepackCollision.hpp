#pragma once

#include "../Mods/MapGen/RepackAccess.hpp"

#include "Repack.hpp"
#include "../Formats/Collision.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Utility
{
    class CollisionDataEditor
    {
    public:
        const std::shared_ptr<std::vector<OpenTK::Mathematics::Vector3>> Points
            = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();
        OpenTK::Mathematics::Vector4 Plane{};
        std::uint16_t LayerMask = 0;
        MphRead::Formats::Collision::CollisionFlags Flags
            = MphRead::Formats::Collision::CollisionFlags::None;

        CollisionDataEditor() = default;
        CollisionDataEditor(const CollisionDataEditor&) = delete;
        CollisionDataEditor& operator=(const CollisionDataEditor&) = delete;
        CollisionDataEditor(CollisionDataEditor&&) = delete;
        CollisionDataEditor& operator=(CollisionDataEditor&&) = delete;

        [[nodiscard]] bool Damaging() const noexcept;
        void Damaging(bool value) noexcept;
        [[nodiscard]] bool Reflect() const noexcept;
        void Reflect(bool value) noexcept;
        [[nodiscard]] bool Players() const noexcept;
        void Players(bool value) noexcept;
        [[nodiscard]] bool Beams() const noexcept;
        void Beams(bool value) noexcept;
        [[nodiscard]] bool Scan() const noexcept;
        void Scan(bool value) noexcept;

        [[nodiscard]] std::int32_t Slipperiness() const noexcept;
        void Slipperiness(std::int32_t value);
        [[nodiscard]] MphRead::Terrain Terrain() const noexcept;
        void Terrain(MphRead::Terrain value);

    private:
        [[nodiscard]] bool Check(
            MphRead::Formats::Collision::CollisionFlags flag) const noexcept;
        void Update(
            MphRead::Formats::Collision::CollisionFlags flag, bool value) noexcept;
    };

    class RepackCollision final
    {
        MPHREAD_REPACK_COLLISION_ACCESS_MEMBERS

    public:
        [[nodiscard]] static std::vector<std::uint8_t> RepackMphRoom(
            const std::string& room);
        [[nodiscard]] static std::vector<std::uint8_t> RepackFhRoom(
            const std::string& room, RepackFilter filter = RepackFilter::All);
        static void TestCollision(std::optional<std::string> room = std::nullopt);

        RepackCollision() = delete;
        RepackCollision(const RepackCollision&) = delete;
        RepackCollision(RepackCollision&&) = delete;
        RepackCollision& operator=(const RepackCollision&) = delete;
        RepackCollision& operator=(RepackCollision&&) = delete;
    };
}

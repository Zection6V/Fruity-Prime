#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace OpenTK::Mathematics
{
    struct Vector4
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };
}

namespace MphRead::Formats::Culling
{
    struct NodeRef
    {
        std::optional<std::string> RoomName{};
        std::int32_t PartIndex = 0;
        std::int32_t NodeIndex = 0;
        std::int32_t ModelIndex = 0;

        static const NodeRef None;

        NodeRef() = default;
        NodeRef(std::optional<std::string> roomName, std::int32_t partIndex,
            std::int32_t nodeIndex, std::int32_t modelIndex);

        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;
    };

    [[nodiscard]] bool operator==(const NodeRef& lhs, const NodeRef& rhs);
    [[nodiscard]] bool operator!=(const NodeRef& lhs, const NodeRef& rhs);

    struct FrustumPlane
    {
        std::int32_t XIndex1 = 0;
        std::int32_t XIndex2 = 0;
        std::int32_t YIndex1 = 0;
        std::int32_t YIndex2 = 0;
        std::int32_t ZIndex1 = 0;
        std::int32_t ZIndex2 = 0;
        OpenTK::Mathematics::Vector4 Plane{};
    };

    class RoomPartVisInfo
    {
    public:
        MphRead::Formats::Culling::NodeRef NodeRef{};
        float ViewMinX = 0.0F;
        float ViewMaxX = 0.0F;
        float ViewMinY = 0.0F;
        float ViewMaxY = 0.0F;
        std::shared_ptr<RoomPartVisInfo> Next{};

        RoomPartVisInfo() = default;
        RoomPartVisInfo(const RoomPartVisInfo&) = delete;
        RoomPartVisInfo& operator=(const RoomPartVisInfo&) = delete;
        RoomPartVisInfo(RoomPartVisInfo&&) = delete;
        RoomPartVisInfo& operator=(RoomPartVisInfo&&) = delete;
    };

    class FrustumInfo
    {
    public:
        std::int32_t Index = 0;
        std::int32_t Count = 0;
        const std::shared_ptr<std::array<FrustumPlane, 10>> Planes;

        FrustumInfo();
        FrustumInfo(const FrustumInfo&) = delete;
        FrustumInfo& operator=(const FrustumInfo&) = delete;
        FrustumInfo(FrustumInfo&&) = delete;
        FrustumInfo& operator=(FrustumInfo&&) = delete;
    };

    class RoomFrustumItem
    {
    public:
        MphRead::Formats::Culling::NodeRef NodeRef{};
        const std::shared_ptr<FrustumInfo> Info;
        std::shared_ptr<RoomFrustumItem> Next{};

        RoomFrustumItem();
        RoomFrustumItem(const RoomFrustumItem&) = delete;
        RoomFrustumItem& operator=(const RoomFrustumItem&) = delete;
        RoomFrustumItem(RoomFrustumItem&&) = delete;
        RoomFrustumItem& operator=(RoomFrustumItem&&) = delete;
    };
}

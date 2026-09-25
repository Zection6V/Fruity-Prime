#include "Culling.hpp"
#include "../NativeRuntime/System/HashCode.hpp"

#include <bit>
#include <random>

using ::MphRead::NativeRuntime::HashCodeCombine;

namespace MphRead::Formats::Culling
{
    constinit const NodeRef NodeRef::None{nullptr, -1, -1, -1};

    NodeRef::NodeRef(NullableString roomName, std::int32_t partIndex,
        std::int32_t nodeIndex, std::int32_t modelIndex) noexcept
        : RoomName(roomName),
          PartIndex(partIndex),
          NodeIndex(nodeIndex),
          ModelIndex(modelIndex)
    {
    }

    bool operator==(const NodeRef& lhs, const NodeRef& rhs)
    {
        return lhs.PartIndex == rhs.PartIndex && lhs.NodeIndex == rhs.NodeIndex
            && lhs.ModelIndex == rhs.ModelIndex;
    }

    bool operator!=(const NodeRef& lhs, const NodeRef& rhs)
    {
        return lhs.PartIndex != rhs.PartIndex || lhs.NodeIndex != rhs.NodeIndex
            || lhs.ModelIndex != rhs.ModelIndex;
    }

    bool NodeRef::Equals(const std::any& obj) const
    {
        const NodeRef* other = std::any_cast<NodeRef>(&obj);
        return other != nullptr && PartIndex == other->PartIndex
            && NodeIndex == other->NodeIndex && ModelIndex == other->ModelIndex;
    }

    std::int32_t NodeRef::GetHashCode() const
    {
        return HashCodeCombine(PartIndex, NodeIndex, ModelIndex);
    }

    FrustumInfo::FrustumInfo()
        : Planes(std::make_shared<std::array<FrustumPlane, 10>>())
    {
    }

    RoomFrustumItem::RoomFrustumItem()
        : NodeRef(MphRead::Formats::Culling::NodeRef::None),
          Info(std::make_shared<FrustumInfo>())
    {
    }
}

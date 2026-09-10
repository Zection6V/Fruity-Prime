#include "Culling.hpp"

#include <bit>
#include <random>

namespace
{
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;

    // .NET 9 HashCode.Combine uses a process-global 32-bit seed obtained from
    // OS random bytes. A separate native process cannot share that hidden seed.
    // The mixing below matches .NET for any given seed; this translation unit
    // supplies its own C++ seed via std::random_device.
    std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queuedValue)
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    std::uint32_t MixFinal(std::uint32_t hash)
    {
        hash ^= hash >> 15;
        hash *= Prime2;
        hash ^= hash >> 13;
        hash *= Prime3;
        hash ^= hash >> 16;
        return hash;
    }

    std::int32_t CombineHashCodes(std::int32_t value1, std::int32_t value2, std::int32_t value3)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 12U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value1));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value2));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value3));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }
}

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
        return CombineHashCodes(PartIndex, NodeIndex, ModelIndex);
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

#include "../../Scene.hpp"

#include "../../Entities/BombEntity.hpp"
#include "../../Formats/Types.hpp"

#include <bit>

namespace MphRead
{
    // Audit only. Read before the next OnDrawFrame returns these Points arrays to the pool.
    std::pair<std::uint64_t, std::int32_t> Scene::ModLockjawTrailSignature()
    {
        constexpr std::uint64_t offset = 14695981039346656037ULL;
        constexpr std::uint64_t prime = 1099511628211ULL;
        std::uint64_t signature = offset;
        std::int32_t trailCount = 0;

        for (const std::shared_ptr<::MphRead::RenderItem>& itemPtr : _usedRenderItems)
        {
            const ::MphRead::RenderItem& item = *itemPtr;
            if (item.Type != RenderItemType::TrailMulti || item.ItemCount != 40)
            {
                continue;
            }
            bool lockjawTrail = false;
            for (auto enumerator = GetBombEntities().GetEnumerator(); enumerator.MoveNext();)
            {
                const Entities::BombEntity& bomb = *enumerator.Current();
                const OpenTK::Mathematics::Vector3 position = bomb.Position;
                if (bomb.Active && bomb.BombType() == BombType::Lockjaw
                    && bomb.BombIndex() > 0 && bomb.ModLockjawTrailBindingId() > 0
                    && item.TextureBindingId == bomb.ModLockjawTrailBindingId()
                    && item.Transform.M41 == position.X
                    && item.Transform.M42 == position.Y
                    && item.Transform.M43 == position.Z)
                {
                    lockjawTrail = true;
                    break;
                }
            }
            if (!lockjawTrail)
            {
                continue;
            }
            trailCount++;
            // ItemCount is the number of Vector3 entries, not the pool array length.
            signature = (signature ^ static_cast<std::uint32_t>(item.ItemCount)) * prime;
            signature = (signature ^ static_cast<std::uint32_t>(item.TextureBindingId)) * prime;
            signature = (signature ^ std::bit_cast<std::uint32_t>(item.Transform.M41)) * prime;
            signature = (signature ^ std::bit_cast<std::uint32_t>(item.Transform.M42)) * prime;
            signature = (signature ^ std::bit_cast<std::uint32_t>(item.Transform.M43)) * prime;
            for (std::int32_t i = 0; i < item.ItemCount; i++)
            {
                const OpenTK::Mathematics::Vector3 point = (*item.Points)[i];
                signature = (signature ^ std::bit_cast<std::uint32_t>(point.X)) * prime;
                signature = (signature ^ std::bit_cast<std::uint32_t>(point.Y)) * prime;
                signature = (signature ^ std::bit_cast<std::uint32_t>(point.Z)) * prime;
            }
        }
        return {signature, trailCount};
    }
}

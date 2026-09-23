#include "RepackAccess.hpp"

#include "../../Utility/Repack.hpp"
#include "../../Utility/RepackCollision.hpp"

namespace MphRead::Utility
{
    std::vector<std::uint8_t> Repack::PackEntities(std::span<Editor::EntityEditorBase* const> entities)
    {
        return RepackEntitiesFrom(entities);
    }

    std::vector<std::uint8_t> RepackCollision::PackMphCollision(
        std::span<CollisionDataEditor* const> data,
        std::span<Formats::Collision::Portal* const> portals)
    {
        return RepackMphCollisionFrom(data, portals);
    }
}

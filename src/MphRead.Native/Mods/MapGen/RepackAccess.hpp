#pragma once

// RepackAccess.cs is a partial of Repack and of RepackCollision. These are the
// members it contributes; the canonical headers expand them.

#include <cstdint>
#include <span>
#include <vector>

namespace MphRead::Editor
{
    class EntityEditorBase;
}

namespace MphRead::Formats::Collision
{
    class Portal;
}

namespace MphRead::Utility
{
    class CollisionDataEditor;
}

#define MPHREAD_REPACK_ACCESS_MEMBERS                                       \
public:                                                                     \
    static std::vector<std::uint8_t> PackEntities(                          \
        std::span<::MphRead::Editor::EntityEditorBase* const> entities);    \
                                                                            \
private:                                                                    \
    static std::vector<std::uint8_t> RepackEntitiesFrom(                    \
        std::span<::MphRead::Editor::EntityEditorBase* const> entities);

#define MPHREAD_REPACK_COLLISION_ACCESS_MEMBERS                             \
public:                                                                     \
    static std::vector<std::uint8_t> PackMphCollision(                      \
        std::span<::MphRead::Utility::CollisionDataEditor* const> data,     \
        std::span<::MphRead::Formats::Collision::Portal* const> portals);   \
                                                                            \
private:                                                                    \
    static std::vector<std::uint8_t> RepackMphCollisionFrom(                \
        std::span<::MphRead::Utility::CollisionDataEditor* const> data,     \
        std::span<::MphRead::Formats::Collision::Portal* const> portals);

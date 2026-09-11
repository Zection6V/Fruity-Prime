#pragma once

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

    class Repack final
    {
    public:
        static std::vector<std::uint8_t> PackEntities(std::span<Editor::EntityEditorBase* const> entities);

    private:
        Repack() = delete;

        static std::vector<std::uint8_t> RepackEntities(std::span<Editor::EntityEditorBase* const> entities);
    };

    class RepackCollision final
    {
    public:
        static std::vector<std::uint8_t> PackMphCollision(
            std::span<CollisionDataEditor* const> data,
            std::span<Formats::Collision::Portal* const> portals);

    private:
        RepackCollision() = delete;

        static std::vector<std::uint8_t> RepackMphCollision(
            std::span<CollisionDataEditor* const> data,
            std::span<Formats::Collision::Portal* const> portals);
    };
}

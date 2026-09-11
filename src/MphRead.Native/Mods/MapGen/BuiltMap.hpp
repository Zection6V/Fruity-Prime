#pragma once

#include "../../Formats/Enums.hpp"

#include <cstdint>
#include <vector>

namespace OpenTK::Mathematics
{
    struct Vector2;
    struct Vector3;
}

namespace MphRead
{
    namespace Editor
    {
        class EntityEditorBase;
    }

    namespace Interop
    {
        template <typename T>
        class ManagedArray;
    }
}

namespace MphRead::Mods::MapGen
{
    class MapDefinition;
    class BuiltFace;

    class BuiltMap
    {
    public:
        explicit BuiltMap(MapDefinition* definition) noexcept;

        BuiltMap(const BuiltMap&) = delete;
        BuiltMap& operator=(const BuiltMap&) = delete;
        BuiltMap(BuiltMap&&) = delete;
        BuiltMap& operator=(BuiltMap&&) = delete;

        [[nodiscard]] MapDefinition* Definition() const noexcept;
        [[nodiscard]] std::vector<BuiltFace*>& Faces() noexcept;
        [[nodiscard]] std::vector<BuiltFace*>& Solid() noexcept;
        [[nodiscard]] std::vector<Editor::EntityEditorBase*>& Entities() noexcept;

    private:
        MapDefinition* const _definition;
        std::vector<BuiltFace*> _faces{};
        std::vector<BuiltFace*> _solid{};
        std::vector<Editor::EntityEditorBase*> _entities{};
    };

    class BuiltFace
    {
    public:
        BuiltFace(
            Interop::ManagedArray<OpenTK::Mathematics::Vector3>* points,
            Interop::ManagedArray<OpenTK::Mathematics::Vector2>* texcoords,
            OpenTK::Mathematics::Vector3 normal,
            std::int32_t material,
            float shade) noexcept;

        BuiltFace(const BuiltFace&) = delete;
        BuiltFace& operator=(const BuiltFace&) = delete;
        BuiltFace(BuiltFace&&) = delete;
        BuiltFace& operator=(BuiltFace&&) = delete;

        [[nodiscard]] Interop::ManagedArray<OpenTK::Mathematics::Vector3>* Points() const noexcept;
        [[nodiscard]] Interop::ManagedArray<OpenTK::Mathematics::Vector2>* Texcoords() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector3 Normal() const noexcept;
        [[nodiscard]] std::int32_t Material() const noexcept;
        [[nodiscard]] float Shade() const noexcept;
        [[nodiscard]] bool Damaging() const noexcept;
        void Damaging(bool value) noexcept;
        [[nodiscard]] MphRead::Terrain Terrain() const noexcept;
        void Terrain(MphRead::Terrain value) noexcept;

    private:
        Interop::ManagedArray<OpenTK::Mathematics::Vector3>* const _points;
        Interop::ManagedArray<OpenTK::Mathematics::Vector2>* const _texcoords;
        const float _normalX;
        const float _normalY;
        const float _normalZ;
        const std::int32_t _material;
        const float _shade;
        bool _damaging = false;
        MphRead::Terrain _terrain = MphRead::Terrain::Metal;
    };
}

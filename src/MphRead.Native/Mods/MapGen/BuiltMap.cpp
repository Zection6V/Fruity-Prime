#include "BuiltMap.hpp"

#include "../Render/PreviewCamera.hpp"

namespace MphRead::Mods::MapGen
{
    BuiltMap::BuiltMap(MapDefinition* definition) noexcept
        : _definition(definition)
    {
    }

    MapDefinition* BuiltMap::Definition() const noexcept
    {
        return _definition;
    }

    std::vector<BuiltFace*>& BuiltMap::Faces() noexcept
    {
        return _faces;
    }

    std::vector<BuiltFace*>& BuiltMap::Solid() noexcept
    {
        return _solid;
    }

    std::vector<Editor::EntityEditorBase*>& BuiltMap::Entities() noexcept
    {
        return _entities;
    }

    BuiltFace::BuiltFace(
        Interop::ManagedArray<OpenTK::Mathematics::Vector3>* points,
        Interop::ManagedArray<OpenTK::Mathematics::Vector2>* texcoords,
        OpenTK::Mathematics::Vector3 normal,
        std::int32_t material,
        float shade) noexcept
        : _points(points),
          _texcoords(texcoords),
          _normalX(normal.X),
          _normalY(normal.Y),
          _normalZ(normal.Z),
          _material(material),
          _shade(shade)
    {
    }

    Interop::ManagedArray<OpenTK::Mathematics::Vector3>* BuiltFace::Points() const noexcept
    {
        return _points;
    }

    Interop::ManagedArray<OpenTK::Mathematics::Vector2>* BuiltFace::Texcoords() const noexcept
    {
        return _texcoords;
    }

    OpenTK::Mathematics::Vector3 BuiltFace::Normal() const noexcept
    {
        return {_normalX, _normalY, _normalZ};
    }

    std::int32_t BuiltFace::Material() const noexcept
    {
        return _material;
    }

    float BuiltFace::Shade() const noexcept
    {
        return _shade;
    }

    bool BuiltFace::Damaging() const noexcept
    {
        return _damaging;
    }

    void BuiltFace::Damaging(bool value) noexcept
    {
        _damaging = value;
    }

    MphRead::Terrain BuiltFace::Terrain() const noexcept
    {
        return _terrain;
    }

    void BuiltFace::Terrain(MphRead::Terrain value) noexcept
    {
        _terrain = value;
    }
}

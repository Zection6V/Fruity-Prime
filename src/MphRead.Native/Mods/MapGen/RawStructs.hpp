#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace MphRead
{
    class Material;
    class Mesh;
    class Node;
}

namespace MphRead::Mods::MapGen
{
    class RawStructs final
    {
    public:
        [[nodiscard]] static std::shared_ptr<Node> MakeNode(
            const std::string& name,
            std::int32_t meshCount,
            std::int32_t firstMeshId,
            std::int32_t parent = -1,
            std::int32_t child = -1,
            std::int32_t next = -1);

        [[nodiscard]] static std::shared_ptr<Material> MakeMaterial(
            const std::string& name,
            std::int32_t textureId,
            std::int32_t paletteId,
            RepeatMode xRepeat,
            RepeatMode yRepeat,
            bool lighting,
            ColorRgb diffuse,
            ColorRgb ambient);

        [[nodiscard]] static std::shared_ptr<Mesh> MakeMesh(
            std::int32_t materialId,
            std::int32_t dlistId);

        RawStructs() = delete;
        RawStructs(const RawStructs&) = delete;
        RawStructs& operator=(const RawStructs&) = delete;
    };
}

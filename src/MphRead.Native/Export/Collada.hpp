#pragma once

#include "../Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class Model;
}

namespace MphRead::Export
{
    class Collada final
    {
    public:
        struct Vertex final
        {
            const OpenTK::Mathematics::Vector3 Position{};
            const OpenTK::Mathematics::Vector3 Normal{};
            const OpenTK::Mathematics::Vector3 Color{};
            const OpenTK::Mathematics::Vector2 Uv{};
            const std::int32_t MatrixId = 0;

            constexpr Vertex() noexcept = default;
            constexpr Vertex(OpenTK::Mathematics::Vector3 position,
                OpenTK::Mathematics::Vector3 normal,
                OpenTK::Mathematics::Vector3 color,
                OpenTK::Mathematics::Vector2 uv,
                std::int32_t matrixId) noexcept
                : Position(position), Normal(normal), Color(color), Uv(uv), MatrixId(matrixId)
            {
            }

            Vertex(const Vertex&) noexcept = default;

            Vertex& operator=(const Vertex& other) noexcept
            {
                if (this != &other)
                {
                    this->~Vertex();
                    ::new (static_cast<void*>(this)) Vertex(other);
                }
                return *this;
            }
        };

        static void ExportModel(const Model& model, bool transformRoom = false);

        Collada() = delete;
        Collada(const Collada&) = delete;
        Collada& operator=(const Collada&) = delete;
        Collada(Collada&&) = delete;
        Collada& operator=(Collada&&) = delete;

    private:
        using VertexList = std::vector<Vertex>;
        using VertexDictionary = std::vector<std::pair<std::string, VertexList>>;

        static std::string FloatFormat(OpenTK::Mathematics::Vector3 vector);
        static std::string FloatFormat(float input);
        static VertexDictionary ExportRecolor(const Model& model, bool transformRoom, std::int32_t recolorIndex);
        static void ExportRoomNodes(const Model& model, std::int32_t parentId, std::string& output,
            std::int32_t indent, bool transformRoom);
        static void ExportNodeMeshes(const Model& model, std::int32_t nodeId, std::string& output, std::int32_t indent);
        static void ExportMeshes(const Model& model, std::string& output, std::int32_t indent);
        static void ExportMesh(const Model& model, std::int32_t meshId, std::string& output, std::int32_t indent);
        static void ExportDlist(const Model& model, std::int32_t dlistId, VertexList& meshVerts, VertexList& tempMeshVerts);
        static Vertex GetCurrentExportTri(const std::array<float, 3>& vtxState,
            const std::array<float, 3>& nrmState,
            const std::array<float, 2>& uvState,
            const std::array<float, 3>& colState,
            std::int32_t mtxState);
    };
}

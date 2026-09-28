#pragma once

#include "RendererGeometry.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace MphRead
{
    struct GpuMeshDrawRange final
    {
        ScenePrimitiveTopology Topology = ScenePrimitiveTopology::Triangles;
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
        std::size_t IndexByteOffset = 0;
    };

    struct GpuMeshDrawPlan final
    {
        std::uint32_t IndexCount = 0;
        std::vector<GpuMeshDrawRange> Ranges{};
    };

    [[nodiscard]] GpuMeshDrawPlan BuildGpuMeshDrawPlan(const RendererGeometry& geometry);

    class GpuMeshResource
    {
    public:
        virtual ~GpuMeshResource() = default;
        GpuMeshResource(const GpuMeshResource&) = delete;
        GpuMeshResource& operator=(const GpuMeshResource&) = delete;
        GpuMeshResource(GpuMeshResource&&) = delete;
        GpuMeshResource& operator=(GpuMeshResource&&) = delete;

        virtual void Draw() = 0;

    protected:
        GpuMeshResource() = default;
    };

    // Mesh::DlistId is the source render-instruction-list identity. The legacy
    // renderer already shared one compiled geometry object among meshes with the
    // same DlistId inside one Model, so live Model identity + DlistId preserves
    // that exact ownership identity without putting a backend handle in Model/Mesh.
    class GpuMeshCache final
    {
    public:
        using Factory = std::function<std::shared_ptr<GpuMeshResource>()>;

        [[nodiscard]] std::shared_ptr<GpuMeshResource> Find(
            const void* modelIdentity, std::int32_t meshIdentity);
        [[nodiscard]] std::shared_ptr<GpuMeshResource> GetOrCreate(
            const std::shared_ptr<const void>& modelLifetime,
            std::int32_t meshIdentity, const Factory& factory);

        void EraseModel(const void* modelIdentity);
        void PruneExpired();
        void Clear() noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        struct Key final
        {
            const void* ModelIdentity = nullptr;
            std::int32_t MeshIdentity = 0;

            bool operator==(const Key&) const noexcept = default;
        };

        struct KeyHash final
        {
            [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
        };

        struct Entry final
        {
            std::weak_ptr<const void> ModelLifetime{};
            std::shared_ptr<GpuMeshResource> Resource{};
        };

        std::unordered_map<Key, Entry, KeyHash> _entries{};
    };
}

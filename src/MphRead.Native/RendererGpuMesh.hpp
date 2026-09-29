#pragma once

#include "RendererGeometry.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
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

    // DlistId remains only the source render-instruction-list selector. GPU
    // ownership is keyed by the live Model object and the live Mesh object so
    // distinct meshes never alias merely because they reference the same list.
    enum class TransientPrimitiveTopology : std::uint8_t
    {
        LineLoop,
        Triangles,
        TriangleStrip,
        TriangleFan,
        Quads,
        QuadStrip
    };

    struct TransientVertex final
    {
        OpenTK::Mathematics::Vector3 Position{};
        OpenTK::Mathematics::Vector3 TexCoord{};
    };

    // Dynamic geometry preserves the caller's submitted vertex order for every
    // supported transient topology. Fill the scene-owned IBO with that stable
    // sequence without exposing backend buffer names to renderer/domain types.
    void BuildTransientIndexSequence(std::span<std::uint32_t> indices);

    class TransientGeometryResource
    {
    public:
        virtual ~TransientGeometryResource() = default;
        TransientGeometryResource(const TransientGeometryResource&) = delete;
        TransientGeometryResource& operator=(const TransientGeometryResource&) = delete;
        TransientGeometryResource(TransientGeometryResource&&) = delete;
        TransientGeometryResource& operator=(TransientGeometryResource&&) = delete;

        virtual void BeginFrame() = 0;
        virtual void Draw(TransientPrimitiveTopology topology,
            std::span<const TransientVertex> vertices, bool hasTexCoords) = 0;

    protected:
        TransientGeometryResource() = default;
    };

    class GpuMeshCache final
    {
    public:
        using Factory = std::function<std::shared_ptr<GpuMeshResource>()>;

        [[nodiscard]] std::shared_ptr<GpuMeshResource> Find(
            const void* modelIdentity, const void* meshIdentity);
        [[nodiscard]] std::shared_ptr<GpuMeshResource> GetOrCreate(
            const std::shared_ptr<const void>& modelLifetime,
            const std::shared_ptr<const void>& meshLifetime,
            const Factory& factory);

        void EraseModel(const void* modelIdentity);
        void PruneExpired();
        void Clear() noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        struct Key final
        {
            const void* ModelIdentity = nullptr;
            const void* MeshIdentity = nullptr;

            bool operator==(const Key&) const noexcept = default;
        };

        struct KeyHash final
        {
            [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
        };

        struct Entry final
        {
            std::weak_ptr<const void> ModelLifetime{};
            std::weak_ptr<const void> MeshLifetime{};
            std::shared_ptr<GpuMeshResource> Resource{};
        };

        std::unordered_map<Key, Entry, KeyHash> _entries{};
    };
}

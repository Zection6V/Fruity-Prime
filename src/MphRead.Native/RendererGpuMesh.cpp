#include "RendererGpuMesh.hpp"

#include <limits>
#include <stdexcept>

namespace MphRead
{
    GpuMeshDrawPlan BuildGpuMeshDrawPlan(const RendererGeometry& geometry)
    {
        if (geometry.Indices.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error("GPU mesh index count exceeds uint32_t.");
        }

        GpuMeshDrawPlan plan{};
        plan.IndexCount = static_cast<std::uint32_t>(geometry.Indices.size());
        plan.Ranges.reserve(geometry.Ranges.size());
        for (const ScenePrimitiveRange& range : geometry.Ranges)
        {
            const std::uint64_t end = static_cast<std::uint64_t>(range.FirstIndex)
                + static_cast<std::uint64_t>(range.IndexCount);
            if (end > geometry.Indices.size())
            {
                throw std::out_of_range("GPU mesh primitive range exceeds the index buffer.");
            }
            plan.Ranges.push_back(GpuMeshDrawRange{
                range.Topology,
                range.FirstIndex,
                range.IndexCount,
                static_cast<std::size_t>(range.FirstIndex) * sizeof(std::uint32_t)
            });
        }
        return plan;
    }

    std::size_t GpuMeshCache::KeyHash::operator()(const Key& key) const noexcept
    {
        const std::size_t modelHash = std::hash<const void*>{}(key.ModelIdentity);
        const std::size_t meshHash = std::hash<std::int32_t>{}(key.MeshIdentity);
        return modelHash ^ (meshHash + static_cast<std::size_t>(0x9e3779b9U)
            + (modelHash << 6U) + (modelHash >> 2U));
    }

    std::shared_ptr<GpuMeshResource> GpuMeshCache::Find(
        const void* modelIdentity, std::int32_t meshIdentity)
    {
        const Key key{modelIdentity, meshIdentity};
        const auto found = _entries.find(key);
        if (found == _entries.end())
        {
            return {};
        }
        if (found->second.ModelLifetime.expired())
        {
            _entries.erase(found);
            return {};
        }
        return found->second.Resource;
    }

    std::shared_ptr<GpuMeshResource> GpuMeshCache::GetOrCreate(
        const std::shared_ptr<const void>& modelLifetime,
        std::int32_t meshIdentity, const Factory& factory)
    {
        if (!modelLifetime)
        {
            throw std::invalid_argument("GPU mesh cache requires a live model identity.");
        }
        if (!factory)
        {
            throw std::invalid_argument("GPU mesh cache requires a resource factory.");
        }

        const Key key{modelLifetime.get(), meshIdentity};
        if (const std::shared_ptr<GpuMeshResource> existing = Find(key.ModelIdentity, meshIdentity))
        {
            return existing;
        }

        std::shared_ptr<GpuMeshResource> resource = factory();
        if (!resource)
        {
            throw std::runtime_error("GPU mesh factory returned no resource.");
        }
        _entries.insert_or_assign(key, Entry{modelLifetime, resource});
        return resource;
    }

    void GpuMeshCache::EraseModel(const void* modelIdentity)
    {
        for (auto it = _entries.begin(); it != _entries.end();)
        {
            if (it->first.ModelIdentity == modelIdentity)
            {
                it = _entries.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void GpuMeshCache::PruneExpired()
    {
        for (auto it = _entries.begin(); it != _entries.end();)
        {
            if (it->second.ModelLifetime.expired())
            {
                it = _entries.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void GpuMeshCache::Clear() noexcept
    {
        _entries.clear();
    }

    std::size_t GpuMeshCache::Size() const noexcept
    {
        return _entries.size();
    }
}

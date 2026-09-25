#pragma once

#include <cstdint>

namespace MphRead::Mods::Multiplayer
{
    class ResourceAudit final
    {
    public:
        ResourceAudit() = delete;
        ~ResourceAudit() = delete;
        ResourceAudit(const ResourceAudit&) = delete;
        ResourceAudit& operator=(const ResourceAudit&) = delete;
        ResourceAudit(ResourceAudit&&) = delete;
        ResourceAudit& operator=(ResourceAudit&&) = delete;

        // Asset-backed metadata checks. Distances are straight lines, not path
        // lengths: reachability and dominant routes still require playtesting.
        [[nodiscard]] static std::int32_t Run();
    };
}

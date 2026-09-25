#pragma once

#include <cstdint>
#include <utility>

// Scene members contributed by the LockjawTrailProbe.cs partial. Audit only.
#define MPHREAD_SCENE_LOCKJAW_TRAIL_PROBE_MEMBERS \
public: \
    [[nodiscard]] std::pair<std::uint64_t, std::int32_t> ModLockjawTrailSignature();

#pragma once

// Native counterpart of the shared helpers on EnemyInstanceEntity.
//
// RotateVector and SeekTargetVector are how an enemy turns toward something:
// it rotates by a fixed angle per step and stops when the remaining angle is
// smaller than one step, snapping to the target rather than overshooting and
// oscillating.  The step budget is spent as it turns, so an enemy that runs
// out stops partway rather than snapping.

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>

namespace fruityprime::entities {

// EnemyInstanceEntity.RotateVector: rotate about an arbitrary axis by an
// angle in degrees.
[[nodiscard]] formats::Vector3 rotate_vector(formats::Vector3 value,
                                             formats::Vector3 axis,
                                             float degrees) noexcept;

// EnemyInstanceEntity.SeekTargetVector.  Returns true once `current` has
// reached `target`; `steps` is decremented for each rotation actually made.
[[nodiscard]] bool seek_target_vector(formats::Vector3 target,
                                      formats::Vector3& current,
                                      formats::Vector3 axis,
                                      std::uint16_t& steps,
                                      float degrees) noexcept;

// EnemyInstanceEntity.FixParallelVectors: an up vector parallel to the facing
// would put NaNs in the transform, so a perpendicular one is substituted.
[[nodiscard]] formats::Vector3 fix_parallel_vectors(
    formats::Vector3 facing, formats::Vector3 up) noexcept;

// EnemyInstanceEntity.HitPlayers / ClearHitPlayers: which players this
// enemy's attack has already hit this swing, so one swing cannot hit twice.
struct EnemyHitPlayers {
    std::array<bool, 4> flags{};

    void clear() noexcept { flags.fill(false); }
    [[nodiscard]] bool operator[](std::size_t slot) const noexcept {
        return slot < flags.size() && flags[slot];
    }
    void set(std::size_t slot, bool value) noexcept {
        if (slot < flags.size()) {
            flags[slot] = value;
        }
    }
};

} // namespace fruityprime::entities

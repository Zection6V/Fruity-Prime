#pragma once

// System.Buffers.ArrayPool<Vector3>.Shared: the members the game calls.

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::NativeRuntime
{
    // ArrayPool<Vector3>.Shared.Rent(minimumLength).
    [[nodiscard]] std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>> RentFromSharedArrayPool(
        std::int32_t minimumLength);
    // ArrayPool<Vector3>.Shared.Return(array).
    void ReturnToSharedArrayPool(
        const std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>>& array);
}

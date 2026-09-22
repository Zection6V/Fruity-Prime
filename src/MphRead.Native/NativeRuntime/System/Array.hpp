#pragma once

// A C# bool[]: fixed length, null-able reference, bounds-checked indexing.

#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::NativeRuntime
{
    class ManagedBoolArray final
    {
    public:
        explicit ManagedBoolArray(std::int32_t length);

        [[nodiscard]] std::int32_t Length() const noexcept;
        [[nodiscard]] bool Get(std::int32_t index) const;
        void Set(std::int32_t index, bool value);

    private:
        std::vector<std::uint8_t> _values;
    };

    // new bool[length].
    [[nodiscard]] std::shared_ptr<ManagedBoolArray> CreateManagedBoolArray(
        std::int32_t length);
    // array.Length.
    [[nodiscard]] std::int32_t ManagedBoolArrayLength(
        const std::shared_ptr<const ManagedBoolArray>& array);
    // array[index].
    [[nodiscard]] bool ManagedBoolArrayGet(
        const std::shared_ptr<const ManagedBoolArray>& array, std::int32_t index);
    // array[index] = value.
    void ManagedBoolArraySet(
        const std::shared_ptr<ManagedBoolArray>& array,
        std::int32_t index,
        bool value);
}

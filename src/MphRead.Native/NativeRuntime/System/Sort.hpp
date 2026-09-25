#pragma once

// Array.Sort / List<T>.Sort / Span<T>.Sort: .NET's ArraySortHelper introsort,
// so that elements comparing equal land in the order .NET leaves them in
// (it is not stable, and std::sort's instability is a different one).

#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime
{
    namespace SortDetail
    {
        template <typename T, typename Compare>
        void SwapIfGreater(std::span<T> keys, Compare& compare, std::size_t i, std::size_t j)
        {
            if (i != j && compare(keys[i], keys[j]) > 0)
            {
                std::swap(keys[i], keys[j]);
            }
        }

        template <typename T, typename Compare>
        void InsertionSort(std::span<T> keys, Compare& compare)
        {
            for (std::size_t i = 0; i + 1 < keys.size(); ++i)
            {
                T value = std::move(keys[i + 1]);
                std::size_t j = i + 1;
                while (j > 0 && compare(value, keys[j - 1]) < 0)
                {
                    keys[j] = std::move(keys[j - 1]);
                    --j;
                }
                keys[j] = std::move(value);
            }
        }

        template <typename T, typename Compare>
        void DownHeap(std::span<T> keys, std::size_t i, std::size_t n, Compare& compare)
        {
            T value = std::move(keys[i - 1]);
            while (i <= n / 2)
            {
                std::size_t child = 2 * i;
                if (child < n && compare(keys[child - 1], keys[child]) < 0)
                {
                    ++child;
                }
                if (!(compare(value, keys[child - 1]) < 0))
                {
                    break;
                }
                keys[i - 1] = std::move(keys[child - 1]);
                i = child;
            }
            keys[i - 1] = std::move(value);
        }

        template <typename T, typename Compare>
        void HeapSort(std::span<T> keys, Compare& compare)
        {
            const std::size_t n = keys.size();
            for (std::size_t i = n / 2; i >= 1; --i)
            {
                DownHeap(keys, i, n, compare);
            }
            for (std::size_t i = n; i > 1; --i)
            {
                std::swap(keys[0], keys[i - 1]);
                DownHeap(keys, 1, i - 1, compare);
            }
        }

        template <typename T, typename Compare>
        std::size_t PickPivotAndPartition(std::span<T> keys, Compare& compare)
        {
            const std::size_t hi = keys.size() - 1;
            const std::size_t middle = hi >> 1;
            SwapIfGreater(keys, compare, 0, middle);
            SwapIfGreater(keys, compare, 0, hi);
            SwapIfGreater(keys, compare, middle, hi);

            T pivot = keys[middle];
            std::swap(keys[middle], keys[hi - 1]);
            std::size_t left = 0;
            std::size_t right = hi - 1;
            while (left < right)
            {
                while (compare(keys[++left], pivot) < 0)
                {
                }
                while (compare(pivot, keys[--right]) < 0)
                {
                }
                if (left >= right)
                {
                    break;
                }
                std::swap(keys[left], keys[right]);
            }
            if (left != hi - 1)
            {
                std::swap(keys[left], keys[hi - 1]);
            }
            return left;
        }

        template <typename T, typename Compare>
        void IntroSort(std::span<T> keys, int depthLimit, Compare& compare)
        {
            constexpr std::size_t IntrosortSizeThreshold = 16;
            std::size_t partitionSize = keys.size();
            while (partitionSize > 1)
            {
                if (partitionSize <= IntrosortSizeThreshold)
                {
                    if (partitionSize == 2)
                    {
                        SwapIfGreater(keys, compare, 0, 1);
                        return;
                    }
                    if (partitionSize == 3)
                    {
                        SwapIfGreater(keys, compare, 0, 1);
                        SwapIfGreater(keys, compare, 0, 2);
                        SwapIfGreater(keys, compare, 1, 2);
                        return;
                    }
                    InsertionSort(keys.first(partitionSize), compare);
                    return;
                }
                if (depthLimit == 0)
                {
                    HeapSort(keys.first(partitionSize), compare);
                    return;
                }
                --depthLimit;
                const std::size_t pivot = PickPivotAndPartition(keys.first(partitionSize), compare);
                IntroSort(keys.subspan(pivot + 1, partitionSize - (pivot + 1)), depthLimit, compare);
                partitionSize = pivot;
            }
        }
    }

    // Span<T>.Sort(Comparison<T>): `compare` returns <0, 0 or >0.
    template <typename T, typename Compare>
    void ManagedSort(std::span<T> keys, Compare compare)
    {
        if (keys.size() <= 1)
        {
            return;
        }
        const int depthLimit = 2 * static_cast<int>(std::bit_width(keys.size()));
        SortDetail::IntroSort(keys, depthLimit, compare);
    }

    template <typename T, typename Compare>
    void ManagedSort(std::vector<T>& keys, Compare compare)
    {
        ManagedSort(std::span<T>(keys), std::move(compare));
    }

    // Array.Sort(T[]) for float and double: NaNs are moved to the front
    // first, as .NET does, and the rest sorted by `<`.
    template <typename T>
        requires std::is_floating_point_v<T>
    void ManagedSort(std::span<T> keys)
    {
        std::size_t nanLeft = 0;
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            if (std::isnan(keys[i]))
            {
                std::swap(keys[nanLeft], keys[i]);
                ++nanLeft;
            }
        }
        ManagedSort(keys.subspan(nanLeft),
            [](T a, T b) noexcept { return a < b ? -1 : (a > b ? 1 : 0); });
    }

    template <typename T>
        requires std::is_floating_point_v<T>
    void ManagedSort(std::vector<T>& keys)
    {
        ManagedSort(std::span<T>(keys));
    }
}

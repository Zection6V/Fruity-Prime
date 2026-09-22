#include "Buffers.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        using Vector3Array = ManagedArray<OpenTK::Mathematics::Vector3>;
        using ArrayRef = std::shared_ptr<Vector3Array>;

        // TlsOverPerCoreLockedStacksArrayPool<T>: 27 buckets of 16 << index
        // elements, a thread-local array per bucket, then per-core stacks of 8.
        constexpr std::int32_t NumBuckets = 27;
        constexpr std::size_t MaxBuffersPerArraySizePerCore = 8;

        [[nodiscard]] std::int32_t SelectBucketIndex(std::int32_t bufferSize) noexcept
        {
            const std::uint32_t value = (static_cast<std::uint32_t>(bufferSize) - 1U) | 15U;
            return static_cast<std::int32_t>(std::bit_width(value)) - 1 - 3;
        }

        [[nodiscard]] std::int32_t GetMaxSizeForBucket(std::int32_t binIndex) noexcept
        {
            return 16 << binIndex;
        }

        class LockedStack final
        {
        public:
            [[nodiscard]] bool TryPush(ArrayRef array)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_arrays.size() >= MaxBuffersPerArraySizePerCore)
                {
                    return false;
                }
                _arrays.push_back(std::move(array));
                return true;
            }

            [[nodiscard]] ArrayRef TryPop()
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_arrays.empty())
                {
                    return nullptr;
                }
                ArrayRef array = std::move(_arrays.back());
                _arrays.pop_back();
                return array;
            }

        private:
            std::mutex _mutex;
            std::vector<ArrayRef> _arrays;
        };

        class PerCoreLockedStacks final
        {
        public:
            PerCoreLockedStacks()
                : _stacks(std::max(1U, std::thread::hardware_concurrency()))
            {
            }

            void TryPush(ArrayRef array)
            {
                const std::size_t start = CurrentCore();
                for (std::size_t i = 0; i < _stacks.size(); i++)
                {
                    if (_stacks[(start + i) % _stacks.size()].TryPush(array))
                    {
                        return;
                    }
                }
            }

            [[nodiscard]] ArrayRef TryPop()
            {
                const std::size_t start = CurrentCore();
                for (std::size_t i = 0; i < _stacks.size(); i++)
                {
                    if (ArrayRef array = _stacks[(start + i) % _stacks.size()].TryPop())
                    {
                        return array;
                    }
                }
                return nullptr;
            }

        private:
            [[nodiscard]] std::size_t CurrentCore() const noexcept
            {
#if defined(_WIN32)
                return static_cast<std::size_t>(::GetCurrentProcessorNumber()) % _stacks.size();
#elif defined(__linux__)
                const int cpu = ::sched_getcpu();
                return cpu < 0 ? 0 : static_cast<std::size_t>(cpu) % _stacks.size();
#else
                return 0;
#endif
            }

            std::vector<LockedStack> _stacks;
        };

        std::array<PerCoreLockedStacks, NumBuckets>& Buckets()
        {
            static std::array<PerCoreLockedStacks, NumBuckets> buckets;
            return buckets;
        }

        std::array<ArrayRef, NumBuckets>& ThreadLocalBuckets()
        {
            thread_local std::array<ArrayRef, NumBuckets> buckets;
            return buckets;
        }
    }

    std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>> RentFromSharedArrayPool(
        std::int32_t minimumLength)
    {
        if (minimumLength < 0)
        {
            throw System::ArgumentOutOfRangeException("minimumLength");
        }
        if (minimumLength == 0)
        {
            return Vector3Array::Empty();
        }
        const std::int32_t bucketIndex = SelectBucketIndex(minimumLength);
        if (bucketIndex < NumBuckets)
        {
            ArrayRef& local = ThreadLocalBuckets()[static_cast<std::size_t>(bucketIndex)];
            if (local)
            {
                return std::exchange(local, nullptr);
            }
            if (ArrayRef pooled = Buckets()[static_cast<std::size_t>(bucketIndex)].TryPop())
            {
                return pooled;
            }
            return std::make_shared<Vector3Array>(
                static_cast<std::size_t>(GetMaxSizeForBucket(bucketIndex)));
        }
        return std::make_shared<Vector3Array>(static_cast<std::size_t>(minimumLength));
    }

    void ReturnToSharedArrayPool(
        const std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>>& array)
    {
        if (!array)
        {
            throw System::ArgumentNullException("array");
        }
        const std::int32_t length = static_cast<std::int32_t>(array->Length());
        const std::int32_t bucketIndex = SelectBucketIndex(length);
        if (bucketIndex >= NumBuckets)
        {
            return;
        }
        if (length != GetMaxSizeForBucket(bucketIndex))
        {
            throw System::ArgumentException(
                "The buffer is not associated with this pool and may not be returned to it. (Parameter 'array')");
        }
        ArrayRef& local = ThreadLocalBuckets()[static_cast<std::size_t>(bucketIndex)];
        if (ArrayRef previous = std::exchange(local, array))
        {
            Buckets()[static_cast<std::size_t>(bucketIndex)].TryPush(std::move(previous));
        }
    }
}

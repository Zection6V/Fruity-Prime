#pragma once

// A reference field read and written from more than one thread -- a C# field
// holding an object, which the runtime reads and writes atomically.
//
// std::atomic<std::shared_ptr<T>> is exactly that, and libstdc++ and MSVC's
// STL have it. libc++ (the NDK's and Apple's) does not implement it yet, so
// there it is the same members over a mutex: nothing that uses it can tell the
// two apart, since every operation is still a single indivisible step.

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>
#include <version>

namespace MphRead::NativeRuntime
{
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    template <typename T>
    using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
    template <typename T>
    class AtomicSharedPtr final
    {
    public:
        using value_type = std::shared_ptr<T>;

        constexpr AtomicSharedPtr() noexcept = default;
        AtomicSharedPtr(value_type value) noexcept
            : _value(std::move(value))
        {
        }
        AtomicSharedPtr(const AtomicSharedPtr&) = delete;
        AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

        void operator=(value_type value) noexcept
        {
            store(std::move(value));
        }

        operator value_type() const noexcept
        {
            return load();
        }

        [[nodiscard]] bool is_lock_free() const noexcept
        {
            return false;
        }

        [[nodiscard]] value_type load(
            std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            (void)order;
            std::lock_guard<std::mutex> lock(_mutex);
            return _value;
        }

        void store(value_type value,
            std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            (void)order;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _value.swap(value);
            }
            // The old value is released outside the lock: its destructor may
            // run arbitrary code, including code that touches this field.
        }

        value_type exchange(value_type value,
            std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            (void)order;
            std::lock_guard<std::mutex> lock(_mutex);
            _value.swap(value);
            return value;
        }

        bool compare_exchange_strong(value_type& expected, value_type desired,
            std::memory_order success, std::memory_order failure) noexcept
        {
            (void)success;
            (void)failure;
            return CompareExchange(expected, std::move(desired));
        }

        bool compare_exchange_strong(value_type& expected, value_type desired,
            std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            (void)order;
            return CompareExchange(expected, std::move(desired));
        }

        bool compare_exchange_weak(value_type& expected, value_type desired,
            std::memory_order success, std::memory_order failure) noexcept
        {
            (void)success;
            (void)failure;
            return CompareExchange(expected, std::move(desired));
        }

        bool compare_exchange_weak(value_type& expected, value_type desired,
            std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            (void)order;
            return CompareExchange(expected, std::move(desired));
        }

    private:
        // Equal means the same pointer and the same ownership, as the standard
        // specialization compares.
        bool CompareExchange(value_type& expected, value_type desired) noexcept
        {
            value_type previous;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_value == expected
                    && !_value.owner_before(expected) && !expected.owner_before(_value))
                {
                    previous = std::exchange(_value, std::move(desired));
                    return true;
                }
                previous = expected;
                expected = _value;
            }
            return false;
        }

        mutable std::mutex _mutex{};
        value_type _value{};
    };
#endif
}

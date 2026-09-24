#pragma once

// A [ThreadStatic] field whose value has a destructor.
//
// C++ thread_local does this directly, and on every platform but one it is
// what this is. MinGW is the exception: its thread_local storage is emulated,
// and a thread_local object's destructor can run after the emulation has
// already freed the storage it lives in, so the destructor reads freed memory
// as the thread exits (a shared_ptr there faulted on text that belonged to
// something else). On Windows each value therefore lives on the heap in a
// fiber-local slot, whose callback Windows runs at thread exit with the value
// still intact. Nothing about what the value holds or when it is created
// changes: it is made on first use in each thread, from the same initial
// value, and destroyed when that thread ends.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime
{
    namespace Detail
    {
        struct ThreadStaticBox
        {
            virtual ~ThreadStaticBox() = default;
        };

        [[nodiscard]] std::uint32_t ThreadStaticAllocate();
        [[nodiscard]] ThreadStaticBox* ThreadStaticGet(std::uint32_t index) noexcept;
        void ThreadStaticSet(std::uint32_t index, ThreadStaticBox* box);
    }

    template <typename T>
    class ThreadStatic final
    {
    public:
        using Factory = T (*)();

        explicit ThreadStatic(Factory factory = nullptr)
            : _factory(factory), _index(Detail::ThreadStaticAllocate())
        {
        }

        ThreadStatic(const ThreadStatic&) = delete;
        ThreadStatic& operator=(const ThreadStatic&) = delete;

        [[nodiscard]] T& Value()
        {
            auto* box = static_cast<Box*>(Detail::ThreadStaticGet(_index));
            if (box == nullptr)
            {
                auto created = std::make_unique<Box>(_factory != nullptr ? _factory() : T{});
                box = created.get();
                Detail::ThreadStaticSet(_index, created.release());
            }
            return box->Value;
        }

    private:
        struct Box final : Detail::ThreadStaticBox
        {
            explicit Box(T value)
                : Value(std::move(value))
            {
            }

            T Value;
        };

        Factory _factory;
        std::uint32_t _index;
    };
}

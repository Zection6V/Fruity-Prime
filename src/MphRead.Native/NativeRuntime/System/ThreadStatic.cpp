#include "ThreadStatic.hpp"

#include <atomic>
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace MphRead::NativeRuntime::Detail
{
#if defined(_WIN32)
    namespace
    {
        VOID WINAPI DestroyBox(PVOID value)
        {
            delete static_cast<ThreadStaticBox*>(value);
        }
    }

    std::uint32_t ThreadStaticAllocate()
    {
        const DWORD index = ::FlsAlloc(&DestroyBox);
        if (index == FLS_OUT_OF_INDEXES)
        {
            throw std::runtime_error("No fiber-local storage slot is left.");
        }
        return static_cast<std::uint32_t>(index);
    }

    ThreadStaticBox* ThreadStaticGet(std::uint32_t index) noexcept
    {
        return static_cast<ThreadStaticBox*>(::FlsGetValue(static_cast<DWORD>(index)));
    }

    void ThreadStaticSet(std::uint32_t index, ThreadStaticBox* box)
    {
        if (!::FlsSetValue(static_cast<DWORD>(index), box))
        {
            delete box;
            throw std::runtime_error("A fiber-local storage slot could not be set.");
        }
    }
#else
    namespace
    {
        std::atomic<std::uint32_t> NextIndex{0};

        std::vector<std::unique_ptr<ThreadStaticBox>>& Boxes()
        {
            thread_local std::vector<std::unique_ptr<ThreadStaticBox>> boxes;
            return boxes;
        }
    }

    std::uint32_t ThreadStaticAllocate()
    {
        return NextIndex.fetch_add(1);
    }

    ThreadStaticBox* ThreadStaticGet(std::uint32_t index) noexcept
    {
        const auto& boxes = Boxes();
        return index < boxes.size() ? boxes[index].get() : nullptr;
    }

    void ThreadStaticSet(std::uint32_t index, ThreadStaticBox* box)
    {
        std::unique_ptr<ThreadStaticBox> owned(box);
        auto& boxes = Boxes();
        if (index >= boxes.size())
        {
            boxes.resize(static_cast<std::size_t>(index) + 1);
        }
        boxes[index] = std::move(owned);
    }
#endif
}

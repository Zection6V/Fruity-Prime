#include "Tasks.hpp"
#include "Exceptions.hpp"

#include <chrono>

#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#endif
#include <utility>

namespace MphRead::NativeRuntime
{
    void SetCurrentThreadName(const std::string& name)
    {
        // Thread.Name is what a debugger shows; the platform call is the same
        // one the runtime makes and a failure is not observable in managed code.
#if defined(_WIN32)
        const int length = MultiByteToWideChar(
            CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length < 0 ? 0 : length), L'\0');
        if (length > 0)
        {
            MultiByteToWideChar(
                CP_UTF8, 0, name.c_str(), static_cast<int>(name.size()),
                wide.data(), length);
        }
        // Looked up rather than linked: SetThreadDescription is Windows 10's.
        using SetThreadDescriptionFn = HRESULT (WINAPI*)(HANDLE, PCWSTR);
        if (HMODULE kernel = GetModuleHandleW(L"Kernel32.dll"); kernel != nullptr)
        {
            if (auto function = reinterpret_cast<SetThreadDescriptionFn>(
                    GetProcAddress(kernel, "SetThreadDescription")))
            {
                (void)function(GetCurrentThread(), wide.c_str());
            }
        }
#elif defined(__APPLE__)
        (void)pthread_setname_np(name.substr(0, 63).c_str());
#elif defined(__linux__)
        // The kernel keeps 15 bytes and refuses a longer name outright.
        (void)pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#else
        (void)name;
#endif
    }

    void ThreadSleep(std::int32_t milliseconds)
    {
        if (milliseconds < -1)
        {
            throw System::ArgumentOutOfRangeException("millisecondsTimeout");
        }
#if defined(_WIN32)
        Sleep(static_cast<DWORD>(milliseconds));
#else
        if (milliseconds == -1)
        {
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::hours(24));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
    }

    void TaskRun(std::function<void()> action, std::stop_token token)
    {
        // Task.Run with a canceled token returns a canceled task without running.
        if (token.stop_requested())
        {
            return;
        }
        std::thread([work = std::move(action)]()
        {
            // An exception inside the task is captured by the task; one nobody
            // observes does not end the process.
            try
            {
                work();
            }
            catch (...)
            {
            }
        }).detach();
    }
}

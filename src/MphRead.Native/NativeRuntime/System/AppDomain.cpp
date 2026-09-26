#include "AppDomain.hpp"

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        // Never destroyed: these are read from a terminate handler and from an
        // exception filter, both of which can run after exit has begun
        // destroying static objects.
        struct Registry final
        {
            std::mutex Lock;
            std::vector<UnhandledExceptionHandler> Handlers;
            std::terminate_handler PreviousTerminate = nullptr;
            std::atomic_bool Hooked{false};
            std::atomic_bool Running{false};
#if defined(_WIN32)
            LPTOP_LEVEL_EXCEPTION_FILTER PreviousFilter = nullptr;
#endif
        };

        Registry& GetRegistry()
        {
            static Registry& registry = *new Registry();
            return registry;
        }

        void Raise(std::exception_ptr exception)
        {
            Registry& registry = GetRegistry();
            // A failure inside a handler must not start the sequence again.
            if (registry.Running.exchange(true))
            {
                return;
            }
            std::vector<UnhandledExceptionHandler> handlers;
            {
                const std::lock_guard<std::mutex> guard(registry.Lock);
                handlers = registry.Handlers;
            }
            for (const UnhandledExceptionHandler& handler : handlers)
            {
                if (!handler)
                {
                    continue;
                }
                try
                {
                    handler(exception);
                }
                catch (...)
                {
                    // One subscriber throwing does not cancel the rest, which
                    // is how the managed event behaves.
                }
            }
            registry.Running.store(false);
        }

        [[noreturn]] void TerminateHandler()
        {
            Raise(std::current_exception());
            Registry& registry = GetRegistry();
            const std::terminate_handler previous = registry.PreviousTerminate;
            if (previous != nullptr && previous != &TerminateHandler)
            {
                previous();
            }
            std::abort();
        }

#if defined(_WIN32)
        LONG WINAPI FaultFilter(EXCEPTION_POINTERS* pointers)
        {
            // A hardware fault: there is no exception object to hand over, and
            // in the managed build this arrives as an ordinary exception.
            Raise(std::exception_ptr());
            Registry& registry = GetRegistry();
            const LPTOP_LEVEL_EXCEPTION_FILTER previous = registry.PreviousFilter;
            if (previous != nullptr && previous != &FaultFilter)
            {
                return previous(pointers);
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }
#endif
    }

    void AppDomainAddUnhandledExceptionHandler(UnhandledExceptionHandler handler)
    {
        if (!handler)
        {
            return;
        }
        Registry& registry = GetRegistry();
        {
            const std::lock_guard<std::mutex> guard(registry.Lock);
            registry.Handlers.push_back(std::move(handler));
        }
        if (!registry.Hooked.exchange(true))
        {
            registry.PreviousTerminate = std::set_terminate(&TerminateHandler);
#if defined(_WIN32)
            registry.PreviousFilter = ::SetUnhandledExceptionFilter(&FaultFilter);
#endif
        }
    }

    void AppDomainAddProcessExitHandler(std::function<void()> handler)
    {
        if (!handler)
        {
            return;
        }
        // Never destroyed, for the same reason as the registry above.
        struct ExitRegistry final
        {
            std::mutex Lock;
            std::vector<std::function<void()>> Handlers;
        };
        static ExitRegistry& exits = *new ExitRegistry();
        static const bool hooked = []()
        {
            std::atexit([]()
            {
                std::vector<std::function<void()>> handlers;
                {
                    const std::lock_guard<std::mutex> guard(exits.Lock);
                    handlers.swap(exits.Handlers);
                }
                for (const std::function<void()>& run : handlers)
                {
                    try
                    {
                        run();
                    }
                    catch (...)
                    {
                    }
                }
            });
            return true;
        }();
        static_cast<void>(hooked);
        const std::lock_guard<std::mutex> guard(exits.Lock);
        exits.Handlers.push_back(std::move(handler));
    }
}

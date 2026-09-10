#include "ShutdownSignals.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <thread>
#include <unistd.h>
#endif

namespace MphRead::Mods
{
    namespace
    {
        struct ShutdownState final
        {
            std::atomic<int> Fired{0};
            std::atomic<std::shared_ptr<std::function<void()>>> Action;

            void SetAction(std::function<void()> action)
            {
                std::shared_ptr<std::function<void()>> next;
                if (action)
                {
                    next = std::make_shared<std::function<void()>>(std::move(action));
                }
                Action.store(std::move(next), std::memory_order_seq_cst);
            }

            void Fire()
            {
                if (Fired.exchange(1, std::memory_order_seq_cst) != 0)
                {
                    return;
                }
                std::shared_ptr<std::function<void()>> action =
                    Action.load(std::memory_order_seq_cst);
                if (action)
                {
                    (*action)();
                }
            }
        };

        struct SignalEntry final
        {
            std::uint64_t Id;
            std::shared_ptr<ShutdownState> State;
        };

        struct SignalSlot final
        {
            std::vector<SignalEntry> Entries;
            bool Installed = false;
#if !defined(_WIN32)
            struct sigaction Previous{};
#endif
        };

#if !defined(_WIN32)
        class SignalHub;
        std::atomic<SignalHub*> ActiveHub{nullptr};
#endif

        class SignalHub final
        {
        public:
            SignalHub(const SignalHub&) = delete;
            SignalHub& operator=(const SignalHub&) = delete;

            static SignalHub& Instance()
            {
                static SignalHub* hub = new SignalHub();
                return *hub;
            }

            void AddConsole(const std::shared_ptr<ShutdownState>& state)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _console.push_back(state);
                try
                {
                    EnsureInstalledLocked(SIGINT);
                }
                catch (...)
                {
                    // Console.CancelKeyPress remains a subscribed event even if
                    // this platform cannot currently attach its native source.
                }
            }

            void RemoveOneConsole(const std::shared_ptr<ShutdownState>& state)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                const auto found = std::find_if(_console.rbegin(), _console.rend(),
                    [&state](const std::shared_ptr<ShutdownState>& candidate)
                    {
                        return candidate.get() == state.get();
                    });
                if (found != _console.rend())
                {
                    _console.erase(std::next(found).base());
                }
                MaybeUninstallLocked(SIGINT);
            }

            std::uint64_t AddSignal(int signal, const std::shared_ptr<ShutdownState>& state)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                EnsureInstalledLocked(signal);
                SignalSlot& slot = Slot(signal);
                const std::uint64_t id = _nextId++;
                slot.Entries.push_back(SignalEntry{id, state});
                return id;
            }

            void RemoveSignal(int signal, std::uint64_t id)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                SignalSlot& slot = Slot(signal);
                const auto found = std::find_if(slot.Entries.begin(), slot.Entries.end(),
                    [id](const SignalEntry& entry)
                    {
                        return entry.Id == id;
                    });
                if (found != slot.Entries.end())
                {
                    slot.Entries.erase(found);
                }
                MaybeUninstallLocked(signal);
            }

#if defined(_WIN32)
            static BOOL WINAPI ConsoleHandler(DWORD controlType)
            {
                SignalHub& hub = Instance();
                if (controlType == CTRL_C_EVENT)
                {
                    return hub.DispatchWindows(true, SIGINT) ? TRUE : FALSE;
                }
                if (controlType == CTRL_BREAK_EVENT)
                {
                    return hub.DispatchWindows(true, 0) ? TRUE : FALSE;
                }
                if (controlType == CTRL_CLOSE_EVENT || controlType == CTRL_LOGOFF_EVENT
                    || controlType == CTRL_SHUTDOWN_EVENT)
                {
                    return hub.DispatchWindows(false, SIGTERM) ? TRUE : FALSE;
                }
                return FALSE;
            }
#endif

        private:
            SignalHub()
            {
#if !defined(_WIN32)
                int descriptors[2] = {-1, -1};
                if (pipe(descriptors) != 0)
                {
                    throw std::system_error(errno, std::generic_category(), "pipe");
                }
                _readFd = descriptors[0];
                _writeFd = descriptors[1];

                const int flags = fcntl(_writeFd, F_GETFL, 0);
                if (flags == -1 || fcntl(_writeFd, F_SETFL, flags | O_NONBLOCK) == -1)
                {
                    const int error = errno;
                    close(_readFd);
                    close(_writeFd);
                    _readFd = -1;
                    _writeFd = -1;
                    throw std::system_error(error, std::generic_category(), "fcntl");
                }

                const int readFlags = fcntl(_readFd, F_GETFD, 0);
                const int writeFlags = fcntl(_writeFd, F_GETFD, 0);
                if (readFlags != -1)
                {
                    static_cast<void>(fcntl(_readFd, F_SETFD, readFlags | FD_CLOEXEC));
                }
                if (writeFlags != -1)
                {
                    static_cast<void>(fcntl(_writeFd, F_SETFD, writeFlags | FD_CLOEXEC));
                }

                ActiveHub.store(this, std::memory_order_release);
                std::thread([this]() { RunPosix(); }).detach();
#endif
            }

            SignalSlot& Slot(int signal)
            {
                if (signal == SIGINT)
                {
                    return _sigint;
                }
                if (signal == SIGTERM)
                {
                    return _sigterm;
                }
                throw std::invalid_argument("unsupported signal");
            }

            bool NeedsSignalLocked(int signal)
            {
                SignalSlot& slot = Slot(signal);
                return !slot.Entries.empty() || (signal == SIGINT && !_console.empty());
            }

            void EnsureInstalledLocked(int signal)
            {
#if defined(_WIN32)
                SignalSlot& slot = Slot(signal);
                if (_windowsInstalled)
                {
                    slot.Installed = true;
                    return;
                }
                if (SetConsoleCtrlHandler(ConsoleHandler, TRUE) == FALSE)
                {
                    throw std::system_error(
                        static_cast<int>(GetLastError()), std::system_category(),
                        "SetConsoleCtrlHandler");
                }
                _windowsInstalled = true;
                slot.Installed = true;
#else
                SignalSlot& slot = Slot(signal);
                if (slot.Installed)
                {
                    return;
                }
                struct sigaction action{};
                action.sa_handler = PosixHandler;
                sigemptyset(&action.sa_mask);
                action.sa_flags = 0;
                if (sigaction(signal, &action, &slot.Previous) != 0)
                {
                    throw std::system_error(errno, std::generic_category(), "sigaction");
                }
                slot.Installed = true;
#endif
            }

            void MaybeUninstallLocked(int signal)
            {
#if defined(_WIN32)
                Slot(signal).Installed = NeedsSignalLocked(signal);
                if (!_windowsInstalled || NeedsSignalLocked(SIGINT) || NeedsSignalLocked(SIGTERM))
                {
                    return;
                }
                if (SetConsoleCtrlHandler(ConsoleHandler, FALSE) != FALSE)
                {
                    _windowsInstalled = false;
                    _sigint.Installed = false;
                    _sigterm.Installed = false;
                }
#else
                SignalSlot& slot = Slot(signal);
                if (!slot.Installed || NeedsSignalLocked(signal))
                {
                    return;
                }
                if (sigaction(signal, &slot.Previous, nullptr) == 0)
                {
                    slot.Installed = false;
                }
#endif
            }

            void Snapshot(bool includeConsole, int signal,
                std::vector<std::shared_ptr<ShutdownState>>& console,
                std::vector<SignalEntry>& entries)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (includeConsole)
                {
                    console = _console;
                }
                if (signal != 0)
                {
                    entries = Slot(signal).Entries;
                }
            }

            void Dispatch(bool includeConsole, int signal)
            {
                std::vector<std::shared_ptr<ShutdownState>> console;
                std::vector<SignalEntry> entries;
                Snapshot(includeConsole, signal, console, entries);
                for (const std::shared_ptr<ShutdownState>& state : console)
                {
                    state->Fire();
                }
                for (const SignalEntry& entry : entries)
                {
                    entry.State->Fire();
                }
            }

#if defined(_WIN32)
            bool DispatchWindows(bool includeConsole, int signal)
            {
                std::vector<std::shared_ptr<ShutdownState>> console;
                std::vector<SignalEntry> entries;
                Snapshot(includeConsole, signal, console, entries);
                if (console.empty() && entries.empty())
                {
                    return false;
                }
                for (const std::shared_ptr<ShutdownState>& state : console)
                {
                    state->Fire();
                }
                for (const SignalEntry& entry : entries)
                {
                    entry.State->Fire();
                }
                return true;
            }
#else
            static void PosixHandler(int signal)
            {
                SignalHub* hub = ActiveHub.load(std::memory_order_acquire);
                if (hub == nullptr)
                {
                    return;
                }
                const unsigned char value = static_cast<unsigned char>(signal);
                static_cast<void>(write(hub->_writeFd, &value, 1));
            }

            void RunPosix()
            {
                unsigned char signals[64];
                for (;;)
                {
                    const ssize_t count = read(_readFd, signals, sizeof(signals));
                    if (count < 0)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        return;
                    }
                    if (count == 0)
                    {
                        return;
                    }
                    for (ssize_t i = 0; i < count; ++i)
                    {
                        const int signal = static_cast<int>(signals[i]);
                        Dispatch(signal == SIGINT, signal);
                    }
                }
            }
#endif

            std::mutex _mutex;
            std::vector<std::shared_ptr<ShutdownState>> _console;
            SignalSlot _sigint;
            SignalSlot _sigterm;
            std::uint64_t _nextId = 1;
#if defined(_WIN32)
            bool _windowsInstalled = false;
#else
            int _readFd = -1;
            int _writeFd = -1;
#endif
        };

        class SignalRegistration final
        {
        public:
            SignalRegistration(int signal, std::shared_ptr<ShutdownState> state)
                : _signal(signal), _id(SignalHub::Instance().AddSignal(signal, state))
            {
            }

            SignalRegistration(const SignalRegistration&) = delete;
            SignalRegistration& operator=(const SignalRegistration&) = delete;

            ~SignalRegistration()
            {
                if (_id != 0)
                {
                    try
                    {
                        Dispose();
                    }
                    catch (...)
                    {
                    }
                }
            }

            void Dispose()
            {
                if (_id == 0)
                {
                    return;
                }
                const std::uint64_t id = _id;
                SignalHub::Instance().RemoveSignal(_signal, id);
                _id = 0;
            }

        private:
            int _signal;
            std::uint64_t _id;
        };
    }

    struct ShutdownSignals::Impl final
    {
        std::vector<std::unique_ptr<SignalRegistration>> Registrations;
        std::shared_ptr<ShutdownState> State = std::make_shared<ShutdownState>();

        void Register(int signal)
        {
            try
            {
                auto registration = std::make_unique<SignalRegistration>(signal, State);
                Registrations.push_back(std::move(registration));
            }
            catch (...)
            {
                // Not every platform offers every signal; Ctrl+C still works.
            }
        }
    };

    ShutdownSignals::ShutdownSignals()
        : _impl(std::make_unique<Impl>())
    {
    }

    ShutdownSignals::~ShutdownSignals() noexcept(false)
    {
        Dispose();
    }

    void ShutdownSignals::OnShutdown(std::function<void()> action)
    {
        _impl->State->SetAction(std::move(action));
        SignalHub::Instance().AddConsole(_impl->State);
        _impl->Register(SIGTERM);
        _impl->Register(SIGINT);
    }

    void ShutdownSignals::Dispose()
    {
        SignalHub::Instance().RemoveOneConsole(_impl->State);
        for (const std::unique_ptr<SignalRegistration>& registration : _impl->Registrations)
        {
            registration->Dispose();
        }
        _impl->Registrations.clear();
    }
}

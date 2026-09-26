#pragma once

// Avalonia.Threading: the UI thread's dispatcher -- a queue of jobs by
// priority, pumped by RunJobs -- and DispatcherTimer, which fires from that
// same pump. The launcher pumps it once a game frame, as UiSurface.Tick does.

#include "Base.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia::Threading
{
    enum class DispatcherPriority : std::int32_t
    {
        Inactive = 0,
        SystemIdle = 1,
        ApplicationIdle = 2,
        ContextIdle = 3,
        Background = 4,
        Input = 5,
        Loaded = 6,
        Render = 7,
        Layout = 8,
        DataBind = 9,
        Normal = 10,
        Send = 11,
        Default = Normal
    };

    using TimeSpan = std::chrono::duration<double>;

    class DispatcherTimer;

    class Dispatcher final
    {
    public:
        // Dispatcher.UIThread: the thread that first asks.
        [[nodiscard]] static Dispatcher& UIThread();

        void Post(std::function<void()> action, DispatcherPriority priority = DispatcherPriority::Default);
        // Runs now when called on the UI thread; otherwise queued and waited for.
        void Invoke(const std::function<void()>& action, DispatcherPriority priority = DispatcherPriority::Send);
        // Everything queued, and every timer that is due.
        void RunJobs();
        void RunJobs(DispatcherPriority minimumPriority);
        [[nodiscard]] bool CheckAccess() const noexcept;
        void VerifyAccess() const;
        [[nodiscard]] bool HasJobs() const;

    private:
        Dispatcher();
        friend class DispatcherTimer;
        void AddTimer(DispatcherTimer* timer);
        void RemoveTimer(DispatcherTimer* timer);
        void FireTimers();

        struct Job final
        {
            std::function<void()> Action;
            DispatcherPriority Priority;
            std::uint64_t Order;
        };
        mutable std::recursive_mutex _mutex;
        std::vector<Job> _jobs;
        std::uint64_t _order = 0;
        std::thread::id _thread;
        std::vector<DispatcherTimer*> _timers;
    };

    class DispatcherTimer final
    {
    public:
        DispatcherTimer() = default;
        explicit DispatcherTimer(DispatcherPriority priority)
            : Priority(priority)
        {
        }
        DispatcherTimer(TimeSpan interval, DispatcherPriority priority, std::function<void()> callback);
        ~DispatcherTimer();

        DispatcherTimer(const DispatcherTimer&) = delete;
        DispatcherTimer& operator=(const DispatcherTimer&) = delete;

        [[nodiscard]] TimeSpan Interval() const noexcept { return _interval; }
        void Interval(TimeSpan value);
        [[nodiscard]] bool IsEnabled() const noexcept { return _enabled; }
        void IsEnabled(bool value);
        void Start();
        void Stop();

        Event<DispatcherTimer&> Tick;
        DispatcherPriority Priority = DispatcherPriority::Background;
        std::any Tag{};

        // DispatcherTimer.RunOnce(action, interval): fires once, then goes.
        static void RunOnce(std::function<void()> action, TimeSpan interval,
            DispatcherPriority priority = DispatcherPriority::Default);
        // DispatcherTimer.Run(func, interval): fires until the callback
        // returns false.
        static void Run(std::function<bool()> action, TimeSpan interval,
            DispatcherPriority priority = DispatcherPriority::Default);

    private:
        friend class Dispatcher;
        TimeSpan _interval{0};
        bool _enabled = false;
        std::chrono::steady_clock::time_point _due{};
    };
}

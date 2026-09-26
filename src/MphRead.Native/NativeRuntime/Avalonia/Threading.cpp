#include "Threading.hpp"

#include <algorithm>
#include <condition_variable>
#include <stdexcept>

namespace MphRead::NativeRuntime::Avalonia::Threading
{
    Dispatcher& Dispatcher::UIThread()
    {
        static Dispatcher dispatcher;
        return dispatcher;
    }

    Dispatcher::Dispatcher()
        : _thread(std::this_thread::get_id())
    {
    }

    bool Dispatcher::CheckAccess() const noexcept
    {
        return std::this_thread::get_id() == _thread;
    }

    void Dispatcher::VerifyAccess() const
    {
        if (!CheckAccess())
        {
            throw std::runtime_error("Call from invalid thread");
        }
    }

    bool Dispatcher::HasJobs() const
    {
        std::lock_guard lock(_mutex);
        return !_jobs.empty();
    }

    void Dispatcher::Post(std::function<void()> action, DispatcherPriority priority)
    {
        if (!action)
        {
            return;
        }
        std::lock_guard lock(_mutex);
        _jobs.push_back(Job{std::move(action), priority, ++_order});
    }

    void Dispatcher::Invoke(const std::function<void()>& action, DispatcherPriority priority)
    {
        if (CheckAccess())
        {
            action();
            return;
        }
        std::mutex done;
        std::condition_variable signal;
        bool finished = false;
        Post([&]
        {
            action();
            std::lock_guard lock(done);
            finished = true;
            signal.notify_all();
        }, priority);
        std::unique_lock lock(done);
        signal.wait(lock, [&] { return finished; });
    }

    void Dispatcher::RunJobs()
    {
        RunJobs(DispatcherPriority::Inactive);
    }

    void Dispatcher::RunJobs(DispatcherPriority minimumPriority)
    {
        FireTimers();
        // Highest priority first, then in the order posted. Jobs posted while
        // running are run in the same pump, as Avalonia's RunJobs does.
        for (int guard = 0; guard < 100000; guard++)
        {
            Job job;
            {
                std::lock_guard lock(_mutex);
                auto best = _jobs.end();
                for (auto it = _jobs.begin(); it != _jobs.end(); ++it)
                {
                    if (static_cast<std::int32_t>(it->Priority) < static_cast<std::int32_t>(minimumPriority))
                    {
                        continue;
                    }
                    if (best == _jobs.end() || static_cast<std::int32_t>(it->Priority) > static_cast<std::int32_t>(best->Priority)
                        || (it->Priority == best->Priority && it->Order < best->Order))
                    {
                        best = it;
                    }
                }
                if (best == _jobs.end())
                {
                    return;
                }
                job = std::move(*best);
                _jobs.erase(best);
            }
            job.Action();
        }
    }

    void Dispatcher::AddTimer(DispatcherTimer* timer)
    {
        std::lock_guard lock(_mutex);
        if (std::find(_timers.begin(), _timers.end(), timer) == _timers.end())
        {
            _timers.push_back(timer);
        }
    }

    void Dispatcher::RemoveTimer(DispatcherTimer* timer)
    {
        std::lock_guard lock(_mutex);
        std::erase(_timers, timer);
    }

    void Dispatcher::FireTimers()
    {
        const auto now = std::chrono::steady_clock::now();
        std::vector<DispatcherTimer*> due;
        {
            std::lock_guard lock(_mutex);
            for (DispatcherTimer* timer : _timers)
            {
                if (timer->_enabled && now >= timer->_due)
                {
                    due.push_back(timer);
                }
            }
        }
        for (DispatcherTimer* timer : due)
        {
            // A tick may stop or delete another timer; check it is still here.
            {
                std::lock_guard lock(_mutex);
                if (std::find(_timers.begin(), _timers.end(), timer) == _timers.end() || !timer->_enabled)
                {
                    continue;
                }
                timer->_due = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(timer->_interval);
            }
            timer->Tick(*timer);
        }
    }

    DispatcherTimer::DispatcherTimer(TimeSpan interval, DispatcherPriority priority, std::function<void()> callback)
        : Priority(priority), _interval(interval)
    {
        Tick += [callback = std::move(callback)](DispatcherTimer&) { callback(); };
        Start();
    }

    DispatcherTimer::~DispatcherTimer()
    {
        Dispatcher::UIThread().RemoveTimer(this);
    }

    void DispatcherTimer::Interval(TimeSpan value)
    {
        _interval = value;
        if (_enabled)
        {
            _due = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(_interval);
        }
    }

    void DispatcherTimer::IsEnabled(bool value)
    {
        if (value)
        {
            Start();
        }
        else
        {
            Stop();
        }
    }

    void DispatcherTimer::Start()
    {
        _enabled = true;
        _due = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(_interval);
        Dispatcher::UIThread().AddTimer(this);
    }

    void DispatcherTimer::Stop()
    {
        _enabled = false;
        Dispatcher::UIThread().RemoveTimer(this);
    }

    void DispatcherTimer::RunOnce(std::function<void()> action, TimeSpan interval, DispatcherPriority priority)
    {
        auto timer = std::make_shared<DispatcherTimer>(priority);
        timer->Interval(interval);
        // The timer keeps itself alive until it has fired.
        auto keep = std::make_shared<std::shared_ptr<DispatcherTimer>>(timer);
        timer->Tick += [action = std::move(action), keep](DispatcherTimer& self)
        {
            self.Stop();
            action();
            Dispatcher::UIThread().Post([keep] { keep->reset(); }, DispatcherPriority::Background);
        };
        timer->Start();
    }

    void DispatcherTimer::Run(std::function<bool()> action, TimeSpan interval, DispatcherPriority priority)
    {
        auto timer = std::make_shared<DispatcherTimer>(priority);
        timer->Interval(interval);
        auto keep = std::make_shared<std::shared_ptr<DispatcherTimer>>(timer);
        timer->Tick += [action = std::move(action), keep](DispatcherTimer& self)
        {
            if (!action())
            {
                self.Stop();
                Dispatcher::UIThread().Post([keep] { keep->reset(); }, DispatcherPriority::Background);
            }
        };
        timer->Start();
    }
}

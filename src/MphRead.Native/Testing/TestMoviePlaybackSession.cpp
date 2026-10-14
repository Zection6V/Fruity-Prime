#include "../Formats/Movie.hpp"
#include "../Mods/MoviePlaybackSession.hpp"

#include <algorithm>
#include <barrier>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
    using MphRead::Formats::MovieNativeRuntime::MovieTask;
    using MphRead::Mods::MoviePlaybackPolicy;
    using MphRead::Mods::MoviePlaybackSession;

    void Expect(bool value, const char* message);

    class ControlledResume final
    {
    public:
        struct Awaiter final
        {
            ControlledResume& Owner;

            [[nodiscard]] bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle) const
            {
                Owner.Suspend(handle);
            }
            void await_resume() const noexcept {}
        };

        [[nodiscard]] Awaiter Wait() noexcept { return Awaiter{*this}; }

        void WaitUntilSuspended()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.wait(lock, [this] { return static_cast<bool>(_continuation); });
        }

        void Resume()
        {
            std::coroutine_handle<> continuation;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                continuation = std::exchange(_continuation, std::coroutine_handle<>{});
            }
            Expect(static_cast<bool>(continuation), "The controlled coroutine was not suspended.");
            continuation.resume();
        }

    private:
        void Suspend(std::coroutine_handle<> continuation)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _continuation = continuation;
            }
            _condition.notify_all();
        }

        std::mutex _mutex;
        std::condition_variable _condition;
        std::coroutine_handle<> _continuation{};
    };

    void Expect(bool value, const char* message)
    {
        if (!value)
        {
            throw std::runtime_error(message);
        }
    }

    MovieTask WaitForStop(ControlledResume& resume, std::stop_token token,
        std::atomic<bool>& observedStop, std::atomic<bool>& reachedFinalSuspend)
    {
        co_await resume.Wait();
        observedStop.store(token.stop_requested(), std::memory_order_release);
        reachedFinalSuspend.store(true, std::memory_order_release);
    }

    void CheckCoroutineDrain()
    {
        std::shared_ptr<MoviePlaybackSession> session = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 7, MoviePlaybackPolicy::Normal);
        std::mutex mutex;
        std::condition_variable condition;
        bool starterReturned = false;
        ControlledResume resume;
        std::atomic<bool> observedStop{false};
        std::atomic<bool> reachedFinalSuspend{false};

        Expect(session->StartTask("controlled-audio", [&](std::stop_token token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            MovieTask task = WaitForStop(resume, token, observedStop, reachedFinalSuspend);
            task.GetAwaiter().ObserveCompletion(std::move(completion));
            {
                std::lock_guard<std::mutex> lock(mutex);
                starterReturned = true;
            }
            condition.notify_all();
        }), "Session refused an initial movie task.");
        session->MarkRunning();

        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&] { return starterReturned; });
        }
        Expect(session->RemainingTasks() == 1,
            "The task was counted complete when its starter returned instead of at final_suspend.");
        Expect(!reachedFinalSuspend.load(std::memory_order_acquire),
            "The controlled coroutine unexpectedly finished before cancellation.");

        session->RequestStop("test", "CheckCoroutineDrain");
        resume.Resume();
        session->WaitForDrain();
        Expect(session->RemainingTasks() == 0, "The movie session did not drain to zero.");
        Expect(observedStop.load(std::memory_order_acquire),
            "The coroutine did not observe the session cancellation token.");
        Expect(reachedFinalSuspend.load(std::memory_order_acquire),
            "The session drained before the coroutine reached final_suspend.");
    }

    void CheckActiveCallDrainBeforeRelease()
    {
        std::shared_ptr<MoviePlaybackSession> session = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 14, MoviePlaybackPolicy::Normal);
        std::mutex mutex;
        std::condition_variable condition;
        bool callEntered = false;
        bool returnFromCall = false;
        bool stopIssued = false;
        bool cleanupReturned = false;
        std::atomic<bool> callReturned{false};
        std::atomic<bool> releasedResources{false};
        std::atomic<bool> releasedBeforeCallReturned{false};

        Expect(session->StartTask("controlled-openal-call", [&](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            std::unique_lock<std::mutex> lock(mutex);
            callEntered = true;
            condition.notify_all();
            condition.wait(lock, [&] { return returnFromCall; });
            lock.unlock();
            callReturned.store(true, std::memory_order_release);
            completion({});
        }), "The session refused the controlled OpenAL call.");

        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!condition.wait_for(lock, std::chrono::seconds(5), [&] { return callEntered; }))
            {
                lock.unlock();
                session->RequestStop("test-timeout", "CheckActiveCallDrainBeforeRelease");
                {
                    std::lock_guard<std::mutex> releaseLock(mutex);
                    returnFromCall = true;
                }
                condition.notify_all();
                session->WaitForDrain();
                Expect(false, "The controlled OpenAL call did not start.");
            }
        }

        std::thread cleanup([&]
        {
            session->RequestStop("test-active-call", "CheckActiveCallDrainBeforeRelease");
            {
                std::lock_guard<std::mutex> lock(mutex);
                stopIssued = true;
            }
            condition.notify_all();
            session->WaitForDrain();
            releasedBeforeCallReturned.store(
                !callReturned.load(std::memory_order_acquire), std::memory_order_release);
            releasedResources.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(mutex);
                cleanupReturned = true;
            }
            condition.notify_all();
        });

        bool stopWasIssued = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            stopWasIssued = condition.wait_for(lock, std::chrono::seconds(5), [&] { return stopIssued; });
        }
        if (!stopWasIssued)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                returnFromCall = true;
            }
            condition.notify_all();
            cleanup.join();
            Expect(false, "Cleanup did not issue its movie stop request.");
        }

        bool cleanupReturnedWhileCallWasHeld = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cleanupReturnedWhileCallWasHeld = condition.wait_for(
                lock, std::chrono::milliseconds(100), [&] { return cleanupReturned; });
            returnFromCall = true;
        }
        condition.notify_all();
        cleanup.join();

        Expect(!cleanupReturnedWhileCallWasHeld,
            "Cleanup released resources while the controlled OpenAL call was still active.");
        Expect(!releasedBeforeCallReturned.load(std::memory_order_acquire),
            "Cleanup returned from drain before the controlled OpenAL call finished.");
        Expect(releasedResources.load(std::memory_order_acquire),
            "Cleanup did not release resources after the active call drained.");
        Expect(session->RemainingTasks() == 0,
            "The active-call cleanup did not drain to zero tasks.");
    }

    void CheckTaskCompletionAndFailure()
    {
        std::shared_ptr<MoviePlaybackSession> completed = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 8, MoviePlaybackPolicy::Normal);
        Expect(completed->StartTask("image-loop", [&](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            completion({});
        }), "Session refused a completed task.");
        completed->WaitForDrain();
        Expect(completed->RemainingTasks() == 0, "Completed work remained in flight.");

        std::shared_ptr<MoviePlaybackSession> failed = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 9, MoviePlaybackPolicy::Normal);
        Expect(failed->StartTask("decoder-top", [](std::stop_token,
            MoviePlaybackSession::TaskCompletion)
        {
            throw std::runtime_error("controlled decoder failure");
        }), "Session refused a failure-path task.");
        failed->WaitForDrain();
        Expect(failed->RemainingTasks() == 0, "Failed work was not included in the drain.");
        Expect(failed->HasFailure(), "The session did not preserve the task exception.");
        Expect(failed->FailureMessage() == "controlled decoder failure",
            "The session lost the task exception message.");
        Expect(failed->TryObserveFailure() && !failed->TryObserveFailure(),
            "A session failure was not observed exactly once.");
    }

    void CheckSessionIsolationAndStopRules()
    {
        std::shared_ptr<MoviePlaybackSession> first = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 10, MoviePlaybackPolicy::Normal);
        std::shared_ptr<MoviePlaybackSession> second = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 11, MoviePlaybackPolicy::Normal);
        std::mutex mutex;
        std::condition_variable condition;
        MoviePlaybackSession::TaskCompletion completeFirst;
        MoviePlaybackSession::TaskCompletion completeSecond;
        bool firstStarted = false;
        bool secondStarted = false;

        Expect(first->StartTask("held-first", [&](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                completeFirst = std::move(completion);
                firstStarted = true;
            }
            condition.notify_all();
        }), "The first session refused a task.");
        Expect(second->StartTask("held-second", [&](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                completeSecond = std::move(completion);
                secondStarted = true;
            }
            condition.notify_all();
        }), "The second session refused a task.");

        {
            std::unique_lock<std::mutex> lock(mutex);
            Expect(condition.wait_for(lock, std::chrono::seconds(5), [&]
            {
                return firstStarted && secondStarted;
            }), "The held session tasks did not start.");
        }

        first->RequestStop("test", "CheckSessionIsolationAndStopRules");
        first->RequestStop("duplicate-test", "CheckSessionIsolationAndStopRules");
        Expect(first->StopRequested(), "The first session did not receive cancellation.");
        Expect(!second->StopRequested(), "Stopping one movie cancelled a different session.");
        Expect(!first->StartTask("late-task", [](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            completion({});
        }), "A stopping session accepted a new task.");

        MoviePlaybackSession::TaskCompletion finishFirst;
        {
            std::lock_guard<std::mutex> lock(mutex);
            finishFirst = completeFirst;
        }
        finishFirst({});
        finishFirst({});
        first->WaitForDrain();
        Expect(first->RemainingTasks() == 0, "The first session did not drain.");
        Expect(second->RemainingTasks() == 1, "Draining one session changed another session's task count.");

        second->RequestStop("test", "CheckSessionIsolationAndStopRules");
        MoviePlaybackSession::TaskCompletion finishSecond;
        {
            std::lock_guard<std::mutex> lock(mutex);
            finishSecond = completeSecond;
        }
        finishSecond({});
        second->WaitForDrain();
        first->WaitForDrain();
        Expect(second->RemainingTasks() == 0, "The second session did not drain.");
    }

    void CheckFadeCompletionIsSessionScopedAndStoppedEventsAreDiscarded()
    {
        std::shared_ptr<MoviePlaybackSession> first = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 36, MoviePlaybackPolicy::Normal);
        std::shared_ptr<MoviePlaybackSession> second = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 37, MoviePlaybackPolicy::Normal);

        first->PostFadeCompletion();
        Expect(first->FadeCompletionPending(), "The active session lost its fade completion.");
        Expect(!second->FadeCompletionPending(), "A fade completion leaked into a different session.");
        Expect(first->TakeFadeCompletion(), "The active session did not consume its fade completion.");
        Expect(first->FadeCompletionConsumeCount() == 1,
            "The active session did not consume exactly one fade completion.");
        Expect(!first->TakeFadeCompletion(), "A fade completion was consumed more than once.");
        Expect(first->FadeCompletionConsumeCount() == 1,
            "A fade completion was counted more than once.");

        first->PostFadeCompletion();
        first->RequestStop("test-stale-fade", "CheckFadeCompletionIsSessionScopedAndStoppedEventsAreDiscarded");
        Expect(!first->TakeFadeCompletion(), "A queued fade completion survived session stop.");
        first->PostFadeCompletion();
        Expect(!first->TakeFadeCompletion(), "A stopped session accepted a late fade completion.");
        Expect(first->FadeCompletionConsumeCount() == 1,
            "A stopped session counted a late fade completion.");
        first->WaitForDrain();
        Expect(!first->FadeCompletionPending(), "A drained session retained a fade completion.");
    }

    void CheckConcurrentFadeCompletionAndStop()
    {
        constexpr std::int32_t iterations = 64;
        for (std::int32_t i = 0; i < iterations; ++i)
        {
            std::shared_ptr<MoviePlaybackSession> session = MoviePlaybackSession::Create(
                MoviePlaybackSession::NextSceneId(), 38, MoviePlaybackPolicy::Normal);
            std::barrier start(3);
            std::thread fadeCompletion([&]
            {
                start.arrive_and_wait();
                session->PostFadeCompletion();
            });
            std::thread stop([&]
            {
                start.arrive_and_wait();
                session->RequestStop("test-fade-race", "CheckConcurrentFadeCompletionAndStop");
            });

            start.arrive_and_wait();
            fadeCompletion.join();
            stop.join();
            session->WaitForDrain();

            Expect(!session->FadeCompletionPending() && !session->TakeFadeCompletion(),
                "A Fade completion survived a concurrent session stop.");
        }
    }

    void CheckFailureStopsSiblingWork()
    {
        std::shared_ptr<MoviePlaybackSession> session = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 12, MoviePlaybackPolicy::Normal);
        std::mutex mutex;
        std::condition_variable condition;
        MoviePlaybackSession::TaskCompletion finishSibling;
        std::stop_token siblingToken;
        bool siblingStarted = false;

        Expect(session->StartTask("held-audio", [&](std::stop_token token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                siblingToken = token;
                finishSibling = std::move(completion);
                siblingStarted = true;
            }
            condition.notify_all();
        }), "The session refused its held audio task.");
        {
            std::unique_lock<std::mutex> lock(mutex);
            Expect(condition.wait_for(lock, std::chrono::seconds(5), [&]
            {
                return siblingStarted;
            }), "The held audio task did not start.");
        }

        Expect(session->StartTask("failing-decoder", [](std::stop_token,
            MoviePlaybackSession::TaskCompletion)
        {
            throw std::runtime_error("sibling decoder failure");
        }), "The session refused its decoder task.");
        while (!session->HasFailure())
        {
            const std::uint64_t version = session->ProgressVersion();
            if (!session->WaitForProgress(version, std::chrono::seconds(5)))
            {
                Expect(false, "The decoder failure did not reach the session.");
            }
        }

        Expect(session->StopRequested(), "A task failure did not stop the session.");
        Expect(siblingToken.stop_requested(), "A task failure did not cancel sibling work.");
        Expect(session->RemainingTasks() == 1,
            "The session treated a still-running sibling as complete after a decoder failure.");

        MoviePlaybackSession::TaskCompletion finish;
        {
            std::lock_guard<std::mutex> lock(mutex);
            finish = finishSibling;
        }
        finish({});
        session->WaitForDrain();
        Expect(session->RemainingTasks() == 0, "Failure cleanup did not wait for sibling completion.");
    }

    void CheckQueuedTaskCancellation()
    {
        const unsigned int workerCount = std::max(1U, std::thread::hardware_concurrency());
        std::shared_ptr<MoviePlaybackSession> session = MoviePlaybackSession::Create(
            MoviePlaybackSession::NextSceneId(), 13, MoviePlaybackPolicy::Normal);
        std::mutex mutex;
        std::condition_variable condition;
        unsigned int workersStarted = 0;
        bool releaseWorkers = false;
        std::atomic<bool> queuedStarterRan{false};
        const auto workerStartDeadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(15);

        for (unsigned int i = 0; i < workerCount; ++i)
        {
            const std::string taskName = "pool-blocker-" + std::to_string(i);
            Expect(session->StartTask(taskName, [&](std::stop_token,
                MoviePlaybackSession::TaskCompletion completion)
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++workersStarted;
                condition.notify_all();
                condition.wait(lock, [&] { return releaseWorkers; });
                lock.unlock();
                completion({});
            }), "The session refused a worker-blocking task.");
            std::unique_lock<std::mutex> lock(mutex);
            const bool workerStarted = condition.wait_until(lock, workerStartDeadline, [&]
            {
                return workersStarted > i;
            });
            if (!workerStarted)
            {
                lock.unlock();
                session->RequestStop("test-timeout", "CheckQueuedTaskCancellation");
                {
                    std::lock_guard<std::mutex> releaseLock(mutex);
                    releaseWorkers = true;
                }
                condition.notify_all();
                session->WaitForDrain();
                Expect(false, "The test could not occupy every TaskRun worker.");
            }
        }

        Expect(session->StartTask("queued-before-start", [&](std::stop_token,
            MoviePlaybackSession::TaskCompletion completion)
        {
            queuedStarterRan.store(true, std::memory_order_release);
            completion({});
        }), "The session refused the task queued behind occupied workers.");
        session->RequestStop("test-queued-cancel", "CheckQueuedTaskCancellation");
        {
            std::lock_guard<std::mutex> lock(mutex);
            releaseWorkers = true;
        }
        condition.notify_all();
        session->WaitForDrain();

        Expect(!queuedStarterRan.load(std::memory_order_acquire),
            "A queued task's starter ran after its session was stopped.");
        Expect(session->RemainingTasks() == 0,
            "A queued task remained registered after the worker pool drained.");
    }
}

int main()
{
    try
    {
        CheckCoroutineDrain();
        CheckActiveCallDrainBeforeRelease();
        CheckTaskCompletionAndFailure();
        CheckSessionIsolationAndStopRules();
        CheckFadeCompletionIsSessionScopedAndStoppedEventsAreDiscarded();
        CheckConcurrentFadeCompletionAndStop();
        CheckFailureStopsSiblingWork();
        CheckQueuedTaskCancellation();
        std::cout << "MoviePlaybackSession PASS; final_suspend drain; active-call drain before release; session isolation; stop admission; queued cancellation; sibling cancellation; async failure capture; zero remaining tasks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

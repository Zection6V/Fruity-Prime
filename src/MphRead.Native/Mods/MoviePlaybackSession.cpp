#include "MoviePlaybackSession.hpp"

#include "DebugLog.hpp"
#include "../NativeRuntime/System/Tasks.hpp"

#include <atomic>
#include <chrono>
#include <sstream>
#include <utility>

namespace MphRead::Mods
{
    namespace
    {
        std::atomic<std::uint64_t> NextMovieSessionId{1};
        std::atomic<std::uint64_t> NextMovieSceneId{1};

        [[nodiscard]] const char* PolicyName(MoviePlaybackPolicy policy) noexcept
        {
            switch (policy)
            {
            case MoviePlaybackPolicy::Normal: return "normal";
            case MoviePlaybackPolicy::SuppressForAudit: return "audit";
            case MoviePlaybackPolicy::SuppressForHeadless: return "headless";
            }
            return "unknown";
        }

        [[nodiscard]] std::string ExceptionMessage(std::exception_ptr exception)
        {
            if (!exception)
            {
                return {};
            }
            try
            {
                std::rethrow_exception(exception);
            }
            catch (const std::exception& ex)
            {
                return ex.what();
            }
            catch (...)
            {
                return "unknown exception";
            }
        }

        [[nodiscard]] std::string SessionPrefix(
            std::uint64_t sceneId, std::uint64_t sessionId, std::int32_t movieId)
        {
            return "scene_id=" + std::to_string(sceneId)
                + " session_id=" + std::to_string(sessionId)
                + " movie_id=" + std::to_string(movieId);
        }
    }

    std::shared_ptr<MoviePlaybackSession> MoviePlaybackSession::Create(
        std::uint64_t sceneId, std::int32_t movieId, MoviePlaybackPolicy policy)
    {
        const std::uint64_t sessionId = NextMovieSessionId.fetch_add(1, std::memory_order_relaxed);
        auto session = std::shared_ptr<MoviePlaybackSession>(
            new MoviePlaybackSession(sessionId, sceneId, movieId, policy));
        DebugLog::Line("movie", "movie-start " + SessionPrefix(sceneId, sessionId, movieId)
            + " policy=" + PolicyName(policy));
        return session;
    }

    std::uint64_t MoviePlaybackSession::NextSceneId() noexcept
    {
        return NextMovieSceneId.fetch_add(1, std::memory_order_relaxed);
    }

    void MoviePlaybackSession::ArmOpenAlCallGateForDiagnostics()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_openAlCallGateEntered || _stopSource.stop_requested())
            {
                return;
            }
            _openAlCallGateArmed = true;
        }
        DebugLog::Line("movie", "movie-openal-call-probe-armed "
            + SessionPrefix(_sceneId, _sessionId, _movieId));
    }

    void MoviePlaybackSession::HoldAtOpenAlCallGateForDiagnostics(std::string_view operation)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_openAlCallGateArmed || _openAlCallGateEntered)
            {
                return;
            }
            _openAlCallGateEntered = true;
            ++_progressVersion;
        }
        _condition.notify_all();
        const std::string prefix = SessionPrefix(_sceneId, _sessionId, _movieId);
        DebugLog::Line("movie", "movie-openal-call-probe-entered " + prefix
            + " operation=" + std::string(operation));

        {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.wait(lock, [this]() { return _stopSource.stop_requested(); });
        }
        DebugLog::Line("movie", "movie-openal-call-probe-released " + prefix
            + " operation=" + std::string(operation) + " stop_requested=1");
    }

    void MoviePlaybackSession::MarkOpenAlCallReturnedForDiagnostics(std::string_view operation)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_openAlCallGateEntered)
            {
                return;
            }
            _openAlCallReturned = true;
        }
        DebugLog::Line("movie", "movie-openal-call-probe-call-returned "
            + SessionPrefix(_sceneId, _sessionId, _movieId)
            + " operation=" + std::string(operation));
    }

    bool MoviePlaybackSession::OpenAlCallGateEnteredForDiagnostics() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _openAlCallGateEntered;
    }

    bool MoviePlaybackSession::OpenAlCallReturnedForDiagnostics() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _openAlCallReturned;
    }

    void MoviePlaybackSession::LogSuppressed(
        std::uint64_t sceneId, std::int32_t movieId,
        MoviePlaybackPolicy policy, std::string_view afterAction)
    {
        const std::uint64_t sessionId = NextMovieSessionId.fetch_add(1, std::memory_order_relaxed);
        DebugLog::Line("movie", "movie-suppressed "
            + SessionPrefix(sceneId, sessionId, movieId)
            + " policy=" + PolicyName(policy)
            + " after_action=" + std::string(afterAction)
            + " remaining_tasks=0");
    }

    MoviePlaybackSession::MoviePlaybackSession(
        std::uint64_t sessionId, std::uint64_t sceneId,
        std::int32_t movieId, MoviePlaybackPolicy) noexcept
        : _sessionId(sessionId),
          _sceneId(sceneId),
          _movieId(movieId),
          _startedAt(std::chrono::steady_clock::now())
    {
    }

    std::uint64_t MoviePlaybackSession::Id() const noexcept { return _sessionId; }
    std::uint64_t MoviePlaybackSession::SceneId() const noexcept { return _sceneId; }
    std::int32_t MoviePlaybackSession::MovieId() const noexcept { return _movieId; }
    std::stop_token MoviePlaybackSession::Token() const noexcept { return _stopSource.get_token(); }

    bool MoviePlaybackSession::StartTask(std::string taskName, TaskStarter starter)
    {
        if (!starter)
        {
            return false;
        }

        std::uint64_t taskId = 0;
        std::size_t remaining = 0;
        const std::stop_token token = _stopSource.get_token();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == State::Stopping || _state == State::Stopped || _state == State::Failed)
            {
                return false;
            }
            if (_state == State::Created)
            {
                _state = State::Starting;
            }
            taskId = _nextTaskId++;
            _tasks.emplace(taskId, taskName);
            remaining = _tasks.size();
            ++_progressVersion;
        }
        _condition.notify_all();

        TaskCompletion completion{};
        try
        {
            const std::string prefix = SessionPrefix(_sceneId, _sessionId, _movieId);
            DebugLog::Line("movie", "movie-task-start " + prefix
                + " task=" + taskName
                + " task_id=" + std::to_string(taskId)
                + " in_flight=" + std::to_string(remaining));

            std::shared_ptr<MoviePlaybackSession> self = shared_from_this();
            auto completionCalled = std::make_shared<std::atomic<bool>>(false);
            completion = [self, completionCalled, taskId](std::exception_ptr exception)
            {
                if (!completionCalled->exchange(true, std::memory_order_acq_rel))
                {
                    self->CompleteTask(taskId, std::move(exception));
                }
            };

            NativeRuntime::TaskRun(
                [token, starter = std::move(starter), completion]() mutable
                {
                    if (token.stop_requested())
                    {
                        completion({});
                        return;
                    }
                    try
                    {
                        starter(token, completion);
                    }
                    catch (...)
                    {
                        completion(std::current_exception());
                    }
                });
            return true;
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            if (completion)
            {
                completion(failure);
            }
            else
            {
                CompleteTask(taskId, failure);
            }
            return false;
        }
    }

    void MoviePlaybackSession::MarkRunning() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_state == State::Starting)
            {
                _state = State::Running;
                ++_progressVersion;
            }
        }
        _condition.notify_all();
    }

    void MoviePlaybackSession::CompleteTask(
        std::uint64_t taskId, std::exception_ptr exception) noexcept
    {
        std::string taskName;
        std::string failureMessage;
        std::size_t remaining = 0;
        bool failed = exception != nullptr;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const auto task = _tasks.find(taskId);
            if (task == _tasks.end())
            {
                return;
            }
            taskName = std::move(task->second);
            _tasks.erase(task);
            remaining = _tasks.size();
            if (exception && !_failure)
            {
                _failure = exception;
                _state = State::Failed;
                failureMessage = ExceptionMessage(exception);
            }
            ++_progressVersion;
        }

        const std::string prefix = SessionPrefix(_sceneId, _sessionId, _movieId);
        const char* status = failed ? "failed" : (_stopSource.stop_requested() ? "cancelled" : "completed");
        DebugLog::Line("movie", "movie-task-complete " + prefix
            + " task=" + taskName
            + " task_id=" + std::to_string(taskId)
            + " status=" + status
            + " in_flight=" + std::to_string(remaining)
            + (failureMessage.empty() ? std::string() : " error=" + failureMessage));
        _condition.notify_all();
        if (failed)
        {
            RequestStop("task-failure", taskName);
        }
    }

    void MoviePlaybackSession::RequestStop(
        std::string_view reason, std::string_view requester) noexcept
    {
        bool logStop = false;
        std::size_t remaining = 0;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            remaining = _tasks.size();
            if (!_stopLogged)
            {
                _stopLogged = true;
                logStop = true;
            }
            if (_state != State::Failed && _state != State::Stopped)
            {
                _state = State::Stopping;
            }
            _fadeCompletionPending = false;
        }
        const bool requested = _stopSource.request_stop();
        if (logStop && requested)
        {
            DebugLog::Line("movie", "movie-stop-request "
                + SessionPrefix(_sceneId, _sessionId, _movieId)
                + " reason=" + std::string(reason)
                + " requester=" + std::string(requester)
                + " in_flight=" + std::to_string(remaining));
        }
        _condition.notify_all();
    }

    void MoviePlaybackSession::WaitForDrain()
    {
        const auto started = std::chrono::steady_clock::now();
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.wait(lock, [this]() { return _tasks.empty(); });
            _fadeCompletionPending = false;
            if (_state != State::Failed)
            {
                _state = State::Stopped;
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        DebugLog::Line("movie", "movie-drained "
            + SessionPrefix(_sceneId, _sessionId, _movieId)
            + " remaining_tasks=0 elapsed_ms=" + std::to_string(elapsed));
    }

    void MoviePlaybackSession::PostFadeCompletion() noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state != State::Stopping && _state != State::Stopped && _state != State::Failed)
        {
            _fadeCompletionPending = true;
        }
    }

    bool MoviePlaybackSession::TakeFadeCompletion() noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const bool pending = _fadeCompletionPending;
        _fadeCompletionPending = false;
        const bool accepted = pending
            && _state != State::Stopping
            && _state != State::Stopped
            && _state != State::Failed;
        if (accepted)
        {
            ++_fadeCompletionConsumeCount;
        }
        return accepted;
    }

    bool MoviePlaybackSession::FadeCompletionPending() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _fadeCompletionPending;
    }

    std::uint64_t MoviePlaybackSession::FadeCompletionConsumeCount() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _fadeCompletionConsumeCount;
    }

    void MoviePlaybackSession::NotifyProgress() noexcept
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            ++_progressVersion;
        }
        _condition.notify_all();
    }

    std::uint64_t MoviePlaybackSession::ProgressVersion() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _progressVersion;
    }

    void MoviePlaybackSession::WaitForProgress(std::uint64_t version)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this, version]()
        {
            return _progressVersion != version
                || _state == State::Stopping
                || _state == State::Stopped
                || _state == State::Failed
                || _tasks.empty();
        });
    }

    bool MoviePlaybackSession::WaitForProgress(
        std::uint64_t version, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _condition.wait_for(lock, timeout, [this, version]()
        {
            return _progressVersion != version
                || _state == State::Stopping
                || _state == State::Stopped
                || _state == State::Failed
                || _tasks.empty();
        });
    }

    std::size_t MoviePlaybackSession::RemainingTasks() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tasks.size();
    }

    bool MoviePlaybackSession::StopRequested() const noexcept
    {
        return _stopSource.stop_requested();
    }

    bool MoviePlaybackSession::HasFailure() const noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return static_cast<bool>(_failure);
    }

    bool MoviePlaybackSession::TryObserveFailure() noexcept
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_failure || _failureObserved)
        {
            return false;
        }
        _failureObserved = true;
        return true;
    }

    std::string MoviePlaybackSession::FailureMessage() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return ExceptionMessage(_failure);
    }
}

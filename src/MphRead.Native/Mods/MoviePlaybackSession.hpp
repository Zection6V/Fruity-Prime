#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>

namespace MphRead::Mods
{
    enum class MoviePlaybackPolicy : std::uint8_t
    {
        Normal,
        SuppressForAudit,
        SuppressForHeadless
    };

    // Owns the lifetime of one movie's decoder and playback coroutines. A task
    // remains registered until its coroutine reaches final_suspend.
    class MoviePlaybackSession final : public std::enable_shared_from_this<MoviePlaybackSession>
    {
    public:
        using TaskCompletion = std::function<void(std::exception_ptr)>;
        using TaskStarter = std::function<void(std::stop_token, TaskCompletion)>;

        [[nodiscard]] static std::shared_ptr<MoviePlaybackSession> Create(
            std::uint64_t sceneId, std::int32_t movieId, MoviePlaybackPolicy policy);
        [[nodiscard]] static std::uint64_t NextSceneId() noexcept;
        static void LogSuppressed(std::uint64_t sceneId, std::int32_t movieId,
            MoviePlaybackPolicy policy, std::string_view afterAction);

        void ArmOpenAlCallGateForDiagnostics();
        void HoldAtOpenAlCallGateForDiagnostics(std::string_view operation);
        void MarkOpenAlCallReturnedForDiagnostics(std::string_view operation);
        [[nodiscard]] bool OpenAlCallGateEnteredForDiagnostics() const noexcept;
        [[nodiscard]] bool OpenAlCallReturnedForDiagnostics() const noexcept;

        [[nodiscard]] std::uint64_t Id() const noexcept;
        [[nodiscard]] std::uint64_t SceneId() const noexcept;
        [[nodiscard]] std::int32_t MovieId() const noexcept;
        [[nodiscard]] std::stop_token Token() const noexcept;

        [[nodiscard]] bool StartTask(std::string taskName, TaskStarter starter);
        void MarkRunning() noexcept;
        void RequestStop(std::string_view reason, std::string_view requester) noexcept;
        void WaitForDrain();
        void PostFadeCompletion() noexcept;
        [[nodiscard]] bool TakeFadeCompletion() noexcept;
        [[nodiscard]] bool FadeCompletionPending() const noexcept;
        [[nodiscard]] std::uint64_t FadeCompletionConsumeCount() const noexcept;
        void NotifyProgress() noexcept;
        [[nodiscard]] std::uint64_t ProgressVersion() const noexcept;
        void WaitForProgress(std::uint64_t version);
        [[nodiscard]] bool WaitForProgress(std::uint64_t version,
            std::chrono::milliseconds timeout);
        [[nodiscard]] std::size_t RemainingTasks() const noexcept;
        [[nodiscard]] bool StopRequested() const noexcept;
        [[nodiscard]] bool HasFailure() const noexcept;
        [[nodiscard]] bool TryObserveFailure() noexcept;
        [[nodiscard]] std::string FailureMessage() const;

        MoviePlaybackSession(const MoviePlaybackSession&) = delete;
        MoviePlaybackSession& operator=(const MoviePlaybackSession&) = delete;

    private:
        enum class State : std::uint8_t
        {
            Created,
            Starting,
            Running,
            Stopping,
            Stopped,
            Failed
        };

        MoviePlaybackSession(std::uint64_t sessionId, std::uint64_t sceneId,
            std::int32_t movieId, MoviePlaybackPolicy policy) noexcept;
        void CompleteTask(std::uint64_t taskId, std::exception_ptr exception) noexcept;

        const std::uint64_t _sessionId;
        const std::uint64_t _sceneId;
        const std::int32_t _movieId;
        std::stop_source _stopSource;
        mutable std::mutex _mutex;
        std::condition_variable _condition;
        State _state = State::Created;
        bool _fadeCompletionPending = false;
        std::uint64_t _fadeCompletionConsumeCount = 0;
        std::unordered_map<std::uint64_t, std::string> _tasks;
        std::uint64_t _nextTaskId = 1;
        std::uint64_t _progressVersion = 0;
        std::exception_ptr _failure{};
        bool _failureObserved = false;
        bool _stopLogged = false;
        bool _openAlCallGateArmed = false;
        bool _openAlCallGateEntered = false;
        bool _openAlCallReturned = false;
        std::chrono::steady_clock::time_point _startedAt;
    };
}

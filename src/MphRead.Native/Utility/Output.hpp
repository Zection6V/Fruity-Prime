#pragma once

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace MphRead
{
    class NullReferenceException final : public std::runtime_error
    {
    public:
        explicit NullReferenceException(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    class InvalidOperationException final : public std::runtime_error
    {
    public:
        explicit InvalidOperationException(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    class SemaphoreFullException final : public std::runtime_error
    {
    public:
        explicit SemaphoreFullException(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    namespace Detail
    {
        class NullableGuid;

        template <typename T>
        class TaskState final
        {
        public:
            void SetResult(T value)
            {
                std::vector<std::coroutine_handle<>> continuations;
                {
                    std::lock_guard<std::mutex> guard(mutex_);
                    value_.emplace(std::move(value));
                    completed_ = true;
                    continuations.swap(continuations_);
                }
                condition_.notify_all();
                for (std::coroutine_handle<> continuation : continuations)
                {
                    continuation.resume();
                }
            }

            void SetException(std::exception_ptr exception)
            {
                std::vector<std::coroutine_handle<>> continuations;
                {
                    std::lock_guard<std::mutex> guard(mutex_);
                    exception_ = std::move(exception);
                    completed_ = true;
                    continuations.swap(continuations_);
                }
                condition_.notify_all();
                for (std::coroutine_handle<> continuation : continuations)
                {
                    continuation.resume();
                }
            }

            [[nodiscard]] bool IsCompleted() const
            {
                std::lock_guard<std::mutex> guard(mutex_);
                return completed_;
            }

            [[nodiscard]] bool AddContinuation(std::coroutine_handle<> continuation)
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (completed_)
                {
                    return false;
                }
                continuations_.push_back(continuation);
                return true;
            }

            [[nodiscard]] T GetResult() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return completed_; });
                if (exception_)
                {
                    std::rethrow_exception(exception_);
                }
                return *value_;
            }

        private:
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
            bool completed_ = false;
            std::optional<T> value_;
            std::exception_ptr exception_;
            std::vector<std::coroutine_handle<>> continuations_;
        };

        template <>
        class TaskState<void> final
        {
        public:
            void SetResult()
            {
                std::vector<std::coroutine_handle<>> continuations;
                {
                    std::lock_guard<std::mutex> guard(mutex_);
                    completed_ = true;
                    continuations.swap(continuations_);
                }
                condition_.notify_all();
                for (std::coroutine_handle<> continuation : continuations)
                {
                    continuation.resume();
                }
            }

            void SetException(std::exception_ptr exception)
            {
                std::vector<std::coroutine_handle<>> continuations;
                {
                    std::lock_guard<std::mutex> guard(mutex_);
                    exception_ = std::move(exception);
                    completed_ = true;
                    continuations.swap(continuations_);
                }
                condition_.notify_all();
                for (std::coroutine_handle<> continuation : continuations)
                {
                    continuation.resume();
                }
            }

            [[nodiscard]] bool IsCompleted() const
            {
                std::lock_guard<std::mutex> guard(mutex_);
                return completed_;
            }

            [[nodiscard]] bool AddContinuation(std::coroutine_handle<> continuation)
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (completed_)
                {
                    return false;
                }
                continuations_.push_back(continuation);
                return true;
            }

            void GetResult() const
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() { return completed_; });
                if (exception_)
                {
                    std::rethrow_exception(exception_);
                }
            }

        private:
            mutable std::mutex mutex_;
            mutable std::condition_variable condition_;
            bool completed_ = false;
            std::exception_ptr exception_;
            std::vector<std::coroutine_handle<>> continuations_;
        };
    }

    template <typename T>
    class Task final
    {
    public:
        struct promise_type final
        {
            std::shared_ptr<Detail::TaskState<T>> State
                = std::make_shared<Detail::TaskState<T>>();

            [[nodiscard]] Task get_return_object()
            {
                return Task(State);
            }

            [[nodiscard]] std::suspend_never initial_suspend() const noexcept
            {
                return {};
            }

            [[nodiscard]] std::suspend_never final_suspend() const noexcept
            {
                return {};
            }

            void return_value(T value)
            {
                State->SetResult(std::move(value));
            }

            void unhandled_exception() noexcept
            {
                State->SetException(std::current_exception());
            }
        };

        class Awaiter final
        {
        public:
            explicit Awaiter(std::shared_ptr<Detail::TaskState<T>> state) noexcept
                : state_(std::move(state))
            {
            }

            [[nodiscard]] bool await_ready() const
            {
                return state_->IsCompleted();
            }

            [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation)
            {
                return state_->AddContinuation(continuation);
            }

            [[nodiscard]] T await_resume() const
            {
                return state_->GetResult();
            }

        private:
            std::shared_ptr<Detail::TaskState<T>> state_;
        };

        Task(const Task&) = default;
        Task(Task&&) noexcept = default;
        Task& operator=(const Task&) = default;
        Task& operator=(Task&&) noexcept = default;

        [[nodiscard]] bool IsCompleted() const
        {
            return state_->IsCompleted();
        }

        [[nodiscard]] T GetResult() const
        {
            return state_->GetResult();
        }

        [[nodiscard]] Awaiter operator co_await() const noexcept
        {
            return Awaiter(state_);
        }

    private:
        explicit Task(std::shared_ptr<Detail::TaskState<T>> state) noexcept
            : state_(std::move(state))
        {
        }

        std::shared_ptr<Detail::TaskState<T>> state_;
    };

    template <>
    class Task<void> final
    {
    public:
        struct promise_type final
        {
            std::shared_ptr<Detail::TaskState<void>> State
                = std::make_shared<Detail::TaskState<void>>();

            [[nodiscard]] Task get_return_object()
            {
                return Task(State);
            }

            [[nodiscard]] std::suspend_never initial_suspend() const noexcept
            {
                return {};
            }

            [[nodiscard]] std::suspend_never final_suspend() const noexcept
            {
                return {};
            }

            void return_void()
            {
                State->SetResult();
            }

            void unhandled_exception() noexcept
            {
                State->SetException(std::current_exception());
            }
        };

        class Awaiter final
        {
        public:
            explicit Awaiter(std::shared_ptr<Detail::TaskState<void>> state) noexcept
                : state_(std::move(state))
            {
            }

            [[nodiscard]] bool await_ready() const
            {
                return state_->IsCompleted();
            }

            [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation)
            {
                return state_->AddContinuation(continuation);
            }

            void await_resume() const
            {
                state_->GetResult();
            }

        private:
            std::shared_ptr<Detail::TaskState<void>> state_;
        };

        Task(const Task&) = default;
        Task(Task&&) noexcept = default;
        Task& operator=(const Task&) = default;
        Task& operator=(Task&&) noexcept = default;

        [[nodiscard]] bool IsCompleted() const
        {
            return state_->IsCompleted();
        }

        void GetResult() const
        {
            state_->GetResult();
        }

        [[nodiscard]] Awaiter operator co_await() const noexcept
        {
            return Awaiter(state_);
        }

        [[nodiscard]] static Task Run(std::function<void()> action)
        {
            auto state = std::make_shared<Detail::TaskState<void>>();
            std::thread worker([state, action = std::move(action)]() mutable
            {
                try
                {
                    action();
                    state->SetResult();
                }
                catch (...)
                {
                    state->SetException(std::current_exception());
                }
            });
            worker.detach();
            return Task(std::move(state));
        }

    private:
        explicit Task(std::shared_ptr<Detail::TaskState<void>> state) noexcept
            : state_(std::move(state))
        {
        }

        std::shared_ptr<Detail::TaskState<void>> state_;
    };

    class Guid final
    {
    public:
        Guid() = default;

        [[nodiscard]] static Guid NewGuid();

        friend bool operator==(const Guid&, const Guid&) = default;

    private:
        Guid(std::uint64_t low, std::uint64_t high) noexcept
            : low_(low), high_(high)
        {
        }

        std::uint64_t low_ = 0;
        std::uint64_t high_ = 0;

        friend class Detail::NullableGuid;
    };

    namespace Detail
    {
        class NullableGuid final
        {
        public:
            NullableGuid() = default;

            [[nodiscard]] std::optional<Guid> Load() const noexcept;
            void Store(std::optional<Guid> value) noexcept;

        private:
            std::atomic<bool> hasValue_{false};
            std::atomic<std::uint64_t> low_{0};
            std::atomic<std::uint64_t> high_{0};
        };

        class SemaphoreSlim final
        {
        public:
            class Awaiter final
            {
            public:
                explicit Awaiter(SemaphoreSlim* semaphore) noexcept
                    : semaphore_(semaphore)
                {
                }

                [[nodiscard]] bool await_ready() const noexcept
                {
                    return false;
                }

                [[nodiscard]] bool await_suspend(std::coroutine_handle<> continuation)
                {
                    return semaphore_->SuspendOrAcquire(continuation);
                }

                void await_resume() const noexcept
                {
                }

            private:
                SemaphoreSlim* semaphore_;
            };

            SemaphoreSlim(std::int32_t initialCount, std::int32_t maxCount);
            ~SemaphoreSlim();

            SemaphoreSlim(const SemaphoreSlim&) = delete;
            SemaphoreSlim& operator=(const SemaphoreSlim&) = delete;
            SemaphoreSlim(SemaphoreSlim&&) = delete;
            SemaphoreSlim& operator=(SemaphoreSlim&&) = delete;

            [[nodiscard]] Awaiter WaitAsync() noexcept
            {
                return Awaiter(this);
            }

            void Wait();
            void Release();

        private:
            struct Impl;

            [[nodiscard]] bool SuspendOrAcquire(std::coroutine_handle<> continuation);

            std::unique_ptr<Impl> impl_;
        };

        class CancellationToken final
        {
        public:
            CancellationToken() = default;

            [[nodiscard]] bool IsCancellationRequested() const noexcept;

        private:
            explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state) noexcept
                : state_(std::move(state))
            {
            }

            std::shared_ptr<std::atomic<bool>> state_;

            friend class CancellationTokenSource;
        };

        class CancellationTokenSource final
        {
        public:
            CancellationTokenSource();

            [[nodiscard]] CancellationToken Token() const noexcept;
            void Cancel() noexcept;

        private:
            std::shared_ptr<std::atomic<bool>> state_;
        };
    }

    class Output final
    {
    public:
        enum class Operation : std::int32_t
        {
            Write = 0,
            Read = 1,
            Clear = 2
        };

        Output() = delete;

        [[nodiscard]] static Task<void> Begin();
        [[nodiscard]] static Task<Guid> StartBatch();
        [[nodiscard]] static Task<void> EndBatch();
        [[nodiscard]] static Task<void> Write(std::optional<Guid> guid = std::nullopt);
        [[nodiscard]] static Task<void> Write(
            std::string message, std::optional<Guid> guid = std::nullopt);
        [[nodiscard]] static Task<void> Clear(std::optional<Guid> guid = std::nullopt);
        [[nodiscard]] static Task<std::string> Read(
            std::optional<std::string> message = std::nullopt,
            std::optional<Guid> guid = std::nullopt);
        [[nodiscard]] static Task<void> End();

    private:
        struct QueueItem final
        {
            Operation OperationValue = Operation::Write;
            std::optional<std::string> Message;

            QueueItem() = default;
            QueueItem(Operation operation, std::optional<std::string> message)
                : OperationValue(operation), Message(std::move(message))
            {
            }
        };

        struct QueueInput final
        {
            std::int32_t Count = 0;
            std::string Message;

            QueueInput() = default;
            QueueInput(std::int32_t count, std::string message)
                : Count(count), Message(std::move(message))
            {
            }
        };

        [[nodiscard]] static Task<std::string> DoRead(std::int32_t count);
        [[nodiscard]] static Task<void> Run(Detail::CancellationToken token);

        static bool initialized_;
        static Detail::SemaphoreSlim lock_;
        static Detail::SemaphoreSlim batchLock_;
        static std::unique_ptr<Detail::CancellationTokenSource> cts_;
        static std::unique_ptr<std::vector<QueueItem>> items_;
        static std::unique_ptr<std::vector<QueueInput>> input_;
        static Detail::NullableGuid batchGuid_;
    };
}

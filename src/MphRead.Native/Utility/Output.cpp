#include "Output.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <locale>
#include <random>
#include <system_error>


#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::ConsoleWrite;
using ::MphRead::NativeRuntime::ConsoleWriteLine;
using ::MphRead::NativeRuntime::ConsoleWriteLineNullable;

namespace
{
    using namespace std::chrono_literals;

    class Delay100Awaiter final
    {
    public:
        [[nodiscard]] bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> continuation) const
        {
            std::thread delay([continuation]()
            {
                std::this_thread::sleep_for(100ms);
                continuation.resume();
            });
            delay.detach();
        }

        void await_resume() const noexcept
        {
        }
    };

    void ResumeAsynchronously(std::coroutine_handle<> continuation)
    {
        std::thread worker([continuation]()
        {
            continuation.resume();
        });
        worker.detach();
    }
}

namespace MphRead::Detail
{
    struct SemaphoreSlim::Impl final
    {
        struct BlockingWaiter final
        {
            std::mutex Mutex;
            std::condition_variable Condition;
            bool Ready = false;
        };

        struct Waiter final
        {
            std::coroutine_handle<> Continuation{};
            std::shared_ptr<BlockingWaiter> Blocking;
        };

        std::int32_t Count;
        const std::int32_t MaxCount;
        std::mutex Mutex;
        std::deque<Waiter> Waiters;

        Impl(std::int32_t initialCount, std::int32_t maxCount)
            : Count(initialCount), MaxCount(maxCount)
        {
        }
    };

    SemaphoreSlim::SemaphoreSlim(std::int32_t initialCount, std::int32_t maxCount)
        : impl_(std::make_unique<Impl>(initialCount, maxCount))
    {
        if (maxCount <= 0 || initialCount < 0 || initialCount > maxCount)
        {
            throw std::invalid_argument("Invalid semaphore count.");
        }
    }

    SemaphoreSlim::~SemaphoreSlim() = default;

    bool SemaphoreSlim::SuspendOrAcquire(std::coroutine_handle<> continuation)
    {
        std::lock_guard<std::mutex> guard(impl_->Mutex);
        if (impl_->Count > 0)
        {
            --impl_->Count;
            return false;
        }
        impl_->Waiters.push_back(Impl::Waiter{continuation, nullptr});
        return true;
    }

    void SemaphoreSlim::Wait()
    {
        std::shared_ptr<Impl::BlockingWaiter> waiter;
        {
            std::lock_guard<std::mutex> guard(impl_->Mutex);
            if (impl_->Count > 0)
            {
                --impl_->Count;
                return;
            }
            waiter = std::make_shared<Impl::BlockingWaiter>();
            impl_->Waiters.push_back(Impl::Waiter{{}, waiter});
        }

        std::unique_lock<std::mutex> lock(waiter->Mutex);
        waiter->Condition.wait(lock, [&waiter]() { return waiter->Ready; });
    }

    void SemaphoreSlim::Release()
    {
        Impl::Waiter waiter;
        bool hasWaiter = false;
        {
            std::lock_guard<std::mutex> guard(impl_->Mutex);
            if (!impl_->Waiters.empty())
            {
                waiter = std::move(impl_->Waiters.front());
                impl_->Waiters.pop_front();
                hasWaiter = true;
            }
            else
            {
                if (impl_->Count == impl_->MaxCount)
                {
                    throw SemaphoreFullException(
                        "Adding the specified count to the semaphore would cause it to exceed its maximum count.");
                }
                ++impl_->Count;
            }
        }

        if (!hasWaiter)
        {
            return;
        }

        if (waiter.Blocking)
        {
            {
                std::lock_guard<std::mutex> guard(waiter.Blocking->Mutex);
                waiter.Blocking->Ready = true;
            }
            waiter.Blocking->Condition.notify_one();
        }
        else
        {
            ResumeAsynchronously(waiter.Continuation);
        }
    }

    CancellationTokenSource::CancellationTokenSource()
        : state_(std::make_shared<std::atomic<bool>>(false))
    {
    }

    CancellationToken CancellationTokenSource::Token() const noexcept
    {
        return CancellationToken(state_);
    }

    void CancellationTokenSource::Cancel() noexcept
    {
        state_->store(true, std::memory_order_release);
    }

    bool CancellationToken::IsCancellationRequested() const noexcept
    {
        return state_ != nullptr && state_->load(std::memory_order_acquire);
    }
}

namespace MphRead
{
    Guid Guid::NewGuid()
    {
        std::array<unsigned char, 16> bytes{};
        std::random_device random;
        for (unsigned char& value : bytes)
        {
            value = static_cast<unsigned char>(random());
        }

        bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0FU) | 0x40U);
        bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3FU) | 0x80U);

        std::uint64_t low = 0;
        std::uint64_t high = 0;
        std::memcpy(&low, bytes.data(), sizeof(low));
        std::memcpy(&high, bytes.data() + sizeof(low), sizeof(high));
        return Guid(low, high);
    }
}

namespace MphRead::Detail
{
    std::optional<Guid> NullableGuid::Load() const noexcept
    {
        if (!hasValue_.load(std::memory_order_relaxed))
        {
            return std::nullopt;
        }
        return Guid(
            low_.load(std::memory_order_relaxed),
            high_.load(std::memory_order_relaxed));
    }

    void NullableGuid::Store(std::optional<Guid> value) noexcept
    {
        if (!value.has_value())
        {
            hasValue_.store(false, std::memory_order_relaxed);
            return;
        }
        low_.store(value->low_, std::memory_order_relaxed);
        high_.store(value->high_, std::memory_order_relaxed);
        hasValue_.store(true, std::memory_order_relaxed);
    }
}

namespace MphRead
{
    bool Output::initialized_ = false;
    Detail::SemaphoreSlim Output::lock_(1, 1);
    Detail::SemaphoreSlim Output::batchLock_(1, 1);
    std::unique_ptr<Detail::CancellationTokenSource> Output::cts_;
    std::unique_ptr<std::vector<Output::QueueItem>> Output::items_;
    std::unique_ptr<std::vector<Output::QueueInput>> Output::input_;
    Detail::NullableGuid Output::batchGuid_;

    Task<void> Output::Begin()
    {
        co_await lock_.WaitAsync();
        if (!initialized_)
        {
            initialized_ = true;
            items_ = std::make_unique<std::vector<QueueItem>>();
            input_ = std::make_unique<std::vector<QueueInput>>();
            cts_ = std::make_unique<Detail::CancellationTokenSource>();
            (void)Task<void>::Run([]()
            {
                ::MphRead::NativeRuntime::UseInvariantCultureOnThisThread();
                if (!cts_)
                {
                    throw NullReferenceException("Object reference not set to an instance of an object: _cts.");
                }
                Run(cts_->Token()).GetResult();
            });
        }
        lock_.Release();
        co_return;
    }

    Task<Guid> Output::StartBatch()
    {
        co_await batchLock_.WaitAsync();
        batchGuid_.Store(Guid::NewGuid());
        std::optional<Guid> value = batchGuid_.Load();
        if (!value.has_value())
        {
            throw InvalidOperationException("Nullable object must have a value.");
        }
        co_return *value;
    }

    Task<void> Output::EndBatch()
    {
        co_await lock_.WaitAsync();
        batchGuid_.Store(std::nullopt);
        batchLock_.Release();
        lock_.Release();
        co_return;
    }

    Task<void> Output::Write(std::optional<Guid> guid)
    {
        co_await Write(std::string(), guid);
        co_return;
    }

    Task<void> Output::Write(std::string message, std::optional<Guid> guid)
    {
        co_await lock_.WaitAsync();
        if (guid != batchGuid_.Load())
        {
            co_await batchLock_.WaitAsync();
        }
        if (!items_)
        {
            throw NullReferenceException("Object reference not set to an instance of an object: _items.");
        }
        items_->emplace_back(Operation::Write, std::optional<std::string>(std::move(message)));
        if (guid != batchGuid_.Load())
        {
            batchLock_.Release();
        }
        lock_.Release();
        co_return;
    }

    Task<void> Output::Clear(std::optional<Guid> guid)
    {
        co_await lock_.WaitAsync();
        if (guid != batchGuid_.Load())
        {
            co_await batchLock_.WaitAsync();
        }
        if (!items_)
        {
            throw NullReferenceException("Object reference not set to an instance of an object: _items.");
        }
        items_->emplace_back(Operation::Clear, std::nullopt);
        if (guid != batchGuid_.Load())
        {
            batchLock_.Release();
        }
        lock_.Release();
        co_return;
    }

    Task<std::string> Output::Read(
        std::optional<std::string> message, std::optional<Guid> guid)
    {
        co_await lock_.WaitAsync();
        if (guid != batchGuid_.Load())
        {
            co_await batchLock_.WaitAsync();
        }
        if (!items_)
        {
            throw NullReferenceException("Object reference not set to an instance of an object: _items.");
        }
        items_->emplace_back(Operation::Read, std::move(message));
        if (!input_)
        {
            throw NullReferenceException("Object reference not set to an instance of an object: _input.");
        }
        const std::int32_t count = static_cast<std::int32_t>(input_->size());
        if (guid != batchGuid_.Load())
        {
            batchLock_.Release();
        }
        lock_.Release();
        co_return co_await DoRead(count);
    }

    Task<std::string> Output::DoRead(std::int32_t count)
    {
        while (true)
        {
            co_await lock_.WaitAsync();
            if (!input_)
            {
                throw NullReferenceException("Object reference not set to an instance of an object: _input.");
            }
            if (!input_->empty() && (*input_)[0].Count == count)
            {
                std::string input = (*input_)[0].Message;
                input_->erase(input_->begin());
                lock_.Release();
                co_return input;
            }
            lock_.Release();
            co_await Delay100Awaiter{};
        }
    }

    Task<void> Output::Run(Detail::CancellationToken token)
    {
        while (!token.IsCancellationRequested())
        {
            lock_.Wait();
            if (!items_)
            {
                throw NullReferenceException("Object reference not set to an instance of an object: _items.");
            }
            while (!items_->empty())
            {
                QueueItem item = (*items_)[0];
                if (item.OperationValue == Operation::Write)
                {
                    ConsoleWriteLineNullable(item.Message);
                }
                else if (item.OperationValue == Operation::Clear)
                {
                    ::MphRead::NativeRuntime::ConsoleClear();
                }
                else if (item.OperationValue == Operation::Read)
                {
                    if (item.Message.has_value())
                    {
                        ConsoleWrite(*item.Message);
                        if (!input_)
                        {
                            throw NullReferenceException(
                                "Object reference not set to an instance of an object: _input.");
                        }
                        const std::int32_t count = static_cast<std::int32_t>(input_->size());
                        std::string message = ::MphRead::NativeRuntime::ConsoleReadLine().value_or(std::string());
                        input_->emplace_back(count, std::move(message));
                    }
                }
                items_->erase(items_->begin());
            }
            lock_.Release();
            std::this_thread::sleep_for(100ms);
        }
        co_return;
    }

    Task<void> Output::End()
    {
        co_await lock_.WaitAsync();
        if (initialized_)
        {
            if (!cts_)
            {
                throw NullReferenceException("Object reference not set to an instance of an object: _cts.");
            }
            cts_->Cancel();
        }
        lock_.Release();
        co_return;
    }
}

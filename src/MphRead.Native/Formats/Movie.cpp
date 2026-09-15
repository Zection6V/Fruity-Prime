#include "Movie.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace System
{
    Decimal::Decimal(std::int32_t value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        _negative = value < 0;
        _lo = _negative ? 0U - bits : bits;
    }

    Decimal::Decimal(std::int32_t lo, std::int32_t mid, std::int32_t hi,
        bool isNegative, std::uint8_t scale)
        : _lo(std::bit_cast<std::uint32_t>(lo)),
          _mid(std::bit_cast<std::uint32_t>(mid)),
          _hi(std::bit_cast<std::uint32_t>(hi)),
          _scale(scale),
          _negative(isNegative)
    {
        if (scale > 28)
        {
            throw ArgumentOutOfRangeException("scale");
        }
    }

    Decimal Decimal::DivideInt32By65536(std::int32_t value) noexcept
    {
        Decimal result(value);
        result._scale = 16;

        // 1 / 2^16 == 5^16 / 10^16. Multiply the 96-bit coefficient by 5^16.
        for (int factor = 0; factor < 16; ++factor)
        {
            std::uint64_t carry = 0;
            std::uint64_t product = static_cast<std::uint64_t>(result._lo) * 5U + carry;
            result._lo = static_cast<std::uint32_t>(product);
            carry = product >> 32;
            product = static_cast<std::uint64_t>(result._mid) * 5U + carry;
            result._mid = static_cast<std::uint32_t>(product);
            carry = product >> 32;
            product = static_cast<std::uint64_t>(result._hi) * 5U + carry;
            result._hi = static_cast<std::uint32_t>(product);
        }

        // Decimal arithmetic canonicalizes exact trailing decimal zeroes.
        while (result._scale > 0)
        {
            std::array<std::uint32_t, 3> quotient{};
            std::uint64_t remainder = 0;
            const std::array<std::uint32_t, 3> limbs{result._lo, result._mid, result._hi};
            for (int i = 2; i >= 0; --i)
            {
                const std::uint64_t current = (remainder << 32)
                    | limbs[static_cast<std::size_t>(i)];
                quotient[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(current / 10U);
                remainder = current % 10U;
            }
            if (remainder != 0)
            {
                break;
            }
            result._lo = quotient[0];
            result._mid = quotient[1];
            result._hi = quotient[2];
            --result._scale;
        }
        if (result._lo == 0 && result._mid == 0 && result._hi == 0)
        {
            result._negative = false;
        }
        return result;
    }

    double Decimal::ToDouble() const noexcept
    {
        long double value = static_cast<long double>(_hi) * 18446744073709551616.0L
            + static_cast<long double>(_mid) * 4294967296.0L
            + static_cast<long double>(_lo);
        for (std::uint8_t i = 0; i < _scale; ++i)
        {
            value /= 10.0L;
        }
        if (_negative)
        {
            value = -value;
        }
        return static_cast<double>(value);
    }
}

namespace MphRead::Formats::MovieNativeRuntime
{
    struct DecoderLifetime
    {
        std::mutex Mutex;
        std::condition_variable Condition;
        std::size_t Active = 0;
    };

    struct TaskState : std::enable_shared_from_this<TaskState>
    {
        std::mutex Mutex;
        std::condition_variable Condition;
        bool Done = false;
        std::exception_ptr Exception{};
        std::coroutine_handle<> Handle{};
        std::coroutine_handle<> Continuation{};
        std::shared_ptr<TaskState> SelfKeepAlive{};

        ~TaskState()
        {
            if (Handle)
            {
                Handle.destroy();
            }
        }
    };

    namespace
    {
        class DelayScheduler final
        {
        public:
            DelayScheduler()
                : _worker([this](std::stop_token token) { Run(token); })
            {
            }

            void Schedule(std::shared_ptr<TaskState> state,
                std::coroutine_handle<MovieTask::promise_type> handle)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _items.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(1),
                    Item{std::move(state), handle});
                _condition.notify_all();
            }

            void DeferRelease(std::shared_ptr<TaskState> state)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _releases.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(1),
                    std::move(state));
                _condition.notify_all();
            }

        private:
            struct Item
            {
                std::shared_ptr<TaskState> State;
                std::coroutine_handle<MovieTask::promise_type> Handle;
            };

            std::mutex _mutex;
            std::condition_variable_any _condition;
            std::multimap<std::chrono::steady_clock::time_point, Item> _items;
            std::multimap<std::chrono::steady_clock::time_point, std::shared_ptr<TaskState>> _releases;
            std::jthread _worker;

            void Run(std::stop_token token)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                while (!token.stop_requested())
                {
                    const auto now = std::chrono::steady_clock::now();
                    while (!_releases.empty() && _releases.begin()->first <= now)
                    {
                        _releases.erase(_releases.begin());
                    }

                    std::optional<std::chrono::steady_clock::time_point> due;
                    if (!_items.empty())
                    {
                        due = _items.begin()->first;
                    }
                    if (!_releases.empty() && (!due.has_value() || _releases.begin()->first < *due))
                    {
                        due = _releases.begin()->first;
                    }
                    if (!due.has_value())
                    {
                        _condition.wait(lock, token, [this]()
                            { return !_items.empty() || !_releases.empty(); });
                        continue;
                    }
                    if (*due > now)
                    {
                        _condition.wait_until(lock, token, *due, []() noexcept { return false; });
                        continue;
                    }
                    if (_items.empty() || _items.begin()->first > now)
                    {
                        continue;
                    }

                    auto first = _items.begin();
                    Item item = std::move(first->second);
                    _items.erase(first);
                    lock.unlock();
                    bool done = false;
                    {
                        std::lock_guard<std::mutex> stateLock(item.State->Mutex);
                        done = item.State->Done;
                    }
                    if (!done && item.Handle && !item.Handle.done())
                    {
                        item.Handle.resume();
                    }
                    lock.lock();
                }
            }
        };

        DelayScheduler& Scheduler()
        {
            static DelayScheduler scheduler;
            return scheduler;
        }

        constexpr std::uint32_t Prime2 = 2246822519U;
        constexpr std::uint32_t Prime3 = 3266489917U;
        constexpr std::uint32_t Prime4 = 668265263U;
        constexpr std::uint32_t Prime5 = 374761393U;

        [[nodiscard]] std::uint32_t GlobalHashSeed() noexcept
        {
            static const std::uint32_t seed = []() noexcept
            {
                try
                {
                    std::random_device device;
                    return (static_cast<std::uint32_t>(device()) << 16)
                        ^ static_cast<std::uint32_t>(device());
                }
                catch (...)
                {
                    return 0U;
                }
            }();
            return seed;
        }

        [[nodiscard]] constexpr std::uint32_t RotateLeft(std::uint32_t value, int offset) noexcept
        {
            return std::rotl(value, offset);
        }

        [[nodiscard]] std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queued) noexcept
        {
            return RotateLeft(hash + queued * Prime3, 17) * Prime4;
        }

        [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash) noexcept
        {
            hash ^= hash >> 15;
            hash *= Prime2;
            hash ^= hash >> 13;
            hash *= Prime3;
            hash ^= hash >> 16;
            return hash;
        }
    }

    std::int32_t HashCombine(std::int32_t first, std::int32_t second) noexcept
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 8U;
        hash = QueueRound(hash, std::bit_cast<std::uint32_t>(first));
        hash = QueueRound(hash, std::bit_cast<std::uint32_t>(second));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    BinaryReader::BinaryReader(std::shared_ptr<std::istream> stream)
        : _stream(std::move(stream))
    {
        if (!_stream)
        {
            throw System::ArgumentNullException("input");
        }
    }

    BinaryReader::~BinaryReader()
    {
        DisposeStream();
    }

    void BinaryReader::DisposeStream() noexcept
    {
        if (!_stream)
        {
            return;
        }
        try
        {
            if (auto* file = dynamic_cast<std::ifstream*>(_stream.get()))
            {
                file->close();
            }
            else if (auto* file = dynamic_cast<std::fstream*>(_stream.get()))
            {
                file->close();
            }
            else
            {
                // std::istream has no virtual Close member. Mark non-file adapters unusable
                // after disposal, matching BinaryReader(Stream, leaveOpen:false) observably.
                _stream->setstate(std::ios::badbit);
            }
        }
        catch (...)
        {
        }
    }

    void BinaryReader::ReadExact(void* destination, std::size_t size)
    {
        _stream->read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
        if (_stream->gcount() != static_cast<std::streamsize>(size))
        {
            throw System::EndOfStreamException();
        }
    }

    std::uint8_t BinaryReader::ReadByte()
    {
        std::uint8_t value = 0;
        ReadExact(&value, sizeof(value));
        return value;
    }

    char16_t BinaryReader::ReadChar()
    {
        if (_pendingChar.has_value())
        {
            const char16_t value = *_pendingChar;
            _pendingChar.reset();
            return value;
        }

        const std::uint8_t first = ReadByte();
        if (first < 0x80U)
        {
            return static_cast<char16_t>(first);
        }

        std::int32_t count = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            count = 2;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            count = 3;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            count = 4;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        }
        else
        {
            return static_cast<char16_t>(0xFFFDU);
        }

        for (std::int32_t i = 1; i < count; ++i)
        {
            std::uint8_t next = 0;
            try
            {
                next = ReadByte();
            }
            catch (const System::EndOfStreamException&)
            {
                return static_cast<char16_t>(0xFFFDU);
            }
            if ((next & 0xC0U) != 0x80U)
            {
                return static_cast<char16_t>(0xFFFDU);
            }
            codePoint = (codePoint << 6) | (next & 0x3FU);
        }

        if (codePoint < minimum || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            return static_cast<char16_t>(0xFFFDU);
        }
        if (codePoint <= 0xFFFFU)
        {
            return static_cast<char16_t>(codePoint);
        }

        codePoint -= 0x10000U;
        const char16_t high = static_cast<char16_t>(0xD800U + (codePoint >> 10));
        _pendingChar = static_cast<char16_t>(0xDC00U + (codePoint & 0x3FFU));
        return high;
    }

    std::int16_t BinaryReader::ReadInt16()
    {
        const std::uint16_t value = ReadUInt16();
        return std::bit_cast<std::int16_t>(value);
    }

    std::uint16_t BinaryReader::ReadUInt16()
    {
        const std::uint16_t b0 = ReadByte();
        const std::uint16_t b1 = ReadByte();
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }

    std::int32_t BinaryReader::ReadInt32()
    {
        std::uint32_t value = ReadByte();
        value |= static_cast<std::uint32_t>(ReadByte()) << 8;
        value |= static_cast<std::uint32_t>(ReadByte()) << 16;
        value |= static_cast<std::uint32_t>(ReadByte()) << 24;
        return std::bit_cast<std::int32_t>(value);
    }

    std::int64_t BinaryReader::Position()
    {
        const std::streampos position = _stream->tellg();
        if (position == std::streampos(-1))
        {
            throw std::runtime_error("Stream position is unavailable.");
        }
        return static_cast<std::int64_t>(position);
    }

    void BinaryReader::Position(std::int64_t value)
    {
        _stream->clear();
        _stream->seekg(static_cast<std::streamoff>(value), std::ios::beg);
        if (!_stream->good())
        {
            throw System::ArgumentOutOfRangeException("value");
        }
    }

    MovieTask::MovieTask(std::shared_ptr<TaskState> state) noexcept
        : _state(std::move(state))
    {
    }

    MovieTask MovieTask::promise_type::get_return_object()
    {
        auto state = std::make_shared<TaskState>();
        State = state.get();
        state->Handle = std::coroutine_handle<promise_type>::from_promise(*this);
        return MovieTask(std::move(state));
    }

    void MovieTask::promise_type::unhandled_exception() noexcept
    {
        Exception = std::current_exception();
    }

    std::coroutine_handle<> MovieTask::promise_type::FinalAwaiter::await_suspend(
        std::coroutine_handle<promise_type> handle) const noexcept
    {
        promise_type& promise = handle.promise();
        TaskState* state = promise.State;
        std::coroutine_handle<> continuation = std::noop_coroutine();
        if (promise.Lifetime)
        {
            std::lock_guard<std::mutex> lifetimeLock(promise.Lifetime->Mutex);
            if (promise.Lifetime->Active > 0)
            {
                --promise.Lifetime->Active;
            }
            promise.Lifetime->Condition.notify_all();
            promise.Lifetime.reset();
        }
        std::shared_ptr<TaskState> deferredRelease;
        {
            std::lock_guard<std::mutex> lock(state->Mutex);
            state->Exception = promise.Exception;
            state->Done = true;
            if (state->Continuation)
            {
                continuation = state->Continuation;
                state->Continuation = {};
            }
            deferredRelease = std::move(state->SelfKeepAlive);
        }
        state->Condition.notify_all();
        if (deferredRelease)
        {
            // A C# Task owns its state machine while suspended even when the returned
            // Task reference is discarded. Release that self-ownership only after the
            // final-suspend transition has safely returned to the scheduler/continuation.
            Scheduler().DeferRelease(std::move(deferredRelease));
        }
        return continuation;
    }

    MovieTask::Awaiter::Awaiter(std::shared_ptr<TaskState> state) noexcept
        : _state(std::move(state))
    {
    }

    bool MovieTask::Awaiter::await_ready() const noexcept
    {
        if (!_state)
        {
            return true;
        }
        std::lock_guard<std::mutex> lock(_state->Mutex);
        return _state->Done;
    }

    bool MovieTask::Awaiter::await_suspend(
        std::coroutine_handle<promise_type> continuation) noexcept
    {
        if (!_state)
        {
            return false;
        }
        TaskState* parentRaw = continuation.promise().State;
        std::shared_ptr<TaskState> parentState = parentRaw->shared_from_this();
        {
            std::lock_guard<std::mutex> parentLock(parentState->Mutex);
            if (!parentState->SelfKeepAlive)
            {
                parentState->SelfKeepAlive = parentState;
            }
        }
        std::lock_guard<std::mutex> lock(_state->Mutex);
        if (_state->Done)
        {
            std::lock_guard<std::mutex> parentLock(parentState->Mutex);
            parentState->SelfKeepAlive.reset();
            return false;
        }
        _state->Continuation = continuation;
        return true;
    }

    void MovieTask::Awaiter::await_resume()
    {
        if (!_state)
        {
            return;
        }
        std::exception_ptr exception;
        {
            std::lock_guard<std::mutex> lock(_state->Mutex);
            exception = _state->Exception;
        }
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }

    void MovieTask::Awaiter::GetResult()
    {
        if (!_state)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(_state->Mutex);
        _state->Condition.wait(lock, [this]() { return _state->Done; });
        std::exception_ptr exception = _state->Exception;
        lock.unlock();
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }

    MovieTask::Awaiter MovieTask::GetAwaiter() const noexcept
    {
        return Awaiter(_state);
    }

    void MovieTask::DelayAwaitable::await_suspend(
        std::coroutine_handle<promise_type> handle) const
    {
        TaskState* raw = handle.promise().State;
        std::shared_ptr<TaskState> state = raw->shared_from_this();
        {
            std::lock_guard<std::mutex> lock(state->Mutex);
            if (!state->SelfKeepAlive)
            {
                state->SelfKeepAlive = state;
            }
        }
        Scheduler().Schedule(std::move(state), handle);
    }

    bool MovieTask::LifetimeAwaitable::await_suspend(
        std::coroutine_handle<promise_type> handle) const noexcept
    {
        if (Lifetime)
        {
            {
                std::lock_guard<std::mutex> lock(Lifetime->Mutex);
                ++Lifetime->Active;
            }
            handle.promise().Lifetime = Lifetime;
        }
        return false;
    }

    MovieTask::LifetimeAwaitable MovieTask::TrackLifetime(
        std::shared_ptr<DecoderLifetime> lifetime) noexcept
    {
        return LifetimeAwaitable{std::move(lifetime)};
    }
}

namespace MphRead::Formats
{
    namespace
    {
        [[nodiscard]] std::int32_t ClampInt(std::int32_t value, std::int32_t minimum, std::int32_t maximum) noexcept
        {
            return std::max(minimum, std::min(maximum, value));
        }

        [[nodiscard]] std::int32_t WrapInt32Add(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t WrapInt32Multiply(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t WrapInt32Subtract(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t WrapInt32Negate(std::int32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(0U - std::bit_cast<std::uint32_t>(value));
        }

        [[nodiscard]] std::int32_t WrapInt32ShiftLeft(std::int32_t value, std::int32_t count) noexcept
        {
            const std::uint32_t shift = static_cast<std::uint32_t>(count) & 31U;
            return std::bit_cast<std::int32_t>(
                std::bit_cast<std::uint32_t>(value) << shift);
        }

        [[nodiscard]] std::int32_t ArithmeticInt32ShiftRight(
            std::int32_t value, std::int32_t count) noexcept
        {
            const std::uint32_t shift = static_cast<std::uint32_t>(count) & 31U;
            if (shift == 0)
            {
                return value;
            }
            std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            bits >>= shift;
            if (value < 0)
            {
                bits |= (~std::uint32_t{0}) << (32U - shift);
            }
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t WrapInt32Add3(
            std::int32_t a, std::int32_t b, std::int32_t c) noexcept
        {
            return WrapInt32Add(WrapInt32Add(a, b), c);
        }

        void FillManagedStackallocUnspecified(std::span<std::int32_t> values) noexcept
        {
            // C# stackalloc without an initializer deliberately exposes unspecified
            // existing stack contents. C++ may not read indeterminate int objects, so
            // materialize arbitrary defined bit patterns instead of inventing zero-init.
            static std::atomic<std::uint32_t> nonce{0xA341316CU};
            std::uint32_t state = nonce.fetch_add(0x9E3779B9U, std::memory_order_relaxed)
                ^ static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(values.data()));
            for (std::int32_t& value : values)
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                value = std::bit_cast<std::int32_t>(state);
            }
        }

        class SharedVectorStreamBuf final : public std::streambuf
        {
        public:
            explicit SharedVectorStreamBuf(std::shared_ptr<std::vector<std::uint8_t>> data)
                : _data(std::move(data)), _length(_data ? _data->size() : 0)
            {
            }

        protected:
            std::streamsize xsgetn(char* destination, std::streamsize count) override
            {
                if (count <= 0 || _position >= _length || !_data)
                {
                    return 0;
                }
                const std::size_t requested = static_cast<std::size_t>(count);
                const std::size_t available = _length - _position;
                const std::size_t actual = std::min(requested, available);
                const std::size_t currentSize = _data->size();
                if (_position >= currentSize)
                {
                    return 0;
                }
                const std::size_t readable = std::min(actual, currentSize - _position);
                std::memcpy(destination, _data->data() + _position, readable);
                _position += readable;
                return static_cast<std::streamsize>(readable);
            }

            int_type underflow() override
            {
                if (!_data || _position >= _length || _position >= _data->size())
                {
                    return traits_type::eof();
                }
                return traits_type::to_int_type(
                    static_cast<char>((*_data)[_position]));
            }

            int_type uflow() override
            {
                const int_type value = underflow();
                if (!traits_type::eq_int_type(value, traits_type::eof()))
                {
                    ++_position;
                }
                return value;
            }

            pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                std::ios_base::openmode which) override
            {
                if ((which & std::ios_base::in) == 0)
                {
                    return pos_type(off_type(-1));
                }
                std::int64_t base = 0;
                if (direction == std::ios_base::beg)
                {
                    base = 0;
                }
                else if (direction == std::ios_base::cur)
                {
                    base = static_cast<std::int64_t>(_position);
                }
                else if (direction == std::ios_base::end)
                {
                    base = static_cast<std::int64_t>(_length);
                }
                else
                {
                    return pos_type(off_type(-1));
                }
                const std::int64_t target = base + static_cast<std::int64_t>(offset);
                if (target < 0)
                {
                    return pos_type(off_type(-1));
                }
                _position = static_cast<std::size_t>(target);
                return pos_type(static_cast<off_type>(_position));
            }

            pos_type seekpos(pos_type position, std::ios_base::openmode which) override
            {
                return seekoff(static_cast<off_type>(position), std::ios_base::beg, which);
            }

        private:
            std::shared_ptr<std::vector<std::uint8_t>> _data;
            const std::size_t _length;
            std::size_t _position = 0;
        };

        class SharedVectorInputStream final : public std::istream
        {
        public:
            explicit SharedVectorInputStream(std::shared_ptr<std::vector<std::uint8_t>> data)
                : std::istream(nullptr), _buffer(std::move(data))
            {
                rdbuf(&_buffer);
            }

        private:
            SharedVectorStreamBuf _buffer;
        };

        [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
        {
#if defined(__cpp_char8_t)
            std::u8string converted;
            converted.reserve(value.size());
            for (unsigned char ch : value)
            {
                converted.push_back(static_cast<char8_t>(ch));
            }
            return std::filesystem::path(converted);
#else
            return std::filesystem::u8path(value.begin(), value.end());
#endif
        }

        [[nodiscard]] std::string Extension(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.extension().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.extension().u8string();
#endif
        }

        [[nodiscard]] std::string FileName(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.filename().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.filename().u8string();
#endif
        }

        [[nodiscard]] std::string PathUtf8(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.u8string();
#endif
        }

        [[nodiscard]] std::string Stem(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.stem().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.stem().u8string();
#endif
        }

        [[nodiscard]] bool FileExists(const std::string& path) noexcept
        {
            try
            {
                return std::filesystem::is_regular_file(PathFromUtf8(path));
            }
            catch (...)
            {
                return false;
            }
        }

        extern "C"
        {
            using StbiWriteFunc = void (*)(void* context, void* data, int size);
            void stbi_flip_vertically_on_write(int flag);
            int stbi_write_png_to_func(StbiWriteFunc func, void* context, int w, int h,
                int comp, const void* data, int stride_in_bytes);
        }

        void StbiStreamWrite(void* context, void* data, int size)
        {
            auto* stream = static_cast<std::ostream*>(context);
            stream->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }

        void WriteRgbPng(const std::string& path, std::span<const std::uint8_t> pixels,
            std::int32_t width, std::int32_t height)
        {
            std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                throw std::runtime_error("Could not create PNG output file.");
            }
            stbi_flip_vertically_on_write(0);
            const int result = stbi_write_png_to_func(
                &StbiStreamWrite, &stream, width, height, 3, pixels.data(),
                WrapInt32Multiply(width, 3));
            if (result == 0 || !stream)
            {
                throw std::runtime_error("Failed to write PNG output file.");
            }
        }

        template <typename T>
        [[nodiscard]] T WrapFromUnsigned(std::make_unsigned_t<T> value) noexcept
        {
            return std::bit_cast<T>(value);
        }
    }

    SeekTableEntry& SeekTableEntry::operator=(const SeekTableEntry& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~SeekTableEntry();
            ::new (static_cast<void*>(this)) SeekTableEntry(other);
        }
        return *this;
    }

    Vector2ir& Vector2ir::operator=(const Vector2ir& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Vector2ir();
            ::new (static_cast<void*>(this)) Vector2ir(other);
        }
        return *this;
    }

    VxBuffers::VxBuffers(
        std::shared_ptr<std::array<std::shared_ptr<VideoFrame>, 3>> prevVideoFrames,
        std::shared_ptr<std::array<std::int32_t, 3>> quantizerTable,
        std::shared_ptr<ByteArray2D> planeBufferY,
        std::shared_ptr<ByteArray2D> planeBufferU,
        std::shared_ptr<ByteArray2D> planeBufferV,
        std::shared_ptr<ByteArray2D> coeffBufferY,
        std::shared_ptr<ByteArray2D> coeffBufferUV,
        std::shared_ptr<Vector2irArray2D> vectors,
        std::shared_ptr<std::array<std::int16_t, 8>> prevSampleBuffer,
        std::shared_ptr<std::array<std::int32_t, 256>> prevPulseBuffer,
        std::shared_ptr<std::array<std::int32_t, 8>> lpcFilterBuffer,
        std::shared_ptr<std::array<std::int32_t, 8>> influenceBuffer,
        std::shared_ptr<std::vector<std::int16_t>> sampleBuffer)
        : PrevVideoFrames(std::move(prevVideoFrames)),
          QuantizerTable(std::move(quantizerTable)),
          PlaneBufferY(std::move(planeBufferY)),
          PlaneBufferU(std::move(planeBufferU)),
          PlaneBufferV(std::move(planeBufferV)),
          CoeffBufferY(std::move(coeffBufferY)),
          CoeffBufferUV(std::move(coeffBufferUV)),
          Vectors(std::move(vectors)),
          PrevSampleBuffer(std::move(prevSampleBuffer)),
          PrevPulseBuffer(std::move(prevPulseBuffer)),
          LpcFilterBuffer(std::move(lpcFilterBuffer)),
          InfluenceBuffer(std::move(influenceBuffer)),
          SampleBuffer(std::move(sampleBuffer))
    {
    }

    VxBuffers& VxBuffers::operator=(const VxBuffers& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~VxBuffers();
            ::new (static_cast<void*>(this)) VxBuffers(other);
        }
        return *this;
    }

    Block& Block::operator=(const Block& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Block();
            ::new (static_cast<void*>(this)) Block(other);
        }
        return *this;
    }

    Block Block::HalfLeft() const noexcept
    {
        return Block(X, Y, W / 2, H);
    }

    Block Block::HalfRight() const noexcept
    {
        return Block(WrapInt32Add(X, W / 2), Y, W / 2, H);
    }

    Block Block::HalfUp() const noexcept
    {
        return Block(X, Y, W, H / 2);
    }

    Block Block::HalfDown() const noexcept
    {
        return Block(X, WrapInt32Add(Y, H / 2), W, H / 2);
    }

    const std::array<std::array<std::int32_t, 3>, 6> VxDecoder::_quantizer4x4Table{{
        {{0x0A, 0x0D, 0x10}},
        {{0x0B, 0x0E, 0x12}},
        {{0x0D, 0x10, 0x14}},
        {{0x0E, 0x12, 0x17}},
        {{0x10, 0x14, 0x19}},
        {{0x12, 0x17, 0x1D}}
    }};

    bool VxDecoder::UseStaticBuffers = true;

    VxDecoder::VxDecoder()
        : _lifetime(std::make_shared<MovieNativeRuntime::DecoderLifetime>())
    {
        for (std::int32_t frame = 0; frame < 4; frame = WrapInt32Add(frame, 1))
        {
            const std::size_t base = static_cast<std::size_t>(frame) * 3U;
            _planeBuffers[base] = std::make_shared<ByteArray2D>(_mphFrameH, _mphFrameW);
            _planeBuffers[base + 1U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
            _planeBuffers[base + 2U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
        }
    }

    VxDecoder::~VxDecoder()
    {
        if (_lifetime)
        {
            std::unique_lock<std::mutex> lock(_lifetime->Mutex);
            _lifetime->Condition.wait(lock, [this]() { return _lifetime->Active == 0; });
        }
    }

    namespace
    {
        struct DecoderInstances
        {
            VxDecoder One;
            VxDecoder Two;
        };

        DecoderInstances& GetDecoderInstances()
        {
            static DecoderInstances instances;
            return instances;
        }
    }

    VxDecoder& VxDecoder::Instance1()
    {
        return GetDecoderInstances().One;
    }

    VxDecoder& VxDecoder::Instance2()
    {
        return GetDecoderInstances().Two;
    }

    void VxDecoder::Reset()
    {
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (_vxFrames)
            {
                _vxFrames->clear();
            }
        }
        _framesQueued.store(0, std::memory_order_relaxed);
        UseStaticBuffers = true;
        std::fill(_sampleBuffer->begin(), _sampleBuffer->end(), 0);
    }

    MovieNativeRuntime::MovieTask VxDecoder::ExportAll()
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        Reset();
        UseStaticBuffers = false;
        std::int32_t i = 0;
        const std::string movieFolder = Paths::Combine(Paths::FileSystem(), "movies");
        std::vector<std::filesystem::path> files;
        for (const std::filesystem::directory_entry& entry
            : std::filesystem::directory_iterator(PathFromUtf8(movieFolder)))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
        for (const std::filesystem::path& path : files)
        {
            if (Extension(path) == ".vx")
            {
                i = WrapInt32Add(i, 1);
                std::cout << "Exporting " << i << " of " << files.size()
                    << ": " << FileName(path) << '\n';
                co_await Decode(PathUtf8(path), true);
            }
        }
        std::cout << "Done." << '\n';
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Export(const std::string& filePath)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        Reset();
        std::string path = Paths::Combine(Paths::FileSystem(), "movies", filePath);
        if (!FileExists(path))
        {
            path = Paths::Combine(Paths::FileSystem(), filePath);
            if (!FileExists(path))
            {
                path = filePath;
            }
        }
        std::cout << "Exporting..." << '\n';
        UseStaticBuffers = false;
        co_await Decode(path, true);
        std::cout << "Done." << '\n';
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        const std::string& filePath, bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        auto stream = std::make_shared<std::ifstream>(PathFromUtf8(filePath), std::ios::binary);
        if (!*stream)
        {
            throw std::runtime_error("Could not find file '" + filePath + "'.");
        }
        co_await Decode(std::static_pointer_cast<std::istream>(stream),
            FileName(PathFromUtf8(filePath)), writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<std::vector<std::uint8_t>> data, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        if (!data)
        {
            // MemoryStream(byte[]) faults the async method with ArgumentNullException.
            throw System::ArgumentNullException("buffer");
        }
        auto stream = std::make_shared<SharedVectorInputStream>(data);
        co_await Decode(std::static_pointer_cast<std::istream>(stream), filename, writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<std::istream> stream, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        co_await MovieNativeRuntime::MovieTask::TrackLifetime(_lifetime);
        co_await DecodeCore(std::move(stream), filename, writeFiles, token);
        co_return;
    }

    MovieNativeRuntime::MovieTask VxDecoder::DecodeCore(
        std::shared_ptr<std::istream> stream, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        const std::string folder = Paths::Combine(Paths::Export(), Stem(PathFromUtf8(filename)));
        if (writeFiles)
        {
            std::filesystem::create_directories(PathFromUtf8(folder));
        }

        MovieNativeRuntime::BinaryReader reader(std::move(stream));
        _nextPlaneBufferIndex = 0;
        _nextSampleBufferIndex = 0;
        _framesQueued.store(0, std::memory_order_relaxed);

        (*Magic)[0] = reader.ReadChar();
        (*Magic)[1] = reader.ReadChar();
        (*Magic)[2] = reader.ReadChar();
        (*Magic)[3] = reader.ReadChar();
        FrameCount = reader.ReadInt32();
        FrameWidth = reader.ReadInt32();
        FrameHeight = reader.ReadInt32();
        FrameRate = System::Decimal::DivideInt32By65536(reader.ReadInt32());
        Quantizer = reader.ReadInt32();
        AudioSampleRate = reader.ReadInt32();
        AudioStreamCount = reader.ReadInt32();
        if (AudioStreamCount > 1)
        {
            throw ProgramException("VX decoding error 022: " + std::to_string(AudioStreamCount));
        }
        MaxDataSize = reader.ReadInt32();
        MaxDataSize = WrapInt32Add(MaxDataSize, -2);
        assert(MaxDataSize % 2 == 0);
        ExtradataOffset = reader.ReadInt32();
        SeekTableOffset = reader.ReadInt32();
        SeekTableCount = reader.ReadInt32();

        if (FrameWidth % 16 != 0 || FrameHeight % 16 != 0)
        {
            throw ProgramException("VX decoding error 001: "
                + std::to_string(FrameWidth) + " x " + std::to_string(FrameHeight));
        }

        const std::int64_t prevPosition = reader.Position();
        reader.Position(ExtradataOffset);

        for (std::int32_t i = 0; i < 3; ++i)
        {
            for (std::int32_t j = 0; j < 64; ++j)
            {
                for (std::int32_t k = 0; k < 8; ++k)
                {
                    (*Extradata->LpcCodebooks)(i, j, k) = reader.ReadInt16();
                }
            }
        }

        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->ScaleModifiers)[static_cast<std::size_t>(i)] = reader.ReadUInt16();
        }
        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->LpcBase)[static_cast<std::size_t>(i)] = reader.ReadInt32();
        }
        Extradata->ScaleInitial = reader.ReadInt32();

        reader.Position(SeekTableOffset);
        for (std::int32_t i = 0; i < SeekTableCount; ++i)
        {
            (*SeekTable)[0] = SeekTableEntry(reader.ReadInt32(), reader.ReadInt32());
        }

        if (Quantizer < 12 || Quantizer > 161)
        {
            throw ProgramException("VX decoding error 002: " + std::to_string(Quantizer));
        }
        const std::int32_t qy = Quantizer / 6;
        const std::int32_t qx = Quantizer % 6;
        const std::array<std::int32_t, 3>& table = _quantizer4x4Table.at(static_cast<std::size_t>(qx));
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            (*QuantizerTable)[i] = table[i] << qy;
        }

        reader.Position(prevPosition);

        (void)VLC::Temp();

        std::shared_ptr<std::vector<std::uint8_t>> buffer = GetDataBuffer();
        std::fill(buffer->begin(), buffer->end(), 0);

        std::shared_ptr<ByteArray2D> coeffBufferY;
        std::shared_ptr<ByteArray2D> coeffBufferUV;
        std::shared_ptr<Vector2irArray2D> vectors;
        if (UseStaticBuffers)
        {
            coeffBufferY = _coeffBufferY;
            coeffBufferUV = _coeffBufferUV;
            vectors = _vectors;
        }
        else
        {
            coeffBufferY = std::make_shared<ByteArray2D>(FrameHeight / 4 + 1, FrameWidth / 4 + 1);
            coeffBufferUV = std::make_shared<ByteArray2D>(FrameHeight / 8 + 1, FrameWidth / 8 + 1);
            vectors = std::make_shared<Vector2irArray2D>(FrameHeight / 16 + 1, FrameWidth / 16 + 2);
        }

        if (FrameCount < 0)
        {
            throw std::out_of_range("Non-negative number required. (Parameter 'capacity')");
        }
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            _vxFrames = std::make_shared<std::vector<std::shared_ptr<VxFrame>>>();
            _vxFrames->reserve(static_cast<std::size_t>(FrameCount));
        }
        (*_prevVideoFrames)[0].reset();
        (*_prevVideoFrames)[1].reset();
        (*_prevVideoFrames)[2].reset();
        _prevSampleBuffer->fill(0);
        _prevPulseBuffer->fill(0);
        _lpcFilterBuffer->fill(0);
        _influenceBuffer->fill(0);
        std::shared_ptr<AudioFrame> prevAudioFrame{};
        _audioFrameTotal.store(0, std::memory_order_release);

        std::ofstream waveFile;
        std::ostringstream nullFile;
        std::ostream* output = &nullFile;
        if (writeFiles && AudioStreamCount >= 1)
        {
            waveFile.open(PathFromUtf8(Paths::Combine(folder, "audio.wav")),
                std::ios::binary | std::ios::trunc);
            if (!waveFile)
            {
                throw std::runtime_error("Could not create audio.wav.");
            }
            output = &waveFile;
        }
        Sound::BinaryWriter writer(*output);
        std::vector<std::uint8_t> fileOutputBuffer;
        if (writeFiles)
        {
            const std::int32_t size = WrapInt32Multiply(
                WrapInt32Multiply(FrameWidth, FrameHeight), 3);
            if (size < 0)
            {
                throw System::OverflowException();
            }
            fileOutputBuffer.resize(static_cast<std::size_t>(size));
        }

        for (std::int32_t i = 0; i < 11; ++i)
        {
            writer.Write(static_cast<std::int32_t>(0));
        }

        for (std::int32_t i = 0; i < FrameCount; ++i)
        {
            while (!writeFiles && _framesQueued.load(std::memory_order_relaxed) >= 4 && !token.stop_requested())
            {
                co_await MovieNativeRuntime::MovieTask::DelayOneMillisecond();
            }
            if (token.stop_requested())
            {
                co_return;
            }

            std::int32_t dataSize = reader.ReadUInt16();
            dataSize = WrapInt32Add(dataSize, -2);
            assert(dataSize % 2 == 0);
            assert(dataSize <= MaxDataSize);
            const std::int32_t audioFrameCount = reader.ReadUInt16();

            std::shared_ptr<ByteArray2D> planeBufferY{};
            std::shared_ptr<ByteArray2D> planeBufferU{};
            std::shared_ptr<ByteArray2D> planeBufferV{};
            if (UseStaticBuffers)
            {
                auto planeBuffers = GetPlaneBuffers();
                planeBufferY = std::move(planeBuffers[0]);
                planeBufferU = std::move(planeBuffers[1]);
                planeBufferV = std::move(planeBuffers[2]);
            }

            coeffBufferY->Clear();
            coeffBufferUV->Clear();
            vectors->Clear();

            VxBuffers buffers(
                _prevVideoFrames, QuantizerTable,
                planeBufferY, planeBufferU, planeBufferV,
                coeffBufferY, coeffBufferUV, vectors,
                _prevSampleBuffer, _prevPulseBuffer,
                _lpcFilterBuffer, _influenceBuffer, _sampleBuffer);

            auto vxFrame = std::make_shared<VxFrame>(
                FrameWidth, FrameHeight, audioFrameCount,
                Extradata, prevAudioFrame, buffers, _nextSampleBufferIndex);
            vxFrame->Decode(reader, buffer, dataSize);

            if (audioFrameCount > 0)
            {
                prevAudioFrame = vxFrame->AudioFrames->back();
                _nextSampleBufferIndex = WrapInt32Add(_nextSampleBufferIndex, audioFrameCount);
                _nextSampleBufferIndex %= SampleBufferCount();
            }

            const std::int32_t audioFrameTotal = WrapInt32Add(
                _audioFrameTotal.load(std::memory_order_relaxed), audioFrameCount);
            _audioFrameTotal.store(audioFrameTotal, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                _vxFrames->push_back(vxFrame);
            }
            const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
            _framesQueued.store(WrapInt32Add(queued, 1), std::memory_order_relaxed);
            (*_prevVideoFrames)[2] = (*_prevVideoFrames)[1];
            (*_prevVideoFrames)[1] = (*_prevVideoFrames)[0];
            (*_prevVideoFrames)[0] = vxFrame->VideoFrame;

            if (writeFiles && UseStaticBuffers)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, i);
            }

            if (writeFiles && audioFrameCount > 0)
            {
                for (std::int32_t j = 0; j < audioFrameCount; ++j)
                {
                    const std::shared_ptr<AudioFrame>& audioFrame
                        = vxFrame->AudioFrames->at(static_cast<std::size_t>(j));
                    const std::span<const std::int16_t> samples = audioFrame->SampleBuffer();
                    for (std::int32_t k = 0; k < 128; ++k)
                    {
                        const std::uint16_t sample
                            = std::bit_cast<std::uint16_t>(samples[static_cast<std::size_t>(k)]);
                        writer.Write(static_cast<std::uint8_t>(sample & 0xFFU));
                        writer.Write(static_cast<std::uint8_t>((sample >> 8) & 0xFFU));
                    }
                }
            }
        }

        if (writeFiles && _audioFrameTotal.load(std::memory_order_acquire) > 0)
        {
            output->clear();
            output->seekp(0, std::ios::beg);
            Sound::SoundRead::WriteWavHeader(
                writer,
                static_cast<std::uint32_t>(_audioFrameTotal.load(std::memory_order_relaxed)) * 128U,
                static_cast<std::uint16_t>(AudioSampleRate),
                WaveFormat::PCM16);
        }

        if (writeFiles && !UseStaticBuffers)
        {
            std::shared_ptr<std::vector<std::shared_ptr<VxFrame>>> frames;
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                frames = _vxFrames;
            }
            std::int32_t frame = 0;
            for (const std::shared_ptr<VxFrame>& vxFrame : *frames)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, frame);
                frame = WrapInt32Add(frame, 1);
            }
        }
        co_return;
    }

    void VxDecoder::WriteFile(
        std::span<std::uint8_t> pixelBuffer, const VxFrame& vxFrame,
        const std::string& folder, std::int32_t frameIndex)
    {
        const VideoFrame& videoFrame = *vxFrame.VideoFrame;
        for (std::int32_t y = 0; y < FrameHeight; ++y)
        {
            for (std::int32_t x = 0; x < FrameWidth; ++x)
            {
                const std::int32_t cy = (*videoFrame.PlaneBufferY())(y, x);
                const std::int32_t cu = (*videoFrame.PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*videoFrame.PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                std::int32_t index = WrapInt32Multiply(
                    WrapInt32Add(WrapInt32Multiply(y, FrameWidth), x), 3);
                if (index < 0 || static_cast<std::size_t>(index) + 2U >= pixelBuffer.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Red;
                index = WrapInt32Add(index, 1);
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Green;
                index = WrapInt32Add(index, 1);
                pixelBuffer[static_cast<std::size_t>(index)] = rgb.Blue;
            }
        }

        std::ostringstream name;
        name << std::setw(4) << std::setfill('0') << frameIndex << ".png";
        WriteRgbPng(Paths::Combine(folder, name.str()), pixelBuffer, FrameWidth, FrameHeight);
    }

    bool VxDecoder::GetImage(std::int32_t frameIndex,
        const std::shared_ptr<std::vector<std::uint8_t>>& texture)
    {
        std::shared_ptr<VxFrame> vxFrame;
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (!_vxFrames || frameIndex >= static_cast<std::int32_t>(_vxFrames->size()))
            {
                return false;
            }
            if (frameIndex < 0)
            {
                throw System::ArgumentOutOfRangeException("index");
            }
            vxFrame = (*_vxFrames)[static_cast<std::size_t>(frameIndex)];
        }
        if (!texture)
        {
            throw System::NullReferenceException();
        }
        for (std::int32_t y = 0; y < FrameHeight; y = WrapInt32Add(y, 1))
        {
            for (std::int32_t x = 0; x < FrameWidth; x = WrapInt32Add(x, 1))
            {
                const std::int32_t cy = (*vxFrame->VideoFrame->PlaneBufferY())(y, x);
                const std::int32_t cu = (*vxFrame->VideoFrame->PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*vxFrame->VideoFrame->PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                const std::int32_t base = WrapInt32Add(
                    WrapInt32Multiply(y, 256 * 3), WrapInt32Multiply(x, 3));
                const std::array<std::uint8_t, 3> values{rgb.Red, rgb.Green, rgb.Blue};
                for (std::int32_t channel = 0; channel < 3; channel = WrapInt32Add(channel, 1))
                {
                    const std::int32_t index = WrapInt32Add(base, channel);
                    if (index < 0 || static_cast<std::size_t>(index) >= texture->size())
                    {
                        throw System::IndexOutOfRangeException();
                    }
                    (*texture)[static_cast<std::size_t>(index)]
                        = values[static_cast<std::size_t>(channel)];
                }
            }
        }
        const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
        _framesQueued.store(WrapInt32Add(queued, -1), std::memory_order_relaxed);
        return true;
    }

    std::span<const std::int16_t> VxDecoder::GetAudioBuffer(std::int32_t index) const
    {
        index %= SampleBufferCount();
        const std::int32_t start = WrapInt32Multiply(128, index);
        if (start < 0 || static_cast<std::size_t>(start) > _sampleBuffer->size()
            || _sampleBuffer->size() - static_cast<std::size_t>(start) < 128U)
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<const std::int16_t>(
            _sampleBuffer->data() + static_cast<std::ptrdiff_t>(start), 128);
    }

    std::shared_ptr<std::vector<std::uint8_t>> VxDecoder::GetDataBuffer()
    {
        if (UseStaticBuffers || MaxDataSize <= _mphMaxDataSize)
        {
            return _dataBuffer;
        }
        return std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(MaxDataSize));
    }

    std::array<std::shared_ptr<ByteArray2D>, 3> VxDecoder::GetPlaneBuffers()
    {
        auto takeBuffer = [this]() -> std::shared_ptr<ByteArray2D>
        {
            const std::int32_t index = _nextPlaneBufferIndex;
            _nextPlaneBufferIndex = WrapInt32Add(_nextPlaneBufferIndex, 1);
            if (index < 0 || static_cast<std::size_t>(index) >= _planeBuffers.size())
            {
                throw System::IndexOutOfRangeException();
            }
            return _planeBuffers[static_cast<std::size_t>(index)];
        };
        std::shared_ptr<ByteArray2D> bufferY = takeBuffer();
        std::shared_ptr<ByteArray2D> bufferU = takeBuffer();
        std::shared_ptr<ByteArray2D> bufferV = takeBuffer();
        _nextPlaneBufferIndex %= static_cast<std::int32_t>(_planeBuffers.size());

        // Preserve the source exactly: V is cleared twice; Y is not cleared here.
        bufferV->Clear();
        bufferU->Clear();
        bufferV->Clear();
        return {std::move(bufferY), std::move(bufferU), std::move(bufferV)};
    }

    ColorRgb VxDecoder::YuvToRgb(std::int32_t y, std::int32_t u, std::int32_t v) noexcept
    {
        u -= 128;
        v -= 128;
        const std::int32_t r = y + 2 * v;
        const std::int32_t g = y - u / 2 - v;
        const std::int32_t b = y + 2 * u;
        return ColorRgb(
            static_cast<std::uint8_t>(ClampInt(r, 0, 255)),
            static_cast<std::uint8_t>(ClampInt(g, 0, 255)),
            static_cast<std::uint8_t>(ClampInt(b, 0, 255)));
    }

    VxFrame::VxFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, std::int32_t audioFrameCount,
        std::shared_ptr<AudioExtradata> extradata, std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers, std::int32_t sampleBufferIndex)
        : VideoFrame(std::make_shared<::MphRead::Formats::VideoFrame>(
            frameWidth, frameHeight, buffers)),
          AudioFrameCount(audioFrameCount),
          AudioFrames([extradata = std::move(extradata), prevAudioFrame = std::move(prevAudioFrame),
              &buffers, sampleBufferIndex, audioFrameCount]() mutable
          {
              if (audioFrameCount < 0)
              {
                  throw std::overflow_error("Array dimensions exceeded supported range.");
              }
              auto frames = std::make_shared<
                  std::vector<std::shared_ptr<::MphRead::Formats::AudioFrame>>>(
                      static_cast<std::size_t>(audioFrameCount));
              for (std::int32_t i = 0; i < audioFrameCount; ++i)
              {
                  auto audioFrame = std::make_shared<::MphRead::Formats::AudioFrame>(
                      extradata, prevAudioFrame, buffers, sampleBufferIndex);
                  sampleBufferIndex = WrapInt32Add(sampleBufferIndex, 1);
                  (*frames)[static_cast<std::size_t>(i)] = audioFrame;
                  prevAudioFrame = std::move(audioFrame);
                  sampleBufferIndex %= VxDecoder::SampleBufferCount();
              }
              return frames;
          }())
    {
    }

    void VxFrame::Decode(
        MovieNativeRuntime::BinaryReader& reader,
        const std::shared_ptr<std::vector<std::uint8_t>>& buffer,
        std::int32_t length)
    {
        for (std::int32_t i = 0; i < length; i += 2)
        {
            buffer->at(static_cast<std::size_t>(i + 1)) = reader.ReadByte();
            buffer->at(static_cast<std::size_t>(i)) = reader.ReadByte();
        }
        BitStreamReader bitReader(buffer, length);
        VideoFrame->Decode(bitReader);
        for (std::int32_t i = 0; i < AudioFrameCount; ++i)
        {
            AudioFrames->at(static_cast<std::size_t>(i))->Decode(bitReader);
        }
    }

    BitStreamReader::BitStreamReader(
        std::shared_ptr<std::vector<std::uint8_t>> buffer, std::int32_t length)
        : _buffer(std::move(buffer)), _length(length)
    {
    }

    std::int32_t BitStreamReader::ReadBit()
    {
        if (!_buffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t bytePosition = _bitPosition / 8;
        const std::int32_t bitPosition = _bitPosition % 8;
        _bitPosition = WrapInt32Add(_bitPosition, 1);
        if (bytePosition < 0
            || static_cast<std::size_t>(bytePosition) >= _buffer->size())
        {
            throw System::IndexOutOfRangeException();
        }
        return ((*_buffer)[static_cast<std::size_t>(bytePosition)]
            >> WrapInt32Subtract(7, bitPosition)) & 1;
    }

    std::int32_t BitStreamReader::ConsumeUntilNotZero()
    {
        std::int32_t count = 0;
        while (ReadBit() == 0)
        {
            count = WrapInt32Add(count, 1);
        }
        return count;
    }

    std::int32_t BitStreamReader::ReadUnsignedExpGolomb()
    {
        const std::int32_t zeroCount = ConsumeUntilNotZero();
        assert(zeroCount <= 30);
        std::int32_t value = WrapInt32ShiftLeft(1, zeroCount);
        for (std::int32_t i = 0; i < zeroCount; i = WrapInt32Add(i, 1))
        {
            const std::int32_t shift = WrapInt32Subtract(
                WrapInt32Subtract(zeroCount, i), 1);
            value |= WrapInt32ShiftLeft(ReadBit(), shift);
        }
        return WrapInt32Add(value, -1);
    }

    std::int32_t BitStreamReader::ReadSignedExpGolomb()
    {
        const std::int32_t value = WrapInt32Add(ReadUnsignedExpGolomb(), 1);
        const std::int32_t sign = WrapInt32Add(
            WrapInt32Multiply(value & 1, -2), 1);
        return WrapInt32Multiply(ArithmeticInt32ShiftRight(value, 1), sign);
    }

    std::int32_t BitStreamReader::ReadInt(std::int32_t bitCount)
    {
        assert(bitCount >= 0 && bitCount <= 32);
        std::int32_t value = 0;
        for (std::int32_t i = 0; i < bitCount; i = WrapInt32Add(i, 1))
        {
            const std::int32_t shift = WrapInt32Subtract(
                WrapInt32Subtract(bitCount, i), 1);
            value |= WrapInt32ShiftLeft(ReadBit(), shift);
        }
        return value;
    }

    std::int32_t BitStreamReader::ReadVLC2(const VLCData& vlc)
    {
        std::int32_t bitCount = 1;
        std::int32_t hashCode = MovieNativeRuntime::HashCombine(0, ReadBit());
        std::int32_t index = vlc.FindBitPattern(hashCode);
        while (index == -1)
        {
            assert(bitCount < vlc.MaxBitCount());
            bitCount = WrapInt32Add(bitCount, 1);
            hashCode = MovieNativeRuntime::HashCombine(hashCode, ReadBit());
            index = vlc.FindBitPattern(hashCode);
        }
        return index;
    }

    void BitStreamReader::EnsureWordAlignment()
    {
        while (_bitPosition % 16 != 0)
        {
            _bitPosition = WrapInt32Add(_bitPosition, 1);
        }
    }

    const std::array<std::int32_t, 32> VideoFrame::_residueMaskTable{
        0x00, 0x08, 0x04, 0x02, 0x01, 0x1F, 0x0F, 0x0A,
        0x05, 0x0C, 0x03, 0x10, 0x0E, 0x0D, 0x0B, 0x07,
        0x09, 0x06, 0x1E, 0x1B, 0x1A, 0x1D, 0x17, 0x15,
        0x18, 0x12, 0x11, 0x1C, 0x14, 0x13, 0x16, 0x19
    };

    const std::array<std::int32_t, 17> VideoFrame::_tokenIndexTable{
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    const std::array<std::int32_t, 7> VideoFrame::_suffixLimits{
        0, 3, 6, 12, 24, 48, 0x8000
    };

    const std::array<std::int32_t, 16> VideoFrame::_zigzagScanTable{
        0 * 4 + 0, 1 * 4 + 0, 0 * 4 + 1, 0 * 4 + 2,
        1 * 4 + 1, 2 * 4 + 0, 3 * 4 + 0, 2 * 4 + 1,
        1 * 4 + 2, 0 * 4 + 3, 1 * 4 + 3, 2 * 4 + 2,
        3 * 4 + 1, 3 * 4 + 2, 2 * 4 + 3, 3 * 4 + 3
    };

    VideoFrame::VideoFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, const VxBuffers& buffers)
        : FrameWidth(frameWidth),
          FrameHeight(frameHeight),
          _planeBufferY(buffers.PlaneBufferY),
          _planeBufferU(buffers.PlaneBufferU),
          _planeBufferV(buffers.PlaneBufferV),
          _coeffBufferY(buffers.CoeffBufferY),
          _coeffBufferUV(buffers.CoeffBufferUV),
          _vectors(buffers.Vectors),
          _prevVideoFrames(buffers.PrevVideoFrames),
          _quantizerTable(buffers.QuantizerTable)
    {
    }

    void VideoFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;

        if (!_planeBufferY)
        {
            _planeBufferY = std::make_shared<ByteArray2D>(FrameHeight, FrameWidth);
            _planeBufferU = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
            _planeBufferV = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
        }

        for (std::int32_t y = 0; y < FrameHeight; y += 16)
        {
            for (std::int32_t x = 0; x < FrameWidth; x += 16)
            {
                const Vector2ir predictionVector(
                    GetMiddleValue(
                        (*_vectors)((y / 16) + 1, (x / 16) + 0).X,
                        (*_vectors)((y / 16) + 0, (x / 16) + 1).X,
                        (*_vectors)((y / 16) + 0, (x / 16) + 2).X),
                    GetMiddleValue(
                        (*_vectors)((y / 16) + 1, (x / 16) + 0).Y,
                        (*_vectors)((y / 16) + 0, (x / 16) + 1).Y,
                        (*_vectors)((y / 16) + 0, (x / 16) + 2).Y));
                DecodeBlock(Block(x, y, 16, 16), predictionVector);
            }
        }

        _reader->EnsureWordAlignment();
        _reader = nullptr;
    }

    std::int32_t VideoFrame::GetMiddleValue(
        std::int32_t a, std::int32_t b, std::int32_t c) noexcept
    {
        std::array<std::int32_t, 3> array{a, b, c};
        std::sort(array.begin(), array.end());
        return array[1];
    }

    std::uint8_t VideoFrame::PlaneBufferGetter(
        const ByteArray2D& planeBuffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return planeBuffer(y / step, x / step);
    }

    void VideoFrame::DecodeBlock(Block block, Vector2ir predictionVector)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();

        if (mode == 0)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 003");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            if (block.W == 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 1)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 2)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 004");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            if (block.W >= 8 && block.H == 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 3)
        {
            PredictInterDC(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 4)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 5)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 6)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 7)
        {
            PredictMBPlane(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 8)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 005");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 9)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 10)
        {
            PredictInterDC(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 11)
        {
            PredictNoTile(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 12)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 13)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 006");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 14)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 15)
        {
            Predict4(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 16)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 17)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 18)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 19)
        {
            Predict4(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 20)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 21)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 22)
        {
            PredictNoTile(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 23)
        {
            PredictMBPlane(block);
            DecodeResidueBlocks(block);
        }
        else
        {
            throw ProgramException("VX decoding error 007: " + std::to_string(mode));
        }
    }

    void VideoFrame::PredictInter(
        Block block, Vector2ir predictionVector, bool hasDelta,
        const std::shared_ptr<VideoFrame>& prevVideoFrame)
    {
        assert(prevVideoFrame);
        if (!prevVideoFrame)
        {
            throw System::NullReferenceException();
        }

        if (hasDelta)
        {
            predictionVector = Vector2ir(
                WrapInt32Add(predictionVector.X, _reader->ReadSignedExpGolomb()),
                WrapInt32Add(predictionVector.Y, _reader->ReadSignedExpGolomb()));
        }

        (*_vectors)((block.Y / 16) + 1, (block.X / 16) + 1) = predictionVector;

        for (std::int32_t y = block.Y; y < block.Y + block.H; ++y)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; ++x)
            {
                (*_planeBufferY)(y, x) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferY, 1,
                    WrapInt32Add(x, predictionVector.X),
                    WrapInt32Add(y, predictionVector.Y));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                (*_planeBufferU)(y / 2, x / 2) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferU, 2,
                    WrapInt32Add(x, predictionVector.X),
                    WrapInt32Add(y, predictionVector.Y));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                (*_planeBufferV)(y / 2, x / 2) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferV, 2,
                    WrapInt32Add(x, predictionVector.X),
                    WrapInt32Add(y, predictionVector.Y));
            }
        }
    }

    void VideoFrame::PredictInterDC(Block block)
    {
        const Vector2ir vec(_reader->ReadSignedExpGolomb(), _reader->ReadSignedExpGolomb());

        const std::int32_t sourceX = WrapInt32Add(block.X, vec.X);
        const std::int32_t sourceY = WrapInt32Add(block.Y, vec.Y);
        const std::int32_t sourceRight = WrapInt32Add(sourceX, block.W);
        const std::int32_t sourceBottom = WrapInt32Add(sourceY, block.H);
        if (sourceX < 0 || sourceRight > FrameWidth
            || sourceY < 0 || sourceBottom > FrameHeight)
        {
            throw ProgramException("VX decoding error 008");
        }

        std::int32_t dcY = _reader->ReadSignedExpGolomb();
        if (dcY < -(1 << 16) || dcY >= (1 << 16))
        {
            throw ProgramException("VX decoding error 009");
        }
        dcY *= 2;

        std::int32_t dcU = _reader->ReadSignedExpGolomb();
        if (dcU < -(1 << 16) || dcU >= (1 << 16))
        {
            throw ProgramException("VX decoding error 010");
        }
        dcU *= 2;

        std::int32_t dcV = _reader->ReadSignedExpGolomb();
        if (dcV < -(1 << 16) || dcV >= (1 << 16))
        {
            throw ProgramException("VX decoding error 011");
        }
        dcV *= 2;

        const std::shared_ptr<VideoFrame>& prevVideoFrame = (*_prevVideoFrames)[0];
        assert(prevVideoFrame);

        for (std::int32_t y = block.Y; y < block.Y + block.H; ++y)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; ++x)
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferY, 1,
                    WrapInt32Add(x, vec.X), WrapInt32Add(y, vec.Y));
                const std::int32_t pixel = WrapInt32Add(predicted, dcY);
                (*_planeBufferY)(y, x) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferU, 2,
                    WrapInt32Add(x, vec.X), WrapInt32Add(y, vec.Y));
                const std::int32_t pixel = WrapInt32Add(predicted, dcU);
                (*_planeBufferU)(y / 2, x / 2) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                const std::int32_t predicted = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferV, 2,
                    WrapInt32Add(x, vec.X), WrapInt32Add(y, vec.Y));
                const std::int32_t pixel = WrapInt32Add(predicted, dcV);
                (*_planeBufferV)(y / 2, x / 2) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
    }

    void VideoFrame::PredictMBPlane(Block block)
    {
        std::int32_t value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 012: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferY, 1, WrapInt32Multiply(value, 2));

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 013: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferU, 2, WrapInt32Multiply(value, 2));

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 014: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferV, 2, WrapInt32Multiply(value, 2));
    }

    void VideoFrame::DecodeResidueBlocks(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y += 8)
        {
            for (std::int32_t x = 0; x < block.W; x += 8)
            {
                const std::int32_t index = _reader->ReadUnsignedExpGolomb();
                if (index > 31)
                {
                    throw ProgramException("VX decoding error 015: " + std::to_string(index));
                }
                const std::int32_t residueMask = _residueMaskTable.at(static_cast<std::size_t>(index));

                if ((residueMask & 1) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y, 0);
                }

                if ((residueMask & 2) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4 - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x + 4, block.Y + y, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y, 0);
                }

                if ((residueMask & 4) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x - 1, block.Y + y + 4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x, block.Y + y + 4 - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x, block.Y + y + 4, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4, 0);
                }

                if ((residueMask & 8) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4 - 1, block.Y + y + 4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4 - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x + 4, block.Y + y + 4, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4, 0);
                }

                if ((residueMask & 16) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferUV, 2, block.X + x - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferUV, 2, block.X + x, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t totalCoeffU = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferU, 2);
                    const std::int32_t totalCoeffV = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferV, 2);
                    const std::int32_t outTotalCoeff = (totalCoeffU + totalCoeffV + 1) / 2;
                    SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y, 0);
                }
            }
        }
    }

    std::int32_t VideoFrame::DecodeResidueCAVLC(
        std::int32_t x, std::int32_t y, std::int32_t nc,
        ByteArray2D& planeBuffer, std::int32_t step)
    {
        const std::int32_t coeffToken = _reader->ReadVLC2(
            VLC::CoeffTokenVlc().at(
                static_cast<std::size_t>(_tokenIndexTable.at(static_cast<std::size_t>(nc)))));
        if (coeffToken == -1)
        {
            throw ProgramException("VX decoding error 016");
        }

        std::int32_t trailingOnes = coeffToken & 3;
        std::int32_t totalCoeff = coeffToken >> 2;
        const std::int32_t outTotalCoeff = totalCoeff;
        if (totalCoeff == 0)
        {
            return outTotalCoeff;
        }

        std::array<std::int32_t, 16> level;
        std::int32_t levelPos = 0;
        std::int32_t zeroesRemaining;
        if (totalCoeff == 16)
        {
            zeroesRemaining = 0;
        }
        else
        {
            zeroesRemaining = _reader->ReadVLC2(
                VLC::TotalZeroesVlc().at(static_cast<std::size_t>(totalCoeff)));
            for (std::int32_t i = 0; i < 16 - (totalCoeff + zeroesRemaining); ++i)
            {
                level.at(static_cast<std::size_t>(levelPos++)) = 0;
            }
        }

        std::int32_t suffixLength = 0;
        while (true)
        {
            if (trailingOnes > 0)
            {
                --trailingOnes;
                level.at(static_cast<std::size_t>(levelPos++))
                    = _reader->ReadBit() == 0 ? 1 : -1;
            }
            else
            {
                std::int32_t levelPrefix = 0;
                while (_reader->ReadBit() == 0)
                {
                    levelPrefix = WrapInt32Add(levelPrefix, 1);
                }

                std::int32_t levelSuffix;
                if (levelPrefix == 15)
                {
                    levelSuffix = _reader->ReadInt(11);
                }
                else
                {
                    levelSuffix = _reader->ReadInt(suffixLength);
                }

                std::int32_t levelCode = WrapInt32Add3(
                    WrapInt32ShiftLeft(levelPrefix, suffixLength), levelSuffix, 1);
                const std::int32_t suffixIndex = WrapInt32Add(suffixLength, 1);
                if (suffixIndex < 0
                    || static_cast<std::size_t>(suffixIndex) >= _suffixLimits.size())
                {
                    throw System::IndexOutOfRangeException();
                }
                if (levelCode > _suffixLimits[static_cast<std::size_t>(suffixIndex)])
                {
                    suffixLength = WrapInt32Add(suffixLength, 1);
                }
                if (_reader->ReadBit() == 1)
                {
                    levelCode = WrapInt32Negate(levelCode);
                }
                level.at(static_cast<std::size_t>(levelPos++)) = levelCode;
            }

            --totalCoeff;
            if (totalCoeff == 0)
            {
                break;
            }
            if (zeroesRemaining == 0)
            {
                continue;
            }

            std::int32_t runBefore;
            if (zeroesRemaining < 7)
            {
                runBefore = _reader->ReadVLC2(
                    VLC::RunVlc().at(static_cast<std::size_t>(zeroesRemaining)));
            }
            else
            {
                runBefore = _reader->ReadVLC2(VLC::Run7Vlc());
            }

            zeroesRemaining = WrapInt32Subtract(zeroesRemaining, runBefore);
            for (std::int32_t i = 0; i < runBefore; ++i)
            {
                level.at(static_cast<std::size_t>(levelPos++)) = 0;
            }
        }

        for (std::int32_t i = 0; i < zeroesRemaining; ++i)
        {
            level.at(static_cast<std::size_t>(levelPos++)) = 0;
        }

        assert(levelPos == 16);
        DecodeDct(x, y, planeBuffer, step, level);
        return outTotalCoeff;
    }

    void VideoFrame::DecodeDct(
        std::int32_t x, std::int32_t y, ByteArray2D& planeBuffer,
        std::int32_t step, std::span<const std::int32_t> level)
    {
        std::array<std::int32_t, 16> dct;

        for (std::size_t i = 0; i < _zigzagScanTable.size(); ++i)
        {
            const std::int32_t z = _zigzagScanTable[i];
            dct.at(static_cast<std::size_t>(z)) = WrapInt32Multiply(
                level[15U - i],
                _quantizerTable->at(static_cast<std::size_t>(
                    (z & 1) + ((z >> 2) & 1))));
        }

        dct[0] = WrapInt32Add(dct[0], WrapInt32ShiftLeft(1, 5));

        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::size_t si = static_cast<std::size_t>(i);
            const std::int32_t z0 = WrapInt32Add(
                dct[si + 4U * 0U], dct[si + 4U * 2U]);
            const std::int32_t z1 = WrapInt32Subtract(
                dct[si + 4U * 0U], dct[si + 4U * 2U]);
            const std::int32_t z2 = WrapInt32Subtract(
                dct[si + 4U * 1U] / 2, dct[si + 4U * 3U]);
            const std::int32_t z3 = WrapInt32Add(
                dct[si + 4U * 1U], dct[si + 4U * 3U] / 2);

            dct[si + 4U * 0U] = WrapInt32Add(z0, z3);
            dct[si + 4U * 1U] = WrapInt32Add(z1, z2);
            dct[si + 4U * 2U] = WrapInt32Subtract(z1, z2);
            dct[si + 4U * 3U] = WrapInt32Subtract(z0, z3);
        }

        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::size_t base = 4U * static_cast<std::size_t>(i);
            const std::int32_t z0 = WrapInt32Add(dct[0U + base], dct[2U + base]);
            const std::int32_t z1 = WrapInt32Subtract(dct[0U + base], dct[2U + base]);
            const std::int32_t z2 = WrapInt32Subtract(dct[1U + base] / 2, dct[3U + base]);
            const std::int32_t z3 = WrapInt32Add(dct[1U + base], dct[3U + base] / 2);

            const std::int32_t bx = WrapInt32Add(x, WrapInt32Multiply(step, i));
            std::int32_t by = y;
            std::int32_t pixel = WrapInt32Add(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(WrapInt32Add(z0, z3), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = WrapInt32Add(y, step);
            pixel = WrapInt32Add(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(WrapInt32Add(z1, z2), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = WrapInt32Add(y, WrapInt32Multiply(step, 2));
            pixel = WrapInt32Add(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(WrapInt32Subtract(z1, z2), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = WrapInt32Add(y, WrapInt32Multiply(step, 3));
            pixel = WrapInt32Add(
                PlaneBufferGetter(planeBuffer, step, bx, by),
                ArithmeticInt32ShiftRight(WrapInt32Subtract(z0, z3), 6));
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
        }
    }

    void VideoFrame::PredictNoTile(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictVertical(block, *_planeBufferY, 1);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, *_planeBufferY, 1);
        }
        else if (mode == 2)
        {
            PredictDC(block, *_planeBufferY, 1);
        }
        else if (mode == 3)
        {
            PredictPlane(block, *_planeBufferY, 1, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 017: " + std::to_string(mode));
        }
        PredictNoTileUV(block);
    }

    void VideoFrame::PredictVertical(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, x, block.Y - 1);
            }
        }
    }

    void VideoFrame::PredictHorizontal(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, block.X - 1, y);
            }
        }
    }

    void VideoFrame::PredictDC(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        std::uint8_t dc = 128;
        if (block.X != 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; ++x)
            {
                sumX += PlaneBufferGetter(planeBuffer, step, block.X + x, block.Y - 1);
            }
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; ++y)
            {
                sumY += PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + y);
            }
            dc = static_cast<std::uint8_t>(((sumX / block.W) + (sumY / block.H) + 1) / 2);
        }
        else if (block.X == 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; ++x)
            {
                sumX += PlaneBufferGetter(planeBuffer, step, block.X + x, block.Y - 1);
            }
            dc = static_cast<std::uint8_t>(sumX / block.W);
        }
        else if (block.X != 0 && block.Y == 0)
        {
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; ++y)
            {
                sumY += PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + y);
            }
            dc = static_cast<std::uint8_t>(sumY / block.H);
        }

        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step) = dc;
            }
        }
    }

    void VideoFrame::PredictPlane(
        Block block, ByteArray2D& planeBuffer, std::int32_t step, std::int32_t value)
    {
        const std::int32_t bottomLeft
            = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + block.H - 1);
        const std::int32_t topRight
            = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y - 1);
        const std::int32_t pixel = (bottomLeft + topRight + 1) / 2 + value;
        const std::int32_t x = block.X + block.W - 1;
        const std::int32_t y = block.Y + block.H - 1;
        planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
        PredictPlaneRecursive(block, planeBuffer, step);
    }

    void VideoFrame::PredictPlaneRecursive(
        Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        if (block.W == step && block.H == step)
        {
            return;
        }
        if (block.W == step && block.H > step)
        {
            const std::int32_t top
                = PlaneBufferGetter(planeBuffer, step, block.X, block.Y - 1);
            const std::int32_t bottom
                = PlaneBufferGetter(planeBuffer, step, block.X, block.Y + block.H - 1);
            const std::int32_t pixel = (top + bottom) / 2;
            const std::int32_t x = block.X;
            const std::int32_t y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfDown(), planeBuffer, step);
        }
        else if (block.W > step && block.H == step)
        {
            const std::int32_t left
                = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y);
            const std::int32_t right
                = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y);
            const std::int32_t pixel = (left + right) / 2;
            const std::int32_t x = block.X + (block.W / 2) - 1;
            const std::int32_t y = block.Y;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight(), planeBuffer, step);
        }
        else
        {
            const std::int32_t bottomLeft
                = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + block.H - 1);
            const std::int32_t topRight
                = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y - 1);
            const std::int32_t bottomRight
                = PlaneBufferGetter(planeBuffer, step,
                    block.X + block.W - 1, block.Y + block.H - 1);
            const std::int32_t bottomCenter = (bottomLeft + bottomRight) / 2;
            const std::int32_t centerRight = (topRight + bottomRight) / 2;
            std::int32_t pixel;
            std::int32_t x = block.X + (block.W / 2) - 1;
            std::int32_t y = block.Y + block.H - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(bottomCenter);
            x = block.X + block.W - 1;
            y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(centerRight);

            if ((block.W == 4 * step || block.W == 16 * step)
                != (block.H == 4 * step || block.H == 16 * step))
            {
                const std::int32_t centerLeft
                    = PlaneBufferGetter(planeBuffer, step,
                        block.X - 1, block.Y + (block.H / 2) - 1);
                pixel = (centerLeft + centerRight) / 2;
            }
            else
            {
                const std::int32_t topCenter
                    = PlaneBufferGetter(planeBuffer, step,
                        block.X + (block.W / 2) - 1, block.Y - 1);
                pixel = (topCenter + bottomCenter) / 2;
            }
            x = block.X + (block.W / 2) - 1;
            y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfLeft().HalfDown(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfDown(), planeBuffer, step);
        }
    }

    void VideoFrame::PredictNoTileUV(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictDC(block, *_planeBufferU, 2);
            PredictDC(block, *_planeBufferV, 2);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, *_planeBufferU, 2);
            PredictHorizontal(block, *_planeBufferV, 2);
        }
        else if (mode == 2)
        {
            PredictVertical(block, *_planeBufferU, 2);
            PredictVertical(block, *_planeBufferV, 2);
        }
        else if (mode == 3)
        {
            PredictPlane(block, *_planeBufferU, 2, 0);
            PredictPlane(block, *_planeBufferV, 2, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 018: " + std::to_string(mode));
        }
    }

    void VideoFrame::Predict4(Block block)
    {
        std::array<std::int32_t, 25> cache{};
        cache.fill(9);

        for (std::int32_t y2 = 0; y2 < block.H / 4; ++y2)
        {
            for (std::int32_t x2 = 0; x2 < block.W / 4; ++x2)
            {
                std::int32_t mode = std::min(
                    cache[static_cast<std::size_t>((1 + y2 - 1) * 5 + 1 + x2)],
                    cache[static_cast<std::size_t>((1 + y2) * 5 + 1 + x2 - 1)]);
                if (mode == 9)
                {
                    mode = 2;
                }

                if (_reader->ReadBit() == 0)
                {
                    const std::int32_t val = _reader->ReadInt(3);
                    mode = val + (val >= mode ? 1 : 0);
                }

                cache[static_cast<std::size_t>((1 + y2) * 5 + 1 + x2)] = mode;

                const Vector2ir vec(block.X + x2 * 4, block.Y + y2 * 4);
                if (mode == 0)
                {
                    Predict4x4Vertical(*_planeBufferY, vec);
                }
                else if (mode == 1)
                {
                    Predict4x4Horizontal(*_planeBufferY, vec);
                }
                else if (mode == 2)
                {
                    if (vec.X != 0 && vec.Y != 0)
                    {
                        Predict4x4Dc(*_planeBufferY, vec);
                    }
                    else if (vec.X != 0)
                    {
                        Predict4x4LeftDc(*_planeBufferY, vec);
                    }
                    else if (vec.Y != 0)
                    {
                        Predict4x4TopDc(*_planeBufferY, vec);
                    }
                    else
                    {
                        Predict4x4Dc128(*_planeBufferY, vec);
                    }
                }
                else if (mode == 3)
                {
                    Predict4x4DownLeft(*_planeBufferY, vec);
                }
                else if (mode == 4)
                {
                    Predict4x4DownRight(*_planeBufferY, vec);
                }
                else if (mode == 5)
                {
                    Predict4x4VerticalRight(*_planeBufferY, vec);
                }
                else if (mode == 6)
                {
                    Predict4x4HorizontalDown(*_planeBufferY, vec);
                }
                else if (mode == 7)
                {
                    Predict4x4VerticalLeft(*_planeBufferY, vec);
                }
                else if (mode == 8)
                {
                    Predict4x4HorizontalUp(*_planeBufferY, vec);
                }
                else
                {
                    throw ProgramException("VX decoding error 019: " + std::to_string(mode));
                }
            }
        }

        PredictNoTileUV(block);
    }

    void VideoFrame::Predict4x4Vertical(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = planeBuffer(vec.Y - 1, vec.X + x);
            }
        }
    }

    void VideoFrame::Predict4x4Horizontal(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            const std::uint8_t value = planeBuffer(vec.Y + y, vec.X - 1);
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y - 1, vec.X + 0)
                + planeBuffer(vec.Y - 1, vec.X + 1)
                + planeBuffer(vec.Y - 1, vec.X + 2)
                + planeBuffer(vec.Y - 1, vec.X + 3)
                + planeBuffer(vec.Y + 0, vec.X - 1)
                + planeBuffer(vec.Y + 1, vec.X - 1)
                + planeBuffer(vec.Y + 2, vec.X - 1)
                + planeBuffer(vec.Y + 3, vec.X - 1) + 4) / 8);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4LeftDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y + 0, vec.X - 1)
                + planeBuffer(vec.Y + 1, vec.X - 1)
                + planeBuffer(vec.Y + 2, vec.X - 1)
                + planeBuffer(vec.Y + 3, vec.X - 1) + 2) / 4);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4TopDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y - 1, vec.X + 0)
                + planeBuffer(vec.Y - 1, vec.X + 1)
                + planeBuffer(vec.Y - 1, vec.X + 2)
                + planeBuffer(vec.Y - 1, vec.X + 3) + 2) / 4);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc128(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = 128;
            }
        }
    }

    void VideoFrame::Predict4x4DownLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t t4 = planeBuffer(vec.Y - 1, vec.X + 4);
        const std::int32_t t5 = planeBuffer(vec.Y - 1, vec.X + 5);
        const std::int32_t t6 = planeBuffer(vec.Y - 1, vec.X + 6);
        const std::int32_t t7 = planeBuffer(vec.Y - 1, vec.X + 7);

        const std::array<std::int32_t, 7> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + 2 * t5 + t6 + 2) / 4,
            (t5 + 2 * t6 + t7 + 2) / 4,
            (t6 + 3 * t7 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(pixels[static_cast<std::size_t>(x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4DownRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 7> pixels{
            (l3 + 2 * l2 + l1 + 2) / 4,
            (l2 + 2 * l1 + l0 + 2) / 4,
            (l1 + 2 * l0 + lt + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);

        const std::array<std::int32_t, 10> pixels{
            (l0 + 2 * l1 + l2 + 2) / 4,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + t0 + 1) / 2,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + 2 * x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalDown(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 10> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + l0 + 1) / 2,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 - x + 2 * y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t t4 = planeBuffer(vec.Y - 1, vec.X + 4);
        const std::int32_t t5 = planeBuffer(vec.Y - 1, vec.X + 5);
        const std::int32_t t6 = planeBuffer(vec.Y - 1, vec.X + 6);

        const std::array<std::int32_t, 10> pixels{
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + t4 + 1) / 2,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + t5 + 1) / 2,
            (t4 + 2 * t5 + t6 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(2 * x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalUp(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 7> pixels{
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2,
            (l2 + 2 * l3 + l3 + 2) / 4,
            l3
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(std::min(x + 2 * y, 6))]);
            }
        }
    }

    std::uint8_t VideoFrame::GetCoeffBuffer(
        const ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return buffer(y / (step * 4) + 1, x / (step * 4) + 1);
    }

    void VideoFrame::SetCoeffBuffer(
        ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y,
        std::uint8_t value)
    {
        buffer(y / (step * 4) + 1, x / (step * 4) + 1) = value;
    }

    void VideoFrame::ClearTotalCoeff(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y += 8)
        {
            for (std::int32_t x = 0; x < block.W; x += 8)
            {
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4, 0);
                SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y, 0);
            }
        }
    }

    const std::array<std::int32_t, 4> AudioFrame::_pulseDataLengths{8, 5, 4, 3};
    const std::array<std::int32_t, 4> AudioFrame::_pulseDistances{3, 3, 4, 5};

    AudioFrame::AudioFrame(
        std::shared_ptr<AudioExtradata> extradata,
        std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers,
        std::int32_t sampleBufferIndex)
        : _extradata(std::move(extradata)),
          _prevAudioFrame(std::move(prevAudioFrame)),
          _prevSampleBuffer(buffers.PrevSampleBuffer),
          _prevPulseBuffer(buffers.PrevPulseBuffer),
          _lpcFilterBuffer(buffers.LpcFilterBuffer),
          _influenceBuffer(buffers.InfluenceBuffer),
          _sampleBuffer(buffers.SampleBuffer),
          _sampleBufferIndex(sampleBufferIndex)
    {
    }

    std::span<std::int16_t> AudioFrame::SampleBuffer()
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = WrapInt32Multiply(128, _sampleBufferIndex);
        if (start < 0 || static_cast<std::size_t>(start) + 128U > _sampleBuffer->size())
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<std::int16_t>(_sampleBuffer->data() + start, 128);
    }

    std::span<const std::int16_t> AudioFrame::SampleBuffer() const
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = WrapInt32Multiply(128, _sampleBufferIndex);
        if (start < 0 || static_cast<std::size_t>(start) + 128U > _sampleBuffer->size())
        {
            throw System::ArgumentOutOfRangeException("start");
        }
        return std::span<const std::int16_t>(_sampleBuffer->data() + start, 128);
    }

    void AudioFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;
        if (!_extradata)
        {
            throw System::NullReferenceException();
        }
        std::array<std::int32_t, 3> lpcCodebookIndices{};
        const std::int32_t header1 = reader.ReadInt(16);
        const std::int32_t header2 = reader.ReadInt(16);
        lpcCodebookIndices[0] = header1 & 0x3F;
        const std::int32_t scaleModifierIndex = (header1 >> 6) & 7;
        const std::int32_t prevFrameOffset = (header1 >> 9) & 0x7F;
        lpcCodebookIndices[2] = header2 & 0x3F;
        lpcCodebookIndices[1] = (header2 >> 6) & 0x3F;
        const std::int32_t pulsePackingMode = (header2 >> 12) & 3;
        const std::int32_t pulseStartPosition = (header2 >> 14) & 3;

        if (pulsePackingMode >= static_cast<std::int32_t>(_pulseDataLengths.size()))
        {
            throw ProgramException(
                "VX decoding error 020: " + std::to_string(pulsePackingMode));
        }
        if (scaleModifierIndex >= static_cast<std::int32_t>(_extradata->ScaleModifiers->size()))
        {
            throw ProgramException(
                "VX decoding error 021: " + std::to_string(scaleModifierIndex)
                + ", " + std::to_string(_extradata->ScaleModifiers->size()));
        }

        const std::int32_t pulseDataLength
            = _pulseDataLengths[static_cast<std::size_t>(pulsePackingMode)];
        std::array<std::uint16_t, 8> pulseData{};
        for (std::int32_t i = 0; i < pulseDataLength; ++i)
        {
            pulseData[static_cast<std::size_t>(i)]
                = static_cast<std::uint16_t>(reader.ReadInt(16));
        }

        std::array<std::int32_t, 42> pulseValues{};
        std::int32_t pulseValueLength
            = pulsePackingMode == 0 ? 42 : WrapInt32Multiply(pulseDataLength, 8);
        if (pulsePackingMode == 0)
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 13; j >= 0; j -= 3)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 7) * 2 - 7;
                }
            }
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[0] & 1) * 4 + (pulseData[1] & 1) * 2
                    + (pulseData[2] & 1)) * 2 - 7;
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[3] & 1) * 4 + (pulseData[4] & 1) * 2
                    + (pulseData[5] & 1)) * 2 - 7;
        }
        else
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 14; j >= 0; j -= 2)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 3) * 2 - 3;
                }
            }
        }

        _scale = _extradata->ScaleInitial;
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            if (!_prevAudioFrame)
            {
                throw System::NullReferenceException();
            }
            _scale = _prevAudioFrame->Scale();
        }
        _scale = WrapInt32Multiply(
            _scale,
            (*_extradata->ScaleModifiers)[static_cast<std::size_t>(scaleModifierIndex)]) / 8192;

        const std::int32_t pulseDistance
            = _pulseDistances[static_cast<std::size_t>(pulsePackingMode)];

        std::array<std::int32_t, 128> pulseBuffer;
        FillManagedStackallocUnspecified(pulseBuffer);
        if (prevFrameOffset < 126)
        {
            for (std::int32_t i = 0; i < 128; i = WrapInt32Add(i, 1))
            {
                const std::int32_t volume = std::min(8,
                    std::min(WrapInt32Add(i, 1), WrapInt32Subtract(128, i)));
                const std::int32_t previousIndex = WrapInt32Subtract(
                    WrapInt32Add(i, 127), prevFrameOffset);
                pulseBuffer[static_cast<std::size_t>(i)] = WrapInt32Multiply(
                    (*_prevPulseBuffer)[static_cast<std::size_t>(previousIndex)], volume) / 16;
            }
        }

        for (std::int32_t i = 0; i < 128; i = WrapInt32Add(i, 1))
        {
            const std::int32_t dividend = WrapInt32Subtract(i, pulseStartPosition);
            const std::int32_t index = dividend / pulseDistance;
            const std::int32_t remainder = dividend % pulseDistance;
            if (remainder == 0 && index >= 0 && index < pulseValueLength)
            {
                pulseBuffer[static_cast<std::size_t>(i)] = WrapInt32Add(
                    pulseBuffer[static_cast<std::size_t>(i)],
                    WrapInt32Multiply(pulseValues[static_cast<std::size_t>(index)], _scale));
            }
        }

        if (prevFrameOffset == 127)
        {
            std::copy(_extradata->LpcBase->begin(), _extradata->LpcBase->end(),
                _lpcFilterBuffer->begin());
        }

        for (std::int32_t i = 0; i < 8; i = WrapInt32Add(i, 1))
        {
            std::int32_t coeffSum = 0;
            for (std::int32_t j = 0; j < 3; j = WrapInt32Add(j, 1))
            {
                const std::int32_t index = lpcCodebookIndices[static_cast<std::size_t>(j)];
                coeffSum = WrapInt32Add(coeffSum,
                    (*_extradata->LpcCodebooks)(j, index, i));
            }
            (*_lpcFilterBuffer)[static_cast<std::size_t>(i)] = WrapInt32Add(
                (*_lpcFilterBuffer)[static_cast<std::size_t>(i)], coeffSum);
        }

        std::array<std::int32_t, 8> influenceValues{};
        std::array<std::int32_t, 8> influenceTemp{};
        for (std::int32_t i = 0; i < 8; i = WrapInt32Add(i, 1))
        {
            std::copy(influenceValues.begin(), influenceValues.end(), influenceTemp.begin());
            const std::int32_t coeff
                = (*_lpcFilterBuffer)[static_cast<std::size_t>(i)];
            for (std::int32_t j = 0; j < i; j = WrapInt32Add(j, 1))
            {
                const std::int32_t sourceIndex = WrapInt32Subtract(
                    WrapInt32Subtract(i, j), 1);
                const std::int32_t contribution = WrapInt32Multiply(
                    influenceTemp[static_cast<std::size_t>(sourceIndex)], coeff) / 32768;
                influenceValues[static_cast<std::size_t>(j)] = WrapInt32Add(
                    influenceValues[static_cast<std::size_t>(j)], contribution);
            }
            influenceValues[static_cast<std::size_t>(i)] = coeff;
        }
        for (std::int32_t i = 0; i < 8; i = WrapInt32Add(i, 1))
        {
            influenceValues[static_cast<std::size_t>(i)] /= -2;
        }

        std::array<std::int32_t, 32> influenceQuarters{};
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            std::copy(influenceValues.begin(), influenceValues.end(),
                influenceQuarters.begin() + 24);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(8 + i)]
                    = WrapInt32Add((*_influenceBuffer)[static_cast<std::size_t>(i)],
                        influenceQuarters[static_cast<std::size_t>(WrapInt32Add(24, i))]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(i)]
                    = WrapInt32Add((*_influenceBuffer)[static_cast<std::size_t>(i)],
                        influenceQuarters[static_cast<std::size_t>(WrapInt32Add(8, i))]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(16 + i)]
                    = WrapInt32Add(
                        influenceQuarters[static_cast<std::size_t>(WrapInt32Add(8, i))],
                        influenceQuarters[static_cast<std::size_t>(WrapInt32Add(24, i))]) / 2;
            }
        }
        else
        {
            for (std::int32_t q = 0; q < 4; ++q)
            {
                std::copy(influenceValues.begin(), influenceValues.end(),
                    influenceQuarters.begin() + q * 8);
            }
        }

        std::span<std::int16_t> sampleBuffer = SampleBuffer();
        std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0);
        for (std::int32_t i = 0; i < 128; i = WrapInt32Add(i, 1))
        {
            const std::int32_t quarterStart = WrapInt32Multiply(
                WrapInt32Multiply(i, 4) / 128, 8);
            std::int32_t sample = WrapInt32Multiply(
                pulseBuffer[static_cast<std::size_t>(i)], 16384);
            for (std::int32_t j = 0; j < 8; j = WrapInt32Add(j, 1))
            {
                const std::int32_t sampleIndex = WrapInt32Subtract(
                    WrapInt32Subtract(i, j), 1);
                const std::int32_t prevSample = sampleIndex >= 0
                    ? sampleBuffer[static_cast<std::size_t>(sampleIndex)]
                    : (*_prevSampleBuffer)[static_cast<std::size_t>(WrapInt32Add(sampleIndex, 8))];
                sample = WrapInt32Add(sample, WrapInt32Multiply(
                    prevSample,
                    influenceQuarters[static_cast<std::size_t>(
                        WrapInt32Add(quarterStart, j))]));
            }
            sample /= 16384;
            sampleBuffer[static_cast<std::size_t>(i)]
                = static_cast<std::int16_t>(
                    ClampInt(sample, std::numeric_limits<std::int16_t>::min(),
                        std::numeric_limits<std::int16_t>::max()));
        }

        std::copy(_prevPulseBuffer->begin() + 128, _prevPulseBuffer->end(),
            _prevPulseBuffer->begin());
        std::copy(pulseBuffer.begin(), pulseBuffer.end(), _prevPulseBuffer->begin() + 128);
        std::copy(sampleBuffer.begin() + 120, sampleBuffer.end(), _prevSampleBuffer->begin());
        std::copy(influenceValues.begin(), influenceValues.end(), _influenceBuffer->begin());
        _reader = nullptr;
    }

    VLCData::VLCData(
        std::span<const std::int32_t> lengthList, std::span<const std::int32_t> bitList)
    {
        for (std::size_t i = 0; i < lengthList.size(); ++i)
        {
            if (i >= bitList.size())
            {
                throw System::IndexOutOfRangeException();
            }
            const std::int32_t length = lengthList[i];
            const std::int32_t value = bitList[i];
            if (length != 0)
            {
                if (length < 0)
                {
                    throw System::ArgumentOutOfRangeException("totalWidth");
                }
                std::string bitString;
                if (value == 0)
                {
                    bitString = "0";
                }
                else
                {
                    const std::uint32_t unsignedValue = std::bit_cast<std::uint32_t>(value);
                    const int width = 32 - std::countl_zero(unsignedValue);
                    bitString.reserve(static_cast<std::size_t>(width));
                    for (int bit = width - 1; bit >= 0; --bit)
                    {
                        bitString.push_back(
                            ((unsignedValue >> bit) & 1U) != 0U ? '1' : '0');
                    }
                }
                if (bitString.size() < static_cast<std::size_t>(length))
                {
                    bitString.insert(bitString.begin(),
                        static_cast<std::size_t>(length) - bitString.size(), '0');
                }

                std::int32_t hashCode = 0;
                for (char c : bitString)
                {
                    hashCode = MovieNativeRuntime::HashCombine(hashCode, c - '0');
                }
                const auto [it, inserted] = _bitDict.emplace(
                    hashCode, static_cast<std::int32_t>(i));
                (void)it;
                if (!inserted)
                {
                    throw System::ArgumentException(
                        "An item with the same key has already been added.");
                }
                _maxBitCount = std::max(
                    _maxBitCount, static_cast<std::int32_t>(bitString.size()));
            }
        }
    }

    std::int32_t VLCData::FindBitPattern(std::int32_t hashCode) const noexcept
    {
        const auto item = _bitDict.find(hashCode);
        return item == _bitDict.end() ? -1 : item->second;
    }

    struct VLC::State
    {
        std::vector<VLCData> CoeffTokenVlc;
        std::vector<VLCData> TotalZeroesVlc;
        std::vector<VLCData> RunVlc;
        VLCData Run7Vlc;
        std::int32_t Temp = 0;

        State()
            : Run7Vlc(
                std::array<std::int32_t, 15>{3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                std::array<std::int32_t, 15>{7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1})
        {
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenLengths{{
                {{
                    1, 0, 0, 0,
                    6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5, 10, 9, 8, 6,
                    11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9, 13, 13, 13, 10,
                    14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14, 15, 15, 15, 14,
                    16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16, 16, 16, 16, 16
                }},
                {{
                    2, 0, 0, 0,
                    6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4, 8, 6, 6, 4,
                    8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6, 11, 11, 11, 7,
                    12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11, 13, 13, 13, 12,
                    13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13, 14, 14, 14, 14
                }},
                {{
                    4, 0, 0, 0,
                    6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4, 7, 5, 5, 4,
                    7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4, 8, 7, 7, 5,
                    8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8, 9, 9, 9, 8,
                    10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
                }},
                {{
                    6, 0, 0, 0,
                    6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
                }}
            }};
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenBits{{
                {{
                    1, 0, 0, 0,
                    5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3, 7, 6, 5, 3,
                    7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4, 8, 10, 13, 4,
                    15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12, 11, 10, 13, 8,
                    15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12, 4, 6, 5, 8
                }},
                {{
                    3, 0, 0, 0,
                    11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5, 7, 6, 5, 4,
                    4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4, 11, 14, 13, 4,
                    15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8, 15, 14, 13, 12,
                    11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1, 7, 6, 5, 4
                }},
                {{
                    15, 0, 0, 0,
                    15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12, 15, 10, 11, 11,
                    11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8, 15, 14, 13, 13,
                    11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12, 8, 10, 13, 8,
                    13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6, 1, 4, 3, 2
                }},
                {{
                    3, 0, 0, 0,
                    0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11, 12, 13, 14, 15,
                    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
                }}
            }};

            static const std::array<std::vector<std::int32_t>, 16> totalZeroesLengths{{
                {},
                {1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
                {3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6},
                {4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6},
                {5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5},
                {4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5},
                {6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6},
                {6, 5, 3, 3, 3, 2, 3, 4, 3, 6},
                {6, 4, 5, 3, 2, 2, 3, 3, 6},
                {6, 6, 4, 2, 2, 3, 2, 5},
                {5, 5, 3, 2, 2, 2, 4},
                {4, 4, 3, 3, 1, 3},
                {4, 4, 2, 1, 3},
                {3, 3, 1, 2},
                {2, 2, 1},
                {1, 1}
            }};
            static const std::array<std::vector<std::int32_t>, 16> totalZeroesBits{{
                {},
                {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1},
                {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0},
                {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0},
                {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0},
                {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 5, 4, 3, 3, 2, 1, 1, 0},
                {1, 1, 1, 3, 3, 2, 2, 1, 0},
                {1, 0, 1, 3, 2, 1, 1, 1},
                {1, 0, 1, 3, 2, 1, 1},
                {0, 1, 1, 2, 1, 3},
                {0, 1, 1, 1, 1},
                {0, 1, 1, 1},
                {0, 1, 1},
                {0, 1}
            }};

            static const std::array<std::vector<std::int32_t>, 7> runLengths{{
                {}, {1, 1}, {1, 2, 2}, {2, 2, 2, 2},
                {2, 2, 2, 3, 3}, {2, 2, 3, 3, 3, 3}, {2, 3, 3, 3, 3, 3, 3}
            }};
            static const std::array<std::vector<std::int32_t>, 7> runBits{{
                {}, {1, 0}, {1, 1, 0}, {3, 2, 1, 0},
                {3, 2, 1, 1, 0}, {3, 2, 3, 2, 1, 0}, {3, 0, 1, 3, 2, 5, 4}
            }};

            CoeffTokenVlc.reserve(coeffTokenLengths.size());
            for (std::size_t i = 0; i < coeffTokenLengths.size(); ++i)
            {
                CoeffTokenVlc.emplace_back(coeffTokenLengths[i], coeffTokenBits[i]);
            }

            TotalZeroesVlc.reserve(totalZeroesLengths.size());
            for (std::size_t i = 0; i < totalZeroesLengths.size(); ++i)
            {
                TotalZeroesVlc.emplace_back(totalZeroesLengths[i], totalZeroesBits[i]);
            }

            RunVlc.reserve(runLengths.size());
            for (std::size_t i = 0; i < runLengths.size(); ++i)
            {
                RunVlc.emplace_back(runLengths[i], runBits[i]);
            }

            Temp = 1;
        }
    };

    const VLC::State& VLC::GetState()
    {
        static const State state;
        return state;
    }

    std::int32_t VLC::Temp()
    {
        return GetState().Temp;
    }

    const std::vector<VLCData>& VLC::CoeffTokenVlc()
    {
        return GetState().CoeffTokenVlc;
    }

    const std::vector<VLCData>& VLC::TotalZeroesVlc()
    {
        return GetState().TotalZeroesVlc;
    }

    const std::vector<VLCData>& VLC::RunVlc()
    {
        return GetState().RunVlc;
    }

    const VLCData& VLC::Run7Vlc()
    {
        return GetState().Run7Vlc;
    }
}

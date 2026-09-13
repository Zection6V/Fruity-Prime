#pragma once

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace NCSFCommon::NC
{
    class NDSStandardHeader
    {
    public:
        virtual ~NDSStandardHeader() = default;

        NDSStandardHeader(const NDSStandardHeader&) = delete;
        NDSStandardHeader(NDSStandardHeader&&) = delete;
        NDSStandardHeader& operator=(const NDSStandardHeader&) = delete;
        NDSStandardHeader& operator=(NDSStandardHeader&&) = delete;

        [[nodiscard]] std::span<const std::uint8_t> Header() const;

        [[nodiscard]] virtual std::uint32_t Magic() const = 0;
        [[nodiscard]] virtual std::uint32_t FileSize() const = 0;
        [[nodiscard]] virtual std::uint16_t HeaderSize() const = 0;
        [[nodiscard]] virtual std::uint16_t Blocks() const = 0;

        virtual void Write(std::span<std::uint8_t> span);

    protected:
        NDSStandardHeader() = default;

        [[nodiscard]] virtual std::vector<std::uint8_t> ExpectedHeader() const = 0;

        void Read(std::span<const std::uint8_t> span) const;

    private:
        enum class ExpectedHeaderState : std::uint8_t
        {
            Uninitialized,
            Initializing,
            Value,
            Error
        };

        [[nodiscard]] const std::vector<std::uint8_t>& ExpectedHeaderValue() const;

        mutable std::mutex _expectedHeaderMutex;
        mutable std::condition_variable _expectedHeaderCondition;
        mutable ExpectedHeaderState _expectedHeaderState = ExpectedHeaderState::Uninitialized;
        mutable std::thread::id _expectedHeaderThread;
        mutable std::vector<std::uint8_t> _expectedHeader;
        mutable std::exception_ptr _expectedHeaderError;
    };
}

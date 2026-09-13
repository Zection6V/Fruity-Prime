#pragma once

#include <cstdint>
#include <span>

namespace NCSFCommon::NC
{
    class FATRecord
    {
    public:
        FATRecord() noexcept = default;

        FATRecord(const FATRecord&) = delete;
        FATRecord(FATRecord&&) = delete;
        FATRecord& operator=(const FATRecord&) = delete;
        FATRecord& operator=(FATRecord&&) = delete;

        [[nodiscard]] std::uint32_t Offset() const noexcept;
        void Offset(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept;
        void Size(std::uint32_t value) noexcept;

        static constexpr std::uint32_t RecordSize = 0x10U;

        FATRecord& Read(std::span<const std::uint8_t> span);
        void Write(std::span<std::uint8_t> span);

    private:
        std::uint32_t _offset = 0;
        std::uint32_t _size = 0;
    };
}

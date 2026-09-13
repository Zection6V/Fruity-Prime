#pragma once

#include "FATRecord.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace NCSFCommon::NC
{
    class FATSection
    {
    public:
        FATSection() = default;

        FATSection(const FATSection&) = delete;
        FATSection& operator=(const FATSection&) = delete;
        FATSection(FATSection&&) noexcept = default;
        FATSection& operator=(FATSection&&) noexcept = default;

        [[nodiscard]] std::span<const std::shared_ptr<FATRecord>> Records() const noexcept;
        [[nodiscard]] std::uint32_t Size() const noexcept;

        void Read(std::span<const std::uint8_t> span);
        void Write(std::span<std::uint8_t> span);

        void ResizeRecords(std::uint32_t newSize);
        void SetNumberOfRecords(std::uint32_t count);

        [[nodiscard]] static FATSection Add(const FATSection* fatSection1, const FATSection* fatSection2);

        friend FATSection operator+(const FATSection& fatSection1, const FATSection& fatSection2);
        friend FATSection operator+(const FATSection& fatSection, std::nullptr_t);
        friend FATSection operator+(std::nullptr_t, const FATSection& fatSection);

    private:
        std::vector<std::shared_ptr<FATRecord>> _records;
    };
}

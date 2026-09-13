#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace NCSFCommon::NC
{
    struct SYMBRecordEntry final
    {
        std::uint32_t Offset = 0;
        std::optional<std::u16string> Name;

        friend bool operator==(const SYMBRecordEntry&, const SYMBRecordEntry&) = default;
    };

    class SYMBRecord
    {
    public:
        using Entry = SYMBRecordEntry;

        SYMBRecord() = default;

        SYMBRecord(const SYMBRecord&) = delete;
        SYMBRecord& operator=(const SYMBRecord&) = delete;
        SYMBRecord(SYMBRecord&&) noexcept = default;
        SYMBRecord& operator=(SYMBRecord&&) noexcept = default;

        [[nodiscard]] std::span<const Entry> Entries() const noexcept;

        [[nodiscard]] std::uint32_t Size() const;
        [[nodiscard]] std::uint32_t HeaderSize() const noexcept;
        [[nodiscard]] std::uint32_t SizeOfNames() const;

        void Read(std::span<const std::uint8_t> span, std::uint32_t offset);
        void FixOffsets(std::uint32_t startOffset) noexcept;
        void WriteHeader(std::span<std::uint8_t> span) const;
        void WriteData(std::span<std::uint8_t> span) const;

        friend SYMBRecord operator+(const SYMBRecord& symbRecord1, const SYMBRecord& symbRecord2);

        void SetNumberOfEntries(std::uint32_t count);
        void ExpandNumberOfEntries(std::uint32_t count);
        void SetEntry(std::uint32_t i, Entry entry);

    private:
        std::vector<Entry> _entries;
    };
}

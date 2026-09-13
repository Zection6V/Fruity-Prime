#pragma once

#include "NDSStandardHeader.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace NCSFCommon::NC
{
    class INFOEntrySEQ;

    class SSEQByteEnumerator
    {
    public:
        virtual ~SSEQByteEnumerator() = default;

        [[nodiscard]] virtual bool MoveNext() = 0;
        [[nodiscard]] virtual std::uint8_t Current() const = 0;
    };

    class SSEQByteEnumerable
    {
    public:
        virtual ~SSEQByteEnumerable() = default;

        [[nodiscard]] virtual bool TryGetNonEnumeratedCount(std::int32_t& count) const = 0;
        [[nodiscard]] virtual std::int32_t Count() const;
        [[nodiscard]] virtual std::unique_ptr<SSEQByteEnumerator> GetEnumerator() const = 0;

        [[nodiscard]] virtual bool TryGetCollectionCount(std::int32_t& count) const;
        virtual void CopyCollectionTo(std::span<std::uint8_t> destination, std::int32_t index) const;
    };

    class SSEQReadOnlyMemory final
    {
    public:
        SSEQReadOnlyMemory() noexcept = default;

        [[nodiscard]] std::size_t Length() const noexcept;
        [[nodiscard]] bool IsEmpty() const noexcept;
        [[nodiscard]] std::span<const std::uint8_t> Span() const noexcept;
        [[nodiscard]] const std::uint8_t& operator[](std::size_t index) const;

    private:
        explicit SSEQReadOnlyMemory(
            std::shared_ptr<const std::vector<std::uint8_t>> owner,
            std::size_t length) noexcept;

        std::shared_ptr<const std::vector<std::uint8_t>> _owner;
        std::size_t _length = 0;

        friend class SSEQ;
    };

    class SSEQ : public NDSStandardHeader
    {
    public:
        explicit SSEQ(
            std::optional<std::u16string> filename = std::nullopt,
            std::optional<std::u16string> originalFilename = std::nullopt);
        ~SSEQ() override = default;

        SSEQ(const SSEQ&) = delete;
        SSEQ(SSEQ&&) = delete;
        SSEQ& operator=(const SSEQ&) = delete;
        SSEQ& operator=(SSEQ&&) = delete;

        [[nodiscard]] std::uint32_t Magic() const noexcept override;
        [[nodiscard]] std::uint32_t FileSize() const noexcept override;
        [[nodiscard]] std::uint16_t HeaderSize() const noexcept override;
        [[nodiscard]] std::uint16_t Blocks() const noexcept override;

        [[nodiscard]] const std::optional<std::u16string>& Filename() const noexcept;
        void Filename(std::optional<std::u16string> value);

        [[nodiscard]] const std::optional<std::u16string>& OriginalFilename() const noexcept;
        void OriginalFilename(std::optional<std::u16string> value);

        [[nodiscard]] SSEQReadOnlyMemory Data() const noexcept;

        [[nodiscard]] std::int32_t EntryNumber() const noexcept;
        void EntryNumber(std::int32_t value) noexcept;

        [[nodiscard]] const std::shared_ptr<INFOEntrySEQ>& Info() const noexcept;
        void Info(std::shared_ptr<INFOEntrySEQ> value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept;
        [[nodiscard]] std::uint32_t DataSize() const noexcept;

        void Read(std::span<const std::uint8_t> span, bool failOnMissingFile);
        void Write(std::span<std::uint8_t> span) override;

        void ReplaceData(const SSEQByteEnumerable* newData);

        [[nodiscard]] bool Equals(const SSEQ* other) const noexcept;
        [[nodiscard]] bool Equals(const std::any& obj) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;

        [[nodiscard]] static bool EqualityOperator(const SSEQ* left, const SSEQ* right) noexcept;
        [[nodiscard]] static bool InequalityOperator(const SSEQ* left, const SSEQ* right) noexcept;

    protected:
        [[nodiscard]] std::vector<std::uint8_t> ExpectedHeader() const override;

    private:
        [[nodiscard]] std::int32_t DataCapacity() const noexcept;
        [[nodiscard]] std::span<std::uint8_t> DataSpan() noexcept;
        [[nodiscard]] std::span<const std::uint8_t> DataSpan() const noexcept;

        void GrowData(std::int32_t required);
        void SetDataCount(std::int32_t count);
        void ClearData() noexcept;
        void EnsureDataCapacity(std::int32_t capacity);
        void AddData(std::uint8_t value);
        void AddDataRange(const SSEQByteEnumerable& source);

        std::optional<std::u16string> _filename;
        std::optional<std::u16string> _originalFilename;
        std::shared_ptr<std::vector<std::uint8_t>> _dataStorage;
        std::int32_t _dataCount = 0;
        std::int32_t _entryNumber = -1;
        std::shared_ptr<INFOEntrySEQ> _info;
    };

    [[nodiscard]] bool operator==(const SSEQ& left, const SSEQ& right) noexcept;
    [[nodiscard]] bool operator!=(const SSEQ& left, const SSEQ& right) noexcept;
}

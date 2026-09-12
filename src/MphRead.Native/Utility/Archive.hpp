#pragma once

#include "../Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace MphRead::Archive
{
    namespace ArchiveDetail
    {
        template <std::size_t N>
        class ByValAnsiString final
        {
        public:
            ByValAnsiString() noexcept = default;
            explicit ByValAnsiString(const std::string& value);
            ByValAnsiString(const ByValAnsiString& other);
            ByValAnsiString& operator=(const ByValAnsiString& other);
            ~ByValAnsiString() noexcept;

            [[nodiscard]] bool IsNull() const;
            [[nodiscard]] std::shared_ptr<const std::string> ManagedValue() const;
            [[nodiscard]] bool Equals(const std::string& value) const;
            [[nodiscard]] bool operator==(const std::string& value) const { return Equals(value); }
            [[nodiscard]] bool operator!=(const std::string& value) const { return !Equals(value); }
            [[nodiscard]] const std::array<std::uint8_t, N>& WireBytes() const noexcept;

            static ByValAnsiString FromMarshaledBytes(const std::uint8_t* bytes);

        private:
            void SetManagedValue(std::shared_ptr<std::string> value) const;
            void ClearManagedValue() const;
            void ClearManagedValueNoThrow() const noexcept;
            [[nodiscard]] std::shared_ptr<std::string> TryGetManagedValue() const;

            mutable std::array<std::uint8_t, N> _wire{};
        };

        template <std::size_t N>
        class ByValCharArray final
        {
        public:
            using ManagedStorage = ManagedArray<char16_t>;

            ByValCharArray() noexcept = default;
            explicit ByValCharArray(std::shared_ptr<ManagedStorage> value);
            explicit ByValCharArray(const std::string& value);
            ByValCharArray(const ByValCharArray& other);
            ByValCharArray& operator=(const ByValCharArray& other);
            ~ByValCharArray() noexcept;

            [[nodiscard]] bool IsNull() const;
            [[nodiscard]] std::shared_ptr<ManagedStorage> ManagedValue() const;
            [[nodiscard]] std::size_t Length() const;
            [[nodiscard]] char16_t& operator[](std::size_t index) const;
            [[nodiscard]] std::string MarshalString() const;
            [[nodiscard]] std::string BinaryWriterBytes() const;
            [[nodiscard]] const std::array<std::uint8_t, N>& WireBytes() const;

            static ByValCharArray FromMarshaledBytes(const std::uint8_t* bytes);

        private:
            void SetManagedValue(std::shared_ptr<ManagedStorage> value) const;
            void ClearManagedValue() const;
            void ClearManagedValueNoThrow() const noexcept;
            [[nodiscard]] std::shared_ptr<ManagedStorage> TryGetManagedValue() const;

            mutable std::array<std::uint8_t, N> _wire{};
        };

        template <std::size_t N>
        class ByValUInt32Array final
        {
        public:
            using ManagedStorage = ManagedArray<std::uint32_t>;

            ByValUInt32Array() noexcept = default;
            explicit ByValUInt32Array(std::shared_ptr<ManagedStorage> value);
            ByValUInt32Array(const ByValUInt32Array& other);
            ByValUInt32Array& operator=(const ByValUInt32Array& other);
            ~ByValUInt32Array() noexcept;

            [[nodiscard]] bool IsNull() const;
            [[nodiscard]] std::shared_ptr<ManagedStorage> ManagedValue() const;
            [[nodiscard]] std::size_t Length() const;
            [[nodiscard]] std::uint32_t& operator[](std::size_t index) const;
            [[nodiscard]] const std::array<std::uint32_t, N>& WireValues() const;

            static ByValUInt32Array FromMarshaledBytes(const std::uint8_t* bytes);

        private:
            void SetManagedValue(std::shared_ptr<ManagedStorage> value) const;
            void ClearManagedValue() const;
            void ClearManagedValueNoThrow() const noexcept;
            [[nodiscard]] std::shared_ptr<ManagedStorage> TryGetManagedValue() const;

            mutable std::array<std::uint32_t, N> _wire{};
        };
    }

    struct ArchiveHeader final
    {
        const ArchiveDetail::ByValAnsiString<8> MagicString{};
        const std::uint32_t FileCount = 0;
        const std::uint32_t TotalSize = 0;
        const ArchiveDetail::ByValUInt32Array<4> Padding{};

        ArchiveHeader() noexcept = default;
        ArchiveHeader(std::string magicString, std::uint32_t fileCount, std::uint32_t totalSize);
        ArchiveHeader(const ArchiveHeader&) = default;
        ArchiveHeader& operator=(const ArchiveHeader& other);

        [[nodiscard]] ArchiveHeader SwapBytes() const;
        [[nodiscard]] static ArchiveHeader FromMarshaledBytes(
            const std::array<std::uint8_t, 32>& bytes);

    private:
        ArchiveHeader(ArchiveDetail::ByValAnsiString<8> magicString, std::uint32_t fileCount,
            std::uint32_t totalSize, ArchiveDetail::ByValUInt32Array<4> padding);
    };

    struct FileHeader final
    {
        const ArchiveDetail::ByValCharArray<32> Filename{};
        const std::uint32_t Offset = 0;
        const std::uint32_t PaddedFileSize = 0;
        const std::uint32_t TargetFileSize = 0;
        const ArchiveDetail::ByValUInt32Array<5> Padding{};

        FileHeader() noexcept = default;
        FileHeader(std::shared_ptr<ManagedArray<char16_t>> filename, std::uint32_t offset,
            std::uint32_t paddedFileSize, std::uint32_t targetFileSize);
        FileHeader(std::string filename, std::uint32_t offset,
            std::uint32_t paddedFileSize, std::uint32_t targetFileSize);
        FileHeader(const FileHeader&) = default;
        FileHeader& operator=(const FileHeader& other);

        [[nodiscard]] FileHeader SwapBytes() const;
        [[nodiscard]] static FileHeader FromMarshaledBytes(
            const std::array<std::uint8_t, 64>& bytes);

    private:
        FileHeader(ArchiveDetail::ByValCharArray<32> filename, std::uint32_t offset,
            std::uint32_t paddedFileSize, std::uint32_t targetFileSize,
            ArchiveDetail::ByValUInt32Array<5> padding);
    };

    class ArchiveSizes final
    {
    public:
        static const std::int32_t ArchiveHeader;
        static const std::int32_t FileHeader;

        ArchiveSizes() = delete;
    };

    class Archiver final
    {
    public:
        [[nodiscard]] static const std::string& MagicString();

        [[nodiscard]] static std::int32_t Extract(
            const std::string& path, const std::optional<std::string>& destination = std::nullopt);
        [[nodiscard]] static std::int32_t Extract(
            const std::string& path, const std::string& destination);

        static void Archive(const std::string& destinationPath,
            const std::shared_ptr<const std::vector<std::string>>& filePaths);

        [[nodiscard]] static std::uint32_t SwapBytes(std::uint32_t value) noexcept;

        Archiver() = delete;
        Archiver(const Archiver&) = delete;
        Archiver& operator=(const Archiver&) = delete;

    private:
        [[noreturn]] static void ThrowRead();
        [[noreturn]] static void ThrowWrite();
        [[nodiscard]] static std::uint32_t NearestMultiple(std::uint32_t value, std::uint32_t of) noexcept;
        static void Nop() noexcept;
    };

    static_assert(std::is_standard_layout_v<ArchiveDetail::ByValAnsiString<8>>);
    static_assert(sizeof(ArchiveDetail::ByValAnsiString<8>) == 8);
    static_assert(std::is_standard_layout_v<ArchiveDetail::ByValCharArray<32>>);
    static_assert(sizeof(ArchiveDetail::ByValCharArray<32>) == 32);
    static_assert(std::is_standard_layout_v<ArchiveDetail::ByValUInt32Array<4>>);
    static_assert(sizeof(ArchiveDetail::ByValUInt32Array<4>) == 16);
    static_assert(std::is_standard_layout_v<ArchiveDetail::ByValUInt32Array<5>>);
    static_assert(sizeof(ArchiveDetail::ByValUInt32Array<5>) == 20);

    static_assert(std::is_standard_layout_v<ArchiveHeader>);
    static_assert(sizeof(ArchiveHeader) == 32);
    static_assert(offsetof(ArchiveHeader, MagicString) == 0);
    static_assert(offsetof(ArchiveHeader, FileCount) == 8);
    static_assert(offsetof(ArchiveHeader, TotalSize) == 12);
    static_assert(offsetof(ArchiveHeader, Padding) == 16);

    static_assert(std::is_standard_layout_v<FileHeader>);
    static_assert(sizeof(FileHeader) == 64);
    static_assert(offsetof(FileHeader, Filename) == 0);
    static_assert(offsetof(FileHeader, Offset) == 32);
    static_assert(offsetof(FileHeader, PaddedFileSize) == 36);
    static_assert(offsetof(FileHeader, TargetFileSize) == 40);
    static_assert(offsetof(FileHeader, Padding) == 44);
}

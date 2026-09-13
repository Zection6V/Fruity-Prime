#pragma once

#include "../Common.hpp"
#include "FATSection.hpp"
#include "INFOSection.hpp"
#include "NDSStandardHeader.hpp"
#include "SYMBSection.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace NCSFCommon::NC
{
    class SBNK;
    class SSEQ;
    class SWAR;

    class SDAT : public NDSStandardHeader
    {
    public:
        SDAT() = default;
        ~SDAT() override = default;

        SDAT(const SDAT&) = delete;
        SDAT(SDAT&&) = delete;
        SDAT& operator=(const SDAT&) = delete;
        SDAT& operator=(SDAT&&) = delete;

        [[nodiscard]] std::uint32_t Magic() const noexcept override;
        [[nodiscard]] std::uint32_t FileSize() const noexcept override;
        [[nodiscard]] std::uint16_t HeaderSize() const noexcept override;
        [[nodiscard]] std::uint16_t Blocks() const noexcept override;

        static const std::array<std::uint8_t, 8> Signature;

        [[nodiscard]] const std::optional<std::u16string>& Filename() const noexcept;
        void Filename(std::optional<std::u16string> value);

        [[nodiscard]] std::uint32_t SYMBOffset() const noexcept;
        void SYMBOffset(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t SYMBSize() const noexcept;
        void SYMBSize(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t INFOOffset() const noexcept;
        void INFOOffset(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t INFOSize() const noexcept;
        void INFOSize(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t FATOffset() const noexcept;
        void FATOffset(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t FATSize() const noexcept;
        void FATSize(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t FILEOffset() const noexcept;
        void FILEOffset(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t FILESize() const noexcept;
        void FILESize(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint16_t Count() const noexcept;
        void Count(std::uint16_t value) noexcept;

        [[nodiscard]] NCSFCommon::NC::SYMBSection* SYMBSection() noexcept;
        [[nodiscard]] const NCSFCommon::NC::SYMBSection* SYMBSection() const noexcept;
        [[nodiscard]] NCSFCommon::NC::INFOSection& INFOSection() noexcept;
        [[nodiscard]] const NCSFCommon::NC::INFOSection& INFOSection() const noexcept;
        [[nodiscard]] NCSFCommon::NC::FATSection& FATSection() noexcept;
        [[nodiscard]] const NCSFCommon::NC::FATSection& FATSection() const noexcept;

        [[nodiscard]] bool SYMBSectionNeedsCleanup() const noexcept;
        void SYMBSectionNeedsCleanup(bool value) noexcept;

        [[nodiscard]] std::span<const std::shared_ptr<SSEQ>> SSEQs() const noexcept;
        [[nodiscard]] std::span<const std::shared_ptr<SBNK>> SBNKs() const noexcept;
        [[nodiscard]] std::span<const std::shared_ptr<SWAR>> SWARs() const noexcept;

        [[nodiscard]] const std::shared_ptr<INFOEntryPLAYER>& Player() const noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept;
        void Size(std::uint32_t value) noexcept;

        void Read(std::u16string filename, std::span<const std::uint8_t> span, bool failOnMissingFiles = true);
        void Read(
            std::u16string filename,
            std::span<const std::uint8_t> span,
            std::uint32_t sseqToLoad,
            bool failOnMissingFiles = true);

        void Write(std::span<std::uint8_t> span) override;

        [[nodiscard]] static std::shared_ptr<SDAT> Add(const SDAT& sdat1, const SDAT& sdat2);
        friend std::shared_ptr<SDAT> operator+(const SDAT& sdat1, const SDAT& sdat2);

        void Strip(
            const std::vector<std::shared_ptr<Common::KeepInfo>>& includesAndExcludes,
            bool verbose,
            bool removeExcluded = true);

        void FixOffsetsAndSizes();

    protected:
        [[nodiscard]] std::vector<std::uint8_t> ExpectedHeader() const override;

    private:
        using DuplicateDictionary = std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>>;

        static const std::array<std::uint8_t, 4> FILEHeader;

        void BaseRead(const std::u16string& filename, std::span<const std::uint8_t> span);

        [[nodiscard]] static std::uint32_t GetNonDuplicateNumber(
            std::uint32_t orig,
            const DuplicateDictionary& duplicates);

        template <typename T>
            requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
        static void OutputList(
            const std::vector<std::uint32_t>& list,
            std::span<const INFORecordEntry<T>> nameSource,
            bool multipleSDATs,
            std::u16string outputPrefix = u" ",
            std::int32_t columnWidth = 80);

        template <typename T>
            requires std::derived_from<T, INFOEntry> && std::default_initializable<T>
        static void OutputDictionary(
            const DuplicateDictionary& dictionary,
            std::span<const INFORecordEntry<T>> nameSource,
            bool multipleSDATs,
            std::int32_t columnWidth = 80);

        std::uint16_t _actualBlocks = 3;
        std::optional<std::u16string> _filename;
        std::uint32_t _symbOffset = 0;
        std::uint32_t _symbSize = 0;
        std::uint32_t _infoOffset = 0;
        std::uint32_t _infoSize = 0;
        std::uint32_t _fatOffset = 0;
        std::uint32_t _fatSize = 0;
        std::uint32_t _fileOffset = 0;
        std::uint32_t _fileSize = 0;
        std::uint16_t _count = 0;
        std::unique_ptr<NCSFCommon::NC::SYMBSection> _symbSection;
        NCSFCommon::NC::INFOSection _infoSection;
        NCSFCommon::NC::FATSection _fatSection;
        bool _symbSectionNeedsCleanup = false;
        std::vector<std::shared_ptr<SSEQ>> _sseqs;
        std::vector<std::shared_ptr<SBNK>> _sbnks;
        std::vector<std::shared_ptr<SWAR>> _swars;
        std::shared_ptr<INFOEntryPLAYER> _player;
        std::uint32_t _size = 0;
    };
}

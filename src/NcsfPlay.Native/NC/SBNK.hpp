#pragma once

#include "NDSStandardHeader.hpp"

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace NCSFCommon::NC
{
    class INFOEntryBANK;
    class SBNKInstrumentEntry;

    class SBNK : public NDSStandardHeader
    {
    public:
        explicit SBNK(std::optional<std::u16string> filename = std::nullopt);
        ~SBNK() override = default;

        SBNK(const SBNK&) = delete;
        SBNK(SBNK&&) = delete;
        SBNK& operator=(const SBNK&) = delete;
        SBNK& operator=(SBNK&&) = delete;

        [[nodiscard]] std::uint32_t Magic() const noexcept override;
        [[nodiscard]] std::uint32_t FileSize() const override;
        [[nodiscard]] std::uint16_t HeaderSize() const noexcept override;
        [[nodiscard]] std::uint16_t Blocks() const noexcept override;

        [[nodiscard]] const std::optional<std::u16string>& Filename() const noexcept;
        void Filename(std::optional<std::u16string> value);

        [[nodiscard]] std::span<const std::shared_ptr<SBNKInstrumentEntry>> Entries() const noexcept;

        [[nodiscard]] std::int32_t EntryNumber() const noexcept;
        void EntryNumber(std::int32_t value) noexcept;

        [[nodiscard]] const std::shared_ptr<INFOEntryBANK>& Info() const noexcept;
        void Info(std::shared_ptr<INFOEntryBANK> value) noexcept;

        [[nodiscard]] std::uint32_t Size() const;
        [[nodiscard]] std::uint32_t DataSize() const;

        void Read(std::span<const std::uint8_t> span, bool failOnMissingFile);
        void FixOffsets();
        void Write(std::span<std::uint8_t> span) override;
        void ReplaceInstruments(std::span<const std::shared_ptr<SBNKInstrumentEntry>> instruments);

        [[nodiscard]] bool Equals(const SBNK* other) const;
        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;

        [[nodiscard]] static bool EqualityOperator(const SBNK* left, const SBNK* right);
        [[nodiscard]] static bool InequalityOperator(const SBNK* left, const SBNK* right);

    protected:
        [[nodiscard]] std::vector<std::uint8_t> ExpectedHeader() const override;

    private:
        using Entry = std::shared_ptr<SBNKInstrumentEntry>;

        struct EntryListState final
        {
            EntryListState();

            std::shared_ptr<std::vector<Entry>> Items;
            std::int32_t Count = 0;
            std::int32_t Version = 0;
        };

        void SetEntryCount(std::int32_t count);

        std::optional<std::u16string> _filename;
        EntryListState _entries;
        std::int32_t _entryNumber = -1;
        std::shared_ptr<INFOEntryBANK> _info;
    };

    [[nodiscard]] bool operator==(const SBNK& left, const SBNK& right);
    [[nodiscard]] bool operator!=(const SBNK& left, const SBNK& right);
}

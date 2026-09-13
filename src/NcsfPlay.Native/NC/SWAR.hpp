#pragma once

#include "NDSStandardHeader.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace NCSFCommon::NC
{
    class SWAV;

    class SWAVDictionary final
    {
    public:
        struct Entry final
        {
            std::uint32_t Key;
            std::shared_ptr<SWAV> Value;
        };

        SWAVDictionary() = default;
        SWAVDictionary(const SWAVDictionary&) = delete;
        SWAVDictionary(SWAVDictionary&&) = delete;
        SWAVDictionary& operator=(const SWAVDictionary&) = delete;
        SWAVDictionary& operator=(SWAVDictionary&&) = delete;

        [[nodiscard]] std::size_t Count() const noexcept;
        [[nodiscard]] std::span<const Entry> Entries() const noexcept;
        [[nodiscard]] bool ContainsKey(std::uint32_t key) const noexcept;
        [[nodiscard]] bool TryGetValue(std::uint32_t key, std::shared_ptr<SWAV>& value) const noexcept;
        [[nodiscard]] const std::shared_ptr<SWAV>& At(std::uint32_t key) const;

        void Clear() noexcept;
        void Set(std::uint32_t key, std::shared_ptr<SWAV> value);

    private:
        std::vector<Entry> _entries;
    };

    class ReadOnlySWAVDictionary final
    {
    public:
        ReadOnlySWAVDictionary(const ReadOnlySWAVDictionary&) = delete;
        ReadOnlySWAVDictionary(ReadOnlySWAVDictionary&&) = delete;
        ReadOnlySWAVDictionary& operator=(const ReadOnlySWAVDictionary&) = delete;
        ReadOnlySWAVDictionary& operator=(ReadOnlySWAVDictionary&&) = delete;

        [[nodiscard]] std::size_t Count() const noexcept;
        [[nodiscard]] std::span<const SWAVDictionary::Entry> Entries() const noexcept;
        [[nodiscard]] bool ContainsKey(std::uint32_t key) const noexcept;
        [[nodiscard]] bool TryGetValue(std::uint32_t key, std::shared_ptr<SWAV>& value) const noexcept;
        [[nodiscard]] const std::shared_ptr<SWAV>& At(std::uint32_t key) const;

    private:
        explicit ReadOnlySWAVDictionary(const SWAVDictionary* dictionary) noexcept;

        const SWAVDictionary* _dictionary;

        friend class SWAR;
    };

    class SWAR : public NDSStandardHeader
    {
    public:
        explicit SWAR(std::optional<std::u16string> filename = std::nullopt);
        ~SWAR() override = default;

        SWAR(const SWAR&) = delete;
        SWAR(SWAR&&) = delete;
        SWAR& operator=(const SWAR&) = delete;
        SWAR& operator=(SWAR&&) = delete;

        [[nodiscard]] std::uint32_t Magic() const noexcept override;
        [[nodiscard]] std::uint32_t FileSize() const override;
        [[nodiscard]] std::uint16_t HeaderSize() const noexcept override;
        [[nodiscard]] std::uint16_t Blocks() const noexcept override;

        [[nodiscard]] const std::optional<std::u16string>& Filename() const noexcept;
        void Filename(std::optional<std::u16string> value);

        [[nodiscard]] const ReadOnlySWAVDictionary& SWAVs() const noexcept;

        [[nodiscard]] std::int32_t EntryNumber() const noexcept;
        void EntryNumber(std::int32_t value) noexcept;

        [[nodiscard]] std::uint32_t Size() const;
        [[nodiscard]] std::uint32_t DataSize() const;

        void Read(std::span<const std::uint8_t> span, bool failOnMissingFile);
        void Write(std::span<std::uint8_t> span) override;

        void ReplaceSWAVs(const SWAVDictionary* swavs);

        [[nodiscard]] bool Equals(const SWAR* other) const;
        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;

        [[nodiscard]] static bool EqualityOperator(const SWAR* left, const SWAR* right);
        [[nodiscard]] static bool InequalityOperator(const SWAR* left, const SWAR* right);

    protected:
        [[nodiscard]] std::vector<std::uint8_t> ExpectedHeader() const override;

    private:
        std::optional<std::u16string> _filename;
        SWAVDictionary _swavs;
        ReadOnlySWAVDictionary _swavsView;
        std::int32_t _entryNumber = -1;
    };

    [[nodiscard]] bool operator==(const SWAR& left, const SWAR& right);
    [[nodiscard]] bool operator!=(const SWAR& left, const SWAR& right);
}

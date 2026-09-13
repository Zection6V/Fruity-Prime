#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace NCSFCommon::NC
{
    class INFOEntry
    {
    public:
        virtual ~INFOEntry() = default;

        INFOEntry(const INFOEntry&) = delete;
        INFOEntry& operator=(const INFOEntry&) = delete;
        INFOEntry(INFOEntry&&) = delete;
        INFOEntry& operator=(INFOEntry&&) = delete;

        [[nodiscard]] const std::optional<std::u16string>& OriginalFilename() const noexcept;
        void OriginalFilename(std::optional<std::u16string> value);

        [[nodiscard]] const std::optional<std::u16string>& SDATNumber() const noexcept;
        void SDATNumber(std::optional<std::u16string> value);

        [[nodiscard]] virtual std::uint32_t Size() const = 0;

        virtual INFOEntry* Read(std::span<const std::uint8_t> span) = 0;
        virtual void Write(std::span<std::uint8_t> span) = 0;

        [[nodiscard]] std::u16string FullFilename(bool multipleSDATs) const;

    protected:
        INFOEntry() = default;
        explicit INFOEntry(const INFOEntry* other);

        [[nodiscard]] virtual std::u16string DebuggerDisplay() const;

    private:
        static void AppendString(std::u16string& destination, const std::optional<std::u16string>& value);

        std::optional<std::u16string> _originalFilename = std::u16string{};
        std::optional<std::u16string> _sdatNumber = std::u16string{};
    };
}

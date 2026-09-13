#pragma once

#include "INFOEntry.hpp"

#include <any>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace NCSFCommon::NC
{
    class SWAR;

    class INFOEntryWAVEARC : public INFOEntry
    {
    public:
        INFOEntryWAVEARC() noexcept = default;
        explicit INFOEntryWAVEARC(const INFOEntryWAVEARC* other);

        INFOEntryWAVEARC(const INFOEntryWAVEARC&) = delete;
        INFOEntryWAVEARC& operator=(const INFOEntryWAVEARC&) = delete;
        INFOEntryWAVEARC(INFOEntryWAVEARC&&) = delete;
        INFOEntryWAVEARC& operator=(INFOEntryWAVEARC&&) = delete;

        [[nodiscard]] std::uint32_t FileID() const noexcept;
        void FileID(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint8_t Flags() const noexcept;
        void Flags(std::uint8_t value) noexcept;

        [[nodiscard]] const std::shared_ptr<NCSFCommon::NC::SWAR>& SWAR() const noexcept;
        void SWAR(std::shared_ptr<NCSFCommon::NC::SWAR> value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept override;

        INFOEntryWAVEARC* Read(std::span<const std::uint8_t> span) override;
        void Write(std::span<std::uint8_t> span) override;

        [[nodiscard]] bool Equals(const INFOEntryWAVEARC* other) const noexcept;
        [[nodiscard]] bool Equals(const std::any& obj) const noexcept;
        [[nodiscard]] std::int32_t GetHashCode() const;

        [[nodiscard]] static bool EqualityOperator(
            const INFOEntryWAVEARC* left, const INFOEntryWAVEARC* right) noexcept;
        [[nodiscard]] static bool InequalityOperator(
            const INFOEntryWAVEARC* left, const INFOEntryWAVEARC* right) noexcept;

    protected:
        [[nodiscard]] std::u16string DebuggerDisplay() const override;

    private:
        std::uint32_t _fileID = 0;
        std::uint8_t _flags = 0;
        std::shared_ptr<NCSFCommon::NC::SWAR> _swar;
    };

    [[nodiscard]] bool operator==(
        const INFOEntryWAVEARC& left, const INFOEntryWAVEARC& right) noexcept;
    [[nodiscard]] bool operator!=(
        const INFOEntryWAVEARC& left, const INFOEntryWAVEARC& right) noexcept;
}

#pragma once

#include "INFOEntry.hpp"

#include <any>
#include <cstdint>
#include <span>
#include <string>

namespace NCSFCommon::NC
{
    class INFOEntryPLAYER : public INFOEntry
    {
    public:
        INFOEntryPLAYER() noexcept = default;
        explicit INFOEntryPLAYER(const INFOEntryPLAYER* other);

        INFOEntryPLAYER(const INFOEntryPLAYER&) = delete;
        INFOEntryPLAYER& operator=(const INFOEntryPLAYER&) = delete;
        INFOEntryPLAYER(INFOEntryPLAYER&&) = delete;
        INFOEntryPLAYER& operator=(INFOEntryPLAYER&&) = delete;

        [[nodiscard]] std::uint8_t MaxSequences() const noexcept;
        void MaxSequences(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Padding() const noexcept;
        void Padding(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t ChannelMask() const noexcept;
        void ChannelMask(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint32_t HeapSize() const noexcept;
        void HeapSize(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept override;

        INFOEntryPLAYER* Read(std::span<const std::uint8_t> span) override;
        void Write(std::span<std::uint8_t> span) override;

        [[nodiscard]] bool Equals(const INFOEntryPLAYER* other) const noexcept;
        [[nodiscard]] virtual bool Equals(const std::any& obj) const;
        [[nodiscard]] virtual std::int32_t GetHashCode() const;

        [[nodiscard]] static bool EqualityOperator(
            const INFOEntryPLAYER* left, const INFOEntryPLAYER* right) noexcept;
        [[nodiscard]] static bool InequalityOperator(
            const INFOEntryPLAYER* left, const INFOEntryPLAYER* right) noexcept;

    protected:
        [[nodiscard]] std::u16string DebuggerDisplay() const override;

    private:
        std::uint8_t _maxSequences = 0;
        std::uint8_t _padding = 0;
        std::uint16_t _channelMask = 0;
        std::uint32_t _heapSize = 0;
    };

    [[nodiscard]] bool operator==(const INFOEntryPLAYER& left, const INFOEntryPLAYER& right) noexcept;
    [[nodiscard]] bool operator!=(const INFOEntryPLAYER& left, const INFOEntryPLAYER& right) noexcept;
}

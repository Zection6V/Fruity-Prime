#pragma once

#include "INFOEntry.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace NCSFCommon::NC
{
    class SSEQ;

    [[nodiscard]] bool operator==(const SSEQ& left, const SSEQ& right);

    class INFOEntrySEQ : public INFOEntry
    {
    public:
        INFOEntrySEQ() noexcept = default;
        explicit INFOEntrySEQ(const INFOEntrySEQ* other);

        INFOEntrySEQ(const INFOEntrySEQ&) = delete;
        INFOEntrySEQ& operator=(const INFOEntrySEQ&) = delete;
        INFOEntrySEQ(INFOEntrySEQ&&) = delete;
        INFOEntrySEQ& operator=(INFOEntrySEQ&&) = delete;

        [[nodiscard]] std::uint32_t FileID() const noexcept;
        void FileID(std::uint32_t value) noexcept;

        [[nodiscard]] std::uint16_t Bank() const noexcept;
        void Bank(std::uint16_t value) noexcept;

        [[nodiscard]] std::uint8_t Volume() const noexcept;
        void Volume(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t ChannelPriority() const noexcept;
        void ChannelPriority(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t PlayerPriority() const noexcept;
        void PlayerPriority(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint8_t Player() const noexcept;
        void Player(std::uint8_t value) noexcept;

        [[nodiscard]] std::uint16_t Reserved() const noexcept;
        void Reserved(std::uint16_t value) noexcept;

        [[nodiscard]] const std::shared_ptr<NCSFCommon::NC::SSEQ>& SSEQ() const noexcept;
        void SSEQ(std::shared_ptr<NCSFCommon::NC::SSEQ> value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept override;

        INFOEntrySEQ* Read(std::span<const std::uint8_t> span) override;
        void Write(std::span<std::uint8_t> span) override;

        [[nodiscard]] bool FileEquals(const INFOEntrySEQ* other) const;

    protected:
        [[nodiscard]] std::u16string DebuggerDisplay() const override;

    private:
        std::uint32_t _fileID = 0;
        std::uint16_t _bank = 0;
        std::uint8_t _volume = 0;
        std::uint8_t _channelPriority = 0;
        std::uint8_t _playerPriority = 0;
        std::uint8_t _player = 0;
        std::uint16_t _reserved = 0;
        std::shared_ptr<NCSFCommon::NC::SSEQ> _sseq;
    };
}

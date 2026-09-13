#pragma once

#include "INFOEntry.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace NCSFCommon::NC
{
    class SBNK;

    [[nodiscard]] bool operator==(const SBNK& left, const SBNK& right);

    class INFOEntryBANK : public INFOEntry
    {
    public:
        INFOEntryBANK() noexcept = default;
        explicit INFOEntryBANK(const INFOEntryBANK* other);

        INFOEntryBANK(const INFOEntryBANK&) = delete;
        INFOEntryBANK& operator=(const INFOEntryBANK&) = delete;
        INFOEntryBANK(INFOEntryBANK&&) = delete;
        INFOEntryBANK& operator=(INFOEntryBANK&&) = delete;

        [[nodiscard]] std::uint32_t FileID() const noexcept;
        void FileID(std::uint32_t value) noexcept;

        [[nodiscard]] std::span<const std::uint16_t> WaveArchives() const noexcept;

        [[nodiscard]] const std::shared_ptr<NCSFCommon::NC::SBNK>& SBNK() const noexcept;
        void SBNK(std::shared_ptr<NCSFCommon::NC::SBNK> value) noexcept;

        [[nodiscard]] std::uint32_t Size() const noexcept override;

        INFOEntryBANK* Read(std::span<const std::uint8_t> span) override;
        void Write(std::span<std::uint8_t> span) override;

        void ReplaceWaveArchive(int i, std::uint16_t newWaveArchive);

        [[nodiscard]] bool FileEquals(const INFOEntryBANK* other) const;

    protected:
        [[nodiscard]] std::u16string DebuggerDisplay() const override;

    private:
        std::uint32_t _fileID = 0;
        std::array<std::uint16_t, 4> _waveArchives{};
        std::shared_ptr<NCSFCommon::NC::SBNK> _sbnk;
    };
}

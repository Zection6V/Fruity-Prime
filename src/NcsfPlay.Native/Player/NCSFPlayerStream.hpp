#pragma once

#include "NCSFFile.hpp"
#include "Player.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace NCSFCommon
{
    class TagList;

    namespace NC
    {
        class SDAT;
    }
}

namespace NCSF123
{
    enum class SeekOrigin : std::int32_t
    {
        Begin = 0,
        Current = 1,
        End = 2
    };

    class NCSFPlayerStream final
    {
    private:
        static constexpr float CheckSilenceBias = 4096.0F;
        static constexpr float CheckSilenceLevel = 0.000213623046875F;

        std::shared_ptr<NCSFFile> ncsf;
        std::uint32_t sampleRate = 0;
        NCSFPlayer::Interpolation interpolation = NCSFPlayer::Interpolation::None;
        std::uint32_t detectedSilenceSample = 0;
        std::uint32_t detectedSilenceSec = 0;
        std::uint32_t skipSilenceOnStartSec = 0;
        std::uint32_t initialSkipSilenceOnStartSec = 0;
        std::int32_t lengthSample = 0;
        std::int32_t fadeSample = 0;
        float prevSampleL = NCSFPlayerStream::CheckSilenceBias;
        float prevSampleR = NCSFPlayerStream::CheckSilenceBias;
        std::int32_t defaultLengthInMS = 0;
        std::int32_t defaultFadeInMS = 0;
        std::int32_t lengthInMS = 0;
        std::int32_t fadeInMS = 0;
        VolumeType volumeType = VolumeType::None;
        PeakType peakType = PeakType::None;
        bool playForever = false;
        float volumeMultiplier = 0.0F;
        std::uint16_t channelMutes = 0;
        std::uint16_t trackMutes = 0;
        bool ignoreVolume = false;
        std::shared_ptr<NCSFCommon::NC::SDAT> sdat;
        std::vector<std::uint8_t> sdatData;
        std::uint32_t sseq = 0;
        std::shared_ptr<NCSFPlayer::Player> player = std::make_shared<NCSFPlayer::Player>();

    public:
        [[nodiscard]] const std::shared_ptr<NCSFPlayer::Player>& Player() const noexcept;

    private:
        float secondsPerSample = 0.0F;
        std::int32_t samplesIntoPlayback = 0;

    public:
        [[nodiscard]] NCSFCommon::TagList& Tags();

    private:
        float volumeModification = 0.0F;

    public:
        [[nodiscard]] float VolumeModification() const noexcept;
        void VolumeModification(float value) noexcept;

        NCSFPlayerStream(
            std::u16string path,
            std::uint32_t sampleRate,
            NCSFPlayer::Interpolation interpolation,
            std::uint32_t skipSilenceOnStartSec,
            std::int32_t defaultLengthInMS,
            std::int32_t defaultFadeInMS,
            VolumeType volumeType,
            PeakType peakType,
            bool playForever,
            float volumeMultiplier,
            std::uint16_t channelMutes,
            std::uint16_t trackMutes,
            bool ignoreVolume);

        NCSFPlayerStream(const NCSFPlayerStream&) = delete;
        NCSFPlayerStream(NCSFPlayerStream&&) = delete;
        NCSFPlayerStream& operator=(const NCSFPlayerStream&) = delete;
        NCSFPlayerStream& operator=(NCSFPlayerStream&&) = delete;

        [[nodiscard]] bool CanRead() const noexcept;
        [[nodiscard]] bool CanSeek() const noexcept;
        [[nodiscard]] bool CanWrite() const noexcept;
        [[nodiscard]] std::int64_t Length() const;

    private:
        std::int64_t position = 0;

    public:
        [[nodiscard]] std::int64_t Position() const noexcept;
        void Position(std::int64_t value) noexcept;
        void Flush();

    private:
        void GenerateSamples(std::span<float> buf);
        void Load();
        void LoadNCSF();
        void MapNCSF(const NCSFFile& ncsfToLoad);
        void MapNCSFSection(std::span<const std::uint8_t> section);

    public:
        std::int32_t Read(std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count);

    private:
        void RecursiveLoadNCSF(const NCSFFile& ncsfToLoad, std::int32_t level);

    public:
        std::int64_t Seek(std::int64_t offset, SeekOrigin origin);
        void SetLength(std::int64_t value);

    private:
        void Terminate();

    public:
        void Write(std::span<std::uint8_t> buffer, std::int32_t offset, std::int32_t count);
    };
}

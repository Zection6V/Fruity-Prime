#include "NCSFPlayerStream.hpp"

#include "../Channel.hpp"
#include "../Common.hpp"
#include "../NCSF.hpp"
#include "../NC/INFOEntryBANK.hpp"
#include "../NC/INFOEntryPLAYER.hpp"
#include "../NC/INFOEntrySEQ.hpp"
#include "../NC/SBNK.hpp"
#include "../NC/SDAT.hpp"
#include "../NC/SSEQ.hpp"
#include "../TagList.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <ios>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    [[nodiscard]] constexpr std::int32_t WrapAdd32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t WrapAdd64(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left) + static_cast<std::uint64_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t WrapSubtract64(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left) - static_cast<std::uint64_t>(right));
    }

    [[nodiscard]] constexpr std::int64_t WrapMultiply64(std::int64_t left, std::int64_t right) noexcept
    {
        return std::bit_cast<std::int64_t>(
            static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapShiftLeft32(std::int32_t value, unsigned count) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value) << (count & 31U));
    }

    [[nodiscard]] constexpr std::int32_t ToInt32Unchecked(std::int64_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] constexpr std::int64_t ShiftRightThree(std::int64_t value) noexcept
    {
        std::int64_t quotient = value / 8;
        if (value < 0 && value % 8 != 0)
            --quotient;
        return quotient;
    }

    [[nodiscard]] constexpr std::int64_t AlignEight(std::int64_t value) noexcept
    {
        return ShiftRightThree(value) * 8;
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(std::span<const std::uint8_t> source)
    {
        if (source.size() < 4U)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        return static_cast<std::uint32_t>(source[0]) |
            (static_cast<std::uint32_t>(source[1]) << 8U) |
            (static_cast<std::uint32_t>(source[2]) << 16U) |
            (static_cast<std::uint32_t>(source[3]) << 24U);
    }

    [[nodiscard]] std::int32_t ReadInt32LittleEndian(std::span<const std::uint8_t> source)
    {
        return std::bit_cast<std::int32_t>(ReadUInt32LittleEndian(source));
    }

    [[nodiscard]] std::u16string LibTag(std::int32_t value)
    {
        std::array<char, 16> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (result.ec != std::errc{})
            throw std::runtime_error("FormatException");

        std::u16string tag = u"_lib";
        for (const char* current = buffer.data(); current != result.ptr; ++current)
            tag.push_back(static_cast<char16_t>(*current));
        return tag;
    }

    [[nodiscard]] std::u16string CombineLibraryPath(
        const std::u16string& filePath,
        const NCSFCommon::TagList::String& library)
    {
        const std::filesystem::path path(filePath);
        if (!path.empty() && path.has_root_path() && path == path.root_path())
            throw NCSFCommon::ArgumentNullException("path1");
        if (library.IsNull())
            throw NCSFCommon::ArgumentNullException("path2");

        const std::filesystem::path directory = path.parent_path();
        const std::u16string libraryValue = library;
        if (libraryValue.empty())
            return directory.u16string();
        if (directory.empty())
            return libraryValue;
        return (directory / std::filesystem::path(libraryValue)).u16string();
    }

    [[noreturn]] void ThrowNullReference()
    {
        throw NCSFCommon::NullReferenceException();
    }

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    [[noreturn]] void ThrowNotSupported()
    {
        throw std::logic_error("NotSupportedException");
    }

    [[noreturn]] void ThrowNotImplemented()
    {
        throw std::logic_error("NotImplementedException");
    }

    [[noreturn]] void ThrowStreamTooLong()
    {
        throw std::ios_base::failure("Stream was too long.");
    }
}

namespace NCSF123
{
    const std::shared_ptr<NCSFPlayer::Player>& NCSFPlayerStream::Player() const noexcept
    {
        return player;
    }

    NCSFCommon::TagList& NCSFPlayerStream::Tags()
    {
        if (!ncsf)
            ThrowNullReference();
        return ncsf->Tags();
    }

    float NCSFPlayerStream::VolumeModification() const noexcept
    {
        return volumeModification;
    }

    void NCSFPlayerStream::VolumeModification(float value) noexcept
    {
        volumeModification = value;
    }

    NCSFPlayerStream::NCSFPlayerStream(
        std::u16string path,
        std::uint32_t sampleRateValue,
        NCSFPlayer::Interpolation interpolationValue,
        std::uint32_t skipSilenceOnStartSecValue,
        std::int32_t defaultLengthInMSValue,
        std::int32_t defaultFadeInMSValue,
        VolumeType volumeTypeValue,
        PeakType peakTypeValue,
        bool playForeverValue,
        float volumeMultiplierValue,
        std::uint16_t channelMutesValue,
        std::uint16_t trackMutesValue,
        bool ignoreVolumeValue)
    {
        ncsf = std::make_shared<NCSFFile>(std::move(path));
        sampleRate = sampleRateValue;
        interpolation = interpolationValue;
        initialSkipSilenceOnStartSec = skipSilenceOnStartSecValue;
        defaultLengthInMS = defaultLengthInMSValue;
        defaultFadeInMS = defaultFadeInMSValue;
        volumeType = volumeTypeValue;
        peakType = peakTypeValue;
        playForever = playForeverValue;
        volumeMultiplier = volumeMultiplierValue;
        channelMutes = channelMutesValue;
        trackMutes = trackMutesValue;
        ignoreVolume = ignoreVolumeValue;
        Load();
    }

    bool NCSFPlayerStream::CanRead() const
    {
        return true;
    }

    bool NCSFPlayerStream::CanSeek() const
    {
        return !playForever;
    }

    bool NCSFPlayerStream::CanWrite() const
    {
        return false;
    }

    std::int64_t NCSFPlayerStream::Length() const
    {
        if (playForever)
            ThrowNotSupported();
        const std::int32_t total = WrapAdd32(lengthSample, fadeSample);
        const std::int32_t shifted = WrapShiftLeft32(total, 3U);
        return shifted;
    }

    std::int64_t NCSFPlayerStream::Position() const
    {
        return position;
    }

    void NCSFPlayerStream::Position(std::int64_t value)
    {
        position = value;
    }

    void NCSFPlayerStream::Flush()
    {
        ThrowNotImplemented();
    }

    void NCSFPlayerStream::GenerateSamples(std::span<float> buf)
    {
        std::int32_t offset = 0;
        const std::int32_t samples = static_cast<std::int32_t>(buf.size()) >> 1;
        for (std::int32_t smpl = 0; smpl < samples; ++smpl)
        {
            samplesIntoPlayback = WrapAdd32(samplesIntoPlayback, 1);

            float leftChannel = 0.0F;
            float rightChannel = 0.0F;

            for (std::int32_t i = 0; i < NCSFPlayer::Player::ChannelCount; ++i)
            {
                const bool muted = (channelMutes & (1U << i)) != 0U;
                const auto channels = player->Channels();
                const auto& chn = channels[static_cast<std::size_t>(i)];
                if (!chn)
                    ThrowNullReference();

                if (chn->IsActive() && chn->Register().Enable())
                {
                    float sample = muted ? 0.0F : chn->GenerateSample();
                    chn->IncrementSample();

                    if (!muted)
                    {
                        sample = NCSFPlayer::Player::MulDiv7(sample, chn->Register().VolumeMultiplier());

                        float divisor = 1.0F;
                        switch (chn->Register().VolumeDivisor())
                        {
                        case 1U:
                            divisor = 0.5F;
                            break;
                        case 2U:
                            divisor = 0.25F;
                            break;
                        case 3U:
                            divisor = 0.0625F;
                            break;
                        default:
                            break;
                        }
                        sample *= divisor;

                        const std::uint8_t leftPan = static_cast<std::uint8_t>(
                            127 - static_cast<std::int32_t>(chn->Register().Panning()));
                        leftChannel += NCSFPlayer::Player::MulDiv7(sample, leftPan);
                        rightChannel += NCSFPlayer::Player::MulDiv7(sample, chn->Register().Panning());
                    }
                }
            }

            buf[static_cast<std::size_t>(offset)] = leftChannel;
            buf[static_cast<std::size_t>(offset + 1)] = rightChannel;
            offset += 2;

            if (static_cast<float>(samplesIntoPlayback) * secondsPerSample >= NCSFPlayer::Player::SecondsPerClockCycle)
            {
                player->SequenceMain();
                samplesIntoPlayback = 0;
            }
        }
    }

    void NCSFPlayerStream::Load()
    {
        LoadNCSF();

        sdat = std::make_shared<NCSFCommon::NC::SDAT>();
        sdat->Read(ncsf->FilePath(), std::span<const std::uint8_t>(sdatData.data(), sdatData.size()), sseq);

        const auto& sdatPlayer = sdat->Player();
        player->ChannelMask(sdatPlayer ? sdatPlayer->ChannelMask() : 0xFFFFU);
        player->SampleRate(sampleRate);
        player->Interpolation(interpolation);
        player->TrackMutes(trackMutes);

        const auto sseqs = sdat->SSEQs();
        if (sseqs.empty())
            ThrowIndexOutOfRange();
        const auto sseqToPlay = sseqs[0];
        if (!sseqToPlay)
            ThrowNullReference();

        const auto firstSseqInfo = sseqToPlay->Info();
        if (!firstSseqInfo)
            ThrowNullReference();
        const std::uint8_t firstSequenceVolume = firstSseqInfo->Volume();

        std::uint8_t sequenceVolume;
        if (firstSequenceVolume == 0U)
        {
            sequenceVolume = 0x7FU;
        }
        else
        {
            const auto secondSseqInfo = sseqToPlay->Info();
            if (!secondSseqInfo)
                ThrowNullReference();
            sequenceVolume = secondSseqInfo->Volume();
        }

        player->PrepareSequence(
            sseqToPlay.get(),
            0,
            NCSFCommon::NCSF::ConvertScale(sequenceVolume));

        const auto sbnks = sdat->SBNKs();
        if (sbnks.empty())
            ThrowIndexOutOfRange();
        player->SBNK(sbnks[0]);

        for (std::int32_t i = 0, j = 0; i < 4; ++i)
        {
            const auto playerSBNK = player->SBNK();
            if (!playerSBNK)
                ThrowNullReference();
            const auto bankInfo = playerSBNK->Info();
            if (!bankInfo)
                ThrowNullReference();
            const auto waveArchives = bankInfo->WaveArchives();

            if (waveArchives[static_cast<std::size_t>(i)] != 0xFFFFU)
            {
                const auto swars = sdat->SWARs();
                if (static_cast<std::size_t>(j) >= swars.size())
                    ThrowIndexOutOfRange();
                player->SetSWAR(i, swars[static_cast<std::size_t>(j++)]);
            }
        }
        player->SequenceMain();
        secondsPerSample = 1.0F / static_cast<float>(sampleRate);

        lengthInMS = ncsf->GetLengthMS(defaultLengthInMS);
        fadeInMS = ncsf->GetFadeMS(defaultFadeInMS);
        lengthSample = ToInt32Unchecked(
            static_cast<std::int64_t>(lengthInMS) * static_cast<std::int64_t>(sampleRate) / 1000);
        fadeSample = ToInt32Unchecked(
            static_cast<std::int64_t>(fadeInMS) * static_cast<std::int64_t>(sampleRate) / 1000);

        VolumeModification(ignoreVolume ? 1.0F : ncsf->GetVolume(volumeType, peakType) * volumeMultiplier);

        skipSilenceOnStartSec = initialSkipSilenceOnStartSec;
        detectedSilenceSample = detectedSilenceSec = 0;
        Position(0);
        prevSampleL = prevSampleR = NCSFPlayerStream::CheckSilenceBias;
    }

    void NCSFPlayerStream::LoadNCSF()
    {
        RecursiveLoadNCSF(*ncsf, 1);
    }

    void NCSFPlayerStream::MapNCSF(const NCSFFile& ncsfToLoad)
    {
        const auto reservedSection = ncsfToLoad.ReservedSection();
        const auto programSection = ncsfToLoad.ProgramSection();

        if (!reservedSection.empty())
            sseq = ReadUInt32LittleEndian(reservedSection);

        if (!programSection.empty())
            MapNCSFSection(programSection);
    }

    void NCSFPlayerStream::MapNCSFSection(std::span<const std::uint8_t> section)
    {
        if (section.size() < 8U)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        const std::int32_t size = ReadInt32LittleEndian(section.subspan(0x08U));
        const std::int32_t currentCount = static_cast<std::int32_t>(sdatData.size());
        if (currentCount < size)
        {
            const bool empty = sdatData.empty();
            sdatData.resize(static_cast<std::size_t>(size));
            if (empty)
                std::fill(sdatData.begin(), sdatData.end(), static_cast<std::uint8_t>(0));
        }
        if (section.size() > sdatData.size())
            throw std::invalid_argument("Destination is too short.");
        std::copy(section.begin(), section.end(), sdatData.begin());
    }

    std::int32_t NCSFPlayerStream::Read(std::span<std::uint8_t> buffer)
    {
        if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::length_error("Span length exceeds Int32.MaxValue.");

        std::vector<std::uint8_t> sharedBuffer(buffer.size());
        const std::int32_t numRead = Read(
            std::span<std::uint8_t>(sharedBuffer.data(), sharedBuffer.size()),
            0,
            static_cast<std::int32_t>(buffer.size()));

        if (static_cast<std::uint32_t>(numRead) > static_cast<std::uint32_t>(buffer.size()))
            ThrowStreamTooLong();

        if (numRead != 0)
        {
            std::copy_n(
                sharedBuffer.begin(),
                static_cast<std::size_t>(numRead),
                buffer.begin());
        }
        return numRead;
    }

    std::int32_t NCSFPlayerStream::Read(
        std::span<std::uint8_t> buffer,
        std::int32_t offset,
        std::int32_t count)
    {
        if (offset < 0 || count < 0)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        const std::size_t byteOffset = static_cast<std::size_t>(offset);
        const std::size_t byteCount = static_cast<std::size_t>(count);
        if (byteOffset > buffer.size() || byteCount > buffer.size() - byteOffset)
            throw std::out_of_range("Specified argument was out of the range of valid values.");

        const std::size_t floatCount = byteCount / sizeof(float);
        std::vector<float> bufFloat(floatCount);
        const std::size_t floatBytes = floatCount * sizeof(float);
        if (floatBytes != 0U)
            std::memcpy(bufFloat.data(), buffer.data() + byteOffset, floatBytes);
        const auto copyBack = [&]() noexcept
        {
            if (floatBytes != 0U)
                std::memcpy(buffer.data() + byteOffset, bufFloat.data(), floatBytes);
        };

        try
        {
            std::int32_t pos = offset;
            std::int32_t bufSize = static_cast<std::int32_t>(bufFloat.size()) >> 1;
            while (pos < bufSize)
            {
                const std::int32_t remain = bufSize - pos;
                GenerateSamples(std::span<float>(bufFloat).subspan(static_cast<std::size_t>(pos << 1)));
                if (skipSilenceOnStartSec != 0U)
                {
                    std::int32_t skipOffset = 0;
                    for (std::int32_t ofs = 0; ofs < remain; ++ofs)
                    {
                        const float sampleL = bufFloat[static_cast<std::size_t>(2 * (pos + ofs))];
                        const float sampleR = bufFloat[static_cast<std::size_t>(2 * (pos + ofs) + 1)];
                        const bool silence =
                            sampleL + NCSFPlayerStream::CheckSilenceBias + NCSFPlayerStream::CheckSilenceLevel - prevSampleL <=
                                NCSFPlayerStream::CheckSilenceLevel * 2.0F &&
                            sampleR + NCSFPlayerStream::CheckSilenceBias + NCSFPlayerStream::CheckSilenceLevel - prevSampleR <=
                                NCSFPlayerStream::CheckSilenceLevel * 2.0F;

                        if (silence)
                        {
                            ++detectedSilenceSample;
                            if (detectedSilenceSample >= sampleRate)
                            {
                                detectedSilenceSample -= sampleRate;
                                ++detectedSilenceSec;
                                if (skipSilenceOnStartSec != 0U && detectedSilenceSec >= skipSilenceOnStartSec)
                                {
                                    skipSilenceOnStartSec = detectedSilenceSec = 0;
                                    skipOffset = ofs;
                                }
                            }
                        }
                        else
                        {
                            detectedSilenceSample = detectedSilenceSec = 0;
                            if (skipSilenceOnStartSec != 0U)
                            {
                                skipSilenceOnStartSec = 0;
                                skipOffset = ofs;
                            }
                        }

                        prevSampleL = sampleL + NCSFPlayerStream::CheckSilenceBias;
                        prevSampleR = sampleR + NCSFPlayerStream::CheckSilenceBias;
                    }

                    if (skipSilenceOnStartSec == 0U)
                    {
                        if (skipOffset != 0)
                        {
                            const std::size_t source = static_cast<std::size_t>((pos + skipOffset) << 1);
                            const std::size_t destination = static_cast<std::size_t>(pos);
                            const std::size_t copyCount = bufFloat.size() - source;
                            std::memmove(
                                bufFloat.data() + destination,
                                bufFloat.data() + source,
                                copyCount * sizeof(float));
                            pos += remain - skipOffset;
                        }
                        else
                            pos += remain;
                    }
                }
                else
                    pos += remain;
            }

            const std::int64_t currentSample = ShiftRightThree(Position());
            if (!playForever)
            {
                const std::int32_t totalSamples = WrapAdd32(lengthSample, fadeSample);
                if (currentSample >= totalSamples)
                {
                    copyBack();
                    return 0;
                }
                if (WrapAdd64(currentSample, static_cast<std::int64_t>(bufSize)) >= totalSamples)
                {
                    bufSize = ToInt32Unchecked(
                        WrapSubtract64(static_cast<std::int64_t>(totalSamples), currentSample));
                }
            }

            for (std::int32_t ofs = 0; ofs < bufSize; ++ofs)
            {
                float& left = bufFloat[static_cast<std::size_t>(2 * ofs)];
                float& right = bufFloat[static_cast<std::size_t>(2 * ofs + 1)];
                left = std::clamp(left * VolumeModification(), -1.0F, 1.0F);
                right = std::clamp(right * VolumeModification(), -1.0F, 1.0F);
            }

            if (!playForever && fadeSample != 0
                && WrapAdd64(currentSample, static_cast<std::int64_t>(bufSize)) >= lengthSample)
            {
                for (std::int32_t ofs = 0; ofs < bufSize; ++ofs)
                {
                    const std::int64_t samplePosition = WrapAdd64(currentSample, static_cast<std::int64_t>(ofs));
                    const std::int32_t totalSamples = WrapAdd32(lengthSample, fadeSample);
                    if (samplePosition >= lengthSample && samplePosition < totalSamples)
                    {
                        const std::int64_t difference = WrapSubtract64(
                            static_cast<std::int64_t>(totalSamples), samplePosition);
                        const std::int64_t numerator = WrapMultiply64(difference, 0x10000);
                        if (numerator == std::numeric_limits<std::int64_t>::min() && fadeSample == -1)
                            throw std::overflow_error("Arithmetic operation resulted in an overflow.");
                        const std::int32_t scale = ToInt32Unchecked(numerator / fadeSample);
                        float& left = bufFloat[static_cast<std::size_t>(2 * ofs)];
                        float& right = bufFloat[static_cast<std::size_t>(2 * ofs + 1)];
                        left = std::scalbn(left * static_cast<float>(scale), -16);
                        right = std::scalbn(right * static_cast<float>(scale), -16);
                    }
                    else if (samplePosition >= totalSamples)
                    {
                        bufFloat[static_cast<std::size_t>(2 * ofs)] = 0.0F;
                        bufFloat[static_cast<std::size_t>(2 * ofs + 1)] = 0.0F;
                    }
                }
            }

            const std::int32_t bytesRead = WrapShiftLeft32(bufSize, 3U);
            const std::int64_t currentPosition = Position();
            Position(WrapAdd64(currentPosition, static_cast<std::int64_t>(bytesRead)));
            copyBack();
            return bytesRead;
        }
        catch (...)
        {
            copyBack();
            throw;
        }
    }

    void NCSFPlayerStream::RecursiveLoadNCSF(const NCSFFile& ncsfToLoad, std::int32_t level)
    {
        if (level <= 10 && ncsfToLoad.Tags().Contains(u"_lib"))
        {
            const auto item = ncsfToLoad.Tags()[u"_lib"];
            RecursiveLoadNCSF(NCSFFile(CombineLibraryPath(ncsfToLoad.FilePath(), item.Value)), WrapAdd32(level, 1));
        }
        MapNCSF(ncsfToLoad);

        std::int32_t n = 2;
        bool found;
        do
        {
            found = false;
            const std::u16string libTag = LibTag(n);
            n = WrapAdd32(n, 1);
            if (ncsfToLoad.Tags().Contains(libTag))
            {
                found = true;
                const auto item = ncsfToLoad.Tags()[libTag];
                RecursiveLoadNCSF(NCSFFile(CombineLibraryPath(ncsfToLoad.FilePath(), item.Value)), WrapAdd32(level, 1));
            }
        } while (found);
    }

    std::int64_t NCSFPlayerStream::Seek(std::int64_t offset, SeekOrigin origin)
    {
        if (playForever)
            ThrowNotImplemented();
        else
        {
            offset = AlignEight(offset);
            if (origin == SeekOrigin::Current)
                offset = WrapAdd64(offset, Position());
            else if (origin == SeekOrigin::End)
                offset = WrapAdd64(offset, Length());
            if (offset < Position())
            {
                Terminate();
                Load();
            }

            std::array<std::uint8_t, 0x1000> dummyBuffer{};
            while (WrapSubtract64(offset, Position()) > 0x1000)
                (void)Read(std::span<std::uint8_t>(dummyBuffer));

            if (WrapSubtract64(offset, Position()) > 0)
            {
                const std::int32_t remaining = ToInt32Unchecked(WrapSubtract64(offset, Position()));
                if (remaining < 0 || static_cast<std::size_t>(remaining) > dummyBuffer.size())
                    throw std::out_of_range("Specified argument was out of the range of valid values.");
                (void)Read(std::span<std::uint8_t>(dummyBuffer).first(static_cast<std::size_t>(remaining)));
            }
            return offset;
        }
    }

    void NCSFPlayerStream::SetLength(std::int64_t)
    {
        ThrowNotImplemented();
    }

    void NCSFPlayerStream::Terminate()
    {
        player->Stop();
    }

    void NCSFPlayerStream::Write(std::span<std::uint8_t>, std::int32_t, std::int32_t)
    {
        ThrowNotImplemented();
    }
}

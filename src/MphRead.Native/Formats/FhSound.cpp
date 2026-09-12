#include "FhSound.hpp"

#include "../Read.hpp"

#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Formats::Sound
{
    namespace FhSoundDependency
    {
        [[nodiscard]] std::shared_ptr<SoundSample> CreateNullSoundSample(std::uint32_t id);
        [[nodiscard]] std::shared_ptr<SoundSample> CreateFhSoundSample(
            std::uint32_t id,
            std::uint32_t offset,
            FhSoundSampleHeader header,
            std::span<const std::uint8_t> data);
        void ExportSamples(
            const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>& samples,
            bool adpcmRoundingError,
            const std::string& prefix);
    }

    namespace
    {
        [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
        {
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream)
            {
                throw std::ios_base::failure("Could not open file: " + path);
            }
            const std::streampos end = stream.tellg();
            if (end < 0)
            {
                throw std::ios_base::failure("Could not determine file length: " + path);
            }
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
            stream.seekg(0, std::ios::beg);
            if (!bytes.empty())
            {
                stream.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!stream)
                {
                    throw std::ios_base::failure("Could not read file: " + path);
                }
            }
            return bytes;
        }
    }

    void SoundRead::ExportAllFh(bool adpcmRoundingError)
    {
        ExportFhBgm(adpcmRoundingError);
        ExportFhGlobalSfx(adpcmRoundingError);
        ExportFhMenuSfx(adpcmRoundingError);
        ExportFhSfx(adpcmRoundingError);
    }

    void SoundRead::ExportFhSfx(bool adpcmRoundingError)
    {
        FhSoundDependency::ExportSamples(
            ReadFhSfx(), adpcmRoundingError, "fh_");
    }

    void SoundRead::ExportFhBgm(bool adpcmRoundingError)
    {
        FhSoundDependency::ExportSamples(
            ReadFhBgm(), adpcmRoundingError, "fh_bgm_");
    }

    void SoundRead::ExportFhMenuSfx(bool adpcmRoundingError)
    {
        FhSoundDependency::ExportSamples(
            ReadFhMenuSfx(), adpcmRoundingError, "fh_menu_");
    }

    void SoundRead::ExportFhGlobalSfx(bool adpcmRoundingError)
    {
        FhSoundDependency::ExportSamples(
            ReadFhGlobalSfx(), adpcmRoundingError, "fh_lid_");
    }

    std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadFhSfx()
    {
        return ReadFhSoundFile("SFXDATA.BIN");
    }

    std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadFhBgm()
    {
        return ReadFhSoundFile("BGMDATA.BIN");
    }

    std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadFhMenuSfx()
    {
        return ReadFhSoundFile("MENUSFXDATA.BIN");
    }

    std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadFhGlobalSfx()
    {
        return ReadFhSoundFile("GLOBALSFXDATA.BIN");
    }

    std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadFhSoundFile(
        const std::string& filename)
    {
        const std::string path = MphRead::Paths::Combine(
            MphRead::Paths::FhFileSystem(), "sound", filename);
        const std::vector<std::uint8_t> storage = FileReadAllBytes(path);
        const std::span<const std::uint8_t> bytes(storage);
        const std::uint32_t count = MphRead::Read::SpanReadUint(bytes, 0);
        auto samples = std::make_shared<std::vector<std::shared_ptr<SoundSample>>>();
        std::uint32_t id = 0;
        const std::shared_ptr<const std::vector<std::uint32_t>> offsets
            = MphRead::Read::DoOffsets<std::uint32_t>(bytes, 4, count);
        for (const std::uint32_t offset : *offsets)
        {
            const FhSoundSampleHeader header
                = MphRead::Read::DoOffset<FhSoundSampleHeader>(bytes, offset);
            if (header.DataSize <= 4)
            {
                samples->push_back(FhSoundDependency::CreateNullSoundSample(id));
            }
            else
            {
                const std::int64_t start
                    = static_cast<std::int64_t>(offset)
                    + static_cast<std::int32_t>(sizeof(FhSoundSampleHeader));
                const std::uint32_t size = header.DataSize;
                samples->push_back(FhSoundDependency::CreateFhSoundSample(
                    id,
                    offset,
                    header,
                    MphRead::CollectionExtensions::Slice(bytes, start, size)));
            }
            ++id;
        }
        return samples;
    }
}

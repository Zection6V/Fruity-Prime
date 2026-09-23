#pragma once

// System.IO.Compression.ZipArchive, opened for reading: the entry list and one
// entry's bytes. Only the stored and deflated methods exist in a .zip this
// program reads.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime
{
    class ZipArchive final
    {
    public:
        struct Entry final
        {
            // ZipArchiveEntry.FullName.
            std::string FullName;
            std::uint16_t Method = 0;
            std::uint32_t CompressedSize = 0;
            std::uint32_t UncompressedSize = 0;
            std::uint32_t LocalHeaderOffset = 0;
        };

        // ZipFile.OpenRead(path); null when the file is not a zip.
        [[nodiscard]] static std::shared_ptr<ZipArchive> OpenRead(const std::string& path);

        [[nodiscard]] std::size_t Count() const noexcept { return _entries.size(); }
        [[nodiscard]] const std::string& FullName(std::size_t index) const;
        // entry.Open() read to the end.
        [[nodiscard]] std::vector<std::uint8_t> Read(std::size_t index) const;

    private:
        std::vector<Entry> _entries;
        std::vector<std::uint8_t> _bytes;
    };
}

#pragma once

// ZipFile.ExtractToDirectory and TarFile.ExtractToDirectory over libarchive,
// refusing every entry .NET refuses: a path or a link leaving the destination.

#include <string>

namespace MphRead::NativeRuntime
{
    // ZipFile.ExtractToDirectory(archive, destination, overwriteFiles: true)
    // when `zip`, and otherwise the .tar.gz form below.
    void ArchiveExtractToDirectory(const std::string& archivePath,
        const std::string& destination, bool zip);

    // TarFile.ExtractToDirectory(new GZipStream(File.OpenRead(archivePath),
    // CompressionMode.Decompress), destination, overwriteFiles: true).
    void TarFileExtractGZipToDirectory(const std::string& archivePath, const std::string& destination);
}

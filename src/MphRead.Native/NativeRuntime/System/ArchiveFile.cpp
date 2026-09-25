#include "ArchiveFile.hpp"

#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "IO.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::DirectoryCreateDirectory;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::Utf8ToWide;

namespace MphRead::NativeRuntime
{
    namespace
    {
        namespace fs = std::filesystem;

        using IOException = ::System::IO::IOException;

        using UnauthorizedAccessException = ::System::UnauthorizedAccessException;

        class ArchiveException final : public std::runtime_error
        {
        public:
            explicit ArchiveException(std::string message)
                : std::runtime_error(std::move(message))
            {
            }
        };

        [[noreturn]] void ThrowFileError(
            const std::string& path, const std::error_code& error)
        {
            if (error == std::errc::permission_denied)
            {
                throw UnauthorizedAccessException(
                    "Access to the path '" + path + "' is denied.");
            }
            throw IOException(error.message());
        }

        [[noreturn]] void ThrowArchive(struct archive* reader)
        {
            const char* message = archive_error_string(reader);
            throw ArchiveException(message == nullptr ? "archive extraction failed" : message);
        }

        [[nodiscard]] bool PathComponentEquals(
            const fs::path& left, const fs::path& right)
        {
#if defined(_WIN32)
            const std::wstring a = left.native();
            const std::wstring b = right.native();
            if (a.size() != b.size())
            {
                return false;
            }
            if (a.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return false;
            }
            return ::CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
                b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
#else
            return left == right;
#endif
        }

        [[nodiscard]] bool IsWithinDirectory(
            const fs::path& directory, const fs::path& candidate)
        {
            const fs::path root = fs::absolute(directory).lexically_normal();
            const fs::path value = fs::absolute(candidate).lexically_normal();
            auto rootIt = root.begin();
            auto valueIt = value.begin();
            for (; rootIt != root.end(); ++rootIt, ++valueIt)
            {
                if (valueIt == value.end() || !PathComponentEquals(*rootIt, *valueIt))
                {
                    return false;
                }
            }
            return valueIt != value.end();
        }

        [[nodiscard]] bool IsDotNetDirectorySeparator(char ch) noexcept
        {
#if defined(_WIN32)
            return ch == '/' || ch == '\\';
#else
            return ch == '/';
#endif
        }

        [[nodiscard]] bool IsDotNetDriveChar(char ch) noexcept
        {
            return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        }

        [[nodiscard]] bool IsDotNetPathRooted(std::string_view path) noexcept
        {
#if defined(_WIN32)
            return (!path.empty() && IsDotNetDirectorySeparator(path[0]))
                || (path.size() >= 2U && IsDotNetDriveChar(path[0]) && path[1] == ':');
#else
            return !path.empty() && path[0] == '/';
#endif
        }

        [[nodiscard]] bool IsDotNetPathFullyQualified(std::string_view path) noexcept
        {
#if defined(_WIN32)
            if (path.size() < 2U)
            {
                return false;
            }
            if (IsDotNetDirectorySeparator(path[0]))
            {
                return path[1] == '?' || IsDotNetDirectorySeparator(path[1]);
            }
            return path.size() >= 3U
                && IsDotNetDriveChar(path[0])
                && path[1] == ':'
                && IsDotNetDirectorySeparator(path[2]);
#else
            return !path.empty() && path[0] == '/';
#endif
        }

        [[nodiscard]] std::string DotNetJoin(
            const std::string& left, const std::string& right)
        {
            if (left.empty())
            {
                return right;
            }
            if (right.empty())
            {
                return left;
            }
            std::string result = left;
            if (!IsDotNetDirectorySeparator(result.back())
                && !IsDotNetDirectorySeparator(right.front()))
            {
                result.push_back(static_cast<char>(fs::path::preferred_separator));
            }
            result += right;
            return result;
        }

        [[nodiscard]] fs::path FullNormalizedPath(const std::string& value)
        {
            return fs::absolute(PathFromUtf8(value)).lexically_normal();
        }

        [[nodiscard]] std::string SanitizeArchivePath(
            std::string value, bool preserveDriveRoot)
        {
#if defined(_WIN32)
            std::size_t offset = 0;
            if (preserveDriveRoot && value.size() >= 3U
                && ((value[0] >= 'A' && value[0] <= 'Z')
                    || (value[0] >= 'a' && value[0] <= 'z'))
                && value[1] == ':' && (value[2] == '/' || value[2] == '\\'))
            {
                offset = 3U;
            }
            for (std::size_t i = offset; i < value.size(); ++i)
            {
                const unsigned char ch = static_cast<unsigned char>(value[i]);
                if (ch <= 0x1FU || value[i] == '"' || value[i] == '*'
                    || value[i] == ':' || value[i] == '<' || value[i] == '>'
                    || value[i] == '?' || value[i] == '|')
                {
                    value[i] = '_';
                }
            }
#else
            (void)preserveDriveRoot;
            std::replace(value.begin(), value.end(), '\0', '_');
#endif
            return value;
        }

        [[nodiscard]] fs::path ResolveArchivePath(
            const std::string& destination, const std::string& rawName, bool zip)
        {
            const std::string name = SanitizeArchivePath(rawName, !zip);
            const fs::path root = FullNormalizedPath(destination);
            std::string extractionRoot = PathToUtf8(root);
            if (extractionRoot.empty()
                || !IsDotNetDirectorySeparator(extractionRoot.back()))
            {
                extractionRoot.push_back(
                    static_cast<char>(fs::path::preferred_separator));
            }

            std::string combined;
            if (zip)
            {
                combined = IsDotNetPathRooted(name)
                    ? name
                    : DotNetJoin(extractionRoot, name);
            }
            else
            {
                combined = IsDotNetPathFullyQualified(name)
                    ? name
                    : DotNetJoin(extractionRoot, name);
            }

            const fs::path output = FullNormalizedPath(combined);
            if (!IsWithinDirectory(root, output))
            {
                if (zip)
                {
                    throw IOException("Extracting the Zip entry would have resulted in a file outside the specified destination directory.");
                }
                std::string directory = PathToUtf8(root);
                if (directory.empty()
                    || (directory.back() != '/' && directory.back() != '\\'))
                {
                    directory.push_back(static_cast<char>(fs::path::preferred_separator));
                }
                throw IOException("Extracting the Tar entry '" + name
                    + "' would have resulted in a file outside the specified destination directory: '"
                    + directory + "'");
            }
            return output;
        }

        void ValidateTarLink(
            const std::string& destination, const fs::path& output,
            struct archive_entry* entry)
        {
            const char* symbolic = archive_entry_symlink(entry);
            const char* hard = archive_entry_hardlink(entry);
            if (symbolic == nullptr && hard == nullptr)
            {
                return;
            }

            const std::string linkName = SanitizeArchivePath(
                symbolic != nullptr ? symbolic : hard, symbolic != nullptr);
            const fs::path root = FullNormalizedPath(destination);

            std::string combined;
            if (symbolic != nullptr)
            {
                combined = IsDotNetPathFullyQualified(linkName)
                    ? linkName
                    : DotNetJoin(PathToUtf8(output.parent_path()), linkName);
            }
            else
            {
                combined = DotNetJoin(destination, linkName);
            }

            const fs::path resolved = FullNormalizedPath(combined);
            if (!IsWithinDirectory(root, resolved))
            {
                std::string directory = PathToUtf8(root);
                if (directory.empty()
                    || (directory.back() != '/' && directory.back() != '\\'))
                {
                    directory.push_back(static_cast<char>(fs::path::preferred_separator));
                }
                throw IOException("Extracting the Tar entry '" + linkName
                    + "' would have resulted in a link target outside the specified destination directory: '"
                    + directory + "'");
            }
            if (symbolic != nullptr)
            {
                archive_entry_set_symlink(entry, linkName.c_str());
            }
            else
            {
                const std::string target = PathToUtf8(resolved);
                archive_entry_set_hardlink(entry, target.c_str());
            }
        }

        void SetArchiveFileLastWriteTime(
            const std::string& path, struct archive_entry* entry) noexcept
        {
            if (archive_entry_mtime_is_set(entry) == 0)
            {
                return;
            }
#if defined(_WIN32)
            try
            {
                constexpr std::int64_t EpochOffsetSeconds = 11644473600LL;
                const std::int64_t seconds = archive_entry_mtime(entry);
                const long nanoseconds = archive_entry_mtime_nsec(entry);
                if (seconds < -EpochOffsetSeconds)
                {
                    return;
                }
                const std::uint64_t ticks = static_cast<std::uint64_t>(
                    seconds + EpochOffsetSeconds) * 10000000ULL
                    + static_cast<std::uint64_t>(nanoseconds / 100L);
                FILETIME writeTime{
                    static_cast<DWORD>(ticks & 0xFFFFFFFFULL),
                    static_cast<DWORD>(ticks >> 32U)};
                const std::wstring native = Utf8ToWide(path);
                HANDLE handle = ::CreateFileW(native.c_str(), FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (handle != INVALID_HANDLE_VALUE)
                {
                    (void)::SetFileTime(handle, nullptr, nullptr, &writeTime);
                    (void)::CloseHandle(handle);
                }
            }
            catch (...)
            {
            }
#else
            struct timespec times[2]{};
            times[0].tv_nsec = UTIME_OMIT;
            times[1].tv_sec = static_cast<time_t>(archive_entry_mtime(entry));
            times[1].tv_nsec = archive_entry_mtime_nsec(entry);
            (void)::utimensat(AT_FDCWD, PathFromUtf8(path).c_str(), times, 0);
#endif
        }

        void ExtractZipFile(struct archive* reader, struct archive_entry* entry,
            const std::string& output, const std::optional<std::string>& linkText)
        {
            const fs::path parent = PathFromUtf8(output).parent_path();
            if (!parent.empty())
            {
                DirectoryCreateDirectory(PathToUtf8(parent));
            }

#if defined(_WIN32)
            const std::wstring native = Utf8ToWide(output);
            HANDLE handle = ::CreateFileW(native.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
            {
                ThrowFileError(output, std::error_code(
                    static_cast<int>(::GetLastError()), std::system_category()));
            }
            auto closeHandle = [&]()
            {
                if (handle != INVALID_HANDLE_VALUE)
                {
                    const HANDLE current = handle;
                    handle = INVALID_HANDLE_VALUE;
                    if (!::CloseHandle(current))
                    {
                        ThrowFileError(output, std::error_code(
                            static_cast<int>(::GetLastError()), std::system_category()));
                    }
                }
            };
            try
            {
                auto write = [&](const void* data, std::size_t size, la_int64_t offset)
                {
                    LARGE_INTEGER position{};
                    position.QuadPart = offset;
                    if (!::SetFilePointerEx(handle, position, nullptr, FILE_BEGIN))
                    {
                        ThrowFileError(output, std::error_code(
                            static_cast<int>(::GetLastError()), std::system_category()));
                    }
                    const char* bytes = static_cast<const char*>(data);
                    while (size > 0)
                    {
                        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
                            size, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
                        DWORD written = 0;
                        if (!::WriteFile(handle, bytes, chunk, &written, nullptr)
                            || written == 0)
                        {
                            ThrowFileError(output, std::error_code(
                                static_cast<int>(::GetLastError()), std::system_category()));
                        }
                        bytes += written;
                        size -= written;
                    }
                };
                if (linkText.has_value())
                {
                    write(linkText->data(), linkText->size(), 0);
                }
                else
                {
                    for (;;)
                    {
                        const void* block = nullptr;
                        std::size_t size = 0;
                        la_int64_t offset = 0;
                        const int result = archive_read_data_block(
                            reader, &block, &size, &offset);
                        if (result == ARCHIVE_EOF)
                        {
                            break;
                        }
                        if (result != ARCHIVE_OK)
                        {
                            ThrowArchive(reader);
                        }
                        write(block, size, offset);
                    }
                }
                closeHandle();
            }
            catch (...)
            {
                if (handle != INVALID_HANDLE_VALUE)
                {
                    (void)::CloseHandle(handle);
                }
                throw;
            }
#else
            int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_CLOEXEC
            flags |= O_CLOEXEC;
#endif
            const mode_t archiveMode = static_cast<mode_t>(archive_entry_perm(entry) & 0777);
            const mode_t createMode = archiveMode == 0 ? 0666 : archiveMode;
            int fd = ::open(PathFromUtf8(output).c_str(), flags, createMode);
            if (fd < 0)
            {
                ThrowFileError(output, std::error_code(errno, std::generic_category()));
            }
            auto closeFd = [&]()
            {
                if (fd >= 0)
                {
                    const int current = fd;
                    fd = -1;
                    if (::close(current) != 0)
                    {
                        ThrowFileError(output,
                            std::error_code(errno, std::generic_category()));
                    }
                }
            };
            try
            {
                auto write = [&](const void* data, std::size_t size, la_int64_t offset)
                {
                    if (::lseek(fd, static_cast<off_t>(offset), SEEK_SET) < 0)
                    {
                        ThrowFileError(output,
                            std::error_code(errno, std::generic_category()));
                    }
                    const char* bytes = static_cast<const char*>(data);
                    while (size > 0)
                    {
                        const ssize_t written = ::write(fd, bytes, size);
                        if (written > 0)
                        {
                            bytes += written;
                            size -= static_cast<std::size_t>(written);
                        }
                        else if (written < 0 && errno == EINTR)
                        {
                            continue;
                        }
                        else
                        {
                            ThrowFileError(output,
                                std::error_code(errno, std::generic_category()));
                        }
                    }
                };
                if (linkText.has_value())
                {
                    write(linkText->data(), linkText->size(), 0);
                }
                else
                {
                    for (;;)
                    {
                        const void* block = nullptr;
                        std::size_t size = 0;
                        la_int64_t offset = 0;
                        const int result = archive_read_data_block(
                            reader, &block, &size, &offset);
                        if (result == ARCHIVE_EOF)
                        {
                            break;
                        }
                        if (result != ARCHIVE_OK)
                        {
                            ThrowArchive(reader);
                        }
                        write(block, size, offset);
                    }
                }
                closeFd();
            }
            catch (...)
            {
                if (fd >= 0)
                {
                    (void)::close(fd);
                }
                throw;
            }
#endif
            SetArchiveFileLastWriteTime(output, entry);
        }

        void ExtractArchive(const std::string& archivePath,
            const std::string& destination, bool zip)
        {
            struct ArchiveReaderDeleter
            {
                void operator()(struct archive* value) const noexcept
                {
                    if (value != nullptr)
                    {
                        archive_read_free(value);
                    }
                }
            };
            struct ArchiveWriterDeleter
            {
                void operator()(struct archive* value) const noexcept
                {
                    if (value != nullptr)
                    {
                        archive_write_free(value);
                    }
                }
            };

            std::unique_ptr<struct archive, ArchiveReaderDeleter> reader(archive_read_new());
            if (!reader)
            {
                throw std::bad_alloc();
            }
            if (zip)
            {
                if (archive_read_support_format_zip(reader.get()) != ARCHIVE_OK)
                {
                    ThrowArchive(reader.get());
                }
                if (archive_read_set_format_option(
                    reader.get(), "zip", "mac-ext", nullptr) != ARCHIVE_OK
                    || archive_read_set_format_option(
                        reader.get(), "zip", "hdrcharset", "UTF-8") != ARCHIVE_OK)
                {
                    ThrowArchive(reader.get());
                }
            }
            else
            {
                if (archive_read_support_filter_gzip(reader.get()) != ARCHIVE_OK
                    || archive_read_support_format_tar(reader.get()) != ARCHIVE_OK)
                {
                    ThrowArchive(reader.get());
                }
                if (archive_read_set_format_option(
                    reader.get(), "tar", "mac-ext", nullptr) != ARCHIVE_OK
                    || archive_read_set_format_option(
                        reader.get(), "tar", "hdrcharset", "UTF-8") != ARCHIVE_OK)
                {
                    ThrowArchive(reader.get());
                }
            }

#if defined(_WIN32)
            const std::wstring archiveWide = Utf8ToWide(archivePath);
            if (archive_read_open_filename_w(reader.get(), archiveWide.c_str(), 64U * 1024U)
                != ARCHIVE_OK)
#else
            if (archive_read_open_filename(reader.get(), archivePath.c_str(), 64U * 1024U)
                != ARCHIVE_OK)
#endif
            {
                ThrowArchive(reader.get());
            }

            std::unique_ptr<struct archive, ArchiveWriterDeleter> writer;
            if (!zip)
            {
                writer.reset(archive_write_disk_new());
                if (!writer)
                {
                    throw std::bad_alloc();
                }
                archive_write_disk_set_options(writer.get(),
                    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_UNLINK);
                archive_write_disk_set_standard_lookup(writer.get());
            }

            struct archive_entry* entry = nullptr;
            for (;;)
            {
                const int next = archive_read_next_header(reader.get(), &entry);
                if (next == ARCHIVE_EOF)
                {
                    break;
                }
                if (next != ARCHIVE_OK)
                {
                    ThrowArchive(reader.get());
                }

                const char* rawName = archive_entry_pathname_utf8(entry);
                if (rawName == nullptr)
                {
                    rawName = archive_entry_pathname(entry);
                }
                if (rawName == nullptr)
                {
                    throw ArchiveException("archive entry has no path");
                }
                const std::string name(rawName);
                const fs::path outputPath = ResolveArchivePath(destination, name, zip);
                const std::string output = PathToUtf8(outputPath);

                if (zip)
                {
                    const bool directoryEntry = name.empty()
                        || name.back() == '/'
#if defined(_WIN32)
                        || name.back() == '\\'
#endif
                        ;
                    if (directoryEntry)
                    {
                        if (archive_entry_size(entry) != 0)
                        {
                            throw IOException("Zip entry name ends in directory separator character but contains data.");
                        }
                        DirectoryCreateDirectory(output);
                        continue;
                    }

                    std::optional<std::string> linkText;
                    if (const char* link = archive_entry_symlink(entry); link != nullptr)
                    {
                        linkText = std::string(link);
                    }
                    ExtractZipFile(reader.get(), entry, output, linkText);
                    continue;
                }

                ValidateTarLink(destination, outputPath, entry);
                archive_entry_unset_atime(entry);
                archive_entry_unset_ctime(entry);
                archive_entry_unset_birthtime(entry);
                const bool symbolicEntry = archive_entry_symlink(entry) != nullptr;
                const bool hardLinkEntry = archive_entry_hardlink(entry) != nullptr;
                const auto fileType = archive_entry_filetype(entry);
                if (symbolicEntry || hardLinkEntry
                    || (fileType != AE_IFREG && fileType != AE_IFDIR))
                {
                    archive_entry_unset_mtime(entry);
                }
                if (fileType == AE_IFREG && !hardLinkEntry)
                {
                    archive_entry_set_perm(entry, archive_entry_perm(entry) & 0777);
                }

#if defined(_WIN32)
                const std::wstring outputWide = Utf8ToWide(output);
                archive_entry_copy_pathname_w(entry, outputWide.c_str());
#else
                archive_entry_set_pathname(entry, output.c_str());
#endif

                const int headerResult = archive_write_header(writer.get(), entry);
                if (headerResult < ARCHIVE_OK)
                {
                    const char* message = archive_error_string(writer.get());
                    throw ArchiveException(message == nullptr
                        ? "archive extraction failed" : message);
                }
                if (archive_entry_size(entry) > 0)
                {
                    for (;;)
                    {
                        const void* block = nullptr;
                        std::size_t size = 0;
                        la_int64_t offset = 0;
                        const int dataResult = archive_read_data_block(
                            reader.get(), &block, &size, &offset);
                        if (dataResult == ARCHIVE_EOF)
                        {
                            break;
                        }
                        if (dataResult != ARCHIVE_OK)
                        {
                            ThrowArchive(reader.get());
                        }
                        const la_ssize_t writeResult = archive_write_data_block(
                            writer.get(), block, size, offset);
                        if (writeResult < ARCHIVE_OK)
                        {
                            const char* message = archive_error_string(writer.get());
                            throw ArchiveException(message == nullptr
                                ? "archive extraction failed" : message);
                        }
                    }
                }
                const int finishResult = archive_write_finish_entry(writer.get());
                if (finishResult != ARCHIVE_OK)
                {
                    const char* message = archive_error_string(writer.get());
                    throw ArchiveException(message == nullptr
                        ? "archive extraction failed" : message);
                }
            }
        }
    }

    void ArchiveExtractToDirectory(const std::string& archivePath,
        const std::string& destination, bool zip)
    {
        ExtractArchive(archivePath, destination, zip);
    }

    void TarFileExtractGZipToDirectory(const std::string& archivePath, const std::string& destination)
    {
        ExtractArchive(archivePath, destination, false);
    }
}

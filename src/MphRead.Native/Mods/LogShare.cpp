#include "LogShare.hpp"

#include "NativeRuntime/System/AtomicSharedPtr.hpp"

#include "Branding.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/DateTime.hpp"
#include "../NativeRuntime/System/ZipArchive.hpp"
#include "../NativeRuntime/System/Sort.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <zlib.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::Utf16ToUtf8;
using ::MphRead::NativeRuntime::Utf8ToUtf16;

using ::MphRead::NativeRuntime::ManagedSort;
namespace
{
    using TickDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;
    constexpr std::uint16_t ZipFlagDataDescriptor = 0x0008U;
    constexpr std::uint16_t ZipFlagUtf8 = 0x0800U;
    constexpr std::uint16_t ZipMethodStore = 0U;
    constexpr std::uint16_t ZipMethodDeflate = 8U;
    constexpr std::uint32_t Zip32Max = 0xFFFFFFFFU;

    class DotNetArgumentException final : public std::runtime_error
    {
    public:
        explicit DotNetArgumentException(const char* message) : std::runtime_error(message) {}
    };

    [[nodiscard]] std::filesystem::path NativePath(std::u16string_view value)
    {
#if defined(_WIN32)
        static_assert(sizeof(wchar_t) == sizeof(char16_t));
        return std::filesystem::path(std::wstring(
            reinterpret_cast<const wchar_t*>(value.data()), value.size()));
#else
        return std::filesystem::path(Utf16ToUtf8(value));
#endif
    }

    [[nodiscard]] std::u16string ManagedPath(const std::filesystem::path& value)
    {
#if defined(_WIN32)
        static_assert(sizeof(wchar_t) == sizeof(char16_t));
        const std::wstring& native = value.native();
        return std::u16string(reinterpret_cast<const char16_t*>(native.data()), native.size());
#else
        return Utf8ToUtf16(value.native());
#endif
    }

    [[nodiscard]] bool ContainsNull(std::u16string_view value) noexcept
    {
        return value.find(u'\0') != std::u16string_view::npos;
    }

    [[nodiscard]] std::u16string CombineLogsPath(std::u16string base)
    {
        constexpr std::u16string_view child = u"logs";
        if (base.empty())
        {
            return std::u16string(child);
        }
#if defined(_WIN32)
        const char16_t last = base.back();
        if (last != u'\\' && last != u'/' && last != u':')
        {
            base.push_back(u'\\');
        }
#else
        if (base.back() != u'/')
        {
            base.push_back(u'/');
        }
#endif
        base.append(child);
        return base;
    }

#if defined(_WIN32)
    [[nodiscard]] std::chrono::system_clock::time_point ToSystemClock(
        std::filesystem::file_time_type value)
    {
        const auto fileNow = std::filesystem::file_time_type::clock::now();
        const auto systemNow = std::chrono::system_clock::now();
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            systemNow + (value - fileNow));
    }

    [[nodiscard]] std::int64_t LastWriteTicks(std::filesystem::file_time_type value)
    {
        const auto systemValue = ToSystemClock(value);
        return std::chrono::duration_cast<TickDuration>(systemValue.time_since_epoch()).count();
    }
#else
    [[nodiscard]] std::int64_t LastWriteTicks(const struct stat& value)
    {
#if defined(__APPLE__)
        const std::int64_t seconds = static_cast<std::int64_t>(value.st_mtimespec.tv_sec);
        const std::int64_t nanoseconds = static_cast<std::int64_t>(value.st_mtimespec.tv_nsec);
#else
        const std::int64_t seconds = static_cast<std::int64_t>(value.st_mtim.tv_sec);
        const std::int64_t nanoseconds = static_cast<std::int64_t>(value.st_mtim.tv_nsec);
#endif
        if (seconds < -62135596800LL || seconds > 253402300799LL)
        {
            throw std::out_of_range("The file timestamp is outside the DateTime range.");
        }
        return seconds * 10000000LL + nanoseconds / 100LL;
    }

    [[nodiscard]] int OpenFile(const std::filesystem::path& path, int flags, mode_t mode = 0)
    {
        int fd = -1;
        do
        {
            fd = mode == 0 ? ::open(path.c_str(), flags) : ::open(path.c_str(), flags, mode);
        }
        while (fd < 0 && errno == EINTR);
        return fd;
    }

    [[nodiscard]] bool TryFileShareLock(int fd, int operation)
    {
        int result = 0;
        do
        {
            result = ::flock(fd, operation | LOCK_NB);
        }
        while (result < 0 && errno == EINTR);
        if (result == 0)
        {
            return true;
        }
        const int code = errno;
        if (code == EWOULDBLOCK || code == EAGAIN)
        {
            throw std::system_error(code, std::generic_category());
        }
        return false;
    }

    void UnlockFileShare(int fd, bool& locked) noexcept
    {
        if (locked)
        {
            int result = 0;
            do
            {
                result = ::flock(fd, LOCK_UN);
            }
            while (result < 0 && errno == EINTR);
            locked = false;
        }
    }
#endif

    [[nodiscard]] std::chrono::system_clock::time_point SystemTimeFromTicks(std::int64_t ticks)
    {
        const std::chrono::time_point<std::chrono::system_clock, TickDuration> value{
            TickDuration{ticks}};
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(value);
    }

    [[nodiscard]] bool MatchesLogName(std::u16string_view name) noexcept
    {
        if (name.size() < 4)
        {
            return false;
        }
        std::u16string_view extension = name.substr(name.size() - 4);
#if defined(_WIN32)
        auto lowerAscii = [](char16_t value) noexcept -> char16_t
        {
            return value >= u'A' && value <= u'Z'
                ? static_cast<char16_t>(value + (u'a' - u'A'))
                : value;
        };
        return extension[0] == u'.'
            && lowerAscii(extension[1]) == u'l'
            && lowerAscii(extension[2]) == u'o'
            && lowerAscii(extension[3]) == u'g';
#else
        return extension == u".log";
#endif
    }

    [[nodiscard]] std::u16string ExceptionMessage(const std::exception& ex)
    {
        return Utf8ToUtf16(ex.what());
    }

}

namespace MphRead::Mods
{
    LogFileInfo::LogFileInfo(std::filesystem::path path, std::u16string fullName,
        std::u16string name, std::int64_t lastWriteTicks)
        : _path(std::move(path)), _fullName(std::move(fullName)), _name(std::move(name)),
          _lastWriteTicks(lastWriteTicks)
    {
    }

    const std::u16string& LogFileInfo::FullName() const noexcept
    {
        return _fullName;
    }

    const std::u16string& LogFileInfo::Name() const noexcept
    {
        return _name;
    }

    std::chrono::system_clock::time_point LogFileInfo::LastWriteTimeUtc() const noexcept
    {
        return SystemTimeFromTicks(_lastWriteTicks);
    }

    std::u16string LogArchive::Directory()
    {
        return CombineLogsPath(Utf8ToUtf16(Launcher::LauncherPrefs::Directory()));
    }

    std::vector<LogFileInfo> LogArchive::Files()
    {
        std::vector<LogFileInfo> files;
        try
        {
            const std::u16string directoryText = Directory();
            if (ContainsNull(directoryText))
            {
                throw DotNetArgumentException("Null character in path. (Parameter 'path')");
            }
            std::filesystem::path directory = NativePath(directoryText);
            std::error_code existsError;
            if (!std::filesystem::is_directory(directory, existsError) || existsError)
            {
                return files;
            }
            directory = std::filesystem::absolute(directory).lexically_normal();
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(directory))
            {
                const std::u16string name = ManagedPath(entry.path().filename());
                if (!MatchesLogName(name))
                {
                    continue;
                }

                std::int64_t ticks = 0;
#if defined(_WIN32)
                std::error_code typeError;
                if (entry.is_directory(typeError) || typeError)
                {
                    if (typeError)
                    {
                        throw std::filesystem::filesystem_error(
                            "Could not read file attributes", entry.path(), typeError);
                    }
                    continue;
                }
                ticks = LastWriteTicks(entry.last_write_time());
#else
                struct stat status{};
                if (::lstat(entry.path().c_str(), &status) != 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
                bool isDirectory = S_ISDIR(status.st_mode);
                if (S_ISLNK(status.st_mode))
                {
                    struct stat target{};
                    int statResult = 0;
                    do
                    {
                        statResult = ::stat(entry.path().c_str(), &target);
                    }
                    while (statResult < 0 && errno == EINTR);
                    isDirectory = statResult == 0 && S_ISDIR(target.st_mode);
                }
                if (isDirectory)
                {
                    continue;
                }
                ticks = LastWriteTicks(status);
#endif
                files.push_back(LogFileInfo(
                    entry.path(), ManagedPath(entry.path()), name, ticks));
            }
            ManagedSort(files, [](const LogFileInfo& left, const LogFileInfo& right) noexcept
            {
                if (right._lastWriteTicks > left._lastWriteTicks)
                {
                    return 1;
                }
                if (right._lastWriteTicks < left._lastWriteTicks)
                {
                    return -1;
                }
                return 0;
            });
        }
        catch (...)
        {
            files.clear();
        }
        return files;
    }

    bool LogArchive::Any()
    {
        return !Files().empty();
    }

    bool LogArchive::Create(std::u16string_view zipPath, std::u16string& error)
    {
        std::u16string zipPathStorage;
        if (zipPath.data() == error.data())
        {
            zipPathStorage.assign(zipPath);
            zipPath = zipPathStorage;
        }
        error.clear();
        std::vector<LogFileInfo> files = Files();
        if (files.empty())
        {
            error = u"there are no logs yet";
            return false;
        }
        try
        {
            if (zipPath.empty())
            {
                throw DotNetArgumentException(
                    "The value cannot be an empty string. (Parameter 'path')");
            }
            if (ContainsNull(zipPath))
            {
                throw DotNetArgumentException("Null character in path. (Parameter 'path')");
            }
            const std::filesystem::path archivePath = NativePath(zipPath);
            const std::filesystem::path parent = archivePath.parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent);
            }
            if (FileExists(PathToUtf8(archivePath)))
            {
                std::filesystem::remove(archivePath);
            }

            using ::MphRead::NativeRuntime::FileAccess;
            using ::MphRead::NativeRuntime::FileMode;
            using ::MphRead::NativeRuntime::FileShare;
            using ::MphRead::NativeRuntime::FileStream;
            auto stream = std::make_shared<FileStream>(PathToUtf8(archivePath), FileMode::Create,
                FileAccess::Write, FileShare::None);
            ::MphRead::NativeRuntime::ZipArchive zip(stream, ::MphRead::NativeRuntime::ZipArchiveMode::Create, true);
            bool noReadableLogs = false;
            std::exception_ptr pending;
            try
            {
                int written = 0;
                for (const LogFileInfo& file : files)
                {
                    try
                    {
                        FileStream source(PathToUtf8(file._path), FileMode::Open, FileAccess::Read,
                            FileShare::ReadWrite);
                        const auto entry = zip.CreateEntry(Utf16ToUtf8(file._name));
                        entry->LastWriteTime(file.LastWriteTimeUtc());
                        const std::shared_ptr<::MphRead::NativeRuntime::Stream> target = entry->Open();
                        try
                        {
                            source.CopyTo(*target);
                            ++written;
                        }
                        catch (...)
                        {
                            target->Dispose();
                            throw;
                        }
                        target->Dispose();
                    }
                    catch (...)
                    {
                    }
                }
                if (written == 0)
                {
                    error = u"none of the logs could be read";
                    noReadableLogs = true;
                }
            }
            catch (...)
            {
                pending = std::current_exception();
            }

            try
            {
                zip.Dispose();
            }
            catch (...)
            {
                pending = std::current_exception();
            }
            try
            {
                stream->Dispose();
            }
            catch (...)
            {
                pending = std::current_exception();
            }
            if (pending)
            {
                std::rethrow_exception(pending);
            }
            if (noReadableLogs)
            {
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            error = ExceptionMessage(ex);
            return false;
        }
        catch (...)
        {
            error = u"An exception was thrown while creating the log archive.";
            return false;
        }
        return true;
    }

    std::u16string LogArchive::FileName()
    {
        std::string product(Branding::Name);
        product.erase(std::remove(product.begin(), product.end(), ' '), product.end());
        return Utf8ToUtf16(product + "-logs-" + ::MphRead::NativeRuntime::DateTimeToString(
            ::MphRead::NativeRuntime::DateTimeNow(), "yyyyMMdd-HHmmss") + ".zip");
    }

    ::MphRead::NativeRuntime::AtomicSharedPtr<ILogShare> LogShare::_current{};

    std::shared_ptr<ILogShare> LogShare::Current()
    {
        return _current.load();
    }

    void LogShare::Current(std::shared_ptr<ILogShare> value)
    {
        _current.store(std::move(value));
    }

    bool LogShare::Available()
    {
        return _current.load() != nullptr && LogArchive::Any();
    }
}

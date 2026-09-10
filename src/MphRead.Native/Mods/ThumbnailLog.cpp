#include "ThumbnailLog.hpp"
#include "Branding.hpp"
#include "Update/BuildVersion.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <stdlib.h>
#include <sys/auxv.h>
#elif defined(__unix__)
#include <stdlib.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace MphRead::Mods::Launcher::Detail
{
    // Narrow link boundary for Launcher.GameFiles.Root. GameFiles has not been
    // ported to Native yet; its eventual native owner must return the current
    // Root value here rather than ThumbnailLog owning a second copy of it.
    [[nodiscard]] std::string ThumbnailLogGameFilesRoot();
}

namespace
{
    [[nodiscard]] constexpr char DirectorySeparator() noexcept
    {
#if defined(_WIN32)
        return '\\';
#else
        return '/';
#endif
    }

    [[nodiscard]] constexpr bool EndsPathCombineBoundary(char value) noexcept
    {
#if defined(_WIN32)
        // Path.Combine does not insert a separator after either directory
        // separator or the Windows volume separator (for example, "C:").
        return value == '\\' || value == '/' || value == ':';
#else
        return value == '/';
#endif
    }

    [[nodiscard]] std::string CombinePath(std::string root, std::string_view leaf)
    {
        if (root.empty())
        {
            return std::string(leaf);
        }
        if (!EndsPathCombineBoundary(root.back()))
        {
            root.push_back(DirectorySeparator());
        }
        root.append(leaf);
        return root;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
        if (value.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("path contains a null character");
        }
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (const unsigned char byte : value)
        {
            converted.push_back(static_cast<char8_t>(byte));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::optional<std::filesystem::path> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return std::filesystem::path(std::wstring(buffer.data(), length));
            }
            if (buffer.size() > static_cast<std::size_t>(
                    std::numeric_limits<DWORD>::max()) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 1024;
        std::vector<char> buffer(size);
        while (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            buffer.resize(size);
        }
        std::unique_ptr<char, decltype(&std::free)> resolved(
            ::realpath(buffer.data(), nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return PathFromUtf8(resolved.get());
#elif defined(__FreeBSD__)
        static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        std::size_t size = 0;
        if (::sysctl(name, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (::sysctl(name, 4, buffer.data(), &size, nullptr, 0) != 0 || size == 0)
        {
            return std::nullopt;
        }
        const std::size_t length = buffer[size - 1] == '\0' ? size - 1 : size;
        return PathFromUtf8(std::string_view(buffer.data(), length));
#elif defined(__linux__)
        std::unique_ptr<char, decltype(&std::free)> resolved(
            ::realpath("/proc/self/exe", nullptr), &std::free);
        if (resolved)
        {
            return PathFromUtf8(resolved.get());
        }
#if defined(AT_EXECFN)
        const auto executable = reinterpret_cast<const char*>(::getauxval(AT_EXECFN));
        if (executable != nullptr)
        {
            resolved.reset(::realpath(executable, nullptr));
            if (resolved)
            {
                return PathFromUtf8(resolved.get());
            }
        }
#endif
        return std::nullopt;
#elif defined(__unix__)
        std::unique_ptr<char, decltype(&std::free)> resolved(
            ::realpath("/proc/curproc/exe", nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return PathFromUtf8(resolved.get());
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::filesystem::path AppContextBaseDirectory()
    {
        const std::optional<std::filesystem::path> processPath = ProcessPath();
        if (!processPath.has_value())
        {
            throw std::runtime_error("Could not determine AppContext.BaseDirectory.");
        }
        return processPath->parent_path();
    }

    [[nodiscard]] std::tm LocalTime(std::time_t value)
    {
        std::tm result{};
#if defined(_WIN32)
        if (::localtime_s(&result, &value) != 0)
        {
            throw std::runtime_error("Could not convert local time.");
        }
#elif defined(__unix__) || defined(__APPLE__)
        if (::localtime_r(&value, &result) == nullptr)
        {
            throw std::runtime_error("Could not convert local time.");
        }
#else
        static std::mutex localTimeLock;
        const std::lock_guard<std::mutex> guard(localTimeLock);
        const std::tm* converted = std::localtime(&value);
        if (converted == nullptr)
        {
            throw std::runtime_error("Could not convert local time.");
        }
        result = *converted;
#endif
        return result;
    }

    [[nodiscard]] std::string FormatDateTime(const std::tm& value, bool seconds)
    {
        char buffer[32]{};
        const int written = seconds
            ? std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                value.tm_hour, value.tm_min, value.tm_sec)
            : std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d",
                value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                value.tm_hour, value.tm_min);
        if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buffer))
        {
            throw std::runtime_error("Could not format local time.");
        }
        return std::string(buffer, static_cast<std::size_t>(written));
    }

    [[nodiscard]] std::string Now(bool seconds)
    {
        const std::time_t value = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        return FormatDateTime(LocalTime(value), seconds);
    }

    [[nodiscard]] std::chrono::system_clock::time_point ToSystemClock(
        std::filesystem::file_time_type value)
    {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::filesystem::file_time_type::clock::to_sys(value));
#else
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            value - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
#endif
    }

    [[nodiscard]] std::string BaseDirectoryLastWriteTime()
    {
        const std::filesystem::file_time_type fileTime
            = std::filesystem::last_write_time(AppContextBaseDirectory());
        const std::time_t value = std::chrono::system_clock::to_time_t(
            ToSystemClock(fileTime));
        // The C# interpolation deliberately uses a format string beginning in
        // ':': $"{date::yyyy-MM-dd HH:mm}". ConsoleSetup installs invariant
        // culture in Native, whose time separator is ':'.
        return ":" + FormatDateTime(LocalTime(value), false);
    }

    [[nodiscard]] std::string EntryAssemblyVersion()
    {
        // A native executable has no System.Reflection Assembly identity.
        // The build may supply the exact managed-equivalent assembly version;
        // otherwise preserve the source expression's null fallback, "?".
#if defined(MPHREAD_ENTRY_ASSEMBLY_VERSION)
        constexpr char version[] = MPHREAD_ENTRY_ASSEMBLY_VERSION;
        return std::string(version, sizeof(version) - 1);
#else
        return "?";
#endif
    }

    [[nodiscard]] constexpr std::string_view NewLine() noexcept
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }

    class NonIoFileFailure final : public std::runtime_error
    {
    public:
        explicit NonIoFileFailure(const char* message)
            : std::runtime_error(message)
        {
        }
    };

#if defined(_WIN32)
    [[noreturn]] void ThrowFileFailure(DWORD error)
    {
        switch (error)
        {
        case ERROR_ACCESS_DENIED:
        case ERROR_OPERATION_ABORTED:
            throw NonIoFileFailure("file access was not permitted");
        default:
            throw std::ios_base::failure(
                "thumbnail log I/O failed",
                std::error_code(static_cast<int>(error), std::system_category()));
        }
    }

    class Win32FileHandle final
    {
    public:
        explicit Win32FileHandle(HANDLE value) noexcept
            : _value(value)
        {
        }

        Win32FileHandle(const Win32FileHandle&) = delete;
        Win32FileHandle& operator=(const Win32FileHandle&) = delete;

        ~Win32FileHandle()
        {
            if (_value != INVALID_HANDLE_VALUE)
            {
                (void)::CloseHandle(_value);
            }
        }

        [[nodiscard]] HANDLE Get() const noexcept
        {
            return _value;
        }

    private:
        HANDLE _value;
    };

    void WriteBytes(const std::filesystem::path& path, std::string_view text,
        std::ios::openmode mode)
    {
        const bool append = (mode & std::ios::app) != std::ios::openmode{};
        const DWORD creationDisposition = append ? OPEN_ALWAYS : CREATE_ALWAYS;

        const HANDLE rawHandle = ::CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            creationDisposition,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (rawHandle == INVALID_HANDLE_VALUE)
        {
            ThrowFileFailure(::GetLastError());
        }
        const Win32FileHandle handle(rawHandle);

        if (append)
        {
            LARGE_INTEGER length{};
            if (::GetFileSizeEx(handle.Get(), &length) == 0)
            {
                ThrowFileFailure(::GetLastError());
            }
            LARGE_INTEGER position{};
            position.QuadPart = length.QuadPart;
            if (::SetFilePointerEx(handle.Get(), position, nullptr, FILE_BEGIN) == 0)
            {
                ThrowFileFailure(::GetLastError());
            }
        }

        std::size_t offset = 0;
        constexpr std::size_t MaxChunk = static_cast<std::size_t>(
            std::numeric_limits<DWORD>::max());
        while (offset < text.size())
        {
            const std::size_t count = std::min(MaxChunk, text.size() - offset);
            DWORD written = 0;
            if (::WriteFile(
                    handle.Get(),
                    text.data() + offset,
                    static_cast<DWORD>(count),
                    &written,
                    nullptr) == 0)
            {
                ThrowFileFailure(::GetLastError());
            }
            if (written == 0)
            {
                throw std::ios_base::failure("thumbnail log write made no progress");
            }
            offset += static_cast<std::size_t>(written);
        }
    }
#elif defined(__unix__) || defined(__APPLE__)
    [[noreturn]] void ThrowFileFailure(int error)
    {
        switch (error)
        {
#ifdef EACCES
        case EACCES:
#endif
#ifdef EBADF
        case EBADF:
#endif
#ifdef EPERM
        case EPERM:
#endif
#ifdef EISDIR
        case EISDIR:
#endif
#ifdef EFBIG
        case EFBIG:
#endif
#ifdef ECANCELED
        case ECANCELED:
#endif
            throw NonIoFileFailure("file access was not permitted");
        default:
            throw std::ios_base::failure(
                "thumbnail log I/O failed",
                std::error_code(error, std::generic_category()));
        }
    }

    class FileDescriptor final
    {
    public:
        explicit FileDescriptor(int value) noexcept
            : _value(value)
        {
        }

        FileDescriptor(const FileDescriptor&) = delete;
        FileDescriptor& operator=(const FileDescriptor&) = delete;

        ~FileDescriptor()
        {
            if (_value >= 0)
            {
                (void)::close(_value);
            }
        }

        [[nodiscard]] int Get() const noexcept
        {
            return _value;
        }

    private:
        int _value;
    };

    void AcquireSharedFileLock(int descriptor)
    {
        for (;;)
        {
            if (::flock(descriptor, LOCK_SH | LOCK_NB) == 0)
            {
                return;
            }
            const int error = errno;
#ifdef EINTR
            if (error == EINTR)
            {
                continue;
            }
#endif
#ifdef EWOULDBLOCK
            if (error == EWOULDBLOCK)
            {
                throw std::ios_base::failure(
                    "thumbnail log sharing violation",
                    std::error_code(error, std::generic_category()));
            }
#endif
#ifdef EAGAIN
            if (error == EAGAIN)
            {
                throw std::ios_base::failure(
                    "thumbnail log sharing violation",
                    std::error_code(error, std::generic_category()));
            }
#endif
            // .NET treats Unix FileShare locking as best-effort and ignores
            // failures other than EWOULDBLOCK.
            return;
        }
    }

    [[nodiscard]] int OpenFile(const std::filesystem::path& path)
    {
        int flags = O_WRONLY | O_CREAT;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif

        int descriptor = -1;
        do
        {
            descriptor = ::open(path.c_str(), flags, 0666);
        }
#ifdef EINTR
        while (descriptor < 0 && errno == EINTR);
#else
        while (false);
#endif
        if (descriptor < 0)
        {
            ThrowFileFailure(errno);
        }
        return descriptor;
    }

    [[nodiscard]] bool CanSeek(int descriptor) noexcept
    {
        errno = 0;
        return ::lseek(descriptor, 0, SEEK_CUR) != static_cast<off_t>(-1);
    }

    [[nodiscard]] off_t FileLength(int descriptor)
    {
        struct stat status{};
        if (::fstat(descriptor, &status) != 0)
        {
            ThrowFileFailure(errno);
        }
        return status.st_size;
    }

    void TruncateLikeFileModeCreate(int descriptor)
    {
        if (::ftruncate(descriptor, 0) == 0)
        {
            return;
        }
        const int error = errno;
#ifdef EBADF
        if (error == EBADF)
        {
            return;
        }
#endif
#ifdef EINVAL
        if (error == EINVAL)
        {
            return;
        }
#endif
        ThrowFileFailure(error);
    }

    void WriteBytesAtOffset(int descriptor, std::string_view text,
        bool seekable, off_t fileOffset)
    {
        std::size_t textOffset = 0;
        constexpr std::size_t MaxChunk = static_cast<std::size_t>(
            std::numeric_limits<ssize_t>::max());
        while (textOffset < text.size())
        {
            const std::size_t count = std::min(MaxChunk, text.size() - textOffset);
            ssize_t written = -1;
            do
            {
                written = seekable
                    ? ::pwrite(descriptor, text.data() + textOffset, count, fileOffset)
                    : ::write(descriptor, text.data() + textOffset, count);
            }
#ifdef EINTR
            while (written < 0 && errno == EINTR);
#else
            while (false);
#endif
            if (written < 0)
            {
                ThrowFileFailure(errno);
            }
            if (written == 0)
            {
                throw std::ios_base::failure("thumbnail log write made no progress");
            }
            const std::size_t advanced = static_cast<std::size_t>(written);
            textOffset += advanced;
            if (seekable)
            {
                fileOffset += static_cast<off_t>(written);
            }
        }
    }

    void WriteBytes(const std::filesystem::path& path, std::string_view text,
        std::ios::openmode mode)
    {
        const bool append = (mode & std::ios::app) != std::ios::openmode{};
        const FileDescriptor descriptor(OpenFile(path));
        AcquireSharedFileLock(descriptor.Get());

        const bool seekable = CanSeek(descriptor.Get());
        off_t fileOffset = 0;
        if (append)
        {
            if (seekable)
            {
                fileOffset = FileLength(descriptor.Get());
            }
        }
        else
        {
            TruncateLikeFileModeCreate(descriptor.Get());
        }

        WriteBytesAtOffset(descriptor.Get(), text, seekable, fileOffset);
    }
#else
    [[noreturn]] void ThrowFileFailure(int error)
    {
        switch (error)
        {
#ifdef EACCES
        case EACCES:
#endif
#ifdef EBADF
        case EBADF:
#endif
#ifdef EPERM
        case EPERM:
#endif
#ifdef EISDIR
        case EISDIR:
#endif
#ifdef EFBIG
        case EFBIG:
#endif
#ifdef ECANCELED
        case ECANCELED:
#endif
            throw NonIoFileFailure("file access was not permitted");
        default:
            throw std::ios_base::failure(
                "thumbnail log I/O failed",
                std::error_code(error, std::generic_category()));
        }
    }

    void WriteBytes(const std::filesystem::path& path, std::string_view text,
        std::ios::openmode mode)
    {
        errno = 0;
        std::ofstream stream(path, std::ios::out | std::ios::binary | mode);
        if (!stream.is_open())
        {
            ThrowFileFailure(errno);
        }
        stream.exceptions(std::ios::badbit | std::ios::failbit);

        std::size_t offset = 0;
        constexpr std::size_t MaxChunk = static_cast<std::size_t>(
            std::numeric_limits<std::streamsize>::max());
        while (offset < text.size())
        {
            const std::size_t count = std::min(MaxChunk, text.size() - offset);
            stream.write(text.data() + offset, static_cast<std::streamsize>(count));
            offset += count;
        }
        stream.close();
    }
#endif

    void WriteAllText(const std::string& path, std::string_view text)
    {
        WriteBytes(PathFromUtf8(path), text, std::ios::trunc);
    }

    void AppendAllText(const std::string& path, std::string_view text)
    {
        WriteBytes(PathFromUtf8(path), text, std::ios::app);
    }
}

namespace MphRead::Mods
{
    std::mutex ThumbnailLog::_lock;
    std::atomic_bool ThumbnailLog::_failed{false};

    std::string ThumbnailLog::Path()
    {
        return CombinePath(Launcher::Detail::ThumbnailLogGameFilesRoot(), "thumbnails.log");
    }

    void ThumbnailLog::Begin(std::int32_t rooms) noexcept
    {
        try
        {
            // C# evaluates method arguments left-to-right, so the live Path
            // property is captured before any of the formatted contents.
            const std::string path = Path();

            std::string contents;
            contents.reserve(192);
            contents += "=== ";
            contents += Branding::Name;
            contents += " preview generation, ";
            contents += Now(true);
            contents += " ===";
            contents += NewLine();
            contents += "build ";
            contents += Update::BuildVersion::Display();
            contents += ", assembly ";
            contents += EntryAssemblyVersion();
            contents += ", file ";
            contents += BaseDirectoryLastWriteTime();
            contents += NewLine();
            contents += std::to_string(rooms);
            contents += " room(s) to render";
            contents += NewLine();

            WriteAllText(path, contents);
            _failed.store(false, std::memory_order_relaxed);
        }
        catch (...)
        {
            _failed.store(true, std::memory_order_relaxed);
        }
    }

    void ThumbnailLog::Write(const std::string& line) noexcept
    {
        if (_failed.load(std::memory_order_relaxed))
        {
            return;
        }

        try
        {
            const std::lock_guard<std::mutex> guard(_lock);
            for (std::int32_t attempt = 0; attempt < 5; ++attempt)
            {
                try
                {
                    // Match File.AppendAllText(Path, interpolatedText): Path is
                    // re-evaluated first on every attempt, then DateTime.Now.
                    const std::string path = Path();

                    std::string text;
                    text.reserve(line.size() + 13);
                    text.push_back('[');
                    text += Now(true).substr(11);
                    text += "] ";
                    text += line;
                    text += NewLine();
                    AppendAllText(path, text);
                    return;
                }
                catch (const std::ios_base::failure&)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                catch (...)
                {
                    _failed.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        }
        catch (...)
        {
            _failed.store(true, std::memory_order_relaxed);
        }
    }
}

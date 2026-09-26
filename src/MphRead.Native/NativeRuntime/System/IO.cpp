
#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "IO.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <system_error>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        [[nodiscard]] bool IsDirectorySeparator(char value) noexcept
        {
#if defined(_WIN32)
            return value == '\\' || value == '/';
#else
            return value == '/';
#endif
        }

        // Path.DirectorySeparatorChar.
#if defined(_WIN32)
        constexpr char PreferredSeparator = '\\';
#else
        constexpr char PreferredSeparator = '/';
#endif

        [[noreturn]] void ThrowFileTooLong(const std::string& fullPath)
        {
            (void)fullPath;
            throw System::IO::IOException(
                "The file is too long. This operation is currently limited to supporting files less than 2 gigabytes in size.");
        }

#if defined(_WIN32)
        [[nodiscard]] std::string SystemMessage(DWORD error)
        {
            wchar_t* buffer = nullptr;
            const DWORD length = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
            std::wstring text = length > 0 ? std::wstring(buffer, length) : std::wstring();
            if (buffer != nullptr)
            {
                ::LocalFree(buffer);
            }
            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n'))
            {
                text.pop_back();
            }
            return WideToWtf8(text);
        }

        // Win32Marshal.GetExceptionForWin32Error.
        [[noreturn]] void ThrowForWin32Error(DWORD error, const std::string& path)
        {
            switch (error)
            {
            case ERROR_FILE_NOT_FOUND:
                throw System::IO::FileNotFoundException("Could not find file '" + path + "'.");
            case ERROR_PATH_NOT_FOUND:
                throw System::IO::DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
            case ERROR_ACCESS_DENIED:
                throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
            case ERROR_ALREADY_EXISTS:
            case ERROR_FILE_EXISTS:
                throw System::IO::IOException("The file '" + path + "' already exists.");
            case ERROR_FILENAME_EXCED_RANGE:
                throw System::IO::PathTooLongException("The path '" + path
                    + "' is too long, or a component of the specified path is too long.");
            case ERROR_SHARING_VIOLATION:
                throw System::IO::IOException("The process cannot access the file '" + path
                    + "' because it is being used by another process.");
            case ERROR_OPERATION_ABORTED:
                throw System::OperationCanceledException();
            default:
                throw System::IO::IOException(SystemMessage(error) + " : '" + path + "'");
            }
        }
#else
        // Interop.GetExceptionForIoErrno.
        [[noreturn]] void ThrowForErrno(int error, const std::string& path, bool isDirError)
        {
            switch (error)
            {
            case ENOENT:
                if (isDirError)
                {
                    throw System::IO::DirectoryNotFoundException("Could not find a part of the path '" + path + "'.");
                }
                throw System::IO::FileNotFoundException("Could not find file '" + path + "'.");
            case EACCES:
            case EBADF:
            case EPERM:
                throw System::UnauthorizedAccessException("Access to the path '" + path + "' is denied.");
            case ENAMETOOLONG:
                throw System::IO::PathTooLongException("The path '" + path
                    + "' is too long, or a component of the specified path is too long.");
            case EWOULDBLOCK:
                throw System::IO::IOException("The process cannot access the file '" + path
                    + "' because it is being used by another process.");
            case ECANCELED:
                throw System::OperationCanceledException();
            case EFBIG:
                throw System::ArgumentOutOfRangeException("value");
            case EEXIST:
                throw System::IO::IOException("The file '" + path + "' already exists.");
            default:
                throw System::IO::IOException(std::string(std::strerror(error)) + " : '" + path + "'");
            }
        }

        [[nodiscard]] std::string DirectoryName(const std::string& fullPath)
        {
            const std::size_t slash = fullPath.find_last_of('/');
            return slash == std::string::npos ? std::string() : (slash == 0 ? std::string("/") : fullPath.substr(0, slash));
        }
#endif
    }

    std::size_t PathRootLength(std::string_view path) noexcept
    {
#if defined(_WIN32)
        // PathInternal.GetRootLength (Windows).
        const std::size_t pathLength = path.size();
        const auto isSeparator = [&](std::size_t i) { return IsDirectorySeparator(path[i]); };
        const bool isExtended = pathLength >= 4 && path[0] == '\\' && (path[1] == '\\' || path[1] == '?')
            && path[2] == '?' && path[3] == '\\';
        const bool deviceSyntax = isExtended || (pathLength >= 4 && isSeparator(0) && isSeparator(1)
            && (path[2] == '.' || path[2] == '?') && isSeparator(3));
        const bool deviceUnc = deviceSyntax && pathLength >= 8 && isSeparator(7)
            && path[4] == 'U' && path[5] == 'N' && path[6] == 'C';
        std::size_t i = 0;
        if ((!deviceSyntax || deviceUnc) && pathLength > 0 && isSeparator(0))
        {
            if (deviceUnc || (pathLength > 1 && isSeparator(1)))
            {
                i = deviceUnc ? 8 : 2;
                int n = 2;
                while (i < pathLength && (!isSeparator(i) || --n > 0))
                {
                    i++;
                }
            }
            else
            {
                i = 1;
            }
        }
        else if (deviceSyntax)
        {
            constexpr std::size_t devicePrefixLength = 4;
            i = devicePrefixLength;
            while (i < pathLength && !isSeparator(i))
            {
                i++;
            }
            if (i < pathLength && i > devicePrefixLength && isSeparator(i))
            {
                i++;
            }
        }
        else if (pathLength >= 2 && path[1] == ':'
            && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        {
            i = 2;
            if (pathLength > 2 && isSeparator(2))
            {
                i++;
            }
        }
        return i;
#else
        return !path.empty() && path[0] == '/' ? 1 : 0;
#endif
    }

    std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(_WIN32)
        return std::filesystem::path(Wtf8ToWide(value));
#else
        return std::filesystem::path(std::string(value));
#endif
    }

    std::string PathToUtf8(const std::filesystem::path& value)
    {
#if defined(_WIN32)
        return WideToWtf8(value.native());
#else
        return value.native();
#endif
    }

    bool PathIsPathRooted(std::string_view path) noexcept
    {
#if defined(_WIN32)
        if (!path.empty() && IsDirectorySeparator(path[0]))
        {
            return true;
        }
        const auto driveLetter = [](char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        };
        return path.size() >= 2 && driveLetter(path[0]) && path[1] == ':';
#else
        return !path.empty() && path[0] == '/';
#endif
    }

    std::string PathCombine(std::string_view first, std::string_view second)
    {
        if (first.find('\0') != std::string_view::npos || second.find('\0') != std::string_view::npos)
        {
            throw System::ArgumentException("Null character in path.");
        }
        if (second.empty())
        {
            return std::string(first);
        }
        if (first.empty() || PathIsPathRooted(second))
        {
            return std::string(second);
        }
        std::string result(first);
        if (!IsDirectorySeparator(first.back()) && !IsDirectorySeparator(second.front()))
        {
#if defined(_WIN32)
            result.push_back('\\');
#else
            result.push_back('/');
#endif
        }
        result.append(second);
        return result;
    }

    std::string PathCombine(std::string_view first, std::string_view second, std::string_view third)
    {
        return PathCombine(PathCombine(first, second), third);
    }

    std::string PathGetFileName(const std::string& path)
    {
        // Path.GetFileName: after the last separator, but never inside the root.
        const std::size_t root = PathRootLength(path);
        std::size_t i = path.size();
        while (i > 0 && !IsDirectorySeparator(path[i - 1]))
        {
            i--;
        }
        // i is one past the last separator, or 0 when there is none.
        if (i == 0)
        {
            return path.substr(0);
        }
        const std::size_t separator = i - 1;
        return path.substr(separator < root ? root : separator + 1);
    }

    std::string PathGetFullPath(const std::string& path)
    {
        if (path.empty())
        {
            throw System::ArgumentException("The value cannot be an empty string. (Parameter 'path')");
        }
#if defined(_WIN32)
        const std::wstring wide = Wtf8ToWide(path);
        const DWORD length = ::GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
        if (length == 0)
        {
            ThrowForWin32Error(::GetLastError(), path);
        }
        std::wstring result(length, L'\0');
        const DWORD written = ::GetFullPathNameW(wide.c_str(), length, result.data(), nullptr);
        result.resize(written);
        return WideToWtf8(result);
#else
        std::filesystem::path full = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal();
        std::string text = full.string();
        if (text.size() > 1 && text.back() == '/' && (path.empty() || path.back() != '/'))
        {
            text.pop_back();
        }
        return text;
#endif
    }

    std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        if (path.empty())
        {
            throw System::ArgumentException("The value cannot be an empty string. (Parameter 'path')");
        }
        const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
        HANDLE file = ::CreateFileW(Wtf8ToWide(fullPath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            ThrowForWin32Error(::GetLastError(), fullPath);
        }
        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(file, &size))
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(file);
            ThrowForWin32Error(error, fullPath);
        }
        if (size.QuadPart > std::numeric_limits<std::int32_t>::max())
        {
            ::CloseHandle(file);
            ThrowFileTooLong(fullPath);
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            DWORD read = 0;
            const DWORD wanted = static_cast<DWORD>(bytes.size() - offset);
            if (!::ReadFile(file, bytes.data() + offset, wanted, &read, nullptr))
            {
                const DWORD error = ::GetLastError();
                ::CloseHandle(file);
                ThrowForWin32Error(error, fullPath);
            }
            if (read == 0)
            {
                ::CloseHandle(file);
                throw System::IO::EndOfStreamException();
            }
            offset += read;
        }
        ::CloseHandle(file);
        return bytes;
#else
        const int fd = ::open(fullPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
        {
            const int error = errno;
            ThrowForErrno(error, fullPath, error == ENOENT && !DirectoryExists(DirectoryName(fullPath)));
        }
        struct stat info{};
        if (::fstat(fd, &info) != 0)
        {
            const int error = errno;
            ::close(fd);
            ThrowForErrno(error, fullPath, false);
        }
        if (S_ISDIR(info.st_mode))
        {
            ::close(fd);
            ThrowForErrno(EACCES, fullPath, false);
        }
        if (info.st_size > std::numeric_limits<std::int32_t>::max())
        {
            ::close(fd);
            ThrowFileTooLong(fullPath);
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.st_size));
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const ssize_t read = ::read(fd, bytes.data() + offset, bytes.size() - offset);
            if (read < 0)
            {
                const int error = errno;
                if (error == EINTR)
                {
                    continue;
                }
                ::close(fd);
                ThrowForErrno(error, fullPath, false);
            }
            if (read == 0)
            {
                ::close(fd);
                throw System::IO::EndOfStreamException();
            }
            offset += static_cast<std::size_t>(read);
        }
        ::close(fd);
        return bytes;
#endif
    }

    void DirectoryEnumerateFiles(
        const std::string& path,
        const std::function<void(const std::string&)>& visitor)
    {
        if (path.empty())
        {
            throw System::ArgumentException("The value cannot be an empty string. (Parameter 'path')");
        }
        const std::string fullPath = PathGetFullPath(path);
        // Path.Join(path, name): the path as given, a separator only when it has none.
        const auto join = [&path](const std::string& name)
        {
            if (IsDirectorySeparator(path.back()))
            {
                return path + name;
            }
#if defined(_WIN32)
            return path + "\\" + name;
#else
            return path + "/" + name;
#endif
        };
#if defined(_WIN32)
        WIN32_FIND_DATAW data{};
        HANDLE find = ::FindFirstFileExW(Wtf8ToWide(fullPath + "\\*").c_str(), FindExInfoBasic, &data,
            FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (find == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            // No match in an existing directory (not even "." and "..") means no files.
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES)
            {
                return;
            }
            ThrowForWin32Error(error, fullPath);
        }
        std::vector<std::string> names;
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                names.push_back(WideToWtf8(data.cFileName));
            }
        }
        while (::FindNextFileW(find, &data));
        ::FindClose(find);
        for (const std::string& name : names)
        {
            visitor(join(name));
        }
#else
        DIR* directory = ::opendir(fullPath.c_str());
        if (directory == nullptr)
        {
            ThrowForErrno(errno, fullPath, true);
        }
        std::vector<std::string> names;
        while (dirent* entry = ::readdir(directory))
        {
            const std::string name = entry->d_name;
            if (name == "." || name == "..")
            {
                continue;
            }
            bool isDirectory = false;
            if (entry->d_type == DT_DIR)
            {
                isDirectory = true;
            }
            else if (entry->d_type == DT_LNK || entry->d_type == DT_UNKNOWN)
            {
                struct stat info{};
                isDirectory = ::stat((fullPath + "/" + name).c_str(), &info) == 0 && S_ISDIR(info.st_mode);
            }
            if (!isDirectory)
            {
                names.push_back(name);
            }
        }
        ::closedir(directory);
        for (const std::string& name : names)
        {
            visitor(join(name));
        }
#endif
    }

#if defined(_WIN32)
    namespace
    {
        // FileSystem.FillAttributeInfo: GetFileAttributesEx, and where that
        // fails for a reason other than the entry not being there -- a file
        // another process holds open with no sharing, such as the page file --
        // FindFirstFile, which still reports it.
        [[nodiscard]] std::optional<DWORD> ReadAttributes(const std::wstring& path) noexcept
        {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            {
                return data.dwFileAttributes;
            }
            switch (::GetLastError())
            {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_NOT_READY:
            case ERROR_INVALID_NAME:
            case ERROR_BAD_PATHNAME:
            case ERROR_BAD_NETPATH:
            case ERROR_BAD_NET_NAME:
            case ERROR_INVALID_PARAMETER:
            case ERROR_NETWORK_UNREACHABLE:
            case ERROR_NETWORK_ACCESS_DENIED:
            case ERROR_INVALID_HANDLE:
            case ERROR_FILENAME_EXCED_RANGE:
                return std::nullopt;
            default:
                break;
            }
            WIN32_FIND_DATAW find{};
            const HANDLE handle = ::FindFirstFileW(path.c_str(), &find);
            if (handle == INVALID_HANDLE_VALUE)
            {
                return std::nullopt;
            }
            (void)::FindClose(handle);
            return find.dwFileAttributes;
        }
    }
#endif

    bool FileExists(std::string_view path) noexcept
    {
        // File.Exists: false for an empty path, one with a NUL in it, one
        // ending in a separator, a directory, and anything that cannot be
        // looked at. On Unix a dangling symbolic link is an existing file.
        if (path.empty() || path.find('\0') != std::string_view::npos
            || IsDirectorySeparator(path.back()))
        {
            return false;
        }
        try
        {
            const std::string value(path);
#if defined(_WIN32)
            const std::optional<DWORD> attributes = ReadAttributes(Wtf8ToWide(value));
            return attributes.has_value() && (*attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
            struct stat info{};
            if (::stat(value.c_str(), &info) != 0)
            {
                return ::lstat(value.c_str(), &info) == 0;
            }
            return !S_ISDIR(info.st_mode);
#endif
        }
        catch (...)
        {
            return false;
        }
    }

    bool DirectoryExists(std::string_view path) noexcept
    {
        if (path.empty() || path.find('\0') != std::string_view::npos)
        {
            return false;
        }
        try
        {
            const std::string value(path);
#if defined(_WIN32)
            const std::optional<DWORD> attributes = ReadAttributes(Wtf8ToWide(value));
            return attributes.has_value() && (*attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
            struct stat info{};
            if (::stat(value.c_str(), &info) != 0)
            {
                return false;
            }
            return S_ISDIR(info.st_mode);
#endif
        }
        catch (...)
        {
            return false;
        }
    }

    std::vector<char> PathGetInvalidFileNameChars()
    {
#if defined(_WIN32)
        // Path.Windows: the control characters plus the ones the file system
        // reserves.
        std::vector<char> result;
        for (char value = 0; value < 32; ++value)
        {
            result.push_back(value);
        }
        for (const char value : {'"', '<', '>', '|', ':', '*', '?', '\\', '/'})
        {
            result.push_back(value);
        }
        return result;
#else
        // Path.Unix: the separator and the terminator, and nothing else.
        return std::vector<char>{'\0', '/'};
#endif
    }

    std::vector<std::string> FileReadAllLines(const std::string& path)
    {
        const std::string text = StreamReaderDecode(FileReadAllBytes(path));
        std::size_t index = 0;
        std::vector<std::string> lines;
        std::string current;
        while (index < text.size())
        {
            const char value = text[index];
            if (value == '\r')
            {
                // CR, LF and CRLF each end exactly one line.
                if (index + 1 < text.size() && text[index + 1] == '\n')
                {
                    ++index;
                }
                lines.push_back(current);
                current.clear();
            }
            else if (value == '\n')
            {
                lines.push_back(current);
                current.clear();
            }
            else
            {
                current += value;
            }
            ++index;
        }
        if (!current.empty())
        {
            // A final line with no terminator is still a line; a file ending
            // in one does not add an empty entry.
            lines.push_back(current);
        }
        return lines;
    }

    namespace
    {
        void WriteAllBytes(const std::string& path, std::string_view text)
        {
            const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
            HANDLE file = ::CreateFileW(Wtf8ToWide(fullPath).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                ThrowForWin32Error(::GetLastError(), fullPath);
            }
            std::size_t offset = 0;
            while (offset < text.size())
            {
                DWORD written = 0;
                const DWORD wanted = static_cast<DWORD>(text.size() - offset);
                if (!::WriteFile(file, text.data() + offset, wanted, &written, nullptr))
                {
                    const DWORD error = ::GetLastError();
                    ::CloseHandle(file);
                    ThrowForWin32Error(error, fullPath);
                }
                offset += written;
            }
            ::CloseHandle(file);
#else
            const int fd = ::open(
                fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
            if (fd < 0)
            {
                const int error = errno;
                ThrowForErrno(
                    error, fullPath,
                    error == ENOENT && !DirectoryExists(DirectoryName(fullPath)));
            }
            std::size_t offset = 0;
            while (offset < text.size())
            {
                const ssize_t written = ::write(fd, text.data() + offset, text.size() - offset);
                if (written < 0)
                {
                    const int error = errno;
                    ::close(fd);
                    ThrowForErrno(error, fullPath, false);
                }
                offset += static_cast<std::size_t>(written);
            }
            ::close(fd);
#endif
        }
    }

    void FileWriteAllLines(const std::string& path, const std::vector<std::string>& lines)
    {
        std::string text;
        for (const std::string& line : lines)
        {
            text += line;
#if defined(_WIN32)
            text += "\r\n";
#else
            text += "\n";
#endif
        }
        WriteAllBytes(path, text);
    }

    std::string FileReadAllText(const std::string& path)
    {
        return StreamReaderDecode(FileReadAllBytes(path));
    }

    void FileWriteAllText(const std::string& path, std::string_view text)
    {
        WriteAllBytes(path, text);
    }

    void FileWriteAllBytes(const std::string& path, std::span<const std::uint8_t> bytes)
    {
        WriteAllBytes(path, std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }

    void DirectoryCreateDirectory(const std::string& path)
    {
        if (path.empty())
        {
            throw System::ArgumentException(
                "The value cannot be an empty string. (Parameter 'path')");
        }
        const std::string fullPath = PathGetFullPath(path);
        // Directory.CreateDirectory walks up to the first parent that exists
        // and creates everything below it; an existing directory is a no-op.
        const std::size_t rootLength = PathRootLength(fullPath);
        std::vector<std::string> pending;
        std::string current = fullPath;
        while (current.size() > rootLength
            && !::MphRead::NativeRuntime::DirectoryExists(current))
        {
            pending.push_back(current);
            const std::size_t slash = current.find_last_of("/\\");
            if (slash == std::string::npos || slash < rootLength)
            {
                break;
            }
            current = current.substr(0, slash);
        }
        for (std::size_t i = pending.size(); i-- > 0;)
        {
#if defined(_WIN32)
            if (::CreateDirectoryW(Wtf8ToWide(pending[i]).c_str(), nullptr) == FALSE)
            {
                const DWORD error = ::GetLastError();
                if (error != ERROR_ALREADY_EXISTS)
                {
                    ThrowForWin32Error(error, pending[i]);
                }
            }
#else
            if (::mkdir(pending[i].c_str(), 0777) != 0 && errno != EEXIST)
            {
                ThrowForErrno(errno, pending[i], false);
            }
#endif
        }
    }

    std::int64_t FileInfoLength(const std::string& path)
    {
        const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
        const std::optional<DWORD> attributes = ReadAttributes(Wtf8ToWide(fullPath));
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!attributes.has_value() || (*attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
            || GetFileAttributesExW(Wtf8ToWide(fullPath).c_str(), GetFileExInfoStandard, &data) == FALSE)
        {
            throw System::IO::FileNotFoundException("Could not find file '" + fullPath + "'.");
        }
        return (static_cast<std::int64_t>(data.nFileSizeHigh) << 32)
            | static_cast<std::int64_t>(data.nFileSizeLow);
#else
        struct stat status{};
        if (::stat(fullPath.c_str(), &status) != 0 || S_ISDIR(status.st_mode))
        {
            throw System::IO::FileNotFoundException("Could not find file '" + fullPath + "'.");
        }
        return static_cast<std::int64_t>(status.st_size);
#endif
    }

    FileInfo CreateFileInfo(const std::string& path)
    {
        FileInfo info;
        info.Name = PathGetFileName(path);
        const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (GetFileAttributesExW(
                Wtf8ToWide(fullPath).c_str(), GetFileExInfoStandard, &data) != FALSE)
        {
            info.Length = (static_cast<std::int64_t>(data.nFileSizeHigh) << 32)
                | static_cast<std::int64_t>(data.nFileSizeLow);
            // FILETIME counts the same 100-nanosecond tick from 1601-01-01.
            const std::int64_t fileTime
                = (static_cast<std::int64_t>(data.ftLastWriteTime.dwHighDateTime) << 32)
                | static_cast<std::int64_t>(data.ftLastWriteTime.dwLowDateTime);
            info.LastWriteTimeTicks = fileTime + 504911232000000000LL;
        }
#else
        struct stat status{};
        if (::stat(fullPath.c_str(), &status) == 0)
        {
            info.Length = static_cast<std::int64_t>(status.st_size);
            info.LastWriteTimeTicks = 621355968000000000LL
                + static_cast<std::int64_t>(status.st_mtime) * 10000000LL;
        }
#endif
        return info;
    }

    std::vector<std::string> DirectoryEnumerateFilesWithSuffix(
        const std::string& path, const std::string& suffix)
    {
        std::vector<std::string> result;
        DirectoryEnumerateFiles(path, [&](const std::string& file)
        {
            const std::string name = PathGetFileName(file);
            if (suffix.empty()
                || (name.size() >= suffix.size()
                    && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0))
            {
                result.push_back(file);
            }
        });
        return result;
    }

    std::string PathGetFileNameWithoutExtension(const std::string& path)
    {
        std::string name = PathGetFileName(path);
        const std::size_t dot = name.find_last_of('.');
        if (dot != std::string::npos)
        {
            name.erase(dot);
        }
        return name;
    }

    namespace
    {
        // Path.IsPathRooted(path).
        [[nodiscard]] bool IsPathRooted(std::string_view path) noexcept
        {
            return PathRootLength(path) > 0;
        }

        // Path.JoinInternal: one separator between the parts, and only where
        // the left part does not already end in one.
        void AppendJoined(std::string& result, std::string_view part)
        {
            if (!result.empty() && !IsDirectorySeparator(result.back())
                && !(!part.empty() && IsDirectorySeparator(part.front())))
            {
                result.push_back(PreferredSeparator);
            }
            result.append(part);
        }
    }

    std::string PathCombine(std::string_view path1, std::string_view path2,
        std::string_view path3, std::string_view path4)
    {
        if (path1.empty())
        {
            return PathCombine(path2, path3, path4);
        }
        if (path2.empty())
        {
            return PathCombine(path1, path3, path4);
        }
        if (path3.empty())
        {
            return PathCombine(path1, path2, path4);
        }
        if (path4.empty())
        {
            return PathCombine(path1, path2, path3);
        }
        if (IsPathRooted(path4))
        {
            return std::string(path4);
        }
        if (IsPathRooted(path3))
        {
            return PathCombine(path3, path4);
        }
        if (IsPathRooted(path2))
        {
            return PathCombine(path2, path3, path4);
        }
        std::string result(path1);
        AppendJoined(result, path2);
        AppendJoined(result, path3);
        AppendJoined(result, path4);
        return result;
    }

    std::string PathCombine(std::span<const std::string> paths)
    {
        // Path.Combine(params string[]): the last rooted part starts the
        // result, empty parts are skipped, and a separator is added only when
        // the result so far does not already end in one.
        std::size_t first = 0;
        for (std::size_t index = 0; index < paths.size(); ++index)
        {
            if (!paths[index].empty() && IsPathRooted(paths[index]))
            {
                first = index;
            }
        }
        std::string result;
        for (std::size_t index = first; index < paths.size(); ++index)
        {
            const std::string& path = paths[index];
            if (path.empty())
            {
                continue;
            }
            if (!result.empty() && !IsDirectorySeparator(result.back()))
            {
                result.push_back(PreferredSeparator);
            }
            result += path;
        }
        return result;
    }

    std::string PathGetExtension(std::string_view path)
    {
        for (std::size_t i = path.size(); i > 0; --i)
        {
            const char value = path[i - 1];
            if (value == '.')
            {
                // ".ext" only when something follows the dot.
                return i == path.size() ? std::string() : std::string(path.substr(i - 1));
            }
            if (IsDirectorySeparator(value))
            {
                break;
            }
        }
        return std::string();
    }

    std::optional<std::string> PathGetDirectoryName(std::string_view path)
    {
        // Path.GetDirectoryName: null for an empty path and for a root, and
        // otherwise everything before the last separator with any run of
        // separators there trimmed off.
        const std::size_t root = PathRootLength(path);
        if (path.size() <= root)
        {
            return std::nullopt;
        }
        std::size_t end = path.size();
        while (end > root && !IsDirectorySeparator(path[end - 1]))
        {
            --end;
        }
        while (end > root && IsDirectorySeparator(path[end - 1]))
        {
            --end;
        }
        const std::string_view result = path.substr(0, end);
#if defined(_WIN32)
        // Windows also hands the result back with every '/' made a '\\' and
        // every run of separators after the first character collapsed to one
        // (PathInternal.NormalizeDirectorySeparators).
        std::string normalized;
        normalized.reserve(result.size());
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            if (IsDirectorySeparator(result[i]))
            {
                if (i > 0 && i + 1 < result.size() && IsDirectorySeparator(result[i + 1]))
                {
                    continue;
                }
                normalized.push_back('\\');
            }
            else
            {
                normalized.push_back(result[i]);
            }
        }
        return normalized;
#else
        return std::string(result);
#endif
    }

    std::string PathGetTempPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(MAX_PATH + 1);
        for (;;)
        {
            const DWORD length = ::GetTempPathW(
                static_cast<DWORD>(buffer.size()), buffer.data());
            if (length == 0)
            {
                return std::string();
            }
            if (length < buffer.size())
            {
                return WideToWtf8(std::wstring(buffer.data(), length));
            }
            buffer.resize(length + 1);
        }
#else
        // Path.GetTempPath on Unix: TMPDIR, then /tmp.
        const char* value = std::getenv("TMPDIR");
        std::string result = value == nullptr || *value == ' ' ? std::string("/tmp") : std::string(value);
        if (result.empty() || !IsDirectorySeparator(result.back()))
        {
            result.push_back('/');
        }
        return result;
#endif
    }

    void FileAppendAllText(const std::string& path, std::string_view contents)
    {
        std::ofstream file(std::filesystem::path(std::u8string(path.begin(), path.end())),
            std::ios::binary | std::ios::app);
        if (!file)
        {
            throw System::IO::IOException("Could not open '" + path + "' for append.");
        }
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!file)
        {
            throw System::IO::IOException("Could not write to '" + path + "'.");
        }
    }

    std::int64_t FileOpenExclusiveLength(const std::string& path)
    {
        const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
        const HANDLE handle = ::CreateFileW(Wtf8ToWide(fullPath).c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            ThrowForWin32Error(::GetLastError(), fullPath);
        }
        LARGE_INTEGER size{};
        const BOOL ok = ::GetFileSizeEx(handle, &size);
        const DWORD error = ok == FALSE ? ::GetLastError() : 0;
        ::CloseHandle(handle);
        if (ok == FALSE)
        {
            ThrowForWin32Error(error, fullPath);
        }
        return size.QuadPart;
#else
        // FileShare.None on Unix is an advisory flock(LOCK_EX | LOCK_NB).
        const int fd = ::open(fullPath.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            ThrowForErrno(errno, fullPath, false);
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
        {
            const int error = errno;
            ::close(fd);
            ThrowForErrno(error, fullPath, false);
        }
        struct stat info{};
        const int result = ::fstat(fd, &info);
        const int error = errno;
        ::close(fd);
        if (result != 0)
        {
            ThrowForErrno(error, fullPath, false);
        }
        return static_cast<std::int64_t>(info.st_size);
#endif
    }

    std::string PathGetFullPath(const std::string& path, const std::string& basePath)
    {
        // Path.Combine keeps a rooted path as it is.
        return PathGetFullPath(PathCombine(basePath, path));
    }

    void FileMove(const std::string& source, const std::string& destination, bool overwrite)
    {
        const std::string from = PathGetFullPath(source);
        const std::string to = PathGetFullPath(destination);
#if defined(_WIN32)
        if (::MoveFileExW(Wtf8ToWide(from).c_str(), Wtf8ToWide(to).c_str(),
            MOVEFILE_COPY_ALLOWED | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0)) == FALSE)
        {
            ThrowForWin32Error(::GetLastError(), from);
        }
#else
        if (!overwrite && ::access(to.c_str(), F_OK) == 0)
        {
            ThrowForErrno(EEXIST, to, false);
        }
        if (::rename(from.c_str(), to.c_str()) != 0)
        {
            ThrowForErrno(errno, from, false);
        }
#endif
    }

    void FileDelete(const std::string& path)
    {
        const std::string fullPath = PathGetFullPath(path);
#if defined(_WIN32)
        if (::DeleteFileW(Wtf8ToWide(fullPath).c_str()) == FALSE)
        {
            const DWORD error = ::GetLastError();
            // A file that is not there is not an error; a directory that is
            // not there is.
            if (error != ERROR_FILE_NOT_FOUND)
            {
                ThrowForWin32Error(error, fullPath);
            }
        }
#else
        if (::unlink(fullPath.c_str()) != 0)
        {
            const int error = errno;
            if (error == ENOENT)
            {
                // FileSystem.Unix matches Windows: a missing parent directory
                // is DirectoryNotFoundException, a missing file is nothing.
                const std::optional<std::string> directory = PathGetDirectoryName(fullPath);
                if (directory.has_value() && !directory->empty() && !DirectoryExists(*directory))
                {
                    ThrowForErrno(error, fullPath, true);
                }
                return;
            }
            if (error == EROFS)
            {
                // A read-only file system is only an error when the file is there.
                struct stat info{};
                if (::lstat(fullPath.c_str(), &info) != 0 && errno == ENOENT)
                {
                    return;
                }
            }
            if (error == EISDIR)
            {
                throw System::UnauthorizedAccessException("Access to the path '" + fullPath + "' is denied.");
            }
            ThrowForErrno(error, fullPath, false);
        }
#endif
    }

    DirectoryInfo::DirectoryInfo(std::string_view path)
    {
        _fullName = PathGetFullPath(std::string(path));
        // GetFullPath keeps a trailing separator; .NET's DirectoryInfo.Name
        // and Parent both look past one, so it is dropped here -- except where
        // the whole path is the root, which has nothing else in it.
        const std::size_t root = PathRootLength(_fullName);
        while (_fullName.size() > root && IsDirectorySeparator(_fullName.back()))
        {
            _fullName.pop_back();
        }
        std::size_t start = _fullName.size();
        while (start > root && !IsDirectorySeparator(_fullName[start - 1]))
        {
            --start;
        }
        _name = _fullName.substr(start);
    }

    const std::string& DirectoryInfo::Name() const noexcept
    {
        return _name;
    }

    const std::string& DirectoryInfo::FullName() const noexcept
    {
        return _fullName;
    }

    std::string DirectoryInfo::Extension() const
    {
        return PathGetExtension(_name);
    }

    std::shared_ptr<DirectoryInfo> DirectoryInfo::Parent() const
    {
        const std::size_t root = PathRootLength(_fullName);
        if (_fullName.size() <= root)
        {
            return nullptr;
        }
        std::size_t end = _fullName.size();
        while (end > root && !IsDirectorySeparator(_fullName[end - 1]))
        {
            --end;
        }
        while (end > root && IsDirectorySeparator(_fullName[end - 1]))
        {
            --end;
        }
        if (end == 0)
        {
            return nullptr;
        }
        return std::make_shared<DirectoryInfo>(_fullName.substr(0, end == root ? root : end));
    }

#if defined(_WIN32)
    void ThrowForLastIOError(unsigned long win32Error, const std::string& path)
    {
        ThrowForWin32Error(static_cast<DWORD>(win32Error), path);
    }
#else
    void ThrowForLastIOError(int error, const std::string& path, bool isDirectory)
    {
        ThrowForErrno(error, path, isDirectory);
    }
#endif
}

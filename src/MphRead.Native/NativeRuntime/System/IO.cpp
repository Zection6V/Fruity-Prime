#include "IO.hpp"

#include "Exceptions.hpp"

#include <limits>
#include <string>

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

        [[noreturn]] void ThrowFileTooLong(const std::string& fullPath)
        {
            (void)fullPath;
            throw System::IO::IOException(
                "The file is too long. This operation is currently limited to supporting files less than 2 gigabytes in size.");
        }

#if defined(_WIN32)
        [[nodiscard]] std::wstring Widen(const std::string& value)
        {
            if (value.empty())
            {
                return {};
            }
            const int length = ::MultiByteToWideChar(CP_UTF8, 0, value.data(),
                static_cast<int>(value.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                result.data(), length);
            return result;
        }

        [[nodiscard]] std::string Narrow(const std::wstring& value)
        {
            if (value.empty())
            {
                return {};
            }
            const int length = ::WideCharToMultiByte(CP_UTF8, 0, value.data(),
                static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            std::string result(static_cast<std::size_t>(length), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                result.data(), length, nullptr, nullptr);
            return result;
        }

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
            return Narrow(text);
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

        [[nodiscard]] bool DirectoryExists(const std::string& path)
        {
            struct stat info{};
            return ::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
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
        const std::wstring wide = Widen(path);
        const DWORD length = ::GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
        if (length == 0)
        {
            ThrowForWin32Error(::GetLastError(), path);
        }
        std::wstring result(length, L'\0');
        const DWORD written = ::GetFullPathNameW(wide.c_str(), length, result.data(), nullptr);
        result.resize(written);
        return Narrow(result);
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
        HANDLE file = ::CreateFileW(Widen(fullPath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
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
        HANDLE find = ::FindFirstFileExW(Widen(fullPath + "\\*").c_str(), FindExInfoBasic, &data,
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
                names.push_back(Narrow(data.cFileName));
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

    bool FileExists(const std::string& path) noexcept
    {
        if (path.empty())
        {
            return false;
        }
#if defined(_WIN32)
        const std::wstring wide = Utf8ToUtf16(path);
        const DWORD attributes = GetFileAttributesW(wide.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        struct stat info{};
        if (::stat(path.c_str(), &info) != 0)
        {
            return false;
        }
        return !S_ISDIR(info.st_mode);
#endif
    }

    bool DirectoryExists(const std::string& path) noexcept
    {
        if (path.empty())
        {
            return false;
        }
#if defined(_WIN32)
        const std::wstring wide = Utf8ToUtf16(path);
        const DWORD attributes = GetFileAttributesW(wide.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
        struct stat info{};
        if (::stat(path.c_str(), &info) != 0)
        {
            return false;
        }
        return S_ISDIR(info.st_mode);
#endif
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
        for (const char value : {'"', '<', '>', '|', ':', '*', '?', '\', '/'})
        {
            result.push_back(value);
        }
        return result;
#else
        // Path.Unix: the separator and the terminator, and nothing else.
        return std::vector<char>{'\0', '/'};
#endif
    }

}

#include "ServerUpdate.hpp"
#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include "BuildVersion.hpp"
#include "DesktopUpdate.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdio.h>
#endif
#endif

namespace MphRead::Mods::Update::Detail
{
    // Keep ServerUpdate dependent on Updater through this narrow seam.
    // Updater.cpp owns Disabled/Check/Describe and supplies these definitions;
    // substituting UpdateCheck here would bypass Updater's observable
    // Available/Checked state.
    [[nodiscard]] bool ServerUpdateUpdaterDisabled();
    [[nodiscard]] std::optional<UpdateInfo> ServerUpdateUpdaterCheck();
    [[nodiscard]] std::string ServerUpdateUpdaterDescribe(UpdateInfo update);
}

namespace MphRead::Mods::Update
{
    namespace
    {
        using FileSystemPath = std::filesystem::path;
        using TimeSpan = ServerUpdate::TimeSpan;

        constexpr std::int64_t TicksPerSecond = 10'000'000;
        constexpr std::int64_t UnixEpochTicks = 621'355'968'000'000'000LL;
        constexpr std::int64_t MaxDateTimeTicks = 3'155'378'975'999'999'999LL;
        constexpr std::int64_t DefaultIntervalTicks = 10LL * 60LL * TicksPerSecond;

        struct State final
        {
            std::atomic_bool Enabled{true};
            std::atomic<std::int64_t> IntervalTicks{DefaultIntervalTicks};
            std::mutex Gate;
            ::MphRead::NativeRuntime::AtomicSharedPtr<const std::vector<std::string>> Relaunch{
                std::make_shared<const std::vector<std::string>>()};
            std::atomic<std::int64_t> NextCheckTicks{0}; // DateTime.MinValue
            bool Working = false;
            std::mutex PendingGate;
            std::optional<UpdateInfo> Pending;
            std::atomic_bool Staged{false};
        };

        State& GetState()
        {
            // Task.Run work can still be alive while process teardown begins.
            // Managed static state has process lifetime, so keep the native
            // equivalent alive until the OS tears the process down as well.
            static State* state = new State();
            return *state;
        }

#ifdef _WIN32
        std::wstring Utf8ToWide(std::string_view value)
        {
            if (value.empty())
            {
                return {};
            }
            const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (needed == 0)
            {
                throw std::system_error(static_cast<int>(::GetLastError()),
                    std::system_category());
            }
            std::wstring result(static_cast<std::size_t>(needed), L'\0');
            if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), result.data(), needed) == 0)
            {
                throw std::system_error(static_cast<int>(::GetLastError()),
                    std::system_category());
            }
            return result;
        }

        std::string WideToUtf8(std::wstring_view value)
        {
            if (value.empty())
            {
                return {};
            }
            const int needed = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), nullptr, 0,
                nullptr, nullptr);
            if (needed == 0)
            {
                throw std::system_error(static_cast<int>(::GetLastError()),
                    std::system_category());
            }
            std::string result(static_cast<std::size_t>(needed), '\0');
            if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                value.data(), static_cast<int>(value.size()), result.data(), needed,
                nullptr, nullptr) == 0)
            {
                throw std::system_error(static_cast<int>(::GetLastError()),
                    std::system_category());
            }
            return result;
        }

        FileSystemPath PathFromUtf8(std::string_view value)
        {
            return FileSystemPath(Utf8ToWide(value));
        }
#else
        FileSystemPath PathFromUtf8(std::string_view value)
        {
            return FileSystemPath(std::string(value));
        }
#endif

        std::string PathToUtf8(const FileSystemPath& path)
        {
#ifdef _WIN32
            return WideToUtf8(path.native());
#else
            return path.native();
#endif
        }

        FileSystemPath ReadProcessPath()
        {
#ifdef _WIN32
            std::vector<wchar_t> buffer(512);
            for (;;)
            {
                const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (length == 0)
                {
                    throw std::system_error(static_cast<int>(::GetLastError()),
                        std::system_category());
                }
                if (length < buffer.size() - 1)
                {
                    return FileSystemPath(std::wstring(buffer.data(), length));
                }
                buffer.resize(buffer.size() * 2);
            }
#elif defined(__APPLE__)
            std::uint32_t size = 0;
            (void)::_NSGetExecutablePath(nullptr, &size);
            std::vector<char> buffer(static_cast<std::size_t>(size) + 1U, '\0');
            if (::_NSGetExecutablePath(buffer.data(), &size) != 0)
            {
                throw std::runtime_error("Could not determine the process path.");
            }
            char resolved[PATH_MAX]{};
            if (::realpath(buffer.data(), resolved) != nullptr)
            {
                return FileSystemPath(resolved);
            }
            return FileSystemPath(buffer.data());
#elif defined(__linux__) || defined(__ANDROID__)
            std::vector<char> buffer(512);
            for (;;)
            {
                const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
                if (length < 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
                if (static_cast<std::size_t>(length) < buffer.size())
                {
                    return FileSystemPath(std::string(buffer.data(),
                        static_cast<std::size_t>(length)));
                }
                buffer.resize(buffer.size() * 2);
            }
#else
#error Unsupported platform for AppContext.BaseDirectory equivalence.
#endif
        }

        const FileSystemPath& BaseDirectoryPath()
        {
            static const FileSystemPath value = ReadProcessPath().parent_path();
            return value;
        }

        std::string BaseDirectoryText()
        {
            std::string value = PathToUtf8(BaseDirectoryPath());
#ifdef _WIN32
            constexpr char separator = '\\';
#else
            constexpr char separator = '/';
#endif
            if (value.empty() || (value.back() != '/' && value.back() != '\\'))
            {
                value.push_back(separator);
            }
            return value;
        }

        std::int64_t AddDateTime(std::int64_t dateTimeTicks, std::int64_t spanTicks)
        {
            if (spanTicks > 0)
            {
                if (dateTimeTicks > MaxDateTimeTicks - spanTicks)
                {
                    throw std::out_of_range(
                        "The added or subtracted value results in an un-representable DateTime.");
                }
            }
            else if (spanTicks < 0)
            {
                if (spanTicks == std::numeric_limits<std::int64_t>::min()
                    || dateTimeTicks < -spanTicks)
                {
                    throw std::out_of_range(
                        "The added or subtracted value results in an un-representable DateTime.");
                }
            }
            return dateTimeTicks + spanTicks;
        }

        std::int64_t UtcNowTicks()
        {
            const auto sinceUnix = std::chrono::system_clock::now().time_since_epoch();
            const std::int64_t ticksSinceUnix =
                std::chrono::duration_cast<TimeSpan>(sinceUnix).count();
            return AddDateTime(UnixEpochTicks, ticksSinceUnix);
        }

        std::string OptionalText(const std::optional<std::string>& value)
        {
            return value.value_or(std::string{});
        }

        void WriteLine(std::string line)
        {
            line.push_back('\n');
            std::cout << line;
            std::cout.flush();
        }

        bool DirectoryExists(const FileSystemPath& path) noexcept
        {
            std::error_code error;
            const bool result = std::filesystem::is_directory(path, error);
            return !error && result;
        }

        bool FileExists(const FileSystemPath& path) noexcept
        {
#ifdef _WIN32
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            {
                return (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
            }

            const DWORD error = ::GetLastError();
            switch (error)
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
                return false;
            default:
                break;
            }

            WIN32_FIND_DATAW findData{};
            HANDLE handle = ::FindFirstFileW(path.c_str(), &findData);
            if (handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            (void)::FindClose(handle);
            return (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
            struct stat info{};
            if (::lstat(path.c_str(), &info) != 0)
            {
                return false;
            }
            if (S_ISLNK(info.st_mode) && ::stat(path.c_str(), &info) != 0)
            {
                // File.Exists reports a dangling symbolic link as an existing
                // non-directory entry.
                return true;
            }
            return !S_ISDIR(info.st_mode);
#endif
        }

        void DeleteFile(const FileSystemPath& path)
        {
#ifdef _WIN32
            if (::DeleteFileW(path.c_str()))
            {
                return;
            }
            const DWORD error = ::GetLastError();
            if (error == ERROR_FILE_NOT_FOUND)
            {
                return;
            }
            throw std::filesystem::filesystem_error("File.Delete", path,
                std::error_code(static_cast<int>(error), std::system_category()));
#else
            if (::unlink(path.c_str()) == 0)
            {
                return;
            }
            const int error = errno;
            if (error == ENOENT)
            {
                const FileSystemPath parent = path.parent_path();
                if (parent.empty() || DirectoryExists(parent))
                {
                    return;
                }
            }
            else if (error == EROFS)
            {
                struct stat info{};
                if (::lstat(path.c_str(), &info) != 0 && errno == ENOENT)
                {
                    return;
                }
            }

            const int reported = error == EISDIR ? EACCES : error;
            throw std::filesystem::filesystem_error("File.Delete", path,
                std::error_code(reported, std::generic_category()));
#endif
        }

        void MoveNoReplace(const FileSystemPath& source, const FileSystemPath& destination)
        {
#ifdef _WIN32
            if (::MoveFileW(source.c_str(), destination.c_str()))
            {
                return;
            }
            throw std::filesystem::filesystem_error("File.Move", source, destination,
                std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
#elif defined(__linux__) && defined(SYS_renameat2)
            constexpr unsigned int RenameNoReplace = 1U;
            if (::syscall(SYS_renameat2, AT_FDCWD, source.c_str(),
                    AT_FDCWD, destination.c_str(), RenameNoReplace) == 0)
            {
                return;
            }
            if (errno != ENOSYS && errno != EINVAL)
            {
                throw std::filesystem::filesystem_error("File.Move", source, destination,
                    std::error_code(errno, std::generic_category()));
            }
            if (::link(source.c_str(), destination.c_str()) != 0)
            {
                throw std::filesystem::filesystem_error("File.Move", source, destination,
                    std::error_code(errno, std::generic_category()));
            }
            if (::unlink(source.c_str()) != 0)
            {
                const int error = errno;
                (void)::unlink(destination.c_str());
                throw std::filesystem::filesystem_error("File.Move", source, destination,
                    std::error_code(error, std::generic_category()));
            }
#elif defined(__APPLE__) && defined(RENAME_EXCL)
            if (::renamex_np(source.c_str(), destination.c_str(), RENAME_EXCL) == 0)
            {
                return;
            }
            throw std::filesystem::filesystem_error("File.Move", source, destination,
                std::error_code(errno, std::generic_category()));
#else
            if (FileExists(destination) || DirectoryExists(destination))
            {
                throw std::filesystem::filesystem_error("File.Move", source, destination,
                    std::make_error_code(std::errc::file_exists));
            }
            std::filesystem::rename(source, destination);
#endif
        }

        bool EndsWithSuffix(std::string_view value, std::string_view suffix) noexcept
        {
            if (value.size() < suffix.size())
            {
                return false;
            }
            const std::size_t offset = value.size() - suffix.size();
            for (std::size_t i = 0; i < suffix.size(); ++i)
            {
                unsigned char left = static_cast<unsigned char>(value[offset + i]);
                unsigned char right = static_cast<unsigned char>(suffix[i]);
#if defined(_WIN32) || defined(__APPLE__)
                if (left >= 'A' && left <= 'Z') left = static_cast<unsigned char>(left + ('a' - 'A'));
                if (right >= 'A' && right <= 'Z') right = static_cast<unsigned char>(right + ('a' - 'A'));
#endif
                if (left != right)
                {
                    return false;
                }
            }
            return true;
        }

        template <typename Action>
        void EnumerateFiles(const FileSystemPath& root, Action&& action)
        {
            struct PendingDirectory final
            {
                FileSystemPath Path;
                std::int32_t RemainingDepth;
                bool Root;
            };

            std::queue<PendingDirectory> pending;
            pending.push(PendingDirectory{
                root, std::numeric_limits<std::int32_t>::max(), true});
            while (!pending.empty())
            {
                PendingDirectory current = std::move(pending.front());
                pending.pop();

                std::error_code openError;
                std::filesystem::directory_iterator iterator(current.Path, openError);
                if (openError)
                {
                    // SearchOption.AllDirectories silently skips a queued
                    // subdirectory that disappeared before it was opened.
                    if (!current.Root
                        && (openError == std::errc::no_such_file_or_directory
                            || openError == std::errc::not_a_directory))
                    {
                        continue;
                    }
                    throw std::filesystem::filesystem_error(
                        "Directory.EnumerateFiles", current.Path, openError);
                }

                const std::filesystem::directory_iterator end;
                for (; iterator != end; ++iterator)
                {
                    std::error_code statusError;
                    const bool isDirectory = iterator->is_directory(statusError);
                    if (!statusError && isDirectory)
                    {
                        // .NET's recursive enumerator queues directories and
                        // drains that queue after the current directory, so
                        // traversal is breadth-first rather than depth-first.
                        // SearchOption.AllDirectories uses the compatible
                        // int.MaxValue recursion budget and decrements it for
                        // every queued level, including symbolic-link cycles.
                        if (current.RemainingDepth > 0)
                        {
                            pending.push(PendingDirectory{
                                iterator->path(),
                                current.RemainingDepth - 1,
                                false});
                        }
                    }
                    else
                    {
                        // Symlinks whose targets cannot be resolved are
                        // non-directory entries for enumeration purposes.
                        action(iterator->path());
                    }
                }
            }
        }

        template <typename Action>
        void EnumerateFiles(const FileSystemPath& root, std::string_view suffix,
            Action&& action)
        {
            EnumerateFiles(root, [&](const FileSystemPath& path)
            {
                if (EndsWithSuffix(PathToUtf8(path.filename()), suffix))
                {
                    action(path);
                }
            });
        }

        bool HasExtension(const FileSystemPath& path)
        {
            // Path.GetExtension scans the final path component from the end.
            // A trailing dot is no extension; a leading dot followed by text
            // (for example .tool) is an extension.
            const std::string name = PathToUtf8(path.filename());
            const std::size_t dot = name.rfind('.');
            return dot != std::string::npos && dot + 1 < name.size();
        }

#ifdef _WIN32
        bool IsDotNetWhiteSpace(wchar_t value) noexcept
        {
            if (value >= L'\t' && value <= L'\r')
            {
                return true;
            }
            switch (value)
            {
            case 0x0020:
            case 0x0085:
            case 0x00A0:
            case 0x1680:
            case 0x2028:
            case 0x2029:
            case 0x202F:
            case 0x205F:
            case 0x3000:
                return true;
            default:
                return value >= 0x2000 && value <= 0x200A;
            }
        }

        std::wstring QuoteWindowsArgument(std::wstring_view value)
        {
            bool simple = !value.empty();
            if (simple)
            {
                for (wchar_t ch : value)
                {
                    if (IsDotNetWhiteSpace(ch) || ch == L'"')
                    {
                        simple = false;
                        break;
                    }
                }
            }
            if (simple)
            {
                return std::wstring(value);
            }

            std::wstring result;
            result.push_back(L'"');
            std::size_t slashes = 0;
            for (wchar_t ch : value)
            {
                if (ch == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (ch == L'"')
                {
                    result.append(slashes * 2 + 1, L'\\');
                    result.push_back(L'"');
                    slashes = 0;
                    continue;
                }
                result.append(slashes, L'\\');
                slashes = 0;
                result.push_back(ch);
            }
            result.append(slashes * 2, L'\\');
            result.push_back(L'"');
            return result;
        }

        std::string WindowsErrorMessage(DWORD error)
        {
            if (error == ERROR_BAD_EXE_FORMAT || error == ERROR_EXE_MACHINE_TYPE_MISMATCH)
            {
                return "The specified executable is not a valid application for this OS platform.";
            }
            LPWSTR buffer = nullptr;
            const DWORD length = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
            if (length == 0 || buffer == nullptr)
            {
                return std::system_category().message(static_cast<int>(error));
            }
            std::wstring_view message(buffer, length);
            while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
            {
                message.remove_suffix(1);
            }
            std::string result = WideToUtf8(message);
            ::LocalFree(buffer);
            return result;
        }
#endif

        std::runtime_error ProcessStartFailure(const FileSystemPath& binary,
            const FileSystemPath& workingDirectory, const std::string& error)
        {
            return std::runtime_error(
                "An error occurred trying to start process '" + PathToUtf8(binary)
                + "' with working directory '" + PathToUtf8(workingDirectory)
                + "'. " + error);
        }

        bool StartProcess(const FileSystemPath& binary,
            const FileSystemPath& workingDirectory,
            const std::vector<std::string>& arguments)
        {
#ifdef _WIN32
            std::wstring command = L"\"" + binary.native() + L"\"";
            for (const std::string& argument : arguments)
            {
                command.push_back(L' ');
                command += QuoteWindowsArgument(Utf8ToWide(argument));
            }
            std::vector<wchar_t> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back(L'\0');

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (!::CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                    0, nullptr, workingDirectory.c_str(), &startup, &process))
            {
                const DWORD error = ::GetLastError();
                throw ProcessStartFailure(binary, workingDirectory,
                    WindowsErrorMessage(error));
            }
            ::CloseHandle(process.hThread);
            ::CloseHandle(process.hProcess);
            return true;
#else
            std::vector<std::string> storage;
            storage.reserve(arguments.size() + 1);
            storage.push_back(PathToUtf8(binary));
            storage.insert(storage.end(), arguments.begin(), arguments.end());
            std::vector<char*> argv;
            argv.reserve(storage.size() + 1);
            for (std::string& value : storage)
            {
                argv.push_back(value.data());
            }
            argv.push_back(nullptr);

            int pipefd[2]{-1, -1};
#if defined(__linux__) && defined(O_CLOEXEC)
            if (::pipe2(pipefd, O_CLOEXEC) != 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
#else
            if (::pipe(pipefd) != 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
            for (int fd : pipefd)
            {
                const int flags = ::fcntl(fd, F_GETFD);
                if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
                {
                    const int error = errno;
                    ::close(pipefd[0]);
                    ::close(pipefd[1]);
                    throw std::system_error(error, std::generic_category());
                }
            }
#endif

            const std::string binaryText = PathToUtf8(binary);
            const std::string workingText = PathToUtf8(workingDirectory);

            std::error_code directoryError;
            if (std::filesystem::is_directory(binary, directoryError) && !directoryError)
            {
                ::close(pipefd[0]);
                ::close(pipefd[1]);
                throw std::runtime_error(
                    "The FileName property should not be a directory unless UseShellExecute is set.");
            }

            sigset_t allSignals{};
            sigset_t oldSignals{};
            ::sigfillset(&allSignals);
            const int maskResult = ::pthread_sigmask(SIG_SETMASK, &allSignals, &oldSignals);
            if (maskResult != 0)
            {
                ::close(pipefd[0]);
                ::close(pipefd[1]);
                throw std::system_error(maskResult, std::generic_category());
            }

            const pid_t pid = ::fork();
            const int forkError = errno;
            if (pid != 0)
            {
                (void)::pthread_sigmask(SIG_SETMASK, &oldSignals, nullptr);
            }
            if (pid < 0)
            {
                ::close(pipefd[0]);
                ::close(pipefd[1]);
                throw ProcessStartFailure(binary, workingDirectory,
                    std::error_code(forkError, std::generic_category()).message());
            }
            if (pid == 0)
            {
                ::close(pipefd[0]);
                auto fail = [&](int error) noexcept
                {
                    const char* bytes = reinterpret_cast<const char*>(&error);
                    std::size_t written = 0;
                    while (written < sizeof(error))
                    {
                        const ssize_t count = ::write(pipefd[1], bytes + written,
                            sizeof(error) - written);
                        if (count > 0)
                        {
                            written += static_cast<std::size_t>(count);
                        }
                        else if (count < 0 && errno == EINTR)
                        {
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }
                    ::_exit(127);
                };

                struct sigaction defaultAction{};
                defaultAction.sa_handler = SIG_DFL;
                ::sigemptyset(&defaultAction.sa_mask);
                for (int signal = 1; signal < NSIG; ++signal)
                {
                    if (signal == SIGKILL || signal == SIGSTOP)
                    {
                        continue;
                    }
                    struct sigaction current{};
                    if (::sigaction(signal, nullptr, &current) == 0
                        && current.sa_handler != SIG_DFL
                        && current.sa_handler != SIG_IGN)
                    {
                        (void)::sigaction(signal, &defaultAction, nullptr);
                    }
                }
                (void)::pthread_sigmask(SIG_SETMASK, &oldSignals, nullptr);

                if (::chdir(workingText.c_str()) != 0)
                {
                    fail(errno);
                }
                ::execv(binaryText.c_str(), argv.data());
                fail(errno);
            }

            ::close(pipefd[1]);
            int launchError = 0;
            std::size_t received = 0;
            while (received < sizeof(launchError))
            {
                const ssize_t count = ::read(pipefd[0],
                    reinterpret_cast<char*>(&launchError) + received,
                    sizeof(launchError) - received);
                if (count > 0)
                {
                    received += static_cast<std::size_t>(count);
                    continue;
                }
                if (count == 0)
                {
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                launchError = errno;
                received = sizeof(launchError);
                break;
            }
            ::close(pipefd[0]);
            if (received != 0)
            {
                int status = 0;
                while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
                {
                }
                throw ProcessStartFailure(binary, workingDirectory,
                    std::error_code(launchError, std::generic_category()).message());
            }
            return true;
#endif
        }
    }

    bool ServerUpdate::Enabled() noexcept
    {
        return GetState().Enabled.load(std::memory_order_relaxed);
    }

    void ServerUpdate::Enabled(bool value) noexcept
    {
        GetState().Enabled.store(value, std::memory_order_relaxed);
    }

    ServerUpdate::TimeSpan ServerUpdate::Interval() noexcept
    {
        return TimeSpan(GetState().IntervalTicks.load(std::memory_order_relaxed));
    }

    void ServerUpdate::Interval(TimeSpan value) noexcept
    {
        GetState().IntervalTicks.store(value.count(), std::memory_order_relaxed);
    }

    std::optional<UpdateInfo> ServerUpdate::Pending()
    {
        State& state = GetState();
        const std::lock_guard<std::mutex> guard(state.PendingGate);
        return state.Pending;
    }

    bool ServerUpdate::Staged() noexcept
    {
        return GetState().Staged.load(std::memory_order_acquire);
    }

    bool ServerUpdate::Supervised() noexcept
    {
        const char* invocation = std::getenv("INVOCATION_ID");
        if (invocation != nullptr && *invocation != '\0')
        {
            return true;
        }
        const char* listenPid = std::getenv("LISTEN_PID");
        return listenPid != nullptr && *listenPid != '\0';
    }

    bool ServerUpdate::AtStartup(const std::vector<std::string>& commandLine)
    {
        State& state = GetState();
        {
            auto relaunch = std::make_shared<const std::vector<std::string>>(commandLine);
            state.Relaunch.store(std::move(relaunch), std::memory_order_release);
            const std::int64_t now = UtcNowTicks();
            const std::int64_t interval =
                state.IntervalTicks.load(std::memory_order_relaxed);
            state.NextCheckTicks.store(AddDateTime(now, interval),
                std::memory_order_relaxed);
        }

        const std::string target = BaseDirectoryText();
        SweepOld(target);
        if (!Enabled() || Detail::ServerUpdateUpdaterDisabled())
        {
            return false;
        }

        std::optional<UpdateInfo> update;
        try
        {
            update = Detail::ServerUpdateUpdaterCheck();
        }
        catch (const std::exception& ex)
        {
            WriteLine(std::string("[update] could not check: ") + ex.what());
            return false;
        }
        if (!update)
        {
            const std::optional<std::string> reason = UpdateCheck::LastReason();
            if (reason && !reason->empty())
            {
                WriteLine(std::string("[update] ") + *reason);
            }
            return false;
        }

        WriteLine(std::string("[update] ")
            + Detail::ServerUpdateUpdaterDescribe(*update));
        if (!Stage(*update))
        {
            WriteLine(std::string("[update] carrying on with ") + BuildVersion::Display());
            return false;
        }
        return Swap("before binding");
    }

    bool ServerUpdate::ShouldRestart(std::int32_t playerCount)
    {
        State& state = GetState();
        if (!Enabled() || Detail::ServerUpdateUpdaterDisabled())
        {
            return false;
        }
        if (state.Staged.load(std::memory_order_acquire))
        {
            if (playerCount > 0)
            {
                return false;
            }
            return Swap("the server is empty");
        }

        {
            const std::lock_guard<std::mutex> guard(state.Gate);
            if (state.Working || UtcNowTicks()
                < state.NextCheckTicks.load(std::memory_order_relaxed))
            {
                return false;
            }
            state.Working = true;
            const std::int64_t now = UtcNowTicks();
            const std::int64_t interval =
                state.IntervalTicks.load(std::memory_order_relaxed);
            state.NextCheckTicks.store(AddDateTime(now, interval),
                std::memory_order_relaxed);
        }

        std::thread([]
        {
            State& taskState = GetState();
            struct WorkingFinally final
            {
                State& Value;
                ~WorkingFinally() noexcept
                {
                    try
                    {
                        const std::lock_guard<std::mutex> guard(Value.Gate);
                        Value.Working = false;
                    }
                    catch (...)
                    {
                    }
                }
            } finally{taskState};

            try
            {
                try
                {
                    std::optional<UpdateInfo> update = Detail::ServerUpdateUpdaterCheck();
                    if (update)
                    {
                        WriteLine(std::string("[update] ")
                            + Detail::ServerUpdateUpdaterDescribe(*update));
                        if (Stage(*update))
                        {
                            WriteLine("[update] staged; it will be applied "
                                "as soon as the server is empty");
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    WriteLine(std::string("[update] check failed: ") + ex.what());
                }
            }
            catch (...)
            {
                // Task.Run stores exceptions thrown by the delegate; this task
                // is intentionally unobserved by the caller.
            }
        }).detach();
        return false;
    }

    bool ServerUpdate::Stage(UpdateInfo update)
    {
        if (!DesktopUpdate::Supported())
        {
            std::optional<std::string> error = DesktopUpdate::LastError();
            const std::string why = error ? *error : "the directory is not writable";
            WriteLine(std::string("[update] this installation cannot update itself (")
                + why + "); fetch it from " + OptionalText(update.PageUrl.Get()));
            return false;
        }

        WriteLine(std::string("[update] fetching ")
            + OptionalText(update.AssetName.Get()));
        if (!DesktopUpdate::Stage(update))
        {
            WriteLine(std::string("[update] could not stage: ")
                + OptionalText(DesktopUpdate::LastError()));
            return false;
        }

        State& state = GetState();
        {
            const std::lock_guard<std::mutex> guard(state.PendingGate);
            state.Pending = update;
        }
        state.Staged.store(true, std::memory_order_release);
        return true;
    }

    bool ServerUpdate::Swap(const std::string& why)
    {
        const std::string staged = DesktopUpdate::StagedBuildPath();
        const std::string target = BaseDirectoryText();
        if (!DirectoryExists(PathFromUtf8(staged)))
        {
            GetState().Staged.store(false, std::memory_order_release);
            return false;
        }

        std::string version = "the new build";
        if (Pending().has_value())
        {
            const std::optional<UpdateInfo> pending = Pending();
            if (!pending)
            {
                throw InvalidOperationException("Nullable object must have a value.");
            }
            const std::optional<Version>& pendingVersion = pending->Version.Get();
            if (!pendingVersion)
            {
                throw NullReferenceException();
            }
            version = pendingVersion->ToString();
        }
        WriteLine(std::string("[update] applying ") + version + " (" + why + ")");

        try
        {
            ReplaceInPlace(staged, target);
        }
        catch (const std::exception& ex)
        {
            WriteLine(std::string("[update] the copy failed: ") + ex.what());
            WriteLine(std::string("[update] the new build is in ") + staged
                + " -- copy it over " + target + " by hand");
            GetState().Staged.store(false, std::memory_order_release);
            return false;
        }

        GetState().Staged.store(false, std::memory_order_release);
        if (Supervised())
        {
            WriteLine("[update] applied; exiting for the supervisor "
                "to start the new build");
            return true;
        }
        if (!Restart(target))
        {
            WriteLine("[update] applied, but could not restart -- "
                "this server is still running the old build until it is "
                "restarted by hand");
            return false;
        }
        WriteLine("[update] applied; restarting");
        return true;
    }

    void ServerUpdate::ReplaceInPlace(const std::string& source, const std::string& target)
    {
        const FileSystemPath sourcePath = PathFromUtf8(source);
        const FileSystemPath targetPath = PathFromUtf8(target);
        EnumerateFiles(sourcePath, [&](const FileSystemPath& path)
        {
            const FileSystemPath relative = path.lexically_relative(sourcePath);
            const FileSystemPath destination = targetPath / relative;
            const FileSystemPath directory = destination.parent_path();
            if (!directory.empty())
            {
                std::filesystem::create_directories(directory);
            }
            const FileSystemPath incoming = PathFromUtf8(
                PathToUtf8(destination) + IncomingSuffix);
            std::filesystem::copy_file(path, incoming,
                std::filesystem::copy_options::overwrite_existing);
            if (FileExists(destination))
            {
                Displace(PathToUtf8(destination));
            }
            MoveNoReplace(incoming, destination);
            MakeExecutable(PathToUtf8(destination));
        });
    }

    void ServerUpdate::Displace(const std::string& destination)
    {
        const FileSystemPath destinationPath = PathFromUtf8(destination);
        try
        {
            DeleteFile(destinationPath);
            return;
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // IOException / UnauthorizedAccessException equivalent: in-use
            // files fall through to the rename, whose failure remains visible.
        }

        const FileSystemPath aside = PathFromUtf8(destination + OldSuffix);
        DeleteFile(aside);
        MoveNoReplace(destinationPath, aside);
    }

    void ServerUpdate::SweepOld(const std::string& target) noexcept
    {
        try
        {
            const FileSystemPath targetPath = PathFromUtf8(target);
            EnumerateFiles(targetPath, OldSuffix, [](const FileSystemPath& path)
            {
                try
                {
                    DeleteFile(path);
                }
                catch (...)
                {
                }
            });
            EnumerateFiles(targetPath, IncomingSuffix, [](const FileSystemPath& path)
            {
                try
                {
                    DeleteFile(path);
                }
                catch (...)
                {
                }
            });
        }
        catch (...)
        {
        }
    }

    bool ServerUpdate::Restart(const std::string& target) noexcept
    {
        try
        {
            const FileSystemPath targetPath = PathFromUtf8(target);
            const FileSystemPath binary = targetPath / PathFromUtf8(UpdateCheck::BinaryName());
            std::vector<std::string> relaunch;
            State& state = GetState();
            for (std::int32_t i = 0;; ++i)
            {
                const std::shared_ptr<const std::vector<std::string>> lengthView =
                    state.Relaunch.load(std::memory_order_acquire);
                if (static_cast<std::size_t>(i) >= lengthView->size())
                {
                    break;
                }
                const std::shared_ptr<const std::vector<std::string>> valueView =
                    state.Relaunch.load(std::memory_order_acquire);
                relaunch.push_back(valueView->at(static_cast<std::size_t>(i)));
            }
            return StartProcess(binary, targetPath, relaunch);
        }
        catch (const std::exception& ex)
        {
            WriteLine(std::string("[update] could not restart: ") + ex.what());
            return false;
        }
        catch (...)
        {
            WriteLine("[update] could not restart");
            return false;
        }
    }

    void ServerUpdate::MakeExecutable(const std::string& path) noexcept
    {
#ifdef _WIN32
        (void)path;
        return;
#else
        try
        {
            const FileSystemPath file = PathFromUtf8(path);
            if (HasExtension(file))
            {
                return;
            }
            struct stat status{};
            if (::stat(file.c_str(), &status) != 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
            if (::chmod(file.c_str(), status.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) != 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
        }
        catch (...)
        {
        }
#endif
    }
}

namespace MphRead::Mods::Network::Detail
{
    bool DedicatedServerServerUpdateShouldRestart(std::int32_t peerCount)
    {
        return MphRead::Mods::Update::ServerUpdate::ShouldRestart(peerCount);
    }
}

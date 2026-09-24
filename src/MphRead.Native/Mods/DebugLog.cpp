#include "DebugLog.hpp"

#include "../Program.hpp"
#include "Branding.hpp"
#include "Launcher/Portable/GameFiles.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "Network/NetLag.hpp"
#include "Network/NetProtocol.hpp"
#include "RenderOptions.hpp"
#include "Update/BuildVersion.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <typeinfo>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <execinfo.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#if !defined(__ANDROID__)
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__linux__) || defined(__ANDROID__)
#include <sched.h>
#endif
#endif

#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace
{
#if defined(_WIN32)
    constexpr std::string_view NewLine = "\r\n";
#else
    constexpr std::string_view NewLine = "\n";
#endif

    constexpr std::int32_t KeepFiles = 8;

    [[nodiscard]] bool IsDirectorySeparator(char value) noexcept
    {
#if defined(_WIN32)
        return value == '\\' || value == '/';
#else
        return value == '/';
#endif
    }

    [[nodiscard]] std::string CombinePath(
        std::string_view first, std::string_view second)
    {
        if (first.find('\0') != std::string_view::npos
            || second.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("Path contains a null character.");
        }
        if (first.empty())
        {
            return std::string(second);
        }
        if (second.empty())
        {
            return std::string(first);
        }

        std::string result(first);
        const char last = result.back();
        if (!IsDirectorySeparator(last)
#if defined(_WIN32)
            && last != ':'
#endif
        )
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

#if defined(_WIN32)
    [[nodiscard]] std::wstring WideFromUtf8(std::string_view text)
    {
        if (text.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("Path contains a null character.");
        }
        if (text.empty())
        {
            return {};
        }
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String is too long.");
        }
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
        {
            throw std::system_error(static_cast<int>(GetLastError()),
                std::system_category());
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), length) <= 0)
        {
            throw std::system_error(static_cast<int>(GetLastError()),
                std::system_category());
        }
        return result;
    }

    [[nodiscard]] std::string Utf8FromWide(std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String is too long.");
        }
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (length <= 0)
        {
            throw std::system_error(static_cast<int>(GetLastError()),
                std::system_category());
        }
        std::string result(static_cast<std::size_t>(length), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), result.data(), length,
            nullptr, nullptr) <= 0)
        {
            throw std::system_error(static_cast<int>(GetLastError()),
                std::system_category());
        }
        return result;
    }
#endif

    [[nodiscard]] std::filesystem::path PathFromManagedString(std::string_view path)
    {
#if defined(_WIN32)
        return std::filesystem::path(WideFromUtf8(path));
#else
        if (path.find('\0') != std::string_view::npos)
        {
            throw std::invalid_argument("Path contains a null character.");
        }
        return std::filesystem::path(std::string(path));
#endif
    }

    class FileSink final
    {
    public:
        explicit FileSink(std::string_view path)
        {
#if defined(_WIN32)
            const std::wstring wide = WideFromUtf8(path);
            _handle = CreateFileW(wide.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (_handle == INVALID_HANDLE_VALUE)
            {
                throw std::system_error(static_cast<int>(GetLastError()),
                    std::system_category());
            }
#else
            if (path.find('\0') != std::string_view::npos)
            {
                throw std::invalid_argument("Path contains a null character.");
            }
            const std::string native(path);
            int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_CLOEXEC
            flags |= O_CLOEXEC;
#endif
            _fd = ::open(native.c_str(), flags, 0666);
            if (_fd == -1)
            {
                throw std::system_error(errno, std::generic_category());
            }
#endif
        }

        FileSink(const FileSink&) = delete;
        FileSink& operator=(const FileSink&) = delete;

        ~FileSink()
        {
            CloseNoThrow();
        }

        void Write(std::string_view bytes)
        {
#if defined(_WIN32)
            std::size_t offset = 0;
            while (offset < bytes.size())
            {
                const std::size_t remaining = bytes.size() - offset;
                const DWORD chunk = remaining > static_cast<std::size_t>(MAXDWORD)
                    ? MAXDWORD
                    : static_cast<DWORD>(remaining);
                DWORD written = 0;
                if (!WriteFile(_handle, bytes.data() + offset, chunk, &written, nullptr))
                {
                    throw std::system_error(static_cast<int>(GetLastError()),
                        std::system_category());
                }
                if (written == 0)
                {
                    throw std::ios_base::failure("Could not write log file.");
                }
                offset += written;
            }
#else
            std::size_t offset = 0;
            while (offset < bytes.size())
            {
                const ssize_t written = ::write(_fd, bytes.data() + offset,
                    bytes.size() - offset);
                if (written == -1)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category());
                }
                if (written == 0)
                {
                    throw std::ios_base::failure("Could not write log file.");
                }
                offset += static_cast<std::size_t>(written);
            }
#endif
        }

        void Flush()
        {
            // FileStream.Flush() is needed by StreamWriter to empty the
            // managed buffers; this sink writes directly to the OS on every
            // call, so there is no corresponding user-space buffer here.
        }

        void Dispose() noexcept
        {
            CloseNoThrow();
        }

#if defined(_WIN32)
        [[nodiscard]] HANDLE Handle() const noexcept
        {
            return _handle;
        }
#else
        [[nodiscard]] int Descriptor() const noexcept
        {
            return _fd;
        }
#endif

    private:
        void CloseNoThrow() noexcept
        {
#if defined(_WIN32)
            if (_handle != INVALID_HANDLE_VALUE)
            {
                (void)CloseHandle(_handle);
                _handle = INVALID_HANDLE_VALUE;
            }
#else
            if (_fd != -1)
            {
                int result = 0;
                do
                {
                    result = ::close(_fd);
                }
                while (result == -1 && errno == EINTR);
                _fd = -1;
            }
#endif
        }

#if defined(_WIN32)
        HANDLE _handle = INVALID_HANDLE_VALUE;
#else
        int _fd = -1;
#endif
    };

    class Utf8Writer final
    {
    public:
        explicit Utf8Writer(std::string_view path)
            : _stream(std::make_shared<FileSink>(path))
        {
            // StreamWriter(stream, Encoding.UTF8) followed by AutoFlush=true
            // flushes the Encoding.UTF8 preamble even before the first line.
            static constexpr char Utf8Preamble[] = "\xEF\xBB\xBF";
            _stream->Write(std::string_view(Utf8Preamble, 3));
        }

        void Write(std::string_view value)
        {
            _stream->Write(value);
            _stream->Flush();
        }

        void Write(char value)
        {
            _stream->Write(std::string_view(&value, 1));
            _stream->Flush();
        }

        void WriteLine(std::string_view value)
        {
            _stream->Write(value);
            _stream->Write(NewLine);
            _stream->Flush();
        }

        void WriteNullLine()
        {
            _stream->Write(NewLine);
            _stream->Flush();
        }

        void Flush()
        {
            _stream->Flush();
        }

        void Dispose() noexcept
        {
            _stream->Dispose();
        }

    private:
        std::shared_ptr<FileSink> _stream;
    };

    struct State;
    State& GetState();

    class TeeBuffer final : public std::streambuf
    {
    public:
        explicit TeeBuffer(std::streambuf* console) : _console(console) {}

    protected:
        int_type overflow(int_type value) override;
        std::streamsize xsputn(const char* data, std::streamsize count) override;
        int sync() override;

    private:
        std::streambuf* _console;
    };

    struct State final
    {
        std::atomic<std::shared_ptr<Utf8Writer>> Writer{};
        std::atomic<std::shared_ptr<const std::string>> Path{};
        std::atomic<std::shared_ptr<const std::string>> NativePath{};
        std::atomic<std::shared_ptr<FileSink>> NativeStream{};
        std::recursive_mutex Lock;
        std::atomic<bool> Hooked{false};
        std::atomic<bool> Forced{false};
        std::streambuf* ConsoleWas = nullptr;
        std::unique_ptr<TeeBuffer> Tee{};
        std::mutex ConsoleStateLock;
        std::atomic<std::uint32_t> HookSubscriptions{0};
        std::once_flag ProcessExitHookOnce;
        std::once_flag TerminateHookOnce;
        std::terminate_handler PreviousTerminate = nullptr;
    };

    State& GetState()
    {
        // Static managed state lives to process exit. Intentionally leak the
        // native carrier so atexit/terminate hooks can still use it after the
        // normal C++ static-destruction phase has begun.
        static State* state = new State();
        return *state;
    }

    TeeBuffer::int_type TeeBuffer::overflow(int_type value)
    {
        if (traits_type::eq_int_type(value, traits_type::eof()))
        {
            return traits_type::not_eof(value);
        }

        const char character = traits_type::to_char_type(value);
        if (traits_type::eq_int_type(_console->sputc(character), traits_type::eof()))
        {
            return traits_type::eof();
        }

        State& state = GetState();
        std::lock_guard<std::recursive_mutex> guard(state.Lock);
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->Write(character);
        }
        return value;
    }

    std::streamsize TeeBuffer::xsputn(const char* data, std::streamsize count)
    {
        const std::streamsize written = _console->sputn(data, count);
        if (written != count)
        {
            return written;
        }

        State& state = GetState();
        std::lock_guard<std::recursive_mutex> guard(state.Lock);
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->Write(std::string_view(data, static_cast<std::size_t>(count)));
        }
        return count;
    }

    int TeeBuffer::sync()
    {
        if (_console->pubsync() != 0)
        {
            return -1;
        }
        State& state = GetState();
        std::lock_guard<std::recursive_mutex> guard(state.Lock);
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->Flush();
        }
        return 0;
    }

    [[nodiscard]] std::tm LocalTime(std::time_t time)
    {
        std::tm result{};
#if defined(_WIN32)
        if (localtime_s(&result, &time) != 0)
        {
            throw std::runtime_error("Could not read local time.");
        }
#else
        if (localtime_r(&time, &result) == nullptr)
        {
            throw std::runtime_error("Could not read local time.");
        }
#endif
        return result;
    }

    [[nodiscard]] long LocalUtcOffsetSeconds(std::time_t time, const std::tm& local)
    {
#if defined(_WIN32)
        std::tm localCopy = local;
        const std::time_t asUtc = _mkgmtime(&localCopy);
        if (asUtc == static_cast<std::time_t>(-1))
        {
            return 0;
        }
        const double difference = std::difftime(asUtc, time);
        if (difference > static_cast<double>(LONG_MAX))
        {
            return LONG_MAX;
        }
        if (difference < static_cast<double>(LONG_MIN))
        {
            return LONG_MIN;
        }
        return static_cast<long>(difference);
#elif defined(__APPLE__) || defined(__linux__) || defined(__ANDROID__) \
    || defined(__FreeBSD__) || defined(__OpenBSD__)
        (void)time;
        return static_cast<long>(local.tm_gmtoff);
#else
        std::tm localCopy = local;
        std::tm utc{};
        if (gmtime_r(&time, &utc) == nullptr)
        {
            return 0;
        }
        const std::time_t localValue = std::mktime(&localCopy);
        const std::time_t utcAsLocal = std::mktime(&utc);
        return static_cast<long>(std::difftime(localValue, utcAsLocal));
#endif
    }

    [[nodiscard]] std::string FileTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        const std::tm local = LocalTime(time);
        std::array<char, 32> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), "%Y%m%d-%H%M%S", &local) == 0)
        {
            throw std::runtime_error("Could not format local time.");
        }
        return std::string(buffer.data());
    }

    [[nodiscard]] std::string LineTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        const std::tm local = LocalTime(time);
        std::array<char, 32> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), "%H:%M:%S", &local) == 0)
        {
            throw std::runtime_error("Could not format local time.");
        }
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        if (milliseconds.count() < 0)
        {
            milliseconds += std::chrono::milliseconds(1000);
        }
        std::ostringstream result;
        result.imbue(std::locale::classic());
        result << buffer.data() << '.' << std::setw(3) << std::setfill('0')
            << milliseconds.count();
        return result.str();
    }

    [[nodiscard]] std::string HeaderTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        const std::tm local = LocalTime(time);
        std::array<char, 32> date{};
        if (std::strftime(date.data(), date.size(), "%Y-%m-%d %H:%M:%S", &local) == 0)
        {
            throw std::runtime_error("Could not format local time.");
        }

        long offset = LocalUtcOffsetSeconds(time, local);
        const char sign = offset < 0 ? '-' : '+';
        if (offset < 0)
        {
            offset = -offset;
        }
        const long hours = offset / 3600;
        const long minutes = (offset % 3600) / 60;

        std::ostringstream result;
        result.imbue(std::locale::classic());
        result << date.data() << ' ' << sign << std::setw(2) << std::setfill('0')
            << hours << ':' << std::setw(2) << std::setfill('0') << minutes;
        return result.str();
    }

    [[nodiscard]] std::string ReplaceSpaces(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (char character : value)
        {
            if (character != ' ')
            {
                result.push_back(character);
            }
        }
        return result;
    }

    [[nodiscard]] bool EndsWithAscii(std::string_view value,
        std::string_view suffix) noexcept
    {
        return value.size() >= suffix.size()
            && value.substr(value.size() - suffix.size()) == suffix;
    }

#if defined(_WIN32)
    [[nodiscard]] char AsciiLower(char value) noexcept
    {
        return value >= 'A' && value <= 'Z'
            ? static_cast<char>(value + ('a' - 'A'))
            : value;
    }

    [[nodiscard]] bool EndsWithAsciiIgnoreCase(std::string_view value,
        std::string_view suffix) noexcept
    {
        if (value.size() < suffix.size())
        {
            return false;
        }
        const std::size_t start = value.size() - suffix.size();
        for (std::size_t index = 0; index < suffix.size(); ++index)
        {
            if (AsciiLower(value[start + index]) != AsciiLower(suffix[index]))
            {
                return false;
            }
        }
        return true;
    }
#endif

    [[nodiscard]] bool MatchesPattern(std::string_view name,
        std::string_view pattern) noexcept
    {
        std::string_view suffix;
        if (pattern == "*.log")
        {
            suffix = ".log";
        }
        else if (pattern == "*-native.txt")
        {
            suffix = "-native.txt";
        }
        else
        {
            return false;
        }
#if defined(_WIN32)
        return EndsWithAsciiIgnoreCase(name, suffix);
#else
        return EndsWithAscii(name, suffix);
#endif
    }

    void PrunePattern(const std::string& directory, std::string_view pattern)
    {
        struct Entry final
        {
            std::filesystem::path Path;
            std::filesystem::file_time_type LastWrite;
        };

        std::vector<Entry> files;
        for (const std::filesystem::directory_entry& entry
            : std::filesystem::directory_iterator(PathFromManagedString(directory)))
        {
            std::error_code typeError;
            if (!entry.is_regular_file(typeError) && !entry.is_symlink(typeError))
            {
                continue;
            }
#if defined(_WIN32)
            const std::string name = Utf8FromWide(entry.path().filename().native());
#else
            const std::string name = entry.path().filename().string();
#endif
            if (!MatchesPattern(name, pattern))
            {
                continue;
            }
            files.push_back(Entry{entry.path(), entry.last_write_time()});
        }

        std::sort(files.begin(), files.end(), [](const Entry& left, const Entry& right)
        {
            return right.LastWrite < left.LastWrite;
        });

        for (std::size_t index = static_cast<std::size_t>(KeepFiles - 1);
            index < files.size(); ++index)
        {
            std::error_code removeError;
            const bool removed = std::filesystem::remove(files[index].Path, removeError);
            (void)removed;
            if (removeError)
            {
                throw std::filesystem::filesystem_error(
                    "Could not delete old log.", files[index].Path, removeError);
            }
        }
    }

    void Prune(const std::string& directory)
    {
        try
        {
            PrunePattern(directory, "*.log");
            PrunePattern(directory, "*-native.txt");
        }
        catch (const std::exception&)
        {
            // A directory that cannot be tidied can still be writable.
        }
    }

    [[nodiscard]] std::string ChangeExtensionToNull(std::string_view path)
    {
        const std::size_t separator = path.find_last_of("/\\");
        const std::size_t period = path.find_last_of('.');
        if (period == std::string_view::npos
            || (separator != std::string_view::npos && period < separator))
        {
            return std::string(path);
        }
        return std::string(path.substr(0, period));
    }

    [[nodiscard]] std::optional<std::string> GetDirectoryName(std::string_view path)
    {
        const std::size_t separator = path.find_last_of("/\\");
        if (separator == std::string_view::npos)
        {
            return std::nullopt;
        }
        if (separator == 0)
        {
            return std::string(path.substr(0, 1));
        }
#if defined(_WIN32)
        if (separator == 2 && path.size() >= 3 && path[1] == ':')
        {
            return std::string(path.substr(0, 3));
        }
#endif
        return std::string(path.substr(0, separator));
    }

    [[nodiscard]] bool Redirect(const std::shared_ptr<FileSink>& stream) noexcept
    {
#if defined(_WIN32)
        return SetStdHandle(static_cast<DWORD>(-12), stream->Handle()) != FALSE;
#elif defined(__ANDROID__)
        (void)stream;
        return false;
#else
        return ::dup2(stream->Descriptor(), 2) != -1;
#endif
    }

    [[nodiscard]] std::string BooleanText(bool value)
    {
        return value ? "True" : "False";
    }

    [[nodiscard]] std::string WindowModeText()
    {
        const auto raw = static_cast<std::int32_t>(
            MphRead::Mods::Launcher::LauncherPrefs::WindowMode());
        if (raw == 0)
        {
            return "Windowed";
        }
        if (raw == 1)
        {
            return "BorderlessFullscreen";
        }
        return std::to_string(raw);
    }

    [[nodiscard]] std::string ProcessArchitecture()
    {
#if defined(__x86_64__) || defined(_M_X64)
        return "X64";
#elif defined(__i386__) || defined(_M_IX86)
        return "X86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        return "Arm64";
#elif defined(__arm__) || defined(_M_ARM)
#if defined(__ARM_ARCH_6__)
        return "Armv6";
#else
        return "Arm";
#endif
#elif defined(__wasm__)
        return "Wasm";
#elif defined(__s390x__)
        return "S390x";
#elif defined(__loongarch64)
        return "LoongArch64";
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
        return "Ppc64le";
#else
        return "Unknown";
#endif
    }

    [[nodiscard]] std::string OsArchitecture()
    {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetNativeSystemInfo(&info);
        switch (info.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "X64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "X86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "Arm";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "Arm64";
        default:
            return ProcessArchitecture();
        }
#else
        struct utsname info{};
        if (::uname(&info) != 0)
        {
            return ProcessArchitecture();
        }
        const std::string machine(info.machine);
        if (machine == "x86_64" || machine == "amd64")
        {
            return "X64";
        }
        if (machine == "i386" || machine == "i486"
            || machine == "i586" || machine == "i686")
        {
            return "X86";
        }
        if (machine == "aarch64" || machine == "arm64")
        {
            return "Arm64";
        }
        if (machine.rfind("armv6", 0) == 0)
        {
            return "Armv6";
        }
        if (machine.rfind("arm", 0) == 0)
        {
            return "Arm";
        }
        if (machine == "s390x")
        {
            return "S390x";
        }
        if (machine == "loongarch64")
        {
            return "LoongArch64";
        }
        if (machine == "ppc64le")
        {
            return "Ppc64le";
        }
        return ProcessArchitecture();
#endif
    }

    [[nodiscard]] std::string RuntimeArchitecture()
    {
        return OsArchitecture() + "/" + ProcessArchitecture();
    }

    [[nodiscard]] std::string OsVersion()
    {
#if defined(_WIN32)
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll != nullptr)
        {
            auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
                GetProcAddress(ntdll, "RtlGetVersion"));
            if (rtlGetVersion != nullptr)
            {
                RTL_OSVERSIONINFOW info{};
                info.dwOSVersionInfoSize = sizeof(info);
                if (rtlGetVersion(&info) == 0)
                {
                    return "Microsoft Windows NT " + std::to_string(info.dwMajorVersion)
                        + "." + std::to_string(info.dwMinorVersion)
                        + "." + std::to_string(info.dwBuildNumber) + ".0";
                }
            }
        }
        return "Microsoft Windows NT";
#else
        struct utsname info{};
        if (::uname(&info) == 0)
        {
#if defined(__APPLE__)
            return std::string("Unix ") + info.release;
#else
            return std::string("Unix ") + info.release;
#endif
        }
        return "Unix";
#endif
    }

    [[nodiscard]] std::int32_t ProcessorCount()
    {
        if (const char* overrideValue = std::getenv("DOTNET_PROCESSOR_COUNT"))
        {
            try
            {
                const long parsed = std::stol(overrideValue);
                if (parsed > 0 && parsed <= std::numeric_limits<std::int32_t>::max())
                {
                    return static_cast<std::int32_t>(parsed);
                }
            }
            catch (const std::exception&)
            {
            }
        }
#if defined(__linux__) || defined(__ANDROID__)
        cpu_set_t set;
        CPU_ZERO(&set);
        if (::sched_getaffinity(0, sizeof(set), &set) == 0)
        {
            const int count = CPU_COUNT(&set);
            if (count > 0)
            {
                return count;
            }
        }
#elif defined(_WIN32)
        DWORD_PTR processMask = 0;
        DWORD_PTR systemMask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask))
        {
            std::int32_t count = 0;
            while (processMask != 0)
            {
                count += static_cast<std::int32_t>(processMask & 1U);
                processMask >>= 1U;
            }
            if (count > 0)
            {
                return count;
            }
        }
#endif
        const unsigned int hardware = std::thread::hardware_concurrency();
        return hardware == 0 ? 1 : static_cast<std::int32_t>(hardware);
    }

    [[nodiscard]] std::string CultureName()
    {
#if defined(_WIN32)
        std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
        const int length = GetUserDefaultLocaleName(name.data(),
            static_cast<int>(name.size()));
        if (length > 1)
        {
            return Utf8FromWide(std::wstring_view(name.data(),
                static_cast<std::size_t>(length - 1)));
        }
        return {};
#else
        const char* locale = std::getenv("LC_ALL");
        if (locale == nullptr || *locale == '\0')
        {
            locale = std::getenv("LC_CTYPE");
        }
        if (locale == nullptr || *locale == '\0')
        {
            locale = std::getenv("LANG");
        }
        if (locale == nullptr || *locale == '\0')
        {
            return {};
        }
        std::string value(locale);
        const std::size_t at = value.find('@');
        if (at != std::string::npos)
        {
            value.resize(at);
        }
        const std::size_t period = value.find('.');
        if (period != std::string::npos)
        {
            value.resize(period);
        }
        if (value == "C" || value == "POSIX")
        {
            return {};
        }
        std::replace(value.begin(), value.end(), '_', '-');
        return value;
#endif
    }

    [[nodiscard]] std::optional<std::string> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return Utf8FromWide(std::wstring_view(buffer.data(), length));
            }
            if (buffer.size() > static_cast<std::size_t>(MAXDWORD) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        (void)_NSGetExecutablePath(nullptr, &size);
        if (size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
        std::error_code error;
        const std::filesystem::path canonical = std::filesystem::canonical(
            std::filesystem::path(buffer.data()), error);
        return error ? std::optional<std::string>(buffer.data())
            : std::optional<std::string>(canonical.string());
#elif defined(__linux__) || defined(__ANDROID__)
        std::vector<char> buffer(256);
        for (;;)
        {
            const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0)
            {
                return std::nullopt;
            }
            if (static_cast<std::size_t>(length) < buffer.size())
            {
                return std::string(buffer.data(), static_cast<std::size_t>(length));
            }
            buffer.resize(buffer.size() * 2U);
        }
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::string BaseDirectory()
    {
        const std::optional<std::string> path = ProcessPath();
        if (path.has_value())
        {
            const std::size_t separator = path->find_last_of("/\\");
            if (separator != std::string::npos)
            {
                return path->substr(0, separator + 1);
            }
        }
        std::filesystem::path current = std::filesystem::current_path();
#if defined(_WIN32)
        std::string result = Utf8FromWide(current.native());
        if (result.empty() || !IsDirectorySeparator(result.back()))
        {
            result.push_back('\\');
        }
#else
        std::string result = current.string();
        if (result.empty() || result.back() != '/')
        {
            result.push_back('/');
        }
#endif
        return result;
    }

    [[nodiscard]] std::vector<std::string> CommandLineArgs()
    {
#if defined(_WIN32)
        using CommandLineToArgvWFn = LPWSTR*(WINAPI*)(LPCWSTR, int*);
        HMODULE shell = LoadLibraryW(L"shell32.dll");
        if (shell != nullptr)
        {
            auto convert = reinterpret_cast<CommandLineToArgvWFn>(
                GetProcAddress(shell, "CommandLineToArgvW"));
            if (convert != nullptr)
            {
                int count = 0;
                LPWSTR* values = convert(GetCommandLineW(), &count);
                if (values != nullptr)
                {
                    std::vector<std::string> result;
                    result.reserve(static_cast<std::size_t>(count));
                    for (int index = 0; index < count; ++index)
                    {
                        result.push_back(Utf8FromWide(values[index]));
                    }
                    LocalFree(values);
                    FreeLibrary(shell);
                    return result;
                }
            }
            FreeLibrary(shell);
        }
#elif defined(__APPLE__)
        int* argc = _NSGetArgc();
        char*** argv = _NSGetArgv();
        if (argc != nullptr && argv != nullptr && *argv != nullptr)
        {
            std::vector<std::string> result;
            result.reserve(static_cast<std::size_t>(*argc));
            for (int index = 0; index < *argc; ++index)
            {
                result.emplace_back((*argv)[index] == nullptr ? "" : (*argv)[index]);
            }
            return result;
        }
#elif defined(__linux__) || defined(__ANDROID__)
        std::ifstream input("/proc/self/cmdline", std::ios::binary);
        if (input)
        {
            const std::string data((std::istreambuf_iterator<char>(input)),
                std::istreambuf_iterator<char>());
            std::vector<std::string> result;
            std::size_t start = 0;
            while (start < data.size())
            {
                const std::size_t end = data.find('\0', start);
                if (end == std::string::npos)
                {
                    result.push_back(data.substr(start));
                    break;
                }
                result.push_back(data.substr(start, end - start));
                start = end + 1;
            }
            if (!result.empty())
            {
                return result;
            }
        }
#endif
        if (const std::optional<std::string> path = ProcessPath())
        {
            return {*path};
        }
        return {std::string(MphRead::Mods::Branding::FileName)};
    }

    [[nodiscard]] std::string JoinCommandLineArgs()
    {
        const std::vector<std::string> args = CommandLineArgs();
        std::string result;
        for (std::size_t index = 0; index < args.size(); ++index)
        {
            if (index != 0)
            {
                result.push_back(' ');
            }
            result.append(args[index]);
        }
        return result;
    }

    [[nodiscard]] std::string EnvironmentValue(std::string_view name)
    {
        const std::string key(name);
        const char* value = std::getenv(key.c_str());
        return value == nullptr || *value == '\0' ? "(unset)" : std::string(value);
    }

    [[nodiscard]] bool EnvironmentValueIsNullOrEmpty(std::string_view name)
    {
        const std::string key(name);
        const char* value = std::getenv(key.c_str());
        return value == nullptr || *value == '\0';
    }

    [[nodiscard]] std::string ProgramVersionText()
    {
        return MphRead::Program::Version.ToString();
    }

    [[nodiscard]] std::string ExceptionTypeName(const std::exception& exception)
    {
        const char* raw = typeid(exception).name();
#if defined(__GNUG__)
        int status = 0;
        std::unique_ptr<char, decltype(&std::free)> demangled(
            abi::__cxa_demangle(raw, nullptr, nullptr, &status), &std::free);
        if (status == 0 && demangled)
        {
            return demangled.get();
        }
#endif
        return raw == nullptr ? "std::exception" : std::string(raw);
    }

    [[nodiscard]] std::exception_ptr InnerException(const std::exception& exception) noexcept
    {
        const auto* nested = dynamic_cast<const std::nested_exception*>(&exception);
        return nested == nullptr ? std::exception_ptr{} : nested->nested_ptr();
    }

    [[nodiscard]] std::optional<std::string> NativeStackTrace()
    {
#if defined(_WIN32)
        std::array<void*, 64> frames{};
        const USHORT count = CaptureStackBackTrace(0,
            static_cast<DWORD>(frames.size()), frames.data(), nullptr);
        if (count == 0)
        {
            return std::nullopt;
        }
        std::ostringstream result;
        result.imbue(std::locale::classic());
        for (USHORT index = 2; index < count; ++index)
        {
            if (index != 2)
            {
                result << NewLine;
            }
            result << "   at 0x" << std::hex << std::uppercase
                << reinterpret_cast<std::uintptr_t>(frames[index]);
        }
        const std::string text = result.str();
        return text.empty() ? std::nullopt
            : std::optional<std::string>(text);
#elif defined(__ANDROID__)
        return std::nullopt;
#else
        std::array<void*, 64> frames{};
        const int count = ::backtrace(frames.data(),
            static_cast<int>(frames.size()));
        if (count <= 0)
        {
            return std::nullopt;
        }
        char** symbols = ::backtrace_symbols(frames.data(), count);
        if (symbols == nullptr)
        {
            return std::nullopt;
        }
        std::unique_ptr<char*, decltype(&std::free)> owned(symbols, &std::free);
        std::string result;
        for (int index = 2; index < count; ++index)
        {
            if (!result.empty())
            {
                result.append(NewLine);
            }
            result.append("   at ");
            result.append(symbols[index] == nullptr ? "(unknown)" : symbols[index]);
        }
        return result.empty() ? std::nullopt
            : std::optional<std::string>(std::move(result));
#endif
    }

    void FlushWriterNoThrow() noexcept
    {
        try
        {
            State& state = GetState();
            std::lock_guard<std::recursive_mutex> guard(state.Lock);
            if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
            {
                writer->Flush();
            }
        }
        catch (...)
        {
        }
    }

    void ProcessExitHandler() noexcept
    {
        const std::uint32_t subscriptions = GetState().HookSubscriptions.load();
        for (std::uint32_t index = 0; index < subscriptions; ++index)
        {
            try
            {
                MphRead::Mods::DebugLog::Line("exit", "process exiting");
                FlushWriterNoThrow();
            }
            catch (...)
            {
            }
        }
    }

    [[noreturn]] void TerminateHandler() noexcept
    {
        State& state = GetState();
        const std::exception_ptr exception = std::current_exception();
        const std::uint32_t subscriptions = state.HookSubscriptions.load();
        for (std::uint32_t index = 0; index < subscriptions; ++index)
        {
            try
            {
                MphRead::Mods::DebugLog::Line("crash",
                    "the process is going down with an exception (terminating=True)");
                MphRead::Mods::DebugLog::Exception("crash", exception);
                FlushWriterNoThrow();
            }
            catch (...)
            {
            }
        }

        const std::terminate_handler previous = state.PreviousTerminate;
        if (previous != nullptr && previous != &TerminateHandler)
        {
            previous();
        }
        std::abort();
    }

#if defined(_WIN32)
    // AppDomain.UnhandledException in the C# build: a null dereference or a
    // bad index there is a managed exception, and the log gets its type and
    // its stack before the process goes. The same fault here is a hardware
    // exception that never reaches std::terminate, so without this the log
    // simply stops. Addresses are given as module+offset, and as the address
    // `addr2line -f -C -e FruityPrime.exe` expects (the image's preferred base
    // plus the offset), since the loader puts the image somewhere else.
    LPTOP_LEVEL_EXCEPTION_FILTER PreviousFaultFilter = nullptr;

    [[nodiscard]] std::string DescribeAddress(const void* address)
    {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        HMODULE module = nullptr;
        if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<LPCWSTR>(address), &module) && module != nullptr)
        {
            std::array<char, MAX_PATH> path{};
            const DWORD length = ::GetModuleFileNameA(module, path.data(),
                static_cast<DWORD>(path.size()));
            std::string name(path.data(), length);
            const std::size_t slash = name.find_last_of("\\/");
            if (slash != std::string::npos)
            {
                name = name.substr(slash + 1);
            }
            const auto base = reinterpret_cast<std::uintptr_t>(module);
            const std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(address) - base;
            text << name << "+0x" << std::hex << std::uppercase << offset;
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
                reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
            text << " (addr2line 0x" << (static_cast<std::uintptr_t>(nt->OptionalHeader.ImageBase) + offset)
                << ")";
        }
        else
        {
            text << "0x" << std::hex << std::uppercase << reinterpret_cast<std::uintptr_t>(address);
        }
        return text.str();
    }

    LONG WINAPI NativeFaultFilter(EXCEPTION_POINTERS* pointers)
    {
        try
        {
            const EXCEPTION_RECORD& record = *pointers->ExceptionRecord;
            std::ostringstream head;
            head.imbue(std::locale::classic());
            head << "the process is going down with a native fault 0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(record.ExceptionCode) << std::dec
                << " on thread " << ::GetCurrentThreadId()
                << " at " << DescribeAddress(record.ExceptionAddress);
            if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
            {
                head << (record.ExceptionInformation[0] == 0 ? ", reading 0x"
                    : record.ExceptionInformation[0] == 1 ? ", writing 0x" : ", executing 0x")
                    << std::hex << std::uppercase << record.ExceptionInformation[1];
            }
            MphRead::Mods::DebugLog::Line("crash", head.str());
            std::array<void*, 62> frames{};
            const USHORT count = ::CaptureStackBackTrace(0,
                static_cast<DWORD>(frames.size()), frames.data(), nullptr);
            for (USHORT index = 0; index < count; ++index)
            {
                MphRead::Mods::DebugLog::Line("crash", "   at " + DescribeAddress(frames[index]));
            }
            FlushWriterNoThrow();
        }
        catch (...)
        {
        }
        return PreviousFaultFilter != nullptr ? PreviousFaultFilter(pointers) : EXCEPTION_CONTINUE_SEARCH;
    }
#endif

    void Hook()
    {
        State& state = GetState();
        if (state.Hooked.load())
        {
            return;
        }
        state.Hooked.store(true);

        {
            std::lock_guard<std::mutex> consoleGuard(state.ConsoleStateLock);
            state.ConsoleWas = std::cout.rdbuf();
            state.Tee = std::make_unique<TeeBuffer>(state.ConsoleWas);
            std::cout.rdbuf(state.Tee.get());
        }

        state.HookSubscriptions.fetch_add(1);
        std::call_once(state.ProcessExitHookOnce, []
        {
            (void)std::atexit(&ProcessExitHandler);
        });
        std::call_once(state.TerminateHookOnce, [&state]
        {
            state.PreviousTerminate = std::set_terminate(&TerminateHandler);
#if defined(_WIN32)
            PreviousFaultFilter = ::SetUnhandledExceptionFilter(&NativeFaultFilter);
#endif
        });
    }

    void CaptureNativeErrors()
    {
        State& state = GetState();
        const std::shared_ptr<const std::string> logPath = state.Path.load();
        if (!logPath)
        {
            return;
        }

        try
        {
            const std::string path = ChangeExtensionToNull(*logPath) + "-native.txt";
            std::shared_ptr<FileSink> stream = std::make_shared<FileSink>(path);
            state.NativeStream.store(stream);
            if (!Redirect(stream))
            {
                stream->Dispose();
                state.NativeStream.store(nullptr);
                return;
            }

            state.NativePath.store(std::make_shared<const std::string>(path));
            MphRead::Mods::DebugLog::Line("crash",
                "native stderr is being captured to " + path);
            std::cout << "[debug] standard error is being written to " << path << std::endl;

            if (EnvironmentValueIsNullOrEmpty("DOTNET_DbgEnableMiniDump"))
            {
                const std::string directory = GetDirectoryName(*logPath).value_or(".");
                MphRead::Mods::DebugLog::Line("crash",
                    "no crash dump is configured. For a native stack from the "
                    "next crash, start the game with these three set (type 4 is "
                    "required -- a single-file app supports no other kind, and the "
                    "file is around 110 MB, which zips well):");
                MphRead::Mods::DebugLog::Line("crash",
                    "  DOTNET_DbgEnableMiniDump=1 DOTNET_DbgMiniDumpType=4 "
                    "DOTNET_DbgMiniDumpName=" + CombinePath(directory, "crash-%p.dmp"));
            }
        }
        catch (const std::exception& ex)
        {
            MphRead::Mods::DebugLog::Line("crash",
                "native stderr could not be captured: " + std::string(ex.what()));
        }
    }

    void WriteDisplay()
    {
#if defined(_WIN32) || defined(__APPLE__)
        return;
#else
        MphRead::Mods::DebugLog::Line("display",
            "session=" + EnvironmentValue("XDG_SESSION_TYPE")
            + " desktop=" + EnvironmentValue("XDG_CURRENT_DESKTOP"));
        MphRead::Mods::DebugLog::Line("display",
            "DISPLAY=" + EnvironmentValue("DISPLAY")
            + " WAYLAND_DISPLAY=" + EnvironmentValue("WAYLAND_DISPLAY"));
#endif
    }

    void WriteHeader()
    {
        using MphRead::Mods::Branding;
        using MphRead::Mods::DebugLog;
        using MphRead::Mods::Launcher::GameFiles;
        using MphRead::Mods::Launcher::LauncherPrefs;
        using MphRead::Mods::Network::NetConfig;
        using MphRead::Mods::Network::NetLag;
        using MphRead::Mods::RenderOptions;
        using MphRead::Mods::Update::BuildVersion;

        DebugLog::Line("build", std::string(Branding::Name) + " "
            + BuildVersion::Display() + ", data format " + ProgramVersionText());
        DebugLog::Line("build", "protocol " + std::to_string(NetConfig::ProtocolVersion)
            + ", log started " + HeaderTimestamp());
        DebugLog::Line("system", OsVersion() + " " + RuntimeArchitecture()
            + ", .NET native, " + std::to_string(ProcessorCount()) + " cpu(s)");
        DebugLog::Line("system", "64-bit process=" + BooleanText(sizeof(void*) == 8)
            + ", culture=" + CultureName());
        DebugLog::Line("paths", "base=" + BaseDirectory());
        DebugLog::Line("paths", "prefs=" + LauncherPrefs::Directory());
        const std::optional<std::string> path = DebugLog::Path();
        DebugLog::Line("paths", "log=" + path.value_or(""));
        try
        {
            DebugLog::Line("paths", "game files ready=" + BooleanText(GameFiles::Ready()));
        }
        catch (const std::exception& ex)
        {
            DebugLog::Line("paths", "game files could not be checked: "
                + std::string(ex.what()));
        }
        DebugLog::Line("args", JoinCommandLineArgs());
        WriteDisplay();
        DebugLog::Line("render", std::string("cel=")
            + std::string(RenderOptions::OnOff(RenderOptions::CelShading()))
            + " fog=" + std::string(RenderOptions::OnOff(RenderOptions::Fog()))
            + " window=" + WindowModeText());
        if (NetLag::Active())
        {
            DebugLog::Line("net", "simulated line: " + NetLag::Describe().value_or(""));
        }
    }
}

namespace MphRead::Mods
{
    bool DebugLog::Active() noexcept
    {
        return static_cast<bool>(GetState().Writer.load());
    }

    std::optional<std::string> DebugLog::Path()
    {
        if (std::shared_ptr<const std::string> path = GetState().Path.load())
        {
            return *path;
        }
        return std::nullopt;
    }

    std::optional<std::string> DebugLog::NativePath()
    {
        if (std::shared_ptr<const std::string> path = GetState().NativePath.load())
        {
            return *path;
        }
        return std::nullopt;
    }

    void DebugLog::Force() noexcept
    {
        GetState().Forced.store(true);
    }

    void DebugLog::Attach()
    {
        State& state = GetState();
        if (state.Writer.load()
            || (!state.Forced.load() && !Launcher::LauncherPrefs::DebugLogs()))
        {
            return;
        }

        try
        {
            const std::string directory = CombinePath(
                Launcher::LauncherPrefs::Directory(), "logs");
            std::filesystem::create_directories(PathFromManagedString(directory));
            Prune(directory);
            const std::string name = ReplaceSpaces(Branding::Name) + "-"
                + FileTimestamp() + ".log";
            const std::string path = CombinePath(directory, name);
            state.Path.store(std::make_shared<const std::string>(path));
            state.Writer.store(std::make_shared<Utf8Writer>(path));
        }
        catch (const std::exception& ex)
        {
            state.Writer.store(nullptr);
            std::cout << "[debug] could not open a log: " << ex.what() << std::endl;
            return;
        }

        Hook();
        CaptureNativeErrors();
        WriteHeader();
    }

    void DebugLog::Detach()
    {
        State& state = GetState();
        std::lock_guard<std::recursive_mutex> guard(state.Lock);
        {
            std::lock_guard<std::mutex> consoleGuard(state.ConsoleStateLock);
            if (state.ConsoleWas != nullptr)
            {
                std::cout.rdbuf(state.ConsoleWas);
                state.ConsoleWas = nullptr;
                state.Tee.reset();
            }
        }
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->Flush();
        }
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->Dispose();
        }
        state.Writer.store(nullptr);
        state.Hooked.store(false);
    }

    void DebugLog::Line(std::string_view category, std::string_view message)
    {
        State& state = GetState();
        if (!state.Writer.load())
        {
            return;
        }
        std::lock_guard<std::recursive_mutex> guard(state.Lock);
        if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
        {
            writer->WriteLine("[" + LineTimestamp() + "] [" + std::string(category)
                + "] " + std::string(message));
        }
    }

    void DebugLog::Exception(std::string_view category,
        const std::exception& exception)
    {
        if (!Active())
        {
            return;
        }
        Line(category, ExceptionTypeName(exception) + ": " + exception.what());
        {
            State& state = GetState();
            std::lock_guard<std::recursive_mutex> guard(state.Lock);
            if (std::shared_ptr<Utf8Writer> writer = state.Writer.load())
            {
                // The CLR stores the throw-site stack on Exception itself.
                // Standard C++ exceptions do not, so the closest platform
                // mechanism is the native stack available at the catch/log
                // boundary. It is written raw, with the same one trailing
                // newline as TextWriter.WriteLine(ex.StackTrace).
                if (std::optional<std::string> stack = NativeStackTrace())
                {
                    writer->WriteLine(*stack);
                }
                else
                {
                    writer->WriteNullLine();
                }
            }
        }
        if (std::exception_ptr inner = InnerException(exception))
        {
            Line(category, "caused by:");
            Exception(category, inner);
        }
    }

    void DebugLog::Exception(std::string_view category,
        std::exception_ptr exception)
    {
        if (!Active() || !exception)
        {
            return;
        }
        try
        {
            std::rethrow_exception(exception);
        }
        catch (const std::exception& ex)
        {
            Exception(category, ex);
        }
        catch (...)
        {
            // C# accepts Exception?, so a non-std C++ throw has no managed
            // Exception object to report here.
            return;
        }
    }

    std::unique_ptr<DebugLog::Timed> DebugLog::Step(
        std::string category, std::string what)
    {
        if (!GetState().Writer.load())
        {
            return nullptr;
        }
        return std::unique_ptr<Timed>(new Timed(std::move(category), std::move(what)));
    }

    DebugLog::Timed::Timed(std::string category, std::string what)
        : _category(), _what(), _started(std::chrono::steady_clock::now())
    {
        _category = std::move(category);
        _what = std::move(what);
        DebugLog::Line(_category, _what + ": started");
    }

    DebugLog::Timed::~Timed() noexcept(false)
    {
        Dispose();
    }

    void DebugLog::Timed::Dispose()
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - _started).count();
        DebugLog::Line(_category, _what + ": done in " + std::to_string(elapsed) + " ms");
    }
}

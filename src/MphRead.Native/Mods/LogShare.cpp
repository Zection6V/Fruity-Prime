#include "LogShare.hpp"

#include "Branding.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"

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

    [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            std::uint32_t codePoint = static_cast<std::uint16_t>(value[index]);
            if (codePoint >= 0xD800U && codePoint <= 0xDBFFU)
            {
                if (index + 1 < value.size())
                {
                    const std::uint32_t low = static_cast<std::uint16_t>(value[index + 1]);
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10)
                            + (low - 0xDC00U);
                        ++index;
                    }
                    else
                    {
                        codePoint = 0xFFFDU;
                    }
                }
                else
                {
                    codePoint = 0xFFFDU;
                }
            }
            else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU)
            {
                codePoint = 0xFFFDU;
            }

            if (codePoint <= 0x7FU)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }
        return result;
    }

    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        std::size_t index = 0;
        while (index < value.size())
        {
            const std::uint8_t first = static_cast<std::uint8_t>(value[index]);
            std::uint32_t codePoint = 0;
            std::size_t length = 0;
            std::uint32_t minimum = 0;
            if (first <= 0x7FU)
            {
                codePoint = first;
                length = 1;
            }
            else if ((first & 0xE0U) == 0xC0U)
            {
                codePoint = first & 0x1FU;
                length = 2;
                minimum = 0x80U;
            }
            else if ((first & 0xF0U) == 0xE0U)
            {
                codePoint = first & 0x0FU;
                length = 3;
                minimum = 0x800U;
            }
            else if ((first & 0xF8U) == 0xF0U)
            {
                codePoint = first & 0x07U;
                length = 4;
                minimum = 0x10000U;
            }
            else
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (index + length > value.size())
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            bool valid = true;
            for (std::size_t offset = 1; offset < length; ++offset)
            {
                const std::uint8_t next = static_cast<std::uint8_t>(value[index + offset]);
                if ((next & 0xC0U) != 0x80U)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3FU);
            }
            if (!valid || codePoint < minimum || codePoint > 0x10FFFFU
                || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (codePoint >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (codePoint & 0x3FFU)));
            }
            index += length;
        }
        return result;
    }

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
    [[nodiscard]] std::int64_t LastWriteTicks(std::filesystem::file_time_type value)
    {
        const auto systemValue = std::chrono::file_clock::to_sys(value);
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

    template <typename T, typename Compare>
    void SwapIfGreater(std::vector<T>& values, std::size_t left, std::size_t right,
        Compare compare)
    {
        if (left != right && compare(values[left], values[right]) > 0)
        {
            std::swap(values[left], values[right]);
        }
    }

    template <typename T, typename Compare>
    void InsertionSort(std::vector<T>& values, std::size_t lo, std::size_t hi, Compare compare)
    {
        for (std::size_t index = lo; index < hi; ++index)
        {
            std::size_t next = index + 1;
            T value = std::move(values[next]);
            while (next > lo && compare(value, values[next - 1]) < 0)
            {
                values[next] = std::move(values[next - 1]);
                --next;
            }
            values[next] = std::move(value);
        }
    }

    template <typename T, typename Compare>
    void DownHeap(std::vector<T>& values, std::size_t index, std::size_t count,
        std::size_t lo, Compare compare)
    {
        T value = std::move(values[lo + index - 1]);
        while (index <= count / 2)
        {
            std::size_t child = 2 * index;
            if (child < count
                && compare(values[lo + child - 1], values[lo + child]) < 0)
            {
                ++child;
            }
            if (compare(value, values[lo + child - 1]) >= 0)
            {
                break;
            }
            values[lo + index - 1] = std::move(values[lo + child - 1]);
            index = child;
        }
        values[lo + index - 1] = std::move(value);
    }

    template <typename T, typename Compare>
    void HeapSort(std::vector<T>& values, std::size_t lo, std::size_t hi, Compare compare)
    {
        const std::size_t count = hi - lo + 1;
        for (std::size_t index = count / 2; index >= 1; --index)
        {
            DownHeap(values, index, count, lo, compare);
            if (index == 1)
            {
                break;
            }
        }
        for (std::size_t index = count; index > 1; --index)
        {
            std::swap(values[lo], values[lo + index - 1]);
            DownHeap(values, 1, index - 1, lo, compare);
        }
    }

    template <typename T, typename Compare>
    std::size_t PickPivotAndPartition(std::vector<T>& values, std::size_t lo,
        std::size_t hi, Compare compare)
    {
        const std::size_t middle = lo + ((hi - lo) >> 1);
        SwapIfGreater(values, lo, middle, compare);
        SwapIfGreater(values, lo, hi, compare);
        SwapIfGreater(values, middle, hi, compare);

        T pivot = values[middle];
        std::swap(values[middle], values[hi - 1]);
        std::size_t left = lo;
        std::size_t right = hi - 1;
        while (left < right)
        {
            while (compare(values[++left], pivot) < 0)
            {
            }
            while (compare(pivot, values[--right]) < 0)
            {
            }
            if (left >= right)
            {
                break;
            }
            std::swap(values[left], values[right]);
        }
        if (left != hi - 1)
        {
            std::swap(values[left], values[hi - 1]);
        }
        return left;
    }

    template <typename T, typename Compare>
    void IntroSort(std::vector<T>& values, std::size_t lo, std::size_t hi,
        int depthLimit, Compare compare)
    {
        constexpr std::size_t IntrosortSizeThreshold = 16;
        while (hi > lo)
        {
            const std::size_t partitionSize = hi - lo + 1;
            if (partitionSize <= IntrosortSizeThreshold)
            {
                if (partitionSize == 2)
                {
                    SwapIfGreater(values, lo, hi, compare);
                    return;
                }
                if (partitionSize == 3)
                {
                    SwapIfGreater(values, lo, hi - 1, compare);
                    SwapIfGreater(values, lo, hi, compare);
                    SwapIfGreater(values, hi - 1, hi, compare);
                    return;
                }
                InsertionSort(values, lo, hi, compare);
                return;
            }
            if (depthLimit == 0)
            {
                HeapSort(values, lo, hi, compare);
                return;
            }
            --depthLimit;
            const std::size_t pivot = PickPivotAndPartition(values, lo, hi, compare);
            IntroSort(values, pivot + 1, hi, depthLimit, compare);
            hi = pivot == 0 ? 0 : pivot - 1;
        }
    }

    template <typename T, typename Compare>
    void DotNetSort(std::vector<T>& values, Compare compare)
    {
        if (values.size() <= 1)
        {
            return;
        }
        std::size_t n = values.size();
        int floorLog2 = 0;
        while (n >>= 1U)
        {
            ++floorLog2;
        }
        IntroSort(values, 0, values.size() - 1, 2 * (floorLog2 + 1), compare);
    }

    struct DosDateTime final
    {
        std::uint16_t time = 0;
        std::uint16_t date = 0;
    };

    [[nodiscard]] std::tm LocalTime(std::chrono::system_clock::time_point value)
    {
        const std::time_t raw = std::chrono::system_clock::to_time_t(value);
        std::tm result{};
#if defined(_WIN32)
        const errno_t status = localtime_s(&result, &raw);
        if (status != 0)
        {
            throw std::system_error(static_cast<int>(status), std::generic_category());
        }
#else
        errno = 0;
        if (localtime_r(&raw, &result) == nullptr)
        {
            const int code = errno == 0 ? EOVERFLOW : errno;
            throw std::system_error(code, std::generic_category());
        }
#endif
        return result;
    }

    [[nodiscard]] DosDateTime ToDosDateTime(std::chrono::system_clock::time_point value,
        bool validateRange)
    {
        const std::tm local = LocalTime(value);
        const int year = local.tm_year + 1900;
        if (validateRange && (year < 1980 || year > 2107))
        {
            throw std::out_of_range(
                "The DateTimeOffset specified cannot be converted into a Zip file timestamp. "
                "(Parameter 'value')");
        }
        const int safeYear = std::clamp(year, 1980, 2107);
        DosDateTime result;
        result.date = static_cast<std::uint16_t>(((safeYear - 1980) << 9)
            | ((local.tm_mon + 1) << 5) | local.tm_mday);
        result.time = static_cast<std::uint16_t>((local.tm_hour << 11)
            | (local.tm_min << 5) | (local.tm_sec / 2));
        return result;
    }

    [[nodiscard]] std::u16string ExceptionMessage(const std::exception& ex)
    {
        return Utf8ToUtf16(ex.what());
    }

    class OutputFile final
    {
    public:
        explicit OutputFile(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            _handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (_handle == INVALID_HANDLE_VALUE)
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
#else
            _fd = OpenFile(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0666);
            if (_fd < 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
            try
            {
                _locked = TryFileShareLock(_fd, LOCK_EX);
                int truncateResult = 0;
                do
                {
                    truncateResult = ::ftruncate(_fd, 0);
                }
                while (truncateResult < 0 && errno == EINTR);
                if (truncateResult != 0)
                {
                    const int code = errno;
                    if (code != EBADF && code != EINVAL)
                    {
                        throw std::system_error(code, std::generic_category());
                    }
                }
            }
            catch (...)
            {
                UnlockFileShare(_fd, _locked);
                ::close(_fd);
                _fd = -1;
                throw;
            }
#endif
        }

        ~OutputFile()
        {
            CloseNoThrow();
        }

        OutputFile(const OutputFile&) = delete;
        OutputFile& operator=(const OutputFile&) = delete;

        [[nodiscard]] std::uint64_t Offset() const noexcept
        {
            return _offset;
        }

        void Seek(std::uint64_t offset)
        {
#if defined(_WIN32)
            if (offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max()))
            {
                throw std::out_of_range("The archive position exceeds the seekable range.");
            }
            LARGE_INTEGER position{};
            position.QuadPart = static_cast<LONGLONG>(offset);
            if (!SetFilePointerEx(_handle, position, nullptr, FILE_BEGIN))
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
#else
            if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
            {
                throw std::out_of_range("The archive position exceeds the seekable range.");
            }
            off_t result = 0;
            do
            {
                result = ::lseek(_fd, static_cast<off_t>(offset), SEEK_SET);
            }
            while (result < 0 && errno == EINTR);
            if (result < 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
#endif
            _offset = offset;
        }

        void Write(const void* data, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            while (size > 0)
            {
#if defined(_WIN32)
                const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
                    size, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
                DWORD written = 0;
                if (!WriteFile(_handle, bytes, request, &written, nullptr))
                {
                    throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
                }
                if (written == 0)
                {
                    throw std::runtime_error("The file write returned zero bytes.");
                }
                bytes += written;
                size -= written;
                _offset += written;
#else
                const ssize_t written = ::write(_fd, bytes, size);
                if (written < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category());
                }
                if (written == 0)
                {
                    throw std::runtime_error("The file write returned zero bytes.");
                }
                bytes += static_cast<std::size_t>(written);
                size -= static_cast<std::size_t>(written);
                _offset += static_cast<std::uint64_t>(written);
#endif
            }
        }

        void Close()
        {
#if defined(_WIN32)
            if (_handle != INVALID_HANDLE_VALUE)
            {
                HANDLE handle = _handle;
                _handle = INVALID_HANDLE_VALUE;
                if (!CloseHandle(handle))
                {
                    throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
                }
            }
#else
            if (_fd >= 0)
            {
                const int fd = _fd;
                UnlockFileShare(fd, _locked);
                _fd = -1;
                if (::close(fd) != 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
            }
#endif
        }

    private:
        void CloseNoThrow() noexcept
        {
#if defined(_WIN32)
            if (_handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(_handle);
                _handle = INVALID_HANDLE_VALUE;
            }
#else
            if (_fd >= 0)
            {
                UnlockFileShare(_fd, _locked);
                ::close(_fd);
                _fd = -1;
            }
#endif
        }

#if defined(_WIN32)
        HANDLE _handle = INVALID_HANDLE_VALUE;
#else
        int _fd = -1;
        bool _locked = false;
#endif
        std::uint64_t _offset = 0;
    };

    class InputFile final
    {
    public:
        explicit InputFile(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            _handle = CreateFileW(path.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (_handle == INVALID_HANDLE_VALUE)
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
#else
            _fd = OpenFile(path, O_RDONLY | O_CLOEXEC);
            if (_fd < 0)
            {
                throw std::system_error(errno, std::generic_category());
            }
            try
            {
                struct stat status{};
                int statResult = 0;
                do
                {
                    statResult = ::fstat(_fd, &status);
                }
                while (statResult < 0 && errno == EINTR);
                if (statResult != 0)
                {
                    throw std::system_error(errno, std::generic_category());
                }
                if (S_ISDIR(status.st_mode))
                {
                    throw std::system_error(EACCES, std::generic_category());
                }
                _locked = TryFileShareLock(_fd, LOCK_SH);
            }
            catch (...)
            {
                UnlockFileShare(_fd, _locked);
                ::close(_fd);
                _fd = -1;
                throw;
            }
#endif
        }

        ~InputFile()
        {
#if defined(_WIN32)
            if (_handle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(_handle);
            }
#else
            if (_fd >= 0)
            {
                UnlockFileShare(_fd, _locked);
                ::close(_fd);
            }
#endif
        }

        InputFile(const InputFile&) = delete;
        InputFile& operator=(const InputFile&) = delete;

        [[nodiscard]] std::size_t Read(void* buffer, std::size_t size)
        {
#if defined(_WIN32)
            const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
                size, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
            DWORD read = 0;
            if (!ReadFile(_handle, buffer, request, &read, nullptr))
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
            return static_cast<std::size_t>(read);
#else
            for (;;)
            {
                const ssize_t read = ::read(_fd, buffer, size);
                if (read >= 0)
                {
                    return static_cast<std::size_t>(read);
                }
                if (errno != EINTR)
                {
                    throw std::system_error(errno, std::generic_category());
                }
            }
#endif
        }

    private:
#if defined(_WIN32)
        HANDLE _handle = INVALID_HANDLE_VALUE;
#else
        int _fd = -1;
        bool _locked = false;
#endif
    };

    void WriteU16(OutputFile& output, std::uint16_t value)
    {
        const std::array<std::uint8_t, 2> bytes{
            static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8)};
        output.Write(bytes.data(), bytes.size());
    }

    void WriteU32(OutputFile& output, std::uint32_t value)
    {
        const std::array<std::uint8_t, 4> bytes{
            static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 24)};
        output.Write(bytes.data(), bytes.size());
    }

    void WriteU64(OutputFile& output, std::uint64_t value)
    {
        const std::array<std::uint8_t, 8> bytes{
            static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 24),
            static_cast<std::uint8_t>(value >> 32),
            static_cast<std::uint8_t>(value >> 40),
            static_cast<std::uint8_t>(value >> 48),
            static_cast<std::uint8_t>(value >> 56)};
        output.Write(bytes.data(), bytes.size());
    }

    struct EncodedEntryName final
    {
        std::string bytes;
        bool utf8 = false;
    };

    [[nodiscard]] EncodedEntryName EncodeEntryName(std::u16string_view name)
    {
        bool ascii = true;
        for (const char16_t value : name)
        {
            if (value < u' ' || value > u'~')
            {
                ascii = false;
                break;
            }
        }

        EncodedEntryName result;
        result.utf8 = !ascii;
        if (ascii)
        {
            result.bytes.reserve(name.size());
            for (const char16_t value : name)
            {
                result.bytes.push_back(static_cast<char>(value));
            }
        }
        else
        {
            result.bytes = Utf16ToUtf8(name);
        }
        return result;
    }

    [[nodiscard]] constexpr std::uint32_t DefaultFileExternalAttributes() noexcept
    {
#if defined(_WIN32)
        return 0U;
#else
        return 0x81A40000U;
#endif
    }

    [[nodiscard]] constexpr std::uint8_t CurrentZipPlatform() noexcept
    {
#if defined(_WIN32)
        return 0U;
#else
        return 3U;
#endif
    }

    struct ZipEntryRecord final
    {
        std::string name;
        DosDateTime timestamp{};
        std::uint16_t flags = 0;
        std::uint16_t method = ZipMethodDeflate;
        std::uint16_t versionNeeded = 20U;
        std::uint16_t versionMadeBySpecification = 20U;
        std::uint32_t crc = 0;
        std::uint64_t compressedSize = 0;
        std::uint64_t uncompressedSize = 0;
        std::uint64_t localOffset = 0;
        bool opened = false;
        bool finalized = false;
        bool localHeaderWritten = false;
        bool zip64HeaderUsed = false;
    };

    class ZipWriter;

    class ZipEntryStream final
    {
    public:
        ZipEntryStream(ZipWriter& owner, std::size_t entryIndex);
        ~ZipEntryStream();

        ZipEntryStream(const ZipEntryStream&) = delete;
        ZipEntryStream& operator=(const ZipEntryStream&) = delete;
        ZipEntryStream(ZipEntryStream&&) = delete;
        ZipEntryStream& operator=(ZipEntryStream&&) = delete;

        void Write(const void* data, std::size_t size);
        void Close();

    private:
        void EnsureStarted();
        void CloseNoThrow() noexcept;

        ZipWriter& _owner;
        std::size_t _entryIndex;
        z_stream _stream{};
        std::array<std::uint8_t, 81920> _buffer{};
        std::uint32_t _crc = 0;
        std::uint64_t _compressedSize = 0;
        std::uint64_t _uncompressedSize = 0;
        bool _initialized = false;
        bool _everWritten = false;
        bool _closed = false;
    };

    class ZipWriter final
    {
    public:
        explicit ZipWriter(OutputFile& output) : _output(output) {}

        ~ZipWriter()
        {
            if (!_closed)
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }
        }

        [[nodiscard]] std::size_t CreateEntry(std::u16string_view name)
        {
            ThrowIfFatal();
            FinishUnopenedOwner();

            EncodedEntryName encoded = EncodeEntryName(name);
            if (encoded.bytes.size() > std::numeric_limits<std::uint16_t>::max())
            {
                throw std::invalid_argument(
                    "Entry names cannot require more than 65535 bytes.");
            }

            ZipEntryRecord record;
            record.name = std::move(encoded.bytes);
            record.flags = encoded.utf8 ? ZipFlagUtf8 : 0U;
            record.timestamp = ToDosDateTime(std::chrono::system_clock::now(), false);
            _entries.push_back(std::move(record));
            _ownerIndex = _entries.size() - 1;
            return *_ownerIndex;
        }

        void LastWriteTime(std::size_t index, std::chrono::system_clock::time_point value)
        {
            ThrowIfFatal();
            ZipEntryRecord& entry = _entries.at(index);
            if (entry.opened)
            {
                throw std::runtime_error("The entry has already been opened for writing.");
            }
            entry.timestamp = ToDosDateTime(value, true);
        }

        [[nodiscard]] std::unique_ptr<ZipEntryStream> Open(std::size_t index)
        {
            ThrowIfFatal();
            ZipEntryRecord& entry = _entries.at(index);
            if (entry.opened)
            {
                throw std::runtime_error("Entries in create mode may only be written to once.");
            }
            if (!_ownerIndex.has_value() || *_ownerIndex != index)
            {
                throw std::runtime_error("The ZIP entry no longer owns the archive stream.");
            }
            entry.opened = true;
            return std::make_unique<ZipEntryStream>(*this, index);
        }

        void BeginEntryData(std::size_t index)
        {
            ThrowIfFatal();
            ZipEntryRecord& entry = _entries.at(index);
            if (entry.localHeaderWritten)
            {
                return;
            }
            entry.localOffset = _output.Offset();
            const bool zip64 = entry.localOffset > Zip32Max;
            if (zip64)
            {
                VersionAtLeast(entry, 45U);
            }
            entry.zip64HeaderUsed = zip64;
            entry.method = ZipMethodDeflate;
            WriteLocalHeader(entry, false, zip64);
            entry.localHeaderWritten = true;
        }

        void CompleteEntry(std::size_t index, std::uint32_t crc,
            std::uint64_t compressedSize, std::uint64_t uncompressedSize)
        {
            ZipEntryRecord& entry = _entries.at(index);
            entry.crc = crc;
            entry.compressedSize = compressedSize;
            entry.uncompressedSize = uncompressedSize;

            const bool zip64Needed = compressedSize > Zip32Max
                || uncompressedSize > Zip32Max || entry.localOffset > Zip32Max;
            const bool pretendStreaming = zip64Needed && !entry.zip64HeaderUsed;
            const std::uint64_t finalPosition = _output.Offset();

            try
            {
                if (pretendStreaming)
                {
                    VersionAtLeast(entry, 45U);
                    entry.flags = static_cast<std::uint16_t>(entry.flags | ZipFlagDataDescriptor);
                    _output.Seek(entry.localOffset + 4U);
                    WriteU16(_output, entry.versionNeeded);
                    WriteU16(_output, entry.flags);
                }

                _output.Seek(entry.localOffset + 14U);
                if (pretendStreaming)
                {
                    WriteU32(_output, 0U);
                    WriteU32(_output, 0U);
                    WriteU32(_output, 0U);
                }
                else
                {
                    WriteU32(_output, crc);
                    WriteU32(_output, zip64Needed ? Zip32Max
                        : static_cast<std::uint32_t>(compressedSize));
                    WriteU32(_output, zip64Needed ? Zip32Max
                        : static_cast<std::uint32_t>(uncompressedSize));
                }

                if (entry.zip64HeaderUsed)
                {
                    _output.Seek(entry.localOffset + 30U + entry.name.size() + 4U);
                    WriteU64(_output, uncompressedSize);
                    WriteU64(_output, compressedSize);
                }

                _output.Seek(finalPosition);
                if (pretendStreaming)
                {
                    WriteU32(_output, crc);
                    WriteU64(_output, compressedSize);
                    WriteU64(_output, uncompressedSize);
                }
                entry.finalized = true;
                ReleaseOwner(index);
            }
            catch (...)
            {
                MarkFatal(std::current_exception());
                throw;
            }
        }

        void CompleteEmptyEntry(std::size_t index)
        {
            ZipEntryRecord& entry = _entries.at(index);
            if (entry.finalized)
            {
                return;
            }
            try
            {
                entry.localOffset = _output.Offset();
                entry.method = ZipMethodStore;
                entry.crc = 0;
                entry.compressedSize = 0;
                entry.uncompressedSize = 0;
                entry.zip64HeaderUsed = false;
                WriteLocalHeader(entry, true, false);
                entry.localHeaderWritten = true;
                entry.finalized = true;
                ReleaseOwner(index);
            }
            catch (...)
            {
                MarkFatal(std::current_exception());
                throw;
            }
        }

        void MarkFatal(std::exception_ptr error) noexcept
        {
            if (!_fatal)
            {
                _fatal = std::move(error);
            }
        }

        [[nodiscard]] OutputFile& Output() noexcept
        {
            return _output;
        }

        void Close()
        {
            if (_closed)
            {
                return;
            }
            ThrowIfFatal();
            try
            {
                if (_ownerIndex.has_value())
                {
                    ZipEntryRecord& owner = _entries[*_ownerIndex];
                    if (!owner.opened)
                    {
                        CompleteEmptyEntry(*_ownerIndex);
                    }
                    else if (!owner.finalized)
                    {
                        throw std::runtime_error("A ZIP entry stream was not closed.");
                    }
                }

                for (std::size_t index = 0; index < _entries.size(); ++index)
                {
                    if (!_entries[index].finalized)
                    {
                        CompleteEmptyEntry(index);
                    }
                }

                const std::uint64_t centralOffset = _output.Offset();
                for (ZipEntryRecord& entry : _entries)
                {
                    WriteCentralHeader(entry);
                }
                const std::uint64_t centralSize = _output.Offset() - centralOffset;
                WriteEndOfCentralDirectory(centralOffset, centralSize);
                _closed = true;
            }
            catch (...)
            {
                MarkFatal(std::current_exception());
                throw;
            }
        }

    private:
        friend class ZipEntryStream;

        static void VersionAtLeast(ZipEntryRecord& entry, std::uint16_t version) noexcept
        {
            if (entry.versionNeeded < version)
            {
                entry.versionNeeded = version;
            }
            if (entry.versionMadeBySpecification < version)
            {
                entry.versionMadeBySpecification = version;
            }
        }

        void ThrowIfFatal() const
        {
            if (_fatal)
            {
                std::rethrow_exception(_fatal);
            }
        }

        void ReleaseOwner(std::size_t index) noexcept
        {
            if (_ownerIndex.has_value() && *_ownerIndex == index)
            {
                _ownerIndex.reset();
            }
        }

        void FinishUnopenedOwner()
        {
            if (!_ownerIndex.has_value())
            {
                return;
            }
            ZipEntryRecord& owner = _entries[*_ownerIndex];
            if (owner.opened)
            {
                throw std::runtime_error(
                    "Entries cannot be created while a previous entry is still open.");
            }
            CompleteEmptyEntry(*_ownerIndex);
        }

        void WriteLocalHeader(const ZipEntryRecord& entry, bool isEmpty, bool zip64)
        {
            WriteU32(_output, 0x04034B50U);
            WriteU16(_output, entry.versionNeeded);
            WriteU16(_output, entry.flags);
            WriteU16(_output, entry.method);
            WriteU16(_output, entry.timestamp.time);
            WriteU16(_output, entry.timestamp.date);
            WriteU32(_output, entry.crc);
            if (isEmpty)
            {
                WriteU32(_output, 0U);
                WriteU32(_output, 0U);
            }
            else if (zip64)
            {
                WriteU32(_output, Zip32Max);
                WriteU32(_output, Zip32Max);
            }
            else
            {
                WriteU32(_output, static_cast<std::uint32_t>(entry.compressedSize));
                WriteU32(_output, static_cast<std::uint32_t>(entry.uncompressedSize));
            }
            WriteU16(_output, static_cast<std::uint16_t>(entry.name.size()));
            WriteU16(_output, zip64 ? 20U : 0U);
            _output.Write(entry.name.data(), entry.name.size());
            if (zip64)
            {
                WriteU16(_output, 0x0001U);
                WriteU16(_output, 16U);
                WriteU64(_output, entry.uncompressedSize);
                WriteU64(_output, entry.compressedSize);
            }
        }

        void WriteCentralHeader(ZipEntryRecord& entry)
        {
            const bool sizes64 = entry.compressedSize > Zip32Max
                || entry.uncompressedSize > Zip32Max;
            const bool offset64 = entry.localOffset > Zip32Max;
            const bool zip64 = sizes64 || offset64;
            if (zip64)
            {
                VersionAtLeast(entry, 45U);
            }

            std::uint16_t extraSize = 0;
            if (zip64)
            {
                extraSize = 4U;
                if (sizes64)
                {
                    extraSize = static_cast<std::uint16_t>(extraSize + 16U);
                }
                if (offset64)
                {
                    extraSize = static_cast<std::uint16_t>(extraSize + 8U);
                }
            }

            WriteU32(_output, 0x02014B50U);
            const std::array<std::uint8_t, 2> versionMadeBy{
                static_cast<std::uint8_t>(entry.versionMadeBySpecification),
                CurrentZipPlatform()};
            _output.Write(versionMadeBy.data(), versionMadeBy.size());
            WriteU16(_output, entry.versionNeeded);
            WriteU16(_output, entry.flags);
            WriteU16(_output, entry.method);
            WriteU16(_output, entry.timestamp.time);
            WriteU16(_output, entry.timestamp.date);
            WriteU32(_output, entry.crc);
            WriteU32(_output, sizes64 ? Zip32Max
                : static_cast<std::uint32_t>(entry.compressedSize));
            WriteU32(_output, sizes64 ? Zip32Max
                : static_cast<std::uint32_t>(entry.uncompressedSize));
            WriteU16(_output, static_cast<std::uint16_t>(entry.name.size()));
            WriteU16(_output, extraSize);
            WriteU16(_output, 0U);
            WriteU16(_output, 0U);
            WriteU16(_output, 0U);
            WriteU32(_output, DefaultFileExternalAttributes());
            WriteU32(_output, offset64 ? Zip32Max
                : static_cast<std::uint32_t>(entry.localOffset));
            _output.Write(entry.name.data(), entry.name.size());
            if (zip64)
            {
                WriteU16(_output, 0x0001U);
                WriteU16(_output, static_cast<std::uint16_t>(extraSize - 4U));
                if (sizes64)
                {
                    WriteU64(_output, entry.uncompressedSize);
                    WriteU64(_output, entry.compressedSize);
                }
                if (offset64)
                {
                    WriteU64(_output, entry.localOffset);
                }
            }
        }

        void WriteEndOfCentralDirectory(std::uint64_t centralOffset,
            std::uint64_t centralSize)
        {
            const bool zip64 = centralOffset >= Zip32Max
                || centralSize >= Zip32Max || _entries.size() >= 0xFFFFU;
            if (zip64)
            {
                const std::uint64_t zip64Offset = _output.Offset();
                WriteU32(_output, 0x06064B50U);
                WriteU64(_output, 44U);
                WriteU16(_output, 45U);
                WriteU16(_output, 45U);
                WriteU32(_output, 0U);
                WriteU32(_output, 0U);
                WriteU64(_output, static_cast<std::uint64_t>(_entries.size()));
                WriteU64(_output, static_cast<std::uint64_t>(_entries.size()));
                WriteU64(_output, centralSize);
                WriteU64(_output, centralOffset);
                WriteU32(_output, 0x07064B50U);
                WriteU32(_output, 0U);
                WriteU64(_output, zip64Offset);
                WriteU32(_output, 1U);
            }

            WriteU32(_output, 0x06054B50U);
            WriteU16(_output, 0U);
            WriteU16(_output, 0U);
            WriteU16(_output, _entries.size() > 0xFFFFU ? 0xFFFFU
                : static_cast<std::uint16_t>(_entries.size()));
            WriteU16(_output, _entries.size() > 0xFFFFU ? 0xFFFFU
                : static_cast<std::uint16_t>(_entries.size()));
            WriteU32(_output, centralSize > Zip32Max ? Zip32Max
                : static_cast<std::uint32_t>(centralSize));
            WriteU32(_output, centralOffset > Zip32Max ? Zip32Max
                : static_cast<std::uint32_t>(centralOffset));
            WriteU16(_output, 0U);
        }

        OutputFile& _output;
        std::vector<ZipEntryRecord> _entries;
        std::optional<std::size_t> _ownerIndex;
        std::exception_ptr _fatal;
        bool _closed = false;
    };

    ZipEntryStream::ZipEntryStream(ZipWriter& owner, std::size_t entryIndex)
        : _owner(owner), _entryIndex(entryIndex)
    {
        _crc = static_cast<std::uint32_t>(crc32(0L, Z_NULL, 0));
    }

    ZipEntryStream::~ZipEntryStream()
    {
        CloseNoThrow();
    }

    void ZipEntryStream::EnsureStarted()
    {
        if (_initialized)
        {
            return;
        }
        _owner.BeginEntryData(_entryIndex);
        const int result = deflateInit2(&_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
            -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        if (result != Z_OK)
        {
            const auto error = std::make_exception_ptr(std::runtime_error(
                std::string("deflateInit2 failed: ") + std::to_string(result)));
            _owner.MarkFatal(error);
            std::rethrow_exception(error);
        }
        _initialized = true;
    }

    void ZipEntryStream::Write(const void* data, std::size_t size)
    {
        if (_closed)
        {
            throw std::runtime_error("Cannot write to a closed ZIP entry.");
        }
        if (size == 0)
        {
            return;
        }
        EnsureStarted();
        _everWritten = true;

        const auto* input = static_cast<const Bytef*>(data);
        while (size > 0)
        {
            const uInt chunk = static_cast<uInt>(std::min<std::size_t>(
                size, static_cast<std::size_t>(std::numeric_limits<uInt>::max())));
            _crc = static_cast<std::uint32_t>(crc32(_crc, input, chunk));
            _uncompressedSize += chunk;
            _stream.next_in = const_cast<Bytef*>(input);
            _stream.avail_in = chunk;
            while (_stream.avail_in > 0)
            {
                _stream.next_out = _buffer.data();
                _stream.avail_out = static_cast<uInt>(_buffer.size());
                const int result = deflate(&_stream, Z_NO_FLUSH);
                if (result != Z_OK)
                {
                    const auto error = std::make_exception_ptr(std::runtime_error(
                        std::string("deflate failed: ") + std::to_string(result)));
                    _owner.MarkFatal(error);
                    std::rethrow_exception(error);
                }
                const std::size_t produced = _buffer.size() - _stream.avail_out;
                if (produced > 0)
                {
                    try
                    {
                        _owner.Output().Write(_buffer.data(), produced);
                    }
                    catch (...)
                    {
                        _owner.MarkFatal(std::current_exception());
                        throw;
                    }
                    _compressedSize += produced;
                }
            }
            input += chunk;
            size -= chunk;
        }
    }

    void ZipEntryStream::Close()
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        if (!_everWritten)
        {
            _owner.CompleteEmptyEntry(_entryIndex);
            return;
        }

        try
        {
            for (;;)
            {
                _stream.next_out = _buffer.data();
                _stream.avail_out = static_cast<uInt>(_buffer.size());
                const int result = deflate(&_stream, Z_FINISH);
                const std::size_t produced = _buffer.size() - _stream.avail_out;
                if (produced > 0)
                {
                    try
                    {
                        _owner.Output().Write(_buffer.data(), produced);
                    }
                    catch (...)
                    {
                        _owner.MarkFatal(std::current_exception());
                        throw;
                    }
                    _compressedSize += produced;
                }
                if (result == Z_STREAM_END)
                {
                    break;
                }
                if (result != Z_OK)
                {
                    throw std::runtime_error(
                        std::string("deflate finish failed: ") + std::to_string(result));
                }
            }
            const int endResult = deflateEnd(&_stream);
            _initialized = false;
            if (endResult != Z_OK)
            {
                throw std::runtime_error(
                    std::string("deflateEnd failed: ") + std::to_string(endResult));
            }
            _owner.CompleteEntry(_entryIndex, _crc, _compressedSize, _uncompressedSize);
        }
        catch (...)
        {
            if (_initialized)
            {
                deflateEnd(&_stream);
                _initialized = false;
            }
            _owner.MarkFatal(std::current_exception());
            throw;
        }
    }

    void ZipEntryStream::CloseNoThrow() noexcept
    {
        if (_closed)
        {
            return;
        }
        try
        {
            Close();
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] bool FileExistsLikeDotNet(const std::filesystem::path& path) noexcept
    {
#if defined(_WIN32)
        std::error_code error;
        const std::filesystem::file_status status = std::filesystem::status(path, error);
        if (error || status.type() == std::filesystem::file_type::not_found)
        {
            return false;
        }
        return status.type() != std::filesystem::file_type::directory;
#else
        struct stat status{};
        if (::lstat(path.c_str(), &status) != 0)
        {
            return false;
        }
        if (S_ISLNK(status.st_mode))
        {
            struct stat target{};
            int statResult = 0;
            do
            {
                statResult = ::stat(path.c_str(), &target);
            }
            while (statResult < 0 && errno == EINTR);
            if (statResult == 0)
            {
                return !S_ISDIR(target.st_mode);
            }
            return true;
        }
        return !S_ISDIR(status.st_mode);
#endif
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
            DotNetSort(files, [](const LogFileInfo& left, const LogFileInfo& right) noexcept
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
            if (FileExistsLikeDotNet(archivePath))
            {
                std::filesystem::remove(archivePath);
            }

            OutputFile stream(archivePath);
            ZipWriter zip(stream);
            bool noReadableLogs = false;
            std::exception_ptr pending;
            try
            {
                int written = 0;
                std::array<std::uint8_t, 81920> buffer{};
                for (const LogFileInfo& file : files)
                {
                    try
                    {
                        InputFile source(file._path);
                        const std::size_t entryIndex = zip.CreateEntry(file._name);
                        zip.LastWriteTime(entryIndex, file.LastWriteTimeUtc());
                        std::unique_ptr<ZipEntryStream> target = zip.Open(entryIndex);
                        try
                        {
                            for (;;)
                            {
                                const std::size_t read = source.Read(buffer.data(), buffer.size());
                                if (read == 0)
                                {
                                    break;
                                }
                                target->Write(buffer.data(), read);
                            }
                            ++written;
                        }
                        catch (...)
                        {
                            target->Close();
                            throw;
                        }
                        target->Close();
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
                zip.Close();
            }
            catch (...)
            {
                pending = std::current_exception();
            }
            try
            {
                stream.Close();
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
        const std::tm local = LocalTime(std::chrono::system_clock::now());
        char timestamp[32]{};
        const int length = std::snprintf(timestamp, sizeof(timestamp),
            "%04d%02d%02d-%02d%02d%02d",
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
            local.tm_hour, local.tm_min, local.tm_sec);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(timestamp))
        {
            throw std::runtime_error("Could not format the current date and time.");
        }
        return Utf8ToUtf16(product + "-logs-" + std::string(timestamp) + ".zip");
    }

    std::atomic<std::shared_ptr<ILogShare>> LogShare::_current{};

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

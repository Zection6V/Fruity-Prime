#include "Memory.hpp"

#include "Formats/Types.hpp"
#include "MemoryClasses.hpp"
#include "Program.hpp"
#include "Scene.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#else
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace
{
    class DllNotFoundException final : public std::runtime_error
    {
    public:
        DllNotFoundException()
            : std::runtime_error("Unable to load shared library 'kernel32.dll'.")
        {
        }
    };

    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    class ArgumentOutOfRangeException final : public std::out_of_range
    {
    public:
        explicit ArgumentOutOfRangeException(std::string parameter)
            : std::out_of_range(
                "Specified argument was out of the range of valid values. (Parameter '"
                + std::move(parameter) + "')")
        {
        }
    };

    class ArgumentException final : public std::invalid_argument
    {
    public:
        ArgumentException()
            : std::invalid_argument(
                "The array starting from the specified index is not long enough.")
        {
        }
    };

    class OverflowException final : public std::overflow_error
    {
    public:
        OverflowException()
            : std::overflow_error("Arithmetic operation resulted in an overflow.")
        {
        }
    };

    template <typename T>
    [[nodiscard]] T& ManagedArrayAt(
        MphRead::ManagedArray<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.Length())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& ManagedArrayAt(
        const MphRead::ManagedArray<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.Length())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] T& VectorArrayAt(std::vector<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.size())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& VectorArrayAt(
        const std::vector<T>& array, std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= array.size())
        {
            throw IndexOutOfRangeException();
        }
        return array[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] constexpr std::int32_t UncheckedAdd32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            + std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedSubtract32(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            - std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] constexpr std::int64_t UncheckedAdd64(
        std::int64_t left, std::int64_t right) noexcept
    {
        const std::uint64_t result = std::bit_cast<std::uint64_t>(left)
            + std::bit_cast<std::uint64_t>(right);
        return std::bit_cast<std::int64_t>(result);
    }

    [[nodiscard]] std::int32_t IntPtrToInt32(std::intptr_t value)
    {
        if (value < static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::min())
            || value > static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw OverflowException();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::intptr_t Int64ToIntPtr(std::int64_t value)
    {
        if constexpr (sizeof(std::intptr_t) < sizeof(std::int64_t))
        {
            if (value < static_cast<std::int64_t>(std::numeric_limits<std::intptr_t>::min())
                || value > static_cast<std::int64_t>(std::numeric_limits<std::intptr_t>::max()))
            {
                throw OverflowException();
            }
        }
        return static_cast<std::intptr_t>(value);
    }

    [[nodiscard]] bool IsAsciiWhiteSpace(char value) noexcept
    {
        switch (value)
        {
        case ' ':
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string_view TrimAsciiWhiteSpace(std::string_view value) noexcept
    {
        while (!value.empty() && IsAsciiWhiteSpace(value.front()))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && IsAsciiWhiteSpace(value.back()))
        {
            value.remove_suffix(1);
        }
        return value;
    }

    [[nodiscard]] bool TryParseInt64Decimal(std::string_view text, std::int64_t& value)
    {
        text = TrimAsciiWhiteSpace(text);
        if (text.empty())
        {
            value = 0;
            return false;
        }

        bool negative = false;
        if (text.front() == '+' || text.front() == '-')
        {
            negative = text.front() == '-';
            text.remove_prefix(1);
        }
        if (text.empty())
        {
            value = 0;
            return false;
        }

        std::uint64_t magnitude = 0;
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), magnitude, 10);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        {
            value = 0;
            return false;
        }

        constexpr std::uint64_t minMagnitude = UINT64_C(0x8000000000000000);
        if (negative)
        {
            if (magnitude > minMagnitude)
            {
                value = 0;
                return false;
            }
            if (magnitude == minMagnitude)
            {
                value = std::numeric_limits<std::int64_t>::min();
            }
            else
            {
                value = -static_cast<std::int64_t>(magnitude);
            }
        }
        else
        {
            if (magnitude > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            {
                value = 0;
                return false;
            }
            value = static_cast<std::int64_t>(magnitude);
        }
        return true;
    }

    void ReplaceAll(std::string& value, std::string_view from, std::string_view to)
    {
        if (from.empty())
        {
            return;
        }
        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos)
        {
            value.replace(position, from.size(), to);
            position += to.size();
        }
    }

    [[nodiscard]] bool TryParseInt64Hex(std::string text, std::int64_t& value)
    {
        ReplaceAll(text, "0x", "");
        std::string_view trimmed = TrimAsciiWhiteSpace(text);
        if (trimmed.empty() || trimmed.size() > 16)
        {
            value = 0;
            return false;
        }

        std::uint64_t bits = 0;
        const auto result = std::from_chars(
            trimmed.data(), trimmed.data() + trimmed.size(), bits, 16);
        if (result.ec != std::errc{} || result.ptr != trimmed.data() + trimmed.size())
        {
            value = 0;
            return false;
        }
        value = std::bit_cast<std::int64_t>(bits);
        return true;
    }

    [[nodiscard]] std::vector<std::string> ReadAllLines(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for reading.");
        }

        std::vector<std::string> result;
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            result.push_back(std::move(line));
        }
        if (stream.bad())
        {
            throw std::ios_base::failure("Failed while reading file.");
        }
        if (!result.empty() && result[0].size() >= 3
            && static_cast<unsigned char>(result[0][0]) == 0xEF
            && static_cast<unsigned char>(result[0][1]) == 0xBB
            && static_cast<unsigned char>(result[0][2]) == 0xBF)
        {
            result[0].erase(0, 3);
        }
        return result;
    }

    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing.");
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::ios_base::failure("Failed while writing file.");
        }
    }

    void WriteAllLines(
        const std::filesystem::path& path, const std::array<std::string, 2>& lines)
    {
        std::ofstream stream(path, std::ios::trunc);
        if (!stream)
        {
            throw std::ios_base::failure("Could not open file for writing.");
        }
        for (const std::string& line : lines)
        {
            stream << line << '\n';
        }
        if (!stream)
        {
            throw std::ios_base::failure("Failed while writing file.");
        }
    }

    [[nodiscard]] std::string FormatPointerHex(std::intptr_t value)
    {
        using UnsignedIntPtr = std::make_unsigned_t<std::intptr_t>;
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2) << static_cast<UnsignedIntPtr>(value);
        return stream.str();
    }

    [[nodiscard]] std::locale CurrentLocale()
    {
        try
        {
            return std::locale("");
        }
        catch (const std::runtime_error&)
        {
            return std::locale::classic();
        }
    }

    [[nodiscard]] std::string FormatIntCurrentCulture(std::int32_t value)
    {
        std::ostringstream stream;
        stream.imbue(CurrentLocale());
        stream << value;
        return stream.str();
    }

    [[nodiscard]] std::string FormatWeightLine(
        std::int32_t weight, float percentage, std::string_view target)
    {
        std::ostringstream stream;
        stream.imbue(CurrentLocale());
        stream << "w: " << std::setw(6) << weight << " / 100000 ("
               << std::setw(5) << std::fixed << std::setprecision(1) << percentage
               << "%) -> " << target;
        return stream.str();
    }

    void AppendEnvironmentNewLine(std::string& value)
    {
#ifdef _WIN32
        value += "\r\n";
#else
        value.push_back('\n');
#endif
    }

    void ClearConsole()
    {
#ifdef _WIN32
        HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (output == nullptr || output == INVALID_HANDLE_VALUE)
        {
            return;
        }
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!::GetConsoleScreenBufferInfo(output, &info))
        {
            return;
        }
        const DWORD cells = static_cast<DWORD>(info.dwSize.X)
            * static_cast<DWORD>(info.dwSize.Y);
        DWORD written = 0;
        const COORD home{0, 0};
        ::FillConsoleOutputCharacterW(output, L' ', cells, home, &written);
        ::FillConsoleOutputAttribute(output, info.wAttributes, cells, home, &written);
        ::SetConsoleCursorPosition(output, home);
#else
        std::cout << "\x1B[2J\x1B[H";
#endif
    }

    struct ProcessCandidate final
    {
        std::int32_t Id = 0;
        std::int64_t StartTimeComparisonTicks = 0;
        std::int64_t StartTimeMilliseconds = 0;
    };

#ifdef _WIN32
    [[nodiscard]] std::int64_t FileTimeToUnixTicks(const FILETIME& time)
    {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        constexpr std::uint64_t UnixEpochFileTime = UINT64_C(116444736000000000);
        if (value.QuadPart >= UnixEpochFileTime)
        {
            return static_cast<std::int64_t>(value.QuadPart - UnixEpochFileTime);
        }
        return -static_cast<std::int64_t>(UnixEpochFileTime - value.QuadPart);
    }

    [[nodiscard]] std::int64_t FileTimeLocalComparisonTicks(const FILETIME& utcTime)
    {
        FILETIME localTime{};
        if (!::FileTimeToLocalFileTime(&utcTime, &localTime))
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }
        ULARGE_INTEGER value{};
        value.LowPart = localTime.dwLowDateTime;
        value.HighPart = localTime.dwHighDateTime;
        if (value.QuadPart > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(value.QuadPart);
    }

    [[nodiscard]] std::int64_t UnixTicksToMilliseconds(std::int64_t ticks)
    {
        if (ticks >= 0)
        {
            return ticks / 10000;
        }
        // DateTimeOffset.ToUnixTimeMilliseconds floors toward negative infinity.
        return -static_cast<std::int64_t>(
            (static_cast<std::uint64_t>(-(ticks + 1)) + 1U + 9999U) / 10000U);
    }

    [[nodiscard]] bool EqualsProcessName(std::wstring_view executable)
    {
        std::wstring name(executable);
        const std::size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos
            && _wcsicmp(name.substr(dot).c_str(), L".exe") == 0)
        {
            name.resize(dot);
        }
        return _wcsicmp(name.c_str(), L"NO$GBA") == 0;
    }

    [[nodiscard]] std::pair<std::int64_t, std::int64_t>
        QueryProcessStartTime(std::int32_t processId)
    {
        HANDLE handle = ::OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
            static_cast<DWORD>(processId));
        if (handle == nullptr)
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }

        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!::GetProcessTimes(handle, &creation, &exit, &kernel, &user))
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(handle);
            throw std::system_error(static_cast<int>(error), std::system_category());
        }
        ::CloseHandle(handle);
        const std::int64_t unixTicks = FileTimeToUnixTicks(creation);
        return {
            FileTimeLocalComparisonTicks(creation),
            UnixTicksToMilliseconds(unixTicks)
        };
    }

    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            throw std::system_error(
                static_cast<int>(::GetLastError()), std::system_category());
        }

        std::vector<ProcessCandidate> result;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (::Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (EqualsProcessName(entry.szExeFile))
                {
                    const auto processId = static_cast<std::int32_t>(entry.th32ProcessID);
                    const auto [ticks, milliseconds]
                        = QueryProcessStartTime(processId);
                    result.push_back({
                        processId,
                        ticks,
                        milliseconds
                    });
                }
            }
            while (::Process32NextW(snapshot, &entry));
        }
        else
        {
            const DWORD error = ::GetLastError();
            if (error != ERROR_NO_MORE_FILES)
            {
                ::CloseHandle(snapshot);
                throw std::system_error(static_cast<int>(error), std::system_category());
            }
        }
        ::CloseHandle(snapshot);
        return result;
    }
#elif defined(__linux__)
    [[nodiscard]] bool TryParsePid(std::string_view text, std::int32_t& processId)
    {
        if (text.empty())
        {
            return false;
        }
        std::int64_t parsed = 0;
        if (!TryParseInt64Decimal(text, parsed)
            || parsed <= 0
            || parsed > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        processId = static_cast<std::int32_t>(parsed);
        return true;
    }

    [[nodiscard]] std::int64_t LinuxBootTimeMilliseconds()
    {
        std::ifstream stream("/proc/stat");
        if (!stream)
        {
            throw std::ios_base::failure("Could not read /proc/stat.");
        }
        std::string key;
        while (stream >> key)
        {
            if (key == "btime")
            {
                std::int64_t seconds = 0;
                stream >> seconds;
                return seconds * 1000;
            }
            std::string rest;
            std::getline(stream, rest);
        }
        throw std::runtime_error("Could not determine system boot time.");
    }

    [[nodiscard]] std::int64_t LinuxProcessStartTimeMilliseconds(
        std::int32_t processId, std::int64_t bootTimeMilliseconds)
    {
        const std::filesystem::path path
            = std::filesystem::path("/proc") / std::to_string(processId) / "stat";
        std::ifstream stream(path);
        if (!stream)
        {
            throw std::ios_base::failure("Could not read process start time.");
        }
        std::string line;
        std::getline(stream, line);
        const std::size_t closeParen = line.rfind(')');
        if (closeParen == std::string::npos || closeParen + 2 >= line.size())
        {
            throw std::runtime_error("Invalid /proc process stat data.");
        }

        std::istringstream fields(line.substr(closeParen + 2));
        std::string field;
        std::uint64_t startTicks = 0;
        // The first token after ')' is field 3 (state); starttime is field 22.
        for (std::int32_t fieldNumber = 3; fieldNumber <= 22; ++fieldNumber)
        {
            if (!(fields >> field))
            {
                throw std::runtime_error("Invalid /proc process stat data.");
            }
            if (fieldNumber == 22)
            {
                const auto parsed = std::from_chars(
                    field.data(), field.data() + field.size(), startTicks, 10);
                if (parsed.ec != std::errc{}
                    || parsed.ptr != field.data() + field.size())
                {
                    throw std::runtime_error("Invalid /proc process start time.");
                }
            }
        }

        const long ticksPerSecond = ::sysconf(_SC_CLK_TCK);
        if (ticksPerSecond <= 0)
        {
            throw std::runtime_error("Could not determine clock tick frequency.");
        }
        return bootTimeMilliseconds
            + static_cast<std::int64_t>(
                startTicks * 1000U / static_cast<std::uint64_t>(ticksPerSecond));
    }

    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        DIR* directory = ::opendir("/proc");
        if (directory == nullptr)
        {
            throw std::system_error(errno, std::generic_category());
        }

        std::vector<ProcessCandidate> result;
        std::optional<std::int64_t> bootTime;
        while (dirent* entry = ::readdir(directory))
        {
            std::int32_t processId = 0;
            if (!TryParsePid(entry->d_name, processId))
            {
                continue;
            }

            const std::filesystem::path commPath
                = std::filesystem::path("/proc") / entry->d_name / "comm";
            std::ifstream comm(commPath);
            if (!comm)
            {
                continue;
            }
            std::string name;
            std::getline(comm, name);
            if (name != "NO$GBA")
            {
                continue;
            }

            if (!bootTime)
            {
                bootTime = LinuxBootTimeMilliseconds();
            }
            const std::int64_t milliseconds
                = LinuxProcessStartTimeMilliseconds(processId, *bootTime);
            result.push_back({
                processId,
                UncheckedAdd64(0, milliseconds * INT64_C(10000)),
                milliseconds
            });
        }
        ::closedir(directory);
        return result;
    }
#else
    [[nodiscard]] std::vector<ProcessCandidate> FindProcessesByName()
    {
        return {};
    }
#endif

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }
}

namespace MphRead::Memory
{
    std::shared_ptr<Memory::AddressInfo> Memory::Addresses{};

    const std::array<
        std::pair<std::string_view, std::shared_ptr<Memory::AddressInfo>>, 2>
        Memory::AllAddresses = {{
            {
                "a76e",
                std::make_shared<AddressInfo>(
                    0x020BC420,
                    0x020B85F8,
                    0x020AE514,
                    0x020B00D4,
                    0x020B00D4,
                    0x02103760,
                    0x020B84C4,
                    0x020E228C,
                    std::make_shared<AddressInfo::SaveAddressInfo>(
                        0x020BD798,
                        0x020D958C,
                        0x020BC364,
                        0x020EB948,
                        0x020ECEE0))
            },
            {
                "amhp1",
                std::make_shared<AddressInfo>(
                    0x020E845C,
                    0x020E3EE0,
                    0x020D94FC,
                    0x020DB034,
                    0x020DB180,
                    0x021335E0,
                    0x020B84C4,
                    0x020E228C,
                    std::make_shared<AddressInfo::SaveAddressInfo>(
                        0x020E97B0,
                        0x020D958C,
                        0x020E83B8,
                        0x020EB948,
                        0x020ECEE0))
            }
        }};

    Memory::AddressInfo::SaveAddressInfo::SaveAddressInfo(
        std::int32_t story,
        std::int32_t type3,
        std::int32_t settings,
        std::int32_t license,
        std::int32_t friends) noexcept
        : Story(story),
          Type3(type3),
          Settings(settings),
          License(license),
          Friends(friends)
    {
    }

    Memory::AddressInfo::AddressInfo(
        std::int32_t gameState,
        std::int32_t entityListHead,
        std::int32_t frameCount,
        std::int32_t players,
        std::int32_t playerUa,
        std::int32_t camSeqData,
        std::int32_t roomDesc,
        std::int32_t rng2,
        std::shared_ptr<SaveAddressInfo> save) noexcept
        : EntityListHead(entityListHead),
          FrameCount(frameCount),
          PlayerUA(playerUa),
          Players(players),
          CamSeqData(camSeqData),
          GameState(gameState),
          RoomDesc(roomDesc),
          Rng2(rng2),
          Save(std::move(save))
    {
    }

    Memory::Memory(
        std::int32_t processId,
        std::int64_t processStartTimeMilliseconds,
        ::MphRead::Scene* scene)
        : _aggroItems(
              std::make_shared<ManagedArray<std::shared_ptr<AIAggro>>>(25)),
          _scene(scene),
          _processId(processId),
          _processStartTimeMilliseconds(processStartTimeMilliseconds),
          _buffer(std::make_shared<ManagedArray<std::uint8_t>>(
              static_cast<std::size_t>(_size))),
          _players(
              std::make_shared<ManagedArray<std::shared_ptr<CPlayer>>>(4))
    {
    }

    Memory::~Memory()
    {
#ifdef _WIN32
        if (_processHandle != 0)
        {
            ::CloseHandle(reinterpret_cast<HANDLE>(_processHandle));
        }
#endif
    }

    std::shared_ptr<ManagedArray<std::uint8_t>> Memory::Buffer() const noexcept
    {
        return _buffer;
    }

    std::shared_ptr<std::shared_future<void>> Memory::Task() const noexcept
    {
        return _task;
    }

    std::shared_ptr<Memory> Memory::Start(::MphRead::Scene* scene, bool blocking)
    {
        std::optional<ProcessCandidate> foundProcess;
        // Process.StartTime is compared as a DateTime before the selected value is
        // later converted to Unix milliseconds for memory.txt.
        std::int64_t startTimeTicks = std::numeric_limits<std::int64_t>::min();
        const std::vector<ProcessCandidate> processes = FindProcessesByName();
        for (const ProcessCandidate& process : processes)
        {
            if (process.StartTimeComparisonTicks > startTimeTicks)
            {
                foundProcess = process;
                startTimeTicks = process.StartTimeComparisonTicks;
            }
        }
        if (!foundProcess)
        {
            throw ProgramException("Could not find process.");
        }

        auto memory = std::shared_ptr<Memory>(new Memory(
            foundProcess->Id, foundProcess->StartTimeMilliseconds, scene));
        memory->Run(blocking);
        return memory;
    }

    void Memory::SetBaseAddress()
    {
        const std::filesystem::path path("memory.txt");
        std::error_code existsError;
        const bool exists = std::filesystem::is_regular_file(path, existsError);
        if (!exists || existsError)
        {
            WriteAllText(path, "");
        }

        const std::int64_t startTime = _processStartTimeMilliseconds;
        const std::vector<std::string> lines = ReadAllLines(path);
        std::int64_t timestamp = 0;
        std::int64_t saved = 0;
        if (lines.size() >= 2
            && TryParseInt64Decimal(lines[0], timestamp)
            && startTime == timestamp
            && TryParseInt64Hex(lines[1], saved))
        {
            _baseAddress = Int64ToIntPtr(saved);
            return;
        }

        std::cout << "Scanning memory..." << std::endl;

        const std::array<std::uint8_t, 12> search{
            0xFF, 0xDE, 0xFF, 0xE7,
            0xFF, 0xDE, 0xFF, 0xE7,
            0xFF, 0xDE, 0xFF, 0xE7
        };

        SystemInfo systemInfo{};
        GetSystemInfo(systemInfo);
        std::intptr_t minAddr = systemInfo.MinimumApplicationAddress;
        const std::intptr_t maxAddr = systemInfo.MaximumApplicationAddress;
        const std::intptr_t processHandle = OpenProcess(
            0x10 | 0x400, false, _processId);
        MemoryInfo64 memoryInfo{};

        while (static_cast<std::int64_t>(minAddr)
            < static_cast<std::int64_t>(maxAddr))
        {
            static_cast<void>(VirtualQueryEx(
                processHandle, minAddr, memoryInfo, 48));
            if (memoryInfo.Protect == 4 && memoryInfo.State == 0x1000)
            {
                if (memoryInfo.RegionSize < 0)
                {
                    throw std::overflow_error(
                        "Array dimensions exceeded supported range.");
                }

                std::vector<std::uint8_t> buffer(
                    static_cast<std::size_t>(memoryInfo.RegionSize));
                const std::intptr_t baseAddr = Int64ToIntPtr(memoryInfo.BaseAddress);
                std::intptr_t count = 0;
                const bool result = ReadProcessMemory(
                    processHandle,
                    baseAddr,
                    buffer.empty() ? nullptr : buffer.data(),
                    static_cast<std::int32_t>(
                        static_cast<std::uint32_t>(memoryInfo.RegionSize)),
                    count);
                assert(result);
                assert(static_cast<std::int64_t>(count) == memoryInfo.RegionSize);

                const std::int64_t lastStart
                    = memoryInfo.RegionSize - static_cast<std::int64_t>(search.size());
                for (std::int32_t i = 0;
                     static_cast<std::int64_t>(i) <= lastStart;
                     i = UncheckedAdd32(i, 1))
                {
                    bool equal = true;
                    for (std::size_t j = 0; j < search.size(); ++j)
                    {
                        if (buffer.at(static_cast<std::size_t>(i) + j) != search[j])
                        {
                            equal = false;
                            break;
                        }
                    }

                    if (equal)
                    {
                        const std::int32_t zeroIndex = UncheckedSubtract32(i, 0x4000);
                        const std::int32_t zeroIndex1 = UncheckedAdd32(zeroIndex, 1);
                        if (VectorArrayAt(buffer, zeroIndex) == 0
                            && VectorArrayAt(buffer, zeroIndex1) == 0)
                        {
                            const std::int64_t found = UncheckedAdd64(
                                memoryInfo.BaseAddress,
                                static_cast<std::int64_t>(zeroIndex));
                            _baseAddress = Int64ToIntPtr(found);
                            WriteAllLines(path, {
                                std::to_string(startTime),
                                FormatPointerHex(_baseAddress)
                            });
                            // The C# source never closes this OpenProcess handle.
                            static_cast<void>(processHandle);
                            return;
                        }
                    }
                }
            }

            if (memoryInfo.RegionSize == 0)
            {
                throw ProgramException("Failed to scan memory.");
            }
            minAddr = Int64ToIntPtr(UncheckedAdd64(
                static_cast<std::int64_t>(minAddr), memoryInfo.RegionSize));
        }

        throw ProgramException("Failed to find search sequence.");
    }

    void Memory::Run(bool blocking)
    {
        std::shared_ptr<AddressInfo> addresses;
        for (const auto& item : AllAddresses)
        {
            if (item.first == "amhp1")
            {
                addresses = item.second;
                break;
            }
        }
        if (!addresses)
        {
            throw std::out_of_range("The given key was not present in the dictionary.");
        }
        Addresses = std::move(addresses);

        SetBaseAddress();

        auto promise = std::make_shared<std::promise<void>>();
        _task = std::make_shared<std::shared_future<void>>(
            promise->get_future().share());
        std::shared_ptr<Memory> self = shared_from_this();
        std::thread([self = std::move(self), promise = std::move(promise)]() mutable
        {
            try
            {
                self->RunTaskBody();
                promise->set_value();
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        }).detach();

        if (blocking)
        {
            _task->get();
        }
    }

    void Memory::RunTaskBody()
    {
        std::string output;
        RefreshMemory();
        (*_players)[0] = std::make_shared<CPlayer>(*this, Addresses->Players);
        (*_players)[1] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(Addresses->Players, 0xF30));
        (*_players)[2] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(
                Addresses->Players,
                static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(0xF30) * UINT32_C(2))));
        (*_players)[3] = std::make_shared<CPlayer>(
            *this, UncheckedAdd32(
                Addresses->Players,
                static_cast<std::int32_t>(
                    static_cast<std::uint32_t>(0xF30) * UINT32_C(3))));

        while (_scene == nullptr || !_scene->Exiting())
        {
            _sb.clear();
            RefreshMemory();
            DoProcess();
            const std::string newOutput = _sb;
            if (newOutput != output)
            {
                output = newOutput;
                ClearConsole();
                std::cout << output;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }

    void Memory::RefreshMemory()
    {
        const std::intptr_t processHandle = ProcessHandle();
        std::vector<std::uint8_t> buffer(static_cast<std::size_t>(_size));
        for (std::int32_t i = 0; i < _size; ++i)
        {
            buffer[static_cast<std::size_t>(i)]
                = (*_buffer)[static_cast<std::size_t>(i)];
        }

        std::intptr_t count = 0;
        const bool result = ReadProcessMemory(
            processHandle,
            _baseAddress,
            buffer.data(),
            _size,
            count);

        for (std::int32_t i = 0; i < _size; ++i)
        {
            (*_buffer)[static_cast<std::size_t>(i)]
                = buffer[static_cast<std::size_t>(i)];
        }

        assert(result);
        assert(static_cast<std::int64_t>(count) == _size);
    }

    void Memory::DoProcess()
    {
        const std::uint32_t aggroCount
            = RequireReference((*_players)[1]).AggroCount();
        static_cast<void>(aggroCount);

        const auto list = RequireReference((*_players)[1]).AIAggro();
        assert(list != nullptr);
        if (!list)
        {
            throw System::NullReferenceException();
        }

        for (std::int32_t i = 0; i < list->Length(); ++i)
        {
            const std::shared_ptr<AIAggro> item = list->Item(i);
            RequireReference(item).UpdateSlots(_players);
            ManagedArrayAt(*_aggroItems, i) = item;
        }

        static_cast<void>(5);
    }

    void Memory::PrintAiContext()
    {
        const auto context = RequireReference((*_players)[1]).AIContext();
        assert(context != nullptr);
        if (!context)
        {
            throw System::NullReferenceException();
        }

        std::string tree;
        for (std::int32_t i = 1; i < 20; ++i)
        {
            const std::shared_ptr<AIContext> item = context->Item(i);
            const std::shared_ptr<AIData1> firstItemData1
                = RequireReference(item).AIData1();
            if (!firstItemData1)
            {
                break;
            }

            const std::shared_ptr<AIContext> parent
                = context->Item(UncheckedSubtract32(i, 1));
            std::int32_t childIndex = -1;

            for (std::int32_t j = 0;
                 j < RequireReference(
                     RequireReference(parent).AIData1()).Data1Count();
                 ++j)
            {
                const auto parentData1ForArray
                    = RequireReference(parent).AIData1();
                const auto parentData1Array
                    = RequireReference(parentData1ForArray).Data1();
                const std::shared_ptr<AIData1> parentChild
                    = RequireReference(parentData1Array).Item(j);

                const auto itemData1ForAddress
                    = RequireReference(item).AIData1();
                if (RequireReference(parentChild).Address()
                    == RequireReference(itemData1ForAddress).Address())
                {
                    childIndex = j;
                    break;
                }
            }

            if (!tree.empty())
            {
                tree += " -> ";
            }
            tree += FormatIntCurrentCulture(childIndex);
            _sb += "d";
            _sb += FormatIntCurrentCulture(i);
            _sb += ": ";
            _sb += FormatIntCurrentCulture(childIndex);
            AppendEnvironmentNewLine(_sb);

            const auto parentData1ForOptional
                = RequireReference(parent).AIData1();
            if (parentData1ForOptional
                && parentData1ForOptional->Data1Count() > 1)
            {
                for (std::int32_t k = 0;
                     k < RequireReference(
                         RequireReference(item).AIData1()).Data2Count();
                     ++k)
                {
                    const auto itemData1ForData2
                        = RequireReference(item).AIData1();
                    const auto data2
                        = RequireReference(itemData1ForData2).Data2();
                    std::int32_t index
                        = RequireReference(
                            RequireReference(data2).Item(k)).Data1SelectIdx();

                    std::string target;
                    if (index >= 20)
                    {
                        index = RequireReference(
                            RequireReference(parent).AIData1()).Data1Count();
                        target = "reset";
                    }
                    else
                    {
                        target = FormatIntCurrentCulture(index);
                    }

                    const std::int32_t weight
                        = RequireReference(RequireReference(parent).Weights()).Item(index);
                    const float percentage
                        = static_cast<float>(weight) / 100000.0F * 100.0F;
                    _sb += FormatWeightLine(weight, percentage, target);
                    AppendEnvironmentNewLine(_sb);
                }
            }

            if (RequireReference(
                    RequireReference(item).AIData1()).Data1Count() == 0)
            {
                break;
            }

            AppendEnvironmentNewLine(_sb);
            static_cast<void>(5);
        }

        const std::int32_t frameCount = ReadInt32FromBuffer(
            UncheckedSubtract32(Addresses->FrameCount, 0x02000000));
        bool add = _mem.empty();
        if (!add)
        {
            const std::string& last = _mem.back();
            const std::size_t separator = last.find(": ");
            const std::string previousTree = separator == std::string::npos
                ? std::string()
                : last.substr(separator + 2);
            add = previousTree != tree;
        }
        if (add)
        {
            _mem.push_back(
                FormatIntCurrentCulture(frameCount) + ": " + tree);
        }

        AppendEnvironmentNewLine(_sb);
        for (const std::string& line : _mem)
        {
            _sb += line;
            AppendEnvironmentNewLine(_sb);
        }
    }

    void Memory::GetEntities()
    {
        _temp.clear();
        for (const std::shared_ptr<CEntity>& entity : _entities)
        {
            CEntity& value = RequireReference(entity);
            const std::intptr_t address = value.Address();
            const auto [iterator, inserted] = _temp.emplace(address, entity);
            static_cast<void>(iterator);
            if (!inserted)
            {
                throw std::invalid_argument(
                    "An item with the same key has already been added.");
            }
        }

        _entities.clear();
        const std::shared_ptr<CEntity> head = GetEntity(Addresses->EntityListHead);
        assert(RequireReference(head).EntityType() == MphRead::EntityType::ListHead);
        _entities.push_back(head);

        std::intptr_t nextAddr = RequireReference(head).Next();
        while (nextAddr != RequireReference(head).Address())
        {
            std::shared_ptr<CEntity> entity;
            const auto cached = _temp.find(nextAddr);
            if (cached != _temp.end())
            {
                entity = cached->second;
            }

            if (entity
                && RequireReference(entity).EntityType()
                    == static_cast<MphRead::EntityType>(
                        ReadUInt16FromBuffer(UncheckedSubtract32(
                            IntPtrToInt32(nextAddr), Offset))))
            {
                _entities.push_back(entity);
            }
            else
            {
                entity = GetEntity(nextAddr);
                _entities.push_back(entity);
            }

            nextAddr = RequireReference(entity).Next();
        }
    }

    void Memory::WriteMemory(
        std::intptr_t address,
        std::shared_ptr<ManagedArray<std::uint8_t>> value,
        std::int32_t size)
    {
        WriteMemory(IntPtrToInt32(address), std::move(value), size);
    }

    void Memory::WriteMemory(
        std::int32_t address,
        std::shared_ptr<ManagedArray<std::uint8_t>> value,
        std::int32_t size)
    {
        const std::int32_t offset = UncheckedSubtract32(address, Offset);
        const std::int32_t pointerValue
            = UncheckedAdd32(IntPtrToInt32(_baseAddress), offset);
        const std::intptr_t pointer = static_cast<std::intptr_t>(pointerValue);
        const std::intptr_t processHandle = ProcessHandle();

        std::vector<std::uint8_t> marshaled;
        const std::uint8_t* source = nullptr;
        if (value)
        {
            const std::size_t valueLength = value->Length();
            std::size_t nativeLength = valueLength;
            if (size > 0
                && static_cast<std::uint64_t>(size) > nativeLength)
            {
                // The C# P/Invoke signature does not tie nSize to the array length.
                // Keep the native call safe while preserving the subsequent managed
                // array bounds failure and the requested nSize.
                nativeLength = static_cast<std::size_t>(size);
            }
            marshaled.resize(nativeLength);
            for (std::size_t i = 0; i < valueLength; ++i)
            {
                marshaled[i] = (*value)[i];
            }
            source = marshaled.empty() ? nullptr : marshaled.data();
        }

        std::intptr_t count = 0;
        const bool result = WriteProcessMemory(
            processHandle, pointer, source, size, count);
        assert(result);
        assert(IntPtrToInt32(count) == size);

        for (std::int32_t i = 0; i < size; ++i)
        {
            const std::int32_t destinationIndex = UncheckedAdd32(offset, i);
            std::uint8_t& destination
                = ManagedArrayAt(*_buffer, destinationIndex);
            if (!value)
            {
                throw System::NullReferenceException();
            }
            destination = ManagedArrayAt(*value, i);
        }
    }

    std::shared_ptr<CEntity> Memory::GetEntity(std::intptr_t address)
    {
        return GetEntity(IntPtrToInt32(address));
    }

    std::shared_ptr<CEntity> Memory::GetEntity(std::int32_t address)
    {
        const std::int32_t offset = UncheckedSubtract32(address, Offset);
        const auto type = static_cast<MphRead::EntityType>(
            ReadUInt16FromBuffer(offset));

        if (type == MphRead::EntityType::Platform)
        {
            return std::make_shared<CPlatform>(*this, address);
        }
        if (type == MphRead::EntityType::Object)
        {
            return std::make_shared<CObject>(*this, address);
        }
        if (type == MphRead::EntityType::PlayerSpawn)
        {
            return std::make_shared<CPlayerSpawn>(*this, address);
        }
        if (type == MphRead::EntityType::Door)
        {
            return std::make_shared<CDoor>(*this, address);
        }
        if (type == MphRead::EntityType::ItemSpawn)
        {
            return std::make_shared<CItemSpawn>(*this, address);
        }
        if (type == MphRead::EntityType::ItemInstance)
        {
            return std::make_shared<CItemInstance>(*this, address);
        }
        if (type == MphRead::EntityType::EnemySpawn)
        {
            return std::make_shared<CEnemySpawn>(*this, address);
        }
        if (type == MphRead::EntityType::TriggerVolume)
        {
            return std::make_shared<CTriggerVolume>(*this, address);
        }
        if (type == MphRead::EntityType::AreaVolume)
        {
            return std::make_shared<CAreaVolume>(*this, address);
        }
        if (type == MphRead::EntityType::JumpPad)
        {
            return std::make_shared<CJumpPad>(*this, address);
        }
        if (type == MphRead::EntityType::PointModule)
        {
            return std::make_shared<CPointModule>(*this, address);
        }
        if (type == MphRead::EntityType::MorphCamera)
        {
            return std::make_shared<CMorphCamera>(*this, address);
        }
        if (type == MphRead::EntityType::OctolithFlag)
        {
            return std::make_shared<COctolithFlag>(*this, address);
        }
        if (type == MphRead::EntityType::FlagBase)
        {
            return std::make_shared<CFlagBase>(*this, address);
        }
        if (type == MphRead::EntityType::Teleporter)
        {
            return std::make_shared<CTeleporter>(*this, address);
        }
        if (type == MphRead::EntityType::NodeDefense)
        {
            return std::make_shared<CNodeDefense>(*this, address);
        }
        if (type == MphRead::EntityType::LightSource)
        {
            return std::make_shared<CLightSource>(*this, address);
        }
        if (type == MphRead::EntityType::Artifact)
        {
            return std::make_shared<CArtifact>(*this, address);
        }
        if (type == MphRead::EntityType::CameraSequence)
        {
            return std::make_shared<CCameraSequence>(*this, address);
        }
        if (type == MphRead::EntityType::ForceField)
        {
            return std::make_shared<CForceField>(*this, address);
        }
        if (type == MphRead::EntityType::BeamEffect)
        {
            return std::make_shared<CBeamEffect>(*this, address);
        }
        if (type == MphRead::EntityType::Bomb)
        {
            return std::make_shared<CBomb>(*this, address);
        }
        if (type == MphRead::EntityType::EnemyInstance)
        {
            const std::shared_ptr<CEnemyBase> enemy
                = std::make_shared<CEnemyBase>(*this, address);
            switch (RequireReference(enemy).Type())
            {
            case MphRead::EnemyType::Gorea1A:
                return std::make_shared<CEnemy24>(*this, address);
            case MphRead::EnemyType::GoreaHead:
                return std::make_shared<CEnemy25>(*this, address);
            case MphRead::EnemyType::GoreaArm:
                return std::make_shared<CEnemy26>(*this, address);
            case MphRead::EnemyType::GoreaLeg:
                return std::make_shared<CEnemy27>(*this, address);
            case MphRead::EnemyType::Gorea1B:
                return std::make_shared<CEnemy28>(*this, address);
            case MphRead::EnemyType::GoreaSealSphere1:
                return std::make_shared<CEnemy29>(*this, address);
            case MphRead::EnemyType::Trocra:
                return std::make_shared<CEnemy30>(*this, address);
            default:
                return enemy;
            }
        }
        if (type == MphRead::EntityType::Halfturret)
        {
            return std::make_shared<CHalfturret>(*this, address);
        }
        if (type == MphRead::EntityType::Player)
        {
            return std::make_shared<CPlayer>(*this, address);
        }
        if (type == MphRead::EntityType::BeamProjectile)
        {
            return std::make_shared<CBeamProjectile>(*this, address);
        }
        return std::make_shared<CEntity>(*this, address);
    }

    std::intptr_t Memory::ProcessHandle()
    {
#ifdef _WIN32
        if (_processHandle == 0)
        {
            HANDLE handle = ::OpenProcess(
                PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(_processId));
            if (handle == nullptr)
            {
                throw std::system_error(
                    static_cast<int>(::GetLastError()), std::system_category());
            }
            _processHandle = reinterpret_cast<std::intptr_t>(handle);
        }
        return _processHandle;
#else
        // System.Diagnostics.Process.Handle on Unix is a manufactured wait handle.
        // Preserve its pre-P/Invoke process-exit check; if the process is still
        // present, the exact handle bits are immaterial because the immediately
        // following kernel32.dll P/Invoke is the observable failure boundary.
        if (::kill(static_cast<pid_t>(_processId), 0) != 0 && errno == ESRCH)
        {
            throw std::runtime_error("Process has exited.");
        }
        return static_cast<std::intptr_t>(_processId == 0 ? 1 : _processId);
#endif
    }

    std::uint16_t Memory::ReadUInt16FromBuffer(std::int32_t offset) const
    {
        const std::int32_t length = static_cast<std::int32_t>(_buffer->Length());
        if (offset < 0 || offset >= length)
        {
            throw ArgumentOutOfRangeException("startIndex");
        }
        if (offset > length - 2)
        {
            throw ArgumentException();
        }

        const std::uint8_t b0 = (*_buffer)[static_cast<std::size_t>(offset)];
        const std::uint8_t b1
            = (*_buffer)[static_cast<std::size_t>(offset + 1)];
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(b0)
            | (static_cast<std::uint16_t>(b1) << 8U));
    }

    std::int32_t Memory::ReadInt32FromBuffer(std::int32_t offset) const
    {
        const std::int32_t length = static_cast<std::int32_t>(_buffer->Length());
        if (offset < 0 || offset >= length)
        {
            throw ArgumentOutOfRangeException("startIndex");
        }
        if (offset > length - 4)
        {
            throw ArgumentException();
        }

        const std::uint32_t b0 = (*_buffer)[static_cast<std::size_t>(offset)];
        const std::uint32_t b1 = (*_buffer)[static_cast<std::size_t>(offset + 1)];
        const std::uint32_t b2 = (*_buffer)[static_cast<std::size_t>(offset + 2)];
        const std::uint32_t b3 = (*_buffer)[static_cast<std::size_t>(offset + 3)];
        const std::uint32_t bits
            = b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
        return std::bit_cast<std::int32_t>(bits);
    }

    void Memory::GetSystemInfo(SystemInfo& lpSystemInfo)
    {
#ifdef _WIN32
        SYSTEM_INFO info{};
        ::GetSystemInfo(&info);
        lpSystemInfo.ProcessorArchitecture = info.wProcessorArchitecture;
        lpSystemInfo.Reserved = info.wReserved;
        lpSystemInfo.PageSize = info.dwPageSize;
        lpSystemInfo.MinimumApplicationAddress
            = reinterpret_cast<std::intptr_t>(info.lpMinimumApplicationAddress);
        lpSystemInfo.MaximumApplicationAddress
            = reinterpret_cast<std::intptr_t>(info.lpMaximumApplicationAddress);
        lpSystemInfo.ActiveProcessorMask
            = static_cast<std::intptr_t>(info.dwActiveProcessorMask);
        lpSystemInfo.NumberOfProcessors = info.dwNumberOfProcessors;
        lpSystemInfo.ProcessorType = info.dwProcessorType;
        lpSystemInfo.AllocationGranularity = info.dwAllocationGranularity;
        lpSystemInfo.ProcessorLevel = info.wProcessorLevel;
        lpSystemInfo.ProcessorRevision = info.wProcessorRevision;
#else
        static_cast<void>(lpSystemInfo);
        throw DllNotFoundException();
#endif
    }

    std::intptr_t Memory::OpenProcess(
        std::int32_t dwDesiredAccess,
        bool bInheritHandle,
        std::int32_t dwProcessId)
    {
#ifdef _WIN32
        HANDLE handle = ::OpenProcess(
            static_cast<DWORD>(dwDesiredAccess),
            bInheritHandle ? TRUE : FALSE,
            static_cast<DWORD>(dwProcessId));
        return reinterpret_cast<std::intptr_t>(handle);
#else
        static_cast<void>(dwDesiredAccess);
        static_cast<void>(bInheritHandle);
        static_cast<void>(dwProcessId);
        throw DllNotFoundException();
#endif
    }

    std::int32_t Memory::VirtualQueryEx(
        std::intptr_t hProcess,
        std::intptr_t lpAddress,
        MemoryInfo64& lpBuffer,
        std::uint32_t dwLength)
    {
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION info{};
        const SIZE_T result = ::VirtualQueryEx(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPCVOID>(lpAddress),
            &info,
            static_cast<SIZE_T>(dwLength));
        if (result != 0)
        {
            lpBuffer.BaseAddress = static_cast<std::int64_t>(
                reinterpret_cast<std::intptr_t>(info.BaseAddress));
            lpBuffer.AllocationBase = static_cast<std::int64_t>(
                reinterpret_cast<std::intptr_t>(info.AllocationBase));
            lpBuffer.AllocationProtect
                = static_cast<std::int32_t>(info.AllocationProtect);
            lpBuffer.Padding1 = 0;
            lpBuffer.RegionSize = static_cast<std::int64_t>(info.RegionSize);
            lpBuffer.State = static_cast<std::int32_t>(info.State);
            lpBuffer.Protect = static_cast<std::int32_t>(info.Protect);
            lpBuffer.lType = static_cast<std::int32_t>(info.Type);
            lpBuffer.Padding2 = 0;
        }
        return static_cast<std::int32_t>(result);
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(dwLength);
        throw DllNotFoundException();
#endif
    }

    bool Memory::ReadProcessMemory(
        std::intptr_t hProcess,
        std::intptr_t lpBaseAddress,
        std::uint8_t* lpBuffer,
        std::int32_t nSize,
        std::intptr_t& lpNumberOfBytesRead)
    {
#ifdef _WIN32
        SIZE_T count = 0;
        const BOOL result = ::ReadProcessMemory(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPCVOID>(lpBaseAddress),
            lpBuffer,
            static_cast<SIZE_T>(static_cast<std::uint32_t>(nSize)),
            &count);
        lpNumberOfBytesRead = static_cast<std::intptr_t>(count);
        return result != FALSE;
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpBaseAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(nSize);
        static_cast<void>(lpNumberOfBytesRead);
        throw DllNotFoundException();
#endif
    }

    bool Memory::WriteProcessMemory(
        std::intptr_t hProcess,
        std::intptr_t lpBaseAddress,
        const std::uint8_t* lpBuffer,
        std::int32_t nSize,
        std::intptr_t& lpNumberOfBytesRead)
    {
#ifdef _WIN32
        SIZE_T count = 0;
        const BOOL result = ::WriteProcessMemory(
            reinterpret_cast<HANDLE>(hProcess),
            reinterpret_cast<LPVOID>(lpBaseAddress),
            lpBuffer,
            static_cast<SIZE_T>(static_cast<std::uint32_t>(nSize)),
            &count);
        lpNumberOfBytesRead = static_cast<std::intptr_t>(count);
        return result != FALSE;
#else
        static_cast<void>(hProcess);
        static_cast<void>(lpBaseAddress);
        static_cast<void>(lpBuffer);
        static_cast<void>(nSize);
        static_cast<void>(lpNumberOfBytesRead);
        throw DllNotFoundException();
#endif
    }

    std::uint32_t Memory::GetLastError()
    {
#ifdef _WIN32
        return static_cast<std::uint32_t>(::GetLastError());
#else
        throw DllNotFoundException();
#endif
    }
}

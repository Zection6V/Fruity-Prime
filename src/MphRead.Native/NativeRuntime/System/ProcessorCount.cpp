#include "Runtime.hpp"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#endif
#endif

// Environment.ProcessorCount as the runtime computes it: DOTNET_PROCESSOR_COUNT
// if set, else the processors this process may run on -- its affinity, and on
// Linux the cgroup CPU quota -- and never less than one.

namespace MphRead::NativeRuntime
{
    namespace
    {
    const char* DotNetConfigValue(
        const char* dotnetName, const char* complusName) noexcept
    {
        const char* value = std::getenv(dotnetName);
        if (value == nullptr || *value == '\0')
        {
            value = std::getenv(complusName);
        }
        return value;
    }

    std::optional<std::uint32_t> ConfiguredProcessorCount() noexcept
    {
        const char* text =
            DotNetConfigValue("DOTNET_PROCESSOR_COUNT", "COMPlus_PROCESSOR_COUNT");
        if (text == nullptr)
        {
            return std::nullopt;
        }

        errno = 0;
        char* end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 10);
        if (end == text || errno == ERANGE || value == 0 || value > 0xFFFFUL)
        {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(value);
    }

#ifdef _WIN32
    std::optional<bool> WindowsBooleanConfig(
        const char* dotnetName, const char* complusName) noexcept
    {
        const char* text = DotNetConfigValue(dotnetName, complusName);
        if (text == nullptr)
        {
            return std::nullopt;
        }

        errno = 0;
        char* end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 16);
        if (end == text || errno == ERANGE)
        {
            return std::nullopt;
        }
        return value != 0;
    }

    struct WindowsCpuGroupSettings
    {
        bool GcCpuGroups = false;
        bool ThreadUseAllCpuGroups = false;
        std::uint32_t AllActiveProcessors = 0;
    };

    WindowsCpuGroupSettings GetWindowsCpuGroupSettings() noexcept
    {
        USHORT groupCount = 0;
        if (::GetProcessGroupAffinity(
                ::GetCurrentProcess(), &groupCount, nullptr)
            || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            groupCount = 1;
        }

        const bool multiGroup = groupCount > 1;
        const bool gcCpuGroups = WindowsBooleanConfig(
            "DOTNET_GCCpuGroup", "COMPlus_GCCpuGroup").value_or(multiGroup);
        const bool useAll = gcCpuGroups && WindowsBooleanConfig(
            "DOTNET_Thread_UseAllCpuGroups",
            "COMPlus_Thread_UseAllCpuGroups").value_or(multiGroup);

        std::uint32_t active = 0;
        if (gcCpuGroups && multiGroup)
        {
            active = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        }
        return {gcCpuGroups && multiGroup, useAll && multiGroup, active};
    }
#endif

#ifndef _WIN32
    std::uint32_t TotalOnlineProcessorCount() noexcept
    {
#if defined(__arm__) || defined(__aarch64__) || defined(__loongarch64) \
    || defined(__riscv)
        const long count = ::sysconf(_SC_NPROCESSORS_CONF);
#else
        const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
#endif
        return count > 0 ? static_cast<std::uint32_t>(count) : 1U;
    }

#if defined(__linux__)
    std::uint32_t AffinityProcessorCount() noexcept
    {
        std::size_t bytes = 128;
        while (bytes <= 1024 * 1024)
        {
            std::vector<unsigned long> words(
                (bytes + sizeof(unsigned long) - 1) / sizeof(unsigned long));
            bytes = words.size() * sizeof(unsigned long);
            if (::sched_getaffinity(
                0, bytes, reinterpret_cast<cpu_set_t*>(words.data())) == 0)
            {
                std::uint32_t count = 0;
                for (unsigned long word : words)
                {
                    while (word != 0)
                    {
                        word &= word - 1;
                        ++count;
                    }
                }
                return count == 0 ? 1U : count;
            }
            if (errno != EINVAL)
            {
                break;
            }
            bytes *= 2;
        }
        return TotalOnlineProcessorCount();
    }

    std::string UnescapeProcPath(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '\\' && i + 3 < value.size()
                && value[i + 1] >= '0' && value[i + 1] <= '7'
                && value[i + 2] >= '0' && value[i + 2] <= '7'
                && value[i + 3] >= '0' && value[i + 3] <= '7')
            {
                const int decoded =
                    (value[i + 1] - '0') * 64
                    + (value[i + 2] - '0') * 8
                    + (value[i + 3] - '0');
                result.push_back(static_cast<char>(decoded));
                i += 3;
            }
            else
            {
                result.push_back(value[i]);
            }
        }
        return result;
    }

    bool ContainsCsvToken(std::string_view list, std::string_view token)
    {
        std::size_t start = 0;
        while (start <= list.size())
        {
            const std::size_t end = list.find(',', start);
            const std::string_view part = list.substr(
                start, end == std::string_view::npos ? list.size() - start : end - start);
            if (part == token)
            {
                return true;
            }
            if (end == std::string_view::npos)
            {
                break;
            }
            start = end + 1;
        }
        return false;
    }

    std::optional<std::pair<std::string, std::string>>
        FindCgroupMount(bool version2)
    {
        std::ifstream file("/proc/self/mountinfo");
        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t separator = line.find(" - ");
            if (separator == std::string::npos)
            {
                continue;
            }

            std::istringstream left(line.substr(0, separator));
            std::vector<std::string> fields;
            std::string field;
            while (left >> field)
            {
                fields.push_back(field);
            }
            if (fields.size() < 5)
            {
                continue;
            }

            std::istringstream right(line.substr(separator + 3));
            std::string fsType;
            std::string source;
            std::string options;
            right >> fsType >> source >> options;
            if (version2)
            {
                if (fsType != "cgroup2")
                {
                    continue;
                }
            }
            else
            {
                if (fsType != "cgroup" || !ContainsCsvToken(options, "cpu"))
                {
                    continue;
                }
            }
            return std::pair<std::string, std::string>(
                UnescapeProcPath(fields[4]), UnescapeProcPath(fields[3]));
        }
        return std::nullopt;
    }

    std::optional<std::string> FindCgroupProcessPath(bool version2)
    {
        std::ifstream file("/proc/self/cgroup");
        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t first = line.find(':');
            const std::size_t second =
                first == std::string::npos ? std::string::npos : line.find(':', first + 1);
            if (first == std::string::npos || second == std::string::npos)
            {
                continue;
            }
            const std::string controllers = line.substr(first + 1, second - first - 1);
            if ((version2 && line.substr(0, first) == "0" && controllers.empty())
                || (!version2 && ContainsCsvToken(controllers, "cpu")))
            {
                return line.substr(second + 1);
            }
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> CpuCgroupDirectory(bool version2)
    {
        const auto mount = FindCgroupMount(version2);
        const auto process = FindCgroupProcessPath(version2);
        if (!mount || !process)
        {
            return std::nullopt;
        }

        const std::string& mountPath = mount->first;
        const std::string& mountRoot = mount->second;
        const std::string& processPath = *process;

        std::string relative = processPath;
        if (mountRoot != "/" && processPath.compare(0, mountRoot.size(), mountRoot) == 0
            && (processPath.size() == mountRoot.size()
                || processPath[mountRoot.size()] == '/'))
        {
            relative = processPath.substr(mountRoot.size());
        }

        std::filesystem::path result(mountPath);
        if (!relative.empty() && relative != "/")
        {
            result /= relative.front() == '/' ? relative.substr(1) : relative;
        }
        return result;
    }

    std::optional<long long> ReadLongLong(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        std::string token;
        if (!(file >> token))
        {
            return std::nullopt;
        }
        errno = 0;
        char* end = nullptr;
        const long long value = std::strtoll(token.c_str(), &end, 10);
        if (end == token.c_str() || errno == ERANGE)
        {
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::uint32_t> LinuxCpuLimit()
    {
        struct statfs stats{};
        if (::statfs("/sys/fs/cgroup", &stats) != 0)
        {
            return std::nullopt;
        }
        constexpr long Cgroup2SuperMagic = 0x63677270;
        const bool version2 = stats.f_type == Cgroup2SuperMagic;
        const auto directory = CpuCgroupDirectory(version2);
        if (!directory)
        {
            return std::nullopt;
        }

        long long quota = 0;
        long long period = 0;
        if (version2)
        {
            std::ifstream file(*directory / "cpu.max");
            std::string quotaText;
            if (!(file >> quotaText >> period) || quotaText == "max" || period <= 0)
            {
                return std::nullopt;
            }
            errno = 0;
            char* end = nullptr;
            quota = std::strtoll(quotaText.c_str(), &end, 10);
            if (end == quotaText.c_str() || errno == ERANGE)
            {
                return std::nullopt;
            }
        }
        else
        {
            const auto quotaValue = ReadLongLong(*directory / "cpu.cfs_quota_us");
            const auto periodValue = ReadLongLong(*directory / "cpu.cfs_period_us");
            if (!quotaValue || !periodValue)
            {
                return std::nullopt;
            }
            quota = *quotaValue;
            period = *periodValue;
        }

        if (quota <= 0 || period <= 0)
        {
            return std::nullopt;
        }
        if (quota <= period)
        {
            return 1U;
        }

        const unsigned long long unsignedQuota =
            static_cast<unsigned long long>(quota);
        const unsigned long long unsignedPeriod =
            static_cast<unsigned long long>(period);
        const unsigned long long value =
            unsignedQuota / unsignedPeriod
            + (unsignedQuota % unsignedPeriod == 0 ? 0ULL : 1ULL);
        return value > std::numeric_limits<std::uint32_t>::max()
            ? std::numeric_limits<std::uint32_t>::max()
            : static_cast<std::uint32_t>(value);
    }

#ifdef __ANDROID__
    std::optional<std::uint32_t> AndroidPresentProcessorCount()
    {
        std::ifstream file("/sys/devices/system/cpu/present");
        std::string text;
        if (!(file >> text))
        {
            return std::nullopt;
        }

        std::uint64_t total = 0;
        std::size_t start = 0;
        while (start < text.size())
        {
            const std::size_t comma = text.find(',', start);
            const std::string part = text.substr(
                start, comma == std::string::npos ? text.size() - start : comma - start);
            const std::size_t dash = part.find('-');
            try
            {
                const unsigned long first = std::stoul(
                    dash == std::string::npos ? part : part.substr(0, dash));
                const unsigned long last = dash == std::string::npos
                    ? first
                    : std::stoul(part.substr(dash + 1));
                if (last < first)
                {
                    return std::nullopt;
                }
                total += static_cast<std::uint64_t>(last - first) + 1;
            }
            catch (...)
            {
                return std::nullopt;
            }
            if (comma == std::string::npos)
            {
                break;
            }
            start = comma + 1;
        }
        if (total == 0)
        {
            return std::nullopt;
        }
        return total > std::numeric_limits<std::uint32_t>::max()
            ? std::numeric_limits<std::uint32_t>::max()
            : static_cast<std::uint32_t>(total);
    }
#endif
#endif
#endif

    std::int32_t ComputeProcessorCount() noexcept
    {
        if (const auto configured = ConfiguredProcessorCount())
        {
            return static_cast<std::int32_t>(*configured);
        }

#ifdef _WIN32
        const WindowsCpuGroupSettings groups = GetWindowsCpuGroupSettings();

        std::uint32_t count = 1;
        if (groups.ThreadUseAllCpuGroups && groups.AllActiveProcessors > 0)
        {
            count = groups.AllActiveProcessors;
        }
        else
        {
            DWORD_PTR processMask = 0;
            DWORD_PTR systemMask = 0;
            if (::GetProcessAffinityMask(
                    ::GetCurrentProcess(), &processMask, &systemMask))
            {
                count = 0;
                while (processMask != 0)
                {
                    processMask &= processMask - 1;
                    ++count;
                }
                if (count == 0)
                {
                    count = 64;
                }
            }
        }

        JOBOBJECT_CPU_RATE_CONTROL_INFORMATION rate{};
        if (::QueryInformationJobObject(
                nullptr,
                JobObjectCpuRateControlInformation,
                &rate,
                sizeof(rate),
                nullptr))
        {
            constexpr DWORD HardCap =
                JOB_OBJECT_CPU_RATE_CONTROL_ENABLE
                | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
            constexpr DWORD MinMax =
                JOB_OBJECT_CPU_RATE_CONTROL_ENABLE
                | JOB_OBJECT_CPU_RATE_CONTROL_MIN_MAX_RATE;
            DWORD maxRate = 0;
            if ((rate.ControlFlags & HardCap) == HardCap)
            {
                maxRate = rate.CpuRate;
            }
            else if ((rate.ControlFlags & MinMax) == MinMax)
            {
                // MaxRate is the high WORD of the CpuRate union in the Windows SDK
                // layout; MinGW-w64 headers omit the MinRate/MaxRate struct member.
                maxRate = static_cast<WORD>(rate.CpuRate >> 16);
            }

            constexpr DWORD MaxCpuRate = 10000;
            if (maxRate > 0 && maxRate < MaxCpuRate)
            {
                std::uint32_t total = groups.GcCpuGroups
                    && groups.AllActiveProcessors > 0
                    ? groups.AllActiveProcessors
                    : 1U;
                if (!groups.GcCpuGroups || groups.AllActiveProcessors == 0)
                {
                    SYSTEM_INFO info{};
                    ::GetSystemInfo(&info);
                    total = info.dwNumberOfProcessors;
                }

                const std::uint64_t limited =
                    (static_cast<std::uint64_t>(maxRate) * total
                        + MaxCpuRate - 1) / MaxCpuRate;
                if (limited < count)
                {
                    count = static_cast<std::uint32_t>(limited);
                }
            }
        }
        return static_cast<std::int32_t>(std::max<std::uint32_t>(count, 1U));
#else
        std::uint32_t count = 1;
#if defined(__linux__)
#ifdef __ANDROID__
        if (const auto present = AndroidPresentProcessorCount())
        {
            count = *present;
        }
        else
#endif
        {
            count = AffinityProcessorCount();
        }
        if (const auto limit = LinuxCpuLimit(); limit && *limit < count)
        {
            count = *limit;
        }
#else
        count = TotalOnlineProcessorCount();
#endif
        return static_cast<std::int32_t>(
            std::min<std::uint32_t>(
                std::max<std::uint32_t>(count, 1U),
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max())));
#endif
    }

    }

    std::int32_t EnvironmentProcessorCount()
    {
        static const std::int32_t count = ComputeProcessorCount();
        return count;
    }
}

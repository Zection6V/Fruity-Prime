#include "Mods/branding.hpp"

#include "Mods/Update/build_version.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <unistd.h>
#endif

namespace {

template<typename Character>
std::basic_string<Character> file_name_without_extension(
    std::basic_string_view<Character> path,
    std::basic_string_view<Character> separators) {
    const std::size_t separator = path.find_last_of(separators);
    std::basic_string_view<Character> name = separator == path.npos
        ? path : path.substr(separator + 1);
    const std::size_t period = name.find_last_of(static_cast<Character>('.'));
    if (period != name.npos) {
        name = name.substr(0, period);
    }
    return std::basic_string<Character>(name);
}

#ifdef _WIN32
std::optional<std::wstring> process_path() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::nullopt;
        }
        if (length < buffer.size()) {
            return std::wstring(buffer.data(), length);
        }
        constexpr std::size_t MaxDword =
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)());
        if (buffer.size() > MaxDword / 2) {
            return std::nullopt;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::optional<std::string> utf8(std::wstring_view value) {
    if (value.empty()) {
        return std::string{};
    }
    if (value.size() > static_cast<std::size_t>(
            (std::numeric_limits<int>::max)())) {
        return std::nullopt;
    }
    const int length = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length,
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), length,
        result.data(), required, nullptr, nullptr);
    if (written != required) {
        return std::nullopt;
    }
    return result;
}
#elif defined(__linux__) || defined(__ANDROID__)
std::optional<std::string> process_path() {
    std::vector<char> buffer(512);
    for (;;) {
        const ssize_t length = readlink(
            "/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return std::nullopt;
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return std::string(buffer.data(), static_cast<std::size_t>(length));
        }
        if (buffer.size() > std::numeric_limits<std::size_t>::max() / 2) {
            return std::nullopt;
        }
        buffer.resize(buffer.size() * 2);
    }
}
#elif defined(__APPLE__)
std::optional<std::string> process_path() {
    std::uint32_t size = 0;
    if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
        return std::nullopt;
    }
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return std::nullopt;
    }
    return std::string(buffer.data());
}
#elif defined(__FreeBSD__)
std::optional<std::string> process_path() {
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t size = 0;
    if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0 || size == 0) {
        return std::nullopt;
    }
    std::vector<char> buffer(size);
    if (sysctl(mib, 4, buffer.data(), &size, nullptr, 0) != 0 || size == 0) {
        return std::nullopt;
    }
    if (buffer[size - 1] == '\0') {
        --size;
    }
    return std::string(buffer.data(), size);
}
#endif

} // namespace

namespace fruityprime::branding {

std::string name_and_version() {
    const auto version = update::BuildVersion::current();
    const std::string display = update::BuildVersion::display(version);
    return version ? std::string(Name) + " " + display
        : std::string(Name) + " (" + display + ")";
}

} // namespace fruityprime::branding

namespace fruityprime::mods {

std::string Branding::Executable() {
#ifdef _WIN32
    const auto path = process_path();
    if (!path) {
        return branding::FileName;
    }
    const std::wstring name = file_name_without_extension<wchar_t>(
        *path, L"\\/");
    const auto converted = utf8(name);
    if (!converted || converted->empty()) {
        return branding::FileName;
    }
    return *converted;
#elif defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__) \
    || defined(__FreeBSD__)
    const auto path = process_path();
    if (!path) {
        return branding::FileName;
    }
    const std::string name = file_name_without_extension<char>(*path, "/");
    return name.empty() ? std::string(branding::FileName) : name;
#else
    return branding::FileName;
#endif
}

std::string Branding::NameAndVersion() {
    return branding::name_and_version();
}

} // namespace fruityprime::mods

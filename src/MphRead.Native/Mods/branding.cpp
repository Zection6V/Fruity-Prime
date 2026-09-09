#include "Mods/branding.hpp"

#include "Mods/Update/build_version.hpp"

#include <array>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fruityprime::branding {

std::string executable_name(const std::filesystem::path& executable_path) {
    const std::string name = executable_path.stem().string();
    return name.empty() ? std::string(FileName) : name;
}

std::string name_and_version() {
    const auto version = update::BuildVersion::current();
    if (version) {
        return std::string(Name) + " " + update::BuildVersion::display(version);
    }
    return std::string(Name) + " (" + update::BuildVersion::display(version)
        + ")";
}

} // namespace fruityprime::branding

namespace fruityprime::mods {

std::string Branding::Executable() {
#ifdef _WIN32
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0 && length < path.size()) {
        return branding::executable_name(std::filesystem::path(
            std::wstring_view(path.data(), length)));
    }
#endif
    return branding::FileName;
}

std::string Branding::NameAndVersion() {
    return branding::name_and_version();
}

} // namespace fruityprime::mods

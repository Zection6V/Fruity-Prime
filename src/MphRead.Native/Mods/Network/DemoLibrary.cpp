#include "Mods/Network/demo_library.hpp"

#include "Mods/Network/demo.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace fruityprime::demo {
namespace {

constexpr std::size_t StampLength = 19;

[[nodiscard]] bool has_demo_extension(
    const std::filesystem::path& path) noexcept {
    const std::string extension = path.extension().string();
    return extension.size() == Extension.size()
        && std::equal(extension.begin(), extension.end(), Extension.begin(),
                      [](char left, char right) {
                          return static_cast<char>(std::tolower(
                              static_cast<unsigned char>(left))) == right;
                      });
}

[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
parse_stamp(std::string_view value) {
    if (value.size() != StampLength) {
        return std::nullopt;
    }
    const auto number = [value](std::size_t offset,
                                std::size_t count) -> std::optional<int> {
        int result = 0;
        for (std::size_t index = 0; index < count; ++index) {
            const char digit = value[offset + index];
            if (digit < '0' || digit > '9') {
                return std::nullopt;
            }
            result = result * 10 + (digit - '0');
        }
        return result;
    };
    if (value[4] != '-' || value[7] != '-' || value[10] != '_'
        || value[13] != '-' || value[16] != '-') {
        return std::nullopt;
    }
    const auto year = number(0, 4);
    const auto month = number(5, 2);
    const auto day = number(8, 2);
    const auto hour = number(11, 2);
    const auto minute = number(14, 2);
    const auto second = number(17, 2);
    if (!year || !month || !day || !hour || !minute || !second
        || *month < 1 || *month > 12 || *day < 1 || *day > 31
        || *hour > 23 || *minute > 59 || *second > 59) {
        return std::nullopt;
    }
    std::tm local{};
    local.tm_year = *year - 1900;
    local.tm_mon = *month - 1;
    local.tm_mday = *day;
    local.tm_hour = *hour;
    local.tm_min = *minute;
    local.tm_sec = *second;
    local.tm_isdst = -1;
    const std::time_t stamp = std::mktime(&local);
    if (stamp == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(stamp);
}

[[nodiscard]] std::chrono::system_clock::time_point file_time(
    const std::filesystem::path& path) {
    const auto value = std::filesystem::last_write_time(path);
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        value - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now());
}

[[nodiscard]] std::string size_text(std::uintmax_t bytes) {
    constexpr std::uintmax_t KiB = 1024;
    constexpr std::uintmax_t MiB = KiB * 1024;
    if (bytes >= MiB) {
        std::ostringstream output;
        output << std::fixed << std::setprecision(1)
               << (static_cast<double>(bytes) / MiB) << " MB";
        return output.str();
    }
    if (bytes >= KiB) {
        return std::to_string(bytes / KiB) + " KB";
    }
    return std::to_string(bytes) + " bytes";
}

} // namespace

std::filesystem::path demos_directory(
    const std::filesystem::path& export_root) {
    const auto root = export_root.empty()
        ? std::filesystem::current_path() : export_root;
    return std::filesystem::absolute(root / "_demos");
}

std::vector<Recording> list_recordings(
    const std::filesystem::path& directory) {
    std::vector<Recording> found;
    try {
        if (!std::filesystem::is_directory(directory)) {
            return found;
        }
        for (const auto& entry : std::filesystem::directory_iterator(
                 directory)) {
            if (!entry.is_regular_file() || !has_demo_extension(entry.path())) {
                continue;
            }
            const std::string name = entry.path().stem().string();
            const std::size_t stamp_start = name.size() >= StampLength + 1
                ? name.size() - StampLength : std::string::npos;
            std::string room = name;
            std::chrono::system_clock::time_point recorded{};
            bool parsed = false;
            if (stamp_start != std::string::npos
                && stamp_start > 0 && name[stamp_start - 1] == '_') {
                if (const auto stamp = parse_stamp(
                        std::string_view(name).substr(stamp_start));
                    stamp.has_value()) {
                    room = name.substr(0, stamp_start - 1);
                    recorded = *stamp;
                    parsed = true;
                }
            }
            if (!parsed) {
                recorded = file_time(entry.path());
            }
            std::error_code size_error;
            const auto bytes = std::filesystem::file_size(
                entry.path(), size_error);
            if (size_error) {
                continue;
            }
            found.push_back(Recording{
                entry.path(), std::move(room), recorded, bytes});
        }
    } catch (const std::filesystem::filesystem_error&) {
        // An inaccessible demo folder is an empty list, matching the managed
        // launcher: importing a file remains available through the picker.
        found.clear();
    }
    std::sort(found.begin(), found.end(),
              [](const Recording& left, const Recording& right) {
                  if (left.recorded != right.recorded) {
                      return left.recorded > right.recorded;
                  }
                  return left.path.filename().string()
                      > right.path.filename().string();
              });
    return found;
}

std::string describe(const Recording& recording) {
    std::time_t stamp = std::chrono::system_clock::to_time_t(
        recording.recorded);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &stamp);
#else
    local = *std::localtime(&stamp);
#endif
    static constexpr std::array<const char*, 12> months{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const int month = std::clamp(local.tm_mon, 0, 11);
    std::ostringstream output;
    output << local.tm_mday << ' ' << months[month] << ' '
           << (local.tm_year + 1900) << ", " << std::setfill('0')
           << std::setw(2) << local.tm_hour << ':' << std::setw(2)
           << local.tm_min << " — " << size_text(recording.bytes);
    return output.str();
}

} // namespace fruityprime::demo

#include "Mods/thumbnail_generator.hpp"

#include "Formats/paths.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace fruityprime::mods::thumbnail {

std::filesystem::path cache_directory(
    const std::filesystem::path& game_root) {
    return game_root / "thumbnails";
}

std::filesystem::path log_path(const std::filesystem::path& game_root) {
    return game_root / "thumbnails.log";
}

std::string safe_room_key(std::string_view room_key) {
    std::string safe;
    safe.reserve(room_key.size());
    for (const unsigned char byte : room_key) {
        if ((byte >= static_cast<unsigned char>('A')
             && byte <= static_cast<unsigned char>('Z'))
            || (byte >= static_cast<unsigned char>('a')
                && byte <= static_cast<unsigned char>('z'))
            || (byte >= static_cast<unsigned char>('0')
                && byte <= static_cast<unsigned char>('9'))) {
            safe.push_back(static_cast<char>(std::tolower(byte)));
        } else {
            safe.push_back('_');
        }
    }
    return safe;
}

std::filesystem::path path_for(const std::filesystem::path& game_root,
                               std::string_view room_key) {
    return cache_directory(game_root) / (safe_room_key(room_key) + ".png");
}

bool first_hunt_available(
    const std::filesystem::path& game_root) noexcept {
    try {
        formats::Paths paths;
        paths.update(game_root);
        paths.choose_fh_path();
        if (paths.fh_file_system().empty()) {
            return false;
        }
        std::error_code error;
        return std::filesystem::is_directory(
                   std::filesystem::path(paths.fh_file_system()), error)
            && !error;
    } catch (...) {
        // Paths can be absent or malformed on a first-run install.  That is
        // the normal "no First Hunt files" answer, not a fatal cache error.
        return false;
    }
}

std::vector<std::string> multiplayer_rooms(
    bool first_hunt_files_available) {
    std::vector<std::string> result;
    result.reserve(scene::multiplayer_rooms().size());
    for (const auto& room : scene::multiplayer_rooms()) {
        if (!first_hunt_files_available && room.id == 119) {
            continue;
        }
        result.push_back(room.name);
    }
    std::sort(result.begin(), result.end(),
              [](const std::string& left, const std::string& right) {
                  const auto insensitive = [](unsigned char value) {
                      return static_cast<unsigned char>(std::tolower(value));
                  };
                  const std::size_t common = std::min(left.size(), right.size());
                  for (std::size_t index = 0; index < common; ++index) {
                      const auto left_value = insensitive(
                          static_cast<unsigned char>(left[index]));
                      const auto right_value = insensitive(
                          static_cast<unsigned char>(right[index]));
                      if (left_value != right_value) {
                          return left_value < right_value;
                      }
                  }
                  return left.size() < right.size();
              });
    return result;
}

std::vector<std::string> multiplayer_rooms_for_install(
    const std::filesystem::path& game_root) {
    return multiplayer_rooms(first_hunt_available(game_root));
}

bool exists(const std::filesystem::path& game_root,
            std::string_view room_key) noexcept {
    std::error_code error;
    const auto output = path_for(game_root, room_key);
    if (!std::filesystem::is_regular_file(output, error) || error) {
        return false;
    }
    const auto size = std::filesystem::file_size(output, error);
    return !error && size > 0;
}

bool ensure_cache_directory(
    const std::filesystem::path& game_root) noexcept {
    std::error_code error;
    const auto directory = cache_directory(game_root);
    if (std::filesystem::is_directory(directory, error) && !error) {
        return true;
    }
    error.clear();
    (void)std::filesystem::create_directories(directory, error);
    return !error && std::filesystem::is_directory(directory, error)
        && !error;
}

std::vector<std::string> missing(
    const std::filesystem::path& game_root,
    std::span<const std::string_view> room_keys) {
    std::vector<std::string> result;
    result.reserve(room_keys.size());
    for (const std::string_view room_key : room_keys) {
        if (!exists(game_root, room_key)) {
            result.emplace_back(room_key);
        }
    }
    return result;
}

} // namespace fruityprime::mods::thumbnail

#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mods::thumbnail {

inline constexpr int ThumbnailWidth = 1600;
inline constexpr int ThumbnailHeight = 900;

// The game-file root is supplied by the launcher/runtime because a native
// command, a desktop game, and Android each choose a different data folder.
[[nodiscard]] std::filesystem::path cache_directory(
    const std::filesystem::path& game_root);

[[nodiscard]] std::filesystem::path log_path(
    const std::filesystem::path& game_root);

// Room keys are human-readable labels, not filenames.  This is the exact
// managed rule for the ASCII room names used by the catalog: letters and
// digits are lower-cased; every other code unit becomes '_'.
[[nodiscard]] std::string safe_room_key(std::string_view room_key);

[[nodiscard]] std::filesystem::path path_for(
    const std::filesystem::path& game_root, std::string_view room_key);

// First Hunt rooms use a separate extracted filesystem.  The launcher passes
// the install root here rather than making the cache module depend on global
// settings state.
[[nodiscard]] bool first_hunt_available(
    const std::filesystem::path& game_root) noexcept;

// Native room catalog counterpart of ThumbnailGenerator.MultiplayerRooms.
// The catalog is already limited to multiplayer rooms; the First Hunt entry
// is removed when its separate files are not present.  Entity-spawn probing
// belongs to the asset-backed capture host and is intentionally not hidden in
// this filename/cache module.
[[nodiscard]] std::vector<std::string> multiplayer_rooms(
    bool first_hunt_files_available = true);

[[nodiscard]] std::vector<std::string> multiplayer_rooms_for_install(
    const std::filesystem::path& game_root);

// Failed encodes can leave a zero-byte file behind.  Such a file is never a
// valid cache hit and must be retried on the next run.
[[nodiscard]] bool exists(const std::filesystem::path& game_root,
                          std::string_view room_key) noexcept;

[[nodiscard]] bool ensure_cache_directory(
    const std::filesystem::path& game_root) noexcept;

// The caller supplies the room list so this module stays independent of a
// particular asset source.  The returned order is the input order, matching
// the managed MissingThumbnails LINQ pipeline.
[[nodiscard]] std::vector<std::string> missing(
    const std::filesystem::path& game_root,
    std::span<const std::string_view> room_keys);

} // namespace fruityprime::mods::thumbnail

#include "Mods/thumbnail_batch.hpp"

#include "Mods/thumbnail_generator.hpp"

#include <algorithm>
#include <system_error>
#include <thread>
#include <utility>

namespace fruityprime::mods::thumbnail {

int default_parallelism() noexcept {
    const unsigned int cores = std::thread::hardware_concurrency();
    const unsigned int usable = cores == 0 ? 2U : cores;
    return static_cast<int>(std::clamp(usable, 2U, 10U));
}

bool can_run(const std::filesystem::path& executable) noexcept {
    if (executable.empty()) {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(executable, error) && !error;
}

std::vector<std::vector<std::string>> shares(
    std::span<const std::string_view> rooms, int parallelism) {
    if (rooms.empty()) {
        return {};
    }
    const int requested = std::clamp(parallelism, 1, 16);
    const std::size_t worker_count = std::min<std::size_t>(
        static_cast<std::size_t>(requested), rooms.size());
    std::vector<std::vector<std::string>> result(worker_count);
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        result[index % worker_count].emplace_back(rooms[index]);
    }
    return result;
}

std::vector<WorkerCommand> worker_commands(
    const std::filesystem::path& executable,
    const std::filesystem::path& working_directory,
    std::span<const std::string_view> rooms, int parallelism,
    int width, int height) {
    std::vector<WorkerCommand> result;
    for (auto& share : shares(rooms, parallelism)) {
        WorkerCommand command;
        command.executable = executable;
        command.working_directory = working_directory;
        command.rooms = std::move(share);
        command.arguments.reserve(command.rooms.size() * 2U + 2U);
        for (const std::string& room : command.rooms) {
            command.arguments.push_back("-thumbnail");
            command.arguments.push_back(room);
        }
        command.arguments.push_back("-size");
        command.arguments.push_back(std::to_string(width) + "x"
                                    + std::to_string(height));
        result.push_back(std::move(command));
    }
    return result;
}

RunResult run_serial(const std::filesystem::path& game_root,
                     std::span<const std::string_view> rooms, int width,
                     int height, const Capture& capture,
                     const Report& report) {
    RunResult result;
    if (!capture || rooms.empty()) {
        return result;
    }
    (void)ensure_cache_directory(game_root);
    for (std::size_t index = 0; index < rooms.size(); ++index) {
        const std::string_view room = rooms[index];
        if (exists(game_root, room)) {
            continue;
        }
        bool ok = false;
        try {
            ok = capture(room, width, height)
                && exists(game_root, room);
        } catch (...) {
            ok = false;
        }
        const std::string line = "[thumbnails] "
            + std::to_string(index + 1) + "/" + std::to_string(rooms.size())
            + (ok ? "  ok  " : "  FAILED  ") + std::string(room);
        if (report) {
            report(line);
        }
        if (ok) {
            ++result.written;
        } else {
            result.failed.emplace_back(room);
        }
    }
    return result;
}

} // namespace fruityprime::mods::thumbnail

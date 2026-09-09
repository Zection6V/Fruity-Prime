#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::mods::thumbnail {

struct WorkerCommand final {
    std::filesystem::path executable;
    std::filesystem::path working_directory;
    std::vector<std::string> arguments;
    std::vector<std::string> rooms;
};

struct RunResult final {
    std::size_t written = 0;
    std::vector<std::string> failed;
};

using Report = std::function<void(std::string_view)>;
using Capture = std::function<bool(std::string_view, int, int)>;

[[nodiscard]] int default_parallelism() noexcept;

[[nodiscard]] bool can_run(const std::filesystem::path& executable) noexcept;

// Deal rooms out round-robin. The source list is copied so the returned
// commands outlive the caller's command-line storage.
[[nodiscard]] std::vector<std::vector<std::string>> shares(
    std::span<const std::string_view> rooms, int parallelism);

// Build the exact argv shape used by the managed worker process. Launching
// the process remains a Win32 frontend concern; keeping command construction
// here makes that frontend and tests share the same batching contract.
[[nodiscard]] std::vector<WorkerCommand> worker_commands(
    const std::filesystem::path& executable,
    const std::filesystem::path& working_directory,
    std::span<const std::string_view> rooms, int parallelism,
    int width, int height);

// Portable serial fallback used when there is no executable to spawn (and by
// Android-style hosts). The callback owns the actual GL-thread capture.
[[nodiscard]] RunResult run_serial(
    const std::filesystem::path& game_root,
    std::span<const std::string_view> rooms, int width, int height,
    const Capture& capture, const Report& report = {});

} // namespace fruityprime::mods::thumbnail

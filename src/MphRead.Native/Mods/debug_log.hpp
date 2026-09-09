#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace fruityprime::debug {

class Log {
public:
    Log() = default;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    ~Log();

    // Returns false when logging was not requested or the file could not be
    // opened. Neither case is a startup error.
    [[nodiscard]] bool attach(const std::filesystem::path& base_directory,
                              bool enabled, bool forced = false);
    void detach() noexcept;
    [[nodiscard]] bool active() const noexcept { return output_.is_open(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void line(std::string_view category, std::string_view message);

private:
    void write_line(std::string_view category, std::string_view message);

    mutable std::mutex mutex_;
    std::ofstream output_;
    std::filesystem::path path_;
};

} // namespace fruityprime::debug

#include "Mods/thumbnail_log.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace fruityprime::mods::thumbnail {
namespace {

std::mutex& log_mutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

[[nodiscard]] std::string timestamp(const char* format) noexcept {
    try {
        const std::time_t value = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        if (localtime_s(&local, &value) != 0) {
            return "?";
        }
#else
        const std::tm* converted = std::localtime(&value);
        if (converted == nullptr) {
            return "?";
        }
        local = *converted;
#endif
        std::ostringstream output;
        output << std::put_time(&local, format);
        return output.str();
    } catch (...) {
        return "?";
    }
}

} // namespace

bool Log::begin(const std::filesystem::path& path, std::size_t rooms,
                std::string_view build, std::string_view assembly,
                std::string_view file_stamp) noexcept {
    std::lock_guard lock(log_mutex());
    path_ = path;
    failed_ = true;
    if (path_.empty()) {
        return false;
    }

    try {
        std::ofstream output(path_, std::ios::trunc);
        if (!output) {
            return false;
        }
        output << "=== Fruity Prime preview generation, "
               << timestamp("%Y-%m-%d %H:%M:%S") << " ===\n"
               << "build " << build << ", assembly " << assembly
               << ", file " << (file_stamp.empty() ? "?" : file_stamp)
               << "\n" << rooms << " room(s) to render\n";
        if (!output) {
            return false;
        }
    } catch (...) {
        return false;
    }
    failed_ = false;
    return true;
}

bool Log::write(std::string_view line) noexcept {
    if (failed_) {
        return false;
    }
    std::lock_guard lock(log_mutex());
    for (int attempt = 0; attempt < 5; ++attempt) {
        try {
            std::ofstream output(path_, std::ios::app);
            if (output) {
                output << '[' << timestamp("%H:%M:%S") << "] " << line
                       << '\n';
                if (output) {
                    return true;
                }
            }
        } catch (...) {
            // Treat all stream failures like the managed IOException path.
        }
        if (attempt + 1 < 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    failed_ = true;
    return false;
}

} // namespace fruityprime::mods::thumbnail

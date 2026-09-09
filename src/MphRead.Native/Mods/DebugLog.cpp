#include "Mods/debug_log.hpp"

#include "Mods/branding.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

namespace fruityprime::debug {
namespace {

[[nodiscard]] std::tm local_time(std::time_t value) noexcept {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

[[nodiscard]] std::string timestamp(bool milliseconds) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto current = local_time(time);
    std::ostringstream output;
    output << std::put_time(&current, "%Y-%m-%d %H:%M:%S");
    if (milliseconds) {
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        output << '.' << std::setfill('0') << std::setw(3) << millis.count();
    }
    return output.str();
}

} // namespace

Log::~Log() {
    detach();
}

bool Log::attach(const std::filesystem::path& base_directory, bool enabled,
                 bool forced) {
    std::lock_guard lock(mutex_);
    if (output_.is_open()) {
        return true;
    }
    if (!enabled && !forced) {
        return false;
    }
    std::error_code error;
    const auto directory = base_directory / "logs";
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }

    std::vector<std::filesystem::directory_entry> logs;
    for (const auto& entry : std::filesystem::directory_iterator(
             directory, std::filesystem::directory_options::skip_permission_denied,
             error)) {
        if (!error && entry.is_regular_file(error)
            && entry.path().extension() == ".log") {
            logs.push_back(entry);
        }
    }
    std::sort(logs.begin(), logs.end(), [](const auto& left, const auto& right) {
        std::error_code left_error;
        std::error_code right_error;
        return std::filesystem::last_write_time(left, left_error)
            > std::filesystem::last_write_time(right, right_error);
    });
    for (std::size_t index = 8; index < logs.size(); ++index) {
        std::filesystem::remove(logs[index].path(), error);
    }

    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    const std::tm current = local_time(now);
    std::ostringstream filename;
    filename << branding::FileName << '-'
             << std::put_time(&current, "%Y%m%d-%H%M%S") << ".log";
    path_ = directory / filename.str();
    for (int suffix = 1; std::filesystem::exists(path_, error); ++suffix) {
        path_ = directory / (filename.str().substr(
            0, filename.str().size() - 4) + "-" + std::to_string(suffix)
            + ".log");
    }
    output_.open(path_, std::ios::out | std::ios::trunc);
    if (!output_) {
        path_.clear();
        return false;
    }
    write_line("build", std::string(branding::Name) + " "
        + "(native)" + ", protocol "
        + std::to_string(net::NetConfig::ProtocolVersion));
    return true;
}

void Log::detach() noexcept {
    std::lock_guard lock(mutex_);
    if (output_.is_open()) {
        output_.flush();
        output_.close();
    }
}

void Log::line(std::string_view category, std::string_view message) {
    std::lock_guard lock(mutex_);
    if (output_.is_open()) {
        write_line(category, message);
    }
}

void Log::write_line(std::string_view category, std::string_view message) {
    output_ << '[' << timestamp(true) << "] [" << category << "] "
            << message << '\n';
    output_.flush();
}

} // namespace fruityprime::debug

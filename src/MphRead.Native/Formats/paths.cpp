#include "Formats/paths.hpp"

#include <array>
#include <fstream>
#include <system_error>

namespace fruityprime::formats {
namespace {

constexpr std::array<std::string_view, 11> PathKeys{{
    AMFE0, AMFP0, A76E0, AMHE0, AMHE1, AMHJ0, AMHJ1, AMHP0, AMHP1,
    AMHK0, Export
}};

void trim(std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

} // namespace

Paths::Paths() {
    reset();
}

void Paths::reset() {
    all_paths_.clear();
    for (const auto key : PathKeys) {
        all_paths_.emplace(key, std::string{});
    }
    mph_key = std::string(AMHE0);
    fh_key = std::string(AMFE0);
}

void Paths::update(const std::filesystem::path& root) {
    reset();
    std::ifstream input(root / "paths.txt");
    if (!input) {
        return;
    }
    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        trim(line);
        if (first_line) {
            first_line = false;
            continue;
        }
        const std::size_t split = line.find('=');
        if (split == std::string::npos || split == 0) {
            continue;
        }
        std::string key = line.substr(0, split);
        std::string value = line.substr(split + 1);
        trim(key);
        trim(value);
        const auto found = all_paths_.find(key);
        if (found != all_paths_.end()) {
            found->second = std::move(value);
        }
    }
}

void Paths::choose_mph_path() noexcept {
    constexpr std::array<std::string_view, 8> Order{{
        AMHE1, AMHP1, AMHJ1, AMHK0, AMHE0, AMHP0, AMHJ0, A76E0
    }};
    for (const auto key : Order) {
        const auto found = all_paths_.find(std::string(key));
        if (found != all_paths_.end() && !found->second.empty()) {
            mph_key = found->first;
            return;
        }
    }
    mph_key = std::string(AMHE0);
}

void Paths::choose_fh_path() noexcept {
    constexpr std::array<std::string_view, 2> Order{{AMFE0, AMFP0}};
    for (const auto key : Order) {
        const auto found = all_paths_.find(std::string(key));
        if (found != all_paths_.end() && !found->second.empty()) {
            fh_key = found->first;
            return;
        }
    }
    fh_key = std::string(AMFE0);
}

bool Paths::set_path(std::string_view key,
                     const std::filesystem::path& path_value) {
    const auto found = all_paths_.find(std::string(key));
    if (found == all_paths_.end()) {
        return false;
    }
    found->second = path_value.generic_string();
    return true;
}

std::string Paths::path(std::string_view key) const {
    const auto found = all_paths_.find(std::string(key));
    return found == all_paths_.end() ? std::string{} : found->second;
}

const std::string& Paths::file_system() const noexcept {
    const auto found = all_paths_.find(mph_key);
    static const std::string Empty;
    return found == all_paths_.end() ? Empty : found->second;
}

const std::string& Paths::fh_file_system() const noexcept {
    const auto found = all_paths_.find(fh_key);
    static const std::string Empty;
    return found == all_paths_.end() ? Empty : found->second;
}

const std::string& Paths::export_path() const noexcept {
    const auto found = all_paths_.find(std::string(Export));
    static const std::string Empty;
    return found == all_paths_.end() ? Empty : found->second;
}

bool Paths::write(const std::filesystem::path& root,
                  std::string_view version) const {
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) {
        return false;
    }
    std::ofstream output(root / "paths.txt", std::ios::trunc);
    if (!output) {
        return false;
    }
    output << version << '\n';
    for (const auto key : PathKeys) {
        output << key << '=' << path(key) << '\n';
    }
    return static_cast<bool>(output);
}

Paths& global_paths() noexcept {
    static Paths paths;
    return paths;
}

} // namespace fruityprime::formats

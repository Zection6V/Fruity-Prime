#include "Assets/game_assets.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <stdexcept>

namespace fruityprime::assets {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open asset " + path.string());
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("could not determine asset size " + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read asset " + path.string());
        }
    }
    return bytes;
}

} // namespace

Store Store::from_path(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) {
        return from_directory(path);
    }
    if (path.extension() == ".nds" || path.extension() == ".NDS") {
        return from_rom(path);
    }
    throw std::invalid_argument(
        "asset source must be a directory or an NDS ROM: " + path.string());
}

Store Store::from_directory(const std::filesystem::path& path) {
    if (!std::filesystem::is_directory(path)) {
        throw std::invalid_argument("asset directory does not exist: "
                                    + path.string());
    }
    return Store(path);
}

Store Store::from_rom(const std::filesystem::path& path) {
    return Store(nds::Rom::read_file(path));
}

Store::Store(std::filesystem::path root)
    : root_(std::move(root)) {}

Store::Store(nds::Rom rom)
    : rom_(std::move(rom)) {}

std::optional<std::string> Store::game_key() const {
    if (rom_.has_value()) {
        return rom_->header().game_code
            + std::to_string(static_cast<unsigned>(rom_->header().version));
    }
    std::filesystem::path candidate = root_.filename();
    if (candidate == "data") {
        candidate = root_.parent_path().filename();
    }
    const std::string key = candidate.string();
    constexpr std::array<std::string_view, 10> keys{{
        "AMFE0", "AMFP0", "A76E0", "AMHE0", "AMHE1", "AMHP0",
        "AMHP1", "AMHJ0", "AMHJ1", "AMHK0"
    }};
    return std::find(keys.begin(), keys.end(), key) != keys.end()
        ? std::optional<std::string>(key) : std::nullopt;
}

std::string Store::normalize_relative(std::string_view path) {
    if (path.empty()) {
        throw std::invalid_argument("asset path is empty");
    }
    std::string normalized(path);
    for (char& value : normalized) {
        if (value == '\\') {
            value = '/';
        }
    }
    const std::filesystem::path candidate(normalized);
    if (candidate.is_absolute()) {
        throw std::invalid_argument("asset path must be relative");
    }
    for (const auto& component : candidate) {
        if (component == ".." || component == ".") {
            throw std::invalid_argument("asset path contains a traversal component");
        }
    }
    return candidate.generic_string();
}

std::vector<std::uint8_t> Store::bytes(std::string_view path) const {
    const std::string normalized = normalize_relative(path);
    if (rom_.has_value()) {
        if (normalized == "_bin/arm9.bin") {
            return rom_->arm9();
        }
        constexpr std::string_view overlay_prefix = "_bin/overlay9_";
        if (normalized.starts_with(overlay_prefix)) {
            const std::string_view id_text = std::string_view(normalized)
                .substr(overlay_prefix.size());
            if (!id_text.empty()) {
                std::uint32_t overlay_id = 0;
                const auto parsed = std::from_chars(
                    id_text.data(), id_text.data() + id_text.size(),
                    overlay_id);
                if (parsed.ec == std::errc{}
                    && parsed.ptr == id_text.data() + id_text.size()) {
                    return rom_->overlay9(overlay_id);
                }
            }
        }
        try {
            return rom_->file(normalized);
        } catch (const std::out_of_range&) {
            // The managed extractor exposes the contents of every ROM
            // archive as `_archives/<archive>/<entry>`.  Keep that path
            // contract available for a ROM-backed store too, so native
            // modules can use the same asset names without requiring a
            // separate extraction pass.
            constexpr std::string_view prefix = "_archives/";
            if (!normalized.starts_with(prefix)) {
                throw std::out_of_range(
                    "ROM asset path was not found: " + normalized);
            }
            const std::string_view virtual_path =
                std::string_view(normalized).substr(prefix.size());
            const std::size_t separator = virtual_path.find('/');
            if (separator == std::string_view::npos
                || separator == 0
                || separator + 1 >= virtual_path.size()) {
                throw std::out_of_range(
                    "ROM archive path is malformed: " + normalized);
            }
            const std::string archive_path = "archives/"
                + std::string(virtual_path.substr(0, separator)) + ".arc";
            const std::string_view entry_name = virtual_path.substr(
                separator + 1);
            const auto resource = archive::Archive::parse(
                rom_->file(archive_path));
            const auto found = std::find_if(
                resource.entries().begin(), resource.entries().end(),
                [entry_name](const auto& entry) {
                    return entry.filename == entry_name;
                });
            if (found == resource.entries().end()) {
                throw std::out_of_range(
                    "ROM archive entry was not found: " + normalized);
            }
            return resource.file(static_cast<std::size_t>(
                found - resource.entries().begin()));
        }
    }
    return read_all(root_ / std::filesystem::path(normalized));
}

archive::Archive Store::archive(std::string_view path) const {
    return archive::Archive::parse(bytes(path));
}

model::File Store::model(std::string_view path) const {
    return model::File::from_bytes(bytes(path));
}

model::File Store::model_from_archive(std::string_view archive_path,
                                      std::string_view entry_name) const {
    const auto resource = archive(archive_path);
    for (std::size_t i = 0; i < resource.entries().size(); ++i) {
        if (resource.entries()[i].filename == entry_name) {
            return model::File::from_bytes(resource.file(i));
        }
    }
    throw std::out_of_range("archive entry was not found: "
                            + std::string(entry_name));
}

} // namespace fruityprime::assets

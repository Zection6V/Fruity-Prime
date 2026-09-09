#pragma once

#include "Mods/Update/build_version.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace fruityprime::update {

// The fields are the native counterpart of MphRead.Mods.Update.UpdateInfo.
// Keep the release metadata separate from the installer so a text launcher,
// a GUI, and a headless server can all report the same answer.
struct UpdateInfo {
    std::string tag;
    Version version;
    std::string asset_name;
    std::string asset_url;
    std::uint64_t asset_size = 0;
    std::string page_url;
    std::string notes;
};

class UpdateCheck final {
public:
    // Query the latest release when this binary has a release stamp.  A local
    // build deliberately returns no update, matching BuildVersion's contract.
    [[nodiscard]] static std::optional<UpdateInfo> latest(
        bool server_build = false);

    // Parse a saved GitHub release response.  Keeping this pure makes the
    // network boundary testable without contacting GitHub.
    [[nodiscard]] static std::optional<UpdateInfo> parse(
        std::string_view json,
        std::optional<Version> installed = std::nullopt,
        bool server_build = false);

    [[nodiscard]] static const std::string& last_reason() noexcept;
    [[nodiscard]] static std::string rid();
    [[nodiscard]] static std::string releases_page();
    [[nodiscard]] static bool is_allowed_url(std::string_view url) noexcept;
};

struct DownloadResult {
    bool ok = false;
    std::string error;
};

// Download to a neighbouring .part file and publish it only after the body
// and optional expected size are complete.  Progress is 0..1, or -1 when the
// server did not provide a length.
class UpdateDownload final {
public:
    [[nodiscard]] static DownloadResult fetch(
        std::string_view url, const std::string& path,
        std::uint64_t expected_bytes = 0,
        const std::function<void(float)>& progress = {});
};

// The desktop installer deliberately has no UI dependency.  A launcher can
// call Stage/Launch from its own thread, while the command-line and game heads
// only need Apply when they are started by the staged copy.  The archive
// reader is public so package handling can be verified without downloading a
// release from GitHub.
class DesktopUpdate final {
public:
    static constexpr std::string_view ApplyFlag = "applyupdate";

    [[nodiscard]] static bool supported(
        const std::filesystem::path& base_directory) noexcept;
    [[nodiscard]] static bool stage(
        const UpdateInfo& update,
        const std::filesystem::path& base_directory,
        const std::function<void(float)>& progress = {});
    [[nodiscard]] static bool extract_package(
        const std::filesystem::path& archive,
        const std::filesystem::path& destination,
        bool zip,
        std::string& error);
    [[nodiscard]] static bool launch(
        const std::filesystem::path& base_directory);
    [[nodiscard]] static int apply(
        const std::filesystem::path& target_directory,
        int wait_for_pid,
        const std::filesystem::path& source_directory = {});
    static void clean(const std::filesystem::path& base_directory) noexcept;

    [[nodiscard]] static std::string binary_name(
        bool server_build = false);
    [[nodiscard]] static const std::string& last_error() noexcept;
};

struct InstallResult {
    bool ok = false;
    std::string error;
};

// Native equivalent of the managed IUpdateInstaller/DesktopUpdateInstaller
// conversation.  It is intentionally a small value-less facade: platform
// heads own progress/UI and decide when to call the irreversible Install.
class UpdateInstall final {
public:
    [[nodiscard]] static bool can_install(
        const std::filesystem::path& base_directory,
        const UpdateInfo& update) noexcept;
    [[nodiscard]] static InstallResult prepare(
        const std::filesystem::path& base_directory,
        const UpdateInfo& update,
        const std::function<void(float)>& progress = {});
    [[nodiscard]] static InstallResult install(
        const std::filesystem::path& base_directory);
    [[nodiscard]] static constexpr bool exit_after_install() noexcept {
        return true;
    }
};

struct UpdateState {
    bool disabled = false;
    bool checked = false;
    std::optional<UpdateInfo> available;
};

// Small state holder for launcher/front-end code.  It intentionally has no
// UI or process-exit policy; those belong to the platform head.
class Updater final {
public:
    explicit Updater(bool server_build = false) noexcept
        : server_build_(server_build) {}

    [[nodiscard]] std::optional<UpdateInfo> check();
    [[nodiscard]] const UpdateState& state() const noexcept { return state_; }
    [[nodiscard]] std::string describe(const UpdateInfo& update) const;
    [[nodiscard]] static bool open_page(const UpdateInfo& update);
    [[nodiscard]] static bool open_link(std::string_view url);

private:
    bool server_build_ = false;
    UpdateState state_;
};

} // namespace fruityprime::update

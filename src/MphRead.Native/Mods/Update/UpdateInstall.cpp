/*
 * Native counterpart of MphRead/Mods/Update/UpdateInstall.cs.
 *
 * The managed interface is represented by the same three operations here:
 * ask whether this desktop can install, prepare the package, then start the
 * staged process for the irreversible copy.
 */
#include "Mods/Update/update.hpp"

#include <atomic>

namespace {

std::atomic<bool> g_desktop_installer_selected{false};

} // namespace

namespace fruityprime::update {

void UpdateInstall::use_desktop_if_possible(
    const std::filesystem::path& base_directory) noexcept {
    if (!g_desktop_installer_selected.load(std::memory_order_relaxed)
        && DesktopUpdate::supported(base_directory)) {
        g_desktop_installer_selected.store(true, std::memory_order_relaxed);
    }
}

bool UpdateInstall::has_current() noexcept {
    return g_desktop_installer_selected.load(std::memory_order_relaxed);
}

bool UpdateInstall::can_install(
    const std::filesystem::path& base_directory,
    const UpdateInfo& update) noexcept {
    return !update.asset_url.empty() && has_current()
        && DesktopUpdate::supported(base_directory);
}

InstallResult UpdateInstall::prepare(
    const std::filesystem::path& base_directory, const UpdateInfo& update,
    const std::function<void(float)>& progress) {
    InstallResult result;
    if (!can_install(base_directory, update)) {
        result.error = update.asset_url.empty()
            ? "this release has no package for this platform"
            : "this installation cannot be replaced in place";
        return result;
    }
    result.ok = DesktopUpdate::stage(update, base_directory, progress);
    if (!result.ok) {
        result.error = DesktopUpdate::last_error();
        if (result.error.empty()) {
            result.error = "the update could not be prepared";
        }
    }
    return result;
}

InstallResult UpdateInstall::install(
    const std::filesystem::path& base_directory) {
    InstallResult result;
    result.ok = DesktopUpdate::launch(base_directory);
    if (!result.ok) {
        result.error = DesktopUpdate::last_error();
        if (result.error.empty()) {
            result.error = "the update could not be started";
        }
    }
    return result;
}

} // namespace fruityprime::update

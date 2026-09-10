/*
 * Native counterpart of MphRead/Mods/Update/Updater.cs.
 *
 * The state is deliberately an instance: native tests and a launcher can
 * inspect two independent heads without leaking a previous check through a
 * process-wide static. The policy remains the same -- checking is read-only,
 * and opening the release page is the only UI-facing action here.
 */
#include "Mods/Update/update.hpp"

#include "Mods/branding.hpp"

#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fruityprime::update {

namespace {

bool g_disabled = false;

} // namespace

void Updater::set_disabled(bool value) noexcept {
    g_disabled = value;
}

bool Updater::disabled() noexcept {
    return g_disabled;
}

std::optional<UpdateInfo> Updater::check() {
    if (state_.disabled || disabled()) {
        return std::nullopt;
    }
    state_.available = UpdateCheck::latest(server_build_);
    state_.checked = true;
    return state_.available;
}

std::string Updater::describe(const UpdateInfo& update) const {
    const std::string package = update.asset_name.empty()
        ? std::string{} : " -- you want " + update.asset_name;
    return update.tag + " is available (this is "
        + BuildVersion::display(BuildVersion::current()) + ")" + package;
}

bool Updater::open_page(const UpdateInfo& update) {
    return open_link(update.page_url.empty()
                         ? UpdateCheck::releases_page() : update.page_url);
}

bool Updater::open_link(std::string_view url) {
    // Update and support links are intentionally HTTPS-only. This prevents a
    // malformed release response from turning the browser action into a
    // local executable or a shell command.
    if (url.size() < 8 || url.substr(0, 8) != "https://") {
        return false;
    }
#ifdef _WIN32
    const HINSTANCE result = ShellExecuteA(
        nullptr, "open", std::string(url).c_str(), nullptr, nullptr,
        SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#else
    static_cast<void>(url);
    return false;
#endif
}

} // namespace fruityprime::update

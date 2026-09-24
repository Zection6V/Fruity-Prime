#pragma once

// The launcher's other screens, in the Avalonia controls they were written
// against: settings, the map picker, the demo picker and the pause menu.
//
// Each is created whole and answered through the few questions HomeView and
// PauseMenuWindow ask of it, so the callers never see the adapters.

#include "HostControls.hpp"

#include "../../Mods/Network/DemoLibrary.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::NativeRuntime::Avalonia
{
    [[nodiscard]] Toolkit::ElementPtr NewSettingsView(
        const std::shared_ptr<MphRead::MenuSettings>& settings, bool inGame);
    void SettingsViewClosed(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void SettingsViewGameFilesRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);

    [[nodiscard]] Toolkit::ElementPtr NewMapPickerView(
        const std::vector<std::string>& rooms, std::string current);
    void MapPickerClosed(const Toolkit::ElementPtr& view, std::function<void()> action);
    [[nodiscard]] std::optional<std::string> MapPickerRoomKey(
        const Toolkit::ElementPtr& view);

    [[nodiscard]] Toolkit::ElementPtr NewDemoPickerView(
        std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>> demos,
        std::string directory);
    void DemoPickerClosed(const Toolkit::ElementPtr& view, std::function<void()> action);
    [[nodiscard]] std::optional<std::string> DemoPickerPath(
        const Toolkit::ElementPtr& view);
    [[nodiscard]] bool DemoPickerImportRequested(const Toolkit::ElementPtr& view);

    [[nodiscard]] Toolkit::ElementPtr NewPauseMenuView(bool offerWindowMode);
    void PauseResumed(const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseSettingsRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseLeaveRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseQuitRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseSpectateRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseRejoinRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseRecordToggleRequested(
        const Toolkit::ElementPtr& view, std::function<void()> action);
    void PauseFocusResume(const Toolkit::ElementPtr& view);
}

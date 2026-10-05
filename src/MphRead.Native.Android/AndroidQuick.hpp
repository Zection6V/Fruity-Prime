#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace MphRead::Droid
{
    void QuickFront();
    // Startup is observable from both sides: the QML boot layer reads the
    // bridge's state, the Activity's watchdog reads StartupFlags().
    enum StartupFlag : std::int32_t
    {
        StartupNativeCreated = 1, StartupFrontPublished = 2, StartupFirstFrame = 4, StartupFailed = 8
    };
    void StartupPhase(const char* phase, const std::string& detail = {});
    void QuickStartup(std::string state, std::string error = {});
    void StartupFail(const std::string& error);
    void StartupMark(std::int32_t flag) noexcept;
    [[nodiscard]] std::int32_t StartupFlags() noexcept;
    [[nodiscard]] std::string StartupError();
    void QuickLobby();
    void QuickPage(std::string page);
    void QuickPause(std::function<void()> resume, std::function<void()> leave, std::function<void()> quit);
    void QuickGoBack();
    void QuickSuspendBackdrop(bool value);
}

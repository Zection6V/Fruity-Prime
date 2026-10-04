#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace MphRead::Droid
{
    void QuickFront();
    void QuickLobby();
    void QuickPage(std::string page);
    void QuickPause(std::function<void()> resume, std::function<void()> leave, std::function<void()> quit);
    void QuickGoBack();
    void QuickSuspendBackdrop(bool value);
}

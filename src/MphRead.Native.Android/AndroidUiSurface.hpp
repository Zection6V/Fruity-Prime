#pragma once
#include "AndroidApp.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
namespace MphRead::Droid
{
    // QtQuickView owns the GPU surface, sizing and input. This is the match's
    // menu state; there is no CPU bitmap or texture upload between toolkits.
    class AndroidUiSurface final
    {
    public:
        static std::shared_ptr<AndroidUiSurface> Current() noexcept;
        static std::shared_ptr<AndroidUiSurface> Ensure();
        bool Visible() const noexcept { return _visible; }
        void Show(const std::shared_ptr<AndroidLauncherPage>& page);
        void ShowEnd();
        void Hide();
    private:
        std::atomic_bool _visible = false;
    };
}

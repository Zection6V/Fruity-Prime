#pragma once

#include "../Portable/LaunchPlan.hpp"

#include <cstdint>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    struct HomeWindowMenuSettingsRef
    {
        void* Native;
    };

    struct HomeWindowRoomsRef
    {
        void* Native;
    };

    enum class HomeWindowStartupLocation : std::uint8_t
    {
        CenterScreen
    };

    enum class HomeWindowIcon : std::uint8_t
    {
        GuiThemeAppIconValue
    };

    enum class HomeWindowBrush : std::uint8_t
    {
        GuiThemePanelBrush
    };

    enum class HomeWindowThemeVariant : std::uint8_t
    {
        Dark
    };

    class HomeWindowAdapter
    {
    public:
        using HomeViewHandle = void*;
        using DoneHandler = void (*)(void* context);

        virtual ~HomeWindowAdapter() = default;

        virtual HomeViewHandle ConstructHomeView(HomeWindowMenuSettingsRef settings,
            HomeWindowRoomsRef rooms) = 0;
        virtual void ConnectHomeViewDone(HomeViewHandle view, void* context,
            DoneHandler handler) = 0;
        [[nodiscard]] virtual LaunchPlan GetHomeViewPlan(HomeViewHandle view) const = 0;

        virtual void Close() = 0;
        virtual void SetTitle(std::string_view title) = 0;
        virtual void SetIcon(HomeWindowIcon icon) = 0;
        virtual void SetWidth(double width) = 0;
        virtual void SetHeight(double height) = 0;
        virtual void SetMinWidth(double minWidth) = 0;
        virtual void SetMinHeight(double minHeight) = 0;
        virtual void SetWindowStartupLocation(HomeWindowStartupLocation location) = 0;
        virtual void SetBackground(HomeWindowBrush brush) = 0;
        virtual void SetRequestedThemeVariant(HomeWindowThemeVariant variant) = 0;
        virtual void SetContent(HomeViewHandle content) = 0;
    };

    class HomeWindow final
    {
    public:
        HomeWindow(HomeWindowAdapter& adapter, HomeWindowMenuSettingsRef settings,
            HomeWindowRoomsRef rooms);

        HomeWindow(const HomeWindow&) = delete;
        HomeWindow& operator=(const HomeWindow&) = delete;
        HomeWindow(HomeWindow&&) = delete;
        HomeWindow& operator=(HomeWindow&&) = delete;

        [[nodiscard]] LaunchPlan Plan() const;

    private:
        static void OnDone(void* context);

        HomeWindowAdapter& _adapter;
        HomeWindowAdapter::HomeViewHandle _view;
    };
}

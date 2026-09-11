#include "HomeWindow.hpp"

#include "../../Branding.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    HomeWindow::HomeWindow(HomeWindowAdapter& adapter, HomeWindowMenuSettingsRef settings,
        HomeWindowRoomsRef rooms)
        : _adapter(adapter),
          _view(_adapter.ConstructHomeView(settings, rooms))
    {
        _adapter.ConnectHomeViewDone(_view, &_adapter, &HomeWindow::OnDone);

        _adapter.SetTitle(MphRead::Mods::Branding::Name);
        _adapter.SetIcon(HomeWindowIcon::GuiThemeAppIconValue);
        _adapter.SetWidth(940.0);
        _adapter.SetHeight(560.0);
        _adapter.SetMinWidth(780.0);
        _adapter.SetMinHeight(480.0);
        _adapter.SetWindowStartupLocation(HomeWindowStartupLocation::CenterScreen);
        _adapter.SetBackground(HomeWindowBrush::GuiThemePanelBrush);
        _adapter.SetRequestedThemeVariant(HomeWindowThemeVariant::Dark);
        _adapter.SetContent(_view);
    }

    LaunchPlan HomeWindow::Plan() const
    {
        return _adapter.GetHomeViewPlan(_view);
    }

    void HomeWindow::OnDone(void* context)
    {
        auto& adapter = *static_cast<HomeWindowAdapter*>(context);
        adapter.Close();
    }
}

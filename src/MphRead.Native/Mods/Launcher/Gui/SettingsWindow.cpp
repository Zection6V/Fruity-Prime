#include "SettingsWindow.hpp"

#include <memory>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    struct SettingsWindowEventTarget final
    {
        explicit SettingsWindowEventTarget(SettingsWindowAdapter& adapter) noexcept
            : Adapter(adapter)
        {
        }

        SettingsWindowAdapter& Adapter;
    };

    void SettingsWindowAdapter::DispatchOpened(
        SettingsWindow& window, SettingsWindowOpenedEventArgs& e)
    {
        window.OnOpened(e);
    }

    SettingsWindow::SettingsWindow(SettingsWindowAdapter& adapter,
        SettingsViewAdapter& viewAdapter,
        std::shared_ptr<MenuSettings> settings,
        bool inGame)
        : SettingsWindow(adapter,
            std::make_shared<SettingsView>(viewAdapter, std::move(settings), inGame))
    {
    }

    SettingsWindow::SettingsWindow(SettingsWindowAdapter& adapter,
        std::shared_ptr<SettingsView> view)
        : _adapter(adapter), _view(std::move(view))
    {
        if (!_view)
        {
            throw SettingsWindowNullReferenceException();
        }

        _eventTarget = std::make_shared<SettingsWindowEventTarget>(_adapter);

        _closedHandler = SettingsViewEventHandler(_eventTarget, &SettingsWindow::OnViewClosed);
        _view->AddClosed(_closedHandler);

        _stylusPlacementRequestedHandler = SettingsViewEventHandler(
            _eventTarget, &SettingsWindow::OnStylusPlacementRequested);
        _view->AddStylusPlacementRequested(_stylusPlacementRequestedHandler);

        _adapter.SetTitle(_view->WindowTitle());
        _adapter.SetIcon(GuiTheme::AppIcon.Value());
        _adapter.SetBackground(GuiTheme::InkBrush);
        _adapter.SetRequestedThemeVariant(SettingsWindowThemeVariant::Dark);
        if (_view->InGame())
        {
            _adapter.SetCanResize(false);
            _adapter.SetSystemDecorations(SettingsWindowSystemDecorations::None);
            _adapter.CoverGameWindow();
            _adapter.SetTopmost(true);
            _adapter.SetShowInTaskbar(false);
        }
        else
        {
            _adapter.SetWidth(980.0);
            _adapter.SetHeight(660.0);
            _adapter.SetMinWidth(720.0);
            _adapter.SetMinHeight(460.0);
            _adapter.SetWindowStartupLocation(SettingsWindowStartupLocation::CenterOwner);
        }
        _adapter.SetContent(_view);
    }

    bool SettingsWindow::Saved() const noexcept
    {
        return _view->Saved();
    }

    void SettingsWindow::OnOpened(SettingsWindowOpenedEventArgs& e)
    {
        _adapter.BaseOnOpened(e);
        if (_view->InGame())
        {
            _adapter.CoverGameWindow();
        }
    }

    void SettingsWindow::OnViewClosed(void* target, void*,
        const SettingsViewEventArgs&)
    {
        static_cast<SettingsWindowEventTarget*>(target)->Adapter.Close();
    }

    void SettingsWindow::OnStylusPlacementRequested(void* target, void*,
        const SettingsViewEventArgs&)
    {
        static_cast<SettingsWindowEventTarget*>(target)->Adapter.Close();
    }
}

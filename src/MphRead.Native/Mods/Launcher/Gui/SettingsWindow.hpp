#pragma once

#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "GuiTheme.hpp"
#include "SettingsView.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::Mods::Launcher::Gui
{
    using SettingsWindowNullReferenceException = ::System::NullReferenceException;

    enum class SettingsWindowThemeVariant : unsigned char
    {
        Dark
    };

    enum class SettingsWindowSystemDecorations : unsigned char
    {
        None
    };

    enum class SettingsWindowStartupLocation : unsigned char
    {
        CenterOwner
    };

    struct SettingsWindowOpenedEventArgs final
    {
        void* Native = nullptr;
    };

    class SettingsWindow;

    class SettingsWindowAdapter
    {
    public:
        virtual ~SettingsWindowAdapter() = default;

        virtual void Close() = 0;
        virtual void SetTitle(std::string title) = 0;
        virtual void SetIcon(const std::optional<GuiWindowIcon>& icon) = 0;
        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetRequestedThemeVariant(SettingsWindowThemeVariant variant) = 0;
        virtual void SetCanResize(bool canResize) = 0;
        virtual void SetSystemDecorations(SettingsWindowSystemDecorations decorations) = 0;

        // Native boundary for PauseMenuWindow.CoverGameWindow(this). Item #62
        // supplies that window implementation; #61 preserves the exact call
        // positions without duplicating or approximating its sizing policy.
        virtual void CoverGameWindow() = 0;

        virtual void SetTopmost(bool topmost) = 0;
        virtual void SetShowInTaskbar(bool showInTaskbar) = 0;
        virtual void SetWidth(double width) = 0;
        virtual void SetHeight(double height) = 0;
        virtual void SetMinWidth(double minWidth) = 0;
        virtual void SetMinHeight(double minHeight) = 0;
        virtual void SetWindowStartupLocation(SettingsWindowStartupLocation location) = 0;
        virtual void SetContent(const std::shared_ptr<SettingsView>& view) = 0;

        void DispatchOpened(SettingsWindow& window, SettingsWindowOpenedEventArgs& e);
        virtual void BaseOnOpened(SettingsWindowOpenedEventArgs& e) = 0;
    };

    struct SettingsWindowEventTarget;

    class SettingsWindow final
    {
    public:
        SettingsWindow(SettingsWindowAdapter& adapter,
            SettingsViewAdapter& viewAdapter,
            std::shared_ptr<MenuSettings> settings,
            bool inGame = false);
        SettingsWindow(SettingsWindowAdapter& adapter,
            std::shared_ptr<SettingsView> view);

        SettingsWindow(const SettingsWindow&) = delete;
        SettingsWindow& operator=(const SettingsWindow&) = delete;
        SettingsWindow(SettingsWindow&&) = delete;
        SettingsWindow& operator=(SettingsWindow&&) = delete;

        [[nodiscard]] bool Saved() const noexcept;

    protected:
        void OnOpened(SettingsWindowOpenedEventArgs& e);

    private:
        friend class SettingsWindowAdapter;

        static void OnViewClosed(void* target, void* sender,
            const SettingsViewEventArgs& args);
        static void OnStylusPlacementRequested(void* target, void* sender,
            const SettingsViewEventArgs& args);

        SettingsWindowAdapter& _adapter;
        std::shared_ptr<SettingsView> _view;
        std::shared_ptr<SettingsWindowEventTarget> _eventTarget;
        SettingsViewEventHandler _closedHandler;
        SettingsViewEventHandler _stylusPlacementRequestedHandler;
    };
}

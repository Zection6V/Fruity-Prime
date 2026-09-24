#include "PauseMenuWindow.hpp"

#include "../../Branding.hpp"
#include "../../PauseMenu.hpp"
#include "../../SpectatorMode.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../WindowMode.hpp"
#include "../../Chat/ChatBox.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Network/MapVote.hpp"
#include "../../Network/NetSession.hpp"
#include "../../../GameState.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    struct PauseMenuWindowEventTarget final
    {
        std::weak_ptr<PauseMenuWindow> Window;
    };

    struct PauseMenuWindowSettingsContinuation final
    {
        std::shared_ptr<PauseMenuWindow> Window;
        std::shared_ptr<PauseMenuWindowChildAdapter> Adapter;
        std::shared_ptr<SettingsViewAdapter> ViewAdapter;
        std::shared_ptr<SettingsWindow> Dialog;
        bool WasTopmost = false;
    };

    struct PauseMenuWindowMapContinuation final
    {
        std::shared_ptr<PauseMenuWindow> Window;
        std::shared_ptr<PauseMenuWindowChildAdapter> Adapter;
        std::shared_ptr<MapPickerViewAdapter> ViewAdapter;
        std::shared_ptr<MapPickerView> View;
        std::shared_ptr<MapPickerWindow> Dialog;
        bool WasTopmost = false;
    };

    std::shared_ptr<PauseMenuWindow> PauseMenuWindow::_open{};
    std::shared_ptr<PauseMenuWindowChildAdapter> PauseMenuWindow::_openSettings{};
    const std::array<PauseMenuWindowTransparencyLevel, 2>
        PauseMenuWindow::_scrimLevels{
            PauseMenuWindowTransparencyLevel::Transparent,
            PauseMenuWindowTransparencyLevel::None
        };

    PauseMenuWindowNullReferenceException::PauseMenuWindowNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    PauseMenuWindowDialogCompletion::PauseMenuWindowDialogCompletion(
        void* context, Callback function, std::shared_ptr<void> keepAlive)
        : _context(context), _function(function), _keepAlive(std::move(keepAlive))
    {
    }

    void PauseMenuWindowDialogCompletion::Invoke(std::exception_ptr error) const
    {
        if (_function != nullptr)
        {
            _function(_context, std::move(error));
        }
    }

    void PauseMenuWindowChildAdapter::CoverGameWindow()
    {
        PauseMenuWindow::CoverGameWindow(*this);
    }

    void PauseMenuWindowAdapter::DispatchOpened(
        PauseMenuWindow& window, PauseMenuWindowEventArgs& e)
    {
        window.OnOpened(e);
    }

    void PauseMenuWindowAdapter::DispatchKeyDown(
        PauseMenuWindow& window, PauseMenuWindowKeyEventArgs& e)
    {
        window.OnKeyDown(e);
    }

    void PauseMenuWindowAdapter::DispatchClosed(
        PauseMenuWindow& window, PauseMenuWindowEventArgs& e)
    {
        window.OnClosed(e);
    }

    namespace
    {
        class VectorRoomEnumerator final : public MapPickerRoomEnumerator
        {
        public:
            explicit VectorRoomEnumerator(
                std::shared_ptr<const std::vector<MapPickerStringRef>> rooms)
                : _rooms(std::move(rooms))
            {
            }

            bool MoveNext() override
            {
                if (!_rooms || _next >= _rooms->size())
                {
                    _current.reset();
                    return false;
                }
                _current = (*_rooms)[_next];
                ++_next;
                return true;
            }

            MapPickerStringRef Current() const override
            {
                return _current;
            }

            void Dispose() override
            {
            }

        private:
            std::shared_ptr<const std::vector<MapPickerStringRef>> _rooms;
            std::size_t _next = 0;
            MapPickerStringRef _current;
        };

        class VectorRoomList final : public MapPickerRoomList
        {
        public:
            explicit VectorRoomList(const std::vector<std::string>& rooms)
            {
                auto values = std::make_shared<std::vector<MapPickerStringRef>>();
                values->reserve(rooms.size());
                for (const std::string& room : rooms)
                {
                    values->push_back(std::make_shared<const std::string>(room));
                }
                _rooms = std::move(values);
            }

            std::shared_ptr<MapPickerRoomEnumerator> GetEnumerator() const override
            {
                return std::make_shared<VectorRoomEnumerator>(_rooms);
            }

        private:
            std::shared_ptr<const std::vector<MapPickerStringRef>> _rooms;
        };

        void WriteSettingsError(const std::exception& ex)
        {
            std::cout << "[pause] the settings could not be opened: "
                << ex.what() << '\n';
        }

        void WriteMapVoteError(const std::exception& ex)
        {
            std::cout << "[pause] the map vote could not be opened: "
                << ex.what() << '\n';
        }

        template <typename TLog, typename TFinally>
        void CatchThenFinally(std::exception_ptr error, TLog&& log, TFinally&& finalizer)
        {
            std::exception_ptr pending;
            if (error)
            {
                try
                {
                    std::rethrow_exception(error);
                }
                catch (const std::exception& ex)
                {
                    try
                    {
                        log(ex);
                    }
                    catch (...)
                    {
                        pending = std::current_exception();
                    }
                }
                catch (...)
                {
                    pending = std::current_exception();
                }
            }

            // C# finally runs even when the catch body throws. If the finalizer
            // throws, its exception replaces the pending one, as in the CLR.
            finalizer();
            if (pending)
            {
                std::rethrow_exception(pending);
            }
        }
    }

    PauseMenuWindow::PauseMenuWindow(
        std::shared_ptr<PauseMenuWindowAdapter> adapter)
        : _adapter(std::move(adapter))
    {
        if (!_adapter)
        {
            throw PauseMenuWindowNullReferenceException();
        }

        _adapter->SetTitle(std::string(MphRead::Mods::Branding::Name) + " - paused");
        _adapter->SetIcon(GuiTheme::AppIcon.Value());
        _adapter->SetCanResize(false);
        _adapter->SetSystemDecorations(PauseMenuWindowSystemDecorations::None);
        _adapter->SetTransparencyLevelHint(_scrimLevels);
        _adapter->SetBackground(GuiTheme::ScrimBrush);
        _adapter->SetRequestedThemeVariant(PauseMenuWindowThemeVariant::Dark);
        _adapter->SetTopmost(true);
        _adapter->SetShowInTaskbar(false);
        CoverGameWindow(*_adapter);

        _view = std::make_shared<PauseMenuView>(*_adapter, true);
    }

    void PauseMenuWindow::FinishConstruction(
        const std::shared_ptr<PauseMenuWindow>& self)
    {
        _adapter->BindWindow(self);
        _eventTarget = std::make_shared<PauseMenuWindowEventTarget>();
        _eventTarget->Window = self;

        _view->AddResumed(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnResumed));
        _view->AddFullscreenRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnFullscreenRequested));
        _view->AddSettingsRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnSettingsRequested));
        _view->AddVoteMapRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnVoteMapRequested));
        _view->AddSpectateRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnSpectateRequested));
        _view->AddRejoinRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnRejoinRequested));
        _view->AddRecordToggleRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnRecordToggleRequested));
        _view->AddLeaveRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnLeaveRequested));
        _view->AddQuitRequested(PauseMenuViewEventHandler::Instance(
            _eventTarget, &PauseMenuWindow::OnQuitRequested));
        _adapter->SetWindowContent(_view);
    }

    void PauseMenuWindow::FollowGameWindow()
    {
        if (_open)
        {
            CoverGameWindow(*_open->_adapter);
        }
        if (_openSettings)
        {
            CoverGameWindow(*_openSettings);
        }
    }

    bool PauseMenuWindow::Open()
    {
        if (_open)
        {
            _open->_adapter->Activate();
            return true;
        }

        std::shared_ptr<PauseMenuWindowAdapter> adapter
            = Detail::CreatePauseMenuWindowAdapter();
        auto window = std::shared_ptr<PauseMenuWindow>(
            new PauseMenuWindow(std::move(adapter)));
        window->FinishConstruction(window);
        _open = window;
        window->_adapter->Show();
        window->_adapter->Activate();
        return true;
    }

    void PauseMenuWindow::CloseIfOpen()
    {
        std::shared_ptr<PauseMenuWindow> window = _open;
        if (!window)
        {
            return;
        }
        try
        {
            window->CloseSelf();
        }
        catch (...)
        {
        }
    }

    bool PauseMenuWindow::IsOpen() noexcept
    {
        return static_cast<bool>(_open);
    }

    void PauseMenuWindow::CoverGameWindow(PauseMenuWindowCoverTarget& window)
    {
        if (MphRead::Mods::PauseMenu::WindowWidth() <= 0
            || MphRead::Mods::PauseMenu::WindowHeight() <= 0)
        {
            window.SetWindowStartupLocation(
                PauseMenuWindowStartupLocation::CenterScreen);
            return;
        }

        window.SetWindowStartupLocation(PauseMenuWindowStartupLocation::Manual);
        const PauseMenuWindowPixelPoint origin{
            MphRead::Mods::PauseMenu::WindowX(),
            MphRead::Mods::PauseMenu::WindowY()
        };
        if (window.Position() != origin)
        {
            window.SetPosition(origin);
        }

        const double scale = ScalingAt(window, origin);
        const double width = MphRead::Mods::PauseMenu::WindowWidth() / scale;
        const double height = MphRead::Mods::PauseMenu::WindowHeight() / scale;
        if (std::abs(window.Width() - width) > 0.5
            || std::abs(window.Height() - height) > 0.5)
        {
            window.SetWidth(width);
            window.SetHeight(height);
        }
    }

    double PauseMenuWindow::ScalingAt(PauseMenuWindowCoverTarget& window,
        PauseMenuWindowPixelPoint point)
    {
        try
        {
            const std::optional<double> scaling
                = window.ScreenScalingFromPoint(point);
            if (scaling.has_value() && *scaling > 0.0)
            {
                return *scaling;
            }
        }
        catch (...)
        {
        }

        const double renderScaling = window.RenderScaling();
        return renderScaling > 0.0 ? renderScaling : 1.0;
    }

    std::string PauseMenuWindow::WindowLabel()
    {
        return MphRead::Mods::WindowMode::IsFullscreen()
            ? "Windowed"
            : "Fullscreen";
    }

    void PauseMenuWindow::OpenSettings()
    {
        if (_settingsOpen)
        {
            return;
        }

        _settingsOpen = true;
        const bool wasTopmost = _adapter->Topmost();
        _adapter->SetTopmost(false);
        try
        {
            std::shared_ptr<MphRead::MenuSettings> settings
                = MphRead::GameState::LoadSettings();
            std::shared_ptr<SettingsViewAdapter> viewAdapter
                = _adapter->CreateSettingsViewAdapter();
            if (!viewAdapter)
            {
                throw PauseMenuWindowNullReferenceException();
            }
            auto view = std::make_shared<SettingsView>(
                *viewAdapter, std::move(settings), true);

            std::shared_ptr<PauseMenuWindowChildAdapter> child
                = _adapter->CreateChildWindowAdapter();
            if (!child)
            {
                throw PauseMenuWindowNullReferenceException();
            }
            auto dialog = std::make_shared<SettingsWindow>(*child, view);
            _openSettings = child;

            auto state = std::make_shared<PauseMenuWindowSettingsContinuation>();
            state->Window = _eventTarget ? _eventTarget->Window.lock() : nullptr;
            if (!state->Window)
            {
                throw PauseMenuWindowNullReferenceException();
            }
            state->Adapter = child;
            state->ViewAdapter = viewAdapter;
            state->Dialog = dialog;
            state->WasTopmost = wasTopmost;
            child->ShowDialog(*_adapter, dialog, PauseMenuWindowDialogCompletion(
                state.get(), &PauseMenuWindow::ContinueSettings, state));
            return;
        }
        catch (...)
        {
            const std::exception_ptr error = std::current_exception();
            CatchThenFinally(error, WriteSettingsError,
                [this, wasTopmost]
                {
                    FinishSettings(wasTopmost);
                });
        }
    }

    void PauseMenuWindow::ContinueSettings(
        void* context, std::exception_ptr error)
    {
        auto& state = *static_cast<PauseMenuWindowSettingsContinuation*>(context);
        std::shared_ptr<PauseMenuWindow> self = state.Window;
        if (!self)
        {
            return;
        }

        try
        {
            CatchThenFinally(std::move(error), WriteSettingsError,
                [self, wasTopmost = state.WasTopmost]
                {
                    self->FinishSettings(wasTopmost);
                });
        }
        catch (...)
        {
            self->_adapter->PostAsyncVoidException(std::current_exception());
        }
    }

    void PauseMenuWindow::FinishSettings(bool wasTopmost)
    {
        _openSettings.reset();
        _settingsOpen = false;
        _adapter->SetTopmost(wasTopmost);
        _adapter->Activate();
    }

    void PauseMenuWindow::OpenMapVote()
    {
        if (_voteOpen)
        {
            return;
        }

        const std::string why = MphRead::Mods::Network::MapVote::WhyNotProposing();
        if (!why.empty())
        {
            MphRead::Mods::Chat::ChatBox::System(
                std::optional<std::string>(why));
            CloseSelf();
            return;
        }

        _voteOpen = true;
        const bool wasTopmost = _adapter->Topmost();
        _adapter->SetTopmost(false);

        bool completeSynchronously = false;
        try
        {
            const std::vector<std::string> rooms
                = MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
            if (rooms.empty())
            {
                MphRead::Mods::Chat::ChatBox::System(
                    std::optional<std::string>(std::string("no maps to vote for")));
                completeSynchronously = true;
            }
            else
            {
                const std::optional<MphRead::Mods::Network::MatchStatePacket> serverMatch
                    = MphRead::Mods::Network::NetSession::ServerMatch();
                std::string current = serverMatch && serverMatch->RoomKey
                    ? *serverMatch->RoomKey
                    : rooms[0];

                std::shared_ptr<MapPickerViewAdapter> viewAdapter
                    = _adapter->CreateMapPickerViewAdapter();
                if (!viewAdapter)
                {
                    throw PauseMenuWindowNullReferenceException();
                }
                auto roomList = std::make_shared<VectorRoomList>(rooms);
                MapPickerStringRef currentRef
                    = std::make_shared<const std::string>(std::move(current));
                auto view = std::make_shared<MapPickerView>(
                    *viewAdapter, roomList, currentRef);

                std::shared_ptr<PauseMenuWindowChildAdapter> child
                    = _adapter->CreateChildWindowAdapter();
                if (!child)
                {
                    throw PauseMenuWindowNullReferenceException();
                }
                auto dialog = std::make_shared<MapPickerWindow>(
                    std::static_pointer_cast<MapPickerWindowAdapter>(child), view);

                view->AddClosed(MapPickerEventHandler::Instance(
                    child, &PauseMenuWindow::OnMapPickerClosed));
                CoverGameWindow(*child);

                auto state = std::make_shared<PauseMenuWindowMapContinuation>();
                state->Window = _eventTarget ? _eventTarget->Window.lock() : nullptr;
                if (!state->Window)
                {
                    throw PauseMenuWindowNullReferenceException();
                }
                state->Adapter = child;
                state->ViewAdapter = viewAdapter;
                state->View = view;
                state->Dialog = dialog;
                state->WasTopmost = wasTopmost;
                child->ShowDialog(*_adapter, dialog, PauseMenuWindowDialogCompletion(
                    state.get(), &PauseMenuWindow::ContinueMapVote, state));
                return;
            }
        }
        catch (...)
        {
            const std::exception_ptr error = std::current_exception();
            CatchThenFinally(error, WriteMapVoteError,
                [this, wasTopmost]
                {
                    FinishMapVote(wasTopmost);
                });
            return;
        }

        if (completeSynchronously)
        {
            FinishMapVote(wasTopmost);
        }
    }

    void PauseMenuWindow::ContinueMapVote(
        void* context, std::exception_ptr error)
    {
        auto& state = *static_cast<PauseMenuWindowMapContinuation*>(context);
        std::shared_ptr<PauseMenuWindow> self = state.Window;
        if (!self)
        {
            return;
        }

        std::exception_ptr bodyError = std::move(error);
        if (!bodyError)
        {
            try
            {
                const MapPickerStringRef roomKey = state.View->RoomKey();
                if (roomKey)
                {
                    MphRead::Mods::Network::MapVote::Propose(
                        std::optional<std::string>(*roomKey));
                    self->CloseSelf();
                }
            }
            catch (...)
            {
                bodyError = std::current_exception();
            }
        }

        try
        {
            CatchThenFinally(std::move(bodyError), WriteMapVoteError,
                [self, wasTopmost = state.WasTopmost]
                {
                    self->FinishMapVote(wasTopmost);
                });
        }
        catch (...)
        {
            self->_adapter->PostAsyncVoidException(std::current_exception());
        }
    }

    void PauseMenuWindow::OnMapPickerClosed(
        void* context, void*, const MapPickerEventArgs&)
    {
        if (context != nullptr)
        {
            static_cast<PauseMenuWindowChildAdapter*>(context)->Close();
        }
    }

    void PauseMenuWindow::FinishMapVote(bool wasTopmost)
    {
        _voteOpen = false;
        _adapter->SetTopmost(wasTopmost);
        _adapter->Activate();
    }

    std::shared_ptr<PauseMenuWindow> PauseMenuWindow::LockEventTarget(void* context)
    {
        if (context == nullptr)
        {
            return {};
        }
        return static_cast<PauseMenuWindowEventTarget*>(context)->Window.lock();
    }

    void PauseMenuWindow::OnResumed(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnFullscreenRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            MphRead::Mods::PauseMenu::RequestFullscreenToggle();
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnSettingsRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            try
            {
                self->OpenSettings();
            }
            catch (...)
            {
                self->_adapter->PostAsyncVoidException(std::current_exception());
            }
        }
    }

    void PauseMenuWindow::OnVoteMapRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            try
            {
                self->OpenMapVote();
            }
            catch (...)
            {
                self->_adapter->PostAsyncVoidException(std::current_exception());
            }
        }
    }

    void PauseMenuWindow::OnSpectateRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            MphRead::Mods::SpectatorMode::Start();
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnRejoinRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            MphRead::Mods::SpectatorMode::Rejoin();
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnRecordToggleRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            if (MphRead::Mods::Network::DemoRecorder::IsRecording())
            {
                const std::optional<std::string> path
                    = MphRead::Mods::Network::DemoRecorder::CurrentPath();
                std::cout << "[demo] recording saved to "
                    << (path ? *path : std::string{}) << '\n';
                MphRead::Mods::Network::DemoRecorder::Stop();
            }
            else
            {
                static_cast<void>(MphRead::Mods::Network::DemoRecorder::Start());
            }
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnLeaveRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            MphRead::Mods::PauseMenu::RequestLeave();
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::OnQuitRequested(
        void* context, void*, const PauseMenuViewEventArgs&)
    {
        if (std::shared_ptr<PauseMenuWindow> self = LockEventTarget(context))
        {
            MphRead::Mods::PauseMenu::RequestQuit();
            self->CloseSelf();
        }
    }

    void PauseMenuWindow::CloseSelf()
    {
        _adapter->Close();
    }

    void PauseMenuWindow::OnOpened(PauseMenuWindowEventArgs& e)
    {
        _adapter->BaseOnOpened(e);
        CoverGameWindow(*_adapter);
        _view->FocusResume();
    }

    void PauseMenuWindow::OnKeyDown(PauseMenuWindowKeyEventArgs& e)
    {
        if (e.Key == PauseMenuWindowKey::Escape)
        {
            CloseSelf();
            e.Handled = true;
            return;
        }
        _adapter->BaseOnKeyDown(e);
    }

    void PauseMenuWindow::OnClosed(PauseMenuWindowEventArgs& e)
    {
        std::shared_ptr<PauseMenuWindow> keepAlive = _open;
        if (keepAlive.get() == this)
        {
            _open.reset();
        }
        else
        {
            keepAlive.reset();
        }
        MphRead::Mods::PauseMenu::MarkClosed();
        _adapter->BaseOnClosed(e);
        if (keepAlive)
        {
            _adapter->ReleaseAfterDispatch(std::move(keepAlive));
        }
    }
}

namespace MphRead::Mods::Detail
{
    void PauseMenuGuiFollowGameWindow()
    {
        MphRead::Mods::Launcher::Gui::PauseMenuWindow::FollowGameWindow();
    }

    bool PauseMenuGuiOpenWindow()
    {
        return MphRead::Mods::Launcher::Gui::PauseMenuWindow::Open();
    }

    void PauseMenuGuiCloseWindowIfOpen()
    {
        MphRead::Mods::Launcher::Gui::PauseMenuWindow::CloseIfOpen();
    }
}

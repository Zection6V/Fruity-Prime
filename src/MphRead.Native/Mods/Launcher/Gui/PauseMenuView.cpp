#include "PauseMenuView.hpp"

#include "../../Network/DemoPlayback.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Network/NetSession.hpp"
#include "../../SpectatorMode.hpp"
#include "../../WindowMode.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace
{
    [[nodiscard]] double Clamp(double value, double min, double max) noexcept
    {
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    struct PauseMenuViewState final
    {
        PauseMenuViewAdapter* Adapter = nullptr;
        PauseMenuView* Sender = nullptr;
        PauseMenuViewControlHandle ResumeControl;
        std::shared_ptr<MenuEntry> Resume;
        PauseMenuViewControlHandle Scaler;
        std::shared_ptr<MenuEntry> WindowEntry;
        double NeededHeight = 0.0;

        PauseMenuViewEvent Resumed;
        PauseMenuViewEvent SettingsRequested;
        PauseMenuViewEvent LeaveRequested;
        PauseMenuViewEvent QuitRequested;
        PauseMenuViewEvent FullscreenRequested;
        PauseMenuViewEvent SpectateRequested;
        PauseMenuViewEvent RejoinRequested;
        PauseMenuViewEvent RecordToggleRequested;
        PauseMenuViewEvent VoteMapRequested;
    };

    const PauseMenuViewEventArgs PauseMenuViewEventArgs::Empty{};

    PauseMenuViewEventHandler::PauseMenuViewEventHandler(
        void* context, Callback function, std::shared_ptr<void> keepAlive)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{context, function, std::move(keepAlive)});
            _invocations = std::move(list);
        }
    }

    PauseMenuViewEventHandler::PauseMenuViewEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    PauseMenuViewEventHandler PauseMenuViewEventHandler::Static(Callback function)
    {
        return PauseMenuViewEventHandler(nullptr, function);
    }

    PauseMenuViewEventHandler PauseMenuViewEventHandler::Instance(
        std::shared_ptr<void> target, Callback function)
    {
        void* context = target.get();
        return PauseMenuViewEventHandler(context, function, std::move(target));
    }

    PauseMenuViewEventHandler PauseMenuViewEventHandler::Combine(
        const PauseMenuViewEventHandler& left,
        const PauseMenuViewEventHandler& right)
    {
        if (left.IsNull())
        {
            return right;
        }
        if (right.IsNull())
        {
            return left;
        }

        auto list = std::make_shared<std::vector<Invocation>>();
        list->reserve(left._invocations->size() + right._invocations->size());
        list->insert(list->end(), left._invocations->begin(), left._invocations->end());
        list->insert(list->end(), right._invocations->begin(), right._invocations->end());
        return PauseMenuViewEventHandler(std::move(list));
    }

    bool PauseMenuViewEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const PauseMenuViewEventHandler& left,
        const PauseMenuViewEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void PauseMenuViewEvent::Add(const PauseMenuViewEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            auto next = std::make_shared<InvocationList>();
            next->reserve((current ? current->size() : 0) + handler._invocations->size());
            if (current)
            {
                next->insert(next->end(), current->begin(), current->end());
            }
            next->insert(next->end(),
                handler._invocations->begin(), handler._invocations->end());
            std::shared_ptr<const InvocationList> desired = std::move(next);
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void PauseMenuViewEvent::Remove(const PauseMenuViewEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            if (!current || current->size() < handler._invocations->size())
            {
                return;
            }

            const std::size_t removeCount = handler._invocations->size();
            std::optional<std::size_t> match;
            for (std::size_t start = current->size() - removeCount + 1; start-- > 0;)
            {
                if (std::equal(handler._invocations->begin(), handler._invocations->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(start)))
                {
                    match = start;
                    break;
                }
            }
            if (!match.has_value())
            {
                return;
            }

            std::shared_ptr<const InvocationList> desired;
            if (removeCount != current->size())
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() - removeCount);
                next->insert(next->end(), current->begin(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match));
                next->insert(next->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(*match + removeCount),
                    current->end());
                desired = std::move(next);
            }

            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void PauseMenuViewEvent::Invoke(
        void* sender, const PauseMenuViewEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Context, sender, args);
        }
    }

    PauseMenuView::PauseMenuView(PauseMenuViewAdapter& adapter, bool offerWindowMode)
        : _adapter(adapter),
          _state(std::make_shared<PauseMenuViewState>())
    {
        _state->Adapter = &_adapter;
        _state->Sender = this;

        constexpr double panelWidth = 420.0;
        const PauseMenuViewControlHandle stack = _adapter.ConstructStackPanel();
        _adapter.SetStackPanelSpacing(stack, 4.0);
        std::vector<PauseMenuViewControlHandle> children;

        PauseMenuViewControlRef<Caption> caption = _adapter.ConstructCaption(u"Paused");
        caption.Value->Height(34.0);
        _adapter.AddPanelChild(stack, caption.Control);
        children.push_back(caption.Control);
        _controlValues.push_back(caption.Value);

        PauseMenuViewControlRef<MenuEntry> resume = Add(_adapter, stack, u"Resume",
            MenuEntryEventHandler{_state.get(), &PauseMenuView::OnResumeClick});
        _state->ResumeControl = resume.Control;
        _state->Resume = resume.Value;
        children.push_back(resume.Control);
        _controlValues.push_back(resume.Value);

        if (offerWindowMode)
        {
            PauseMenuViewControlRef<MenuEntry> windowEntry =
                _adapter.ConstructMenuEntry(WindowLabel(), std::u16string{}, 17.0);
            windowEntry.Value->AddClick(
                MenuEntryEventHandler{_state.get(), &PauseMenuView::OnFullscreenClick});
            _adapter.AddPanelChild(stack, windowEntry.Control);
            _state->WindowEntry = windowEntry.Value;
            children.push_back(windowEntry.Control);
            _controlValues.push_back(windowEntry.Value);
        }

        PauseMenuViewControlRef<MenuEntry> settings = Add(_adapter, stack, u"Settings",
            MenuEntryEventHandler{_state.get(), &PauseMenuView::OnSettingsClick});
        children.push_back(settings.Control);
        _controlValues.push_back(settings.Value);

        if (!Mods::Network::DemoPlayback::IsActive())
        {
            if (Mods::SpectatorMode::IsSpectating())
            {
                PauseMenuViewControlRef<MenuEntry> rejoin = Add(
                    _adapter, stack, u"Rejoin match",
                    MenuEntryEventHandler{_state.get(), &PauseMenuView::OnRejoinClick});
                children.push_back(rejoin.Control);
                _controlValues.push_back(rejoin.Value);
            }
            else if (Mods::SpectatorMode::CanSpectate())
            {
                PauseMenuViewControlRef<MenuEntry> spectate = Add(
                    _adapter, stack, u"Spectate",
                    MenuEntryEventHandler{_state.get(), &PauseMenuView::OnSpectateClick});
                children.push_back(spectate.Control);
                _controlValues.push_back(spectate.Value);
            }
            if (Mods::Network::NetSession::Active())
            {
                PauseMenuViewControlRef<MenuEntry> vote = Add(
                    _adapter, stack, u"Vote maps",
                    MenuEntryEventHandler{_state.get(), &PauseMenuView::OnVoteMapClick});
                children.push_back(vote.Control);
                _controlValues.push_back(vote.Value);

                const std::u16string recordTitle = Mods::Network::DemoRecorder::IsRecording()
                    ? u"Stop recording"
                    : u"Record demo";
                PauseMenuViewControlRef<MenuEntry> record = Add(
                    _adapter, stack, recordTitle,
                    MenuEntryEventHandler{
                        _state.get(), &PauseMenuView::OnRecordToggleClick});
                children.push_back(record.Control);
                _controlValues.push_back(record.Value);
            }
        }

        PauseMenuViewControlRef<MenuEntry> leave = Add(_adapter, stack, u"Leave match",
            MenuEntryEventHandler{_state.get(), &PauseMenuView::OnLeaveClick});
        children.push_back(leave.Control);
        _controlValues.push_back(leave.Value);

        PauseMenuViewControlRef<MenuEntry> quit = Add(_adapter, stack, u"Quit",
            MenuEntryEventHandler{_state.get(), &PauseMenuView::OnQuitClick});
        children.push_back(quit.Control);
        _controlValues.push_back(quit.Value);

        const PauseMenuViewControlHandle panel = _adapter.ConstructBorder();
        _adapter.SetBorderBackground(panel, GuiTheme::PanelBrush);
        _adapter.SetBorderBrush(panel, GuiTheme::EdgeBrush);
        _adapter.SetBorderThickness(panel, PauseMenuViewThickness::Uniform(1.0));
        _adapter.SetBorderPadding(panel, PauseMenuViewThickness{22.0, 18.0, 22.0, 18.0});
        _adapter.SetBorderChild(panel, stack);
        _adapter.SetControlMaxWidth(panel, panelWidth);
        _adapter.SetBorderCornerRadius(panel, 6.0);
        _adapter.SetControlHorizontalAlignment(
            panel, PauseMenuViewHorizontalAlignment::Center);
        _adapter.SetControlVerticalAlignment(
            panel, PauseMenuViewVerticalAlignment::Center);

        double needed = PanelPadding;
        for (const PauseMenuViewControlHandle& child : children)
        {
            const double height = _adapter.GetControlHeight(child);
            needed += (std::isnan(height) ? 0.0 : height) + 4.0;
        }
        _state->NeededHeight = needed;

        const PauseMenuViewControlHandle scaler =
            _adapter.ConstructLayoutTransformControl();
        _adapter.SetLayoutTransformChild(scaler, panel);
        _adapter.SetControlHorizontalAlignment(
            scaler, PauseMenuViewHorizontalAlignment::Center);
        _adapter.SetControlVerticalAlignment(
            scaler, PauseMenuViewVerticalAlignment::Center);
        _state->Scaler = scaler;

        const PauseMenuViewControlHandle scroller = _adapter.ConstructScrollViewer();
        _adapter.SetScrollViewerContent(scroller, scaler);
        _adapter.SetScrollViewerPadding(
            scroller, PauseMenuViewThickness::Uniform(12.0));
        _adapter.SetHorizontalScrollBarVisibility(
            scroller, PauseMenuViewScrollBarVisibility::Disabled);
        _adapter.SetVerticalScrollBarVisibility(
            scroller, PauseMenuViewScrollBarVisibility::Auto);
        _adapter.AddSizeChanged(scroller, PauseMenuViewSizeChangedHandler{
            _state.get(), &PauseMenuView::OnSizeChanged, _state});
        _adapter.SetContent(scroller);
    }

    void PauseMenuView::AddResumed(const PauseMenuViewEventHandler& handler)
    {
        _state->Resumed.Add(handler);
    }

    void PauseMenuView::RemoveResumed(const PauseMenuViewEventHandler& handler)
    {
        _state->Resumed.Remove(handler);
    }

    void PauseMenuView::AddSettingsRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->SettingsRequested.Add(handler);
    }

    void PauseMenuView::RemoveSettingsRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->SettingsRequested.Remove(handler);
    }

    void PauseMenuView::AddLeaveRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->LeaveRequested.Add(handler);
    }

    void PauseMenuView::RemoveLeaveRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->LeaveRequested.Remove(handler);
    }

    void PauseMenuView::AddQuitRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->QuitRequested.Add(handler);
    }

    void PauseMenuView::RemoveQuitRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->QuitRequested.Remove(handler);
    }

    void PauseMenuView::AddFullscreenRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->FullscreenRequested.Add(handler);
    }

    void PauseMenuView::RemoveFullscreenRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->FullscreenRequested.Remove(handler);
    }

    void PauseMenuView::AddSpectateRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->SpectateRequested.Add(handler);
    }

    void PauseMenuView::RemoveSpectateRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->SpectateRequested.Remove(handler);
    }

    void PauseMenuView::AddRejoinRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->RejoinRequested.Add(handler);
    }

    void PauseMenuView::RemoveRejoinRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->RejoinRequested.Remove(handler);
    }

    void PauseMenuView::AddRecordToggleRequested(
        const PauseMenuViewEventHandler& handler)
    {
        _state->RecordToggleRequested.Add(handler);
    }

    void PauseMenuView::RemoveRecordToggleRequested(
        const PauseMenuViewEventHandler& handler)
    {
        _state->RecordToggleRequested.Remove(handler);
    }

    void PauseMenuView::AddVoteMapRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->VoteMapRequested.Add(handler);
    }

    void PauseMenuView::RemoveVoteMapRequested(const PauseMenuViewEventHandler& handler)
    {
        _state->VoteMapRequested.Remove(handler);
    }

    void PauseMenuView::FocusResume()
    {
        _adapter.PostUiThread(
            PauseMenuViewAction{_state.get(), &PauseMenuView::OnFocusPosted, _state},
            PauseMenuViewDispatcherPriority::Background);
    }

    std::u16string PauseMenuView::WindowLabel()
    {
        return Mods::WindowMode::IsFullscreen() ? u"Windowed" : u"Fullscreen";
    }

    PauseMenuViewControlRef<MenuEntry> PauseMenuView::Add(
        PauseMenuViewAdapter& adapter,
        const PauseMenuViewControlHandle& stack,
        std::u16string text,
        MenuEntryEventHandler handler)
    {
        PauseMenuViewControlRef<MenuEntry> entry =
            adapter.ConstructMenuEntry(std::move(text), std::u16string{}, 17.0);
        entry.Value->AddClick(handler);
        adapter.AddPanelChild(stack, entry.Control);
        return entry;
    }

    void PauseMenuView::FitToHost(PauseMenuViewState& state, double height)
    {
        if (height <= 0.0 || state.NeededHeight <= 0.0)
        {
            return;
        }
        const double scale = Clamp(height / state.NeededHeight, 0.5, 1.0);
        const std::optional<double> current =
            state.Adapter->GetLayoutScaleY(state.Scaler);
        if (current.has_value() && std::abs(*current - scale) < 0.001)
        {
            return;
        }
        if (scale >= 1.0)
        {
            state.Adapter->SetLayoutTransform(state.Scaler, std::nullopt);
        }
        else
        {
            state.Adapter->SetLayoutTransform(state.Scaler,
                PauseMenuViewScaleTransform{scale, scale});
        }
    }

    void PauseMenuView::OnResumeClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.Resumed.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnSettingsClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.SettingsRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnLeaveClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.LeaveRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnQuitClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.QuitRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnFullscreenClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.FullscreenRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
        state.WindowEntry->Title(
            Mods::WindowMode::IsFullscreen() ? u"Windowed" : u"Fullscreen");
    }

    void PauseMenuView::OnSpectateClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.SpectateRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnRejoinClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.RejoinRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnRecordToggleClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.RecordToggleRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnVoteMapClick(
        void* context, void*, const MenuEntryEventArgs&)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.VoteMapRequested.Invoke(state.Sender, PauseMenuViewEventArgs::Empty);
    }

    void PauseMenuView::OnSizeChanged(void* context, double newHeight)
    {
        FitToHost(*static_cast<PauseMenuViewState*>(context), newHeight);
    }

    void PauseMenuView::OnFocusPosted(void* context)
    {
        auto& state = *static_cast<PauseMenuViewState*>(context);
        state.Adapter->Focus(state.ResumeControl);
    }
}

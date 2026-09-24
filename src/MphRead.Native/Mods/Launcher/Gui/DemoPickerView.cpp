#include "DemoPickerView.hpp"
#include "NativeRuntime/System/AtomicSharedPtr.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    struct DemoPickerViewState final
    {
        DemoPickerViewAdapter* Adapter = nullptr;
        DemoPickerView* Sender = nullptr;
        ::MphRead::NativeRuntime::AtomicSharedPtr<const std::string> Path{};
        std::atomic<bool> ImportRequested{false};
        DemoPickerViewAdapter::ControlHandle First;
        DemoPickerViewEvent Closed;
    };

    struct DemoPickerViewDemoClickTarget final
    {
        std::shared_ptr<DemoPickerViewState> View;
        std::shared_ptr<const std::string> Path;
    };

    const DemoPickerViewEventArgs DemoPickerViewEventArgs::Empty{};

    DemoPickerViewNullReferenceException::DemoPickerViewNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    DemoPickerViewEventHandler::DemoPickerViewEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    DemoPickerViewEventHandler::DemoPickerViewEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    DemoPickerViewEventHandler DemoPickerViewEventHandler::Combine(
        const DemoPickerViewEventHandler& left,
        const DemoPickerViewEventHandler& right)
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
        return DemoPickerViewEventHandler(std::move(list));
    }

    bool DemoPickerViewEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(
        const DemoPickerViewEventHandler& left,
        const DemoPickerViewEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        const auto& a = *left._invocations;
        const auto& b = *right._invocations;
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    void DemoPickerViewEvent::Add(const DemoPickerViewEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }

        std::shared_ptr<const InvocationList> current = _handlers.load();
        for (;;)
        {
            std::shared_ptr<const InvocationList> desired;
            if (!current)
            {
                desired = handler._invocations;
            }
            else
            {
                auto next = std::make_shared<InvocationList>();
                next->reserve(current->size() + handler._invocations->size());
                next->insert(next->end(), current->begin(), current->end());
                next->insert(next->end(), handler._invocations->begin(),
                    handler._invocations->end());
                desired = std::move(next);
            }
            if (_handlers.compare_exchange_weak(current, desired))
            {
                return;
            }
        }
    }

    void DemoPickerViewEvent::Remove(const DemoPickerViewEventHandler& handler)
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

    void DemoPickerViewEvent::Invoke(
        void* sender, const DemoPickerViewEventArgs& args) const
    {
        const std::shared_ptr<const InvocationList> handlers = _handlers.load();
        if (!handlers)
        {
            return;
        }
        for (const Invocation& handler : *handlers)
        {
            handler.Function(handler.Target.get(), sender, args);
        }
    }

    DemoPickerView::DemoPickerView(DemoPickerViewAdapter& adapter,
        std::shared_ptr<DemoPickerViewDemoList> demos,
        std::optional<std::string> directory)
        : _adapter(adapter),
          _state(std::make_shared<DemoPickerViewState>())
    {
        _state->Adapter = &_adapter;
        _state->Sender = this;

        _adapter.SetBackground(GuiTheme::InkBrush);
        _adapter.SetFocusable(true);

        const DemoPickerViewAdapter::ControlHandle list = _adapter.ConstructStackPanel();
        _adapter.SetStackPanelSpacing(list, 2.0);

        if (!demos)
        {
            throw DemoPickerViewNullReferenceException();
        }

        const std::shared_ptr<DemoPickerViewDemoEnumerator> enumerator = demos->GetEnumerator();
        try
        {
            for (;;)
            {
                if (!enumerator)
                {
                    throw DemoPickerViewNullReferenceException();
                }
                if (!enumerator->MoveNext())
                {
                    break;
                }

                const Mods::Network::DemoRecording demo = enumerator->Current();

                std::string title;
                if (!demo.Room().empty())
                {
                    title = demo.Room();
                }
                else
                {
                    title = demo.FileName();
                }
                std::string subtitle = Mods::Network::DemoLibrary::Describe(demo);
                const DemoPickerViewAdapter::ControlHandle entry =
                    _adapter.ConstructMenuEntry(std::move(title), std::move(subtitle), 15.0);

                auto path = std::make_shared<const std::string>(demo.Path());
                auto clickTarget = std::make_shared<DemoPickerViewDemoClickTarget>(
                    DemoPickerViewDemoClickTarget{_state, std::move(path)});
                _adapter.AddMenuEntryClick(entry,
                    DemoPickerViewAction{std::move(clickTarget), &DemoPickerView::OnDemoClick});

                if (!_state->First)
                {
                    _state->First = entry;
                }
                _adapter.AddPanelChild(list, entry);
            }
        }
        catch (...)
        {
            if (enumerator)
            {
                enumerator->Dispose();
            }
            throw;
        }
        if (enumerator)
        {
            enumerator->Dispose();
        }

        if (demos->Count() == 0)
        {
            const DemoPickerViewAdapter::ControlHandle empty = _adapter.ConstructTextBlock();
            std::string message =
                "Nothing recorded yet. Recordings are made from the pause menu "
                "during an online match, and are written to:\n";
            if (directory.has_value())
            {
                message += *directory;
            }
            _adapter.SetTextBlockText(empty, std::move(message));
            _adapter.SetTextBlockForeground(empty, GuiTheme::TextDimBrush);
            _adapter.SetTextBlockFontSize(empty, 12.0);
            _adapter.SetTextBlockTextWrapping(empty, DemoPickerViewTextWrapping::Wrap);
            _adapter.SetControlMargin(empty, DemoPickerViewThickness{4.0, 6.0, 4.0, 12.0});
            _adapter.AddPanelChild(list, empty);
        }

        const DemoPickerViewAdapter::ControlHandle import = _adapter.ConstructMenuEntry(
            "Open a file...", "A demo from somewhere else on this device", 13.0);
        _adapter.SetMenuEntryAccent(import, GuiTheme::TextDim);
        _adapter.SetControlMargin(import, DemoPickerViewThickness{0.0, 10.0, 0.0, 0.0});
        _adapter.AddMenuEntryClick(import,
            DemoPickerViewAction{_state, &DemoPickerView::OnImportClick});
        if (!_state->First)
        {
            _state->First = import;
        }
        _adapter.AddPanelChild(list, import);

        const DemoPickerViewAdapter::ControlHandle back =
            _adapter.ConstructMenuEntry("Back", "", 13.0);
        _adapter.SetMenuEntryAccent(back, GuiTheme::TextDim);
        _adapter.SetControlWidth(back, 120.0);
        _adapter.SetControlHorizontalAlignment(
            back, DemoPickerViewHorizontalAlignment::Right);
        _adapter.SetControlVerticalAlignment(
            back, DemoPickerViewVerticalAlignment::Center);
        _adapter.AddMenuEntryClick(back,
            DemoPickerViewAction{_state, &DemoPickerView::OnBackClick});

        const DemoPickerViewAdapter::ControlHandle title = _adapter.ConstructTextBlock();
        _adapter.SetTextBlockText(title, "Demos");
        _adapter.SetTextBlockFontFamily(title, GuiTheme::Display);
        _adapter.SetTextBlockFontSize(title, 18.0);
        _adapter.SetTextBlockForeground(title, GuiTheme::TextBrush);
        _adapter.SetControlVerticalAlignment(
            title, DemoPickerViewVerticalAlignment::Center);

        const DemoPickerViewAdapter::ControlHandle bar = _adapter.ConstructGrid();
        const DemoPickerViewAdapter::ColumnDefinitionsHandle columns =
            _adapter.ConstructColumnDefinitions("*,Auto");
        _adapter.SetGridColumnDefinitions(bar, columns);
        _adapter.SetGridColumn(title, 0);
        _adapter.SetGridColumn(back, 1);
        _adapter.AddPanelChild(bar, title);
        _adapter.AddPanelChild(bar, back);

        const DemoPickerViewAdapter::ControlHandle header = _adapter.ConstructBorder();
        _adapter.SetBorderBackground(header, GuiTheme::PanelBrush);
        _adapter.SetBorderPadding(
            header, DemoPickerViewThickness{18.0, 12.0, 12.0, 12.0});
        _adapter.SetBorderChild(header, bar);

        const DemoPickerViewAdapter::ControlHandle body = _adapter.ConstructScrollViewer();
        _adapter.SetScrollViewerContent(body, list);
        _adapter.SetScrollViewerPadding(body, DemoPickerViewThickness::Uniform(14.0));
        _adapter.SetScrollViewerHorizontalScrollBarVisibility(
            body, DemoPickerViewScrollBarVisibility::Disabled);

        const DemoPickerViewAdapter::ControlHandle dock = _adapter.ConstructDockPanel();
        _adapter.SetDockPanelLastChildFill(dock, true);
        _adapter.SetDock(header, DemoPickerViewDock::Top);
        _adapter.AddPanelChild(dock, header);
        _adapter.AddPanelChild(dock, body);
        _adapter.SetContent(dock);
    }

    std::shared_ptr<const std::string> DemoPickerView::Path() const noexcept
    {
        return _state->Path.load(std::memory_order_relaxed);
    }

    bool DemoPickerView::ImportRequested() const noexcept
    {
        return _state->ImportRequested.load(std::memory_order_relaxed);
    }

    void DemoPickerView::AddClosed(const DemoPickerViewEventHandler& handler)
    {
        _state->Closed.Add(handler);
    }

    void DemoPickerView::RemoveClosed(const DemoPickerViewEventHandler& handler)
    {
        _state->Closed.Remove(handler);
    }

    void DemoPickerView::OnAttachedToVisualTree(
        DemoPickerViewVisualTreeAttachmentEventArgs& e)
    {
        _adapter.BaseOnAttachedToVisualTree(e);
        _adapter.PostUiThread(
            DemoPickerViewAction{_state, &DemoPickerView::OnFocusPosted},
            DemoPickerViewDispatcherPriority::Background);
    }

    void DemoPickerView::OnKeyDown(DemoPickerViewKeyEventArgs& e)
    {
        if (e.Key == DemoPickerViewKey::Escape)
        {
            _state->Closed.Invoke(this, DemoPickerViewEventArgs::Empty);
            e.Handled = true;
            return;
        }
        _adapter.BaseOnKeyDown(e);
    }

    void DemoPickerView::OnDemoClick(void* target)
    {
        auto& click = *static_cast<DemoPickerViewDemoClickTarget*>(target);
        click.View->Path.store(click.Path, std::memory_order_relaxed);
        click.View->Closed.Invoke(click.View->Sender, DemoPickerViewEventArgs::Empty);
    }

    void DemoPickerView::OnImportClick(void* target)
    {
        auto& state = *static_cast<DemoPickerViewState*>(target);
        state.ImportRequested.store(true, std::memory_order_relaxed);
        state.Closed.Invoke(state.Sender, DemoPickerViewEventArgs::Empty);
    }

    void DemoPickerView::OnBackClick(void* target)
    {
        auto& state = *static_cast<DemoPickerViewState*>(target);
        state.Closed.Invoke(state.Sender, DemoPickerViewEventArgs::Empty);
    }

    void DemoPickerView::OnFocusPosted(void* target)
    {
        auto& state = *static_cast<DemoPickerViewState*>(target);
        if (state.First)
        {
            state.Adapter->Focus(state.First);
        }
    }
}

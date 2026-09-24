#include "HomeViewHost.hpp"

#include "HostViews.hpp"

#include "../Gui/Host.hpp"
#include "../../Mods/ThumbnailBatch.hpp"
#include "../../Mods/ThumbnailGenerator.hpp"
#include "../../Mods/Network/NetHostSession.hpp"
#include "../../Mods/Network/NetLaunch.hpp"
#include "../../Mods/Network/NetMaster.hpp"
#include "../../Mods/Network/NetSession.hpp"
#include "../../Mods/Network/NetStatus.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
// windows.h makes DeleteFile a macro for one of its own two entry points,
// which would rename the adapter method this file defines.
#undef DeleteFile
#endif

namespace MphRead::NativeRuntime::Avalonia
{
    namespace
    {
        [[nodiscard]] Toolkit::ElementPtr P(const Launcher::HomeViewElement& element)
        {
            return std::static_pointer_cast<Toolkit::Element>(element.Native);
        }

        [[nodiscard]] Toolkit::Element* E(const Launcher::HomeViewElement& element)
        {
            return static_cast<Toolkit::Element*>(element.Native.get());
        }

        [[nodiscard]] Launcher::HomeViewElement Wrap(Toolkit::ElementPtr element)
        {
            return Launcher::HomeViewElement{std::move(element)};
        }

        [[nodiscard]] Toolkit::Thickness Thick(
            Launcher::HomeViewThickness value) noexcept
        {
            return Toolkit::Thickness{value.Left, value.Top, value.Right, value.Bottom};
        }

        // A HomeViewTask is whatever the host says one is; this is the one.
        class TaskHandle final : public Launcher::HomeViewTask
        {
        public:
            explicit TaskHandle(std::shared_ptr<HostTask> task) noexcept
                : Task(std::move(task))
            {
            }

            std::shared_ptr<HostTask> Task;
        };

        [[nodiscard]] Launcher::HomeViewTaskRef Ref(std::shared_ptr<HostTask> task)
        {
            return std::make_shared<TaskHandle>(std::move(task));
        }

        [[nodiscard]] std::shared_ptr<HostTask> Task(
            const Launcher::HomeViewTaskRef& task)
        {
            const auto handle = std::static_pointer_cast<TaskHandle>(task);
            return handle == nullptr ? nullptr : handle->Task;
        }

        // The room list HomeView enumerates.
        class RoomList final : public Launcher::HomeViewRoomList
        {
        public:
            explicit RoomList(std::vector<std::string> rooms) noexcept
                : _rooms(std::move(rooms))
            {
            }

            [[nodiscard]] std::shared_ptr<Launcher::HomeViewRoomEnumerator>
                GetEnumerator() const override
            {
                return std::make_shared<Enumerator>(_rooms);
            }

        private:
            class Enumerator final : public Launcher::HomeViewRoomEnumerator
            {
            public:
                explicit Enumerator(const std::vector<std::string>& rooms) noexcept
                    : _rooms(rooms)
                {
                }

                [[nodiscard]] bool MoveNext() override
                {
                    ++_index;
                    return _index < static_cast<std::ptrdiff_t>(_rooms.size());
                }

                [[nodiscard]] std::string Current() const override
                {
                    return _rooms[static_cast<std::size_t>(_index)];
                }

                void Dispose() override {}

            private:
                const std::vector<std::string>& _rooms;
                std::ptrdiff_t _index = -1;
            };

            std::vector<std::string> _rooms;
        };

        // The root of the view: a control that measures its one child and
        // says so when the width it is given changes, which is the size
        // changed event HomeView lays itself out from.
        class RootBehaviour final : public Toolkit::CustomBehaviour
        {
        public:
            std::vector<std::function<void(double)>> SizeChanged;

            [[nodiscard]] Toolkit::Size Measure(
                Toolkit::Element& element, Toolkit::Size available) override
            {
                if (available.Width != _width)
                {
                    _width = available.Width;
                    const std::vector<std::function<void(double)>> handlers
                        = SizeChanged;
                    for (const std::function<void(double)>& handler : handlers)
                    {
                        handler(available.Width);
                    }
                }
                for (const Toolkit::ElementPtr& child : element.Children())
                {
                    child->Measure(available);
                }
                return available;
            }

            void Arrange(Toolkit::Element& element, Toolkit::Rect bounds) override
            {
                for (const Toolkit::ElementPtr& child : element.Children())
                {
                    child->Arrange(bounds);
                }
            }

        private:
            double _width = -1.0;
        };

        void FocusFirst(Toolkit::Element& element)
        {
            if (element.Focusable && element.Visible)
            {
                if (Toolkit::Window* const window = Toolkit::Window::Of(element))
                {
                    window->Focus(&element);
                }
                return;
            }
            for (const Toolkit::ElementPtr& child : element.Children())
            {
                if (child->Visible)
                {
                    FocusFirst(*child);
                }
            }
        }

        [[nodiscard]] Launcher::HomeViewServerStatus Convert(
            const MphRead::Mods::Network::ServerStatus& status)
        {
            Launcher::HomeViewServerStatus result;
            result.Online = status.Online;
            result.RoomKey = status.RoomKey;
            result.Mode = status.Mode;
            result.Players = status.Players;
            result.MaxPlayers = status.MaxPlayers;
            result.Latency = status.Latency;
            return result;
        }

        [[nodiscard]] MphRead::Mods::Network::ServerStatus Convert(
            const Launcher::HomeViewServerStatus& status)
        {
            MphRead::Mods::Network::ServerStatus result;
            result.Online = status.Online;
            result.RoomKey = status.RoomKey;
            result.Mode = status.Mode;
            result.Players = status.Players;
            result.MaxPlayers = status.MaxPlayers;
            result.Latency = status.Latency;
            return result;
        }

        // The file the picker hands back is its path and nothing else.
        [[nodiscard]] std::string PathOf(const Launcher::HomeViewStorageFile& file)
        {
            const auto* const value = static_cast<const std::string*>(file.Native.get());
            return value != nullptr ? *value : std::string();
        }

        [[nodiscard]] std::vector<std::string> ChooseFiles(
            const Launcher::HomeViewFilePickerOptions& options)
        {
            std::vector<std::string> chosen;
#if defined(_WIN32)
            std::wstring filter;
            if (options.FileTypeFilter.has_value())
            {
                for (const Launcher::HomeViewFilePickerFilter& entry :
                    *options.FileTypeFilter)
                {
                    filter.append(entry.Name.begin(), entry.Name.end());
                    filter.push_back(L'\0');
                    bool first = true;
                    for (const std::string& pattern : entry.Patterns)
                    {
                        if (!first)
                        {
                            filter.push_back(L';');
                        }
                        first = false;
                        filter.append(pattern.begin(), pattern.end());
                    }
                    filter.push_back(L'\0');
                }
            }
            filter.append(L"All files");
            filter.push_back(L'\0');
            filter.append(L"*.*");
            filter.push_back(L'\0');
            filter.push_back(L'\0');

            const std::wstring title(options.Title.begin(), options.Title.end());
            std::vector<wchar_t> buffer(4096, L'\0');
            OPENFILENAMEW open{};
            open.lStructSize = sizeof(open);
            open.lpstrFilter = filter.c_str();
            open.lpstrFile = buffer.data();
            open.nMaxFile = static_cast<DWORD>(buffer.size());
            open.lpstrTitle = title.empty() ? nullptr : title.c_str();
            open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (::GetOpenFileNameW(&open) == FALSE)
            {
                return chosen;
            }
            const std::wstring wide(buffer.data());
            const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            std::string path(static_cast<std::size_t>(size), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                static_cast<int>(wide.size()), path.data(), size, nullptr, nullptr);
            chosen.push_back(std::move(path));
#else
            // No portable picker: zenity is what a desktop Linux has, and a
            // machine without one gets nothing rather than a wrong answer.
            std::string command = "zenity --file-selection";
            if (!options.Title.empty())
            {
                command += " --title=\"" + options.Title + "\"";
            }
            command += " 2>/dev/null";
            std::unique_ptr<std::FILE, int (*)(std::FILE*)> pipe(
                ::popen(command.c_str(), "r"), &::pclose);
            if (pipe == nullptr)
            {
                return chosen;
            }
            std::string line;
            int c = 0;
            while ((c = std::fgetc(pipe.get())) != EOF)
            {
                if (c == '\n')
                {
                    break;
                }
                line.push_back(static_cast<char>(c));
            }
            if (!line.empty())
            {
                chosen.push_back(std::move(line));
            }
#endif
            return chosen;
        }
    }

    // --- construction -----------------------------------------------------

    Toolkit::ElementPtr HomeViewHost::Create(
        std::shared_ptr<MphRead::MenuSettings> settings,
        const std::vector<std::string>& rooms)
    {
        auto host = std::make_shared<HomeViewHost>();
        Toolkit::ElementPtr element
            = Toolkit::Element::Create(Toolkit::ElementKind::Custom);
        element->Behaviour = std::make_shared<RootBehaviour>();
        element->Tag = host;
        host->_visual = element.get();
        host->_view = std::make_unique<Launcher::HomeView>(*host, std::move(settings),
            std::make_shared<const RoomList>(rooms));
        return element;
    }

    Toolkit::ElementPtr HomeViewHost::Content() const
    {
        return _visual->Children().empty() ? nullptr : _visual->Children().front();
    }

    // --- UserControl, dispatcher and tasks --------------------------------

    void HomeViewHost::SetBackground(const Launcher::GuiBrush& brush)
    {
        _visual->Background = ToColor(brush);
    }

    void HomeViewHost::SetContent(const Launcher::HomeViewElement& content)
    {
        _visual->ClearChildren();
        if (content)
        {
            _visual->AddChild(P(content));
        }
    }

    void HomeViewHost::AddSizeChanged(SizeChangedAction action)
    {
        static_cast<RootBehaviour*>(_visual->Behaviour.get())
            ->SizeChanged.push_back(std::move(action));
    }

    void HomeViewHost::BaseOnKeyDown(Launcher::HomeViewKeyEventArgs& e)
    {
        (void)e;
    }

    void HomeViewHost::Post(Action action, Launcher::HomeViewDispatcherPriority priority)
    {
        Toolkit::Dispatcher::Instance().Post(std::move(action),
            priority == Launcher::HomeViewDispatcherPriority::Background
                ? Toolkit::Priority::Background
                : Toolkit::Priority::Normal);
    }

    Launcher::HomeViewTaskRef HomeViewHost::CompletedTask()
    {
        return Ref(HostTask::Completed());
    }

    Launcher::HomeViewTaskRef HomeViewHost::FaultedTask(std::exception_ptr error)
    {
        return Ref(HostTask::Faulted(std::move(error)));
    }

    Launcher::HomeViewTaskRef HomeViewHost::PendingTask()
    {
        return Ref(HostTask::Pending());
    }

    bool HomeViewHost::TrySetTaskResult(const Launcher::HomeViewTaskRef& task)
    {
        const std::shared_ptr<HostTask> value = Task(task);
        return value != nullptr && value->Complete();
    }

    Launcher::HomeViewTaskRef HomeViewHost::RunBackground(Action action)
    {
        return Ref(HostTask::Run(std::move(action)));
    }

    Launcher::HomeViewTaskRef HomeViewHost::ContinueWithTask(
        const Launcher::HomeViewTaskRef& task,
        std::function<Launcher::HomeViewTaskRef()> continuation)
    {
        auto result = HostTask::Pending();
        Task(task)->Then(
            [result, continuation = std::move(continuation)](
                HostTask::State, std::exception_ptr)
            {
                Launcher::HomeViewTaskRef inner;
                try
                {
                    inner = continuation();
                }
                catch (...)
                {
                    result->Fail(std::current_exception());
                    return;
                }
                if (inner == nullptr)
                {
                    result->Complete();
                    return;
                }
                Task(inner)->Then(
                    [result](HostTask::State state, std::exception_ptr error)
                    {
                        if (state == HostTask::State::Faulted)
                        {
                            result->Fail(std::move(error));
                            return;
                        }
                        result->Complete();
                    });
            });
        return Ref(result);
    }

    Launcher::HomeViewTaskRef HomeViewHost::ContinueWithAction(
        const Launcher::HomeViewTaskRef& task, Action continuation)
    {
        auto result = HostTask::Pending();
        Task(task)->Then(
            [result, continuation = std::move(continuation)](
                HostTask::State, std::exception_ptr)
            {
                try
                {
                    if (continuation)
                    {
                        continuation();
                    }
                }
                catch (...)
                {
                    result->Fail(std::current_exception());
                    return;
                }
                result->Complete();
            });
        return Ref(result);
    }

    Launcher::HomeViewTaskRef HomeViewHost::CatchTask(
        const Launcher::HomeViewTaskRef& task,
        std::function<Launcher::HomeViewTaskRef(std::exception_ptr)> handler)
    {
        auto result = HostTask::Pending();
        Task(task)->Then(
            [result, handler = std::move(handler)](
                HostTask::State state, std::exception_ptr error)
            {
                if (state != HostTask::State::Faulted)
                {
                    result->Complete();
                    return;
                }
                Launcher::HomeViewTaskRef inner;
                try
                {
                    inner = handler(std::move(error));
                }
                catch (...)
                {
                    result->Fail(std::current_exception());
                    return;
                }
                if (inner == nullptr)
                {
                    result->Complete();
                    return;
                }
                Task(inner)->Then(
                    [result](HostTask::State inner, std::exception_ptr failure)
                    {
                        if (inner == HostTask::State::Faulted)
                        {
                            result->Fail(std::move(failure));
                            return;
                        }
                        result->Complete();
                    });
            });
        return Ref(result);
    }

    void HomeViewHost::StartAsyncVoid(std::function<Launcher::HomeViewTaskRef()> body)
    {
        if (!body)
        {
            return;
        }
        // An async void method runs to its first await on the calling thread.
        const Launcher::HomeViewTaskRef task = body();
        Forget(task);
    }

    void HomeViewHost::Forget(const Launcher::HomeViewTaskRef& task)
    {
        if (task == nullptr)
        {
            return;
        }
        // Nothing waits on it, but a fault must not be lost the way an
        // unobserved Task's would be.
        _kept.push_back(task);
        Task(task)->Then(
            [](HostTask::State state, std::exception_ptr error)
            {
                if (state != HostTask::State::Faulted)
                {
                    return;
                }
                try
                {
                    std::rethrow_exception(error);
                }
                catch (const std::exception& failure)
                {
                    std::cout << "[launcher] " << failure.what() << '\n';
                }
                catch (...)
                {
                }
            });
    }

    // --- the control tree -------------------------------------------------

    Launcher::HomeViewElement HomeViewHost::CreatePanel()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::Panel));
    }

    Launcher::HomeViewElement HomeViewHost::CreateGrid()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::Grid));
    }

    Launcher::HomeViewElement HomeViewHost::CreateScrollViewer()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::ScrollViewer));
    }

    Launcher::HomeViewElement HomeViewHost::CreateBorder()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::Border));
    }

    Launcher::HomeViewElement HomeViewHost::CreateStackPanel()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::StackPanel));
    }

    Launcher::HomeViewElement HomeViewHost::CreateTextBlock()
    {
        return Wrap(Toolkit::Element::Create(Toolkit::ElementKind::TextBlock));
    }

    void HomeViewHost::AddChild(const Launcher::HomeViewElement& parent,
        const Launcher::HomeViewElement& child)
    {
        E(parent)->AddChild(P(child));
    }

    void HomeViewHost::InsertChild(const Launcher::HomeViewElement& parent,
        std::int32_t index, const Launcher::HomeViewElement& child)
    {
        E(parent)->InsertChild(index, P(child));
    }

    void HomeViewHost::ClearChildren(const Launcher::HomeViewElement& parent)
    {
        E(parent)->ClearChildren();
    }

    void HomeViewHost::SetGridRowDefinitions(
        const Launcher::HomeViewElement& grid, std::string_view definitions)
    {
        E(grid)->RowDefinitions = Toolkit::ParseGridDefinitions(definitions);
    }

    void HomeViewHost::SetGridColumnDefinitions(
        const Launcher::HomeViewElement& grid, std::string_view definitions)
    {
        E(grid)->ColumnDefinitions = Toolkit::ParseGridDefinitions(definitions);
    }

    void HomeViewHost::SetGridRow(
        const Launcher::HomeViewElement& child, std::int32_t row)
    {
        E(child)->GridRow = row;
    }

    void HomeViewHost::SetGridColumn(
        const Launcher::HomeViewElement& child, std::int32_t column)
    {
        E(child)->GridColumn = column;
    }

    void HomeViewHost::SetScrollContent(const Launcher::HomeViewElement& scroll,
        const Launcher::HomeViewElement& content)
    {
        E(scroll)->ClearChildren();
        E(scroll)->AddChild(P(content));
    }

    void HomeViewHost::DisableHorizontalScrollBar(
        const Launcher::HomeViewElement& scroll)
    {
        E(scroll)->HorizontalScrollDisabled = true;
    }

    void HomeViewHost::SetPanelBackground(
        const Launcher::HomeViewElement& panel, const Launcher::GuiBrush& brush)
    {
        E(panel)->Background = ToColor(brush);
    }

    void HomeViewHost::SetBorderBackground(
        const Launcher::HomeViewElement& border, const Launcher::GuiBrush& brush)
    {
        E(border)->Background = ToColor(brush);
    }

    void HomeViewHost::SetBorderBrush(
        const Launcher::HomeViewElement& border, const Launcher::GuiBrush& brush)
    {
        E(border)->BorderColor = ToColor(brush);
    }

    void HomeViewHost::SetBorderThickness(const Launcher::HomeViewElement& border,
        Launcher::HomeViewThickness thickness)
    {
        E(border)->BorderThickness = Thick(thickness);
    }

    void HomeViewHost::SetBorderCornerRadius(
        const Launcher::HomeViewElement& border, double radius)
    {
        E(border)->CornerRadius = radius;
    }

    void HomeViewHost::SetBorderPadding(
        const Launcher::HomeViewElement& border, Launcher::HomeViewThickness padding)
    {
        E(border)->Padding = Thick(padding);
    }

    void HomeViewHost::SetBorderChild(const Launcher::HomeViewElement& border,
        const Launcher::HomeViewElement& child)
    {
        E(border)->ClearChildren();
        E(border)->AddChild(P(child));
    }

    void HomeViewHost::SetMargin(
        const Launcher::HomeViewElement& element, Launcher::HomeViewThickness margin)
    {
        E(element)->Margin = Thick(margin);
    }

    void HomeViewHost::SetWidth(const Launcher::HomeViewElement& element, double width)
    {
        E(element)->Width = width;
    }

    void HomeViewHost::SetWidthAuto(const Launcher::HomeViewElement& element)
    {
        E(element)->Width.reset();
    }

    void HomeViewHost::SetHeight(const Launcher::HomeViewElement& element, double height)
    {
        E(element)->Height = height;
    }

    void HomeViewHost::SetHeightAuto(const Launcher::HomeViewElement& element)
    {
        E(element)->Height.reset();
    }

    void HomeViewHost::SetIsVisible(
        const Launcher::HomeViewElement& element, bool visible)
    {
        E(element)->Visible = visible;
    }

    bool HomeViewHost::GetIsVisible(const Launcher::HomeViewElement& element) const
    {
        return E(element)->Visible;
    }

    void HomeViewHost::SetHorizontalAlignment(const Launcher::HomeViewElement& element,
        Launcher::HomeViewHorizontalAlignment alignment)
    {
        E(element)->Horizontal
            = alignment == Launcher::HomeViewHorizontalAlignment::Right
            ? Toolkit::HorizontalAlignment::Right
            : Toolkit::HorizontalAlignment::Left;
    }

    void HomeViewHost::SetVerticalAlignment(const Launcher::HomeViewElement& element,
        Launcher::HomeViewVerticalAlignment alignment)
    {
        (void)alignment;
        E(element)->Vertical = Toolkit::VerticalAlignment::Bottom;
    }

    void HomeViewHost::SetCursor(
        const Launcher::HomeViewElement& element, Launcher::HomeViewCursor cursor)
    {
        E(element)->CursorKind = cursor == Launcher::HomeViewCursor::Hand
            ? Toolkit::Cursor::Hand
            : Toolkit::Cursor::Arrow;
    }

    void HomeViewHost::SetStackSpacing(
        const Launcher::HomeViewElement& stack, double spacing)
    {
        E(stack)->Spacing = spacing;
    }

    void HomeViewHost::SetStackOrientation(const Launcher::HomeViewElement& stack,
        Launcher::HomeViewOrientation orientation)
    {
        (void)orientation;
        E(stack)->StackOrientation = Toolkit::Orientation::Horizontal;
    }

    void HomeViewHost::Focus(const Launcher::HomeViewElement& element)
    {
        Toolkit::Element* const target = E(element);
        if (Toolkit::Window* const window = Toolkit::Window::Of(*target))
        {
            window->Focus(target);
        }
    }

    void HomeViewHost::FocusFirstFocusableDescendant(
        const Launcher::HomeViewElement& element)
    {
        FocusFirst(*E(element));
    }

    void HomeViewHost::AddPointerPressed(
        const Launcher::HomeViewElement& element, PointerAction action)
    {
        E(element)->PointerPressed
            = [action = std::move(action)](Toolkit::PointerEvent& e)
        {
            Launcher::HomeViewPointerEventArgs args{&e, false};
            action(args);
            e.Handled = args.Handled;
        };
    }

    void HomeViewHost::SetToolTip(
        const Launcher::HomeViewElement& element, std::string text)
    {
        E(element)->ToolTip = std::move(text);
    }

    void HomeViewHost::SetText(
        const Launcher::HomeViewElement& textBlock, std::optional<std::string> text)
    {
        E(textBlock)->Text = std::move(text);
    }

    void HomeViewHost::SetFontSize(
        const Launcher::HomeViewElement& textBlock, double size)
    {
        E(textBlock)->FontSize = size;
    }

    void HomeViewHost::SetForeground(
        const Launcher::HomeViewElement& element, const Launcher::GuiBrush& brush)
    {
        E(element)->Foreground = ToColor(brush);
    }

    void HomeViewHost::SetForegroundColor(
        const Launcher::HomeViewElement& element, Launcher::GuiColor color)
    {
        E(element)->Foreground = ToColor(color);
    }

    // --- the launcher's own controls --------------------------------------

    Launcher::HomeViewElement HomeViewHost::CreateSplashView()
    {
        return Wrap(SplashViewHost::Create());
    }

    void HomeViewHost::SplashShowRoom(const Launcher::HomeViewElement& splash,
        std::optional<std::string> roomKey)
    {
        HostOf<SplashViewHost>(P(splash))->Splash().ShowRoom(roomKey);
    }

    void HomeViewHost::SplashBottomInset(
        const Launcher::HomeViewElement& splash, double value)
    {
        HostOf<SplashViewHost>(P(splash))->Splash().BottomInset(value);
    }

    Launcher::HomeViewElement HomeViewHost::CreateUpdateBadge()
    {
        return Wrap(UpdateBadgeHost::Create());
    }

    void HomeViewHost::AddUpdateBadgeClick(
        const Launcher::HomeViewElement& badge, Action action)
    {
        HostOf<UpdateBadgeHost>(P(badge))->Click(std::move(action));
    }

    void HomeViewHost::UpdateBadgeShow(
        const Launcher::HomeViewElement& badge, std::optional<std::string> subtitle)
    {
        HostOf<UpdateBadgeHost>(P(badge))->Badge().Show(
            subtitle.has_value() ? std::optional<std::u16string>(Utf16(*subtitle))
                                 : std::nullopt);
    }

    void HomeViewHost::UpdateBadgeSay(
        const Launcher::HomeViewElement& badge, std::optional<std::string> subtitle)
    {
        HostOf<UpdateBadgeHost>(P(badge))->Badge().Say(
            subtitle.has_value() ? std::optional<std::u16string>(Utf16(*subtitle))
                                 : std::nullopt);
    }

    double HomeViewHost::UpdateBadgeDesiredHeight(
        const Launcher::HomeViewElement& badge) const
    {
        return HostOf<UpdateBadgeHost>(P(badge))->Badge().DesiredSize().Height;
    }

    Launcher::HomeViewElement HomeViewHost::CreateMenuEntry(
        std::optional<std::string> title, std::optional<std::string> subtitle,
        double titleSize)
    {
        return Wrap(MenuEntryHost::Create(
            title.has_value() ? std::optional<std::u16string>(Utf16(*title))
                              : std::nullopt,
            subtitle.has_value() ? std::optional<std::u16string>(Utf16(*subtitle))
                                 : std::nullopt,
            titleSize));
    }

    void HomeViewHost::AddMenuEntryClick(
        const Launcher::HomeViewElement& entry, Action action)
    {
        HostOf<MenuEntryHost>(P(entry))->Click(std::move(action));
    }

    void HomeViewHost::MenuEntryTitle(
        const Launcher::HomeViewElement& entry, std::optional<std::string> title)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().Title(
            title.has_value() ? std::optional<std::u16string>(Utf16(*title))
                              : std::nullopt);
    }

    void HomeViewHost::MenuEntrySubtitle(
        const Launcher::HomeViewElement& entry, std::optional<std::string> subtitle)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().Subtitle(
            subtitle.has_value() ? std::optional<std::u16string>(Utf16(*subtitle))
                                 : std::nullopt);
    }

    void HomeViewHost::MenuEntryAccent(
        const Launcher::HomeViewElement& entry, Launcher::GuiColor color)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().Accent(color);
    }

    void HomeViewHost::MenuEntrySubtitleColor(
        const Launcher::HomeViewElement& entry, Launcher::GuiColor color)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().SubtitleColor(color);
    }

    void HomeViewHost::MenuEntryPrimary(
        const Launcher::HomeViewElement& entry, bool value)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().Primary(value);
    }

    void HomeViewHost::MenuEntryEnabled(
        const Launcher::HomeViewElement& entry, bool value)
    {
        HostOf<MenuEntryHost>(P(entry))->Entry().IsEnabled(value);
    }

    Launcher::HomeViewElement HomeViewHost::CreateProgressRow()
    {
        return Wrap(ProgressRowHost::Create());
    }

    void HomeViewHost::ProgressRowSet(
        const Launcher::HomeViewElement& row, double fraction, std::string stage)
    {
        HostOf<ProgressRowHost>(P(row))->Row().Set(fraction, stage);
    }

    Launcher::HomeViewElement HomeViewHost::CreateCaption(std::string text)
    {
        return Wrap(CaptionHost::Create(Utf16(text)));
    }

    Launcher::HomeViewElement HomeViewHost::CreateNote(
        std::string text, std::optional<Launcher::GuiColor> color)
    {
        return Wrap(NoteHost::Create(Utf16(text), color));
    }

    std::optional<std::string> HomeViewHost::NoteText(
        const Launcher::HomeViewElement& note) const
    {
        const std::optional<std::u16string> text
            = HostOf<NoteHost>(P(note))->Note().Text();
        return text.has_value() ? std::optional<std::string>(Utf8(*text))
                                : std::nullopt;
    }

    void HomeViewHost::NoteText(
        const Launcher::HomeViewElement& note, std::optional<std::string> text)
    {
        if (!text.has_value())
        {
            HostOf<NoteHost>(P(note))->Note().Text(std::nullopt);
            return;
        }
        const std::u16string value = Utf16(*text);
        HostOf<NoteHost>(P(note))->Note().Text(value);
    }

    void HomeViewHost::NoteForeground(
        const Launcher::HomeViewElement& note, const Launcher::GuiBrush& brush)
    {
        HostOf<NoteHost>(P(note))->Note().Foreground(
            Launcher::RowsBrush::Solid(brush.Color));
    }

    void HomeViewHost::NoteForegroundColor(
        const Launcher::HomeViewElement& note, Launcher::GuiColor color)
    {
        HostOf<NoteHost>(P(note))->Note().Foreground(
            Launcher::RowsBrush::Solid(color));
    }

    Launcher::HomeViewElement HomeViewHost::CreateChoiceRow(std::string label,
        const std::vector<std::string>& items, std::int32_t index)
    {
        return Wrap(ChoiceRowHost::Create(Utf16(label), items, index));
    }

    std::int32_t HomeViewHost::ChoiceIndex(const Launcher::HomeViewElement& row) const
    {
        return HostOf<ChoiceRowHost>(P(row))->Row().Index();
    }

    void HomeViewHost::ChoiceIndex(
        const Launcher::HomeViewElement& row, std::int32_t index)
    {
        HostOf<ChoiceRowHost>(P(row))->Row().Index(index);
    }

    std::string HomeViewHost::ChoiceValue(const Launcher::HomeViewElement& row) const
    {
        const std::optional<std::u16string> value
            = HostOf<ChoiceRowHost>(P(row))->Row().Value();
        return value.has_value() ? Utf8(*value) : std::string();
    }

    void HomeViewHost::ChoiceSetItems(const Launcher::HomeViewElement& row,
        const std::vector<std::string>& items, std::int32_t index)
    {
        std::vector<std::u16string> values;
        values.reserve(items.size());
        for (const std::string& item : items)
        {
            values.push_back(Utf16(item));
        }
        HostOf<ChoiceRowHost>(P(row))->Row().SetItems(
            MakeStringList(std::move(values)), index);
    }

    void HomeViewHost::AddChoiceChanged(
        const Launcher::HomeViewElement& row, Action action)
    {
        HostOf<ChoiceRowHost>(P(row))->Changed(std::move(action));
    }

    Launcher::HomeViewElement HomeViewHost::CreateToggleRow(std::string label, bool on)
    {
        return Wrap(ToggleRowHost::Create(Utf16(label), on));
    }

    bool HomeViewHost::ToggleOn(const Launcher::HomeViewElement& row) const
    {
        return HostOf<ToggleRowHost>(P(row))->Row().On();
    }

    void HomeViewHost::ToggleOn(const Launcher::HomeViewElement& row, bool value)
    {
        HostOf<ToggleRowHost>(P(row))->Row().On(value);
    }

    void HomeViewHost::AddToggleChanged(
        const Launcher::HomeViewElement& row, Action action)
    {
        HostOf<ToggleRowHost>(P(row))->Changed(std::move(action));
    }

    Launcher::HomeViewElement HomeViewHost::CreateFieldRow(
        std::string label, std::string value, double boxWidth)
    {
        return Wrap(FieldRowHost::Create(Utf16(label), Utf16(value), boxWidth));
    }

    std::string HomeViewHost::FieldValue(const Launcher::HomeViewElement& row) const
    {
        return Utf8(HostOf<FieldRowHost>(P(row))->Row().Value());
    }

    void HomeViewHost::FieldValue(
        const Launcher::HomeViewElement& row, std::string value)
    {
        const std::u16string text = Utf16(value);
        HostOf<FieldRowHost>(P(row))->Row().Value(text);
    }

    void HomeViewHost::AddFieldLostFocus(
        const Launcher::HomeViewElement& row, Action action)
    {
        HostOf<FieldRowHost>(P(row))->LostFocus(std::move(action));
    }

    Launcher::HomeViewElement HomeViewHost::CreateServerHeader()
    {
        return Wrap(ServerHeaderHost::Create());
    }

    Launcher::HomeViewElement HomeViewHost::CreateServerRow(
        std::string name, std::string endpoint)
    {
        return Wrap(ServerRowHost::Create(Utf16(name), Utf16(endpoint)));
    }

    void HomeViewHost::AddServerRowClicked(
        const Launcher::HomeViewElement& row, Action action)
    {
        HostOf<ServerRowHost>(P(row))->Clicked(std::move(action));
    }

    void HomeViewHost::ServerRowSetStatus(const Launcher::HomeViewElement& row,
        const Launcher::HomeViewServerStatus& status)
    {
        HostOf<ServerRowHost>(P(row))->Row().SetStatus(Convert(status));
    }

    // --- the other screens ------------------------------------------------

    Launcher::HomeViewElement HomeViewHost::CreateSettingsView(
        const std::shared_ptr<MphRead::MenuSettings>& settings, bool inGame)
    {
        return Wrap(NewSettingsView(settings, inGame));
    }

    void HomeViewHost::AddSettingsClosed(
        const Launcher::HomeViewElement& view, Action action)
    {
        SettingsViewClosed(P(view), std::move(action));
    }

    void HomeViewHost::AddSettingsGameFilesRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        SettingsViewGameFilesRequested(P(view), std::move(action));
    }

    Launcher::HomeViewElement HomeViewHost::CreateMapPickerView(
        const std::vector<std::string>& rooms, std::string current)
    {
        return Wrap(NewMapPickerView(rooms, std::move(current)));
    }

    void HomeViewHost::AddMapPickerClosed(
        const Launcher::HomeViewElement& view, Action action)
    {
        MapPickerClosed(P(view), std::move(action));
    }

    std::optional<std::string> HomeViewHost::MapPickerRoomKey(
        const Launcher::HomeViewElement& view) const
    {
        return ::MphRead::NativeRuntime::Avalonia::MapPickerRoomKey(P(view));
    }

    Launcher::HomeViewElement HomeViewHost::CreateDemoPickerView(
        std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>> demos,
        std::string directory)
    {
        return Wrap(NewDemoPickerView(std::move(demos), std::move(directory)));
    }

    void HomeViewHost::AddDemoPickerClosed(
        const Launcher::HomeViewElement& view, Action action)
    {
        DemoPickerClosed(P(view), std::move(action));
    }

    std::optional<std::string> HomeViewHost::DemoPickerPath(
        const Launcher::HomeViewElement& view) const
    {
        return ::MphRead::NativeRuntime::Avalonia::DemoPickerPath(P(view));
    }

    bool HomeViewHost::DemoPickerImportRequested(
        const Launcher::HomeViewElement& view) const
    {
        return ::MphRead::NativeRuntime::Avalonia::DemoPickerImportRequested(P(view));
    }

    Launcher::HomeViewElement HomeViewHost::CreatePauseMenuView(bool offerWindowMode)
    {
        return Wrap(NewPauseMenuView(offerWindowMode));
    }

    void HomeViewHost::AddPauseResumed(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseResumed(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseSettingsRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseSettingsRequested(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseLeaveRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseLeaveRequested(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseQuitRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseQuitRequested(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseSpectateRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseSpectateRequested(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseRejoinRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseRejoinRequested(P(view), std::move(action));
    }

    void HomeViewHost::AddPauseRecordToggleRequested(
        const Launcher::HomeViewElement& view, Action action)
    {
        PauseRecordToggleRequested(P(view), std::move(action));
    }

    void HomeViewHost::PauseFocusResume(const Launcher::HomeViewElement& view)
    {
        ::MphRead::NativeRuntime::Avalonia::PauseFocusResume(P(view));
    }

    // --- storage, strings and numbers -------------------------------------

    bool HomeViewHost::HasTopLevel() const
    {
        return Toolkit::Window::Of(*_visual) != nullptr;
    }

    bool HomeViewHost::IsAndroid() const
    {
#if defined(ANDROID) || defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }

    Launcher::HomeViewTaskRef HomeViewHost::OpenFilePickerAsync(
        Launcher::HomeViewFilePickerOptions options,
        std::shared_ptr<std::vector<Launcher::HomeViewStorageFile>> result)
    {
        // The platform dialog is modal and runs on this thread, as Avalonia's
        // does; the task it returns has already settled.
        try
        {
            for (std::string& path : ChooseFiles(options))
            {
                Launcher::HomeViewStorageFile file;
                file.Native = std::make_shared<std::string>(std::move(path));
                result->push_back(std::move(file));
                if (!options.AllowMultiple)
                {
                    break;
                }
            }
        }
        catch (...)
        {
            return Ref(HostTask::Faulted(std::current_exception()));
        }
        return Ref(HostTask::Completed());
    }

    std::optional<std::string> HomeViewHost::TryGetLocalPath(
        const Launcher::HomeViewStorageFile& file) const
    {
        const std::string path = PathOf(file);
        return path.empty() ? std::nullopt : std::optional<std::string>(path);
    }

    Launcher::HomeViewTaskRef HomeViewHost::CopyStorageFileToPathAsync(
        const Launcher::HomeViewStorageFile& file, std::string path)
    {
        const std::string source = PathOf(file);
        return Ref(HostTask::Run(
            [source, path = std::move(path)]()
            {
                std::filesystem::copy_file(source, path,
                    std::filesystem::copy_options::overwrite_existing);
            }));
    }

    Launcher::HomeViewTaskRef HomeViewHost::TryGetFolderFromPathAsync(
        std::string path, std::shared_ptr<std::optional<Launcher::HomeViewStorageFolder>> result)
    {
        std::error_code error;
        if (std::filesystem::is_directory(path, error))
        {
            Launcher::HomeViewStorageFolder folder;
            folder.Native = std::make_shared<std::string>(std::move(path));
            *result = std::move(folder);
        }
        return Ref(HostTask::Completed());
    }

    bool HomeViewHost::DirectoryExists(std::string path) const
    {
        std::error_code error;
        return std::filesystem::is_directory(path, error);
    }

    void HomeViewHost::DeleteFile(std::string path)
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::string HomeViewHost::CombinePath(std::string left, std::string right) const
    {
        return (std::filesystem::path(left) / std::filesystem::path(right)).string();
    }

    std::string HomeViewHost::Trim(std::string value) const
    {
        const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
        std::size_t start = 0;
        while (start < value.size()
            && space(static_cast<unsigned char>(value[start])))
        {
            ++start;
        }
        std::size_t end = value.size();
        while (end > start && space(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }
        return value.substr(start, end - start);
    }

    bool HomeViewHost::TryParseInt32InvariantInteger(
        std::string_view text, std::int32_t& value) const
    {
        const std::string trimmed = Trim(std::string(text));
        if (trimmed.empty())
        {
            return false;
        }
        const char* const first = trimmed.data();
        const char* const last = first + trimmed.size();
        const std::from_chars_result parsed = std::from_chars(first, last, value);
        return parsed.ec == std::errc() && parsed.ptr == last;
    }

    std::string HomeViewHost::FormatCurrentInt32(std::int32_t value) const
    {
        return std::to_string(value);
    }

    void HomeViewHost::ConsoleWriteLine(std::string line)
    {
        std::cout << line << '\n';
    }

    std::string HomeViewHost::ExceptionMessage(std::exception_ptr error) const
    {
        if (error == nullptr)
        {
            return std::string();
        }
        try
        {
            std::rethrow_exception(error);
        }
        catch (const std::exception& failure)
        {
            return failure.what();
        }
        catch (...)
        {
            return "Exception of type 'System.Object' was thrown.";
        }
    }

    std::string HomeViewHost::FromUtf16(std::u16string_view text) const
    {
        return Utf8(text);
    }

    std::u16string HomeViewHost::ToUtf16(std::string_view text) const
    {
        return Utf16(text);
    }

    // --- networking -------------------------------------------------------

    Launcher::HomeViewServerStatus HomeViewHost::NetStatusQuery(
        const std::string& host, std::int32_t port, bool allowJoinProbe)
    {
        return Convert(
            MphRead::Mods::Network::NetStatus::Query(host, port, allowJoinProbe));
    }

    std::string HomeViewHost::NetStatusModeName(GameMode mode)
    {
        return MphRead::Mods::Network::NetStatus::ModeName(mode);
    }

    bool HomeViewHost::NetJoin(const std::string& host, std::int32_t port,
        const std::string& name, Hunter hunter)
    {
        return MphRead::Mods::Network::NetLaunch::Join(host, port, name, hunter);
    }

    std::string HomeViewHost::NetLastJoinError()
    {
        return MphRead::Mods::Network::NetLaunch::LastJoinError();
    }

    void HomeViewHost::NetSessionStop()
    {
        MphRead::Mods::Network::NetSession::Stop();
    }

    Launcher::HomeViewMasterListResult HomeViewHost::NetMasterQuery(
        const std::string& host, std::int32_t port)
    {
        const MphRead::Mods::Network::MasterListResult answer
            = MphRead::Mods::Network::NetMasterClient::Query(host, port);
        auto servers = std::make_shared<std::vector<Launcher::HomeViewMasterListing>>();
        if (answer.Servers != nullptr)
        {
            for (const MphRead::Mods::Network::MasterListing& listing : *answer.Servers)
            {
                servers->push_back(Launcher::HomeViewMasterListing{listing.Address,
                    listing.Port, listing.ServerName, listing.Endpoint()});
            }
        }
        return Launcher::HomeViewMasterListResult{std::move(servers), answer.Answered};
    }

    Launcher::HomeViewHostedGame HomeViewHost::NetMasterRequestGame(
        const std::string& masterHost, std::int32_t masterPort,
        const std::string& roomKey, GameMode mode, float timeLimit,
        std::int32_t pointGoal, std::int32_t maxPlayers, const std::string& serverName)
    {
        const MphRead::Mods::Network::HostedGame game
            = MphRead::Mods::Network::NetMasterClient::RequestGame(masterHost,
                masterPort, roomKey, mode, timeLimit, pointGoal, maxPlayers,
                serverName);
        return Launcher::HomeViewHostedGame{game.Started, game.Host, game.Port};
    }

    bool HomeViewHost::NetHostStartAndJoin(std::int32_t port, const std::string& name,
        Hunter hunter, const std::string& roomKey, GameMode mode, float timeLimit,
        std::int32_t pointGoal,
        std::optional<std::tuple<std::string, std::int32_t, std::string>> listing)
    {
        return MphRead::Mods::Network::NetHostSession::StartAndJoin(port, name, hunter,
            roomKey, mode, timeLimit, pointGoal,
            MphRead::Entities::PlayerEntity::SlotCapacity, std::move(listing));
    }

    std::optional<std::string> HomeViewHost::NetHostLastError()
    {
        return MphRead::Mods::Network::NetHostSession::LastError();
    }

    void HomeViewHost::NetHostStop()
    {
        MphRead::Mods::Network::NetHostSession::Stop();
    }

    Launcher::HomeViewTaskRef HomeViewHost::RenderMissingPreviewsAsync(
        std::function<void(std::string)> report)
    {
        return Ref(HostTask::Run(
            [report = std::move(report)]()
            {
                const std::vector<std::string> rooms
                    = MphRead::Mods::ThumbnailGenerator::MissingThumbnails();
                if (rooms.empty() || !MphRead::Mods::ThumbnailBatch::CanRun())
                {
                    return;
                }
                (void)MphRead::Mods::ThumbnailBatch::Run(rooms,
                    MphRead::Mods::ThumbnailBatch::DefaultParallelism(),
                    MphRead::Mods::ThumbnailGenerator::ThumbnailWidth,
                    MphRead::Mods::ThumbnailGenerator::ThumbnailHeight,
                    [&report](const std::string& line)
                    {
                        if (report)
                        {
                            report(line);
                        }
                    });
            }));
    }

    // --- timers -----------------------------------------------------------

    Launcher::HomeViewElement HomeViewHost::CreateDispatcherTimer()
    {
        return Launcher::HomeViewElement{Toolkit::Dispatcher::Instance().CreateTimer()};
    }

    void HomeViewHost::TimerIntervalSeconds(
        const Launcher::HomeViewElement& timer, double seconds)
    {
        static_cast<Toolkit::Timer*>(timer.Native.get())->IntervalSeconds(seconds);
    }

    void HomeViewHost::AddTimerTick(
        const Launcher::HomeViewElement& timer, Action action)
    {
        static_cast<Toolkit::Timer*>(timer.Native.get())->Tick(std::move(action));
    }

    void HomeViewHost::StartTimer(const Launcher::HomeViewElement& timer)
    {
        static_cast<Toolkit::Timer*>(timer.Native.get())->Start();
    }

    void HomeViewHost::StopTimer(const Launcher::HomeViewElement& timer)
    {
        static_cast<Toolkit::Timer*>(timer.Native.get())->Stop();
    }
}

#include "HomeView.hpp"

#include "../../../Menu.hpp"
#include "../Portable/AdventureSave.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../Portable/SetupProgress.hpp"
#include "../../Branding.hpp"
#include "../../DebugLog.hpp"
#include "../../LogShare.hpp"
#include "../../SpectatorMode.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../ThumbnailHost.hpp"
#include "../../Network/DemoFile.hpp"
#include "../../Network/DemoPlayback.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Update/BuildVersion.hpp"
#include "../../Update/UpdateInstall.hpp"
#include "../../Update/Updater.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <type_traits>

namespace MphRead::Mods::Launcher::Gui
{
    namespace
    {
        using MphRead::Mods::Launcher::AdventureSave;
        using MphRead::Mods::Launcher::GameFiles;
        using MphRead::Mods::Launcher::Hunters;
        using MphRead::Mods::Launcher::LauncherPrefs;
        using MphRead::Mods::Launcher::SetupProgress;
        using MphRead::Mods::Network::DemoFile;
        using MphRead::Mods::Network::DemoLibrary;
        using MphRead::Mods::Network::DemoPlayback;
        using MphRead::Mods::Network::DemoRecorder;
        using MphRead::Mods::Update::BuildVersion;
        using MphRead::Mods::Update::UpdateInstall;
        using MphRead::Mods::Update::Updater;

        template <typename T>
        std::int32_t IndexOf(const std::vector<T>& values, const T& value)
        {
            const auto it = std::find(values.begin(), values.end(), value);
            if (it == values.end())
            {
                return -1;
            }
            return static_cast<std::int32_t>(std::distance(values.begin(), it));
        }


        std::string ExceptionMessage(HomeViewAdapter& adapter, std::exception_ptr error)
        {
            return adapter.ExceptionMessage(error);
        }

        template <typename F>
        HomeViewTaskRef CaptureTask(HomeViewAdapter& adapter, F&& body)
        {
            try
            {
                return body();
            }
            catch (...)
            {
                return adapter.FaultedTask(std::current_exception());
            }
        }

        std::string JoinLines(const std::vector<std::string>& lines,
            std::size_t begin)
        {
            std::string result;
            for (std::size_t index = begin; index < lines.size(); ++index)
            {
                if (!result.empty())
                {
                    result.push_back('\n');
                }
                result += lines[index];
            }
            return result;
        }
    }

    HomeViewNullReferenceException::HomeViewNullReferenceException()
        : std::runtime_error("Object reference not set to an instance of an object.")
    {
    }

    HomeViewEventHandler::HomeViewEventHandler(
        std::shared_ptr<void> target, Callback function)
    {
        if (function != nullptr)
        {
            auto list = std::make_shared<std::vector<Invocation>>();
            list->push_back(Invocation{std::move(target), function});
            _invocations = std::move(list);
        }
    }

    HomeViewEventHandler::HomeViewEventHandler(
        std::shared_ptr<const std::vector<Invocation>> invocations) noexcept
        : _invocations(std::move(invocations))
    {
    }

    HomeViewEventHandler HomeViewEventHandler::Combine(
        const HomeViewEventHandler& left, const HomeViewEventHandler& right)
    {
        if (left.IsNull())
        {
            return right;
        }
        if (right.IsNull())
        {
            return left;
        }
        auto combined = std::make_shared<std::vector<Invocation>>();
        combined->reserve(left._invocations->size() + right._invocations->size());
        combined->insert(combined->end(), left._invocations->begin(), left._invocations->end());
        combined->insert(combined->end(), right._invocations->begin(), right._invocations->end());
        return HomeViewEventHandler(std::move(combined));
    }

    bool HomeViewEventHandler::IsNull() const noexcept
    {
        return !_invocations || _invocations->empty();
    }

    bool operator==(const HomeViewEventHandler& left,
        const HomeViewEventHandler& right) noexcept
    {
        if (left.IsNull() || right.IsNull())
        {
            return left.IsNull() == right.IsNull();
        }
        return *left._invocations == *right._invocations;
    }

    void HomeViewEvent::Add(const HomeViewEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }
        auto current = _handlers.load();
        for (;;)
        {
            auto next = std::make_shared<InvocationList>();
            if (current)
            {
                *next = *current;
            }
            next->insert(next->end(), handler._invocations->begin(),
                handler._invocations->end());
            if (_handlers.compare_exchange_weak(current,
                std::shared_ptr<const InvocationList>(next)))
            {
                return;
            }
        }
    }

    void HomeViewEvent::Remove(const HomeViewEventHandler& handler)
    {
        if (handler.IsNull())
        {
            return;
        }
        auto current = _handlers.load();
        for (;;)
        {
            if (!current || current->size() < handler._invocations->size())
            {
                return;
            }
            const std::size_t needle = handler._invocations->size();
            std::optional<std::size_t> found;
            for (std::size_t start = current->size() - needle + 1; start-- > 0;)
            {
                if (std::equal(handler._invocations->begin(), handler._invocations->end(),
                    current->begin() + static_cast<std::ptrdiff_t>(start)))
                {
                    found = start;
                    break;
                }
                if (start == 0)
                {
                    break;
                }
            }
            if (!found.has_value())
            {
                return;
            }
            auto next = std::make_shared<InvocationList>(*current);
            next->erase(next->begin() + static_cast<std::ptrdiff_t>(*found),
                next->begin() + static_cast<std::ptrdiff_t>(*found + needle));
            std::shared_ptr<const InvocationList> desired = next->empty()
                ? std::shared_ptr<const InvocationList>{}
                : std::shared_ptr<const InvocationList>(next);
            if (_handlers.compare_exchange_weak(current, std::move(desired)))
            {
                return;
            }
        }
    }

    void HomeViewEvent::Invoke(void* sender, LaunchPlan plan) const
    {
        const auto snapshot = _handlers.load();
        if (!snapshot)
        {
            return;
        }
        for (const Invocation& invocation : *snapshot)
        {
            invocation.Function(invocation.Target.get(), sender, plan);
        }
    }

    const std::vector<HomeView::ModeEntry> HomeView::Modes =
    {
        {"Battle", GameMode::Battle},
        {"Battle teams", GameMode::BattleTeams},
        {"Survival", GameMode::Survival},
        {"Survival teams", GameMode::SurvivalTeams},
        {"Capture", GameMode::Capture},
        {"Bounty", GameMode::Bounty},
        {"Bounty teams", GameMode::BountyTeams},
        {"Defender", GameMode::Defender},
        {"Defender teams", GameMode::DefenderTeams},
        {"Nodes", GameMode::Nodes},
        {"Nodes teams", GameMode::NodesTeams},
        {"Prime hunter", GameMode::PrimeHunter}
    };

    const std::vector<std::string> HomeView::Hunters =
    {
        "Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel", "Random"
    };

    HomeView::HomeView(HomeViewAdapter& adapter,
        std::shared_ptr<MphRead::MenuSettings> settings,
        HomeViewRoomListRef rooms)
        : _adapter(adapter),
          _splash(_adapter.CreateSplashView()),
          _cards(_adapter.CreatePanel()),
          _layout(_adapter.CreateGrid()),
          _setupProgress(_adapter.CreateProgressRow()),
          _updateBadge(_adapter.CreateUpdateBadge())
    {
        _settings = std::move(settings);
        if (!rooms)
        {
            throw HomeViewNullReferenceException();
        }
        auto enumerator = rooms->GetEnumerator();
        if (!enumerator)
        {
            throw HomeViewNullReferenceException();
        }
        std::exception_ptr enumerationError;
        try
        {
            while (enumerator->MoveNext())
            {
                _playable.push_back(enumerator->Current());
            }
        }
        catch (...)
        {
            enumerationError = std::current_exception();
        }
        enumerator->Dispose();
        if (enumerationError)
        {
            std::rethrow_exception(enumerationError);
        }

        _adapter.SetBackground(GuiTheme::PanelBrush);

        const HomeViewElement scroll = _adapter.CreateScrollViewer();
        _adapter.SetScrollContent(scroll, _cards);
        _adapter.DisableHorizontalScrollBar(scroll);

        const HomeViewElement stack = _adapter.CreateGrid();
        _adapter.SetGridRowDefinitions(stack, "*");
        _adapter.SetGridRow(scroll, 0);
        _adapter.AddChild(stack, scroll);

        _panel = _adapter.CreateBorder();
        _adapter.SetBorderBackground(_panel, GuiTheme::PanelBrush);
        _adapter.SetBorderPadding(_panel, HomeViewThickness{22, 20, 22, 16});
        _adapter.SetBorderChild(_panel, stack);

        _adapter.AddUpdateBadgeClick(_updateBadge, [this]() { UpdateNow(); });
        _adapter.SetHorizontalAlignment(_updateBadge, HomeViewHorizontalAlignment::Left);
        _adapter.SetVerticalAlignment(_updateBadge, HomeViewVerticalAlignment::Bottom);
        _adapter.SetMargin(_updateBadge, HomeViewThickness{24, 0, 24, 22});
        _adapter.AddChild(_layout, _splash);
        _adapter.AddChild(_layout, _updateBadge);
        _adapter.AddChild(_layout, BuildVersionLine());
        _adapter.AddChild(_layout, BuildDebugSwitch());
        _adapter.AddChild(_layout, _panel);
        ApplyLayout(false);

        _overlay = _adapter.CreatePanel();
        _adapter.SetPanelBackground(_overlay, GuiTheme::InkBrush);
        _adapter.SetIsVisible(_overlay, false);

        const HomeViewElement root = _adapter.CreatePanel();
        _adapter.AddChild(root, _layout);
        _adapter.AddChild(root, _overlay);
        _adapter.SetContent(root);
        _adapter.AddSizeChanged([this](double width)
        {
            ApplyLayout(width < NarrowWidth);
        });

        _homeCard = BuildHomeCard();
        _setupCard = BuildSetupCard();
        _onlineCard = BuildOnlineCard();
        _hostCard = BuildHostCard();
        _browseCard = BuildBrowseCard();

        _adapter.SetIsVisible(_setupBack, GameFiles::Ready());
        ShowCard(GameFiles::Ready() ? _homeCard : _setupCard);
        RefreshSplash();
        RefreshPreviewEntry();

        if (LauncherPrefs::AutoUpdate())
        {
            Updater::CheckInBackground(
                [this](Update::UpdateInfo update)
                {
                    _adapter.Post([this, update]() { ShowUpdate(update); });
                },
                [this]()
                {
                    _adapter.Post([this]() { RefreshVersionLine(); });
                });
        }
        _adapter.Forget(CatchUpPreviews());
    }

    LaunchPlan HomeView::Plan() const
    {
        return _plan;
    }

    void HomeView::AddDone(const HomeViewEventHandler& handler)
    {
        _done.Add(handler);
    }

    void HomeView::RemoveDone(const HomeViewEventHandler& handler)
    {
        _done.Remove(handler);
    }

    HomeViewTaskRef HomeView::CatchUpPreviews()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            if (!GameFiles::Ready() || !ThumbnailHost::CanRender()
                || ThumbnailGenerator::MissingThumbnails().empty())
            {
                return _adapter.CompletedTask();
            }
            HomeViewTaskRef render = _adapter.RenderMissingPreviewsAsync(
                [this](std::string line)
                {
                    _adapter.Post([this, line = std::move(line)]()
                    {
                        if (_previewProgress)
                        {
                            _adapter.MenuEntrySubtitle(_previewProgress, line);
                        }
                    });
                });
            return _adapter.ContinueWithAction(render, [this]()
            {
                _adapter.Post([this]()
                {
                    RefreshSplash();
                    RefreshPreviewEntry();
                });
            });
        });
    }

    void HomeView::ApplyLayout(bool narrow)
    {
        if (_laidOut && narrow == _narrow)
        {
            return;
        }
        _narrow = narrow;
        _laidOut = true;
        if (narrow)
        {
            _adapter.SetGridColumnDefinitions(_layout, "*");
            _adapter.SetGridRowDefinitions(_layout, "Auto,*");
            _adapter.SetHeight(_splash, 150);
            _adapter.SetWidthAuto(_panel);
            for (const HomeViewElement& element :
                {_splash, _updateBadge, _versionBox, _debugRow})
            {
                _adapter.SetGridColumn(element, 0);
                _adapter.SetGridRow(element, 0);
            }
            _adapter.SetGridColumn(_panel, 0);
            _adapter.SetGridRow(_panel, 1);
            return;
        }
        _adapter.SetGridColumnDefinitions(_layout, "*,Auto");
        _adapter.SetGridRowDefinitions(_layout, "*");
        _adapter.SetHeightAuto(_splash);
        _adapter.SetWidth(_panel, PanelWidth());
        for (const HomeViewElement& element :
            {_splash, _updateBadge, _versionBox, _debugRow})
        {
            _adapter.SetGridColumn(element, 0);
            _adapter.SetGridRow(element, 0);
        }
        _adapter.SetGridColumn(_panel, 1);
        _adapter.SetGridRow(_panel, 0);
    }

    double HomeView::PanelWidth() const
    {
        return _current == _browseCard ? BrowseWidth : 400.0;
    }

    bool HomeView::GoBack()
    {
        if (_adapter.GetIsVisible(_overlay))
        {
            CloseOverlay();
            return true;
        }
        if (!(_current == _homeCard) && GameFiles::Ready())
        {
            ShowCard(_homeCard);
            return true;
        }
        return false;
    }

    void HomeView::OnKeyDown(HomeViewKeyEventArgs& e)
    {
        if (e.Key == HomeViewKey::Escape)
        {
            if (!GoBack())
            {
                Finish(LaunchPlan{});
            }
            e.Handled = true;
            return;
        }
        _adapter.BaseOnKeyDown(e);
    }

    void HomeView::Finish(LaunchPlan plan)
    {
        if (_finished)
        {
            return;
        }
        _finished = true;
        StopStatusPolling();
        _plan = std::move(plan);
        _done.Invoke(this, _plan);
    }

    void HomeView::Reset()
    {
        _finished = false;
        _plan = LaunchPlan{};
        Hunters::Reroll();
        LauncherPrefs::Load();
        RefreshPrefRows();
        RefreshRooms();
        ShowCard(GameFiles::Ready() ? _homeCard : _setupCard);
        RefreshSplash();
        RefreshPreviewEntry();
    }

    HomeViewTaskRef HomeView::ShowOverlay(const HomeViewElement& view,
        std::function<void(HomeViewAdapter::Action)> subscribeClosed)
    {
        HomeViewTaskRef done = _adapter.PendingTask();
        _overlayDone = done;
        subscribeClosed([this]() { CloseOverlay(); });
        _adapter.ClearChildren(_overlay);
        _adapter.AddChild(_overlay, view);
        _adapter.SetIsVisible(_overlay, true);
        _adapter.Post([this, view]() { _adapter.Focus(view); },
            HomeViewDispatcherPriority::Background);
        return done;
    }

    void HomeView::ShowPauseMenu(std::function<void()> onResume,
        std::function<void()> onLeave, std::function<void()> onQuit)
    {
        const HomeViewElement view = _adapter.CreatePauseMenuView(false);
        auto closed = std::make_shared<std::vector<HomeViewAdapter::Action>>();
        auto close = [view, closed]()
        {
            const auto snapshot = *closed;
            for (const auto& handler : snapshot)
            {
                handler();
            }
        };
        _adapter.AddPauseResumed(view, [close, onResume]()
        {
            close();
            onResume();
        });
        _adapter.AddPauseLeaveRequested(view, [close, onLeave]()
        {
            close();
            onLeave();
        });
        _adapter.AddPauseQuitRequested(view, [close, onQuit]()
        {
            close();
            onQuit();
        });
        _adapter.AddPauseSpectateRequested(view, [close, onResume]()
        {
            close();
            SpectatorMode::Start();
            onResume();
        });
        _adapter.AddPauseRejoinRequested(view, [close, onResume]()
        {
            close();
            SpectatorMode::Rejoin();
            onResume();
        });
        _adapter.AddPauseRecordToggleRequested(view, [this, close, onResume]()
        {
            if (DemoRecorder::IsRecording())
            {
                _adapter.ConsoleWriteLine("[demo] recording saved to "
                    + DemoRecorder::CurrentPath().value_or(""));
                DemoRecorder::Stop();
            }
            else
            {
                (void)DemoRecorder::Start();
            }
            close();
            onResume();
        });
        _adapter.AddPauseSettingsRequested(view,
            [this, onResume, onLeave, onQuit]()
            {
                _adapter.StartAsyncVoid([this, onResume, onLeave, onQuit]()
                {
                    return _adapter.ContinueWithAction(OpenSettings(),
                        [this, onResume, onLeave, onQuit]()
                        {
                            ShowPauseMenu(onResume, onLeave, onQuit);
                        });
                });
            });
        _adapter.Forget(ShowOverlay(view,
            [closed](HomeViewAdapter::Action handler)
            {
                closed->push_back(std::move(handler));
            }));
        _adapter.PauseFocusResume(view);
    }

    void HomeView::CloseOverlay()
    {
        _adapter.SetIsVisible(_overlay, false);
        _adapter.ClearChildren(_overlay);
        HomeViewTaskRef done = _overlayDone;
        _overlayDone.reset();
        if (done)
        {
            (void)_adapter.TrySetTaskResult(done);
        }
    }

    void HomeView::ShowCard(const HomeViewElement& card)
    {
        StopStatusPolling();
        _adapter.ClearChildren(_cards);
        _adapter.AddChild(_cards, card);
        _current = card;
        if (_versionBox)
        {
            const bool home = card == _homeCard;
            _adapter.SetIsVisible(_versionBox, home);
            if (_debugRow)
            {
                _adapter.SetIsVisible(_debugRow, home);
                RefreshShareButton();
            }
            if (home && _adapter.GetIsVisible(_updateBadge))
            {
                _adapter.SetIsVisible(_updateBadge, false);
            }
            else if (!home && Updater::Available().has_value())
            {
                _adapter.SetIsVisible(_updateBadge, true);
            }
        }
        if (!_narrow)
        {
            _adapter.SetWidth(_panel, PanelWidth());
        }
        if (_matchMap)
        {
            RefreshSplash();
        }
        if (card == _onlineCard)
        {
            StartStatusPolling();
        }
        _adapter.Post([this, card]()
        {
            _adapter.FocusFirstFocusableDescendant(card);
        }, HomeViewDispatcherPriority::Background);
    }

    HomeViewElement HomeView::Card()
    {
        HomeViewElement card = _adapter.CreateStackPanel();
        _adapter.SetStackSpacing(card, 2);
        return card;
    }

    HomeViewElement HomeView::Back(HomeViewAdapter::Action go)
    {
        HomeViewElement entry = _adapter.CreateMenuEntry("Back", std::string{}, 13);
        _adapter.MenuEntryAccent(entry, GuiTheme::TextDim);
        _adapter.AddMenuEntryClick(entry, std::move(go));
        return entry;
    }

    HomeViewElement HomeView::BuildHomeCard()
    {
        HomeViewElement card = Card();
        _hostEntry = _adapter.CreateMenuEntry("Host");
        _adapter.AddMenuEntryClick(_hostEntry, [this]() { OpenHost(); });
        _onlineEntry = _adapter.CreateMenuEntry("Join");
        _adapter.AddMenuEntryClick(_onlineEntry, [this]() { OpenJoin(); });
        _demoEntry = _adapter.CreateMenuEntry("Demos");
        _adapter.AddMenuEntryClick(_demoEntry, [this]()
        {
            _adapter.StartAsyncVoid([this]() { return ChooseDemo(); });
        });
        HomeViewElement settings = _adapter.CreateMenuEntry("Settings");
        _adapter.AddMenuEntryClick(settings, [this]()
        {
            _adapter.StartAsyncVoid([this]() { return OpenSettings(); });
        });
        HomeViewElement quit = _adapter.CreateMenuEntry("Quit");
        _adapter.AddMenuEntryClick(quit, [this]() { Finish(LaunchPlan{}); });

        _adapter.AddChild(card, _hostEntry);
        _adapter.AddChild(card, _onlineEntry);
        _adapter.AddChild(card, _demoEntry);
        _adapter.AddChild(card, settings);
        _adapter.AddChild(card, quit);
        RefreshVersionLine();
        return card;
    }

    HomeViewElement HomeView::BuildVersionLine()
    {
        _versionLine = _adapter.CreateTextBlock();
        _adapter.SetText(_versionLine, VersionNumber());
        _adapter.SetFontSize(_versionLine, 11);
        _adapter.SetForeground(_versionLine, GuiTheme::TextDimBrush);

        _versionBox = _adapter.CreateBorder();
        _adapter.SetBorderBackground(_versionBox, GuiTheme::ScrimBrush);
        _adapter.SetBorderBrush(_versionBox, GuiTheme::EdgeBrush);
        _adapter.SetBorderThickness(_versionBox, HomeViewThickness::Uniform(1));
        _adapter.SetBorderCornerRadius(_versionBox, 4);
        _adapter.SetBorderPadding(_versionBox, HomeViewThickness{9, 4, 9, 4});
        _adapter.SetMargin(_versionBox, HomeViewThickness{0, 0, 24, 22});
        _adapter.SetHorizontalAlignment(_versionBox, HomeViewHorizontalAlignment::Right);
        _adapter.SetVerticalAlignment(_versionBox, HomeViewVerticalAlignment::Bottom);
        _adapter.SetIsVisible(_versionBox, false);
        _adapter.SetBorderChild(_versionBox, _versionLine);
        _adapter.AddPointerPressed(_versionBox, [this](HomeViewPointerEventArgs& e)
        {
            if (_updatable)
            {
                e.Handled = true;
                UpdateNow();
            }
        });
        return _versionBox;
    }

    HomeViewElement HomeView::BuildDebugSwitch()
    {
        _debugLine = _adapter.CreateTextBlock();
        _adapter.SetFontSize(_debugLine, 10);
        _adapter.SetForeground(_debugLine, GuiTheme::TextDimBrush);

        _debugBox = _adapter.CreateBorder();
        _adapter.SetBorderBackground(_debugBox, GuiTheme::ScrimBrush);
        _adapter.SetBorderBrush(_debugBox, GuiTheme::EdgeBrush);
        _adapter.SetBorderThickness(_debugBox, HomeViewThickness::Uniform(1));
        _adapter.SetBorderCornerRadius(_debugBox, 4);
        _adapter.SetBorderPadding(_debugBox, HomeViewThickness{8, 2, 8, 2});
        _adapter.SetCursor(_debugBox, HomeViewCursor::Hand);
        _adapter.SetBorderChild(_debugBox, _debugLine);
        _adapter.AddPointerPressed(_debugBox, [this](HomeViewPointerEventArgs& e)
        {
            e.Handled = true;
            ToggleDebugLogs();
        });

        _shareLine = _adapter.CreateTextBlock();
        _adapter.SetText(_shareLine, "\xE2\x86\x97 Share logs");
        _adapter.SetFontSize(_shareLine, 10);
        _adapter.SetForeground(_shareLine, GuiTheme::TextDimBrush);

        _shareBox = _adapter.CreateBorder();
        _adapter.SetBorderBackground(_shareBox, GuiTheme::ScrimBrush);
        _adapter.SetBorderBrush(_shareBox, GuiTheme::EdgeBrush);
        _adapter.SetBorderThickness(_shareBox, HomeViewThickness::Uniform(1));
        _adapter.SetBorderCornerRadius(_shareBox, 4);
        _adapter.SetBorderPadding(_shareBox, HomeViewThickness{8, 2, 8, 2});
        _adapter.SetIsVisible(_shareBox, false);
        _adapter.SetCursor(_shareBox, HomeViewCursor::Hand);
        _adapter.SetBorderChild(_shareBox, _shareLine);
        _adapter.SetToolTip(_shareBox,
            "Zip the log files and hand them to another app.");
        _adapter.AddPointerPressed(_shareBox, [this](HomeViewPointerEventArgs& e)
        {
            e.Handled = true;
            _adapter.StartAsyncVoid([this]() { return ShareLogs(); });
        });

        _debugRow = _adapter.CreateStackPanel();
        _adapter.SetStackOrientation(_debugRow, HomeViewOrientation::Horizontal);
        _adapter.SetStackSpacing(_debugRow, 6);
        _adapter.SetMargin(_debugRow, HomeViewThickness{0, 0, 24, 2});
        _adapter.SetHorizontalAlignment(_debugRow, HomeViewHorizontalAlignment::Right);
        _adapter.SetVerticalAlignment(_debugRow, HomeViewVerticalAlignment::Bottom);
        _adapter.SetIsVisible(_debugRow, false);
        _adapter.AddChild(_debugRow, _shareBox);
        _adapter.AddChild(_debugRow, _debugBox);
        RefreshDebugSwitch();
        return _debugRow;
    }

    HomeViewTaskRef HomeView::ShareLogs()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            std::shared_ptr<ILogShare> sharer = LogShare::Current();
            if (_sharing || !sharer)
            {
                return _adapter.CompletedTask();
            }
            _sharing = true;
            _adapter.SetText(_shareLine, "\xE2\x86\x97 Zipping\xE2\x80\xA6");

            struct State
            {
                std::shared_ptr<ILogShare> Sharer;
                std::u16string Name;
                std::u16string Path;
                std::u16string Error;
                bool Built = false;
            };
            auto state = std::make_shared<State>();
            state->Sharer = std::move(sharer);
            state->Name = LogArchive::FileName();

            HomeViewTaskRef worker = _adapter.RunBackground([this, state]()
            {
                try
                {
                    state->Path = state->Sharer->StagingPath(state->Name);
                }
                catch (...)
                {
                    state->Error = _adapter.ToUtf16(
                        ExceptionMessage(_adapter, std::current_exception()));
                    state->Built = false;
                    return;
                }
                state->Built = LogArchive::Create(state->Path, state->Error);
            });

            return _adapter.ContinueWithAction(worker, [this, state]()
            {
                if (state->Built)
                {
                    state->Built = state->Sharer->Share(
                        state->Path, state->Name, state->Error);
                }
                _sharing = false;
                if (!state->Built)
                {
                    _adapter.SetText(_shareLine,
                        "\xE2\x86\x97 " + _adapter.FromUtf16(state->Error));
                    _adapter.SetForegroundColor(_shareLine, GuiTheme::Warm);
                    return;
                }
                RefreshShareButton();
            });
        });
    }

    void HomeView::RefreshShareButton()
    {
        if (!_shareBox || _sharing)
        {
            return;
        }
        _adapter.SetText(_shareLine, "\xE2\x86\x97 Share logs");
        _adapter.SetForeground(_shareLine, GuiTheme::TextDimBrush);
        _adapter.SetIsVisible(_shareBox, LogShare::Available());
    }

    void HomeView::ToggleDebugLogs()
    {
        LauncherPrefs::DebugLogs(!LauncherPrefs::DebugLogs());
        LauncherPrefs::Save();
        if (LauncherPrefs::DebugLogs())
        {
            DebugLog::Attach();
            DebugLog::Line("launcher",
                "debug logging turned on from the front screen");
        }
        else
        {
            DebugLog::Line("launcher",
                "debug logging turned off from the front screen");
            DebugLog::Detach();
        }
        RefreshDebugSwitch();
    }

    void HomeView::RefreshDebugSwitch()
    {
        if (!_debugLine)
        {
            return;
        }
        RefreshShareButton();
        if (!LauncherPrefs::DebugLogs())
        {
            _adapter.SetText(_debugLine, "\xE2\x96\xA2 Enable debugging logs");
            _adapter.SetForeground(_debugLine, GuiTheme::TextDimBrush);
            _adapter.SetToolTip(_debugBox,
                "Write everything this build can say about itself to a file, for a bug report.");
            return;
        }
        _adapter.SetText(_debugLine, "\xE2\x96\xA3 Debugging logs on");
        _adapter.SetForegroundColor(_debugLine, GuiTheme::Warm);
        const std::optional<std::string> path = DebugLog::Path();
        _adapter.SetToolTip(_debugBox, path.has_value()
            ? "Writing to " + *path
            : "Logging starts with the next run.");
    }

    std::string HomeView::VersionNumber()
    {
        const std::optional<Update::Version>& current = BuildVersion::Current();
        return current.has_value() ? current->ToString(3) : "a local build";
    }

    void HomeView::SayVersion(std::string text, GuiColor colour, bool pressable)
    {
        if (!_versionLine)
        {
            return;
        }
        _adapter.SetText(_versionLine, std::move(text));
        _adapter.SetForegroundColor(_versionLine, colour);
        _updatable = pressable;
        _adapter.SetCursor(_versionBox,
            pressable ? HomeViewCursor::Hand : HomeViewCursor::Arrow);
    }

    void HomeView::RefreshVersionLine()
    {
        if (!_versionLine || _updating)
        {
            return;
        }
        const std::string number = VersionNumber();
        if (Updater::Available().has_value())
        {
            SayVersion(number + " : Update available ! Click here to update",
                GuiTheme::Warm, true);
            return;
        }
        SayVersion(number,
            BuildVersion::IsRelease() && Updater::Checked()
                ? GuiTheme::Good : GuiTheme::TextDim);
    }

    const std::string& HomeView::Require(const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            throw HomeViewNullReferenceException();
        }
        return *value;
    }

    void HomeView::ShowUpdate(const Update::UpdateInfo& update)
    {
        const std::string& assetName = Require(update.AssetName.Get());
        const std::string tag = update.Tag.Get().value_or("");
        _adapter.UpdateBadgeShow(_updateBadge, assetName.empty()
            ? tag + " is out"
            : tag + " is out -- get " + assetName);
        _adapter.SetIsVisible(_updateBadge, !(_current == _homeCard));
        _adapter.SplashBottomInset(_splash,
            _adapter.UpdateBadgeDesiredHeight(_updateBadge) + 22);
        RefreshVersionLine();
    }

    void HomeView::UpdateNow()
    {
        std::optional<Update::UpdateInfo> found = Updater::Available();
        if (!found.has_value())
        {
            return;
        }
        Update::UpdateInfo update = *found;
        if (UpdateInstall::CanInstall(update))
        {
            _adapter.Forget(FetchAndInstall(update, UpdateInstall::Current()));
            return;
        }
        if (!Updater::OpenPage(update))
        {
            _adapter.UpdateBadgeSay(_updateBadge, update.PageUrl.Get());
        }
    }

    HomeViewTaskRef HomeView::FetchAndInstall(Update::UpdateInfo update,
        std::shared_ptr<Update::IUpdateInstaller> installer)
    {
        return CaptureTask(_adapter, [this, update = std::move(update),
            installer = std::move(installer)]() mutable -> HomeViewTaskRef
        {
            if (_updating)
            {
                return _adapter.CompletedTask();
            }
            const std::string number = VersionNumber();
            if (!installer)
            {
                throw HomeViewNullReferenceException();
            }
            if (!installer->Allowed())
            {
                SayVersion(number
                    + " : allow installs from this app, then press again",
                    GuiTheme::Warm, true);
                (void)installer->RequestPermission();
                return _adapter.CompletedTask();
            }
            _updating = true;
            installer->Finished([this, number](bool ok, std::string message)
            {
                _adapter.Post([this, number, ok, message = std::move(message)]()
                {
                    _updating = false;
                    SayVersion(ok ? number : number + " : " + message,
                        ok ? GuiTheme::TextDim : GuiTheme::Warm, !ok);
                });
            });

            const std::string& asset = Require(update.AssetName.Get());
            const std::string tag = update.Tag.Get().value_or("");
            const std::string label = !asset.empty() ? asset : tag;
            SayVersion(number + " : downloading " + label + "...",
                GuiTheme::Warm);

            struct State
            {
                std::mutex Reported;
                std::int32_t Shown = -1;
                std::string Error;
                bool Ready = false;
            };
            auto state = std::make_shared<State>();
            auto progress = [this, state, number, label](float fraction)
            {
                const std::int32_t percent = fraction < 0
                    ? -1 : static_cast<std::int32_t>(fraction * 100);
                {
                    std::lock_guard lock(state->Reported);
                    if (percent == state->Shown)
                    {
                        return;
                    }
                    state->Shown = percent;
                }
                _adapter.Post([this, number, label, percent]()
                {
                    SayVersion(percent < 0
                        ? number + " : downloading " + label + "..."
                        : number + " : downloading " + label + "... "
                            + _adapter.FormatCurrentInt32(percent) + "%",
                        GuiTheme::Warm);
                });
            };

            HomeViewTaskRef worker = _adapter.RunBackground(
                [state, installer, update, progress]() mutable
                {
                    state->Ready = installer->Prepare(
                        update, progress, state->Error);
                });

            return _adapter.ContinueWithTask(worker,
                [this, state, installer, number]() -> HomeViewTaskRef
                {
                    if (!state->Ready)
                    {
                        _updating = false;
                        SayVersion(number + " : "
                            + (!state->Error.empty() ? state->Error
                                : "the download failed"),
                            GuiTheme::Warm, true);
                        return _adapter.CompletedTask();
                    }
                    SayVersion(installer->ExitAfterInstall()
                        ? number + " : restarting to finish..."
                        : number + " : waiting for the system installer...",
                        GuiTheme::Warm);
                    state->Error.clear();
                    if (!installer->Install(state->Error))
                    {
                        _updating = false;
                        SayVersion(number + " : "
                            + (!state->Error.empty() ? state->Error
                                : "the install could not be started"),
                            GuiTheme::Warm, true);
                        return _adapter.CompletedTask();
                    }
                    if (installer->ExitAfterInstall())
                    {
                        Finish(LaunchPlan{});
                    }
                    return _adapter.CompletedTask();
                });
        });
    }

    void HomeView::RefreshGameFilesState()
    {
        const bool ready = GameFiles::Ready();
        _adapter.MenuEntryEnabled(_onlineEntry, ready);
        _adapter.MenuEntryEnabled(_hostEntry, ready);
        _adapter.MenuEntryEnabled(_demoEntry, ready);
        _adapter.MenuEntrySubtitle(_hostEntry, ready ? "" : GameFiles::Describe());
        _adapter.MenuEntrySubtitleColor(_hostEntry,
            ready ? GuiTheme::TextDim : GuiTheme::Warm);
    }

    HomeViewElement HomeView::BuildSetupCard()
    {
        HomeViewElement card = Card();
        HomeViewElement log = _adapter.CreateNote("");
        HomeViewElement scroll = _adapter.CreateScrollViewer();
        _adapter.SetHeight(scroll, 150);
        _adapter.SetScrollContent(scroll, log);
        _adapter.DisableHorizontalScrollBar(scroll);

        HomeViewElement choose = _adapter.CreateMenuEntry(
            "Choose your .nds file", "", 15);
        _adapter.MenuEntryPrimary(choose, true);
        _adapter.SetHeight(choose, 44);
        _adapter.AddMenuEntryClick(choose, [this, choose, log]()
        {
            _adapter.StartAsyncVoid([this, choose, log]()
            {
                return ChooseRom(choose, log);
            });
        });

        _adapter.AddChild(card, _adapter.CreateCaption("Game files"));
        _adapter.AddChild(card, _adapter.CreateNote(
            std::string(Branding::Name)
            + " needs your own Metroid Prime Hunters cartridge dump. It "
              "unpacks what it needs next to this program and leaves the file "
              "alone. No game data is included in this download, and none is "
              "downloaded."));
        if (GameFiles::InProcessSetup())
        {
            _adapter.AddChild(card, _adapter.CreateNote(
                "The unpacked files land in " + GameFiles::Root()
                + " -- this device's own folder for the app, which shows up over USB "
                  "under Android/data. Files already copied there are found without "
                  "picking anything.", GuiTheme::TextDim));
        }
        _adapter.AddChild(card, choose);

        _previewEntry = _adapter.CreateMenuEntry(
            "Render map previews", "", 13);
        _adapter.MenuEntryAccent(_previewEntry, GuiTheme::TextDim);
        _adapter.AddMenuEntryClick(_previewEntry, [this, log]()
        {
            _adapter.StartAsyncVoid([this, log]()
            {
                _adapter.MenuEntryEnabled(_previewEntry, false);
                _adapter.MenuEntryTitle(_previewEntry, "Rendering...");
                _previewProgress = _previewEntry;
                HomeViewTaskRef task = RenderPreviews(log);
                return _adapter.ContinueWithAction(task, [this]()
                {
                    _previewProgress = {};
                    _adapter.MenuEntryEnabled(_previewEntry, true);
                    _adapter.MenuEntryTitle(_previewEntry, "Render map previews");
                    RefreshPreviewEntry();
                });
            });
        });
        _adapter.AddChild(card, _previewEntry);
        _adapter.AddChild(card, _setupProgress);
        _adapter.AddChild(card, scroll);

        _setupBack = Back([this]() { ShowCard(_homeCard); });
        _adapter.SetIsVisible(_setupBack, false);
        _adapter.AddChild(card, _setupBack);
        return card;
    }

    HomeViewTaskRef HomeView::ChooseRom(const HomeViewElement& button,
        const HomeViewElement& log)
    {
        return CaptureTask(_adapter, [this, button, log]() -> HomeViewTaskRef
        {
            if (!_adapter.HasTopLevel())
            {
                return _adapter.CompletedTask();
            }
            HomeViewFilePickerOptions options;
            options.Title = "Your Metroid Prime Hunters cartridge dump";
            options.AllowMultiple = false;
            if (!_adapter.IsAndroid())
            {
                options.FileTypeFilter = std::vector<HomeViewFilePickerFilter>
                {
                    {"Nintendo DS ROM", {"*.nds"}},
                    {"Every file", {"*"}}
                };
            }

            auto picked = std::make_shared<std::vector<HomeViewStorageFile>>();
            HomeViewTaskRef picker = _adapter.OpenFilePickerAsync(options, picked);
            return _adapter.ContinueWithTask(picker,
                [this, button, log, picked]() -> HomeViewTaskRef
                {
                    if (picked->empty())
                    {
                        return _adapter.CompletedTask();
                    }
                    _adapter.MenuEntryEnabled(button, false);
                    _adapter.MenuEntryTitle(button, "Working...");
                    _adapter.NoteText(log, "");
                    auto progress = std::make_shared<SetupProgress>();
                    _adapter.SetIsVisible(_setupProgress, true);
                    _adapter.ProgressRowSet(_setupProgress, 0, "Starting");

                    struct State
                    {
                        std::optional<std::string> Path;
                        std::optional<std::string> Scratch;
                        bool CopyFailed = false;
                        bool Ok = false;
                    };
                    auto state = std::make_shared<State>();
                    state->Path = _adapter.TryGetLocalPath((*picked)[0]);

                    auto runSetup = [this, button, log, progress, state]()
                        -> HomeViewTaskRef
                    {
                        HomeViewTaskRef worker = _adapter.RunBackground(
                            [this, log, progress, state]()
                            {
                                if (!state->Path.has_value())
                                {
                                    throw HomeViewNullReferenceException();
                                }
                                state->Ok = GameFiles::RunSetup(*state->Path,
                                    [this, log, progress](const std::string& line)
                                    {
                                        _adapter.Post([this, log, progress, line]()
                                        {
                                            _adapter.NoteText(log, Tail(
                                                _adapter.NoteText(log), line));
                                            if (progress->Observe(line))
                                            {
                                                _adapter.ProgressRowSet(_setupProgress,
                                                    progress->Fraction(), progress->Stage());
                                            }
                                        });
                                    });
                            });

                        return _adapter.ContinueWithTask(worker,
                            [this, button, log, progress, state]() -> HomeViewTaskRef
                            {
                                if (state->Scratch.has_value())
                                {
                                    try
                                    {
                                        _adapter.DeleteFile(*state->Scratch);
                                    }
                                    catch (const HomeViewIOException&)
                                    {
                                    }
                                }
                                HomeViewTaskRef previews = state->Ok
                                    ? RenderPreviews(log, progress)
                                    : _adapter.CompletedTask();
                                return _adapter.ContinueWithAction(previews,
                                    [this, button, log, progress, state]()
                                    {
                                        progress->Finish(state->Ok);
                                        _adapter.ProgressRowSet(_setupProgress,
                                            progress->Fraction(), progress->Stage());
                                        _adapter.MenuEntryEnabled(button, true);
                                        _adapter.MenuEntryTitle(button,
                                            "Choose your .nds file");
                                        _adapter.NoteText(log, Tail(
                                            _adapter.NoteText(log), state->Ok
                                                ? "Ready to play."
                                                : "Setup did not finish."));
                                        RefreshGameFilesState();
                                        if (state->Ok)
                                        {
                                            RefreshRooms();
                                        }
                                        RefreshSplash();
                                        if (state->Ok)
                                        {
                                            _adapter.SetIsVisible(_setupBack, true);
                                            _adapter.SetIsVisible(_setupProgress, false);
                                            ShowCard(_homeCard);
                                        }
                                    });
                            });
                    };

                    if (state->Path.has_value())
                    {
                        return runSetup();
                    }

                    _adapter.NoteText(log, "Copying the file onto this device...");
                    HomeViewTaskRef copy;
                    try
                    {
                        state->Scratch = _adapter.CombinePath(
                            GameFiles::Root(), "picked.nds");
                        copy = _adapter.CopyStorageFileToPathAsync(
                            (*picked)[0], *state->Scratch);
                    }
                    catch (...)
                    {
                        _adapter.NoteText(log,
                            "The file could not be read: "
                            + ExceptionMessage(_adapter, std::current_exception()));
                        _adapter.MenuEntryEnabled(button, true);
                        _adapter.MenuEntryTitle(button,
                            "Choose your .nds file");
                        return _adapter.CompletedTask();
                    }

                    HomeViewTaskRef recovered = _adapter.CatchTask(copy,
                        [this, button, log, state](std::exception_ptr error)
                        {
                            state->CopyFailed = true;
                            _adapter.NoteText(log,
                                "The file could not be read: "
                                + ExceptionMessage(_adapter, error));
                            _adapter.MenuEntryEnabled(button, true);
                            _adapter.MenuEntryTitle(button,
                                "Choose your .nds file");
                            return _adapter.CompletedTask();
                        });
                    return _adapter.ContinueWithTask(recovered,
                        [this, state, runSetup]() mutable -> HomeViewTaskRef
                        {
                            if (state->CopyFailed)
                            {
                                return _adapter.CompletedTask();
                            }
                            state->Path = state->Scratch;
                            return runSetup();
                        });
                });
        });
    }

    HomeViewTaskRef HomeView::ChooseDemo()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            HomeViewElement view = _adapter.CreateDemoPickerView(
                DemoLibrary::List(), DemoLibrary::Directory());
            HomeViewTaskRef overlay = ShowOverlay(view,
                [this, view](HomeViewAdapter::Action handler)
                {
                    _adapter.AddDemoPickerClosed(view, std::move(handler));
                });
            return _adapter.ContinueWithTask(overlay,
                [this, view]() -> HomeViewTaskRef
                {
                    std::optional<std::string> path =
                        _adapter.DemoPickerPath(view);
                    if (path.has_value())
                    {
                        return PlayDemo(*path);
                    }
                    if (_adapter.DemoPickerImportRequested(view))
                    {
                        return ImportDemo();
                    }
                    return _adapter.CompletedTask();
                });
        });
    }

    HomeViewTaskRef HomeView::ImportDemo()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            if (!_adapter.HasTopLevel())
            {
                return _adapter.CompletedTask();
            }
            auto options = std::make_shared<HomeViewFilePickerOptions>();
            options->Title = "Demos";
            options->AllowMultiple = false;
            if (!_adapter.IsAndroid())
            {
                options->FileTypeFilter = std::vector<HomeViewFilePickerFilter>
                {
                    {std::string(Branding::Name) + " demo",
                        {"*" + std::string(DemoFile::Extension)}},
                    {"Every file", {"*"}}
                };
            }

            auto openPicker = [this, options]() -> HomeViewTaskRef
            {
                auto picked = std::make_shared<std::vector<HomeViewStorageFile>>();
                HomeViewTaskRef picker = _adapter.OpenFilePickerAsync(*options, picked);
                return _adapter.ContinueWithTask(picker,
                    [this, picked]() -> HomeViewTaskRef
                    {
                        if (picked->empty())
                        {
                            return _adapter.CompletedTask();
                        }
                        std::optional<std::string> path =
                            _adapter.TryGetLocalPath((*picked)[0]);
                        if (path.has_value())
                        {
                            return PlayDemo(*path);
                        }
                        std::string scratch;
                        HomeViewTaskRef copy;
                        try
                        {
                            scratch = _adapter.CombinePath(
                                GameFiles::Root(),
                                "picked" + std::string(DemoFile::Extension));
                            copy = _adapter.CopyStorageFileToPathAsync(
                                (*picked)[0], scratch);
                        }
                        catch (...)
                        {
                            _adapter.MenuEntrySubtitle(_demoEntry,
                                "That file could not be read: "
                                + ExceptionMessage(_adapter,
                                    std::current_exception()));
                            _adapter.MenuEntrySubtitleColor(
                                _demoEntry, GuiTheme::Warm);
                            return _adapter.CompletedTask();
                        }
                        auto copyFailed = std::make_shared<bool>(false);
                        HomeViewTaskRef recovered = _adapter.CatchTask(copy,
                            [this, copyFailed](std::exception_ptr error)
                            {
                                *copyFailed = true;
                                _adapter.MenuEntrySubtitle(_demoEntry,
                                    "That file could not be read: "
                                    + ExceptionMessage(_adapter, error));
                                _adapter.MenuEntrySubtitleColor(
                                    _demoEntry, GuiTheme::Warm);
                                return _adapter.CompletedTask();
                            });
                        return _adapter.ContinueWithTask(recovered,
                            [this, copyFailed, scratch]() -> HomeViewTaskRef
                            {
                                if (*copyFailed)
                                {
                                    return _adapter.CompletedTask();
                                }
                                return PlayDemo(scratch);
                            });
                    });
            };

            std::string demoDir;
            bool exists;
            try
            {
                demoDir = DemoLibrary::Directory();
                exists = _adapter.DirectoryExists(demoDir);
            }
            catch (const HomeViewIOException&)
            {
                return openPicker();
            }
            catch (const MphRead::Mods::Network::Detail::DemoLibraryIOException&)
            {
                return openPicker();
            }
            if (!exists)
            {
                return openPicker();
            }
            auto folder = std::make_shared<std::optional<HomeViewStorageFolder>>();
            HomeViewTaskRef locate;
            try
            {
                locate = _adapter.TryGetFolderFromPathAsync(demoDir, folder);
            }
            catch (const HomeViewIOException&)
            {
                return openPicker();
            }
            HomeViewTaskRef caught = _adapter.CatchTask(locate,
                [this, folder](std::exception_ptr error) -> HomeViewTaskRef
                {
                    try
                    {
                        std::rethrow_exception(error);
                    }
                    catch (const HomeViewIOException&)
                    {
                        folder->reset();
                        return _adapter.CompletedTask();
                    }
                    catch (...)
                    {
                        return _adapter.FaultedTask(error);
                    }
                });
            return _adapter.ContinueWithTask(caught,
                [options, folder, openPicker]() mutable -> HomeViewTaskRef
                {
                    if (folder->has_value())
                    {
                        options->SuggestedStartLocation = **folder;
                    }
                    return openPicker();
                });
        });
    }

    HomeViewTaskRef HomeView::PlayDemo(std::string path)
    {
        return CaptureTask(_adapter, [this, path = std::move(path)]() mutable
            -> HomeViewTaskRef
        {
            _adapter.MenuEntryEnabled(_demoEntry, false);
            _adapter.MenuEntryTitle(_demoEntry, "Loading...");
            auto joined = std::make_shared<bool>(false);
            HomeViewTaskRef worker = _adapter.RunBackground(
                [joined, path]() { *joined = DemoPlayback::Join(path); });
            return _adapter.ContinueWithAction(worker,
                [this, joined, path]()
                {
                    if (!*joined)
                    {
                        _adapter.MenuEntryEnabled(_demoEntry, true);
                        _adapter.MenuEntryTitle(_demoEntry, "Demos");
                        _adapter.MenuEntrySubtitle(_demoEntry,
                            DemoPlayback::LastError().value_or(
                                "That file could not be read as a demo."));
                        _adapter.MenuEntrySubtitleColor(
                            _demoEntry, GuiTheme::Warm);
                        return;
                    }
                    LaunchPlan::Init init;
                    init.Kind = LaunchKind::Demo;
                    init.DemoPath = path;
                    init.Hunter = Hunter::Samus;
                    init.PlayerName = std::string{};
                    init.RoomKey = std::string{};
                    Finish(LaunchPlan(init));
                });
        });
    }

    HomeViewTaskRef HomeView::RenderPreviews(const HomeViewElement& log,
        std::shared_ptr<SetupProgress> progress)
    {
        return CaptureTask(_adapter, [this, log, progress = std::move(progress)]()
            -> HomeViewTaskRef
        {
            if (!ThumbnailHost::CanRender())
            {
                return _adapter.CompletedTask();
            }
            _adapter.NoteText(log, Tail(_adapter.NoteText(log),
                "Rendering map previews..."));
            HomeViewTaskRef render = _adapter.RenderMissingPreviewsAsync(
                [this, log, progress](std::string line)
                {
                    _adapter.Post([this, log, progress, line = std::move(line)]()
                    {
                        _adapter.NoteText(log, Tail(_adapter.NoteText(log), line));
                        if (_previewProgress)
                        {
                            _adapter.MenuEntrySubtitle(_previewProgress, line);
                        }
                        if (progress && progress->Observe(line))
                        {
                            _adapter.ProgressRowSet(_setupProgress,
                                progress->Fraction(), progress->Stage());
                        }
                    });
                });
            return _adapter.ContinueWithAction(render,
                [this]() { RefreshSplash(); });
        });
    }

    void HomeView::RefreshPreviewEntry()
    {
        if (!_previewEntry)
        {
            return;
        }
        if (!GameFiles::Ready() || !ThumbnailHost::CanRender())
        {
            _adapter.SetIsVisible(_previewEntry, false);
            return;
        }
        const std::size_t missing = ThumbnailGenerator::MissingThumbnails().size();
        _adapter.SetIsVisible(_previewEntry, true);
        _adapter.MenuEntrySubtitle(_previewEntry, missing == 0
            ? "Every map has one"
            : _adapter.FormatCurrentInt32(static_cast<std::int32_t>(missing))
                + " still to render, from your own files");
        _adapter.MenuEntryEnabled(_previewEntry, missing > 0);
    }

    std::string HomeView::Tail(std::optional<std::string> existing,
        std::string line)
    {
        std::string all = existing.value_or("") + "\n" + line;
        std::vector<std::string> lines;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t end = all.find('\n', start);
            const std::string part = end == std::string::npos
                ? all.substr(start) : all.substr(start, end - start);
            if (!part.empty())
            {
                lines.push_back(part);
            }
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }
        const std::size_t begin = lines.size() > 8 ? lines.size() - 8 : 0;
        return JoinLines(lines, begin);
    }

    HomeViewElement HomeView::BuildOnlineCard()
    {
        HomeViewElement card = Card();
        const std::int32_t hunterIndex = IndexOf(Hunters,
            HunterName(LauncherPrefs::LastHunter()));
        _onlineHunter = _adapter.CreateChoiceRow("Hunter", Hunters, hunterIndex);
        _onlineAddress = _adapter.CreateFieldRow("Server",
            LauncherPrefs::ServerAddress() + ":"
                + _adapter.FormatCurrentInt32(LauncherPrefs::ServerPort()), 190);
        _onlineStatus = _adapter.CreateNote("Checking...");
        _adapter.AddFieldLostFocus(_onlineAddress,
            [this]() { QueryStatusSoon(); });
        _connect = _adapter.CreateMenuEntry("Connect", "", 16);
        _adapter.MenuEntryPrimary(_connect, true);
        _adapter.SetHeight(_connect, 44);
        _adapter.AddMenuEntryClick(_connect, [this]()
        {
            _adapter.StartAsyncVoid([this]() { return Connect(); });
        });

        _adapter.AddChild(card, _adapter.CreateCaption("Join"));
        _adapter.AddChild(card, _onlineHunter);
        _adapter.AddChild(card, _onlineAddress);
        _adapter.AddChild(card, _onlineStatus);
        _adapter.AddChild(card, _connect);
        _adapter.AddChild(card, Back([this]() { OpenJoin(); }));
        return card;
    }

    std::pair<std::string, std::int32_t> HomeView::OnlineEndpoint()
    {
        std::string host = LauncherPrefs::ServerAddress();
        std::int32_t port = LauncherPrefs::ServerPort();
        (void)ParseEndpoint(_adapter.FieldValue(_onlineAddress), host, port);
        return {std::move(host), port};
    }

    void HomeView::StartStatusPolling()
    {
        QueryStatusSoon();
        _statusTimer = _adapter.CreateDispatcherTimer();
        _adapter.TimerIntervalSeconds(_statusTimer, 4.0);
        _adapter.AddTimerTick(_statusTimer,
            [this]() { QueryStatusSoon(); });
        _adapter.StartTimer(_statusTimer);
    }

    void HomeView::StopStatusPolling()
    {
        if (_statusTimer)
        {
            _adapter.StopTimer(_statusTimer);
            _statusTimer = {};
        }
        if (_statusCancel)
        {
            _statusCancel->Cancel();
            _statusCancel.reset();
        }
    }

    void HomeView::QueryStatusSoon()
    {
        if (_statusCancel)
        {
            _statusCancel->Cancel();
        }
        auto cancel = std::make_shared<Cancellation>();
        _statusCancel = cancel;
        const auto [host, port] = OnlineEndpoint();
        HomeViewTaskRef worker = _adapter.RunBackground(
            [this, cancel, host, port]()
            {
                const HomeViewServerStatus status =
                    _adapter.NetStatusQuery(host, port, true);
                if (cancel->IsCancellationRequested.load())
                {
                    return;
                }
                _adapter.Post([this, cancel, status]()
                {
                    if (cancel->IsCancellationRequested.load())
                    {
                        return;
                    }
                    if (status.Online)
                    {
                        _adapter.NoteText(_onlineStatus, Describe(status));
                        _adapter.NoteForeground(_onlineStatus,
                            GuiTheme::GoodBrush);
                        _adapter.SplashShowRoom(_splash, status.RoomKey);
                    }
                    else
                    {
                        _adapter.NoteText(_onlineStatus,
                            "No answer -- it may be off, or UDP may be blocked.");
                        _adapter.NoteForeground(_onlineStatus,
                            GuiTheme::WarmBrush);
                    }
                });
            });
        _adapter.Forget(worker);
    }

    std::string HomeView::Describe(const HomeViewServerStatus& status) const
    {
        const std::string players = status.MaxPlayers > 0
            ? _adapter.FormatCurrentInt32(status.Players) + "/"
                + _adapter.FormatCurrentInt32(status.MaxPlayers)
            : std::to_string(status.Players);
        const std::string ping = status.Latency >= 0
            ? std::to_string(status.Latency) + " ms"
            : "-- ms";
        return status.RoomKey + " (" + _adapter.NetStatusModeName(status.Mode)
            + ") " + players + " players, " + ping;
    }

    HomeViewTaskRef HomeView::Connect()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            const auto [host, port] = OnlineEndpoint();
            const std::string name = PlayerName();
            const Hunter hunter = ParseHunter(_adapter.ChoiceValue(_onlineHunter));
            StopStatusPolling();
            _adapter.MenuEntryEnabled(_connect, false);
            _adapter.MenuEntryTitle(_connect, "Connecting");
            _adapter.NoteText(_onlineStatus,
                "Connecting to " + host + ":" + _adapter.FormatCurrentInt32(port) + "...");
            _adapter.NoteForeground(_onlineStatus, GuiTheme::TextDimBrush);

            LauncherPrefs::PlayerName(name);
            LauncherPrefs::LastHunter(hunter);
            LauncherPrefs::ServerAddress(host);
            LauncherPrefs::ServerPort(port);
            LauncherPrefs::LastKind(static_cast<std::int32_t>(LaunchKind::Online));
            LauncherPrefs::Save();

            auto joined = std::make_shared<bool>(false);
            HomeViewTaskRef worker = _adapter.RunBackground(
                [this, joined, host, port, name, hunter]()
                {
                    *joined = _adapter.NetJoin(host, port, name, hunter);
                });
            return _adapter.ContinueWithAction(worker,
                [this, joined, hunter, name, port]()
                {
                    _adapter.MenuEntryEnabled(_connect, true);
                    _adapter.MenuEntryTitle(_connect, "Connect");
                    if (!*joined)
                    {
                        _adapter.NetSessionStop();
                        _adapter.NoteText(_onlineStatus,
                            _adapter.NetLastJoinError());
                        _adapter.NoteForeground(_onlineStatus,
                            GuiTheme::BadBrush);
                        StartStatusPolling();
                        return;
                    }
                    LaunchPlan::Init init;
                    init.Kind = LaunchKind::Online;
                    init.Hunter = hunter;
                    init.PlayerName = name;
                    init.RoomKey = std::string{};
                    init.Mode = GameMode::Battle;
                    init.Port = port;
                    Finish(LaunchPlan(init));
                });
        });
    }

    HomeViewElement HomeView::BuildBattleGroup()
    {
        HomeViewElement group = Card();
        _hostWhere = _adapter.CreateChoiceRow(
            "Where", {"Local", "Online"}, 0);
        _adapter.AddChoiceChanged(_hostWhere, [this]()
        {
            _matchKind = _adapter.ChoiceIndex(_hostWhere) == 1
                ? LaunchKind::Host : LaunchKind::Offline;
            RefreshMatchCard();
        });

        if (!_settings)
        {
            throw HomeViewNullReferenceException();
        }
        const std::int32_t roomIndex = std::max<std::int32_t>(
            0, IndexOf(_playable, _settings->RoomKey));
        _matchMap = _adapter.CreateChoiceRow(
            "Map", _playable, roomIndex);
        _adapter.AddChoiceChanged(_matchMap,
            [this]() { RefreshSplash(); });

        HomeViewElement browseMaps = _adapter.CreateMenuEntry(
            "See every map", "", 13);
        _adapter.MenuEntryAccent(browseMaps, GuiTheme::TextDim);
        _adapter.AddMenuEntryClick(browseMaps, [this]()
        {
            _adapter.StartAsyncVoid([this]() { return BrowseMaps(); });
        });

        std::vector<std::string> modeLabels;
        modeLabels.reserve(Modes.size());
        for (const ModeEntry& entry : Modes)
        {
            modeLabels.emplace_back(entry.Label);
        }
        _matchMode = _adapter.CreateChoiceRow(
            "Match type", modeLabels, 0);
        _matchHunter = _adapter.CreateChoiceRow("Hunter", Hunters,
            IndexOf(Hunters, HunterName(LauncherPrefs::LastHunter())));

        std::vector<std::string> bots;
        bots.reserve(static_cast<std::size_t>(PlayerSlotCapacity));
        for (std::int32_t value = 0; value < PlayerSlotCapacity; ++value)
        {
            bots.push_back(std::to_string(value));
        }
        _matchBots = _adapter.CreateChoiceRow(
            "Bots", bots, LauncherPrefs::Bots());
        _matchSkill = _adapter.CreateChoiceRow(
            "Bot skill", {"Easy", "Normal", "Hard"},
            LauncherPrefs::BotLevel());
        _matchPort = _adapter.CreateFieldRow(
            "Port", std::to_string(LauncherPrefs::HostPort()), 90);
        _matchOnMaster = _adapter.CreateToggleRow(
            "Let the directory run it", true);
        _matchListed = _adapter.CreateToggleRow(
            "List it so others can find it", true);
        _matchNote = _adapter.CreateNote("");
        _matchStart = _adapter.CreateMenuEntry("Start", "", 16);
        _adapter.MenuEntryPrimary(_matchStart, true);
        _adapter.SetHeight(_matchStart, 44);
        _adapter.AddMenuEntryClick(_matchStart, [this]()
        {
            _adapter.StartAsyncVoid([this]() { return StartMatch(); });
        });

        _adapter.AddChild(group, _hostWhere);
        _adapter.AddChild(group, _matchMap);
        _adapter.AddChild(group, browseMaps);
        _adapter.AddChild(group, _matchMode);
        _adapter.AddChild(group, _matchHunter);
        _adapter.AddChild(group, _matchBots);
        _adapter.AddChild(group, _matchSkill);
        _adapter.AddChild(group, _matchNote);
        _adapter.AddChild(group, _matchStart);
        return group;
    }

    HomeViewElement HomeView::BuildAdventureGroup()
    {
        HomeViewElement group = Card();
        std::vector<std::string> slots;
        for (std::int32_t index = 0; index < AdventureSave::SlotCount; ++index)
        {
            slots.push_back("Slot " + _adapter.FormatCurrentInt32(index + 1));
        }
        _adventureSlot = _adapter.CreateChoiceRow("Save slot", slots, 0);
        _adapter.AddChoiceChanged(_adventureSlot,
            [this]() { RefreshAdventureCard(); });
        _adventureHunter = _adapter.CreateChoiceRow("Hunter", Hunters,
            IndexOf(Hunters, HunterName(LauncherPrefs::LastHunter())));
        _adventureNote = _adapter.CreateNote("");
        _adventureStart = _adapter.CreateMenuEntry("Continue", "", 16);
        _adapter.MenuEntryPrimary(_adventureStart, true);
        _adapter.SetHeight(_adventureStart, 44);
        _adapter.AddMenuEntryClick(_adventureStart,
            [this]() { StartAdventure(false); });
        _adventureNew = _adapter.CreateMenuEntry("New game");
        _adapter.AddMenuEntryClick(_adventureNew,
            [this]() { StartAdventure(true); });

        _adapter.AddChild(group, _adventureSlot);
        _adapter.AddChild(group, _adventureNote);
        _adapter.AddChild(group, _adventureHunter);
        _adapter.AddChild(group, _adventureStart);
        _adapter.AddChild(group, _adventureNew);
        return group;
    }

    HomeViewElement HomeView::BuildHostCard()
    {
        HomeViewElement card = Card();
        _hostMode = _adapter.CreateChoiceRow(
            "Mode", {"Adventure", "Battle"}, 0);
        _adapter.AddChoiceChanged(_hostMode,
            [this]() { RefreshHostCard(); });
        _hostCoop = _adapter.CreateToggleRow(
            "Online co-op (coming soon!)", false);
        _adapter.AddToggleChanged(_hostCoop,
            [this]() { RefreshAdventureCard(); });
        _hostAdventure = BuildAdventureGroup();
        _adapter.InsertChild(_hostAdventure, 0, _hostCoop);
        _hostBattle = BuildBattleGroup();

        _adapter.AddChild(card, _adapter.CreateCaption("Host"));
        _adapter.AddChild(card, _hostMode);
        _adapter.AddChild(card, _hostAdventure);
        _adapter.AddChild(card, _hostBattle);
        _adapter.AddChild(card, Back([this]() { ShowCard(_homeCard); }));
        return card;
    }

    void HomeView::OpenHost()
    {
        RefreshHostCard();
        ShowCard(_hostCard);
        RefreshSplash();
    }

    void HomeView::RefreshHostCard()
    {
        const bool adventure = _adapter.ChoiceIndex(_hostMode) == 0;
        _adapter.SetIsVisible(_hostAdventure, adventure);
        _adapter.SetIsVisible(_hostBattle, !adventure);
        if (adventure)
        {
            RefreshAdventureCard();
        }
        else
        {
            _matchKind = _adapter.ChoiceIndex(_hostWhere) == 1
                ? LaunchKind::Host : LaunchKind::Offline;
            RefreshMatchCard();
        }
    }

    void HomeView::RefreshAdventureCard()
    {
        const AdventureSave::SlotInfo info = AdventureSave::Read(CurrentSlot());
        _adapter.NoteText(_adventureNote, info.Describe());
        _adapter.SetIsVisible(_adventureNote, true);
        _adapter.MenuEntryTitle(_adventureStart,
            info.Used ? "Continue" : "Start a new game");
        _adapter.SetIsVisible(_adventureNew, info.Used);
        const bool ready = !_adapter.ToggleOn(_hostCoop);
        _adapter.MenuEntryEnabled(_adventureStart, ready);
        _adapter.MenuEntryEnabled(_adventureNew, ready);
        if (!ready)
        {
            _adapter.NoteText(_adventureNote,
                "Online co-op is not built yet. Untick it to play.");
        }
    }

    std::uint8_t HomeView::CurrentSlot() const
    {
        const std::int32_t value = std::clamp(
            _adapter.ChoiceIndex(_adventureSlot) + 1,
            1, AdventureSave::SlotCount);
        return static_cast<std::uint8_t>(value);
    }

    void HomeView::StartAdventure(bool newGame)
    {
        const std::uint8_t slot = CurrentSlot();
        if (!AdventureSave::Read(slot).Used)
        {
            newGame = true;
        }
        const Hunter hunter = ParseHunter(
            _adapter.ChoiceValue(_adventureHunter));
        LauncherPrefs::LastHunter(hunter);
        LauncherPrefs::LastKind(static_cast<std::int32_t>(LaunchKind::Adventure));
        LauncherPrefs::Save();

        LaunchPlan::Init init;
        init.Kind = LaunchKind::Adventure;
        init.Hunter = hunter;
        init.PlayerName = LauncherPrefs::PlayerName();
        init.RoomKey = std::string{};
        init.SaveSlot = slot;
        init.NewGame = newGame;
        Finish(LaunchPlan(init));
    }

    void HomeView::RefreshPrefRows()
    {
        _adapter.FieldValue(_onlineAddress,
            LauncherPrefs::ServerAddress() + ":"
                + _adapter.FormatCurrentInt32(LauncherPrefs::ServerPort()));
        const std::int32_t hunter = IndexOf(Hunters,
            HunterName(LauncherPrefs::LastHunter()));
        if (hunter >= 0)
        {
            _adapter.ChoiceIndex(_onlineHunter, hunter);
            _adapter.ChoiceIndex(_matchHunter, hunter);
            _adapter.ChoiceIndex(_adventureHunter, hunter);
        }
    }

    HomeViewTaskRef HomeView::BrowseMaps()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            if (_playable.empty())
            {
                return _adapter.CompletedTask();
            }
            HomeViewElement view = _adapter.CreateMapPickerView(
                _playable, _adapter.ChoiceValue(_matchMap));
            HomeViewTaskRef overlay = ShowOverlay(view,
                [this, view](HomeViewAdapter::Action handler)
                {
                    _adapter.AddMapPickerClosed(view, std::move(handler));
                });
            return _adapter.ContinueWithAction(overlay, [this, view]()
            {
                const std::optional<std::string> room =
                    _adapter.MapPickerRoomKey(view);
                if (!room.has_value())
                {
                    return;
                }
                const std::int32_t index = IndexOf(_playable, *room);
                if (index >= 0)
                {
                    _adapter.ChoiceIndex(_matchMap, index);
                    RefreshSplash();
                }
            });
        });
    }

    std::string HomeView::PlayerName() const
    {
        const std::string name = _adapter.Trim(LauncherPrefs::PlayerName());
        return !name.empty() ? name : "Player";
    }

    void HomeView::OpenJoin()
    {
        _browseReturn = _homeCard;
        ShowCard(_browseCard);
        ReloadServers();
    }

    void HomeView::RefreshMatchCard()
    {
        const bool host = _matchKind == LaunchKind::Host;
        _adapter.SetIsVisible(_matchBots, !host);
        _adapter.SetIsVisible(_matchSkill, !host);
        _adapter.ToggleOn(_matchOnMaster, true);
        _adapter.ToggleOn(_matchListed, true);
        const std::string text = host
            ? "The directory runs the match, so nothing here needs a "
              "forwarded port. To run one on your own machine, use the "
              "dedicated server."
            : "";
        _adapter.NoteText(_matchNote, text);
        _adapter.SetIsVisible(_matchNote, !text.empty());
    }

    HomeViewTaskRef HomeView::StartMatch()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            if (_playable.empty())
            {
                _adapter.NoteText(_matchNote,
                    "No multiplayer rooms were found.");
                _adapter.SetIsVisible(_matchNote, true);
                return _adapter.CompletedTask();
            }
            const std::string roomKey = _adapter.ChoiceValue(_matchMap);
            const std::int32_t modeIndex = _adapter.ChoiceIndex(_matchMode);
            if (modeIndex < 0
                || static_cast<std::size_t>(modeIndex) >= Modes.size())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            const GameMode mode = Modes[static_cast<std::size_t>(modeIndex)].Mode;
            const Hunter hunter = ParseHunter(
                _adapter.ChoiceValue(_matchHunter));
            if (!_settings)
            {
                throw HomeViewNullReferenceException();
            }
            _settings->RoomKey = roomKey;
            LauncherPrefs::LastHunter(hunter);
            LauncherPrefs::LastKind(static_cast<std::int32_t>(_matchKind));

            if (_matchKind == LaunchKind::Offline)
            {
                LauncherPrefs::Bots(_adapter.ChoiceIndex(_matchBots));
                LauncherPrefs::BotLevel(_adapter.ChoiceIndex(_matchSkill));
                LauncherPrefs::Save();
                LaunchPlan::Init init;
                init.Kind = LaunchKind::Offline;
                init.Hunter = hunter;
                init.PlayerName = LauncherPrefs::PlayerName();
                init.RoomKey = roomKey;
                init.Mode = mode;
                init.Bots = _adapter.ChoiceIndex(_matchBots);
                init.BotLevel = _adapter.ChoiceIndex(_matchSkill);
                Finish(LaunchPlan(init));
                return _adapter.CompletedTask();
            }

            const std::string name = PlayerName();
            LauncherPrefs::PlayerName(name);
            LauncherPrefs::HostOnMaster(_adapter.ToggleOn(_matchOnMaster));
            LauncherPrefs::ListHostedGame(_adapter.ToggleOn(_matchListed));
            std::int32_t port = 0;
            if (_adapter.TryParseInt32InvariantInteger(
                    _adapter.FieldValue(_matchPort), port)
                && port > 0 && port <= 65535)
            {
                LauncherPrefs::HostPort(port);
            }
            LauncherPrefs::Save();

            _adapter.MenuEntryEnabled(_matchStart, false);
            _adapter.MenuEntryTitle(_matchStart, "Starting");
            _adapter.SetIsVisible(_matchNote, true);
            auto ok = std::make_shared<bool>(false);
            HomeViewTaskRef worker;
            if (_adapter.ToggleOn(_matchOnMaster))
            {
                _adapter.NoteText(_matchNote,
                    "Asking " + LauncherPrefs::MasterHost()
                    + " to run " + roomKey + "...");
                worker = _adapter.RunBackground(
                    [this, ok, roomKey, mode, name, hunter]()
                    {
                        const HomeViewHostedGame game =
                            _adapter.NetMasterRequestGame(
                                LauncherPrefs::MasterHost(),
                                LauncherPrefs::MasterPort(), roomKey, mode,
                                7 * 60, 7, PlayerSlotCapacity,
                                name + "'s game");
                        *ok = game.Started
                            && _adapter.NetJoin(game.Host, game.Port,
                                name, hunter);
                    });
            }
            else
            {
                _adapter.NoteText(_matchNote,
                    "Starting a server on port "
                    + _adapter.FormatCurrentInt32(LauncherPrefs::HostPort()) + "...");
                const bool listed = _adapter.ToggleOn(_matchListed);
                worker = _adapter.RunBackground(
                    [this, ok, roomKey, mode, name, hunter, listed]()
                    {
                        std::optional<std::tuple<std::string,
                            std::int32_t, std::string>> listing;
                        if (listed)
                        {
                            listing = std::make_tuple(
                                LauncherPrefs::MasterHost(),
                                LauncherPrefs::MasterPort(),
                                name + "'s game");
                        }
                        *ok = _adapter.NetHostStartAndJoin(
                            LauncherPrefs::HostPort(), name, hunter,
                            roomKey, mode, 7 * 60, 7, std::move(listing));
                    });
            }

            return _adapter.ContinueWithAction(worker,
                [this, ok, hunter, name, roomKey, mode]()
                {
                    _adapter.MenuEntryEnabled(_matchStart, true);
                    _adapter.MenuEntryTitle(_matchStart, "Start");
                    if (!*ok)
                    {
                        _adapter.NetSessionStop();
                        _adapter.NetHostStop();
                        _adapter.NoteText(_matchNote,
                            _adapter.NetHostLastError().value_or(
                                "The game could not be started. The port may be in use, or the "
                                "directory may be down."));
                        _adapter.NoteForeground(_matchNote,
                            GuiTheme::BadBrush);
                        return;
                    }
                    LaunchPlan::Init init;
                    init.Kind = LaunchKind::Host;
                    init.Hunter = hunter;
                    init.PlayerName = name;
                    init.RoomKey = roomKey;
                    init.Mode = mode;
                    init.Port = LauncherPrefs::HostPort();
                    Finish(LaunchPlan(init));
                });
        });
    }

    HomeViewElement HomeView::BuildBrowseCard()
    {
        HomeViewElement card = Card();
        _browseList = _adapter.CreateStackPanel();
        _adapter.SetStackSpacing(_browseList, 2);
        _browseNote = _adapter.CreateNote("");
        HomeViewElement refresh = _adapter.CreateMenuEntry(
            "Refresh", std::string{}, 15);
        _adapter.AddMenuEntryClick(refresh,
            [this]() { ReloadServers(); });

        _adapter.AddChild(card, _adapter.CreateCaption("Join"));
        _adapter.AddChild(card, _browseNote);
        _adapter.AddChild(card, _adapter.CreateServerHeader());
        HomeViewElement scroll = _adapter.CreateScrollViewer();
        _adapter.SetHeight(scroll, 300);
        _adapter.SetScrollContent(scroll, _browseList);
        _adapter.DisableHorizontalScrollBar(scroll);
        _adapter.AddChild(card, scroll);
        _adapter.AddChild(card, refresh);
        _adapter.AddChild(card, Back([this]()
        {
            ShowCard(_browseReturn ? _browseReturn : _homeCard);
        }));
        return card;
    }

    void HomeView::ReloadServers()
    {
        _adapter.ClearChildren(_browseList);
        _adapter.NoteText(_browseNote,
            "Asking " + LauncherPrefs::MasterHost() + "...");
        _adapter.NoteForeground(_browseNote, GuiTheme::TextDimBrush);
        HomeViewTaskRef worker = _adapter.RunBackground([this]()
        {
            HomeViewMasterListResult result = _adapter.NetMasterQuery(
                LauncherPrefs::MasterHost(), LauncherPrefs::MasterPort());
            _adapter.Post([this, result = std::move(result)]() mutable
            {
                if (!result.Answered)
                {
                    _adapter.NoteText(_browseNote,
                        "The directory did not answer. It may be down, "
                        "or UDP may not reach it.");
                    _adapter.NoteForeground(_browseNote, GuiTheme::WarmBrush);
                    return;
                }
                if (!result.Servers)
                {
                    throw HomeViewNullReferenceException();
                }
                if (result.Servers->empty())
                {
                    _adapter.NoteText(_browseNote,
                        "The directory is up and has nobody listed.");
                    _adapter.NoteForeground(_browseNote, GuiTheme::WarmBrush);
                    return;
                }
                _adapter.NoteText(_browseNote,
                    _adapter.FormatCurrentInt32(
                        static_cast<std::int32_t>(result.Servers->size())) + " listed.");
                for (const HomeViewMasterListing& listing : *result.Servers)
                {
                    AddServerRow(listing);
                }
            });
        });
        _adapter.Forget(worker);
    }

    void HomeView::AddServerRow(HomeViewMasterListing listing)
    {
        const std::string name = !listing.ServerName.empty()
            ? listing.ServerName : listing.Endpoint;
        HomeViewElement row = _adapter.CreateServerRow(name, listing.Endpoint);
        _adapter.SetToolTip(row, listing.Endpoint);
        _adapter.AddServerRowClicked(row, [this, listing]()
        {
            _adapter.FieldValue(_onlineAddress,
                listing.Address + ":" + _adapter.FormatCurrentInt32(listing.Port));
            ShowCard(_onlineCard);
            QueryStatusSoon();
        });
        _adapter.AddChild(_browseList, row);
        HomeViewTaskRef worker = _adapter.RunBackground(
            [this, row, listing]()
            {
                const HomeViewServerStatus status = _adapter.NetStatusQuery(
                    listing.Address, listing.Port, false);
                _adapter.Post([this, row, status]()
                {
                    _adapter.ServerRowSetStatus(row, status);
                });
            });
        _adapter.Forget(worker);
    }

    HomeViewTaskRef HomeView::OpenSettings()
    {
        return CaptureTask(_adapter, [this]() -> HomeViewTaskRef
        {
            HomeViewElement view;
            auto gameFiles = std::make_shared<bool>(false);
            HomeViewTaskRef guarded;
            try
            {
                view = _adapter.CreateSettingsView(_settings);
                _adapter.AddSettingsGameFilesRequested(view,
                    [gameFiles]() { *gameFiles = true; });
                HomeViewTaskRef overlay = ShowOverlay(view,
                    [this, view](HomeViewAdapter::Action handler)
                    {
                        _adapter.AddSettingsClosed(view, std::move(handler));
                    });
                guarded = _adapter.ContinueWithAction(overlay,
                    [this, gameFiles]()
                    {
                        if (*gameFiles)
                        {
                            ShowCard(_setupCard);
                        }
                    });
            }
            catch (...)
            {
                _adapter.ConsoleWriteLine(
                    "[launcher] the settings could not be opened: "
                    + ExceptionMessage(_adapter, std::current_exception()));
                return _adapter.CompletedTask();
            }

            auto failed = std::make_shared<bool>(false);
            HomeViewTaskRef caught = _adapter.CatchTask(guarded,
                [this, failed](std::exception_ptr error)
                {
                    *failed = true;
                    _adapter.ConsoleWriteLine(
                        "[launcher] the settings could not be opened: "
                        + ExceptionMessage(_adapter, error));
                    return _adapter.CompletedTask();
                });
            return _adapter.ContinueWithAction(caught,
                [this, failed, gameFiles]()
                {
                    if (*failed || *gameFiles)
                    {
                        return;
                    }
                    if (!_settings)
                    {
                        throw HomeViewNullReferenceException();
                    }
                    const std::int32_t index = IndexOf(
                        _playable, _settings->RoomKey);
                    if (index >= 0 && _matchMap)
                    {
                        _adapter.ChoiceIndex(_matchMap, index);
                    }
                    RefreshPrefRows();
                    RefreshSplash();
                });
        });
    }

    void HomeView::RefreshSplash()
    {
        RefreshGameFilesState();
        std::optional<std::string> room;
        if (!_playable.empty() && _matchMap)
        {
            room = _adapter.ChoiceValue(_matchMap);
        }
        if (_current == _setupCard)
        {
            room.reset();
        }
        _adapter.SplashShowRoom(_splash, room);
    }

    void HomeView::RefreshRooms()
    {
        if (!GameFiles::Ready())
        {
            return;
        }
        _playable.clear();
        for (std::string room : ThumbnailGenerator::MultiplayerRooms())
        {
            _playable.push_back(std::move(room));
        }
        if (_matchMap)
        {
            if (!_settings)
            {
                throw HomeViewNullReferenceException();
            }
            const std::int32_t index = std::max<std::int32_t>(
                0, IndexOf(_playable, _settings->RoomKey));
            _adapter.ChoiceSetItems(_matchMap, _playable, index);
        }
    }

    bool HomeView::ParseEndpoint(std::string text,
        std::string& host, std::int32_t& port) const
    {
        text = _adapter.Trim(std::move(text));
        if (text.empty())
        {
            return false;
        }
        const std::size_t found = text.rfind(':');
        const std::int64_t colon = found == std::string::npos
            ? -1 : static_cast<std::int64_t>(found);
        if (colon <= 0)
        {
            host = std::move(text);
            return true;
        }
        std::int32_t parsed = 0;
        if (!_adapter.TryParseInt32InvariantInteger(
                std::string_view(text).substr(static_cast<std::size_t>(colon + 1)),
                parsed)
            || parsed < 1 || parsed > 65535)
        {
            return false;
        }
        host = text.substr(0, static_cast<std::size_t>(colon));
        port = parsed;
        return true;
    }

    std::string HomeView::HunterName(Hunter hunter)
    {
        switch (hunter)
        {
        case Hunter::Samus: return "Samus";
        case Hunter::Kanden: return "Kanden";
        case Hunter::Trace: return "Trace";
        case Hunter::Sylux: return "Sylux";
        case Hunter::Noxus: return "Noxus";
        case Hunter::Spire: return "Spire";
        case Hunter::Weavel: return "Weavel";
        case Hunter::Guardian: return "Guardian";
        case Hunter::Random: return "Random";
        default:
            return std::to_string(static_cast<std::uint8_t>(hunter));
        }
    }

    Hunter HomeView::ParseHunter(std::string_view text)
    {
        if (text == "Samus") return Hunter::Samus;
        if (text == "Kanden") return Hunter::Kanden;
        if (text == "Trace") return Hunter::Trace;
        if (text == "Sylux") return Hunter::Sylux;
        if (text == "Noxus") return Hunter::Noxus;
        if (text == "Spire") return Hunter::Spire;
        if (text == "Weavel") return Hunter::Weavel;
        if (text == "Guardian") return Hunter::Guardian;
        if (text == "Random") return Hunter::Random;

        std::int32_t value = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        auto parsed = std::from_chars(first, last, value, 10);
        if (parsed.ec == std::errc{} && parsed.ptr == last
            && value >= std::numeric_limits<std::uint8_t>::min()
            && value <= std::numeric_limits<std::uint8_t>::max())
        {
            return static_cast<Hunter>(static_cast<std::uint8_t>(value));
        }
        throw std::invalid_argument(
            "Requested value '" + std::string(text) + "' was not found.");
    }
}

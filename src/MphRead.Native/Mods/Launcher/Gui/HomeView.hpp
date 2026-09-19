#pragma once

#include "../Portable/LaunchPlan.hpp"
#include "../../../Formats/Formats.hpp"
#include "GuiTheme.hpp"
#include "../../Network/DemoLibrary.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::Mods::Launcher
{
    class SetupProgress;
}

namespace MphRead::Mods::Update
{
    struct UpdateInfo;
    class IUpdateInstaller;
}

namespace MphRead::Mods::Launcher::Gui
{
    class HomeViewNullReferenceException final : public std::runtime_error
    {
    public:
        HomeViewNullReferenceException();
    };

    class HomeViewIOException : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    enum class HomeViewKey : std::uint8_t
    {
        Other,
        Escape
    };

    enum class HomeViewHorizontalAlignment : std::uint8_t
    {
        Left,
        Right
    };

    enum class HomeViewVerticalAlignment : std::uint8_t
    {
        Bottom
    };

    enum class HomeViewOrientation : std::uint8_t
    {
        Horizontal
    };

    enum class HomeViewCursor : std::uint8_t
    {
        Arrow,
        Hand
    };

    enum class HomeViewDispatcherPriority : std::uint8_t
    {
        Normal,
        Background
    };

    struct HomeViewThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        [[nodiscard]] static constexpr HomeViewThickness Uniform(double value) noexcept
        {
            return HomeViewThickness{value, value, value, value};
        }
    };

    struct HomeViewKeyEventArgs final
    {
        void* Native = nullptr;
        HomeViewKey Key = HomeViewKey::Other;
        bool Handled = false;
    };

    struct HomeViewPointerEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct HomeViewElement final
    {
        std::shared_ptr<void> Native{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return Native != nullptr;
        }

        [[nodiscard]] const void* Identity() const noexcept
        {
            return Native.get();
        }

        friend bool operator==(const HomeViewElement& left,
            const HomeViewElement& right) noexcept
        {
            return left.Native.get() == right.Native.get();
        }
    };

    struct HomeViewStorageFile final
    {
        std::shared_ptr<void> Native{};
    };

    struct HomeViewStorageFolder final
    {
        std::shared_ptr<void> Native{};
    };

    struct HomeViewFilePickerFilter final
    {
        std::string Name;
        std::vector<std::string> Patterns;
    };

    struct HomeViewFilePickerOptions final
    {
        std::string Title;
        bool AllowMultiple = false;
        std::optional<std::vector<HomeViewFilePickerFilter>> FileTypeFilter{};
        std::optional<HomeViewStorageFolder> SuggestedStartLocation{};
    };

    struct HomeViewServerStatus final
    {
        bool Online = false;
        std::string RoomKey{};
        GameMode Mode = GameMode::None;
        std::int32_t Players = 0;
        std::int32_t MaxPlayers = 0;
        std::int32_t Latency = 0;
    };

    struct HomeViewMasterListing final
    {
        std::string Address{};
        std::int32_t Port = 0;
        std::string ServerName{};
        std::string Endpoint{};
    };

    struct HomeViewMasterListResult final
    {
        std::shared_ptr<const std::vector<HomeViewMasterListing>> Servers{};
        bool Answered = false;
    };

    struct HomeViewHostedGame final
    {
        bool Started = false;
        std::string Host{};
        std::int32_t Port = 0;
    };

    class HomeViewTask
    {
    public:
        virtual ~HomeViewTask() = default;
    };

    using HomeViewTaskRef = std::shared_ptr<HomeViewTask>;

    class HomeViewRoomEnumerator
    {
    public:
        virtual ~HomeViewRoomEnumerator() = default;
        [[nodiscard]] virtual bool MoveNext() = 0;
        [[nodiscard]] virtual std::string Current() const = 0;
        virtual void Dispose() = 0;
    };

    class HomeViewRoomList
    {
    public:
        virtual ~HomeViewRoomList() = default;
        [[nodiscard]] virtual std::shared_ptr<HomeViewRoomEnumerator> GetEnumerator() const = 0;
    };

    using HomeViewRoomListRef = std::shared_ptr<const HomeViewRoomList>;

    class HomeViewAdapter
    {
    public:
        using Action = std::function<void()>;
        using PointerAction = std::function<void(HomeViewPointerEventArgs&)>;
        using SizeChangedAction = std::function<void(double)>;

        virtual ~HomeViewAdapter() = default;

        // UserControl / dispatcher / task mechanics.
        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetContent(const HomeViewElement& content) = 0;
        virtual void AddSizeChanged(SizeChangedAction action) = 0;
        virtual void BaseOnKeyDown(HomeViewKeyEventArgs& e) = 0;
        virtual void Post(Action action,
            HomeViewDispatcherPriority priority = HomeViewDispatcherPriority::Normal) = 0;
        [[nodiscard]] virtual HomeViewTaskRef CompletedTask() = 0;
        [[nodiscard]] virtual HomeViewTaskRef FaultedTask(std::exception_ptr error) = 0;
        [[nodiscard]] virtual HomeViewTaskRef PendingTask() = 0;
        virtual bool TrySetTaskResult(const HomeViewTaskRef& task) = 0;
        [[nodiscard]] virtual HomeViewTaskRef RunBackground(Action action) = 0;
        [[nodiscard]] virtual HomeViewTaskRef ContinueWithTask(
            const HomeViewTaskRef& task, std::function<HomeViewTaskRef()> continuation) = 0;
        [[nodiscard]] virtual HomeViewTaskRef ContinueWithAction(
            const HomeViewTaskRef& task, Action continuation) = 0;
        [[nodiscard]] virtual HomeViewTaskRef CatchTask(
            const HomeViewTaskRef& task,
            std::function<HomeViewTaskRef(std::exception_ptr)> handler) = 0;
        virtual void StartAsyncVoid(std::function<HomeViewTaskRef()> body) = 0;
        virtual void Forget(const HomeViewTaskRef& task) = 0;

        // Generic Avalonia tree mechanics.
        [[nodiscard]] virtual HomeViewElement CreatePanel() = 0;
        [[nodiscard]] virtual HomeViewElement CreateGrid() = 0;
        [[nodiscard]] virtual HomeViewElement CreateScrollViewer() = 0;
        [[nodiscard]] virtual HomeViewElement CreateBorder() = 0;
        [[nodiscard]] virtual HomeViewElement CreateStackPanel() = 0;
        [[nodiscard]] virtual HomeViewElement CreateTextBlock() = 0;
        virtual void AddChild(const HomeViewElement& parent, const HomeViewElement& child) = 0;
        virtual void InsertChild(const HomeViewElement& parent, std::int32_t index,
            const HomeViewElement& child) = 0;
        virtual void ClearChildren(const HomeViewElement& parent) = 0;
        virtual void SetGridRowDefinitions(const HomeViewElement& grid,
            std::string_view definitions) = 0;
        virtual void SetGridColumnDefinitions(const HomeViewElement& grid,
            std::string_view definitions) = 0;
        virtual void SetGridRow(const HomeViewElement& child, std::int32_t row) = 0;
        virtual void SetGridColumn(const HomeViewElement& child, std::int32_t column) = 0;
        virtual void SetScrollContent(const HomeViewElement& scroll,
            const HomeViewElement& content) = 0;
        virtual void DisableHorizontalScrollBar(const HomeViewElement& scroll) = 0;
        virtual void SetPanelBackground(const HomeViewElement& panel,
            const GuiBrush& brush) = 0;
        virtual void SetBorderBackground(const HomeViewElement& border,
            const GuiBrush& brush) = 0;
        virtual void SetBorderBrush(const HomeViewElement& border,
            const GuiBrush& brush) = 0;
        virtual void SetBorderThickness(const HomeViewElement& border,
            HomeViewThickness thickness) = 0;
        virtual void SetBorderCornerRadius(const HomeViewElement& border, double radius) = 0;
        virtual void SetBorderPadding(const HomeViewElement& border,
            HomeViewThickness padding) = 0;
        virtual void SetBorderChild(const HomeViewElement& border,
            const HomeViewElement& child) = 0;
        virtual void SetMargin(const HomeViewElement& element, HomeViewThickness margin) = 0;
        virtual void SetWidth(const HomeViewElement& element, double width) = 0;
        virtual void SetWidthAuto(const HomeViewElement& element) = 0;
        virtual void SetHeight(const HomeViewElement& element, double height) = 0;
        virtual void SetHeightAuto(const HomeViewElement& element) = 0;
        virtual void SetIsVisible(const HomeViewElement& element, bool visible) = 0;
        [[nodiscard]] virtual bool GetIsVisible(const HomeViewElement& element) const = 0;
        virtual void SetHorizontalAlignment(const HomeViewElement& element,
            HomeViewHorizontalAlignment alignment) = 0;
        virtual void SetVerticalAlignment(const HomeViewElement& element,
            HomeViewVerticalAlignment alignment) = 0;
        virtual void SetCursor(const HomeViewElement& element, HomeViewCursor cursor) = 0;
        virtual void SetStackSpacing(const HomeViewElement& stack, double spacing) = 0;
        virtual void SetStackOrientation(const HomeViewElement& stack,
            HomeViewOrientation orientation) = 0;
        virtual void Focus(const HomeViewElement& element) = 0;
        virtual void FocusFirstFocusableDescendant(const HomeViewElement& element) = 0;
        virtual void AddPointerPressed(const HomeViewElement& element,
            PointerAction action) = 0;
        virtual void SetToolTip(const HomeViewElement& element, std::string text) = 0;

        // TextBlock mechanics.
        virtual void SetText(const HomeViewElement& textBlock,
            std::optional<std::string> text) = 0;
        virtual void SetFontSize(const HomeViewElement& textBlock, double size) = 0;
        virtual void SetForeground(const HomeViewElement& element,
            const GuiBrush& brush) = 0;
        virtual void SetForegroundColor(const HomeViewElement& element,
            GuiColor color) = 0;

        // HomeView's existing launcher controls. These are mechanical wrappers
        // around the separately migrated control pairs; HomeView owns all policy.
        [[nodiscard]] virtual HomeViewElement CreateSplashView() = 0;
        virtual void SplashShowRoom(const HomeViewElement& splash,
            std::optional<std::string> roomKey) = 0;
        virtual void SplashBottomInset(const HomeViewElement& splash, double value) = 0;

        [[nodiscard]] virtual HomeViewElement CreateUpdateBadge() = 0;
        virtual void AddUpdateBadgeClick(const HomeViewElement& badge, Action action) = 0;
        virtual void UpdateBadgeShow(const HomeViewElement& badge,
            std::optional<std::string> subtitle) = 0;
        virtual void UpdateBadgeSay(const HomeViewElement& badge,
            std::optional<std::string> subtitle) = 0;
        [[nodiscard]] virtual double UpdateBadgeDesiredHeight(
            const HomeViewElement& badge) const = 0;

        [[nodiscard]] virtual HomeViewElement CreateMenuEntry(
            std::optional<std::string> title,
            std::optional<std::string> subtitle = std::string{},
            double titleSize = 21.0) = 0;
        virtual void AddMenuEntryClick(const HomeViewElement& entry, Action action) = 0;
        virtual void MenuEntryTitle(const HomeViewElement& entry,
            std::optional<std::string> title) = 0;
        virtual void MenuEntrySubtitle(const HomeViewElement& entry,
            std::optional<std::string> subtitle) = 0;
        virtual void MenuEntryAccent(const HomeViewElement& entry, GuiColor color) = 0;
        virtual void MenuEntrySubtitleColor(const HomeViewElement& entry, GuiColor color) = 0;
        virtual void MenuEntryPrimary(const HomeViewElement& entry, bool value) = 0;
        virtual void MenuEntryEnabled(const HomeViewElement& entry, bool value) = 0;

        [[nodiscard]] virtual HomeViewElement CreateProgressRow() = 0;
        virtual void ProgressRowSet(const HomeViewElement& row,
            double fraction, std::string stage) = 0;

        [[nodiscard]] virtual HomeViewElement CreateCaption(std::string text) = 0;
        [[nodiscard]] virtual HomeViewElement CreateNote(std::string text,
            std::optional<GuiColor> color = std::nullopt) = 0;
        [[nodiscard]] virtual std::optional<std::string> NoteText(
            const HomeViewElement& note) const = 0;
        virtual void NoteText(const HomeViewElement& note,
            std::optional<std::string> text) = 0;
        virtual void NoteForeground(const HomeViewElement& note,
            const GuiBrush& brush) = 0;
        virtual void NoteForegroundColor(const HomeViewElement& note,
            GuiColor color) = 0;

        [[nodiscard]] virtual HomeViewElement CreateChoiceRow(std::string label,
            const std::vector<std::string>& items, std::int32_t index = 0) = 0;
        [[nodiscard]] virtual std::int32_t ChoiceIndex(
            const HomeViewElement& row) const = 0;
        virtual void ChoiceIndex(const HomeViewElement& row, std::int32_t index) = 0;
        [[nodiscard]] virtual std::string ChoiceValue(
            const HomeViewElement& row) const = 0;
        virtual void ChoiceSetItems(const HomeViewElement& row,
            const std::vector<std::string>& items, std::int32_t index = 0) = 0;
        virtual void AddChoiceChanged(const HomeViewElement& row, Action action) = 0;

        [[nodiscard]] virtual HomeViewElement CreateToggleRow(
            std::string label, bool on) = 0;
        [[nodiscard]] virtual bool ToggleOn(const HomeViewElement& row) const = 0;
        virtual void ToggleOn(const HomeViewElement& row, bool value) = 0;
        virtual void AddToggleChanged(const HomeViewElement& row, Action action) = 0;

        [[nodiscard]] virtual HomeViewElement CreateFieldRow(std::string label,
            std::string value, double boxWidth = 150.0) = 0;
        [[nodiscard]] virtual std::string FieldValue(
            const HomeViewElement& row) const = 0;
        virtual void FieldValue(const HomeViewElement& row, std::string value) = 0;
        virtual void AddFieldLostFocus(const HomeViewElement& row, Action action) = 0;

        [[nodiscard]] virtual HomeViewElement CreateServerHeader() = 0;
        [[nodiscard]] virtual HomeViewElement CreateServerRow(
            std::string name, std::string endpoint) = 0;
        virtual void AddServerRowClicked(const HomeViewElement& row, Action action) = 0;
        virtual void ServerRowSetStatus(const HomeViewElement& row,
            const HomeViewServerStatus& status) = 0;

        // Views whose native owners are this or later order items.
        [[nodiscard]] virtual HomeViewElement CreateSettingsView(
            const std::shared_ptr<MphRead::MenuSettings>& settings,
            bool inGame = false) = 0;
        virtual void AddSettingsClosed(const HomeViewElement& view, Action action) = 0;
        virtual void AddSettingsGameFilesRequested(
            const HomeViewElement& view, Action action) = 0;

        [[nodiscard]] virtual HomeViewElement CreateMapPickerView(
            const std::vector<std::string>& rooms, std::string current) = 0;
        virtual void AddMapPickerClosed(const HomeViewElement& view, Action action) = 0;
        [[nodiscard]] virtual std::optional<std::string> MapPickerRoomKey(
            const HomeViewElement& view) const = 0;

        [[nodiscard]] virtual HomeViewElement CreateDemoPickerView(
            std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>> demos,
            std::string directory) = 0;
        virtual void AddDemoPickerClosed(const HomeViewElement& view, Action action) = 0;
        [[nodiscard]] virtual std::optional<std::string> DemoPickerPath(
            const HomeViewElement& view) const = 0;
        [[nodiscard]] virtual bool DemoPickerImportRequested(
            const HomeViewElement& view) const = 0;

        [[nodiscard]] virtual HomeViewElement CreatePauseMenuView(bool offerWindowMode) = 0;
        virtual void AddPauseResumed(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseSettingsRequested(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseLeaveRequested(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseQuitRequested(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseSpectateRequested(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseRejoinRequested(const HomeViewElement& view, Action action) = 0;
        virtual void AddPauseRecordToggleRequested(const HomeViewElement& view, Action action) = 0;
        virtual void PauseFocusResume(const HomeViewElement& view) = 0;

        // Platform storage / .NET string-number-path mechanics.
        [[nodiscard]] virtual bool HasTopLevel() const = 0;
        [[nodiscard]] virtual bool IsAndroid() const = 0;
        [[nodiscard]] virtual HomeViewTaskRef OpenFilePickerAsync(
            HomeViewFilePickerOptions options,
            std::shared_ptr<std::vector<HomeViewStorageFile>> result) = 0;
        [[nodiscard]] virtual std::optional<std::string> TryGetLocalPath(
            const HomeViewStorageFile& file) const = 0;
        [[nodiscard]] virtual HomeViewTaskRef CopyStorageFileToPathAsync(
            const HomeViewStorageFile& file, std::string path) = 0;
        [[nodiscard]] virtual HomeViewTaskRef TryGetFolderFromPathAsync(
            std::string path, std::shared_ptr<std::optional<HomeViewStorageFolder>> result) = 0;
        [[nodiscard]] virtual bool DirectoryExists(std::string path) const = 0;
        virtual void DeleteFile(std::string path) = 0;
        [[nodiscard]] virtual std::string CombinePath(
            std::string left, std::string right) const = 0;
        [[nodiscard]] virtual std::string Trim(std::string value) const = 0;
        [[nodiscard]] virtual bool TryParseInt32InvariantInteger(
            std::string_view text, std::int32_t& value) const = 0;
        [[nodiscard]] virtual std::string FormatCurrentInt32(std::int32_t value) const = 0;
        virtual void ConsoleWriteLine(std::string line) = 0;
        [[nodiscard]] virtual std::string ExceptionMessage(std::exception_ptr error) const = 0;
        [[nodiscard]] virtual std::string FromUtf16(std::u16string_view text) const = 0;
        [[nodiscard]] virtual std::u16string ToUtf16(std::string_view text) const = 0;

        // The existing native networking headers carry an incompatible
        // GameMode forward declaration. These methods are a compile/ABI seam
        // only; their behavior is exactly the named C# call.
        [[nodiscard]] virtual HomeViewServerStatus NetStatusQuery(
            const std::string& host, std::int32_t port, bool allowJoinProbe) = 0;
        [[nodiscard]] virtual std::string NetStatusModeName(GameMode mode) = 0;
        [[nodiscard]] virtual bool NetJoin(const std::string& host, std::int32_t port,
            const std::string& name, Hunter hunter) = 0;
        [[nodiscard]] virtual std::string NetLastJoinError() = 0;
        virtual void NetSessionStop() = 0;
        [[nodiscard]] virtual HomeViewMasterListResult NetMasterQuery(
            const std::string& host, std::int32_t port) = 0;
        [[nodiscard]] virtual HomeViewHostedGame NetMasterRequestGame(
            const std::string& masterHost, std::int32_t masterPort,
            const std::string& roomKey, GameMode mode, float timeLimit,
            std::int32_t pointGoal, std::int32_t maxPlayers,
            const std::string& serverName) = 0;
        [[nodiscard]] virtual bool NetHostStartAndJoin(
            std::int32_t port, const std::string& name, Hunter hunter,
            const std::string& roomKey, GameMode mode, float timeLimit,
            std::int32_t pointGoal,
            std::optional<std::tuple<std::string, std::int32_t, std::string>> listing) = 0;
        [[nodiscard]] virtual std::optional<std::string> NetHostLastError() = 0;
        virtual void NetHostStop() = 0;

        // Mechanical adapter for ThumbnailHost's native-opaque task.
        [[nodiscard]] virtual HomeViewTaskRef RenderMissingPreviewsAsync(
            std::function<void(std::string)> report) = 0;

        // DispatcherTimer equivalent.
        [[nodiscard]] virtual HomeViewElement CreateDispatcherTimer() = 0;
        virtual void TimerIntervalSeconds(const HomeViewElement& timer, double seconds) = 0;
        virtual void AddTimerTick(const HomeViewElement& timer, Action action) = 0;
        virtual void StartTimer(const HomeViewElement& timer) = 0;
        virtual void StopTimer(const HomeViewElement& timer) = 0;
    };

    class HomeViewEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender, LaunchPlan plan);

        HomeViewEventHandler() = default;
        HomeViewEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static HomeViewEventHandler Combine(
            const HomeViewEventHandler& left, const HomeViewEventHandler& right);
        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(const HomeViewEventHandler& left,
            const HomeViewEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            std::shared_ptr<void> Target;
            Callback Function = nullptr;

            friend bool operator==(const Invocation& left,
                const Invocation& right) noexcept
            {
                return left.Target.get() == right.Target.get()
                    && left.Function == right.Function;
            }
        };

        explicit HomeViewEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
        friend class HomeViewEvent;
    };

    class HomeViewEvent final
    {
    public:
        void Add(const HomeViewEventHandler& handler);
        void Remove(const HomeViewEventHandler& handler);
        void Invoke(void* sender, LaunchPlan plan) const;

    private:
        using Invocation = HomeViewEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    class HomeView final
    {
    public:
        HomeView(HomeViewAdapter& adapter,
            std::shared_ptr<MphRead::MenuSettings> settings,
            HomeViewRoomListRef rooms);

        HomeView(const HomeView&) = delete;
        HomeView& operator=(const HomeView&) = delete;
        HomeView(HomeView&&) = delete;
        HomeView& operator=(HomeView&&) = delete;

        [[nodiscard]] LaunchPlan Plan() const;
        void AddDone(const HomeViewEventHandler& handler);
        void RemoveDone(const HomeViewEventHandler& handler);

        [[nodiscard]] bool GoBack();
        void OnKeyDown(HomeViewKeyEventArgs& e);
        void Reset();
        void ShowPauseMenu(std::function<void()> onResume,
            std::function<void()> onLeave,
            std::function<void()> onQuit);

    private:
        struct ModeEntry final
        {
            const char* Label;
            GameMode Mode;
        };

        struct Cancellation final
        {
            std::atomic<bool> IsCancellationRequested{false};
            void Cancel() noexcept { IsCancellationRequested.store(true); }
        };

        static constexpr double NarrowWidth = 720.0;
        static constexpr double BrowseWidth = 600.0;
        static constexpr std::int32_t PlayerSlotCapacity = 8;

        static const std::vector<ModeEntry> Modes;
        static const std::vector<std::string> Hunters;

        [[nodiscard]] HomeViewTaskRef CatchUpPreviews();
        void ApplyLayout(bool narrow);
        [[nodiscard]] double PanelWidth() const;
        void Finish(LaunchPlan plan);

        [[nodiscard]] HomeViewTaskRef ShowOverlay(const HomeViewElement& view,
            std::function<void(HomeViewAdapter::Action)> subscribeClosed);
        void CloseOverlay();
        void ShowCard(const HomeViewElement& card);
        [[nodiscard]] HomeViewElement Card();
        [[nodiscard]] HomeViewElement Back(HomeViewAdapter::Action go);

        [[nodiscard]] HomeViewElement BuildHomeCard();
        [[nodiscard]] HomeViewElement BuildVersionLine();
        [[nodiscard]] HomeViewElement BuildDebugSwitch();
        [[nodiscard]] HomeViewTaskRef ShareLogs();
        void RefreshShareButton();
        void ToggleDebugLogs();
        void RefreshDebugSwitch();
        [[nodiscard]] static std::string VersionNumber();
        void SayVersion(std::string text, GuiColor colour, bool pressable = false);
        void RefreshVersionLine();
        void ShowUpdate(const Update::UpdateInfo& update);
        void UpdateNow();
        [[nodiscard]] HomeViewTaskRef FetchAndInstall(Update::UpdateInfo update,
            std::shared_ptr<Update::IUpdateInstaller> installer);
        void RefreshGameFilesState();

        [[nodiscard]] HomeViewElement BuildSetupCard();
        [[nodiscard]] HomeViewTaskRef ChooseRom(const HomeViewElement& button,
            const HomeViewElement& log);
        [[nodiscard]] HomeViewTaskRef ChooseDemo();
        [[nodiscard]] HomeViewTaskRef ImportDemo();
        [[nodiscard]] HomeViewTaskRef PlayDemo(std::string path);
        [[nodiscard]] HomeViewTaskRef RenderPreviews(const HomeViewElement& log,
            std::shared_ptr<MphRead::Mods::Launcher::SetupProgress> progress = {});
        void RefreshPreviewEntry();
        [[nodiscard]] static std::string Tail(
            std::optional<std::string> existing, std::string line);

        [[nodiscard]] HomeViewElement BuildOnlineCard();
        [[nodiscard]] std::pair<std::string, std::int32_t> OnlineEndpoint();
        void StartStatusPolling();
        void StopStatusPolling();
        void QueryStatusSoon();
        [[nodiscard]] std::string Describe(const HomeViewServerStatus& status) const;
        [[nodiscard]] HomeViewTaskRef Connect();

        [[nodiscard]] HomeViewElement BuildBattleGroup();
        [[nodiscard]] HomeViewElement BuildAdventureGroup();
        [[nodiscard]] HomeViewElement BuildHostCard();
        void OpenHost();
        void RefreshHostCard();
        void RefreshAdventureCard();
        [[nodiscard]] std::uint8_t CurrentSlot() const;
        void StartAdventure(bool newGame);
        void RefreshPrefRows();
        [[nodiscard]] HomeViewTaskRef BrowseMaps();
        [[nodiscard]] std::string PlayerName() const;
        void OpenJoin();
        void RefreshMatchCard();
        [[nodiscard]] HomeViewTaskRef StartMatch();

        [[nodiscard]] HomeViewElement BuildBrowseCard();
        void ReloadServers();
        void AddServerRow(HomeViewMasterListing listing);
        [[nodiscard]] HomeViewTaskRef OpenSettings();

        void RefreshSplash();
        void RefreshRooms();
        [[nodiscard]] bool ParseEndpoint(
            std::string text, std::string& host, std::int32_t& port) const;

        [[nodiscard]] static std::string HunterName(Hunter hunter);
        [[nodiscard]] static Hunter ParseHunter(std::string_view text);
        [[nodiscard]] static const std::string& Require(
            const std::optional<std::string>& value);

        HomeViewAdapter& _adapter;
        std::shared_ptr<MphRead::MenuSettings> _settings{};
        std::vector<std::string> _playable{};
        HomeViewElement _splash;
        HomeViewElement _cards;
        HomeViewElement _layout;
        HomeViewElement _overlay;
        HomeViewElement _panel;

        HomeViewElement _setupProgress;
        HomeViewElement _setupBack;
        HomeViewElement _previewEntry;
        HomeViewElement _previewProgress;
        HomeViewElement _homeCard;
        HomeViewElement _setupCard;
        HomeViewElement _onlineCard;
        HomeViewElement _hostCard;
        HomeViewElement _hostAdventure;
        HomeViewElement _hostBattle;
        HomeViewElement _hostMode;
        HomeViewElement _hostWhere;
        HomeViewElement _hostCoop;
        HomeViewElement _browseCard;
        HomeViewElement _current;
        HomeViewElement _browseReturn;

        HomeViewElement _statusTimer;
        std::shared_ptr<Cancellation> _statusCancel{};
        bool _finished = false;

        LaunchPlan _plan{};
        HomeViewEvent _done;

        bool _narrow = false;
        bool _laidOut = false;
        HomeViewTaskRef _overlayDone{};

        HomeViewElement _updateBadge;
        HomeViewElement _onlineEntry;
        HomeViewElement _hostEntry;
        HomeViewElement _demoEntry;
        HomeViewElement _adventureSlot;
        HomeViewElement _adventureHunter;
        HomeViewElement _adventureNote;
        HomeViewElement _adventureStart;
        HomeViewElement _adventureNew;

        HomeViewElement _versionLine;
        HomeViewElement _versionBox;
        HomeViewElement _debugLine;
        HomeViewElement _debugBox;
        HomeViewElement _shareLine;
        HomeViewElement _shareBox;
        HomeViewElement _debugRow;
        bool _sharing = false;
        bool _updatable = false;
        bool _updating = false;

        HomeViewElement _onlineHunter;
        HomeViewElement _onlineAddress;
        HomeViewElement _onlineStatus;
        HomeViewElement _connect;

        LaunchKind _matchKind = LaunchKind::Offline;
        HomeViewElement _matchMap;
        HomeViewElement _matchMode;
        HomeViewElement _matchHunter;
        HomeViewElement _matchBots;
        HomeViewElement _matchSkill;
        HomeViewElement _matchPort;
        HomeViewElement _matchOnMaster;
        HomeViewElement _matchListed;
        HomeViewElement _matchStart;
        HomeViewElement _matchNote;

        HomeViewElement _browseList;
        HomeViewElement _browseNote;
    };
}

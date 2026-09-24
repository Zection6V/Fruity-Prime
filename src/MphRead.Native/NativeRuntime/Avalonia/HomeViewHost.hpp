#pragma once

// The Avalonia UserControl the launcher's home screen lives in: the control
// tree, the dispatcher, the task model, the file picker and the network seams
// HomeView was written against, on NativeRuntime/Gui's toolkit.
//
// HomeView owns every decision about what the screen contains; nothing here
// does anything but carry those decisions to the toolkit.

#include "HostRows.hpp"
#include "HostTask.hpp"

#include "../../Mods/Launcher/Gui/HomeView.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::NativeRuntime::Avalonia
{
    class HomeViewHost final : public Launcher::HomeViewAdapter
    {
    public:
        // The element the window shows, with the host and the view it carries.
        [[nodiscard]] static Toolkit::ElementPtr Create(
            std::shared_ptr<MphRead::MenuSettings> settings,
            const std::vector<std::string>& rooms);

        [[nodiscard]] Launcher::HomeView& View() const noexcept { return *_view; }
        [[nodiscard]] Toolkit::Element& Visual() const noexcept { return *_visual; }

        void SetBackground(const Launcher::GuiBrush& brush) override;
        void SetContent(const Launcher::HomeViewElement& content) override;
        void AddSizeChanged(SizeChangedAction action) override;
        void BaseOnKeyDown(Launcher::HomeViewKeyEventArgs& e) override;
        void Post(Action action,
            Launcher::HomeViewDispatcherPriority priority = Launcher::HomeViewDispatcherPriority::Normal) override;
        [[nodiscard]] Launcher::HomeViewTaskRef CompletedTask() override;
        [[nodiscard]] Launcher::HomeViewTaskRef FaultedTask(std::exception_ptr error) override;
        [[nodiscard]] Launcher::HomeViewTaskRef PendingTask() override;
        bool TrySetTaskResult(const Launcher::HomeViewTaskRef& task) override;
        [[nodiscard]] Launcher::HomeViewTaskRef RunBackground(Action action) override;
        [[nodiscard]] Launcher::HomeViewTaskRef ContinueWithTask(const Launcher::HomeViewTaskRef& task,
            std::function<Launcher::HomeViewTaskRef()> continuation) override;
        [[nodiscard]] Launcher::HomeViewTaskRef ContinueWithAction(const Launcher::HomeViewTaskRef& task,
            Action continuation) override;
        [[nodiscard]] Launcher::HomeViewTaskRef CatchTask(const Launcher::HomeViewTaskRef& task,
            std::function<Launcher::HomeViewTaskRef(std::exception_ptr)> handler) override;
        void StartAsyncVoid(std::function<Launcher::HomeViewTaskRef()> body) override;
        void Forget(const Launcher::HomeViewTaskRef& task) override;
        [[nodiscard]] Launcher::HomeViewElement CreatePanel() override;
        [[nodiscard]] Launcher::HomeViewElement CreateGrid() override;
        [[nodiscard]] Launcher::HomeViewElement CreateScrollViewer() override;
        [[nodiscard]] Launcher::HomeViewElement CreateBorder() override;
        [[nodiscard]] Launcher::HomeViewElement CreateStackPanel() override;
        [[nodiscard]] Launcher::HomeViewElement CreateTextBlock() override;
        void AddChild(const Launcher::HomeViewElement& parent,
            const Launcher::HomeViewElement& child) override;
        void InsertChild(const Launcher::HomeViewElement& parent, std::int32_t index,
            const Launcher::HomeViewElement& child) override;
        void ClearChildren(const Launcher::HomeViewElement& parent) override;
        void SetGridRowDefinitions(const Launcher::HomeViewElement& grid,
            std::string_view definitions) override;
        void SetGridColumnDefinitions(const Launcher::HomeViewElement& grid,
            std::string_view definitions) override;
        void SetGridRow(const Launcher::HomeViewElement& child,
            std::int32_t row) override;
        void SetGridColumn(const Launcher::HomeViewElement& child,
            std::int32_t column) override;
        void SetScrollContent(const Launcher::HomeViewElement& scroll,
            const Launcher::HomeViewElement& content) override;
        void DisableHorizontalScrollBar(const Launcher::HomeViewElement& scroll) override;
        void SetPanelBackground(const Launcher::HomeViewElement& panel,
            const Launcher::GuiBrush& brush) override;
        void SetBorderBackground(const Launcher::HomeViewElement& border,
            const Launcher::GuiBrush& brush) override;
        void SetBorderBrush(const Launcher::HomeViewElement& border,
            const Launcher::GuiBrush& brush) override;
        void SetBorderThickness(const Launcher::HomeViewElement& border,
            Launcher::HomeViewThickness thickness) override;
        void SetBorderCornerRadius(const Launcher::HomeViewElement& border,
            double radius) override;
        void SetBorderPadding(const Launcher::HomeViewElement& border,
            Launcher::HomeViewThickness padding) override;
        void SetBorderChild(const Launcher::HomeViewElement& border,
            const Launcher::HomeViewElement& child) override;
        void SetMargin(const Launcher::HomeViewElement& element,
            Launcher::HomeViewThickness margin) override;
        void SetWidth(const Launcher::HomeViewElement& element, double width) override;
        void SetWidthAuto(const Launcher::HomeViewElement& element) override;
        void SetHeight(const Launcher::HomeViewElement& element,
            double height) override;
        void SetHeightAuto(const Launcher::HomeViewElement& element) override;
        void SetIsVisible(const Launcher::HomeViewElement& element,
            bool visible) override;
        [[nodiscard]] bool GetIsVisible(const Launcher::HomeViewElement& element) const override;
        void SetHorizontalAlignment(const Launcher::HomeViewElement& element,
            Launcher::HomeViewHorizontalAlignment alignment) override;
        void SetVerticalAlignment(const Launcher::HomeViewElement& element,
            Launcher::HomeViewVerticalAlignment alignment) override;
        void SetCursor(const Launcher::HomeViewElement& element,
            Launcher::HomeViewCursor cursor) override;
        void SetStackSpacing(const Launcher::HomeViewElement& stack,
            double spacing) override;
        void SetStackOrientation(const Launcher::HomeViewElement& stack,
            Launcher::HomeViewOrientation orientation) override;
        void Focus(const Launcher::HomeViewElement& element) override;
        void FocusFirstFocusableDescendant(const Launcher::HomeViewElement& element) override;
        void AddPointerPressed(const Launcher::HomeViewElement& element,
            PointerAction action) override;
        void SetToolTip(const Launcher::HomeViewElement& element,
            std::string text) override;
        void SetText(const Launcher::HomeViewElement& textBlock,
            std::optional<std::string> text) override;
        void SetFontSize(const Launcher::HomeViewElement& textBlock,
            double size) override;
        void SetForeground(const Launcher::HomeViewElement& element,
            const Launcher::GuiBrush& brush) override;
        void SetForegroundColor(const Launcher::HomeViewElement& element,
            Launcher::GuiColor color) override;
        [[nodiscard]] Launcher::HomeViewElement CreateSplashView() override;
        void SplashShowRoom(const Launcher::HomeViewElement& splash,
            std::optional<std::string> roomKey) override;
        void SplashBottomInset(const Launcher::HomeViewElement& splash,
            double value) override;
        [[nodiscard]] Launcher::HomeViewElement CreateUpdateBadge() override;
        void AddUpdateBadgeClick(const Launcher::HomeViewElement& badge,
            Action action) override;
        void UpdateBadgeShow(const Launcher::HomeViewElement& badge,
            std::optional<std::string> subtitle) override;
        void UpdateBadgeSay(const Launcher::HomeViewElement& badge,
            std::optional<std::string> subtitle) override;
        [[nodiscard]] double UpdateBadgeDesiredHeight(const Launcher::HomeViewElement& badge) const override;
        [[nodiscard]] Launcher::HomeViewElement CreateMenuEntry(std::optional<std::string> title,
            std::optional<std::string> subtitle = std::string{},
            double titleSize = 21.0) override;
        void AddMenuEntryClick(const Launcher::HomeViewElement& entry,
            Action action) override;
        void MenuEntryTitle(const Launcher::HomeViewElement& entry,
            std::optional<std::string> title) override;
        void MenuEntrySubtitle(const Launcher::HomeViewElement& entry,
            std::optional<std::string> subtitle) override;
        void MenuEntryAccent(const Launcher::HomeViewElement& entry,
            Launcher::GuiColor color) override;
        void MenuEntrySubtitleColor(const Launcher::HomeViewElement& entry,
            Launcher::GuiColor color) override;
        void MenuEntryPrimary(const Launcher::HomeViewElement& entry,
            bool value) override;
        void MenuEntryEnabled(const Launcher::HomeViewElement& entry,
            bool value) override;
        [[nodiscard]] Launcher::HomeViewElement CreateProgressRow() override;
        void ProgressRowSet(const Launcher::HomeViewElement& row, double fraction,
            std::string stage) override;
        [[nodiscard]] Launcher::HomeViewElement CreateCaption(std::string text) override;
        [[nodiscard]] Launcher::HomeViewElement CreateNote(std::string text,
            std::optional<Launcher::GuiColor> color = std::nullopt) override;
        [[nodiscard]] std::optional<std::string> NoteText(const Launcher::HomeViewElement& note) const override;
        void NoteText(const Launcher::HomeViewElement& note,
            std::optional<std::string> text) override;
        void NoteForeground(const Launcher::HomeViewElement& note,
            const Launcher::GuiBrush& brush) override;
        void NoteForegroundColor(const Launcher::HomeViewElement& note,
            Launcher::GuiColor color) override;
        [[nodiscard]] Launcher::HomeViewElement CreateChoiceRow(std::string label,
            const std::vector<std::string>& items, std::int32_t index = 0) override;
        [[nodiscard]] std::int32_t ChoiceIndex(const Launcher::HomeViewElement& row) const override;
        void ChoiceIndex(const Launcher::HomeViewElement& row,
            std::int32_t index) override;
        [[nodiscard]] std::string ChoiceValue(const Launcher::HomeViewElement& row) const override;
        void ChoiceSetItems(const Launcher::HomeViewElement& row,
            const std::vector<std::string>& items, std::int32_t index = 0) override;
        void AddChoiceChanged(const Launcher::HomeViewElement& row,
            Action action) override;
        [[nodiscard]] Launcher::HomeViewElement CreateToggleRow(std::string label,
            bool on) override;
        [[nodiscard]] bool ToggleOn(const Launcher::HomeViewElement& row) const override;
        void ToggleOn(const Launcher::HomeViewElement& row, bool value) override;
        void AddToggleChanged(const Launcher::HomeViewElement& row,
            Action action) override;
        [[nodiscard]] Launcher::HomeViewElement CreateFieldRow(std::string label,
            std::string value, double boxWidth = 150.0) override;
        [[nodiscard]] std::string FieldValue(const Launcher::HomeViewElement& row) const override;
        void FieldValue(const Launcher::HomeViewElement& row,
            std::string value) override;
        void AddFieldLostFocus(const Launcher::HomeViewElement& row,
            Action action) override;
        [[nodiscard]] Launcher::HomeViewElement CreateServerHeader() override;
        [[nodiscard]] Launcher::HomeViewElement CreateServerRow(std::string name,
            std::string endpoint) override;
        void AddServerRowClicked(const Launcher::HomeViewElement& row,
            Action action) override;
        void ServerRowSetStatus(const Launcher::HomeViewElement& row,
            const Launcher::HomeViewServerStatus& status) override;
        [[nodiscard]] Launcher::HomeViewElement CreateSettingsView(const std::shared_ptr<MphRead::MenuSettings>& settings,
            bool inGame = false) override;
        void AddSettingsClosed(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddSettingsGameFilesRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        [[nodiscard]] Launcher::HomeViewElement CreateMapPickerView(const std::vector<std::string>& rooms,
            std::string current) override;
        void AddMapPickerClosed(const Launcher::HomeViewElement& view,
            Action action) override;
        [[nodiscard]] std::optional<std::string> MapPickerRoomKey(const Launcher::HomeViewElement& view) const override;
        [[nodiscard]] Launcher::HomeViewElement CreateDemoPickerView(std::shared_ptr<const std::vector<MphRead::Mods::Network::DemoRecording>> demos,
            std::string directory) override;
        void AddDemoPickerClosed(const Launcher::HomeViewElement& view,
            Action action) override;
        [[nodiscard]] std::optional<std::string> DemoPickerPath(const Launcher::HomeViewElement& view) const override;
        [[nodiscard]] bool DemoPickerImportRequested(const Launcher::HomeViewElement& view) const override;
        [[nodiscard]] Launcher::HomeViewElement CreatePauseMenuView(bool offerWindowMode) override;
        void AddPauseResumed(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseSettingsRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseLeaveRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseQuitRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseSpectateRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseRejoinRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void AddPauseRecordToggleRequested(const Launcher::HomeViewElement& view,
            Action action) override;
        void PauseFocusResume(const Launcher::HomeViewElement& view) override;
        [[nodiscard]] bool HasTopLevel() const override;
        [[nodiscard]] bool IsAndroid() const override;
        [[nodiscard]] Launcher::HomeViewTaskRef OpenFilePickerAsync(Launcher::HomeViewFilePickerOptions options,
            std::shared_ptr<std::vector<Launcher::HomeViewStorageFile>> result) override;
        [[nodiscard]] std::optional<std::string> TryGetLocalPath(const Launcher::HomeViewStorageFile& file) const override;
        [[nodiscard]] Launcher::HomeViewTaskRef CopyStorageFileToPathAsync(const Launcher::HomeViewStorageFile& file,
            std::string path) override;
        [[nodiscard]] Launcher::HomeViewTaskRef TryGetFolderFromPathAsync(std::string path,
            std::shared_ptr<std::optional<Launcher::HomeViewStorageFolder>> result) override;
        [[nodiscard]] bool DirectoryExists(std::string path) const override;
        void DeleteFile(std::string path) override;
        [[nodiscard]] std::string CombinePath(std::string left,
            std::string right) const override;
        [[nodiscard]] std::string Trim(std::string value) const override;
        [[nodiscard]] bool TryParseInt32InvariantInteger(std::string_view text,
            std::int32_t& value) const override;
        [[nodiscard]] std::string FormatCurrentInt32(std::int32_t value) const override;
        void ConsoleWriteLine(std::string line) override;
        [[nodiscard]] std::string ExceptionMessage(std::exception_ptr error) const override;
        [[nodiscard]] std::string FromUtf16(std::u16string_view text) const override;
        [[nodiscard]] std::u16string ToUtf16(std::string_view text) const override;
        [[nodiscard]] Launcher::HomeViewServerStatus NetStatusQuery(const std::string& host,
            std::int32_t port, bool allowJoinProbe) override;
        [[nodiscard]] std::string NetStatusModeName(GameMode mode) override;
        [[nodiscard]] bool NetJoin(const std::string& host, std::int32_t port,
            const std::string& name, Hunter hunter) override;
        [[nodiscard]] std::string NetLastJoinError() override;
        void NetSessionStop() override;
        [[nodiscard]] Launcher::HomeViewMasterListResult NetMasterQuery(const std::string& host,
            std::int32_t port) override;
        [[nodiscard]] Launcher::HomeViewHostedGame NetMasterRequestGame(const std::string& masterHost,
            std::int32_t masterPort, const std::string& roomKey, GameMode mode,
            float timeLimit, std::int32_t pointGoal, std::int32_t maxPlayers,
            const std::string& serverName) override;
        [[nodiscard]] bool NetHostStartAndJoin(std::int32_t port,
            const std::string& name, Hunter hunter, const std::string& roomKey,
            GameMode mode, float timeLimit, std::int32_t pointGoal,
            std::optional<std::tuple<std::string, std::int32_t, std::string>> listing) override;
        [[nodiscard]] std::optional<std::string> NetHostLastError() override;
        void NetHostStop() override;
        [[nodiscard]] Launcher::HomeViewTaskRef RenderMissingPreviewsAsync(std::function<void(std::string)> report) override;
        [[nodiscard]] Launcher::HomeViewElement CreateDispatcherTimer() override;
        void TimerIntervalSeconds(const Launcher::HomeViewElement& timer,
            double seconds) override;
        void AddTimerTick(const Launcher::HomeViewElement& timer,
            Action action) override;
        void StartTimer(const Launcher::HomeViewElement& timer) override;
        void StopTimer(const Launcher::HomeViewElement& timer) override;

    private:
        [[nodiscard]] Toolkit::ElementPtr Content() const;

        Toolkit::Element* _visual = nullptr;
        std::unique_ptr<Launcher::HomeView> _view;
        std::vector<std::shared_ptr<void>> _kept;
    };
}

#pragma once

#include "GuiTheme.hpp"
#include "KeyRow.hpp"
#include "MenuEntry.hpp"
#include "PadRow.hpp"
#include "Rows.hpp"
#include "SliderRow.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::Mods
{
    struct InputBindingProperty;
}

namespace MphRead::Mods::Launcher::Gui
{
    class SettingsViewNullReferenceException final : public std::runtime_error
    {
    public:
        SettingsViewNullReferenceException();
    };

    enum class SettingsViewScrollBarVisibility : std::uint8_t
    {
        Disabled,
        Auto
    };

    enum class SettingsViewVerticalAlignment : std::uint8_t
    {
        Bottom
    };

    enum class SettingsViewDispatcherPriority : std::uint8_t
    {
        Background
    };

    enum class SettingsViewKey : std::uint8_t
    {
        Other,
        Escape
    };

    struct SettingsViewThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        friend constexpr bool operator==(
            const SettingsViewThickness&, const SettingsViewThickness&) noexcept = default;
    };

    struct SettingsViewVisualTreeAttachmentEventArgs final
    {
        void* Native = nullptr;
    };

    struct SettingsViewKeyEventArgs final
    {
        void* Native = nullptr;
        SettingsViewKey Key = SettingsViewKey::Other;
        bool Handled = false;
    };

    struct SettingsViewEventArgs final
    {
        static const SettingsViewEventArgs Empty;
    };

    class SettingsViewEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender,
            const SettingsViewEventArgs& args);

        SettingsViewEventHandler() = default;
        SettingsViewEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static SettingsViewEventHandler Combine(
            const SettingsViewEventHandler& left,
            const SettingsViewEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const SettingsViewEventHandler& left,
            const SettingsViewEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            std::shared_ptr<void> Target;
            Callback Function = nullptr;

            friend bool operator==(
                const Invocation& left, const Invocation& right) noexcept
            {
                return left.Target.get() == right.Target.get()
                    && left.Function == right.Function;
            }
        };

        explicit SettingsViewEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class SettingsViewEvent;
    };

    class SettingsViewEvent final
    {
    public:
        void Add(const SettingsViewEventHandler& handler);
        void Remove(const SettingsViewEventHandler& handler);
        void Invoke(void* sender, const SettingsViewEventArgs& args) const;

    private:
        using Invocation = SettingsViewEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    using SettingsViewControlHandle = std::shared_ptr<void>;
    using SettingsViewDefinitionsHandle = std::shared_ptr<void>;

    template <typename T>
    struct SettingsViewControlRef final
    {
        SettingsViewControlHandle Control;
        std::shared_ptr<T> Value;
    };

    struct SettingsViewAction final
    {
        using Callback = void (*)(void* target);

        void* Context = nullptr;
        Callback Function = nullptr;
        std::shared_ptr<void> KeepAlive{};

        SettingsViewAction() = default;
        SettingsViewAction(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {})
            : Context(context), Function(function), KeepAlive(std::move(keepAlive))
        {
        }

        void Invoke() const
        {
            if (Function != nullptr)
            {
                Function(Context);
            }
        }
    };

    struct SettingsViewSizeChangedHandler final
    {
        using Callback = void (*)(void* target, double newWidth);

        void* Context = nullptr;
        Callback Function = nullptr;
        std::shared_ptr<void> KeepAlive{};

        SettingsViewSizeChangedHandler() = default;
        SettingsViewSizeChangedHandler(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {})
            : Context(context), Function(function), KeepAlive(std::move(keepAlive))
        {
        }

        void Invoke(double newWidth) const
        {
            if (Function != nullptr)
            {
                Function(Context, newWidth);
            }
        }
    };

    class SettingsViewAdapter
    {
    public:
        using SliderFormat = std::function<std::optional<std::u16string>(std::int32_t)>;

        virtual ~SettingsViewAdapter() = default;

        [[nodiscard]] virtual bool IsAndroid() const = 0;
        [[nodiscard]] virtual bool OrdinalIgnoreCaseEquals(
            std::optional<std::u16string_view> left,
            std::optional<std::u16string_view> right) const = 0;

        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetFocusable(bool focusable) = 0;

        [[nodiscard]] virtual SettingsViewControlHandle ConstructStackPanel() = 0;
        [[nodiscard]] virtual SettingsViewControlHandle ConstructWrapPanel() = 0;
        [[nodiscard]] virtual SettingsViewControlHandle ConstructPanel() = 0;
        [[nodiscard]] virtual SettingsViewControlHandle ConstructGrid() = 0;
        [[nodiscard]] virtual SettingsViewControlHandle ConstructBorder() = 0;
        [[nodiscard]] virtual SettingsViewControlHandle ConstructScrollViewer() = 0;

        [[nodiscard]] virtual SettingsViewControlRef<Caption> ConstructCaption(
            std::optional<std::u16string> text) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<Note> ConstructNote(
            std::optional<std::u16string> text,
            std::optional<GuiColor> color = std::nullopt) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<MenuEntry> ConstructMenuEntry(
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle = std::u16string{},
            double titleSize = 21.0) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<ChoiceRow> ConstructChoiceRow(
            std::optional<std::u16string> label,
            RowsStringListRef options,
            std::int32_t index = 0) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<ToggleRow> ConstructToggleRow(
            std::optional<std::u16string> label, bool on) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<FieldRow> ConstructFieldRow(
            std::optional<std::u16string> label,
            std::optional<std::u16string> value,
            double boxWidth = 150.0) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<SliderRow> ConstructSliderRow(
            std::optional<std::u16string> label,
            std::int32_t value,
            SliderFormat format = {},
            double labelWidth = 120.0,
            std::int32_t min = 0,
            std::int32_t max = 100,
            std::int32_t keyStep = 5) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<KeyRow> ConstructKeyRow(
            const Mods::InputBindingProperty* property,
            double labelWidth = 160.0) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<KeyRow> ConstructKeyRow(
            std::optional<std::u16string> label,
            KeyRowGetHandler get,
            KeyRowSetHandler set,
            double labelWidth = 160.0) = 0;
        [[nodiscard]] virtual SettingsViewControlRef<PadRow> ConstructPadRow(
            Mods::Input::PadAction action,
            double labelWidth = 160.0) = 0;

        virtual void SetStackPanelSpacing(
            const SettingsViewControlHandle& panel, double spacing) = 0;
        virtual void AddPanelChild(const SettingsViewControlHandle& panel,
            const SettingsViewControlHandle& child) = 0;
        virtual void ClearPanelChildren(const SettingsViewControlHandle& panel) = 0;
        [[nodiscard]] virtual SettingsViewControlHandle GetParent(
            const SettingsViewControlHandle& control) const = 0;

        virtual void SetControlMargin(const SettingsViewControlHandle& control,
            SettingsViewThickness margin) = 0;
        virtual void SetControlWidth(
            const SettingsViewControlHandle& control, double width) = 0;
        virtual void SetControlIsVisible(
            const SettingsViewControlHandle& control, bool visible) = 0;
        virtual void SetControlVerticalAlignment(
            const SettingsViewControlHandle& control,
            SettingsViewVerticalAlignment alignment) = 0;
        virtual void Focus(const SettingsViewControlHandle& control) = 0;

        [[nodiscard]] virtual SettingsViewDefinitionsHandle ConstructColumnDefinitions(
            std::string_view definitions) = 0;
        [[nodiscard]] virtual SettingsViewDefinitionsHandle ConstructRowDefinitions(
            std::string_view definitions) = 0;
        virtual void SetGridColumnDefinitions(const SettingsViewControlHandle& grid,
            const SettingsViewDefinitionsHandle& definitions) = 0;
        virtual void SetGridRowDefinitions(const SettingsViewControlHandle& grid,
            const SettingsViewDefinitionsHandle& definitions) = 0;
        virtual void SetGridRow(
            const SettingsViewControlHandle& control, std::int32_t row) = 0;
        virtual void SetGridColumn(
            const SettingsViewControlHandle& control, std::int32_t column) = 0;
        virtual void SetGridRowSpan(
            const SettingsViewControlHandle& control, std::int32_t rowSpan) = 0;

        virtual void SetBorderBackground(const SettingsViewControlHandle& border,
            const GuiBrush& brush) = 0;
        virtual void SetBorderPadding(const SettingsViewControlHandle& border,
            SettingsViewThickness padding) = 0;
        virtual void SetBorderChild(const SettingsViewControlHandle& border,
            const SettingsViewControlHandle& child) = 0;

        virtual void SetScrollViewerContent(
            const SettingsViewControlHandle& viewer,
            const SettingsViewControlHandle& content) = 0;
        virtual void SetScrollViewerHorizontalScrollBarVisibility(
            const SettingsViewControlHandle& viewer,
            SettingsViewScrollBarVisibility visibility) = 0;
        virtual void SetScrollViewerVerticalScrollBarVisibility(
            const SettingsViewControlHandle& viewer,
            SettingsViewScrollBarVisibility visibility) = 0;

        virtual void AddMenuEntryClick(
            const SettingsViewControlHandle& entry,
            SettingsViewAction handler) = 0;
        virtual void AddSizeChanged(SettingsViewSizeChangedHandler handler) = 0;

        virtual void SetContent(const SettingsViewControlHandle& content) = 0;
        virtual void PostUiThread(
            SettingsViewAction action,
            SettingsViewDispatcherPriority priority) = 0;

        virtual void BaseOnAttachedToVisualTree(
            SettingsViewVisualTreeAttachmentEventArgs& e) = 0;
        virtual void BaseOnKeyDown(SettingsViewKeyEventArgs& e) = 0;
    };

    struct SettingsViewState;
    struct SettingsViewSectionClickTarget;
    struct SettingsViewSupportClickTarget;

    class SettingsView final
    {
    public:
        SettingsView(SettingsViewAdapter& adapter,
            std::shared_ptr<MenuSettings> settings,
            bool inGame = false);

        SettingsView(const SettingsView&) = delete;
        SettingsView& operator=(const SettingsView&) = delete;
        SettingsView(SettingsView&&) = delete;
        SettingsView& operator=(SettingsView&&) = delete;

        [[nodiscard]] bool Saved() const noexcept;
        [[nodiscard]] std::string WindowTitle() const;
        [[nodiscard]] bool InGame() const noexcept;

        void AddClosed(const SettingsViewEventHandler& handler);
        void RemoveClosed(const SettingsViewEventHandler& handler);
        void AddGameFilesRequested(const SettingsViewEventHandler& handler);
        void RemoveGameFilesRequested(const SettingsViewEventHandler& handler);
        void AddStylusPlacementRequested(const SettingsViewEventHandler& handler);
        void RemoveStylusPlacementRequested(const SettingsViewEventHandler& handler);

        void ShowSection(std::optional<std::u16string_view> name);

        void OnAttachedToVisualTree(SettingsViewVisualTreeAttachmentEventArgs& e);
        void OnKeyDown(SettingsViewKeyEventArgs& e);

    private:
        static constexpr double NarrowWidth = 720.0;
        static constexpr double RailWidth = 216.0;

        void ApplyLayout(bool narrow);
        void MoveSections(const SettingsViewControlHandle& target);
        void Place(const SettingsViewControlHandle& control,
            std::int32_t row, std::int32_t column, std::int32_t rowSpan);
        void Close();

        [[nodiscard]] SettingsViewControlHandle AddSection(std::u16string name);
        void ShowPage(const SettingsViewControlHandle& page);
        [[nodiscard]] SettingsViewControlRef<Caption> Heading(
            const SettingsViewControlHandle& page, std::u16string text);
        [[nodiscard]] SettingsViewControlRef<Note> Explain(
            const SettingsViewControlHandle& page, std::u16string text,
            std::optional<GuiColor> color = std::nullopt);

        void BuildPages();
        void BuildCredits();
        void BuildDisplay();
        void ShowCrosshairRows();
        void BuildAudio();
        void BuildControls();
        void BuildStylusZone(const SettingsViewControlHandle& page);
        void BuildTouchControls(const SettingsViewControlHandle& page);
        void BuildMatch();
        void BuildLauncher();
        [[nodiscard]] SettingsViewControlHandle BuildFooter();

        void TryCommit();
        void Commit();

        static void OnSizeChanged(void* target, double newWidth);
        static void OnSectionClick(void* target);
        static void OnSupportClick(void* target);
        static void OnResetClick(void* target);
        static void OnPlaceStylusClick(void* target);
        static void OnGameFilesClick(void* target);
        static void OnSaveClick(void* target);
        static void OnCancelClick(void* target);
        static void OnFocusPosted(void* target);
        static void OnProHudChanged(
            void* target, void* sender, const RowsEventArgs& args);
        static void OnCrosshairSizeChanged(
            void* target, void* sender, const RowsEventArgs& args);
        static void OnTouchButtonsChanged(
            void* target, void* sender, const RowsEventArgs& args);

        SettingsViewAdapter& _adapter;
        std::shared_ptr<SettingsViewState> _state;
    };
}

#pragma once

#include "GuiTheme.hpp"
#include "../../Network/DemoLibrary.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    class DemoPickerViewNullReferenceException final : public std::runtime_error
    {
    public:
        DemoPickerViewNullReferenceException();
    };

    enum class DemoPickerViewTextWrapping : std::uint8_t
    {
        Wrap
    };

    enum class DemoPickerViewHorizontalAlignment : std::uint8_t
    {
        Right
    };

    enum class DemoPickerViewVerticalAlignment : std::uint8_t
    {
        Center
    };

    enum class DemoPickerViewScrollBarVisibility : std::uint8_t
    {
        Disabled
    };

    enum class DemoPickerViewDock : std::uint8_t
    {
        Top
    };

    enum class DemoPickerViewDispatcherPriority : std::uint8_t
    {
        Background
    };

    enum class DemoPickerViewKey : std::uint8_t
    {
        Other,
        Escape
    };

    struct DemoPickerViewThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        [[nodiscard]] static constexpr DemoPickerViewThickness Uniform(double value) noexcept
        {
            return DemoPickerViewThickness{value, value, value, value};
        }

        friend constexpr bool operator==(
            const DemoPickerViewThickness&, const DemoPickerViewThickness&) noexcept = default;
    };

    struct DemoPickerViewVisualTreeAttachmentEventArgs final
    {
        void* Native = nullptr;
    };

    struct DemoPickerViewKeyEventArgs final
    {
        void* Native = nullptr;
        DemoPickerViewKey Key = DemoPickerViewKey::Other;
        bool Handled = false;
    };

    struct DemoPickerViewEventArgs final
    {
        static const DemoPickerViewEventArgs Empty;
    };

    class DemoPickerViewEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender,
            const DemoPickerViewEventArgs& args);

        DemoPickerViewEventHandler() = default;
        DemoPickerViewEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static DemoPickerViewEventHandler Combine(
            const DemoPickerViewEventHandler& left,
            const DemoPickerViewEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const DemoPickerViewEventHandler& left,
            const DemoPickerViewEventHandler& right) noexcept;

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

        explicit DemoPickerViewEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class DemoPickerViewEvent;
    };

    class DemoPickerViewEvent final
    {
    public:
        void Add(const DemoPickerViewEventHandler& handler);
        void Remove(const DemoPickerViewEventHandler& handler);

    private:
        friend class DemoPickerView;
        void Invoke(void* sender, const DemoPickerViewEventArgs& args) const;

        using Invocation = DemoPickerViewEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    struct DemoPickerViewAction final
    {
        using Callback = void (*)(void* target);

        std::shared_ptr<void> Target;
        Callback Function = nullptr;
    };

    class DemoPickerViewAdapter
    {
    public:
        using ControlHandle = std::shared_ptr<void>;
        using ColumnDefinitionsHandle = std::shared_ptr<void>;

        virtual ~DemoPickerViewAdapter() = default;

        virtual void SetBackground(const GuiBrush& brush) = 0;
        virtual void SetFocusable(bool focusable) = 0;

        [[nodiscard]] virtual ControlHandle ConstructStackPanel() = 0;
        virtual void SetStackPanelSpacing(const ControlHandle& panel, double spacing) = 0;

        [[nodiscard]] virtual ControlHandle ConstructMenuEntry(
            std::string title, std::string subtitle, double titleSize) = 0;
        virtual void SetMenuEntryAccent(const ControlHandle& entry, GuiColor accent) = 0;
        virtual void AddMenuEntryClick(
            const ControlHandle& entry, DemoPickerViewAction handler) = 0;
        virtual void Focus(const ControlHandle& control) = 0;

        [[nodiscard]] virtual ControlHandle ConstructTextBlock() = 0;
        virtual void SetTextBlockText(const ControlHandle& textBlock, std::string text) = 0;
        virtual void SetTextBlockForeground(
            const ControlHandle& textBlock, const GuiBrush& brush) = 0;
        virtual void SetTextBlockFontSize(const ControlHandle& textBlock, double fontSize) = 0;
        virtual void SetTextBlockTextWrapping(
            const ControlHandle& textBlock, DemoPickerViewTextWrapping wrapping) = 0;
        virtual void SetTextBlockFontFamily(
            const ControlHandle& textBlock, const GuiFontFamily& fontFamily) = 0;

        virtual void SetControlMargin(
            const ControlHandle& control, DemoPickerViewThickness margin) = 0;
        virtual void SetControlWidth(const ControlHandle& control, double width) = 0;
        virtual void SetControlHorizontalAlignment(const ControlHandle& control,
            DemoPickerViewHorizontalAlignment alignment) = 0;
        virtual void SetControlVerticalAlignment(const ControlHandle& control,
            DemoPickerViewVerticalAlignment alignment) = 0;

        [[nodiscard]] virtual ControlHandle ConstructGrid() = 0;
        [[nodiscard]] virtual ColumnDefinitionsHandle ConstructColumnDefinitions(
            std::string_view definitions) = 0;
        virtual void SetGridColumnDefinitions(const ControlHandle& grid,
            const ColumnDefinitionsHandle& definitions) = 0;
        virtual void SetGridColumn(const ControlHandle& control, std::int32_t column) = 0;

        [[nodiscard]] virtual ControlHandle ConstructBorder() = 0;
        virtual void SetBorderBackground(
            const ControlHandle& border, const GuiBrush& brush) = 0;
        virtual void SetBorderPadding(
            const ControlHandle& border, DemoPickerViewThickness padding) = 0;
        virtual void SetBorderChild(
            const ControlHandle& border, const ControlHandle& child) = 0;

        [[nodiscard]] virtual ControlHandle ConstructScrollViewer() = 0;
        virtual void SetScrollViewerContent(
            const ControlHandle& viewer, const ControlHandle& content) = 0;
        virtual void SetScrollViewerPadding(
            const ControlHandle& viewer, DemoPickerViewThickness padding) = 0;
        virtual void SetScrollViewerHorizontalScrollBarVisibility(
            const ControlHandle& viewer,
            DemoPickerViewScrollBarVisibility visibility) = 0;

        [[nodiscard]] virtual ControlHandle ConstructDockPanel() = 0;
        virtual void SetDockPanelLastChildFill(
            const ControlHandle& panel, bool lastChildFill) = 0;
        virtual void SetDock(const ControlHandle& control, DemoPickerViewDock dock) = 0;

        virtual void AddPanelChild(
            const ControlHandle& panel, const ControlHandle& child) = 0;
        virtual void SetContent(const ControlHandle& content) = 0;

        virtual void PostUiThread(
            DemoPickerViewAction action, DemoPickerViewDispatcherPriority priority) = 0;

        virtual void BaseOnAttachedToVisualTree(
            DemoPickerViewVisualTreeAttachmentEventArgs& e) = 0;
        virtual void BaseOnKeyDown(DemoPickerViewKeyEventArgs& e) = 0;
    };

    struct DemoPickerViewState;
    struct DemoPickerViewDemoClickTarget;

    class DemoPickerView final
    {
    public:
        DemoPickerView(DemoPickerViewAdapter& adapter,
            std::shared_ptr<const std::vector<Mods::Network::DemoRecording>> demos,
            std::optional<std::string> directory);

        DemoPickerView(const DemoPickerView&) = delete;
        DemoPickerView& operator=(const DemoPickerView&) = delete;
        DemoPickerView(DemoPickerView&&) = delete;
        DemoPickerView& operator=(DemoPickerView&&) = delete;

        [[nodiscard]] std::optional<std::string> Path() const;
        [[nodiscard]] bool ImportRequested() const noexcept;

        void AddClosed(const DemoPickerViewEventHandler& handler);
        void RemoveClosed(const DemoPickerViewEventHandler& handler);

        void OnAttachedToVisualTree(DemoPickerViewVisualTreeAttachmentEventArgs& e);
        void OnKeyDown(DemoPickerViewKeyEventArgs& e);

    private:
        static void OnDemoClick(void* target);
        static void OnImportClick(void* target);
        static void OnBackClick(void* target);
        static void OnFocusPosted(void* target);

        DemoPickerViewAdapter& _adapter;
        std::shared_ptr<DemoPickerViewState> _state;
    };
}

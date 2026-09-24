#pragma once

#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include "GuiTheme.hpp"
#include "MenuEntry.hpp"
#include "Rows.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    enum class PauseMenuViewHorizontalAlignment : std::uint8_t
    {
        Center
    };

    enum class PauseMenuViewVerticalAlignment : std::uint8_t
    {
        Center
    };

    enum class PauseMenuViewScrollBarVisibility : std::uint8_t
    {
        Disabled,
        Auto
    };

    enum class PauseMenuViewDispatcherPriority : std::uint8_t
    {
        Background
    };

    struct PauseMenuViewThickness final
    {
        double Left;
        double Top;
        double Right;
        double Bottom;

        [[nodiscard]] static constexpr PauseMenuViewThickness Uniform(double value) noexcept
        {
            return PauseMenuViewThickness{value, value, value, value};
        }

        friend constexpr bool operator==(
            const PauseMenuViewThickness&, const PauseMenuViewThickness&) noexcept = default;
    };

    struct PauseMenuViewScaleTransform final
    {
        double ScaleX;
        double ScaleY;

        friend constexpr bool operator==(
            const PauseMenuViewScaleTransform&,
            const PauseMenuViewScaleTransform&) noexcept = default;
    };

    struct PauseMenuViewEventArgs final
    {
        static const PauseMenuViewEventArgs Empty;
    };

    class PauseMenuViewEventHandler final
    {
    public:
        using Callback = void (*)(void* context, void* sender,
            const PauseMenuViewEventArgs& args);

        PauseMenuViewEventHandler() = default;
        PauseMenuViewEventHandler(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {});

        [[nodiscard]] static PauseMenuViewEventHandler Static(Callback function);
        [[nodiscard]] static PauseMenuViewEventHandler Instance(
            std::shared_ptr<void> target, Callback function);
        [[nodiscard]] static PauseMenuViewEventHandler Combine(
            const PauseMenuViewEventHandler& left,
            const PauseMenuViewEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const PauseMenuViewEventHandler& left,
            const PauseMenuViewEventHandler& right) noexcept;

    private:
        struct Invocation final
        {
            void* Context = nullptr;
            Callback Function = nullptr;
            std::shared_ptr<void> KeepAlive{};

            friend bool operator==(
                const Invocation& left, const Invocation& right) noexcept
            {
                return left.Context == right.Context
                    && left.Function == right.Function;
            }
        };

        explicit PauseMenuViewEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
        friend class PauseMenuViewEvent;
    };

    class PauseMenuViewEvent final
    {
    public:
        void Add(const PauseMenuViewEventHandler& handler);
        void Remove(const PauseMenuViewEventHandler& handler);
        void Invoke(void* sender, const PauseMenuViewEventArgs& args) const;

    private:
        using Invocation = PauseMenuViewEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        ::MphRead::NativeRuntime::AtomicSharedPtr<const InvocationList> _handlers{};
    };

    using PauseMenuViewControlHandle = std::shared_ptr<void>;

    template <typename T>
    struct PauseMenuViewControlRef final
    {
        PauseMenuViewControlHandle Control;
        std::shared_ptr<T> Value;
    };

    struct PauseMenuViewAction final
    {
        using Callback = void (*)(void* context);

        void* Context = nullptr;
        Callback Function = nullptr;
        std::shared_ptr<void> KeepAlive{};

        PauseMenuViewAction() = default;
        PauseMenuViewAction(void* context, Callback function,
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

    struct PauseMenuViewSizeChangedHandler final
    {
        using Callback = void (*)(void* context, double newHeight);

        void* Context = nullptr;
        Callback Function = nullptr;
        std::shared_ptr<void> KeepAlive{};

        PauseMenuViewSizeChangedHandler() = default;
        PauseMenuViewSizeChangedHandler(void* context, Callback function,
            std::shared_ptr<void> keepAlive = {})
            : Context(context), Function(function), KeepAlive(std::move(keepAlive))
        {
        }

        void Invoke(double newHeight) const
        {
            if (Function != nullptr)
            {
                Function(Context, newHeight);
            }
        }
    };

    class PauseMenuViewAdapter
    {
    public:
        virtual ~PauseMenuViewAdapter() = default;

        [[nodiscard]] virtual PauseMenuViewControlHandle ConstructStackPanel() = 0;
        virtual void SetStackPanelSpacing(
            const PauseMenuViewControlHandle& panel, double spacing) = 0;

        [[nodiscard]] virtual PauseMenuViewControlRef<Caption> ConstructCaption(
            std::optional<std::u16string> text) = 0;
        [[nodiscard]] virtual PauseMenuViewControlRef<MenuEntry> ConstructMenuEntry(
            std::optional<std::u16string> title,
            std::optional<std::u16string> subtitle = std::u16string{},
            double titleSize = 21.0) = 0;

        virtual void AddPanelChild(const PauseMenuViewControlHandle& panel,
            const PauseMenuViewControlHandle& child) = 0;
        [[nodiscard]] virtual double GetControlHeight(
            const PauseMenuViewControlHandle& control) const = 0;

        [[nodiscard]] virtual PauseMenuViewControlHandle ConstructBorder() = 0;
        virtual void SetBorderBackground(const PauseMenuViewControlHandle& border,
            const GuiBrush& brush) = 0;
        virtual void SetBorderBrush(const PauseMenuViewControlHandle& border,
            const GuiBrush& brush) = 0;
        virtual void SetBorderThickness(const PauseMenuViewControlHandle& border,
            PauseMenuViewThickness thickness) = 0;
        virtual void SetBorderPadding(const PauseMenuViewControlHandle& border,
            PauseMenuViewThickness padding) = 0;
        virtual void SetBorderChild(const PauseMenuViewControlHandle& border,
            const PauseMenuViewControlHandle& child) = 0;
        virtual void SetBorderCornerRadius(
            const PauseMenuViewControlHandle& border, double radius) = 0;

        virtual void SetControlMaxWidth(
            const PauseMenuViewControlHandle& control, double maxWidth) = 0;
        virtual void SetControlHorizontalAlignment(
            const PauseMenuViewControlHandle& control,
            PauseMenuViewHorizontalAlignment alignment) = 0;
        virtual void SetControlVerticalAlignment(
            const PauseMenuViewControlHandle& control,
            PauseMenuViewVerticalAlignment alignment) = 0;

        [[nodiscard]] virtual PauseMenuViewControlHandle
            ConstructLayoutTransformControl() = 0;
        virtual void SetLayoutTransformChild(
            const PauseMenuViewControlHandle& control,
            const PauseMenuViewControlHandle& child) = 0;
        [[nodiscard]] virtual std::optional<double> GetLayoutScaleY(
            const PauseMenuViewControlHandle& control) const = 0;
        virtual void SetLayoutTransform(
            const PauseMenuViewControlHandle& control,
            std::optional<PauseMenuViewScaleTransform> transform) = 0;

        [[nodiscard]] virtual PauseMenuViewControlHandle ConstructScrollViewer() = 0;
        virtual void SetScrollViewerContent(
            const PauseMenuViewControlHandle& viewer,
            const PauseMenuViewControlHandle& content) = 0;
        virtual void SetScrollViewerPadding(
            const PauseMenuViewControlHandle& viewer,
            PauseMenuViewThickness padding) = 0;
        virtual void SetHorizontalScrollBarVisibility(
            const PauseMenuViewControlHandle& viewer,
            PauseMenuViewScrollBarVisibility visibility) = 0;
        virtual void SetVerticalScrollBarVisibility(
            const PauseMenuViewControlHandle& viewer,
            PauseMenuViewScrollBarVisibility visibility) = 0;
        virtual void AddSizeChanged(
            const PauseMenuViewControlHandle& control,
            PauseMenuViewSizeChangedHandler handler) = 0;

        virtual void SetContent(const PauseMenuViewControlHandle& content) = 0;
        virtual void Focus(const PauseMenuViewControlHandle& control) = 0;
        virtual void PostUiThread(PauseMenuViewAction action,
            PauseMenuViewDispatcherPriority priority) = 0;
    };

    struct PauseMenuViewState;

    class PauseMenuView final
    {
    public:
        PauseMenuView(PauseMenuViewAdapter& adapter, bool offerWindowMode);

        PauseMenuView(const PauseMenuView&) = delete;
        PauseMenuView& operator=(const PauseMenuView&) = delete;
        PauseMenuView(PauseMenuView&&) = delete;
        PauseMenuView& operator=(PauseMenuView&&) = delete;

        void AddResumed(const PauseMenuViewEventHandler& handler);
        void RemoveResumed(const PauseMenuViewEventHandler& handler);
        void AddSettingsRequested(const PauseMenuViewEventHandler& handler);
        void RemoveSettingsRequested(const PauseMenuViewEventHandler& handler);
        void AddLeaveRequested(const PauseMenuViewEventHandler& handler);
        void RemoveLeaveRequested(const PauseMenuViewEventHandler& handler);
        void AddQuitRequested(const PauseMenuViewEventHandler& handler);
        void RemoveQuitRequested(const PauseMenuViewEventHandler& handler);
        void AddFullscreenRequested(const PauseMenuViewEventHandler& handler);
        void RemoveFullscreenRequested(const PauseMenuViewEventHandler& handler);
        void AddSpectateRequested(const PauseMenuViewEventHandler& handler);
        void RemoveSpectateRequested(const PauseMenuViewEventHandler& handler);
        void AddRejoinRequested(const PauseMenuViewEventHandler& handler);
        void RemoveRejoinRequested(const PauseMenuViewEventHandler& handler);
        void AddRecordToggleRequested(const PauseMenuViewEventHandler& handler);
        void RemoveRecordToggleRequested(const PauseMenuViewEventHandler& handler);
        void AddVoteMapRequested(const PauseMenuViewEventHandler& handler);
        void RemoveVoteMapRequested(const PauseMenuViewEventHandler& handler);

        void FocusResume();

    private:
        static constexpr double PanelPadding = 18.0 + 18.0 + 12.0 + 12.0;

        [[nodiscard]] static std::u16string WindowLabel();
        [[nodiscard]] static PauseMenuViewControlRef<MenuEntry> Add(
            PauseMenuViewAdapter& adapter,
            const PauseMenuViewControlHandle& stack,
            std::u16string text,
            MenuEntryEventHandler handler);
        static void FitToHost(PauseMenuViewState& state, double height);

        static void OnResumeClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnSettingsClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnLeaveClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnQuitClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnFullscreenClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnSpectateClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnRejoinClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnRecordToggleClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnVoteMapClick(void* context, void* sender,
            const MenuEntryEventArgs& args);
        static void OnSizeChanged(void* context, double newHeight);
        static void OnFocusPosted(void* context);

        PauseMenuViewAdapter& _adapter;
        std::shared_ptr<PauseMenuViewState> _state;
        std::vector<std::shared_ptr<void>> _controlValues;
    };
}

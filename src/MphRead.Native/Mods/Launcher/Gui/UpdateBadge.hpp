#pragma once

#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include "GuiTheme.hpp"
#include "TrackedText.hpp"

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
    class UpdateBadgeNullReferenceException final : public std::runtime_error
    {
    public:
        UpdateBadgeNullReferenceException();
    };

    // Keep the surrogate numerically identical to Avalonia 11.3.11 Key so
    // adapters can pass raw Key values through without remapping or collision.
    enum class UpdateBadgeKey : std::int32_t
    {
        Other = 0,
        Enter = 6,
        Space = 18
    };

    struct UpdateBadgeSize final
    {
        double Width;
        double Height;

        friend constexpr bool operator==(
            const UpdateBadgeSize&, const UpdateBadgeSize&) noexcept = default;
    };

    struct UpdateBadgePointerEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct UpdateBadgePointerPressedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct UpdateBadgePointerReleasedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct UpdateBadgeGotFocusEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct UpdateBadgeRoutedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct UpdateBadgeKeyEventArgs final
    {
        void* Native = nullptr;
        UpdateBadgeKey Key = UpdateBadgeKey::Other;
        bool Handled = false;
    };

    struct UpdateBadgeEventArgs final
    {
        static const UpdateBadgeEventArgs Empty;
    };

    class UpdateBadgeEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender,
            const UpdateBadgeEventArgs& args);

        UpdateBadgeEventHandler() = default;
        UpdateBadgeEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static UpdateBadgeEventHandler Combine(
            const UpdateBadgeEventHandler& left,
            const UpdateBadgeEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const UpdateBadgeEventHandler& left,
            const UpdateBadgeEventHandler& right) noexcept;

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

        explicit UpdateBadgeEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class UpdateBadgeEvent;
    };

    class UpdateBadgeEvent final
    {
    public:
        void Add(const UpdateBadgeEventHandler& handler);
        void Remove(const UpdateBadgeEventHandler& handler);

    private:
        friend class UpdateBadge;
        void Invoke(void* sender, const UpdateBadgeEventArgs& args) const;

        using Invocation = UpdateBadgeEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        ::MphRead::NativeRuntime::AtomicSharedPtr<const InvocationList> _handlers{};
    };

    struct UpdateBadgePen final
    {
    };

    class UpdateBadgeControlAdapter
    {
    public:
        virtual ~UpdateBadgeControlAdapter() = default;

        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;

        [[nodiscard]] virtual bool GetIsVisible() const = 0;
        virtual void SetIsVisible(bool isVisible) = 0;
        [[nodiscard]] virtual bool IsPointerOver() const = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;
        [[nodiscard]] virtual UpdateBadgeSize DesiredSize() const = 0;

        [[nodiscard]] virtual std::u16string ToUpperInvariant(
            std::u16string_view text) = 0;
        [[nodiscard]] virtual TrackedTextFormattedText CreateFormattedText(
            std::u16string_view text, TrackedTextCulture culture,
            TrackedTextFlowDirection flowDirection, TrackedTextFace face,
            double fontSize, TrackedTextBrush brush) = 0;

        virtual void InvalidateMeasure() = 0;
        virtual void InvalidateVisual() = 0;

        virtual void BaseOnPointerEntered(UpdateBadgePointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(UpdateBadgePointerEventArgs& e) = 0;
        virtual void BaseOnPointerPressed(UpdateBadgePointerPressedEventArgs& e) = 0;
        virtual void BaseOnPointerReleased(UpdateBadgePointerReleasedEventArgs& e) = 0;
        virtual void BaseOnGotFocus(UpdateBadgeGotFocusEventArgs& e) = 0;
        virtual void BaseOnLostFocus(UpdateBadgeRoutedEventArgs& e) = 0;
        virtual void BaseOnKeyDown(UpdateBadgeKeyEventArgs& e) = 0;
    };

    class UpdateBadgeDrawingContext : public TrackedTextAdapter
    {
    public:
        UpdateBadgeDrawingContext() noexcept;
        ~UpdateBadgeDrawingContext() override = default;

        virtual void DrawRectangle(const GuiBrush& brush,
            std::optional<UpdateBadgePen> pen, GuiRoundedRect rect) = 0;
    };

    class UpdateBadge final
    {
    public:
        explicit UpdateBadge(UpdateBadgeControlAdapter& control);

        UpdateBadge(const UpdateBadge&) = delete;
        UpdateBadge& operator=(const UpdateBadge&) = delete;
        UpdateBadge(UpdateBadge&&) = delete;
        UpdateBadge& operator=(UpdateBadge&&) = delete;

        [[nodiscard]] bool IsVisible() const;
        void IsVisible(bool value);
        [[nodiscard]] UpdateBadgeSize DesiredSize() const;

        void AddClick(const UpdateBadgeEventHandler& handler);
        void RemoveClick(const UpdateBadgeEventHandler& handler);

        void Show(std::optional<std::u16string> subtitle);
        void Say(std::optional<std::u16string> subtitle);

        [[nodiscard]] UpdateBadgeSize MeasureOverride(UpdateBadgeSize available);

        void OnPointerEntered(UpdateBadgePointerEventArgs& e);
        void OnPointerExited(UpdateBadgePointerEventArgs& e);
        void OnPointerPressed(UpdateBadgePointerPressedEventArgs& e);
        void OnPointerReleased(UpdateBadgePointerReleasedEventArgs& e);
        void OnGotFocus(UpdateBadgeGotFocusEventArgs& e);
        void OnLostFocus(UpdateBadgeRoutedEventArgs& e);
        void OnKeyDown(UpdateBadgeKeyEventArgs& e);

        void Render(UpdateBadgeDrawingContext& context);

    private:
        static constexpr double TitleSize = 13.0;
        static constexpr double SubtitleSize = 11.0;
        static constexpr double Tracking = 1.5;
        static constexpr double PadX = 14.0;
        static constexpr double PadY = 9.0;

        [[nodiscard]] const std::u16string& RequireSubtitle() const;

        UpdateBadgeControlAdapter& _control;
        std::u16string _title = u"Update now";
        std::optional<std::u16string> _subtitle = std::u16string{};
        bool _pressed = false;
        UpdateBadgeEvent _click;
    };
}

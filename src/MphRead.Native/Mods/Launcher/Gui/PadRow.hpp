#pragma once

#include "GuiTheme.hpp"
#include "TrackedText.hpp"
#include "../../Input/PadBindings.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    // Keep the surrogate numerically identical to Avalonia 11.3.11 Key.
    // This lets an adapter pass through the toolkit value directly while
    // unsupported keys remain unsupported instead of aliasing a handled key.
    enum class PadRowKey : std::int32_t
    {
        Other = 0,
        Enter = 6,
        Space = 18,
        Escape = 13,
        Back = 2,
        Delete = 32
    };

    // Avalonia 11.3.11 DispatcherPriority.Input has Value == -1.
    enum class PadRowDispatcherPriority : std::int32_t
    {
        Input = -1
    };

    enum class PadRowTextTrimming : std::uint8_t
    {
        CharacterEllipsis
    };

    enum class PadRowBrushKind : std::uint8_t
    {
        Transparent,
        Reference,
        SolidColor
    };

    class PadRowBrush final
    {
    public:
        PadRowBrush(const PadRowBrush&) = default;
        PadRowBrush& operator=(const PadRowBrush&) = default;
        PadRowBrush(PadRowBrush&&) noexcept = default;
        PadRowBrush& operator=(PadRowBrush&&) noexcept = default;

        [[nodiscard]] static PadRowBrush Transparent() noexcept;
        [[nodiscard]] static PadRowBrush Reference(const GuiBrush& brush) noexcept;
        [[nodiscard]] static PadRowBrush Solid(GuiColor color);

        [[nodiscard]] PadRowBrushKind Kind() const noexcept;
        [[nodiscard]] const GuiBrush* Brush() const noexcept;
        [[nodiscard]] const void* Identity() const noexcept;

    private:
        PadRowBrush(PadRowBrushKind kind, const GuiBrush* shared,
            std::shared_ptr<GuiBrush> owned) noexcept;

        PadRowBrushKind _kind;
        const GuiBrush* _shared;
        std::shared_ptr<GuiBrush> _owned;
    };

    class PadRowPen final
    {
    public:
        PadRowPen(PadRowBrush brush, double thickness);

        [[nodiscard]] const PadRowBrush& Brush() const noexcept;
        [[nodiscard]] double Thickness() const noexcept;

    private:
        PadRowBrush _brush;
        double _thickness;
    };

    struct PadRowPoint final
    {
        double X;
        double Y;
    };

    struct PadRowRoundedRect final
    {
        GuiRect Rect;
        double Radius;
    };

    struct PadRowPointerPressedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct PadRowPointerEventArgs final
    {
        void* Native = nullptr;
    };

    struct PadRowKeyEventArgs final
    {
        void* Native = nullptr;
        PadRowKey Key = PadRowKey::Other;
        bool Handled = false;
    };

    struct PadRowRoutedEventArgs final
    {
        void* Native = nullptr;
    };

    struct PadRowGotFocusEventArgs final
    {
        void* Native = nullptr;
    };

    struct PadRowVisualTreeAttachmentEventArgs final
    {
        void* Native = nullptr;
    };

    struct PadRowEventArgs final
    {
        static const PadRowEventArgs Empty;
    };

    class PadRowEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender,
            const PadRowEventArgs& args);

        PadRowEventHandler() = default;
        PadRowEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static PadRowEventHandler Combine(
            const PadRowEventHandler& left, const PadRowEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const PadRowEventHandler& left, const PadRowEventHandler& right) noexcept;

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

        explicit PadRowEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class PadRowEvent;
    };

    class PadRowEvent final
    {
    public:
        void Add(const PadRowEventHandler& handler);
        void Remove(const PadRowEventHandler& handler);

    private:
        friend class PadRow;
        void Invoke(void* sender, const PadRowEventArgs& args) const;

        using Invocation = PadRowEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    class PadRowDispatcherTimer
    {
    public:
        virtual ~PadRowDispatcherTimer() = default;
        virtual void Start() = 0;
        virtual void Stop() = 0;
    };

    class PadRowControlAdapter
    {
    public:
        using Tick = std::function<void()>;

        virtual ~PadRowControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;
        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;
        [[nodiscard]] virtual PadRowPoint GetPosition(
            const PadRowPointerPressedEventArgs& e) const = 0;
        [[nodiscard]] virtual bool RectContains(
            GuiRect rect, PadRowPoint point) const = 0;
        virtual void InvalidateVisual() = 0;

        [[nodiscard]] virtual std::shared_ptr<PadRowDispatcherTimer>
            CreateDispatcherTimer(std::chrono::milliseconds interval,
                PadRowDispatcherPriority priority, Tick tick) = 0;

        virtual void BaseOnPointerPressed(PadRowPointerPressedEventArgs& e) = 0;
        virtual void BaseOnKeyDown(PadRowKeyEventArgs& e) = 0;
        virtual void BaseOnPointerEntered(PadRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(PadRowPointerEventArgs& e) = 0;
        virtual void BaseOnLostFocus(PadRowRoutedEventArgs& e) = 0;
        virtual void BaseOnGotFocus(PadRowGotFocusEventArgs& e) = 0;
        virtual void BaseOnDetachedFromVisualTree(
            PadRowVisualTreeAttachmentEventArgs& e) = 0;
    };

    class PadRowDrawingContext : public TrackedTextAdapter
    {
    public:
        PadRowDrawingContext() noexcept;
        ~PadRowDrawingContext() override = default;

        virtual void FillRectangle(PadRowBrush brush, GuiRect rect) = 0;
        virtual void DrawRectangle(PadRowBrush brush, PadRowPen pen,
            PadRowRoundedRect rect) = 0;
        virtual void SetFormattedTextMaxTextWidth(
            TrackedTextFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextMaxTextHeight(
            TrackedTextFormattedText& text, double maxTextHeight) = 0;
        virtual void SetFormattedTextTrimming(
            TrackedTextFormattedText& text, PadRowTextTrimming trimming) = 0;
    };

    class PadRow final
    {
    public:
        PadRow(PadRowControlAdapter& control, Mods::Input::PadAction action,
            double labelWidth = 160.0);

        PadRow(const PadRow&) = delete;
        PadRow& operator=(const PadRow&) = delete;
        PadRow(PadRow&&) = delete;
        PadRow& operator=(PadRow&&) = delete;

        [[nodiscard]] double Height() const;
        void Height(double value);
        void InvalidateVisual();

        void AddRebound(const PadRowEventHandler& handler);
        void RemoveRebound(const PadRowEventHandler& handler);

        void OnPointerPressed(PadRowPointerPressedEventArgs& e);
        void OnKeyDown(PadRowKeyEventArgs& e);
        void OnPointerEntered(PadRowPointerEventArgs& e);
        void OnPointerExited(PadRowPointerEventArgs& e);
        void OnLostFocus(PadRowRoutedEventArgs& e);
        void OnGotFocus(PadRowGotFocusEventArgs& e);
        void OnDetachedFromVisualTree(PadRowVisualTreeAttachmentEventArgs& e);
        void Render(PadRowDrawingContext& context);

    private:
        [[nodiscard]] GuiRect Box() const;
        void Listen();
        void Check();
        void Done();

        PadRowControlAdapter& _control;
        const Mods::Input::PadAction _action;
        const double _labelWidth;
        bool _listening = false;
        bool _hot = false;
        std::shared_ptr<PadRowDispatcherTimer> _watch;
        Mods::Input::GamepadButtons _baseline = Mods::Input::GamepadButtons::None;
        PadRowEvent _rebound;
    };
}

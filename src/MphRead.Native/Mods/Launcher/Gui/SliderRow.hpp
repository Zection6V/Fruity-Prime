#pragma once

#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include "GuiTheme.hpp"
#include "TrackedText.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using SliderRowNullReferenceException = ::System::NullReferenceException;


    enum class SliderRowKey : std::uint8_t
    {
        Other,
        Left,
        Right
    };

    enum class SliderRowBrushKind : std::uint8_t
    {
        Transparent,
        Brush
    };

    struct SliderRowBrush final
    {
        SliderRowBrushKind Kind;
        const GuiBrush* Brush;

        [[nodiscard]] static constexpr SliderRowBrush Transparent() noexcept
        {
            return SliderRowBrush{SliderRowBrushKind::Transparent, nullptr};
        }

        [[nodiscard]] static constexpr SliderRowBrush From(const GuiBrush& brush) noexcept
        {
            return SliderRowBrush{SliderRowBrushKind::Brush, &brush};
        }
    };

    struct SliderRowPoint final
    {
        double X;
        double Y;
    };

    struct SliderRowPointerEventArgs final
    {
        void* Native = nullptr;
        void* Pointer = nullptr;
    };

    struct SliderRowGotFocusEventArgs final
    {
        void* Native = nullptr;
    };

    struct SliderRowRoutedEventArgs final
    {
        void* Native = nullptr;
    };

    struct SliderRowKeyEventArgs final
    {
        void* Native = nullptr;
        SliderRowKey Key = SliderRowKey::Other;
        bool Handled = false;
    };

    struct SliderRowEventArgs final
    {
        static const SliderRowEventArgs Empty;
    };

    class SliderRowEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender, const SliderRowEventArgs& args);

        SliderRowEventHandler() = default;
        SliderRowEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static SliderRowEventHandler Combine(
            const SliderRowEventHandler& left, const SliderRowEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const SliderRowEventHandler& left, const SliderRowEventHandler& right) noexcept;

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

        explicit SliderRowEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class SliderRowEvent;
    };

    class SliderRowEvent final
    {
    public:
        void Add(const SliderRowEventHandler& handler);
        void Remove(const SliderRowEventHandler& handler);

    private:
        friend class SliderRow;
        void Invoke(void* sender, const SliderRowEventArgs& args) const;

        using Invocation = SliderRowEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        ::MphRead::NativeRuntime::AtomicSharedPtr<const InvocationList> _handlers{};
    };

    class SliderRowControlAdapter
    {
    public:
        virtual ~SliderRowControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;

        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;

        [[nodiscard]] virtual bool GetIsEnabled() const = 0;
        virtual void SetIsEnabled(bool isEnabled) = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;

        [[nodiscard]] virtual std::u16string ToUpperInvariant(
            std::u16string_view text) = 0;
        [[nodiscard]] virtual SliderRowPoint GetPosition(
            const SliderRowPointerEventArgs& e) const = 0;
        virtual void Capture(void* pointer, void* control) = 0;

        virtual void InvalidateVisual() = 0;

        virtual void BaseOnPointerPressed(SliderRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerMoved(SliderRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerReleased(SliderRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(SliderRowPointerEventArgs& e) = 0;
        virtual void BaseOnKeyDown(SliderRowKeyEventArgs& e) = 0;
        virtual void BaseOnGotFocus(SliderRowGotFocusEventArgs& e) = 0;
        virtual void BaseOnLostFocus(SliderRowRoutedEventArgs& e) = 0;
    };

    struct SliderRowPen final
    {
    };

    class SliderRowDrawingContext : public TrackedTextAdapter
    {
    public:
        SliderRowDrawingContext() noexcept;
        ~SliderRowDrawingContext() override = default;

        virtual void FillRectangle(SliderRowBrush brush, GuiRect rect) = 0;
        virtual void DrawEllipse(SliderRowBrush brush,
            std::optional<SliderRowPen> pen, SliderRowPoint center,
            double radiusX, double radiusY) = 0;
    };

    class SliderRow final
    {
    public:
        using FormatHandler = std::function<std::optional<std::u16string>(std::int32_t)>;

        SliderRow(SliderRowControlAdapter& control,
            std::optional<std::u16string> label, std::int32_t value,
            FormatHandler format = {}, double labelWidth = 120.0,
            std::int32_t min = 0, std::int32_t max = 100,
            std::int32_t keyStep = 5);

        SliderRow(const SliderRow&) = delete;
        SliderRow& operator=(const SliderRow&) = delete;
        SliderRow(SliderRow&&) = delete;
        SliderRow& operator=(SliderRow&&) = delete;

        [[nodiscard]] double Height() const;
        void Height(double value);

        [[nodiscard]] bool IsEnabled() const;
        void IsEnabled(bool value);

        [[nodiscard]] std::int32_t Value() const noexcept;
        void Value(std::int32_t value);

        void AddValueChanged(const SliderRowEventHandler& handler);
        void RemoveValueChanged(const SliderRowEventHandler& handler);

        void InvalidateVisual();

        void OnPointerPressed(SliderRowPointerEventArgs& e);
        void OnPointerMoved(SliderRowPointerEventArgs& e);
        void OnPointerReleased(SliderRowPointerEventArgs& e);
        void OnPointerExited(SliderRowPointerEventArgs& e);
        void OnKeyDown(SliderRowKeyEventArgs& e);
        void OnGotFocus(SliderRowGotFocusEventArgs& e);
        void OnLostFocus(SliderRowRoutedEventArgs& e);

        void Render(SliderRowDrawingContext& context);

    private:
        static constexpr double ValueGutter = 112.0;

        [[nodiscard]] GuiRect Track() const;
        void SetFromPointer(double x);
        [[nodiscard]] const std::u16string& RequireLabel() const;
        [[nodiscard]] static const std::u16string& RequireFormatted(
            const std::optional<std::u16string>& value);
        [[nodiscard]] static std::optional<std::u16string> DefaultFormat(
            std::int32_t value);

        SliderRowControlAdapter& _control;
        std::optional<std::u16string> _label;
        double _labelWidth = 0.0;
        FormatHandler _format;
        std::int32_t _min = 0;
        std::int32_t _max = 0;
        std::int32_t _keyStep = 0;
        std::int32_t _value = 0;
        bool _dragging = false;
        bool _hot = false;
        SliderRowEvent _valueChanged;
    };
}

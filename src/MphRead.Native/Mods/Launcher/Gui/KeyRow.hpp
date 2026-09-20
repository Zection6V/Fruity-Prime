#pragma once

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

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    enum class Keys : std::int32_t;
}

namespace MphRead::Mods
{
    struct InputBindingProperty;
}

namespace MphRead::Mods::Launcher::Gui
{
    using KeyRowGlfwKey = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;

    class KeyRowNullReferenceException final : public std::runtime_error
    {
    public:
        KeyRowNullReferenceException();
    };

    // Keep the surrogate numerically identical to Avalonia 11.3.11 Key.
    // This preserves enum aliases (for example Return == Enter) and lets an
    // adapter pass through unsupported/raw Key values without truncation.
    enum class KeyRowKey : std::int32_t
    {
        Other = 0,
        A = 44, B = 45, C = 46, D = 47, E = 48, F = 49, G = 50,
        H = 51, I = 52, J = 53, K = 54, L = 55, M = 56,
        N = 57, O = 58, P = 59, Q = 60, R = 61, S = 62, T = 63,
        U = 64, V = 65, W = 66, X = 67, Y = 68, Z = 69,
        D0 = 34, D1 = 35, D2 = 36, D3 = 37, D4 = 38,
        D5 = 39, D6 = 40, D7 = 41, D8 = 42, D9 = 43,
        NumPad0 = 74, NumPad1 = 75, NumPad2 = 76, NumPad3 = 77,
        NumPad4 = 78, NumPad5 = 79, NumPad6 = 80, NumPad7 = 81,
        NumPad8 = 82, NumPad9 = 83,
        F1 = 90, F2 = 91, F3 = 92, F4 = 93, F5 = 94, F6 = 95,
        F7 = 96, F8 = 97, F9 = 98, F10 = 99, F11 = 100, F12 = 101,
        Space = 18,
        Tab = 3,
        Enter = 6,
        Escape = 13,
        Back = 2,
        Delete = 32,
        LeftShift = 116,
        RightShift = 117,
        LeftCtrl = 118,
        RightCtrl = 119,
        LeftAlt = 120,
        RightAlt = 121,
        Left = 23,
        Right = 25,
        Up = 24,
        Down = 26,
        Insert = 31,
        Home = 22,
        End = 21,
        PageUp = 19,
        PageDown = 20,
        CapsLock = 8,
        OemMinus = 143,
        OemPlus = 141,
        OemOpenBrackets = 149,
        OemCloseBrackets = 151,
        OemSemicolon = 140,
        OemQuotes = 152,
        OemComma = 142,
        OemPeriod = 144,
        OemQuestion = 145,
        OemBackslash = 154,
        OemPipe = 150,
        OemTilde = 146,
        Add = 85,
        Subtract = 87,
        Multiply = 84,
        Divide = 89
    };

    // Same numeric values as Avalonia 11.3.11 PointerUpdateKind. Released
    // button values (5-9) can therefore pass through and correctly hit the
    // switch default rather than being remapped to a press.
    enum class KeyRowPointerUpdateKind : std::int32_t
    {
        Other = 10,
        LeftButtonPressed = 0,
        RightButtonPressed = 2,
        MiddleButtonPressed = 1,
        XButton1Pressed = 3,
        XButton2Pressed = 4
    };

    enum class KeyRowTextTrimming : std::uint8_t
    {
        CharacterEllipsis
    };

    enum class KeyRowBrushKind : std::uint8_t
    {
        Transparent,
        Shared,
        SolidColor
    };

    class KeyRowBrush final
    {
    public:
        KeyRowBrush(const KeyRowBrush&) = default;
        KeyRowBrush& operator=(const KeyRowBrush&) = default;
        KeyRowBrush(KeyRowBrush&&) noexcept = default;
        KeyRowBrush& operator=(KeyRowBrush&&) noexcept = default;

        [[nodiscard]] static KeyRowBrush Transparent() noexcept;
        [[nodiscard]] static KeyRowBrush Reference(GuiBrush& brush) noexcept;
        [[nodiscard]] static KeyRowBrush Solid(GuiColor color);

        [[nodiscard]] KeyRowBrushKind Kind() const noexcept;
        [[nodiscard]] const GuiBrush* Brush() const noexcept;
        [[nodiscard]] const void* Identity() const noexcept;

    private:
        KeyRowBrush(KeyRowBrushKind kind, GuiBrush* shared,
            std::shared_ptr<GuiBrush> owned) noexcept;

        KeyRowBrushKind _kind;
        GuiBrush* _shared;
        std::shared_ptr<GuiBrush> _owned;
    };

    class KeyRowPen final
    {
    public:
        KeyRowPen(KeyRowBrush brush, double thickness) noexcept;

        [[nodiscard]] const KeyRowBrush& Brush() const noexcept;
        [[nodiscard]] double Thickness() const noexcept;

    private:
        KeyRowBrush _brush;
        double _thickness;
    };

    struct KeyRowPoint final
    {
        double X;
        double Y;
    };

    struct KeyRowRoundedRect final
    {
        GuiRect Rect;
        double Radius;
    };

    struct KeyRowPointerPressedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct KeyRowPointerWheelEventArgs final
    {
        void* Native = nullptr;
        double DeltaY = 0.0;
        bool Handled = false;
    };

    struct KeyRowPointerEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct KeyRowKeyEventArgs final
    {
        void* Native = nullptr;
        KeyRowKey Key = KeyRowKey::Other;
        bool Handled = false;
    };

    struct KeyRowRoutedEventArgs final
    {
        void* Native = nullptr;
        bool Handled = false;
    };

    struct KeyRowGotFocusEventArgs final
    {
        void* Native = nullptr;
    };

    struct KeyRowEventArgs final
    {
        static const KeyRowEventArgs Empty;
    };

    class KeyRowEventHandler final
    {
    public:
        using Callback = void (*)(void* target, void* sender, const KeyRowEventArgs& args);

        KeyRowEventHandler() = default;
        KeyRowEventHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static KeyRowEventHandler Combine(
            const KeyRowEventHandler& left, const KeyRowEventHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;

        friend bool operator==(
            const KeyRowEventHandler& left, const KeyRowEventHandler& right) noexcept;

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

        explicit KeyRowEventHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;

        friend class KeyRowEvent;
    };

    class KeyRowEvent final
    {
    public:
        void Add(const KeyRowEventHandler& handler);
        void Remove(const KeyRowEventHandler& handler);

    private:
        friend class KeyRow;
        void Invoke(void* sender, const KeyRowEventArgs& args) const;

        using Invocation = KeyRowEventHandler::Invocation;
        using InvocationList = std::vector<Invocation>;
        std::atomic<std::shared_ptr<const InvocationList>> _handlers{};
    };

    class KeyRowGetHandler final
    {
    public:
        using Callback = KeyRowGlfwKey (*)(void* target);

        KeyRowGetHandler() = default;
        KeyRowGetHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static KeyRowGetHandler Combine(
            const KeyRowGetHandler& left, const KeyRowGetHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;
        [[nodiscard]] KeyRowGlfwKey Invoke() const;

    private:
        struct Invocation final
        {
            std::shared_ptr<void> Target;
            Callback Function = nullptr;
        };

        explicit KeyRowGetHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
    };

    class KeyRowSetHandler final
    {
    public:
        using Callback = void (*)(void* target, KeyRowGlfwKey key);

        KeyRowSetHandler() = default;
        KeyRowSetHandler(std::shared_ptr<void> target, Callback function);

        [[nodiscard]] static KeyRowSetHandler Combine(
            const KeyRowSetHandler& left, const KeyRowSetHandler& right);

        [[nodiscard]] bool IsNull() const noexcept;
        void Invoke(KeyRowGlfwKey key) const;

    private:
        struct Invocation final
        {
            std::shared_ptr<void> Target;
            Callback Function = nullptr;
        };

        explicit KeyRowSetHandler(
            std::shared_ptr<const std::vector<Invocation>> invocations) noexcept;

        std::shared_ptr<const std::vector<Invocation>> _invocations;
    };

    class KeyRowControlAdapter
    {
    public:
        virtual ~KeyRowControlAdapter() = default;

        virtual void SetHeight(double height) = 0;
        [[nodiscard]] virtual double GetHeight() const = 0;
        virtual void SetFocusable(bool focusable) = 0;
        virtual void SetHandCursor() = 0;
        virtual void Focus() = 0;
        [[nodiscard]] virtual bool IsFocused() const = 0;
        [[nodiscard]] virtual GuiRect Bounds() const = 0;

        [[nodiscard]] virtual KeyRowPointerUpdateKind GetPointerUpdateKind(
            const KeyRowPointerPressedEventArgs& e) const = 0;
        [[nodiscard]] virtual KeyRowPoint GetPosition(
            const KeyRowPointerPressedEventArgs& e) const = 0;

        virtual void InvalidateVisual() = 0;

        virtual void BaseOnPointerPressed(KeyRowPointerPressedEventArgs& e) = 0;
        virtual void BaseOnPointerWheelChanged(KeyRowPointerWheelEventArgs& e) = 0;
        virtual void BaseOnPointerEntered(KeyRowPointerEventArgs& e) = 0;
        virtual void BaseOnPointerExited(KeyRowPointerEventArgs& e) = 0;
        virtual void BaseOnKeyDown(KeyRowKeyEventArgs& e) = 0;
        virtual void BaseOnLostFocus(KeyRowRoutedEventArgs& e) = 0;
        virtual void BaseOnGotFocus(KeyRowGotFocusEventArgs& e) = 0;
    };

    class KeyRowDrawingContext : public TrackedTextAdapter
    {
    public:
        KeyRowDrawingContext() noexcept;
        ~KeyRowDrawingContext() override = default;

        virtual void FillRectangle(KeyRowBrush brush, GuiRect rect) = 0;
        virtual void DrawRectangle(KeyRowBrush brush, KeyRowPen pen,
            KeyRowRoundedRect rect) = 0;
        virtual void SetFormattedTextMaxTextWidth(
            TrackedTextFormattedText& text, double maxTextWidth) = 0;
        virtual void SetFormattedTextMaxTextHeight(
            TrackedTextFormattedText& text, double maxTextHeight) = 0;
        virtual void SetFormattedTextTrimming(
            TrackedTextFormattedText& text, KeyRowTextTrimming trimming) = 0;
    };

    class KeyRow final
    {
    public:
        KeyRow(KeyRowControlAdapter& control,
            const ::MphRead::Mods::InputBindingProperty* property,
            double labelWidth = 160.0);
        KeyRow(KeyRowControlAdapter& control,
            std::optional<std::u16string> label,
            KeyRowGetHandler get, KeyRowSetHandler set,
            double labelWidth = 160.0);

        KeyRow(const KeyRow&) = delete;
        KeyRow& operator=(const KeyRow&) = delete;
        KeyRow(KeyRow&&) = delete;
        KeyRow& operator=(KeyRow&&) = delete;

        [[nodiscard]] double Height() const;
        void Height(double value);
        void InvalidateVisual();

        void AddRebound(const KeyRowEventHandler& handler);
        void RemoveRebound(const KeyRowEventHandler& handler);

        void OnPointerPressed(KeyRowPointerPressedEventArgs& e);
        void OnPointerWheelChanged(KeyRowPointerWheelEventArgs& e);
        void OnPointerEntered(KeyRowPointerEventArgs& e);
        void OnPointerExited(KeyRowPointerEventArgs& e);
        void OnKeyDown(KeyRowKeyEventArgs& e);
        void OnLostFocus(KeyRowRoutedEventArgs& e);
        void OnGotFocus(KeyRowGotFocusEventArgs& e);
        void Render(KeyRowDrawingContext& context);

    private:
        [[nodiscard]] GuiRect Box() const;
        void Assign(KeyRowGlfwKey key);
        void Done();
        [[nodiscard]] static std::optional<KeyRowGlfwKey> Translate(KeyRowKey key) noexcept;
        [[nodiscard]] const ::MphRead::Mods::InputBindingProperty& RequireProperty() const;

        KeyRowControlAdapter& _control;
        std::shared_ptr<const ::MphRead::Mods::InputBindingProperty> _property;
        double _labelWidth = 0.0;
        std::optional<std::u16string> _label;
        KeyRowGetHandler _get;
        KeyRowSetHandler _set;
        bool _listening = false;
        bool _hot = false;
        KeyRowEvent _rebound;
    };
}

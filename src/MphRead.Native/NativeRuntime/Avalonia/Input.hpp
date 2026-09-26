#pragma once

// Avalonia.Input and Avalonia.Interactivity as the launcher uses them: keys
// (with Avalonia's own numbering, which the screens do arithmetic on),
// modifiers, pointers, cursors, routed events and their argument types.

#include "Base.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::NativeRuntime::Avalonia
{
    class Visual;
}

namespace MphRead::NativeRuntime::Avalonia::Interactivity
{
    enum class RoutingStrategies : std::int32_t
    {
        Direct = 1,
        Tunnel = 2,
        Bubble = 4
    };

    [[nodiscard]] constexpr RoutingStrategies operator|(RoutingStrategies a, RoutingStrategies b) noexcept
    {
        return static_cast<RoutingStrategies>(static_cast<std::int32_t>(a) | static_cast<std::int32_t>(b));
    }

    [[nodiscard]] constexpr bool HasFlag(RoutingStrategies value, RoutingStrategies flag) noexcept
    {
        return (static_cast<std::int32_t>(value) & static_cast<std::int32_t>(flag)) != 0;
    }

    class RoutedEvent final
    {
    public:
        RoutedEvent(std::string name, Interactivity::RoutingStrategies strategies)
            : Name(std::move(name)), RoutingStrategies(strategies)
        {
        }
        RoutedEvent(const RoutedEvent&) = delete;
        RoutedEvent& operator=(const RoutedEvent&) = delete;

        std::string Name;
        Interactivity::RoutingStrategies RoutingStrategies;
    };

    class Interactive;

    class RoutedEventArgs
    {
    public:
        RoutedEventArgs() = default;
        explicit RoutedEventArgs(const Interactivity::RoutedEvent* routedEvent)
            : RoutedEvent(routedEvent)
        {
        }
        virtual ~RoutedEventArgs() = default;

        bool Handled = false;
        const Interactivity::RoutedEvent* RoutedEvent = nullptr;
        Interactive* Source = nullptr;
        Interactivity::RoutingStrategies Route = Interactivity::RoutingStrategies::Bubble;
    };
}

namespace MphRead::NativeRuntime::Avalonia::Input
{
    // Avalonia.Input.Key, numbered as Avalonia numbers it.
    enum class Key : std::int32_t
    {
        None = 0,
        Cancel = 1,
        Back = 2,
        Tab = 3,
        LineFeed = 4,
        Clear = 5,
        Return = 6,
        Enter = 6,
        Pause = 7,
        Capital = 8,
        CapsLock = 8,
        HangulMode = 9,
        KanaMode = 9,
        JunjaMode = 10,
        FinalMode = 11,
        HanjaMode = 12,
        KanjiMode = 12,
        Escape = 13,
        ImeConvert = 14,
        ImeNonConvert = 15,
        ImeAccept = 16,
        ImeModeChange = 17,
        Space = 18,
        PageUp = 19,
        Prior = 19,
        PageDown = 20,
        Next = 20,
        End = 21,
        Home = 22,
        Left = 23,
        Up = 24,
        Right = 25,
        Down = 26,
        Select = 27,
        Print = 28,
        Execute = 29,
        Snapshot = 30,
        PrintScreen = 30,
        Insert = 31,
        Delete = 32,
        Help = 33,
        D0 = 34, D1, D2, D3, D4, D5, D6, D7, D8, D9,
        A = 44, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LWin = 70,
        RWin = 71,
        Apps = 72,
        Sleep = 73,
        NumPad0 = 74, NumPad1, NumPad2, NumPad3, NumPad4, NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
        Multiply = 84,
        Add = 85,
        Separator = 86,
        Subtract = 87,
        Decimal = 88,
        Divide = 89,
        F1 = 90, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22,
        F23, F24,
        NumLock = 114,
        Scroll = 115,
        LeftShift = 116,
        RightShift = 117,
        LeftCtrl = 118,
        RightCtrl = 119,
        LeftAlt = 120,
        RightAlt = 121,
        BrowserBack = 122,
        BrowserForward = 123,
        BrowserRefresh = 124,
        BrowserStop = 125,
        BrowserSearch = 126,
        BrowserFavorites = 127,
        BrowserHome = 128,
        VolumeMute = 129,
        VolumeDown = 130,
        VolumeUp = 131,
        MediaNextTrack = 132,
        MediaPreviousTrack = 133,
        MediaStop = 134,
        MediaPlayPause = 135,
        LaunchMail = 136,
        SelectMedia = 137,
        LaunchApplication1 = 138,
        LaunchApplication2 = 139,
        Oem1 = 140,
        OemSemicolon = 140,
        OemPlus = 141,
        OemComma = 142,
        OemMinus = 143,
        OemPeriod = 144,
        Oem2 = 145,
        OemQuestion = 145,
        Oem3 = 146,
        OemTilde = 146,
        AbntC1 = 147,
        AbntC2 = 148,
        Oem4 = 149,
        OemOpenBrackets = 149,
        Oem5 = 150,
        OemPipe = 150,
        Oem6 = 151,
        OemCloseBrackets = 151,
        Oem7 = 152,
        OemQuotes = 152,
        Oem8 = 153,
        Oem102 = 154,
        OemBackslash = 154
    };

    // Enum.ToString for a key, as Avalonia's own names read.
    [[nodiscard]] std::string ToString(Key key);

    enum class KeyModifiers : std::int32_t
    {
        None = 0,
        Alt = 1,
        Control = 2,
        Shift = 4,
        Meta = 8
    };

    [[nodiscard]] constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b) noexcept
    {
        return static_cast<KeyModifiers>(static_cast<std::int32_t>(a) | static_cast<std::int32_t>(b));
    }
    [[nodiscard]] constexpr KeyModifiers operator&(KeyModifiers a, KeyModifiers b) noexcept
    {
        return static_cast<KeyModifiers>(static_cast<std::int32_t>(a) & static_cast<std::int32_t>(b));
    }
    [[nodiscard]] constexpr bool HasFlag(KeyModifiers value, KeyModifiers flag) noexcept
    {
        return (static_cast<std::int32_t>(value) & static_cast<std::int32_t>(flag)) == static_cast<std::int32_t>(flag);
    }

    enum class RawInputModifiers : std::int32_t
    {
        None = 0,
        Alt = 1,
        Control = 2,
        Shift = 4,
        Meta = 8,
        LeftMouseButton = 16,
        RightMouseButton = 32,
        MiddleMouseButton = 64,
        XButton1MouseButton = 128,
        XButton2MouseButton = 256,
        KeyboardMask = Alt | Control | Shift | Meta,
        PenInverted = 512,
        PenEraser = 1024,
        PenBarrelButton = 2048
    };

    [[nodiscard]] constexpr RawInputModifiers operator|(RawInputModifiers a, RawInputModifiers b) noexcept
    {
        return static_cast<RawInputModifiers>(static_cast<std::int32_t>(a) | static_cast<std::int32_t>(b));
    }
    [[nodiscard]] constexpr RawInputModifiers operator&(RawInputModifiers a, RawInputModifiers b) noexcept
    {
        return static_cast<RawInputModifiers>(static_cast<std::int32_t>(a) & static_cast<std::int32_t>(b));
    }
    [[nodiscard]] constexpr RawInputModifiers operator~(RawInputModifiers a) noexcept
    {
        return static_cast<RawInputModifiers>(~static_cast<std::int32_t>(a));
    }
    constexpr RawInputModifiers& operator|=(RawInputModifiers& a, RawInputModifiers b) noexcept { return a = a | b; }
    constexpr RawInputModifiers& operator&=(RawInputModifiers& a, RawInputModifiers b) noexcept { return a = a & b; }

    enum class MouseButton : std::int32_t { None, Left, Right, Middle, XButton1, XButton2 };
    enum class PointerType : std::int32_t { Mouse, Touch, Pen };
    enum class NavigationMethod : std::int32_t { Unspecified, Tab, Directional, Pointer };
    enum class PointerUpdateKind : std::int32_t
    {
        LeftButtonPressed, MiddleButtonPressed, RightButtonPressed, XButton1Pressed, XButton2Pressed,
        LeftButtonReleased, MiddleButtonReleased, RightButtonReleased, XButton1Released, XButton2Released, Other
    };

    enum class StandardCursorType : std::int32_t
    {
        Arrow, Ibeam, Wait, Cross, UpArrow, SizeWestEast, SizeNorthSouth, SizeAll, No, Hand, AppStarting, Help,
        TopSide, BottomSide, LeftSide, RightSide, TopLeftCorner, TopRightCorner, BottomLeftCorner, BottomRightCorner,
        DragMove, DragCopy, DragLink, None
    };

    class Cursor final
    {
    public:
        explicit Cursor(StandardCursorType type)
            : Type(type)
        {
        }
        StandardCursorType Type;
    };

    class IInputElement;

    // IPointer: one mouse, one finger, one pen, with what it is captured to.
    class IPointer final
    {
    public:
        IPointer(std::int32_t id, PointerType type, bool isPrimary)
            : Id(id), Type(type), IsPrimary(isPrimary)
        {
        }
        const std::int32_t Id;
        const PointerType Type;
        const bool IsPrimary;

        [[nodiscard]] IInputElement* Captured() const noexcept { return _captured; }
        // Capture(null) releases it. The owner is told when it loses it.
        void Capture(IInputElement* element);

    private:
        IInputElement* _captured = nullptr;
    };

    using Pointer = IPointer;

    struct PointerPointProperties final
    {
        bool IsLeftButtonPressed = false;
        bool IsRightButtonPressed = false;
        bool IsMiddleButtonPressed = false;
        Input::PointerUpdateKind PointerUpdateKind = Input::PointerUpdateKind::Other;
        float Pressure = 0.5F;
    };

    struct PointerPoint final
    {
        IPointer* Pointer = nullptr;
        Avalonia::Point Position{};
        PointerPointProperties Properties{};
    };

    class PointerEventArgs : public Interactivity::RoutedEventArgs
    {
    public:
        PointerEventArgs(const Interactivity::RoutedEvent* routedEvent, IPointer* pointer, Avalonia::Point rootPosition,
            const Visual* root, RawInputModifiers modifiers, std::uint64_t timestamp)
            : Interactivity::RoutedEventArgs(routedEvent), Pointer(pointer), Timestamp(timestamp),
              _rootPosition(rootPosition), _root(root), _modifiers(modifiers)
        {
        }

        IPointer* Pointer;
        std::uint64_t Timestamp;

        // The position relative to a visual, or to the top level when null.
        [[nodiscard]] Avalonia::Point GetPosition(const Visual* relativeTo) const;
        [[nodiscard]] PointerPoint GetCurrentPoint(const Visual* relativeTo) const;
        [[nodiscard]] Input::KeyModifiers KeyModifiers() const noexcept
        {
            return static_cast<Input::KeyModifiers>(static_cast<std::int32_t>(_modifiers)
                & static_cast<std::int32_t>(RawInputModifiers::KeyboardMask));
        }
        [[nodiscard]] RawInputModifiers RawModifiers() const noexcept { return _modifiers; }

        Input::PointerUpdateKind UpdateKind = Input::PointerUpdateKind::Other;

    private:
        Avalonia::Point _rootPosition;
        const Visual* _root;
        RawInputModifiers _modifiers;
    };

    class PointerPressedEventArgs final : public PointerEventArgs
    {
    public:
        using PointerEventArgs::PointerEventArgs;
        std::int32_t ClickCount = 1;
    };

    class PointerReleasedEventArgs final : public PointerEventArgs
    {
    public:
        using PointerEventArgs::PointerEventArgs;
        MouseButton InitialPressMouseButton = MouseButton::Left;
    };

    class PointerWheelEventArgs final : public PointerEventArgs
    {
    public:
        PointerWheelEventArgs(const Interactivity::RoutedEvent* routedEvent, IPointer* pointer,
            Avalonia::Point rootPosition, const Visual* root, RawInputModifiers modifiers, std::uint64_t timestamp,
            Vector delta)
            : PointerEventArgs(routedEvent, pointer, rootPosition, root, modifiers, timestamp), Delta(delta)
        {
        }
        Vector Delta;
    };

    class PointerCaptureLostEventArgs final : public Interactivity::RoutedEventArgs
    {
    public:
        PointerCaptureLostEventArgs(const Interactivity::RoutedEvent* routedEvent, IPointer* pointer)
            : Interactivity::RoutedEventArgs(routedEvent), Pointer(pointer)
        {
        }
        IPointer* Pointer;
    };

    class KeyEventArgs final : public Interactivity::RoutedEventArgs
    {
    public:
        using Interactivity::RoutedEventArgs::RoutedEventArgs;
        Input::Key Key = Input::Key::None;
        Input::KeyModifiers KeyModifiers = Input::KeyModifiers::None;
        std::optional<std::string> KeySymbol{};
    };

    class TextInputEventArgs final : public Interactivity::RoutedEventArgs
    {
    public:
        using Interactivity::RoutedEventArgs::RoutedEventArgs;
        std::optional<std::string> Text{};
    };

    class FocusChangedEventArgs final : public Interactivity::RoutedEventArgs
    {
    public:
        using Interactivity::RoutedEventArgs::RoutedEventArgs;
        Input::NavigationMethod NavigationMethod = Input::NavigationMethod::Unspecified;
        Input::KeyModifiers KeyModifiers = Input::KeyModifiers::None;
    };

    using GotFocusEventArgs = FocusChangedEventArgs;
}

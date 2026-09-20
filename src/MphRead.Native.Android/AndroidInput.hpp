#pragma once

#include "../MphRead.Native/Entities/Players/PlayerInput.hpp"

#include <vector>

namespace MphRead::Droid
{
    class AndroidInput final
    {
    public:
        using Keys = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;
        using MouseButton = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
        using KeyboardState = ::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState;
        using MouseState = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseState;
        using Vector2 = ::OpenTK::Mathematics::Vector2;

        AndroidInput();
        AndroidInput(const AndroidInput&) = delete;
        AndroidInput& operator=(const AndroidInput&) = delete;
        AndroidInput(AndroidInput&&) = delete;
        AndroidInput& operator=(AndroidInput&&) = delete;
        ~AndroidInput() = default;

        [[nodiscard]] KeyboardState& Keyboard() noexcept { return _keyboard; }
        [[nodiscard]] MouseState& Mouse() noexcept { return _mouse; }
        [[nodiscard]] Vector2 Pointer() const noexcept { return _pointer; }

        void SetKey(Keys key, bool down);
        void SetButton(MouseButton button, bool down);

        void Apply(const Entities::Keybind& bind, bool down);
        void ApplyKey(Keys key, bool down);
        void ApplyButton(MouseButton button, bool down);

        void BeginFrame() noexcept;
        void CommitFrame();

        void MovePointer(float deltaX, float deltaY);
        void PlacePointer(float x, float y);

        void ReleaseAll();

    private:
        using SetKeyAction = void (*)(KeyboardState&, Keys, bool);
        using SetPositionAction = void (*)(MouseState&, Vector2);
        using SetButtonAction = void (*)(MouseState&, MouseButton, bool);

        static void SetKeyState(KeyboardState& keyboard, Keys key, bool down);
        static void SetPosition(MouseState& mouse, Vector2 position) noexcept;
        static void SetButtonState(MouseState& mouse, MouseButton button, bool down);

        template <typename T>
        [[nodiscard]] static bool Contains(const std::vector<T>& values, T value) noexcept
        {
            for (T item : values)
            {
                if (item == value)
                {
                    return true;
                }
            }
            return false;
        }

        template <typename T>
        static void Add(std::vector<T>& values, T value)
        {
            if (!Contains(values, value))
            {
                values.push_back(value);
            }
        }

        KeyboardState _keyboard{};
        MouseState _mouse{};

        SetKeyAction _setKey;
        SetPositionAction _setPosition;
        SetButtonAction _setButton;

        Vector2 _pointer{};

        // HashSet<T> in the C# source has set semantics. These tiny ordered
        // vectors preserve uniqueness and the current CLR enumeration order
        // produced by clear-then-add/union operations, which keeps CommitFrame
        // phase ordering deterministic without introducing a C++ hash policy.
        std::vector<Keys> _keysDown;
        std::vector<MouseButton> _buttonsDown;
        std::vector<Keys> _keysHeld;
        std::vector<MouseButton> _buttonsHeld;
    };
}

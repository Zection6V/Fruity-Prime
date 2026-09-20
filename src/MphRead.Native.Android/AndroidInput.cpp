#include "AndroidInput.hpp"

#if !defined(__ANDROID__)
#error "AndroidInput is only valid for the Android native target."
#endif

#include "../MphRead.Native/Entities/Players/PlayerEntity.hpp"

#include <cstdint>
#include <stdexcept>

namespace
{
    constexpr std::int32_t OpenTkLastKey = 348;
    constexpr std::int32_t OpenTkMouseButtonCount = 16;
    constexpr std::int32_t NativeMouseButtonCount = 8;

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was outside the bounds of the input state.");
    }
}

namespace MphRead::Droid
{
    AndroidInput::AndroidInput()
    {
        _keyboard = std::make_unique<KeyboardState>();
        _mouse = std::make_unique<MouseState>();
        _setKey = &SetKeyState;
        _setPosition = &SetPosition;
        _setButton = &SetButtonState;
    }

    void AndroidInput::SetKeyState(KeyboardState& keyboard, Keys key, bool down)
    {
        const std::int32_t value = static_cast<std::int32_t>(key);
        if (value < 0 || value > OpenTkLastKey)
        {
            ThrowIndexOutOfRange();
        }
        keyboard.SetKeyDown(key, down);
    }

    void AndroidInput::SetPosition(MouseState& mouse, Vector2 position) noexcept
    {
        mouse.X = position.X;
        mouse.Y = position.Y;
    }

    void AndroidInput::SetButtonState(MouseState& mouse, MouseButton button, bool down)
    {
        const std::int32_t value = static_cast<std::int32_t>(button);
        if (value < 0 || value >= OpenTkMouseButtonCount)
        {
            ThrowIndexOutOfRange();
        }

        // OpenTK 4.9.4's MouseState backing BitArray has 16 slots although
        // MouseButton names only 0..7. The shared native MouseState currently
        // models the named eight slots. Preserve the C# setter's 0..15
        // acceptance/exception boundary here; 8..15 cannot be represented in
        // the shared state until that external OpenTK shim is widened.
        if (value < NativeMouseButtonCount)
        {
            mouse.SetButtonDown(button, down);
        }
    }

    void AndroidInput::SetKey(Keys key, bool down)
    {
        if (key != Keys::Unknown)
        {
            _setKey(*_keyboard, key, down);
        }
    }

    void AndroidInput::SetButton(MouseButton button, bool down)
    {
        _setButton(*_mouse, button, down);
    }

    void AndroidInput::Apply(const Entities::Keybind& bind, bool down)
    {
        if (bind.Type() == Entities::ButtonType::Key)
        {
            if (bind.Key() != Keys::Unknown && down)
            {
                Add(_keysDown, bind.Key());
            }
        }
        else if (bind.Type() == Entities::ButtonType::Mouse)
        {
            if (down)
            {
                Add(_buttonsDown, bind.MouseButton());
            }
        }
        // Scroll binds have no touch equivalent and are left alone.
    }

    void AndroidInput::ApplyKey(Keys key, bool down)
    {
        if (key != Keys::Unknown && down)
        {
            Add(_keysDown, key);
        }
    }

    void AndroidInput::ApplyButton(MouseButton button, bool down)
    {
        if (down)
        {
            Add(_buttonsDown, button);
        }
    }

    void AndroidInput::BeginFrame() noexcept
    {
        _keysDown.clear();
        _buttonsDown.clear();
    }

    void AndroidInput::CommitFrame()
    {
        for (Keys key : _keysHeld)
        {
            if (!Contains(_keysDown, key))
            {
                SetKey(key, false);
            }
        }
        for (MouseButton button : _buttonsHeld)
        {
            if (!Contains(_buttonsDown, button))
            {
                SetButton(button, false);
            }
        }
        for (Keys key : _keysDown)
        {
            SetKey(key, true);
        }
        for (MouseButton button : _buttonsDown)
        {
            SetButton(button, true);
        }

        _keysHeld.clear();
        for (Keys key : _keysDown)
        {
            Add(_keysHeld, key);
        }
        _buttonsHeld.clear();
        for (MouseButton button : _buttonsDown)
        {
            Add(_buttonsHeld, button);
        }
    }

    void AndroidInput::MovePointer(float deltaX, float deltaY)
    {
        if (deltaX == 0.0F && deltaY == 0.0F)
        {
            return;
        }
        _pointer.X += deltaX;
        _pointer.Y += deltaY;
        _setPosition(*_mouse, _pointer);
    }

    void AndroidInput::PlacePointer(float x, float y)
    {
        _pointer = Vector2(x, y);
        _setPosition(*_mouse, _pointer);
    }

    void AndroidInput::ReleaseAll()
    {
        BeginFrame();
        CommitFrame();

        std::shared_ptr<Entities::PlayerEntity> main = Entities::PlayerEntity::Main();
        if (main == nullptr)
        {
            return;
        }

        auto& all = main->Controls().All();
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            const std::shared_ptr<Entities::Keybind>& bind = all[index];
            if (bind == nullptr)
            {
                throw System::NullReferenceException();
            }

            if (bind->Type() == Entities::ButtonType::Key)
            {
                SetKey(bind->Key(), false);
            }
            else if (bind->Type() == Entities::ButtonType::Mouse)
            {
                SetButton(bind->MouseButton(), false);
            }
        }
    }
}

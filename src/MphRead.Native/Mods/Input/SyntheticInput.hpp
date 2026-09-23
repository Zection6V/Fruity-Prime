#pragma once

#include "../../Entities/Players/PlayerInput.hpp"

#include <memory>
#include <string>
#include <variant>

namespace MphRead::Mods::Input
{
    // Activator.CreateInstance hands back OpenTK's own state objects; the
    // shared_ptr is the reference C# has.
    using KeyboardState
        = std::shared_ptr<::OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState>;
    using MouseState
        = std::shared_ptr<::OpenTK::Windowing::GraphicsLibraryFramework::MouseState>;

    namespace Detail
    {
        // std::monostate is the nullable Activator result. The two typed
        // alternatives retain the observable cast boundary without inventing
        // reflection in C++.
        using SyntheticInputFactoryObject =
            std::variant<std::monostate, KeyboardState, MouseState>;

        [[nodiscard]] SyntheticInputFactoryObject SyntheticInputCreateKeyboardState();
        [[nodiscard]] SyntheticInputFactoryObject SyntheticInputCreateMouseState();
    }

    class SyntheticInput final
    {
    public:
        SyntheticInput() = delete;

        [[nodiscard]] static KeyboardState CreateKeyboard();
        [[nodiscard]] static MouseState CreateMouse();
        [[nodiscard]] static bool Available(std::string& reason);

    private:
        template <typename T>
        [[nodiscard]] static T Create();
    };
}

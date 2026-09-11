#include "SyntheticInput.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace MphRead::Mods::Input
{
    template <typename T>
    T SyntheticInput::Create()
    {
        static_assert(std::is_same_v<T, KeyboardState> || std::is_same_v<T, MouseState>);

        Detail::SyntheticInputFactoryObject instance;
        std::string_view typeName;

        if constexpr (std::is_same_v<T, KeyboardState>)
        {
            typeName = "KeyboardState";
            instance = Detail::SyntheticInputCreateKeyboardState();
        }
        else
        {
            typeName = "MouseState";
            instance = Detail::SyntheticInputCreateMouseState();
        }

        bool nullInstance = std::holds_alternative<std::monostate>(instance);
        if (const auto* keyboard = std::get_if<KeyboardState>(&instance);
            keyboard != nullptr && !*keyboard)
        {
            nullInstance = true;
        }
        if (const auto* mouse = std::get_if<MouseState>(&instance);
            mouse != nullptr && !*mouse)
        {
            nullInstance = true;
        }

        if (nullInstance)
        {
            std::string message = "Could not construct ";
            message.append(typeName.data(), typeName.size());
            message += ": OpenTK's non-public constructor is gone. ";
            message += "See Mods/Input/SyntheticInput.cs.";
            throw ProgramException(std::move(message));
        }

        if (const auto* value = std::get_if<T>(&instance); value != nullptr)
        {
            return *value;
        }

        // C# performs an explicit (T) cast after the null check. There is no
        // CLR InvalidCastException in native C++; std::bad_cast is the narrow
        // language-boundary equivalent for a non-null wrong concrete result.
        throw std::bad_cast{};
    }

    KeyboardState SyntheticInput::CreateKeyboard()
    {
        return Create<KeyboardState>();
    }

    MouseState SyntheticInput::CreateMouse()
    {
        return Create<MouseState>();
    }

    bool SyntheticInput::Available(std::string& reason)
    {
        try
        {
            (void)CreateKeyboard();
            (void)CreateMouse();
            reason = "";
            return true;
        }
        catch (const std::exception& ex)
        {
            reason = ex.what();
            return false;
        }
    }
}

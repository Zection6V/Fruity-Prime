#pragma once

#include <memory>
#include <string>
#include <variant>

namespace MphRead::Mods::Input
{
    namespace Detail
    {
        // Opaque bridge objects only. These intentionally contain no synthetic
        // input state; the platform adapter that owns the real concrete object
        // defines them and the shared_ptr preserves its identity and lifetime.
        struct SyntheticInputKeyboardState;
        struct SyntheticInputMouseState;
    }

    using KeyboardState = std::shared_ptr<Detail::SyntheticInputKeyboardState>;
    using MouseState = std::shared_ptr<Detail::SyntheticInputMouseState>;

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

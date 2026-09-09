#pragma once

#include "Mods/settings.hpp"

#include <optional>
#include <string_view>

namespace fruityprime::game {
struct State;
}

namespace fruityprime::sound {
class MusicController;
class SfxRuntime;
}

namespace fruityprime::mods {

// Native counterpart of the managed static Mods.GameSettings class. Current
// and the runtime bindings are process-wide, matching the managed ownership
// model instead of creating one settings instance per frontend.
class GameSettings final {
public:
    [[nodiscard]] static const std::optional<settings::MenuSettings>& Current()
        noexcept;

    static void Apply(const settings::MenuSettings& settings) noexcept;
    static void ApplyMatchRules() noexcept;

    // C# reaches these objects through other process-wide static classes.
    // Native frontends own them explicitly, so this is the single adapter at
    // that ownership boundary. Rebinding reapplies Current immediately.
    static void BindRuntime(game::State* state,
                            ::fruityprime::sound::SfxRuntime* sfx,
                            ::fruityprime::sound::MusicController* music,
                            std::string_view mph_key = {}) noexcept;

private:
    [[nodiscard]] static bool TryVolume(std::string_view value,
                                        float& volume) noexcept;
    [[nodiscard]] static bool TryTime(std::string_view value,
                                      float& seconds) noexcept;
};

} // namespace fruityprime::mods

namespace MphReadNative::Mods {
using GameSettings = ::fruityprime::mods::GameSettings;
}

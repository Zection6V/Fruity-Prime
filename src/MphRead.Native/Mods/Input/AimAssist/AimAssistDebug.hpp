#pragma once

#include "AimAssistTarget.hpp"
#include "../../../NativeRuntime/System/Numerics.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Hud
{
    class HudObjectInstance;
}

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistDebug final
    {
    public:
        AimAssistDebug() = delete;

        inline static bool Enabled = false;
        inline static bool UnassistedArm = false;
        inline static AimAssistResult Result{};
        inline static AimAssistTarget Target{};
        inline static System::Numerics::Vector2 Raw{};
        inline static System::Numerics::Vector2 Velocity{};

        static void Draw(Scene& scene);

    private:
        inline static Scene* _scene = nullptr;
        inline static std::shared_ptr<Hud::HudObjectInstance> _font{};
        inline static std::int64_t _textAt = 0;
        inline static std::vector<std::string> _lines{};
    };
}

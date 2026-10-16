#pragma once

#include "Mods/Render/AnimationBlend.hpp"
#include <array>

namespace MphRead::Entities { class PlayerEntity; }
namespace MphRead::Mods::Network
{
    class FormTransitionDraw final
    {
    public:
        explicit FormTransitionDraw(Entities::PlayerEntity& player);
    private:
        friend class MphRead::Entities::PlayerEntity;
        std::array<Render::AnimationBlend::FrameOverride, 2> _frames;
    };
}

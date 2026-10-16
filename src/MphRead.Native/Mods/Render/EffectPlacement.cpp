#include "Mods/Render/EffectPlacement.hpp"

#include "Formats/Collision.hpp"
#include "Formats/Effects.hpp"

namespace MphRead::Mods::Render
{
    void PlaceEffect(const std::shared_ptr<Effects::EffectEntry>& effect,
        OpenTK::Mathematics::Vector3 facing, OpenTK::Mathematics::Vector3 up,
        OpenTK::Mathematics::Vector3 position)
    {
        if (effect == nullptr) return;
        effect->Transform(facing, up, position);
        for (const auto& element : *effect->Elements)
        {
            if (element == nullptr) continue;
            element->Transform = element->EntityCollision != nullptr && !element->Expired
                ? ::OpenTK::Mathematics::Multiply(element->OwnTransform, element->EntityCollision->Transform)
                : element->OwnTransform;
        }
    }
}

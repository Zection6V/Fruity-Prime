#pragma once

#include "../../Entities/EntityBase.hpp"

#include <cstdint>
#include <memory>

namespace MphRead::Mods::Render
{
    class HunterPreviewEntity : public MphRead::Entities::EntityBase
    {
    public:
        explicit HunterPreviewEntity(Scene* scene);

        [[nodiscard]] bool Ready() const noexcept;
        void SetUp(Hunter hunter, std::int32_t recolor);
        void Step();
        void Reset();
        void GetDrawInfo() override;

    protected:
        [[nodiscard]] LightInfo GetLightInfo() override;

    private:
        static const LightInfo _light;
        static const ::OpenTK::Mathematics::Matrix4 _facing;

        Hunter _hunter = Hunter::Random;
        std::int32_t _recolor = -1;
        std::shared_ptr<ModelInstance> _model{};
    };
}

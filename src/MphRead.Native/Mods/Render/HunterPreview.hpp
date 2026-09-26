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
        // The hunter and suit the model actually loaded is, which is not
        // always the one last asked for: whoever draws the panel around this
        // has to know, or it leaves a hole for the wrong character.
        [[nodiscard]] Hunter Shown() const noexcept { return _model == nullptr ? Hunter::Random : _hunter; }
        [[nodiscard]] std::int32_t ShownSuit() const noexcept { return _recolor; }
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
        // The hunter whose model is not on this machine. See SetUp.
        Hunter _missing = Hunter::Random;
        std::int32_t _recolor = -1;
        std::shared_ptr<ModelInstance> _model{};
    };
}

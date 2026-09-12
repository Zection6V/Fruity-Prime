#pragma once

#include "../../Formats/Entity.hpp"
#include "../EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Formats
{
    class CameraSequence;
}

namespace MphRead::Entities
{
    class CamSeqEntity : public EntityBase
    {
    public:
        CamSeqEntity(CameraSequenceEntityData data, Scene* scene);

        CamSeqEntity(const CamSeqEntity&) = delete;
        CamSeqEntity& operator=(const CamSeqEntity&) = delete;
        CamSeqEntity(CamSeqEntity&&) = delete;
        CamSeqEntity& operator=(CamSeqEntity&&) = delete;

        [[nodiscard]] CameraSequenceEntityData Data() const;
        [[nodiscard]] std::shared_ptr<Formats::CameraSequence> Sequence() const noexcept;
        [[nodiscard]] std::string Name() const;

        [[nodiscard]] static CamSeqEntity* Current() noexcept;
        static void Current(CamSeqEntity* value) noexcept;

        static void ClearData();

        void Initialize() override;
        [[nodiscard]] bool Process() override;
        void HandleMessage(MessageInfo info) override;

        static void CancelCurrent();

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        void TryStart();
        void Start();
        void Cancel();
        void SendEndMessage();

        const CameraSequenceEntityData _data;
        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0x69, 0xB4).AsVector4();
        std::shared_ptr<Formats::CameraSequence> _sequence{};

        bool _active = false;
        bool _handoff = false;
        std::uint8_t _handoffTimer = 0;
        std::uint8_t _delayTimer = 0;
        std::shared_ptr<EntityBase> _endMessageTarget{};

        static CamSeqEntity* _current;
        static std::array<std::shared_ptr<Formats::CameraSequence>, 199> _sequenceData;
    };
}

#include "ForceFieldEntity.hpp"

#include "../GameState.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"
#include "../Scene.hpp"
#include "../Sound/Sfx.hpp"
#include "CamSeq/CameraSequence.hpp"
#include "Enemies/49_ForceFieldLock.hpp"
#include "EnemySpawnEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace
{
    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] MphRead::StorySave& RequireStorySave()
    {
        if (MphRead::GameState::StorySave == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *MphRead::GameState::StorySave;
    }

    template <typename T, std::size_t Size>
    [[nodiscard]] const T& GetChecked(
        const std::array<T, Size>& values, std::uint32_t index)
    {
        if (index >= Size)
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }
}

namespace MphRead::Entities
{
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    const std::array<std::int32_t, 10> ForceFieldEntity::_scanIds{
        0, 294, 295, 291, 290, 292, 293, 296, 0, 267
    };

    ForceFieldEntity::ForceFieldEntity(
        ForceFieldEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::ForceField, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        _upVector = data.Header.UpVector.ToFloatVector();
        _facingVector = data.Header.FacingVector.ToFloatVector();
        _rightVector = Vector3::Cross(_upVector, _facingVector).Normalized();
        _width = data.Width.FloatValue();
        _height = data.Height.FloatValue();
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        Scale = Vector3(_width, _height, 1.0F);
        assert(GameState::Mode == GameMode::SinglePlayer);
        const std::int32_t state = RequireStorySave().InitRoomState(
            RequireReference(_scene).RoomId, Id, _data.Active != 0);
        _active = state != 0;
        if (_active)
        {
            _scanId = GetChecked(_scanIds, static_cast<std::uint32_t>(data.Type));
        }
        else
        {
            Alpha = 0.0F;
        }
        const Vector3 position = static_cast<Vector3>(Position);
        _plane = Vector4(_facingVector, Vector3::Dot(_facingVector, position));
        SetRecolor(GetChecked(Metadata::DoorPalettes, static_cast<std::uint32_t>(data.Type)));
        ModelInstance& inst = SetUpModel("ForceField");
        (void)Read::GetModelInstance("ForceFieldLock");
        inst.SetAnimation(0);
    }

    ForceFieldEntityData ForceFieldEntity::Data() const
    {
        return _data;
    }

    Vector3 ForceFieldEntity::FieldUpVector() const noexcept
    {
        return _upVector;
    }

    Vector3 ForceFieldEntity::FieldFacingVector() const noexcept
    {
        return _facingVector;
    }

    Vector3 ForceFieldEntity::FieldRightVector() const noexcept
    {
        return _rightVector;
    }

    Vector4 ForceFieldEntity::Plane() const noexcept
    {
        return _plane;
    }

    float ForceFieldEntity::Width() const noexcept
    {
        return _width;
    }

    float ForceFieldEntity::Height() const noexcept
    {
        return _height;
    }

    bool ForceFieldEntity::Active() const noexcept
    {
        return _active;
    }

    std::shared_ptr<Enemies::Enemy49Entity> ForceFieldEntity::Lock() const noexcept
    {
        return _lock;
    }

    void ForceFieldEntity::Initialize()
    {
        EntityBase::Initialize();
        if (_active && _data.Type != 9)
        {
            std::shared_ptr<EnemyInstanceEntity> enemy = EnemySpawnEntity::SpawnEnemy(
                this, EnemyType::ForceFieldLock, NodeRef, _scene);
            _lock = std::dynamic_pointer_cast<Enemies::Enemy49Entity>(enemy);
            if (_lock != nullptr)
            {
                RequireReference(_scene).AddEntity(_lock);
            }
        }
        RequireReference(_scene).LoadEffect(77, false); // deathMech1
    }

    bool ForceFieldEntity::Process()
    {
        (void)EntityBase::Process();
        if (_active)
        {
            if (Alpha < 1.0F)
            {
                Alpha += 1.0F / 31.0F / 2.0F;
                if (Alpha > 1.0F)
                {
                    Alpha = 1.0F;
                }
            }
        }
        else if (Alpha > 0.0F)
        {
            Alpha -= 1.0F / 31.0F / 2.0F;
            if (Alpha < 0.0F)
            {
                Alpha = 0.0F;
            }
        }
        return true;
    }

    void ForceFieldEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Unlock)
        {
            if (_lock != nullptr)
            {
                _lock->SetHealth(0);
            }
            if (GameState::SinglePlayer)
            {
                if (Formats::CameraSequence::Current() == nullptr)
                {
                    RequireReference(PlayerEntity::Main()).ForceFieldSfxTimer = 2.0F / 30.0F;
                }
                else if (Sound::Sfx::ForceFieldSfxMute == 0
                    && _soundSource.CountPlayingSfx(SfxId::GEN_OFF) == 0)
                {
                    _soundSource.PlayFreeSfx(SfxId::GEN_OFF);
                }
                RequireStorySave().SetRoomState(RequireReference(_scene).RoomId, Id, 1);
            }
            _active = false;
            _scanId = 0;
        }
        else if (info.Message == Message::Lock)
        {
            if (!_active && GameState::SinglePlayer
                && Formats::CameraSequence::Current() != nullptr
                && _soundSource.CountPlayingSfx(SfxId::FORCEFIELD_APPEAR) == 0)
            {
                _soundSource.PlayFreeSfx(SfxId::FORCEFIELD_APPEAR);
            }
            _active = true;
            if (_data.Type == 9)
            {
                // bugfix?: this is a different result than when first created
                _scanId = 0;
            }
            else
            {
                _scanId = GetChecked(_scanIds, static_cast<std::uint32_t>(_data.Type));
            }
            RequireStorySave().SetRoomState(RequireReference(_scene).RoomId, Id, 3);
            if (_lock == nullptr && _data.Type != 9)
            {
                std::shared_ptr<EnemyInstanceEntity> enemy = EnemySpawnEntity::SpawnEnemy(
                    this, EnemyType::ForceFieldLock, NodeRef, _scene);
                _lock = std::dynamic_pointer_cast<Enemies::Enemy49Entity>(enemy);
                if (_lock != nullptr)
                {
                    RequireReference(_scene).AddEntity(_lock);
                }
            }
        }
    }

    void ForceFieldEntity::GetDrawInfo()
    {
        if (Alpha > 0.0F && IsVisible(NodeRef))
        {
            EntityBase::GetDrawInfo();
        }
    }
}

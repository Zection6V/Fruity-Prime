#include "ObjectEntity.hpp"

#include "../Formats/Collision.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Utility/Rng.hpp"
#include "Players/PlayerEntity.hpp"
#include "../Formats/Types.hpp"

#include <any>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

using ::MphRead::TestFlag;
using ::OpenTK::Mathematics::ClearScale;
using ::OpenTK::Mathematics::Length;

namespace
{
    using MphRead::AnimFlags;
    using MphRead::ObjectMetadata;
    using MphRead::SetFlags;
    using MphRead::StorySave;
    using MphRead::Entities::ObjEffFlags;
    using MphRead::Entities::ObjectFlags;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::Memory::Detail::InvalidCastException();
        }
    }

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return scene->RoomId();
    }

    [[nodiscard]] std::shared_ptr<MphRead::Entities::PlayerEntity> RequireMainPlayer()
    {
        std::shared_ptr<MphRead::Entities::PlayerEntity> player
            = MphRead::Entities::PlayerEntity::Main();
        if (player == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return player;
    }

    [[nodiscard]] constexpr std::int32_t UncheckedEffectInterval(
        std::uint32_t value) noexcept
    {
        return MphRead::Memory::Detail::UncheckedMultiply(
            std::bit_cast<std::int32_t>(value), 2);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedAddOne(std::int32_t value) noexcept
    {
        return MphRead::Memory::Detail::UncheckedAdd(value, 1);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedSubtractOne(std::int32_t value) noexcept
    {
        return MphRead::Memory::Detail::UncheckedAdd(value, -1);
    }

    [[nodiscard]] bool MatrixEquals(Matrix4 left, Matrix4 right) noexcept
    {
        return left.M11 == right.M11 && left.M12 == right.M12
            && left.M13 == right.M13 && left.M14 == right.M14
            && left.M21 == right.M21 && left.M22 == right.M22
            && left.M23 == right.M23 && left.M24 == right.M24
            && left.M31 == right.M31 && left.M32 == right.M32
            && left.M33 == right.M33 && left.M34 == right.M34
            && left.M41 == right.M41 && left.M42 == right.M42
            && left.M43 == right.M43 && left.M44 == right.M44;
    }

    [[nodiscard]] Matrix4 MultiplyMatrix4(Matrix4 left, Matrix4 right) noexcept
    {
        Matrix4 result{};
        result.M11 = left.M11 * right.M11 + left.M12 * right.M21
            + left.M13 * right.M31 + left.M14 * right.M41;
        result.M12 = left.M11 * right.M12 + left.M12 * right.M22
            + left.M13 * right.M32 + left.M14 * right.M42;
        result.M13 = left.M11 * right.M13 + left.M12 * right.M23
            + left.M13 * right.M33 + left.M14 * right.M43;
        result.M14 = left.M11 * right.M14 + left.M12 * right.M24
            + left.M13 * right.M34 + left.M14 * right.M44;
        result.M21 = left.M21 * right.M11 + left.M22 * right.M21
            + left.M23 * right.M31 + left.M24 * right.M41;
        result.M22 = left.M21 * right.M12 + left.M22 * right.M22
            + left.M23 * right.M32 + left.M24 * right.M42;
        result.M23 = left.M21 * right.M13 + left.M22 * right.M23
            + left.M23 * right.M33 + left.M24 * right.M43;
        result.M24 = left.M21 * right.M14 + left.M22 * right.M24
            + left.M23 * right.M34 + left.M24 * right.M44;
        result.M31 = left.M31 * right.M11 + left.M32 * right.M21
            + left.M33 * right.M31 + left.M34 * right.M41;
        result.M32 = left.M31 * right.M12 + left.M32 * right.M22
            + left.M33 * right.M32 + left.M34 * right.M42;
        result.M33 = left.M31 * right.M13 + left.M32 * right.M23
            + left.M33 * right.M33 + left.M34 * right.M43;
        result.M34 = left.M31 * right.M14 + left.M32 * right.M24
            + left.M33 * right.M34 + left.M34 * right.M44;
        result.M41 = left.M41 * right.M11 + left.M42 * right.M21
            + left.M43 * right.M31 + left.M44 * right.M41;
        result.M42 = left.M41 * right.M12 + left.M42 * right.M22
            + left.M43 * right.M32 + left.M44 * right.M42;
        result.M43 = left.M41 * right.M13 + left.M42 * right.M23
            + left.M43 * right.M33 + left.M44 * right.M43;
        result.M44 = left.M41 * right.M14 + left.M42 * right.M24
            + left.M43 * right.M34 + left.M44 * right.M44;
        return result;
    }

    [[nodiscard]] std::int32_t AnimationIdAt(
        const ObjectMetadata& meta, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= meta.AnimationIds.size())
        {
            throw MphRead::Memory::Detail::ArgumentOutOfRangeException();
        }
        return meta.AnimationIds[static_cast<std::size_t>(index)];
    }
}

namespace MphRead::Entities
{
    const std::array<SfxId, 6> ObjectEntity::_secretSwitchSfx = {
        SfxId::GOREA_SWITCH1,
        SfxId::GOREA_SWITCH1,
        SfxId::GOREA_SWITCH1,
        SfxId::GOREA_SWITCH4,
        SfxId::GOREA_SWITCH5,
        SfxId::GOREA_SWITCH6
    };

    const std::unordered_map<std::int32_t, ObjectEntity::EffectSfxInfo>
        ObjectEntity::_sfxInfo = {
            {10, EffectSfxInfo(102 | 0x4000, 0x1B, false)},
            {88, EffectSfxInfo(4, 0xE3)},
            {89, EffectSfxInfo(57 | 0x4000, 0x1B, false)},
            {97, EffectSfxInfo(57 | 0x4000, 0x1B, false)},
            {106, EffectSfxInfo(1, 0x1F)},
            {127, EffectSfxInfo(7, 0xA4)},
            {186, EffectSfxInfo(3, 0x8F)},
            {199, EffectSfxInfo(2, 0x9C)}
        };

    ObjectEntity::ObjectEntity(
        ObjectEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::Object, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _prevTransform = Transform;
        UpdateVisiblePosition();
        _flags = data.Flags;
        _state = static_cast<std::int32_t>(data.Flags & ObjectFlags::State);
        assert(GameState::Mode() == GameMode::SinglePlayer);
        std::shared_ptr<StorySave> storySave = GameState::StorySave;
        const std::int32_t roomId = GetRoomId(scene);
        const std::int32_t id = Id;
        if (storySave == nullptr)
        {
            throw Memory::Detail::NullReferenceException();
        }
        if (storySave->GetRoomState(roomId, id) == -1)
        {
            assert(_state >= 0 && _state <= 2);
            std::shared_ptr<StorySave> setStorySave = GameState::StorySave;
            const std::int32_t setRoomId = GetRoomId(scene);
            const std::int32_t setId = Id;
            const std::int32_t roomState = UncheckedAddOne(_state);
            if (setStorySave == nullptr)
            {
                throw Memory::Detail::NullReferenceException();
            }
            setStorySave->SetRoomState(setRoomId, setId, roomState);
        }
        std::shared_ptr<StorySave> getStorySave = GameState::StorySave;
        const std::int32_t getRoomId = GetRoomId(scene);
        const std::int32_t getId = Id;
        if (getStorySave == nullptr)
        {
            throw Memory::Detail::NullReferenceException();
        }
        _state = getStorySave->GetRoomState(getRoomId, getId);
        if (_state != 0 || _data.ModelId == 53)
        {
            _scanId = _data.ScanId;
        }
        _flags &= ~ObjectFlags::NoAnimation;
        _flags &= ~ObjectFlags::IsVisible;
        _flags &= ~ObjectFlags::EntityLinked;
        if (data.EffectId > 0
            && TestFlag(data.EffectFlags, ObjEffFlags::UseEffectVolume))
        {
            const RawCollisionVolume effectRawVolume = data.Volume;
            const Matrix4 effectTransform = Transform;
            _effectVolume = CollisionVolume::Transform(effectRawVolume, effectTransform);
        }
        _effectInterval = UncheckedEffectInterval(data.EffectInterval);
        if (data.ModelId == -1)
        {
            AddPlaceholderModel();
            _flags |= ObjectFlags::NoAnimation;
        }
        else
        {
            _meta = &Metadata::GetObjectById(data.ModelId);
            if (_meta->Lighting)
            {
                _anyLighting = true;
            }
            SetRecolor(_meta->RecolorId);
            ModelInstance& inst = SetUpModel(_meta->Name);
            const std::int32_t animIndex = AnimationIdAt(*_meta, _state);
            if (data.ModelId == 45)
            {
                inst.SetAnimation(animIndex, 1, SetFlags::Texcoord);
                inst.SetAnimation(
                    animIndex, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
                if (_state == 2)
                {
                    (*inst.AnimInfo->Flags)[0] |= AnimFlags::Paused;
                }
                else
                {
                    (*inst.AnimInfo->Flags)[0] |= AnimFlags::Ended;
                    (*inst.AnimInfo->Frame)[0] = Memory::Detail::UncheckedAdd(
                        (*inst.AnimInfo->FrameCount)[0], -1);
                }
            }
            else if (_meta->IgnoreAnimation)
            {
                inst.SetAnimation(-1);
            }
            else if (animIndex >= 0)
            {
                AnimFlags animFlags = AnimFlags::None;
                if ((data.ModelId == 46 && _state == 2)
                    || (data.ModelId == 53 && _state == 1))
                {
                    animFlags = AnimFlags::NoLoop;
                }
                inst.SetAnimation(animIndex, animFlags);
            }
            else
            {
                _flags |= ObjectFlags::NoAnimation;
            }

            const auto modelMetaIterator = Metadata::ModelMetadata.find(_meta->Name);
            if (modelMetaIterator == Metadata::ModelMetadata.end())
            {
                throw SceneDetail::KeyNotFoundException();
            }
            const ModelMetadata& modelMeta = modelMetaIterator->second;
            if (modelMeta.CollisionPath.has_value())
            {
                const std::shared_ptr<Formats::Collision::CollisionInstance> collision
                    = Formats::Collision::Collision::GetCollision(&modelMeta);
                SetCollision(collision, 0, &inst);
                if (modelMeta.ExtraCollisionPath.has_value())
                {
                    const std::shared_ptr<Formats::Collision::CollisionInstance> extraCollision
                        = Formats::Collision::Collision::GetCollision(&modelMeta, true);
                    SetCollision(extraCollision, 1);
                    if (_state != 2)
                    {
                        const auto entCol = EntityCollision[1];
                        if (entCol == nullptr || entCol->Collision == nullptr)
                        {
                            throw MphRead::Memory::Detail::NullReferenceException();
                        }
                        entCol->Collision->Active = false;
                    }
                }
            }
        }
    }

    ObjectEntityData ObjectEntity::Data() const
    {
        return _data;
    }

    std::optional<::OpenTK::Mathematics::Vector4> ObjectEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void ObjectEntity::Initialize()
    {
        EntityBase::Initialize();
        if (_data.EffectId > 0)
        {
            _scene->LoadEffect(_data.EffectId, false);
        }
        std::shared_ptr<EntityBase> target;
        if (_scene->TryGetEntity(_data.ScanMsgTarget, target))
        {
            _scanMsgTarget = target;
        }
    }

    void ObjectEntity::Destroy()
    {
        _soundSource.StopAllSfx(true);
        if (_effectEntry != nullptr)
        {
            _scene->UnlinkEffectEntry(_effectEntry);
        }
        EntityBase::Destroy();
    }

    void ObjectEntity::UpdateVisiblePosition()
    {
        _visiblePosition = Position;
        if (_data.ModelId != -1)
        {
            const Vector3 up = UpVector();
            const Vector3 facing = FacingVector();
            const Vector3 right = RightVector();
            if (_data.ModelId < 0
                || static_cast<std::size_t>(_data.ModelId) >= Metadata::ObjectVisPosOffsets.size())
            {
                throw Memory::Detail::ArgumentOutOfRangeException();
            }
            const Vector3 offset
                = Metadata::ObjectVisPosOffsets[static_cast<std::size_t>(_data.ModelId)];
            _visiblePosition.X += right.X * offset.X + up.X * offset.Y + facing.X * offset.Z;
            _visiblePosition.Y += right.Y * offset.X + up.Y * offset.Y + facing.Y * offset.Z;
            _visiblePosition.Z += right.Z * offset.X + up.Z * offset.Y + facing.Z * offset.Z;
        }
    }

    void ObjectEntity::GetPosition(Vector3& position)
    {
        position = _visiblePosition;
    }

    void ObjectEntity::GetVectors(Vector3& position, Vector3& up, Vector3& facing)
    {
        position = _visiblePosition;
        up = UpVector();
        facing = FacingVector();
    }

    void ObjectEntity::OnScanned()
    {
        bool sendMessage = false;
        if (_data.ScanMessage != Message::None
            && _scanMsgTarget != nullptr
            && _data.ModelId != 46)
        {
            if (TestFlag(_data.EffectFlags, ObjEffFlags::RepeatScanMessage))
            {
                sendMessage = true;
            }
            else
            {
                std::shared_ptr<StorySave> storySave = GameState::StorySave;
                const std::int32_t scanId = GetScanId();
                if (storySave == nullptr)
                {
                    throw Memory::Detail::NullReferenceException();
                }
                sendMessage = !storySave->CheckLogbook(scanId);
            }
        }
        if (sendMessage)
        {
            const MessageObject param1 = BoxInt32(-1);
            const MessageObject param2 = BoxInt32(0);
            _scene->SendMessage(
                _data.ScanMessage, this, _scanMsgTarget.get(), param1, param2);
        }
    }

    void ObjectEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate)
        {
            UpdateState(2);
        }
        else if (info.Message == Message::SetActive)
        {
            UpdateState(UnboxInt32(info.Param1));
        }
    }

    void ObjectEntity::UpdateState(std::int32_t state)
    {
        if (_state == state)
        {
            return;
        }

        const std::int32_t animId = _meta == nullptr ? -1 : AnimationIdAt(*_meta, state);
        if (animId < 0)
        {
            _flags |= ObjectFlags::NoAnimation;
        }
        else
        {
            bool needsUpdate = true;
            if (_data.ModelId == 45)
            {
                if (state <= 1)
                {
                    _soundSource.PlayFreeSfx(SfxId::EXPOSE_ARTIFACT);
                    _models[0].SetAnimation(
                        animId, 0,
                        SetFlags::Texture | SetFlags::Material | SetFlags::Node,
                        AnimFlags::NoLoop);
                    const auto entCol = EntityCollision[1];
                    if (entCol != nullptr && entCol->Collision != nullptr)
                    {
                        entCol->Collision->Active = false;
                    }
                }
                needsUpdate = false;
            }
            else if (_data.ModelId == 46)
            {
                assert(_meta != nullptr);
                if (state == 0)
                {
                    const std::int32_t currentAnimIndex
                        = (*_models[0].AnimInfo->Index)[0];
                    const std::int32_t baseAnimId = AnimationIdAt(*_meta, 0);
                    if (currentAnimIndex == baseAnimId)
                    {
                        (*_models[0].AnimInfo->Flags)[0] &= ~AnimFlags::Ended;
                        (*_models[0].AnimInfo->Flags)[0] &= ~AnimFlags::Reverse;
                        (*_models[0].AnimInfo->Flags)[0] |= AnimFlags::NoLoop;
                    }
                    else
                    {
                        _models[0].SetAnimation(animId, AnimFlags::NoLoop);
                    }
                    needsUpdate = false;
                }
                else if (state != 1)
                {
                    _models[0].SetAnimation(animId, AnimFlags::NoLoop);
                    _soundSource.PlayFreeSfx(SfxId::CHIME1);
                    needsUpdate = false;
                }
                else
                {
                    const std::int32_t currentAnimIndex
                        = (*_models[0].AnimInfo->Index)[0];
                    const std::int32_t baseAnimId = AnimationIdAt(*_meta, 0);
                    if (currentAnimIndex == baseAnimId)
                    {
                        (*_models[0].AnimInfo->Flags)[0] &= ~AnimFlags::Ended;
                        (*_models[0].AnimInfo->Flags)[0] |= AnimFlags::Reverse;
                        (*_models[0].AnimInfo->Flags)[0] |= AnimFlags::NoLoop;
                        needsUpdate = false;
                    }
                }
            }
            else if (_data.ModelId == 53 && state == 1)
            {
                _models[0].SetAnimation(animId, AnimFlags::NoLoop);
                _soundSource.PlayFreeSfx(SfxId::F2_SWITCH);
                needsUpdate = false;
            }
            else if (_data.ModelId >= 47
                && _data.ModelId <= 52
                && (state == 1 || state == 2))
            {
                if (state == 1 && _state == 0)
                {
                    return;
                }
                SfxId sfx = SfxId::GOREA_SWITCH_DEACTIVATE;
                if (state != 1)
                {
                    sfx = _secretSwitchSfx.at(
                        static_cast<std::size_t>(_data.ModelId - 47));
                }
                _soundSource.PlaySfx(sfx);
                _models[0].SetAnimation(animId, AnimFlags::NoLoop);
                needsUpdate = false;
            }
            if (needsUpdate)
            {
                _models[0].SetAnimation(animId);
            }
        }

        _state = state;
        _effectIntervalTimer = 0;
        _effectIntervalIndex = 15;
        assert(_state >= 0 && _state <= 2);
        std::shared_ptr<StorySave> storySave = GameState::StorySave;
        const std::int32_t roomId = GetRoomId(_scene);
        const std::int32_t id = Id;
        const std::int32_t roomState = UncheckedAddOne(_state);
        if (storySave == nullptr)
        {
            throw Memory::Detail::NullReferenceException();
        }
        storySave->SetRoomState(roomId, id, roomState);
        if (state != 0 || _data.ModelId == 53)
        {
            _scanId = _data.ScanId;
        }
        else
        {
            RemoveEffect();
            _scanId = 0;
        }
    }

    bool ObjectEntity::Process()
    {
        (void)EntityBase::Process();

        if (_data.ModelId == 46)
        {
            if (_state != 2)
            {
                const std::shared_ptr<PlayerEntity> mainPlayer = RequireMainPlayer();
                const Vector3 mainPosition = mainPlayer->Position;
                const Vector3 objectPosition = Position;
                const Vector3 between = mainPosition - objectPosition;
                if (Vector3::Dot(between, between) >= 15 * 15)
                {
                    if (_scanMsgTarget != nullptr)
                    {
                        const MessageObject param1 = BoxInt32(1);
                        const MessageObject param2 = BoxInt32(0);
                        _scene->SendMessage(
                            Message::SetActive, this, _scanMsgTarget.get(), param1, param2);
                    }
                    UpdateState(1);
                    if (TestFlag(
                        (*_models[0].AnimInfo->Flags)[0], AnimFlags::Ended))
                    {
                        assert(_meta != nullptr);
                        _models[0].SetAnimation(AnimationIdAt(*_meta, _state));
                    }
                }
                else
                {
                    if (_scanMsgTarget != nullptr)
                    {
                        const MessageObject param1 = BoxInt32(0);
                        const MessageObject param2 = BoxInt32(0);
                        _scene->SendMessage(
                            Message::SetActive, this, _scanMsgTarget.get(), param1, param2);
                    }
                    UpdateState(0);
                }
            }
        }
        else if (_data.ModelId == 53)
        {
            if (_state == 1
                && TestFlag((*_models[0].AnimInfo->Flags)[0], AnimFlags::Ended))
            {
                UpdateState(2);
            }
        }
        else if (_data.ModelId >= 47 && _data.ModelId <= 52)
        {
            _soundSource.Update(Position, 32);
            if (_state == 1
                && TestFlag((*_models[0].AnimInfo->Flags)[0], AnimFlags::Ended))
            {
                UpdateState(0);
            }
        }

        if (_state == 0)
        {
            return true;
        }

        if (!TestFlag(_flags, ObjectFlags::EntityLinked))
        {
            std::shared_ptr<EntityBase> entity;
            if (_data.LinkedEntity != -1
                && _scene->TryGetEntity(_data.LinkedEntity, entity))
            {
                _parent = entity;
                if (_parent == nullptr)
                {
                    throw Memory::Detail::NullReferenceException();
                }
                _parentEntCol = _parent->EntityCollision[0];
                if (_parentEntCol != nullptr)
                {
                    const Matrix4 objectTransform = _transform;
                    const Matrix4 parentInverse = _parentEntCol->Inverse2;
                    _invTransform = MultiplyMatrix4(objectTransform, parentInverse);
                }
            }
            _flags |= ObjectFlags::EntityLinked;
        }

        if (_parentEntCol != nullptr)
        {
            const Matrix4 inverseTransform = _invTransform;
            const Matrix4 parentTransform = _parentEntCol->Transform;
            Transform = MultiplyMatrix4(inverseTransform, parentTransform);
            UpdateVisiblePosition();
        }

        const Matrix4 transformForComparison = Transform;
        if (!MatrixEquals(transformForComparison, _prevTransform))
        {
            const RawCollisionVolume effectRawVolume = _data.Volume;
            const Matrix4 transformForVolume = Transform;
            _effectVolume = CollisionVolume::Transform(effectRawVolume, transformForVolume);
            _prevTransform = Transform;
        }

        UpdateCollisionTransform(0, CollisionTransform());
        UpdateCollisionTransform(1, CollisionTransform());
        UpdateLinkedInverse(0);
        UpdateLinkedInverse(1);

        if (_data.EffectId > 0)
        {
            bool processEffect = false;
            if (TestFlag(_data.EffectFlags, ObjEffFlags::AlwaysUpdateEffect))
            {
                processEffect = true;
            }
            else if (TestFlag(_flags, ObjectFlags::IsVisible))
            {
                if (TestFlag(_data.EffectFlags, ObjEffFlags::UseEffectVolume))
                {
                    Vector3 cameraPosition;
                    if (_scene->CameraMode() == CameraMode::Player)
                    {
                        const std::shared_ptr<PlayerEntity> mainPlayer = RequireMainPlayer();
                        const std::shared_ptr<CameraInfo> cameraInfo = mainPlayer->CameraInfo();
                        if (cameraInfo == nullptr)
                        {
                            throw Memory::Detail::NullReferenceException();
                        }
                        cameraPosition = cameraInfo->Position;
                    }
                    else
                    {
                        cameraPosition = _scene->CameraPosition();
                    }
                    processEffect = _effectVolume.TestPoint(cameraPosition);
                }
                else
                {
                    processEffect = _state != 0;
                }
            }

            const EffectSfxInfo* sfxInfo = nullptr;
            const auto sfxIterator = _sfxInfo.find(_data.EffectId);
            if (sfxIterator != _sfxInfo.end())
            {
                sfxInfo = &sfxIterator->second;
            }

            if (processEffect)
            {
                if (!_effectProcessing)
                {
                    _effectIntervalTimer = 0;
                    _effectIntervalIndex = 15;
                }
                if (sfxInfo != nullptr)
                {
                    _soundSource.Update(Position, sfxInfo->Data & 0x3F);
                    UpdateNodeRefVolume();
                }

                _effectIntervalTimer = UncheckedSubtractOne(_effectIntervalTimer);
                if (_effectIntervalTimer > 0)
                {
                    if (sfxInfo != nullptr
                        && sfxInfo->Environment
                        && (sfxInfo->Data & 0x80) == 0
                        && (_data.EffectOnIntervals
                            & (std::uint32_t{1}
                                << static_cast<std::uint32_t>(_effectIntervalIndex))) != 0)
                    {
                        _soundSource.PlayEnvironmentSfx(sfxInfo->SfxId);
                    }
                }
                else
                {
                    _effectIntervalIndex = Memory::Detail::UncheckedAdd(_effectIntervalIndex, 1);
                    _effectIntervalIndex %= 16;
                    const std::uint32_t intervalBit
                        = std::uint32_t{1}
                        << static_cast<std::uint32_t>(_effectIntervalIndex);
                    if (TestFlag(_data.EffectFlags, ObjEffFlags::AttachEffect))
                    {
                        const bool previouslyActive = _effectActive;
                        _effectActive = (_data.EffectOnIntervals & intervalBit) != 0;
                        if (_effectActive != previouslyActive)
                        {
                            if (!_effectActive)
                            {
                                RemoveEffect();
                            }
                            else
                            {
                                const std::int32_t effectId = _data.EffectId;
                                const Matrix4 effectTransform = Transform;
                                _effectEntry = _scene->SpawnEffectGetEntry(
                                    effectId, effectTransform);
                                if (_effectEntry != nullptr)
                                {
                                    _effectEntry->SetElementExtension(true);
                                }
                                if (sfxInfo != nullptr && !sfxInfo->Environment)
                                {
                                    _soundSource.PlaySfx(sfxInfo->SfxId);
                                }
                            }
                        }
                    }
                    else if ((_data.EffectOnIntervals & intervalBit) != 0)
                    {
                        std::shared_ptr<Formats::Collision::EntityCollision> entCol;
                        if (_parent != nullptr)
                        {
                            entCol = _parent->EntityCollision[0];
                        }
                        Vector3 spawnFacing = FacingVector();
                        Vector3 spawnUp = UpVector();
                        Vector3 spawnPos = Position;
                        if (entCol != nullptr)
                        {
                            spawnPos = Matrix::Vec3MultMtx4(spawnPos, entCol->Inverse1);
                            spawnUp = Matrix::Vec3MultMtx3(spawnUp, entCol->Inverse1);
                            spawnFacing = Matrix::Vec3MultMtx3(spawnFacing, entCol->Inverse1);
                        }
                        if (TestFlag(_data.EffectFlags, ObjEffFlags::UseEffectOffset))
                        {
                            Vector3 offset = _data.EffectPositionOffset.ToFloatVector();
                            offset.X *= Fixed::ToFloat(
                                std::uint32_t{2}
                                * (Rng::GetRandomInt1(0x1000U) - std::uint32_t{2048}));
                            offset.Y *= Fixed::ToFloat(
                                std::uint32_t{2}
                                * (Rng::GetRandomInt1(0x1000U) - std::uint32_t{2048}));
                            offset.Z *= Fixed::ToFloat(
                                std::uint32_t{2}
                                * (Rng::GetRandomInt1(0x1000U) - std::uint32_t{2048}));
                            spawnPos = spawnPos + Matrix::Vec3MultMtx3(
                                offset, GetTransformMatrix(spawnFacing, spawnUp));
                        }
                        _scene->SpawnEffect(
                            _data.EffectId, spawnFacing, spawnUp, spawnPos, false, entCol);
                        if (sfxInfo != nullptr && !sfxInfo->Environment)
                        {
                            _soundSource.PlaySfx(sfxInfo->SfxId);
                        }
                    }
                    _effectIntervalTimer = _effectInterval;
                }
            }

            _effectProcessing = processEffect;
            if (sfxInfo != nullptr
                && sfxInfo->Environment
                && (sfxInfo->Data & 0x80) != 0
                && ((sfxInfo->Data & 0x40) == 0
                    || _scene->CountElements(_data.EffectId) > 0))
            {
                _soundSource.Update(Position, sfxInfo->Data & 0x3F);
                _soundSource.PlayEnvironmentSfx(sfxInfo->SfxId);
            }
        }

        if (_effectEntry != nullptr)
        {
            const Vector3 effectPosition = Position;
            const Matrix4 effectTransform = Transform;
            const Matrix4 effectRotation = ClearScale(effectTransform);
            _effectEntry->Transform(effectPosition, effectRotation);
        }

        if (_data.ModelId == 0
            && (*_models[0].AnimInfo->Index)[0] == 3
            && TestFlag((*_models[0].AnimInfo->Flags)[0], AnimFlags::Ended))
        {
            _models[0].SetAnimation(
                static_cast<std::int32_t>(Rng::GetRandomInt1(2)));
        }
        return true;
    }

    void ObjectEntity::RemoveEffect()
    {
        if (_effectEntry != nullptr)
        {
            if (TestFlag(_data.EffectFlags, ObjEffFlags::DestroyEffect))
            {
                _scene->UnlinkEffectEntry(_effectEntry);
            }
            else
            {
                _scene->DetachEffectEntry(_effectEntry, false);
            }
        }
    }

    void ObjectEntity::GetDrawInfo()
    {
        _flags |= ObjectFlags::IsVisible;
        if (!TestFlag(_flags, ObjectFlags::NoAnimation)
            && _data.ModelId != -1)
        {
            if (_scene->ScanVisor()
                || (_data.ModelId != 0 && _data.ModelId != 41))
            {
                if (IsVisible(NodeRef))
                {
                    EntityBase::GetDrawInfo();
                }
            }
        }
        else
        {
            if (_data.EffectId != 0)
            {
                if (!IsVisible(NodeRef))
                {
                    _flags &= ~ObjectFlags::IsVisible;
                }
            }
            if (_scene->ShowInvisibleEntities())
            {
                EntityBase::GetDrawInfo();
            }
        }
    }

    void ObjectEntity::GetDisplayVolumes()
    {
        if (_data.EffectId > 0 && _scene->ShowVolumes() == VolumeDisplay::Object)
        {
            AddVolumeItem(_effectVolume, Vector3(1.0F, 0.0F, 0.0F));
        }
    }
}

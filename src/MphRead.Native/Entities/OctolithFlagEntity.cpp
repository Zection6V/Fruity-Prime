#include "OctolithFlagEntity.hpp"

#include "../Features.hpp"
#include "../Formats/CollisionDetection.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Sound/Music.hpp"
#include "Players/PlayerEntity.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

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

    [[nodiscard]] std::size_t CheckedSlotIndex(std::int32_t index)
    {
        if (index < 0 || index >= MphRead::Entities::PlayerEntity::SlotCapacity)
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(index);
    }

    [[nodiscard]] constexpr Vector3 AddScaled(
        Vector3 value, Vector3 direction, float scale) noexcept
    {
        return Vector3(
            value.X + direction.X * scale,
            value.Y + direction.Y * scale,
            value.Z + direction.Z * scale);
    }
}

namespace MphRead::Entities
{
    OctolithFlagEntity::OctolithFlagEntity(
        OctolithFlagEntityData data, Scene* scene)
        : EntityBase(EntityType::OctolithFlag, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        const GameMode mode = GameState::Mode;
        SetRecolor(mode == GameMode::Capture
            ? static_cast<std::int32_t>(data.TeamId)
            : 2);
        _bounty = mode != GameMode::Capture;
        if (mode == GameMode::Capture
            || mode == GameMode::Bounty
            || mode == GameMode::BountyTeams)
        {
            SetUpModel("octolith_ctf");
            SetUpModel(mode == GameMode::Capture ? "flagbase_ctf" : "flagbase_bounty");
            _basePosition = Position;
            SetAtBase();
        }
    }

    OctolithFlagEntityData OctolithFlagEntity::Data() const
    {
        return _data;
    }

    Vector3 OctolithFlagEntity::BasePosition() const noexcept
    {
        return _basePosition;
    }

    std::shared_ptr<PlayerEntity> OctolithFlagEntity::Carrier() const noexcept
    {
        return _carrier;
    }

    bool OctolithFlagEntity::AtBase() const noexcept
    {
        return _atBase;
    }

    std::shared_ptr<Formats::NodeData3> OctolithFlagEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void OctolithFlagEntity::SetClosestNode(
        std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    std::shared_ptr<Formats::NodeData3> OctolithFlagEntity::BaseClosestNode() const noexcept
    {
        return _baseClosestNode;
    }

    void OctolithFlagEntity::SetBaseClosestNode(
        std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _baseClosestNode = std::move(value);
    }

    void OctolithFlagEntity::SetAtBase()
    {
        Position = TypeExtensions::AddY(_basePosition, 1.25F);
        _atBase = true;
        _grounded = true;
        _resetTimer = 0.0F;
        _gravity = 0.0F;
        if (_carrier != nullptr)
        {
            _carrier->SetOctolithFlag(nullptr);
            _carrier.reset();
        }
        _lastCarrier.reset();
        _closestNode = _baseClosestNode;
    }

    void OctolithFlagEntity::GetVectors(
        Vector3& position, Vector3& up, Vector3& facing)
    {
        position = _basePosition;
        up = UpVector();
        facing = FacingVector();
    }

    void OctolithFlagEntity::GetPosition(Vector3& position)
    {
        position = _basePosition;
    }

    bool OctolithFlagEntity::Process()
    {
        (void)EntityBase::Process();
        bool pickedUp = false;
        if (_carrier == nullptr)
        {
            auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<PlayerEntity> playerValue = enumerator.Current();
                PlayerEntity& player = RequireReference(playerValue);
                if (player.Health() == 0
                    || player.IsAltForm()
                    || player.IsMorphing()
                    || (!_bounty
                        && player.TeamIndex() == static_cast<std::int32_t>(_data.TeamId)
                        && _atBase))
                {
                    continue;
                }

                const float max = Fixed::ToFloat(player.Values().MaxPickupHeight);
                const float min = Fixed::ToFloat(player.Values().MinPickupHeight);
                const float radius = Fixed::ToFloat(player.Values().BipedColRadius);
                const Vector3 cylPos(
                    Position.X,
                    Position.Y - max - 1.25F,
                    Position.Z);
                const float cylHeight = max - min + 0.5F;
                const float radii = radius + 0.5F;
                Formats::CollisionResult discard{};
                if (Formats::CollisionDetection::CheckCylinderBetweenPoints(
                    player.PrevPosition(),
                    player.Position,
                    cylPos,
                    cylHeight,
                    radii,
                    discard))
                {
                    pickedUp = OnTouched(playerValue);
                    break;
                }
            }

            if (!_atBase && _carrier == nullptr)
            {
                _resetTimer += RequireReference(_scene).FrameTime;
                if (_resetTimer >= 20.0F)
                {
                    Reset();
                }
            }
        }

        if (_carrier != nullptr)
        {
            _atBase = false;
            _grounded = true;
            _resetTimer = 0.0F;
            _closestNode = _carrier->ClosestNode();
            Position = Vector3(
                _carrier->Position.X + -0.35F * _carrier->Field70(),
                _carrier->Position.Y + 1.05F,
                _carrier->Position.Z + -0.35F * _carrier->Field74());

            if (_carrier->Health() <= 0
                || _carrier->IsAltForm()
                || _carrier->IsMorphing()
                || !TypeExtensions::TestFlag(_carrier->LoadFlags(), LoadFlags::Active))
            {
                const bool reset = _carrier->Health() == 0 && GameState::OctolithReset;
                OnDropped(reset);
            }
            else if (pickedUp)
            {
                if (!_bounty)
                {
                    if (RequireReference(PlayerEntity::Main()).TeamIndex()
                        == static_cast<std::int32_t>(_data.TeamId))
                    {
                        _soundSource.QueueStream(
                            VoiceId::VOICE_OCTO_PICKUP, 1.0F, 2.0F);
                        RequireReference(PlayerEntity::Main()).StartFlagCarrySfx();
                        Music::PlayRoomMusic(RequireReference(_scene).RoomId, 1);
                    }
                    else
                    {
                        (void)_soundSource.PlayFreeSfx(SfxId::FLAG_ACQUIRED);
                    }
                }
                else
                {
                    Music::PlayRoomMusic(RequireReference(_scene).RoomId, 1);
                    if (_carrier.get() == PlayerEntity::Main())
                    {
                        _soundSource.QueueStream(
                            VoiceId::VOICE_OCTO_PICKUP, 1.0F, 2.0F);
                        (void)_soundSource.PlayFreeSfx(SfxId::FLAG_ACQUIRED);
                        RequireReference(PlayerEntity::Main()).QueueHudMessage(
                            128, 133, 90.0F / 30.0F, 1, 202);
                    }
                    else
                    {
                        _soundSource.QueueStream(
                            VoiceId::VOICE_OCTO_PICKUP, 1.0F, 2.0F);
                        RequireReference(PlayerEntity::Main()).StartFlagCarrySfx();
                    }
                }
            }
        }

        if (_grounded)
        {
            _gravity = 0.0F;
        }
        else
        {
            const Vector3 prevPos = Position;
            float gravity;
            float displacement;
            std::tie(gravity, displacement)
                = ConstantAcceleration(-0.02F, _gravity);
            Position = TypeExtensions::AddY(Position, displacement);
            _gravity = gravity;
            _closestNode.reset();

            ManagedArray<Formats::CollisionResult> results(16);
            const std::int32_t count
                = Formats::CollisionDetection::CheckSphereBetweenPoints(
                    prevPos,
                    Position,
                    1.25F,
                    16,
                    false,
                    Formats::TestFlags::None,
                    _scene,
                    &results);
            for (std::int32_t i = 0; i < count; ++i)
            {
                const Formats::CollisionResult result
                    = results[static_cast<std::size_t>(i)];
                if (result.Plane.Y > Fixed::ToFloat(1401))
                {
                    const Vector3 pos = TypeExtensions::AddY(Position, -1.25F);
                    const Vector3 normal = result.Plane.Xyz();
                    const float dist = result.Plane.W - Vector3::Dot(pos, normal);
                    Position = AddScaled(Position, normal, dist);
                    _grounded = true;
                }
            }

            Scene& scene = RequireReference(_scene);
            assert(scene.Room != nullptr);
            if (Position.Y < RequireReference(scene.Room).Meta.KillHeight)
            {
                Reset();
            }
        }
        return true;
    }

    bool OctolithFlagEntity::OnTouched(const std::shared_ptr<PlayerEntity>& playerValue)
    {
        PlayerEntity& player = RequireReference(playerValue);
        if (_lastCarrier != nullptr && player.TeamIndex() != _lastCarrier->TeamIndex())
        {
            const std::size_t index = CheckedSlotIndex(player.SlotIndex());
            GameState::OctolithStops[index]
                = Memory::Detail::UncheckedAdd(GameState::OctolithStops[index], 1);
        }

        if (!_bounty && player.TeamIndex() == static_cast<std::int32_t>(_data.TeamId))
        {
            if (!_atBase)
            {
                (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET2);
                const std::int32_t messageId
                    = RequireReference(PlayerEntity::Main()).TeamIndex()
                        == static_cast<std::int32_t>(_data.TeamId)
                    ? 201
                    : 207;
                RequireReference(PlayerEntity::Main()).QueueHudMessage(
                    128, 133, 60.0F / 30.0F, 1, messageId);
            }
            SetAtBase();
            return false;
        }

        if (_carrier != nullptr)
        {
            _carrier->SetOctolithFlag(nullptr);
        }
        player.SetOctolithFlag(shared_from_this());
        _carrier = playerValue;
        _lastCarrier = playerValue;
        _atBase = false;
        _grounded = true;
        _resetTimer = 0.0F;
        return true;
    }

    void OctolithFlagEntity::Reset()
    {
        SetAtBase();
        std::int32_t messageId;
        if (!_bounty)
        {
            if (RequireReference(PlayerEntity::Main()).TeamIndex()
                == static_cast<std::int32_t>(_data.TeamId))
            {
                messageId = 201;
                (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET2);
            }
            else
            {
                messageId = 207;
                (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET1);
            }
        }
        else
        {
            messageId = 257;
            (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET2);
        }
        RequireReference(PlayerEntity::Main()).QueueHudMessage(
            128, 133, 60.0F / 30.0F, 1, messageId);
    }

    void OctolithFlagEntity::OnDropped(bool reset)
    {
        assert(_carrier != nullptr);
        const std::size_t dropIndex
            = CheckedSlotIndex(RequireReference(_carrier).SlotIndex());
        GameState::OctolithDrops[dropIndex]
            = Memory::Detail::UncheckedAdd(GameState::OctolithDrops[dropIndex], 1);

        std::int32_t messageId;
        if (!_bounty)
        {
            if (RequireReference(PlayerEntity::Main()).TeamIndex()
                == static_cast<std::int32_t>(_data.TeamId))
            {
                if (reset)
                {
                    messageId = 201;
                    (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET2);
                }
                else
                {
                    messageId = 230;
                    _soundSource.QueueStream(
                        VoiceId::VOICE_OCTO_RESET, 1.0F, 2.0F);
                }
            }
            else if (reset)
            {
                messageId = 207;
                (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET1);
            }
            else
            {
                messageId = 231;
                (void)_soundSource.PlayFreeSfx(SfxId::FLAG_DROPPED);
            }
        }
        else if (reset)
        {
            messageId = 257;
            (void)_soundSource.PlayFreeSfx(SfxId::FLAG_RESET2);
        }
        else
        {
            _soundSource.QueueStream(
                VoiceId::VOICE_OCTO_RESET, 1.0F, 2.0F);
            messageId = 229;
            (void)_soundSource.PlayFreeSfx(SfxId::FLAG_DROPPED);
        }

        RequireReference(PlayerEntity::Main()).QueueHudMessage(
            128, 133, 60.0F / 30.0F, 1, messageId);
        RequireReference(PlayerEntity::Main()).StopFlagCarrySfx();
        Music::PlayRoomMusic(RequireReference(_scene).RoomId, 0);
        if (reset)
        {
            SetAtBase();
        }
        else
        {
            _grounded = false;
            _atBase = false;
            if (_carrier != nullptr)
            {
                _carrier->SetOctolithFlag(nullptr);
                _carrier.reset();
            }
            _resetTimer = 0.0F;
        }
    }

    void OctolithFlagEntity::OnCaptured()
    {
        assert(_carrier != nullptr);
        if (!_bounty)
        {
            if (RequireReference(PlayerEntity::Main()).TeamIndex()
                == static_cast<std::int32_t>(_data.TeamId))
            {
                _soundSource.QueueStream(
                    VoiceId::VOICE_OCTO_SCORE, 40.0F / 30.0F);
                (void)_soundSource.PlayFreeSfx(SfxId::SCORE);
            }
            else
            {
                (void)_soundSource.PlayFreeSfx(SfxId::SCORED_ON);
            }
        }
        else
        {
            if ((Bugfixes::CorrectBountySfx
                    && RequireReference(_carrier).TeamIndex()
                        == RequireReference(PlayerEntity::Main()).TeamIndex())
                || (!Bugfixes::CorrectBountySfx
                    && RequireReference(_carrier).IsMainPlayer()))
            {
                _soundSource.QueueStream(
                    VoiceId::VOICE_BOUNTY, 40.0F / 30.0F);
                (void)_soundSource.PlayFreeSfx(SfxId::SCORE);
            }
            else
            {
                (void)_soundSource.PlayFreeSfx(SfxId::SCORED_ON);
            }
            RequireReference(PlayerEntity::Main()).QueueHudMessage(
                128, 133, 90.0F / 30.0F, 1, 203);
        }

        RequireReference(PlayerEntity::Main()).StopFlagCarrySfx();
        Music::PlayRoomMusic(RequireReference(_scene).RoomId, 0);

        const std::size_t pointsIndex
            = CheckedSlotIndex(RequireReference(_carrier).SlotIndex());
        GameState::Points[pointsIndex]
            = Memory::Detail::UncheckedAdd(GameState::Points[pointsIndex], 1);

        const std::size_t scoresIndex
            = CheckedSlotIndex(RequireReference(_carrier).SlotIndex());
        GameState::OctolithScores[scoresIndex]
            = Memory::Detail::UncheckedAdd(GameState::OctolithScores[scoresIndex], 1);
        SetAtBase();
    }

    Matrix4 OctolithFlagEntity::GetModelTransform(
        ModelInstance& inst, std::int32_t index)
    {
        Matrix4 transform = EntityBase::GetModelTransform(inst, index);
        if (index == 1)
        {
            transform.M41 = _basePosition.X;
            transform.M42 = _basePosition.Y;
            transform.M43 = _basePosition.Z;
        }
        return transform;
    }

    std::int32_t OctolithFlagEntity::GetModelRecolor(
        ModelInstance& inst, std::int32_t index)
    {
        if (index == 1 && _bounty)
        {
            return 0;
        }
        return EntityBase::GetModelRecolor(inst, index);
    }
}

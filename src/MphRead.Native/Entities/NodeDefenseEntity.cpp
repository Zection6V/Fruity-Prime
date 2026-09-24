#include "NodeDefenseEntity.hpp"

#include "../GameState.hpp"
#include "../HUD/HudInfo.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "../Sound/Music.hpp"
#include "../Sound/Sfx.hpp"
#include "../Strings.hpp"
#include "Players/PlayerEntity.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../Formats/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::CreateRotationY;
using ::OpenTK::Mathematics::CreateScale;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[nodiscard]] std::size_t CheckedSlotIndex(std::int32_t index)
    {
        if (index < 0 || index >= MphRead::Entities::PlayerEntity::SlotCapacity)
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(index);
    }

    [[nodiscard]] MphRead::ColorRgb CheckedTeamColor(std::int32_t index)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= MphRead::Metadata::TeamColors.size())
        {
            throw MphRead::Memory::Detail::IndexOutOfRangeException();
        }
        return MphRead::Metadata::TeamColors[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] Matrix4 MultiplyMatrix4(Matrix4 left, Matrix4 right) noexcept
    {
        const float leftM11 = left.M11;
        const float leftM12 = left.M12;
        const float leftM13 = left.M13;
        const float leftM14 = left.M14;
        const float leftM21 = left.M21;
        const float leftM22 = left.M22;
        const float leftM23 = left.M23;
        const float leftM24 = left.M24;
        const float leftM31 = left.M31;
        const float leftM32 = left.M32;
        const float leftM33 = left.M33;
        const float leftM34 = left.M34;
        const float leftM41 = left.M41;
        const float leftM42 = left.M42;
        const float leftM43 = left.M43;
        const float leftM44 = left.M44;

        const float rightM11 = right.M11;
        const float rightM12 = right.M12;
        const float rightM13 = right.M13;
        const float rightM14 = right.M14;
        const float rightM21 = right.M21;
        const float rightM22 = right.M22;
        const float rightM23 = right.M23;
        const float rightM24 = right.M24;
        const float rightM31 = right.M31;
        const float rightM32 = right.M32;
        const float rightM33 = right.M33;
        const float rightM34 = right.M34;
        const float rightM41 = right.M41;
        const float rightM42 = right.M42;
        const float rightM43 = right.M43;
        const float rightM44 = right.M44;

        Matrix4 result{};
        result.M11 = (leftM11 * rightM11) + (leftM12 * rightM21)
            + (leftM13 * rightM31) + (leftM14 * rightM41);
        result.M12 = (leftM11 * rightM12) + (leftM12 * rightM22)
            + (leftM13 * rightM32) + (leftM14 * rightM42);
        result.M13 = (leftM11 * rightM13) + (leftM12 * rightM23)
            + (leftM13 * rightM33) + (leftM14 * rightM43);
        result.M14 = (leftM11 * rightM14) + (leftM12 * rightM24)
            + (leftM13 * rightM34) + (leftM14 * rightM44);
        result.M21 = (leftM21 * rightM11) + (leftM22 * rightM21)
            + (leftM23 * rightM31) + (leftM24 * rightM41);
        result.M22 = (leftM21 * rightM12) + (leftM22 * rightM22)
            + (leftM23 * rightM32) + (leftM24 * rightM42);
        result.M23 = (leftM21 * rightM13) + (leftM22 * rightM23)
            + (leftM23 * rightM33) + (leftM24 * rightM43);
        result.M24 = (leftM21 * rightM14) + (leftM22 * rightM24)
            + (leftM23 * rightM34) + (leftM24 * rightM44);
        result.M31 = (leftM31 * rightM11) + (leftM32 * rightM21)
            + (leftM33 * rightM31) + (leftM34 * rightM41);
        result.M32 = (leftM31 * rightM12) + (leftM32 * rightM22)
            + (leftM33 * rightM32) + (leftM34 * rightM42);
        result.M33 = (leftM31 * rightM13) + (leftM32 * rightM23)
            + (leftM33 * rightM33) + (leftM34 * rightM43);
        result.M34 = (leftM31 * rightM14) + (leftM32 * rightM24)
            + (leftM33 * rightM34) + (leftM34 * rightM44);
        result.M41 = (leftM41 * rightM11) + (leftM42 * rightM21)
            + (leftM43 * rightM31) + (leftM44 * rightM41);
        result.M42 = (leftM41 * rightM12) + (leftM42 * rightM22)
            + (leftM43 * rightM32) + (leftM44 * rightM42);
        result.M43 = (leftM41 * rightM13) + (leftM42 * rightM23)
            + (leftM43 * rightM33) + (leftM44 * rightM43);
        result.M44 = (leftM41 * rightM14) + (leftM42 * rightM24)
            + (leftM43 * rightM34) + (leftM44 * rightM44);
        return result;
    }

    [[nodiscard]] std::shared_ptr<MphRead::Material> FirstMaterial(
        MphRead::ModelInstance& instance, const std::string& name)
    {
        const std::shared_ptr<MphRead::Model> model = instance.Model();
        if (!model || !model->Materials)
        {
            throw System::NullReferenceException();
        }
        for (const std::shared_ptr<MphRead::Material>& material : *model->Materials)
        {
            if (!material)
            {
                throw System::NullReferenceException();
            }
            if (material->Name == name)
            {
                return material;
            }
        }
        throw MphRead::SceneDetail::InvalidOperationException();
    }
}

namespace MphRead::Entities
{
    const ColorRgb NodeDefenseEntity::_neutralColor(31, 31, 31);
    const ColorRgb NodeDefenseEntity::_selfColor(15, 15, 31);
    const ColorRgb NodeDefenseEntity::_enemyColor(31, 0, 0);

    NodeDefenseEntity::NodeDefenseEntity(NodeDefenseEntityData data, Scene* scene)
        : EntityBase(EntityType::NodeDefense, scene),
          _data(data),
          _occupiedBy(std::make_shared<std::vector<bool>>(PlayerEntity::SlotCapacity, false))
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);

        const GameMode mode = GameState::Mode();
        if (mode == GameMode::Defender || mode == GameMode::DefenderTeams
            || mode == GameMode::Nodes || mode == GameMode::NodesTeams)
        {
            ModelInstance& terminalInst = SetUpModel("koth_data_flow");
            ModelInstance& ringInst = SetUpModel("koth_terminal");
            const float scale = data.Volume.CylinderRadius.FloatValue();
            _circleScale = CreateScale(scale);
            _terminalMat = FirstMaterial(terminalInst, "lambert4");
            _ringMat = FirstMaterial(ringInst, "lambert2");
        }
        if (mode == GameMode::Defender || mode == GameMode::DefenderTeams)
        {
            _defender = true;
        }
    }

    CollisionVolume NodeDefenseEntity::Volume() const noexcept
    {
        return _volume;
    }

    std::shared_ptr<PlayerEntity> NodeDefenseEntity::CapturedPlayer() const noexcept
    {
        return _capturedPlayer;
    }

    bool NodeDefenseEntity::Contested() const noexcept
    {
        return _contested;
    }

    bool NodeDefenseEntity::InProgress() const noexcept
    {
        return _inProgress;
    }

    std::int32_t NodeDefenseEntity::CurrentTeam() const noexcept
    {
        return _currentTeam;
    }

    std::int32_t NodeDefenseEntity::OccupyingTeam() const noexcept
    {
        return _occupyingTeam;
    }

    bool NodeDefenseEntity::Blinking() const noexcept
    {
        return _blinkTimer > 0.0F;
    }

    std::shared_ptr<const std::vector<bool>> NodeDefenseEntity::OccupiedBy() const noexcept
    {
        return _occupiedBy;
    }

    bool NodeDefenseEntity::IsOccupied() const
    {
        return RequireReference(_occupiedBy)[CheckedOccupiedIndex(0)]
            || RequireReference(_occupiedBy)[CheckedOccupiedIndex(1)]
            || RequireReference(_occupiedBy)[CheckedOccupiedIndex(2)]
            || RequireReference(_occupiedBy)[CheckedOccupiedIndex(3)];
    }

    float NodeDefenseEntity::Progress() const noexcept
    {
        return _progress;
    }

    std::shared_ptr<Formats::NodeData3> NodeDefenseEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void NodeDefenseEntity::SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    std::size_t NodeDefenseEntity::CheckedOccupiedIndex(std::int32_t index) const
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= RequireReference(_occupiedBy).size())
        {
            throw Memory::Detail::IndexOutOfRangeException();
        }
        return static_cast<std::size_t>(index);
    }

    bool NodeDefenseEntity::Process()
    {
        if (_defender)
        {
            ProcessDefender();
        }
        else
        {
            ProcessNodes();
        }
        return true;
    }

    void NodeDefenseEntity::ProcessDefender()
    {
        std::int32_t team = 4;
        _contested = false;

        auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<PlayerEntity> playerValue = enumerator.Current();
            PlayerEntity& player = RequireReference(playerValue);
            if (player.Health() > 0
                && _volume.TestPoint(player.Volume().SpherePosition))
            {
                if (team == 4)
                {
                    team = player.TeamIndex();
                }
                else if (team != player.TeamIndex())
                {
                    _contested = true;
                }
            }
        }

        if (_contested)
        {
            team = 4;
        }

        float speed;
        float rotation;
        if (team == 4)
        {
            std::tie(speed, rotation)
                = ConstantAcceleration(-0.25F, _spinSpeed, 0.0F);
        }
        else
        {
            std::tie(speed, rotation)
                = ConstantAcceleration(0.25F, _spinSpeed,
                    std::numeric_limits<float>::lowest(), 8.0F * 30.0F);
            auto& teamTimes = GameState::TeamTime();
            const std::size_t teamIndex = CheckedSlotIndex(team);
            float& teamTime = teamTimes[teamIndex];
            const float frameTime = RequireReference(_scene).FrameTime();
            teamTime += frameTime;
        }
        _spinSpeed = speed;
        _curRotation += rotation;
        if (_curRotation >= 360.0F)
        {
            _curRotation -= 360.0F;
        }
        _currentTeam = team;
    }

    void NodeDefenseEntity::ProcessNodes()
    {
        std::int32_t value1 = 0;
        std::int32_t value2 = 0;
        std::vector<bool> prevOccupiedBy(PlayerEntity::SlotCapacity, false);
        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::size_t index = CheckedOccupiedIndex(i);
            prevOccupiedBy[index] = RequireReference(_occupiedBy)[index];
            RequireReference(_occupiedBy)[index] = false;
        }

        _contested = false;
        std::int32_t slot = 0;
        bool occupiedByAny = false;
        _soundSource.Update(Position, 17);

        auto playerEnumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
        while (playerEnumerator.MoveNext())
        {
            std::shared_ptr<PlayerEntity> playerValue = playerEnumerator.Current();
            PlayerEntity& player = RequireReference(playerValue);
            if (player.Health() > 0
                && _volume.TestPoint(player.Volume().SpherePosition))
            {
                if (_occupyingTeam == player.TeamIndex())
                {
                    RequireReference(_occupiedBy)[CheckedOccupiedIndex(player.SlotIndex())] = true;
                    occupiedByAny = true;
                    slot = player.SlotIndex();
                }
                else if (_occupyingTeam == 4 && _currentTeam != player.TeamIndex())
                {
                    RequireReference(_occupiedBy)[CheckedOccupiedIndex(player.SlotIndex())] = true;
                    occupiedByAny = true;
                    _occupyingTeam = player.TeamIndex();
                    _progress = 0.0F;
                    _inProgress = false;
                    slot = player.SlotIndex();
                }
                else if (_occupyingTeam != player.TeamIndex())
                {
                    _contested = true;
                }
            }
        }
        (void)slot;

        float rotation = 0.0F;
        if (occupiedByAny)
        {
            if (_contested)
            {
                PlayerEntity& main = RequireReference(PlayerEntity::Main());
                if (RequireReference(_occupiedBy)[CheckedOccupiedIndex(main.SlotIndex())])
                {
                    _soundSource.SetPausedFreeSfxScripts(true);
                }
            }
            else if (_currentTeam != _occupyingTeam)
            {
                PlayerEntity& main = RequireReference(PlayerEntity::Main());
                if (RequireReference(_occupiedBy)[CheckedOccupiedIndex(main.SlotIndex())])
                {
                    if (!_inProgress && _progress >= 10.0F / 30.0F)
                    {
                        Music::PlayRoomMusic(RequireReference(_scene).RoomId(), 2);
                        value1 = 1;
                        _inProgress = true;
                    }
                    _soundSource.SetPausedFreeSfxScripts(false);
                }

                _progress += RequireReference(_scene).FrameTime();
                const float spinSpeed
                    = _progress / (300.0F / 30.0F) * (15.0F * 30.0F);
                const float firstRotationTerm
                    = _spinSpeed * RequireReference(_scene).FrameTime();
                const float secondRotationFactor = (spinSpeed - _spinSpeed) / 2.0F;
                const float secondFrameTime = RequireReference(_scene).FrameTime();
                const float secondRotationTerm = secondRotationFactor * secondFrameTime;
                rotation = firstRotationTerm + secondRotationTerm;
                _spinSpeed = spinSpeed;
                if (_progress >= 300.0F / 30.0F)
                {
                    Complete(value1, value2);
                    occupiedByAny = false;
                }
            }
        }
        else
        {
            PlayerEntity& main = RequireReference(PlayerEntity::Main());
            if (prevOccupiedBy[CheckedOccupiedIndex(main.SlotIndex())])
            {
                Music::PlayRoomMusic(RequireReference(_scene).RoomId(), 0);
                if (value1 != 2)
                {
                    value1 = 3;
                }
            }
            _occupyingTeam = 4;
            _progress = 0.0F;
            _inProgress = false;
            std::tie(_spinSpeed, rotation)
                = ConstantAcceleration(-0.15F, _spinSpeed, 0.0F);
        }

        std::int32_t nodeCount = 0;
        std::int32_t team = _currentTeam;
        float scoreThreshold = 150.0F / 30.0F;
        if (team == 4)
        {
            team = _occupyingTeam;
        }
        if (team != 4)
        {
            auto nodeEnumerator = RequireReference(_scene).GetNodeDefenseEntities().GetEnumerator();
            while (nodeEnumerator.MoveNext())
            {
                std::shared_ptr<NodeDefenseEntity> nodeValue = nodeEnumerator.Current();
                NodeDefenseEntity& node = RequireReference(nodeValue);
                if (node._currentTeam == team && node._occupyingTeam == 4)
                {
                    nodeCount = Memory::Detail::UncheckedAdd(nodeCount, 1);
                    if (nodeCount > 1)
                    {
                        scoreThreshold -= 45.0F / 30.0F;
                    }
                }
            }
        }

        if (_currentTeam != 4 && !occupiedByAny)
        {
            _scoreTimer += RequireReference(_scene).FrameTime();
            if (_scoreTimer >= scoreThreshold)
            {
                assert(_capturedPlayer != nullptr);
                auto& points = GameState::Points();
                PlayerEntity& capturedPlayer = RequireReference(_capturedPlayer);
                const std::size_t capturedSlot = CheckedSlotIndex(capturedPlayer.SlotIndex());
                std::int32_t& capturedPoints = points[capturedSlot];
                capturedPoints = Memory::Detail::UncheckedAdd(capturedPoints, 1);
                _scoreTimer = 0.0F;
            }

            if (nodeCount == 1)
            {
                _soundSource.PlaySfx(SfxId::DATA_SLOW, true);
            }
            else if (nodeCount > 1)
            {
                _soundSource.PlaySfx(SfxId::DATA_FAST, true);
            }
        }

        if (value2 != 0 && nodeCount >= 2)
        {
            value1 = 5;
        }
        if (value1 == 1)
        {
            _soundSource.StopFreeSfxScripts();
            if (nodeCount == 0)
            {
                (void)_soundSource.PlayFreeSfx(SfxId::CAPTURE_RING_SCRIPT1);
            }
            else if (nodeCount == 1)
            {
                (void)_soundSource.PlayFreeSfx(SfxId::CAPTURE_RING_SCRIPT2);
            }
            else
            {
                (void)_soundSource.PlayFreeSfx(SfxId::CAPTURE_RING_SCRIPT3);
            }
        }
        else if (value1 == 2)
        {
            if (nodeCount >= 2)
            {
                _soundSource.QueueStream(
                    VoiceId::VOICE_MULTI_NODE, 1.0F, 35.0F / 30.0F);
            }
        }
        else if (value1 == 3)
        {
            _soundSource.StopFreeSfxScripts();
            (void)_soundSource.PlayFreeSfx(SfxId::CAPTURE_RING_FAIL);
        }

        const float prevRotation = _curRotation;
        _curRotation += rotation;
        if (_curRotation >= 360.0F)
        {
            _curRotation -= 360.0F;
        }
        if (!occupiedByAny)
        {
            _blinkTimer = 0.0F;
        }
        else
        {
            const std::int32_t currentFixed = Fixed::ToInt(_curRotation);
            const std::int32_t previousFixed = Fixed::ToInt(prevRotation);
            if (currentFixed / 61440 != previousFixed / 61440)
            {
                _blinkTimer = 1.0F / 30.0F;
            }
            else if (_blinkTimer > 0.0F)
            {
                _blinkTimer -= RequireReference(_scene).FrameTime();
            }
        }
    }

    void NodeDefenseEntity::Complete(std::int32_t& dest1, std::int32_t& dest2)
    {
        if (_currentTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
        {
            dest1 = 4;
            const std::string msg = Text::Strings::GetHudMessage(211);
            RequireReference(PlayerEntity::Main()).QueueHudMessage(
                128, 133, Hud::Align::Center, 256, 8,
                ColorRgba(31U), 1, 90.0F / 30.0F, 17, msg);
        }

        for (std::int32_t i = 0; i < 4; ++i)
        {
            std::shared_ptr<PlayerEntity> playerValue
                = PlayerEntity::Players()[static_cast<std::size_t>(i)];
            const std::size_t index = CheckedOccupiedIndex(i);
            if (RequireReference(_occupiedBy)[index])
            {
                auto& nodesCaptured = GameState::NodesCaptured();
                std::int32_t& capturedCount = nodesCaptured[static_cast<std::size_t>(i)];
                capturedCount = Memory::Detail::UncheckedAdd(capturedCount, 1);
                if (TypeExtensions::TestFlag(
                    RequireReference(playerValue).LoadFlags(), LoadFlags::Active))
                {
                    _capturedPlayer = playerValue;
                }
            }
            else if (_currentTeam == RequireReference(playerValue).TeamIndex())
            {
                auto& nodesLost = GameState::NodesLost();
                std::int32_t& lostCount = nodesLost[static_cast<std::size_t>(i)];
                lostCount = Memory::Detail::UncheckedAdd(lostCount, 1);
            }
            RequireReference(_occupiedBy)[index] = false;
        }

        std::shared_ptr<PlayerEntity> mainPlayer = PlayerEntity::Main();
        if (_capturedPlayer == mainPlayer)
        {
            RequireReference(PlayerEntity::Main()).QueueHudMessage(
                128, 133, 90.0F / 30.0F, 1, 206);
        }

        _currentTeam = _occupyingTeam;
        _progress = 0.0F;
        _inProgress = false;
        _occupyingTeam = 4;
        _scoreTimer = 150.0F / 30.0F;
        if (_currentTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
        {
            Music::PlayRoomMusic(RequireReference(_scene).RoomId(), 0);
            dest1 = 2;
        }
        else
        {
            dest2 = 1;
        }
        _blinkTimer = 0.0F;
    }

    void NodeDefenseEntity::GetDrawInfo()
    {
        const bool blinking = _blinkTimer > 0.0F;
        ColorRgb color = _neutralColor;
        if (_currentTeam == 4)
        {
            if (blinking)
            {
                if (GameState::Teams())
                {
                    color = CheckedTeamColor(_occupyingTeam);
                }
                else if (_occupyingTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
                {
                    color = _selfColor;
                }
                else
                {
                    color = _enemyColor;
                }
            }
        }
        else if (GameState::Teams())
        {
            if (blinking)
            {
                color = CheckedTeamColor(_occupyingTeam);
            }
            else
            {
                color = CheckedTeamColor(_currentTeam);
            }
        }
        else
        {
            if (_currentTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
            {
                if (!blinking
                    || _occupyingTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
                {
                    color = _selfColor;
                }
                else
                {
                    color = _enemyColor;
                }
            }
            else if (blinking
                && _occupyingTeam == RequireReference(PlayerEntity::Main()).TeamIndex())
            {
                color = _selfColor;
            }
            else
            {
                color = _enemyColor;
            }
        }

        RequireReference(_terminalMat).Diffuse = color;
        RequireReference(_ringMat).Diffuse = color;
        EntityBase::GetDrawInfo();
    }

    Matrix4 NodeDefenseEntity::GetModelTransform(ModelInstance& inst, std::int32_t index)
    {
        Matrix4 transform = EntityBase::GetModelTransform(inst, index);
        if (index == 1)
        {
            const Matrix4 rotY = CreateRotationY(DegreesToRadians(_curRotation));
            transform = MultiplyMatrix4(MultiplyMatrix4(_circleScale, rotY), transform);
            const Vector3 position = TypeExtensions::AddY(Position, 0.7F);
            transform.M41 = position.X;
            transform.M42 = position.Y;
            transform.M43 = position.Z;
        }
        return transform;
    }

    void NodeDefenseEntity::GetDisplayVolumes()
    {
        if (RequireReference(_scene).ShowVolumes() == VolumeDisplay::DefenseNode)
        {
            AddVolumeItem(_volume, Vector3(1.0F, 1.0F, 1.0F));
        }
    }
}

#include "ItemSpawnEntity.hpp"

#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Messaging.hpp"
#include "../Scene.hpp"
#include "../Sound/Sfx.hpp"
#include "../Utility/Rng.hpp"
#include "EnemySpawnEntity.hpp"
#include "ItemInstanceEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <any>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
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

    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw System::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::SceneDetail::InvalidCastException();
        }
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] Matrix4 Invert(Matrix4 value)
    {
        const float a = value.M11;
        const float b = value.M21;
        const float c = value.M31;
        const float d = value.M41;
        const float e = value.M12;
        const float f = value.M22;
        const float g = value.M32;
        const float h = value.M42;
        const float i = value.M13;
        const float j = value.M23;
        const float k = value.M33;
        const float l = value.M43;
        const float m = value.M14;
        const float n = value.M24;
        const float o = value.M34;
        const float p = value.M44;

        const float kpLo = k * p - l * o;
        const float jpLn = j * p - l * n;
        const float joKn = j * o - k * n;
        const float ipLm = i * p - l * m;
        const float ioKm = i * o - k * m;
        const float inJm = i * n - j * m;

        const float a11 = +(f * kpLo - g * jpLn + h * joKn);
        const float a12 = -(e * kpLo - g * ipLm + h * ioKm);
        const float a13 = +(e * jpLn - f * ipLm + h * inJm);
        const float a14 = -(e * joKn - f * ioKm + g * inJm);

        const float det = a * a11 + b * a12 + c * a13 + d * a14;
        if (std::abs(det) < std::numeric_limits<float>::denorm_min())
        {
            throw std::runtime_error("Matrix is singular and cannot be inverted.");
        }

        const float invDet = 1.0F / det;
        Matrix4 result{};
        result.M11 = a11 * invDet;
        result.M12 = a12 * invDet;
        result.M13 = a13 * invDet;
        result.M14 = a14 * invDet;
        result.M21 = -(b * kpLo - c * jpLn + d * joKn) * invDet;
        result.M22 = +(a * kpLo - c * ipLm + d * ioKm) * invDet;
        result.M23 = -(a * jpLn - b * ipLm + d * inJm) * invDet;
        result.M24 = +(a * joKn - b * ioKm + c * inJm) * invDet;

        const float gpHo = g * p - h * o;
        const float fpHn = f * p - h * n;
        const float foGn = f * o - g * n;
        const float epHm = e * p - h * m;
        const float eoGm = e * o - g * m;
        const float enFm = e * n - f * m;

        result.M31 = +(b * gpHo - c * fpHn + d * foGn) * invDet;
        result.M32 = -(a * gpHo - c * epHm + d * eoGm) * invDet;
        result.M33 = +(a * fpHn - b * epHm + d * enFm) * invDet;
        result.M34 = -(a * foGn - b * eoGm + c * enFm) * invDet;

        const float glHk = g * l - h * k;
        const float flHj = f * l - h * j;
        const float fkGj = f * k - g * j;
        const float elHi = e * l - h * i;
        const float ekGi = e * k - g * i;
        const float ejFi = e * j - f * i;

        result.M41 = -(b * glHk - c * flHj + d * fkGj) * invDet;
        result.M42 = +(a * glHk - c * elHi + d * ekGi) * invDet;
        result.M43 = -(a * flHj - b * elHi + d * ejFi) * invDet;
        result.M44 = +(a * fkGj - b * ekGi + c * ejFi) * invDet;
        return result;
    }
}

namespace MphRead::Entities
{
    ItemSpawnEntity::ItemSpawnEntity(
        ItemSpawnEntityData data,
        std::string nodeName,
        Scene* scene)
        : EntityBase(EntityType::ItemSpawn, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        Position = data.Header.Position.ToFloatVector();
        _alwaysActive = data.AlwaysActive != 0;
        if (GameState::Mode == GameMode::SinglePlayer)
        {
            StorySave* storySave = GameState::StorySave;
            const std::int32_t roomId = RequireReference(_scene).RoomId;
            if (storySave == nullptr)
            {
                throw System::NullReferenceException();
            }
            const std::int32_t state = storySave->InitRoomState(
                roomId,
                Id,
                data.Enabled != 0);
            if (_alwaysActive)
            {
                Active = data.Enabled != 0;
            }
            else
            {
                Active = state != 0;
            }
        }
        else
        {
            Active = data.Enabled != 0;
        }
        _spawnCooldown = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(data.SpawnDelay) * 2);
        if (data.HasBase != 0)
        {
            SetUpModel("items_base");
        }
        else
        {
            AddPlaceholderModel();
        }
    }

    ItemSpawnEntityData ItemSpawnEntity::Data() const
    {
        return _data;
    }

    bool ItemSpawnEntity::AlwaysActive() const noexcept
    {
        return _alwaysActive;
    }

    void ItemSpawnEntity::SetAlwaysActive(bool value) noexcept
    {
        _alwaysActive = value;
    }

    std::shared_ptr<ItemInstanceEntity> ItemSpawnEntity::Item() const noexcept
    {
        return _item;
    }

    void ItemSpawnEntity::SetItem(std::shared_ptr<ItemInstanceEntity> value) noexcept
    {
        _item = std::move(value);
    }

    std::shared_ptr<Formats::NodeData3> ItemSpawnEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void ItemSpawnEntity::SetClosestNode(
        std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    std::optional<OpenTK::Mathematics::Vector4> ItemSpawnEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void ItemSpawnEntity::Initialize()
    {
        EntityBase::Initialize();
        (void)RequireReference(_scene).TryGetEntity(
            _data.NotifyEntityId,
            _pickupNotifyEntity);
    }

    bool ItemSpawnEntity::Process()
    {
        if (!_linkDone && _data.ParentId != -1)
        {
            std::shared_ptr<EntityBase> parent{};
            if (RequireReference(_scene).TryGetEntity(_data.ParentId, parent))
            {
                _parent = std::move(parent);
            }
            if (_parent != nullptr)
            {
                _invPos = Matrix::Vec3MultMtx4(
                    Position,
                    Invert(_parent->CollisionTransform()));
            }
            _linkDone = true;
        }
        if (_linkDone && _parent != nullptr)
        {
            Position = Matrix::Vec3MultMtx4(
                _invPos,
                _parent->CollisionTransform());
        }
        if (!Active)
        {
            return true;
        }
        if (_item == nullptr && _spawnCooldown > 0)
        {
            --_spawnCooldown;
        }
        if (_item == nullptr
            && _spawnCooldown == 0
            && (_data.MaxSpawnCount == 0 || _spawnCount < _data.MaxSpawnCount))
        {
            _item = SpawnItem(
                _data.ItemType,
                TypeExtensions::AddY(Position, 0.65F),
                NodeRef,
                _scene);
            if (_item != nullptr)
            {
                _spawnCooldown = static_cast<std::uint16_t>(
                    static_cast<std::int32_t>(_data.SpawnInterval) * 2);
                ++_spawnCount;
                _item->SetOwner(this);
                _item->SetParentId(_data.ParentId);
                if (_data.ItemType != ItemType::ArtifactKey)
                {
                    _soundSource.Update(Position, 7);
                    UpdateNodeRefVolume();
                    _soundSource.PlaySfx(SfxId::ITEM_SPAWN1);
                }
                else if (_playKeySfx)
                {
                    (void)_soundSource.PlayFreeSfx(SfxId::KEY_APPEAR);
                }
                _playKeySfx = false;
            }
        }
        return EntityBase::Process();
    }

    void ItemSpawnEntity::OnItemPickedUp()
    {
        if (_data.CollectedMessage != Message::None)
        {
            Scene* scene = _scene;
            MessageObject param1 = BoxInt32(_data.CollectedMsgParam1);
            MessageObject param2 = BoxInt32(_data.CollectedMsgParam2);
            RequireReference(scene).SendMessage(
                _data.CollectedMessage,
                this,
                _pickupNotifyEntity.get(),
                std::move(param1),
                std::move(param2));
        }
    }

    void ItemSpawnEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate
            || (info.Message == Message::SetActive
                && UnboxInt32(info.Param1) != 0))
        {
            Active = true;
            _playKeySfx = true;
            if (GameState::Mode == GameMode::SinglePlayer)
            {
                StorySave* storySave = GameState::StorySave;
                const std::int32_t roomId = RequireReference(_scene).RoomId;
                if (storySave == nullptr)
                {
                    throw System::NullReferenceException();
                }
                storySave->SetRoomState(roomId, Id, 3);
            }
        }
        else if (info.Message == Message::SetActive
            && UnboxInt32(info.Param1) == 0)
        {
            Active = false;
            if (GameState::Mode == GameMode::SinglePlayer)
            {
                StorySave* storySave = GameState::StorySave;
                const std::int32_t roomId = RequireReference(_scene).RoomId;
                if (storySave == nullptr)
                {
                    throw System::NullReferenceException();
                }
                storySave->SetRoomState(roomId, Id, 1);
            }
            if (_item != nullptr)
            {
                _item->SetDespawnTimer(0);
                --_spawnCount;
            }
        }
        else if (info.Message == Message::MoveItemSpawner
            && info.Sender != nullptr)
        {
            if (info.Sender->Type == EntityType::EnemySpawn)
            {
                EnemySpawnEntity* enemySpawn
                    = dynamic_cast<EnemySpawnEntity*>(info.Sender);
                if (enemySpawn == nullptr)
                {
                    throw SceneDetail::InvalidCastException();
                }
                if (enemySpawn->Data.EnemyType == EnemyType::Hunter)
                {
                    auto enumerator = RequireReference(_scene).GetPlayerEntities().GetEnumerator();
                    while (enumerator.MoveNext())
                    {
                        std::shared_ptr<PlayerEntity> player = enumerator.Current();
                        PlayerEntity& playerRef = RequireReference(player);
                        if (playerRef.EnemySpawner == info.Sender)
                        {
                            Vector3 position{};
                            playerRef.GetPosition(position);
                            Position = position;
                            break;
                        }
                    }
                }
                else
                {
                    Vector3 position{};
                    info.Sender->GetPosition(position);
                    Position = position;
                }
            }
            else
            {
                Vector3 position{};
                info.Sender->GetPosition(position);
                Position = position;
            }
            if (_item != nullptr)
            {
                _item->Position = TypeExtensions::AddY(Position, 1.0F);
            }
        }
    }

    void ItemSpawnEntity::GetDrawInfo()
    {
        if (IsVisible(NodeRef))
        {
            EntityBase::GetDrawInfo();
        }
    }

    std::shared_ptr<ItemInstanceEntity> ItemSpawnEntity::SpawnItemDrop(
        ItemType type,
        Vector3 position,
        Formats::Culling::NodeRef nodeRef,
        std::uint32_t chance,
        Scene* scene)
    {
        return SpawnItem(
            type,
            position,
            nodeRef,
            scene,
            chance,
            450 * 2);
    }

    std::shared_ptr<ItemInstanceEntity> ItemSpawnEntity::SpawnItem(
        ItemType type,
        Vector3 position,
        Formats::Culling::NodeRef nodeRef,
        std::int32_t despawnTime,
        Scene* scene)
    {
        return SpawnItem(
            type,
            position,
            nodeRef,
            scene,
            std::nullopt,
            despawnTime);
    }

    std::shared_ptr<ItemInstanceEntity> ItemSpawnEntity::SpawnItem(
        ItemType type,
        Vector3 position,
        Formats::Culling::NodeRef nodeRef,
        Scene* scene,
        std::optional<std::uint32_t> chance,
        std::int32_t despawnTime)
    {
        std::shared_ptr<ItemInstanceEntity> item{};
        if (type != ItemType::None
            && (!chance.has_value()
                || Rng::GetRandomInt2(100) < chance.value()))
        {
            item = std::make_shared<ItemInstanceEntity>(
                ItemInstanceEntityData(position, type, despawnTime),
                nodeRef,
                scene);
            RequireReference(scene).AddEntity(item);
        }
        return item;
    }

    FhItemSpawnEntity::FhItemSpawnEntity(
        FhItemSpawnEntityData data,
        Scene* scene)
        : EntityBase(EntityType::FhItemSpawn, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(
            data.Header.FacingVector,
            data.Header.UpVector,
            data.Header.Position);
        AddPlaceholderModel();
    }

    std::optional<OpenTK::Mathematics::Vector4> FhItemSpawnEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    bool FhItemSpawnEntity::Process()
    {
        if (_spawn)
        {
            std::shared_ptr<FhItemEntity> item = SpawnItem(
                Position,
                _data.ItemType,
                _scene);
            RequireReference(_scene).AddEntity(item);
            _spawn = false;
        }
        return EntityBase::Process();
    }

    std::shared_ptr<FhItemEntity> FhItemSpawnEntity::SpawnItem(
        Vector3 position,
        FhItemType itemType,
        Scene* scene)
    {
        return std::make_shared<FhItemEntity>(
            FhItemInstanceEntityData(position, itemType),
            scene);
    }
}

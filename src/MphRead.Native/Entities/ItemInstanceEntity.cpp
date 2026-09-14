#include "ItemInstanceEntity.hpp"

#include "../Formats/Effects.hpp"
#include "../GameState.hpp"
#include "../MemoryArrays.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Scene.hpp"
#include "ItemSpawnEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace
{
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;

    constexpr Vector3 UnitX(1.0F, 0.0F, 0.0F);
    constexpr Vector3 UnitY(0.0F, 1.0F, 0.0F);
    constexpr Vector3 UnitZ(0.0F, 0.0F, 1.0F);
    constexpr Vector3 One(1.0F, 1.0F, 1.0F);

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

    template <typename T, std::size_t Size>
    [[nodiscard]] const T& GetListItem(
        const std::array<T, Size>& values,
        std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Size)
        {
            throw MphRead::Memory::Detail::ArgumentOutOfRangeException();
        }
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] Matrix4 CreateScale(Vector3 scale) noexcept
    {
        return Matrix4(
            OpenTK::Mathematics::Vector4(scale.X, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, scale.Y, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, scale.Z, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F));
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

    [[nodiscard]] Vector3 Scale(Vector3 value, float scale) noexcept
    {
        return Vector3(
            value.X * scale,
            value.Y * scale,
            value.Z * scale);
    }

    [[nodiscard]] float LengthSquared(Vector3 value) noexcept
    {
        return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
    }

    [[nodiscard]] float DegreesToRadians(float degrees) noexcept
    {
        return degrees * (std::numbers::pi_v<float> / 180.0F);
    }

}

namespace MphRead::Entities
{
    const std::array<std::int32_t, 22> ItemInstanceEntity::_scanIds{
        9, 11, 12, 0, 10, 21, 13, 23, 22, 18, 20, 19, 28, 14, 15, 16, 17, 0, 24, 463, 0, 0
    };

    const std::array<std::int32_t, 22> ItemInstanceEntity::_sfxIds{
        33, 33, 33, -1, 34, -1, 34, -1, -1, -1, -1, -1, -1, 33, 33, 33, 33, -1, -1, -1, -1, -1
    };

    std::uint16_t SpinningEntityBase::_nextItemRotation = 0;

    ItemInstanceEntityData& ItemInstanceEntityData::operator=(
        const ItemInstanceEntityData& other) noexcept
    {
        if (this != &other)
        {
            this->~ItemInstanceEntityData();
            ::new (this) ItemInstanceEntityData(other);
        }
        return *this;
    }

    SpinningEntityBase::SpinningEntityBase(
        float spinSpeed,
        Vector3 spinAxis,
        EntityType type,
        Scene* scene)
        : EntityBase(type, scene),
          _spin(GetItemRotation()),
          _spinSpeed(spinSpeed),
          _spinAxis(spinAxis),
          _spinModelIndex(-1),
          _floatModelIndex(-1)
    {
    }

    SpinningEntityBase::SpinningEntityBase(
        float spinSpeed,
        Vector3 spinAxis,
        std::int32_t spinModelIndex,
        EntityType type,
        Scene* scene)
        : EntityBase(type, scene),
          _spin(GetItemRotation()),
          _spinSpeed(spinSpeed),
          _spinAxis(spinAxis),
          _spinModelIndex(spinModelIndex),
          _floatModelIndex(-1)
    {
    }

    SpinningEntityBase::SpinningEntityBase(
        float spinSpeed,
        Vector3 spinAxis,
        std::int32_t spinModelIndex,
        std::int32_t floatModelIndex,
        EntityType type,
        Scene* scene)
        : EntityBase(type, scene),
          _spin(GetItemRotation()),
          _spinSpeed(spinSpeed),
          _spinAxis(spinAxis),
          _spinModelIndex(spinModelIndex),
          _floatModelIndex(floatModelIndex)
    {
    }

    SpinningEntityBase::SpinningEntityBase(
        float spinSpeed,
        Vector3 spinAxis,
        std::int32_t spinModelIndex,
        std::int32_t floatModelIndex,
        EntityType type,
        Formats::Culling::NodeRef nodeRef,
        Scene* scene)
        : EntityBase(type, nodeRef, scene),
          _spin(GetItemRotation()),
          _spinSpeed(spinSpeed),
          _spinAxis(spinAxis),
          _spinModelIndex(spinModelIndex),
          _floatModelIndex(floatModelIndex)
    {
    }

    SpinningEntityBase::~SpinningEntityBase() = default;

    bool SpinningEntityBase::Process()
    {
        _spin = std::fmod(
            static_cast<float>(
                _spin
                + RequireReference(_scene).FrameTime * 360.0F * _spinSpeed),
            360.0F);
        return EntityBase::Process();
    }

    Matrix4 SpinningEntityBase::GetModelTransform(
        ModelInstance& inst,
        std::int32_t index)
    {
        const std::shared_ptr<Model> model = inst.Model();
        Matrix4 transform = CreateScale(RequireReference(model).Scale);
        if (index == _spinModelIndex)
        {
            const Vector3 rotation(
                DegreesToRadians(_spinAxis.X * _spin),
                DegreesToRadians(_spinAxis.Y * _spin),
                DegreesToRadians(_spinAxis.Z * _spin));
            transform = Matrix::Multiply44(
                transform,
                Matrix::GetTransformSRT(One, rotation, Vector3::Zero));
        }
        transform = Matrix::Multiply44(transform, _transform);
        if (index == _floatModelIndex)
        {
            transform.M42 += (
                std::sin(_spin / 180.0F * std::numbers::pi_v<float>) + 1.0F)
                / 8.0F;
        }
        return transform;
    }

    float SpinningEntityBase::GetItemRotation() noexcept
    {
        const float rotation
            = static_cast<float>(_nextItemRotation)
            / static_cast<float>(0x10000)
            * 360.0F;
        _nextItemRotation = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(_nextItemRotation) + 0x2000U);
        return rotation;
    }

    ItemInstanceEntity::ItemInstanceEntity(
        ItemInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef,
        Scene* scene)
        : SpinningEntityBase(
              0.35F,
              UnitY,
              0,
              0,
              EntityType::ItemInstance,
              nodeRef,
              scene)
    {
        Position = data.Position;
        _itemType = data.ItemType;
        _scanId = GetListItem(
            _scanIds,
            static_cast<std::int32_t>(data.ItemType));
        if (GameState::Mode != GameMode::SinglePlayer
            && GameState::AffinityWeapons
            && (_itemType == MphRead::ItemType::VoltDriver
                || _itemType == MphRead::ItemType::Battlehammer
                || _itemType == MphRead::ItemType::Imperialist
                || _itemType == MphRead::ItemType::Judicator
                || _itemType == MphRead::ItemType::Magmaul
                || _itemType == MphRead::ItemType::ShockCoil))
        {
            _itemType = MphRead::ItemType::AffinityWeapon;
        }
        SetUpModel(GetListItem(
            Metadata::Items,
            static_cast<std::int32_t>(_itemType)));
        if (data.DespawnTimer > 0)
        {
            _despawnTimer = data.DespawnTimer;
        }
    }

    MphRead::ItemType ItemInstanceEntity::ItemType() const noexcept
    {
        return _itemType;
    }

    std::int32_t ItemInstanceEntity::ParentId() const noexcept
    {
        return _parentId;
    }

    void ItemInstanceEntity::SetParentId(std::int32_t value) noexcept
    {
        _parentId = value;
    }

    std::int32_t ItemInstanceEntity::DespawnTimer() const noexcept
    {
        return _despawnTimer;
    }

    void ItemInstanceEntity::SetDespawnTimer(std::int32_t value) noexcept
    {
        _despawnTimer = value;
    }

    ItemSpawnEntity* ItemInstanceEntity::Owner() const noexcept
    {
        return _owner;
    }

    void ItemInstanceEntity::SetOwner(ItemSpawnEntity* value) noexcept
    {
        _owner = value;
    }

    std::shared_ptr<Formats::NodeData3> ItemInstanceEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void ItemInstanceEntity::SetClosestNode(
        std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    void ItemInstanceEntity::Initialize()
    {
        SpinningEntityBase::Initialize();
        if (_itemType == MphRead::ItemType::ArtifactKey)
        {
            const Matrix4 transform = Matrix::GetTransform4(
                UnitX,
                UnitY,
                Position);
            _effectEntry = RequireReference(_scene).SpawnEffectGetEntry(
                144,
                transform);
            if (_effectEntry != nullptr)
            {
                _effectEntry->SetElementExtension(true);
            }
        }
    }

    void ItemInstanceEntity::GetVectors(
        Vector3& position,
        Vector3& up,
        Vector3& facing)
    {
        position = Position;
        up = UnitY;
        facing = UnitZ;
    }

    bool ItemInstanceEntity::Process()
    {
        if (!_linkDone && _parentId != -1)
        {
            std::shared_ptr<EntityBase> parent{};
            if (RequireReference(_scene).TryGetEntity(_parentId, parent))
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

        _soundSource.Update(Position, 7);
        UpdateNodeRefVolume();

        if (_effectEntry != nullptr)
        {
            const Matrix4 transform = GetTransformMatrix(
                UnitX,
                UnitY,
                Position);
            _effectEntry->Transform(Position, transform);
        }

        if (_despawnTimer > 0)
        {
            --_despawnTimer;
        }
        if (_despawnTimer == 0)
        {
            if (_owner != nullptr)
            {
                _owner->SetItem(nullptr);
                if (GameState::Mode == GameMode::SinglePlayer)
                {
                    auto storySave = GameState::StorySave;
                    const std::int32_t roomId = RequireReference(_scene).RoomId;
                    const std::int32_t ownerId = _owner->Id;
                    RequireReference(storySave).SetRoomState(
                        roomId,
                        ownerId,
                        1);
                    if (!_owner->AlwaysActive())
                    {
                        _owner->Active = false;
                    }
                }
            }
            if (_effectEntry != nullptr)
            {
                RequireReference(_scene).DetachEffectEntry(
                    _effectEntry,
                    false);
                _effectEntry.reset();
            }
            return false;
        }

        const std::int32_t sfx = GetListItem(
            _sfxIds,
            static_cast<std::int32_t>(_itemType));
        if (sfx != -1)
        {
            _soundSource.PlaySfx(sfx, true);
        }

        if (_owner == nullptr
            && GameState::Mode == GameMode::SinglePlayer)
        {
            auto&& conditionMainValue = PlayerEntity::Main();
            PlayerEntity& conditionMain = RequireReference(conditionMainValue);
            auto&& conditionEquipValue = conditionMain.EquipInfo();
            EquipInfo& conditionEquip = RequireReference(conditionEquipValue);
            auto&& conditionWeaponValue = conditionEquip.Weapon();
            if (conditionWeaponValue != nullptr)
            {
                auto&& mainValue = PlayerEntity::Main();
                PlayerEntity& main = RequireReference(mainValue);
                auto&& equipValue = main.EquipInfo();
                EquipInfo& equip = RequireReference(equipValue);

                const auto chargeLevel = equip.ChargeLevel();
                auto&& weaponValue = equip.Weapon();
                WeaponInfo& weapon = RequireReference(weaponValue);
                if (chargeLevel >= weapon.MinCharge * 2)
                {
                    auto&& positionMainValue = PlayerEntity::Main();
                    PlayerEntity& positionMain = RequireReference(positionMainValue);
                    const Vector3 between
                        = static_cast<Vector3>(positionMain.Position)
                        - static_cast<Vector3>(Position);
                    const float distSqr = LengthSquared(between);
                    if (distSqr > 0.0F && distSqr < 20.0F * 20.0F)
                    {
                        const float distance = std::sqrt(distSqr);
                        const float div = distance / 20.0F;
                        const float pct = (1.0F - div) / distance;
                        Position = static_cast<Vector3>(Position)
                            + Scale(
                                between,
                                pct / static_cast<float>(4 * 2));
                    }
                }
            }
        }

        return SpinningEntityBase::Process();
    }

    void ItemInstanceEntity::OnPickedUp()
    {
        _despawnTimer = 0;
        if (_owner != nullptr)
        {
            _owner->OnItemPickedUp();
        }
        if (GameState::Mode == GameMode::SinglePlayer)
        {
            const std::int32_t scanId = GetScanId();
            auto storySave = GameState::StorySave;
            RequireReference(storySave).UpdateLogbook(scanId);
        }
    }

    void ItemInstanceEntity::GetDrawInfo()
    {
        if (IsVisible(NodeRef))
        {
            SpinningEntityBase::GetDrawInfo();
        }
    }

    void ItemInstanceEntity::Destroy()
    {
        if (_effectEntry != nullptr)
        {
            RequireReference(_scene).UnlinkEffectEntry(_effectEntry);
        }
        _soundSource.StopAllSfx(true);
        SpinningEntityBase::Destroy();
    }

    FhItemInstanceEntityData& FhItemInstanceEntityData::operator=(
        const FhItemInstanceEntityData& other) noexcept
    {
        if (this != &other)
        {
            this->~FhItemInstanceEntityData();
            ::new (this) FhItemInstanceEntityData(other);
        }
        return *this;
    }

    FhItemEntity::FhItemEntity(
        FhItemInstanceEntityData data,
        Scene* scene)
        : SpinningEntityBase(
              0.35F,
              UnitY,
              0,
              0,
              EntityType::FhItemInstance,
              scene)
    {
        Vector3 position = data.Position;
        position.Y += 0.5F;
        Position = position;
        SetUpModel(
            GetListItem(
                Metadata::FhItems,
                static_cast<std::int32_t>(data.ItemType)),
            0,
            AnimFlags::None,
            true);
    }
}

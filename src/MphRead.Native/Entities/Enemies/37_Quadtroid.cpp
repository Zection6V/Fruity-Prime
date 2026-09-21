#include "37_Quadtroid.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../BeamProjectileEntity.hpp"
#include "../BombEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
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

        template <typename T>
        [[nodiscard]] const T& RequireReference(const std::shared_ptr<const T>& value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }

        template <typename T>
        [[nodiscard]] T& CastReference(EntityBase* value)
        {
            T* cast = dynamic_cast<T*>(value);
            if (cast == nullptr)
            {
                throw SceneDetail::InvalidCastException();
            }
            return *cast;
        }

        [[nodiscard]] EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            return RequireReference(PlayerEntity::Main());
        }

        [[nodiscard]] constexpr Vector3 ScaleVector(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
        }

        [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
        }

        [[nodiscard]] constexpr Vector3 AddY(Vector3 value, float amount) noexcept
        {
            value.Y += amount;
            return value;
        }

        [[nodiscard]] constexpr Vector3 WithY(Vector3 value, float y) noexcept
        {
            value.Y = y;
            return value;
        }

        [[nodiscard]] constexpr bool Equal(Vector3 left, Vector3 right) noexcept
        {
            return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
        }

        [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] std::int32_t AddInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t SubtractInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left) - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t MultiplyInt32Unchecked(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = std::bit_cast<std::uint32_t>(left) * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t UInt32ToInt32Unchecked(std::uint32_t value) noexcept
        {
            return std::bit_cast<std::int32_t>(value);
        }

        [[nodiscard]] AnimationInfo& RequireAnimInfo(ModelInstance& model)
        {
            return RequireReference(model.AnimInfo);
        }

        [[nodiscard]] Model& RequireModel(ModelInstance& model)
        {
            return RequireReference(model.Model());
        }

        [[nodiscard]] const std::vector<std::shared_ptr<Material>>& RequireMaterials(
            ModelInstance& model)
        {
            return RequireReference(RequireModel(model).Materials);
        }

        [[nodiscard]] std::int32_t RandomTimer(std::int32_t limit,
            std::int32_t add, std::int32_t multiplier)
        {
            const std::int32_t random = UInt32ToInt32Unchecked(Rng::GetRandomInt2(limit));
            return MultiplyInt32Unchecked(AddInt32Unchecked(random, add), multiplier);
        }
    }

    Enemy37Entity::Enemy37Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
    }

    void Enemy37Entity::EnemyInitialize()
    {
        Flags |= EnemyFlags::Visible;
        _state1 = _state2 = 1;
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        Vector3 up = spawner.Data.Header.UpVector.ToFloatVector();
        Vector3 facing = spawner.Data.Header.FacingVector.ToFloatVector();
        Vector3 position = spawner.Data.Header.Position.ToFloatVector();
        facing = RotateVectorRandom(facing, up);
        SetTransform(facing, up.Normalized(), position);
        _prevPos = position;
        _boundingRadius = 0.25F;
        _health = _healthMax = 120;
        _prevHealth = _health;
        _hurtVolumeInit = CollisionVolume(spawner.Data.Fields.S00().Volume0);
        _volume1 = CollisionVolume::Move(spawner.Data.Fields.S00().Volume1, Position);
        _rightVector = Vector3::Cross(facing, up).Normalized();
        _field238 = RandomTimer(60, 45, 2);
        _field23C = RandomTimer(60, 90, 2);
        ModelInstance& inst = SetUpModel(Metadata::EnemyModelNames.at(37));
        inst.SetAnimation(4, 0, SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        inst.SetAnimation(6, 1, SetFlags::Texcoord);
    }

    void Enemy37Entity::EnemyProcess()
    {
        if (_state1 == 0)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            Func214E750();
            Func214E668(Fixed::ToFloat(218));
            Func214DC90();
            Func214EF68([this]() { Func214EDD0(); });
        }
        else if (_state1 == 1)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            if (!Func214E5EC() && !Func214E58C())
            {
                Func214E668(Fixed::ToFloat(218));
                Func214DC90();
                PlayerEntity& player1 = MainPlayer();
                if (Func214D6E0(&player1))
                {
                    PlayerEntity& player2 = MainPlayer();
                    if (player2.AttachedEnemy() == nullptr)
                    {
                        PlayerEntity& player3 = MainPlayer();
                        if (Func214D690(&player3))
                        {
                            PlayerEntity& player4 = MainPlayer();
                            if (Func214D828(&player4))
                            {
                                _target = &MainPlayer();
                                Func214E1C0(_target);
                                Func214EC08();
                            }
                        }
                    }
                }
                Func214EF68([this]() { Func214EDD0(); });
            }
        }
        else if (_state1 == 2)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214D954();
            if (Func214E110())
            {
                Func214ED20();
            }
            Func214EF68([this]() { Func214ED78(); });
        }
        else if (_state1 == 3)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            if (!Func214E5EC())
            {
                Func214E668(Fixed::ToFloat(218));
                if (CheckInVolume())
                {
                    Func214EDD0();
                }
                Func214EF68([this]() { Func214ED20(); });
            }
        }
        else if (_state1 == 4)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214E708([this]() { Func214EC60(); });
            Func214EF68([this]() { Func214EDD0(); });
        }
        else if (_state1 == 5)
        {
            Func214DCB8();
            UpdateCollision();
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214E708([this]() { Func214EDD0(); });
            Func214EF68([this]() { Func214EDD0(); });
        }
        else if (_state1 == 6)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214D954();
            if (Func214E110())
            {
                Func214EDD0();
            }
            Func214EF68([this]() { Func214EC60(); });
        }
        else if (_state1 == 7)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214D954();
            if (Func214E110())
            {
                Func214EB84();
            }
            Func214EF68([this]() { Func214EC60(); });
        }
        else if (_state1 == 8)
        {
            _soundSource.PlaySfx(SfxId::DRIPSTANK_IDLE, true);
            Func214DCB8();
            UpdateCollision();
            Func214E668(Fixed::ToFloat(654));
            Func214DC90();
            Func214E4D8(_target);
            if (!Func214D5B8([this](PlayerEntity* player) { return Func214D828(player); })
                && Func214D65C(_target)
                && (_flags & QuadtroidFlags::Bit6) == QuadtroidFlags::Bit6)
            {
                Func214EB08();
            }
            Func214EEC4();
        }
        else if (_state1 == 9)
        {
            _hitByBeam = false;
            _hitByBomb = false;
            Func214DCB8();
            UpdateCollision();
            Func214E4D8(_target);
            _speed = ScaleVector(UpVector(), Fixed::ToFloat(-307));
            _speed = Divide(_speed, 2.0F);
            Func214E708([this]() { Func214E9F4(); });
        }
        else if (_state1 == 10)
        {
            _hitByBeam = false;
            _hitByBomb = false;
            Func214DC90();
            if (Func214DF58())
            {
                Func214E4D8(_target);
            }
            if (Func214DAF8())
            {
                Func214E994();
            }
            if (Func214D5B8([this](PlayerEntity* player) { return Func214D828(player); }))
            {
                Func214DAB0(Vector3::UnitY);
            }
            else if (!Func214D65C(_target))
            {
                Func214EB84();
            }
            Func214E708([this]() { Func214EDD0(); });
        }
        else if (_state1 == 11)
        {
            assert(_target != nullptr);
            _hitByBeam = false;
            _hitByBomb = false;
            Func214D9F8();
            PlayerEntity& target = RequireReference(_target);
            if (TypeExtensions::TestFlag(target.Flags1(), PlayerFlags1::Morphing))
            {
                Func214E8D4();
            }
        }
        else if (_state1 == 12)
        {
            assert(_target != nullptr);
            _hitByBeam = false;
            _hitByBomb = false;
            PlayerEntity& target = RequireReference(_target);
            Position = ScaleVector(target.FacingVector(), Fixed::ToFloat(819))
                + static_cast<Vector3>(target.Position);
            Func214E708([this]() { Func214E82C(); });
        }
        else if (_state1 == 13 && _target != nullptr)
        {
            Func214D9F8();
            Func214D864(_target);
            PlayerEntity& target = RequireReference(_target);
            if (target.IsUnmorphing())
            {
                Func214E4D8(_target);
                Func214E7B8();
            }
            if (Func214EE28())
            {
                Vector3 facing = FacingVector();
                _speed = Vector3(0.0F, Fixed::ToFloat(218), 0.0F);
                _speed = _speed + ScaleVector(facing, Fixed::ToFloat(-364));
                _speed = Divide(_speed, 2.0F);
                _field224 = Vector3(0.0F, Fixed::ToFloat(-17), 0.0F);
                _field224 = _field224 + ScaleVector(facing, Fixed::ToFloat(5));
            }
        }
        else if (_state1 == 14)
        {
            _hitByBeam = false;
            _hitByBomb = false;
            Func214DE50();
            Func214E708([this]() { Func214E92C(); });
        }
        else if (_state1 == 15)
        {
            UpdateCollision();
            _hitByBeam = false;
            _hitByBomb = false;
            Func214E708([this]() { Func214E788(); });
        }
        else if (_state1 == 16)
        {
            UpdateCollision();
            _hitByBeam = false;
            _hitByBomb = false;
            Func214E708([this]() { Func214EDD0(); });
        }
        else if (_state1 == 17)
        {
            UpdateCollision();
            Func214E708([this]() { Func214EDD0(); });
            Func214EF68([this]() { Func214EDD0(); });
        }
        else if (_state1 == 18)
        {
            UpdateCollision();
            _speed = _speed + Divide(_field224, 4.0F);
            Func214E708([this]() { Func214EDD0(); });
            _hitByBeam = false;
            _hitByBomb = false;
        }
    }

    void Enemy37Entity::Func214EF68(std::function<void()> func)
    {
        if (_hitByBeam || _hitByBomb)
        {
            _hitByBeam = false;
            _hitByBomb = false;
            if (_damageTaken >= 25)
            {
                _models[0].SetAnimation(7, 0,
                    SetFlags::Texture | SetFlags::Material | SetFlags::Node,
                    AnimFlags::NoLoop);
                _state1 = _state2 = 17;
                _soundSource.StopSfx(SfxId::DRIPSTANK_IDLE);
                _speed = Vector3::Zero;
                Func214D9B0();
                _field234 = std::move(func);
            }
        }
    }

    void Enemy37Entity::Func214E708(std::function<void()> func)
    {
        ModelInstance& model = _models[0];
        AnimationInfo& animInfo = RequireAnimInfo(model);
        if ((RequireReference(animInfo.Flags)[0] & AnimFlags::Ended) == AnimFlags::Ended)
        {
            if (_field234)
            {
                std::function<void()> saved = _field234;
                saved();
                _field234 = {};
            }
            else
            {
                func();
            }
        }
    }

    bool Enemy37Entity::Func214D5B8(std::function<bool(PlayerEntity*)> func)
    {
        if (_target == nullptr)
        {
            return true;
        }
        PlayerEntity& target1 = RequireReference(_target);
        const bool targetDead = target1.Health() == 0;
        PlayerEntity& target2 = RequireReference(_target);
        const bool targetHasAttached = target2.AttachedEnemy() != nullptr;
        PlayerEntity& target3 = RequireReference(_target);
        const bool targetAttachedIsNotSelf = target3.AttachedEnemy().get() != this;
        PlayerEntity& target4 = RequireReference(_target);
        if (!func(&target4) || targetDead || (targetHasAttached && targetAttachedIsNotSelf))
        {
            Func214D9B0();
            Func214EDD0();
            return true;
        }
        return false;
    }

    void Enemy37Entity::Func214DCB8()
    {
        PlayerEntity& player = MainPlayer();
        const std::int32_t slotIndex = player.SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            Vector3 between = static_cast<Vector3>(player.Position)
                - static_cast<Vector3>(Position);
            player.SetSpeed(player.Speed() + Divide(between, 4.0F));
            player.TakeDamage(3, DamageFlags::None, std::nullopt, this);
        }
    }

    void Enemy37Entity::UpdateCollision()
    {
        _flags &= ~QuadtroidFlags::Bit6;
        if (HandleCollision(_field1D0, _field1DC, _rightVector, 0.2F))
        {
            _flags |= QuadtroidFlags::Bit6;
        }
    }

    bool Enemy37Entity::HandleCollision(
        Vector3& dest, Vector3 someVec, Vector3 right, float dist)
    {
        Vector3 up = UpVector();
        dest = Divide(up, 2.0F);
        Vector3 testVec = ScaleVector(up, dist);
        Vector3 testPos = static_cast<Vector3>(Position) + testVec;
        ManagedArray<Formats::CollisionResult> results(8);
        const std::int32_t count = Formats::CollisionDetection::CheckInRadius(
            testPos, _boundingRadius, 8, false, Formats::TestFlags::None, _scene, &results);
        if (count > 0)
        {
            for (std::int32_t i = 0; i < count; i = AddInt32Unchecked(i, 1))
            {
                const Formats::CollisionResult result = results[static_cast<std::size_t>(i)];
                float v37 = 0.0F;
                if (result.Field0 != 0)
                {
                    v37 = _boundingRadius - result.Field14;
                }
                else
                {
                    const Vector3 plane(result.Plane.X, result.Plane.Y, result.Plane.Z);
                    v37 = _boundingRadius + result.Plane.W - Vector3::Dot(testPos, plane);
                }
                const Vector3 plane(result.Plane.X, result.Plane.Y, result.Plane.Z);
                Vector3 posDelta = ScaleVector(plane, v37);
                if (Vector3::Dot(posDelta, _speed) <= 0.0F)
                {
                    if (Vector3::Dot(someVec, plane) < Fixed::ToFloat(4094))
                    {
                        dest = dest + plane;
                    }
                    testPos = testPos + posDelta;
                }
            }
            Vector3 newPosition = testPos - testVec;
            if (!Equal(dest, Vector3::Zero))
            {
                dest = dest.Normalized();
            }
            else
            {
                dest = someVec;
            }
            Vector3 newUp = dest - up;
            newUp = ScaleVector(newUp, Fixed::ToFloat(409)) + up;
            newUp = newUp.Normalized();
            Vector3 newFacing = Vector3::Cross(newUp, right).Normalized();
            SetTransform(newFacing, newUp, newPosition);
            return true;
        }
        return false;
    }

    void Enemy37Entity::Func214E750()
    {
        _field238 = SubtractInt32Unchecked(_field238, 1);
        if (_field238 <= 0)
        {
            Func214EDD0();
        }
    }

    void Enemy37Entity::Func214E668(float a2)
    {
        Vector3 up = UpVector();
        _speed = ScaleVector(up, Fixed::ToFloat(-307));
        const float dot = Vector3::Dot(_field1D0, up);
        if (dot >= Fixed::ToFloat(1731))
        {
            _speed = _speed + ScaleVector(FacingVector(), a2);
            if (dot >= Fixed::ToFloat(4094))
            {
                _field1DC = _field1D0;
            }
        }
        _speed = Divide(_speed, 2.0F);
    }

    void Enemy37Entity::Func214DC90()
    {
        if (!CheckInVolume())
        {
            Func214DBDC();
        }
    }

    bool Enemy37Entity::Func214E5EC()
    {
        _field238 = SubtractInt32Unchecked(_field238, 1);
        if (_field238 == 0)
        {
            _field238 = RandomTimer(60, 45, 2);
            _field1E8 = RotateVectorRandom(FacingVector(), UpVector());
            Func214E444(_field1E8, 4, 10, AnimFlags::NoLoop);
            return true;
        }
        return false;
    }

    bool Enemy37Entity::Func214E58C()
    {
        if (_field23C > 0)
        {
            _field23C = SubtractInt32Unchecked(_field23C, 1);
        }
        if (_field23C == 0)
        {
            _field23C = RandomTimer(60, 90, 2);
            Func214ECB8();
            return true;
        }
        return false;
    }

    bool Enemy37Entity::Func214D6E0(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        Vector3 facing = FacingVector();
        Vector3 between = static_cast<Vector3>(Position)
            - static_cast<Vector3>(playerRef.Position);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            Vector3 vec = Func204D518(between, UpVector());
            if (LengthSquared(vec) > 1.0F / 128.0F
                && Vector3::Dot(facing, vec.Normalized()) > -1.0F)
            {
                vec = Func204D518(between, _rightVector);
                if (LengthSquared(vec) > 1.0F / 128.0F
                    && Vector3::Dot(facing, vec.Normalized()) > -1.0F)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool Enemy37Entity::Func214D690(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        Formats::CollisionResult discard{};
        return playerRef.Health() > 0
            && Formats::CollisionDetection::CheckSphereOverlapVolume(
                &playerRef.Volume(), Position, 10.0F, discard);
    }

    bool Enemy37Entity::Func214D828(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        return playerRef.Health() > 0
            && _volume1.TestPoint(static_cast<Vector3>(playerRef.Position));
    }

    void Enemy37Entity::Func214EC08()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(12, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 7;
    }

    void Enemy37Entity::Func214D954()
    {
        if (std::fabs(Vector3::Dot(_field1E8, UpVector())) >= Fixed::ToFloat(71))
        {
            _field1E8 = Func204D518(_field1E8, UpVector()).Normalized();
        }
    }

    bool Enemy37Entity::Func214E110()
    {
        bool result = false;
        Vector3 up = UpVector();
        Vector3 facing = FacingVector();
        if (Vector3::Dot(_field1E8, facing) >= Fixed::ToFloat(4034))
        {
            facing = _field1E8;
            result = true;
        }
        else
        {
            const float angle = (_flags & QuadtroidFlags::Bit3) == QuadtroidFlags::Bit3
                ? -10.0F : 10.0F;
            facing = RotateVector(facing, up, angle).Normalized();
        }
        SetTransform(facing, up, Position);
        _rightVector = Vector3::Cross(facing, up).Normalized();
        return result;
    }

    void Enemy37Entity::Func214ED20()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(4, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 3;
    }

    void Enemy37Entity::Func214EDD0()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(4, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 1;
    }

    void Enemy37Entity::Func214EB84()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _flags &= ~QuadtroidFlags::Bit1;
        _flags &= ~QuadtroidFlags::Bit2;
        _models[0].SetAnimation(9, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 8;
        _field238 = RandomTimer(15, 30, 2);
    }

    void Enemy37Entity::Func214E4D8(PlayerEntity* player)
    {
        if (player != nullptr)
        {
            Vector3 up = UpVector();
            Vector3 between = static_cast<Vector3>(player->Position)
                - static_cast<Vector3>(Position);
            between = Func204D518(between, up);
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                Vector3 facing = between.Normalized();
                _rightVector = Vector3::Cross(facing, up).Normalized();
                SetTransform(facing, up, Position);
            }
        }
    }

    bool Enemy37Entity::Func214D65C(PlayerEntity* player)
    {
        if (player == nullptr)
        {
            return false;
        }
        Formats::CollisionResult discard{};
        return Formats::CollisionDetection::CheckSphereOverlapVolume(
            &player->Volume(), Position, 4.0F, discard);
    }

    void Enemy37Entity::Func214EB08()
    {
        _flags |= QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(11, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _state1 = _state2 = 9;
        _soundSource.StopSfx(SfxId::DRIPSTANK_IDLE);
        _speed = Vector3::Zero;
    }

    void Enemy37Entity::Func214EEC4()
    {
        if (_hitByBeam && _damageTaken >= 25)
        {
            _hitByBeam = false;
            _models[0].SetAnimation(7, 0,
                SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            _state1 = _state2 = 17;
            _soundSource.StopSfx(SfxId::DRIPSTANK_IDLE);
            _speed = Vector3::Zero;
            Func214D9B0();
        }
    }

    bool Enemy37Entity::Func214DF58()
    {
        if ((_flags & QuadtroidFlags::Bit1) == QuadtroidFlags::Bit1
            || (_flags & QuadtroidFlags::Bit2) == QuadtroidFlags::Bit2)
        {
            if ((_flags & QuadtroidFlags::Bit1) == QuadtroidFlags::Bit1)
            {
                const float angle = (_flags & QuadtroidFlags::Bit3) == QuadtroidFlags::Bit3
                    ? -10.0F : 10.0F;
                Vector3 facing = FacingVector();
                if (Vector3::Dot(_field1E8, facing) >= Fixed::ToFloat(4034))
                {
                    facing = _field1E8;
                    _flags &= ~QuadtroidFlags::Bit1;
                }
                else
                {
                    facing = RotateVector(facing, _field1B8, angle).Normalized();
                }
                Vector3 up = RotateVector(UpVector(), _field1B8, angle).Normalized();
                SetTransform(facing, up, Position);
            }
            if ((_flags & QuadtroidFlags::Bit2) == QuadtroidFlags::Bit2)
            {
                Vector3 facing = FacingVector();
                Vector3 up = UpVector();
                if (Vector3::Dot(up, Vector3::UnitY) >= Fixed::ToFloat(4034))
                {
                    up = Vector3::UnitY;
                    _flags &= ~QuadtroidFlags::Bit2;
                }
                else
                {
                    const float angle = Func214D500(up, Vector3::UnitY, facing)
                        ? -10.0F : 10.0F;
                    up = RotateVector(up, facing, angle).Normalized();
                }
                SetTransform(facing, up, Position);
            }
            _rightVector = Vector3::Cross(FacingVector(), UpVector()).Normalized();
            return false;
        }
        return true;
    }

    bool Enemy37Entity::Func214DAF8()
    {
        if (_target == nullptr)
        {
            return false;
        }
        PlayerEntity& target = RequireReference(_target);
        Vector3 targetPos;
        if (target.IsAltForm())
        {
            targetPos = AddY(static_cast<Vector3>(target.Position), 0.625F);
        }
        else
        {
            targetPos = AddY(RequireReference(target.CameraInfo()).Position, -0.5F);
        }
        Vector3 between = targetPos - static_cast<Vector3>(Position);
        if (LengthSquared(between) > 0.375F)
        {
            between = between.Normalized();
            Position = static_cast<Vector3>(Position) + ScaleVector(between, Fixed::ToFloat(872));
            return false;
        }
        return true;
    }

    void Enemy37Entity::Func214E994()
    {
        assert(_target != nullptr);
        _field238 = 8 * 2;
        _flags &= ~QuadtroidFlags::Bit1;
        _flags &= ~QuadtroidFlags::Bit2;
        PlayerEntity& target = RequireReference(_target);
        if (target.IsAltForm())
        {
            Func214E82C();
        }
        else
        {
            Func214E92C();
        }
    }

    void Enemy37Entity::Func214DAB0(Vector3 vec)
    {
        Vector3 up = vec.Normalized();
        Vector3 facing = Vector3::Cross(up, _rightVector).Normalized();
        SetTransform(facing, up, Position);
    }

    void Enemy37Entity::Func214D9F8()
    {
        assert(_target != nullptr);
        _soundSource.PlaySfx(SfxId::DRIPSTANK_ATTACK2, true);
        _field238 = SubtractInt32Unchecked(_field238, 1);
        if (_field238 < 0)
        {
            RequireReference(_target).TakeDamage(
                2, DamageFlags::NoDmgInvuln, std::nullopt, this);
            _field238 = 8 * 2;
        }
    }

    void Enemy37Entity::Func214E8D4()
    {
        _flags |= QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(5, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _state1 = _state2 = 12;
    }

    void Enemy37Entity::Func214D864(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        if (playerRef.IsAltForm())
        {
            Vector3 position = playerRef.Position;
            if (!Equal(playerRef.Speed(), Vector3::Zero))
            {
                position.Y += 0.4F;
                AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
                if (RequireReference(animInfo.Index)[0] != 12)
                {
                    _models[0].SetAnimation(12, 0,
                        SetFlags::Texture | SetFlags::Material | SetFlags::Node);
                }
            }
            else
            {
                position.Y -= 0.25F;
                AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
                if (RequireReference(animInfo.Index)[0] != 1)
                {
                    _models[0].SetAnimation(1, 0,
                        SetFlags::Texture | SetFlags::Material | SetFlags::Node);
                }
            }
            Position = position;
        }
    }

    void Enemy37Entity::Func214E7B8()
    {
        _flags |= QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(5, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node,
            AnimFlags::NoLoop | AnimFlags::Reverse);
        _state1 = _state2 = 14;
        _field224 = Position;
        Func214E4D8(_target);
    }

    bool Enemy37Entity::Func214EE28()
    {
        if (_hitByBomb)
        {
            _hitByBomb = false;
            _models[0].SetAnimation(8, 0,
                SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
            _state1 = _state2 = 18;
            _soundSource.StopSfx(SfxId::DRIPSTANK_IDLE);
            _speed = Vector3::Zero;
            _flags |= QuadtroidFlags::Bit7;
            Func214D9B0();
            return true;
        }
        return false;
    }

    void Enemy37Entity::Func214DE50()
    {
        assert(_target != nullptr);
        PlayerEntity& target = RequireReference(_target);
        Vector3 pos = RequireReference(target.CameraInfo()).Position;
        Vector3 targetFacing = target.FacingVector();
        Vector3 targetUp = target.UpVector();
        Vector3 targetRight = Vector3::Cross(targetUp, targetFacing).Normalized();
        pos = pos + ScaleVector(targetFacing, 0.74F);
        pos = pos + ScaleVector(targetUp, -1.1F);
        pos = pos + ScaleVector(targetRight, Fixed::ToFloat(97));
        Vector3 vec = pos - _field224;
        ModelInstance& bipedModel2 = RequireReference(target.BipedModel2());
        AnimationInfo& animInfo = RequireAnimInfo(bipedModel2);
        const float div = static_cast<float>(RequireReference(animInfo.Frame)[0])
            / static_cast<float>(RequireReference(animInfo.FrameCount)[0]);
        Position = ScaleVector(vec, div) + _field224;
    }

    void Enemy37Entity::Func214ED78()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(12, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 2;
    }

    void Enemy37Entity::Func214EC60()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(12, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 6;
    }

    void Enemy37Entity::Func214E9F4()
    {
        _field224 = Position;
        if (_target != nullptr)
        {
            Vector3 between = WithY(
                static_cast<Vector3>(_target->Position) - static_cast<Vector3>(Position), 0.0F);
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                Func214E314(between.Normalized());
            }
        }
        _models[0].SetAnimation(3, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _flags |= QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit6;
        _flags &= ~QuadtroidFlags::Bit0;
        _state1 = _state2 = 10;
        _speed = Vector3::Zero;
        _soundSource.PlaySfx(SfxId::DRIPSTANK_ATTACK1);
    }

    void Enemy37Entity::Func214E314(Vector3 vec)
    {
        Vector3 facing = FacingVector();
        _field1E8 = vec;
        _field1B8 = Vector3::Cross(_field1E8, facing);
        if (LengthSquared(_field1B8) > 1.0F / 128.0F)
        {
            _field1B8 = _field1B8.Normalized();
            if (facing.Y > 0.0F)
            {
                _field1B8 = ScaleVector(_field1B8, -1.0F);
            }
            _flags |= QuadtroidFlags::Bit1;
            _flags |= QuadtroidFlags::Bit2;
            _flags &= ~QuadtroidFlags::Bit3;
            _flags &= ~QuadtroidFlags::Bit4;
            if (Func214D500(facing, _field1E8, _field1B8))
            {
                _flags |= QuadtroidFlags::Bit3;
            }
            if (Func214D500(UpVector(), Vector3::UnitY, facing))
            {
                _flags |= QuadtroidFlags::Bit4;
            }
        }
    }

    void Enemy37Entity::Func214E82C()
    {
        assert(_target != nullptr);
        PlayerEntity& target = RequireReference(_target);
        std::shared_ptr<EnemyInstanceEntity> attachedEnemy{};
        auto enumerator = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EnemyInstanceEntity> current = enumerator.Current();
            if (current.get() == this)
            {
                attachedEnemy = current;
                break;
            }
        }
        if (!attachedEnemy)
        {
            throw SceneDetail::InvalidOperationException();
        }
        target.SetAttachedEnemy(std::move(attachedEnemy));
        _models[0].SetAnimation(1, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 13;
        Position = AddY(static_cast<Vector3>(target.Position), -0.25F);
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
    }

    void Enemy37Entity::Func214E92C()
    {
        assert(_target != nullptr);
        PlayerEntity& target = RequireReference(_target);
        std::shared_ptr<EnemyInstanceEntity> attachedEnemy{};
        auto enumerator = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EnemyInstanceEntity> current = enumerator.Current();
            if (current.get() == this)
            {
                attachedEnemy = current;
                break;
            }
        }
        if (!attachedEnemy)
        {
            throw SceneDetail::InvalidOperationException();
        }
        target.SetAttachedEnemy(std::move(attachedEnemy));
        _flags |= QuadtroidFlags::Bit7;
        _flags |= QuadtroidFlags::Bit0;
        _models[0].SetAnimation(0, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node);
        _state1 = _state2 = 11;
    }

    void Enemy37Entity::Func214E788()
    {
        Func214DBDC();
        if (_state1 == 15)
        {
            _state1 = _state2 = 1;
        }
        Position = WithY(static_cast<Vector3>(Position), 0.625F);
    }

    void Enemy37Entity::Func214DBDC()
    {
        EnemySpawnEntity& spawner = RequireReference(_spawner);
        Vector3 toSpawn = (
            spawner.Data.Header.Position.ToFloatVector() - static_cast<Vector3>(Position)).Normalized();
        Vector3 vec = Func204D518(toSpawn, UpVector());
        if (LengthSquared(vec) > 1.0F / 128.0F)
        {
            Func214E444(vec.Normalized(), 2, 12, AnimFlags::None);
        }
    }

    void Enemy37Entity::Func214ECB8()
    {
        _flags &= ~QuadtroidFlags::Bit7;
        _flags &= ~QuadtroidFlags::Bit0;
        _models[0].SetAnimation(11, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node, AnimFlags::NoLoop);
        _state1 = _state2 = 5;
        _speed = Vector3::Zero;
    }

    bool Enemy37Entity::EnemyTakeDamage(EntityBase* source)
    {
        bool hitByNonSamusBomb = false;
        if (source != nullptr
            && (_flags & QuadtroidFlags::Bit7) != QuadtroidFlags::Bit7)
        {
            if (source->Type == EntityType::BeamProjectile)
            {
                _hitByBeam = true;
                if (CheckInVolume())
                {
                    Func214E1C0(source);
                }
            }
            else if (source->Type == EntityType::Bomb || source->Type == EntityType::Player)
            {
                _hitByBomb = true;
                Func214E1C0(&MainPlayer());
                if (source->Type == EntityType::Player)
                {
                    hitByNonSamusBomb = true;
                }
                else
                {
                    BombEntity& bomb = CastReference<BombEntity>(source);
                    PlayerEntity& owner = RequireReference(bomb.Owner());
                    if (owner.Hunter() != Hunter::Samus)
                    {
                        hitByNonSamusBomb = true;
                    }
                }
            }
        }
        if (_health == 0)
        {
            Func214D9B0();
        }
        if (_prevHealth > _health)
        {
            _damageTaken = static_cast<std::uint16_t>(
                static_cast<std::uint32_t>(_prevHealth) - static_cast<std::uint32_t>(_health));
            if (hitByNonSamusBomb && _damageTaken < 25)
            {
                _damageTaken = 25;
            }
        }
        _prevHealth = _health;
        return false;
    }

    void Enemy37Entity::UpdateAttached(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        if (playerRef.IsMorphing())
        {
            return;
        }
        Vector3 position = RequireReference(playerRef.CameraInfo()).Position;
        Vector3 facing = playerRef.FacingVector();
        position = position + ScaleVector(facing, 0.74F);
        facing = ScaleVector(facing, -1.0F);
        Vector3 up = playerRef.UpVector();
        position = position + ScaleVector(up, -1.1F);
        Vector3 right = Vector3::Cross(facing, up).Normalized();
        position = position + ScaleVector(right, Fixed::ToFloat(97));
        SetTransform(facing, up, position);
    }

    Vector3 Enemy37Entity::RotateVectorRandom(Vector3 vec, Vector3 axis)
    {
        const float angle = Fixed::ToFloat(Rng::GetRandomInt2(std::int32_t{359}));
        vec = RotateVector(vec, axis, angle);
        return Func204D518(vec, axis).Normalized();
    }

    Vector3 Enemy37Entity::Func204D518(Vector3 vec, Vector3 axis)
    {
        return vec - Func204D57C(vec, axis);
    }

    Vector3 Enemy37Entity::Func204D57C(Vector3 vec, Vector3 axis)
    {
        const float dot1 = Vector3::Dot(axis, axis);
        if (dot1 == 0.0F)
        {
            return Vector3::Zero;
        }
        const float dot2 = Vector3::Dot(vec, axis);
        return Divide(ScaleVector(axis, dot2), dot1);
    }

    void Enemy37Entity::Func214E1C0(EntityBase* entity)
    {
        EntityBase& entityRef = RequireReference(entity);
        Vector3 vec = Vector3::Zero;
        if (entityRef.Type == EntityType::Player)
        {
            vec = static_cast<Vector3>(entityRef.Position) - static_cast<Vector3>(Position);
        }
        else if (entityRef.Type == EntityType::BeamProjectile)
        {
            BeamProjectileEntity& beam = CastReference<BeamProjectileEntity>(&entityRef);
            vec = ScaleVector(beam.Direction(), -1.0F);
        }
        if (LengthSquared(vec) > 1.0F / 128.0F)
        {
            vec = Func204D518(vec, UpVector());
            if (LengthSquared(vec) > 1.0F / 128.0F)
            {
                vec = vec.Normalized();
                if (Vector3::Dot(FacingVector(), vec) < Fixed::ToFloat(4034))
                {
                    Func214E444(vec, 6, 12, AnimFlags::None);
                }
            }
        }
    }

    bool Enemy37Entity::CheckInVolume()
    {
        return _volume1.TestPoint(Position);
    }

    void Enemy37Entity::Func214D9B0()
    {
        _soundSource.StopSfx(SfxId::DRIPSTANK_ATTACK2);
        if (_target != nullptr)
        {
            _target->SetAttachedEnemy(nullptr);
            _target = nullptr;
        }
        _flags &= ~QuadtroidFlags::Bit0;
    }

    void Enemy37Entity::Func214E444(
        Vector3 vec, std::uint8_t state, std::int32_t animId, AnimFlags animFlags)
    {
        _flags &= ~QuadtroidFlags::Bit3;
        const bool result = Func214D500(FacingVector(), vec, UpVector());
        if (result)
        {
            _flags |= QuadtroidFlags::Bit3;
        }
        _field1E8 = vec;
        _state1 = _state2 = state;
        _speed = Vector3::Zero;
        _models[0].SetAnimation(animId, 0,
            SetFlags::Texture | SetFlags::Material | SetFlags::Node, animFlags);
    }

    bool Enemy37Entity::Func214D500(Vector3 vec1, Vector3 vec2, Vector3 vec3)
    {
        Vector3 cross = Vector3::Cross(vec2, vec1);
        if (LengthSquared(cross) > 1.0F / 128.0F
            && Vector3::Dot(cross.Normalized(), vec3) > 0.0F)
        {
            return true;
        }
        return false;
    }

    bool Enemy37Entity::EnemyGetDrawInfo()
    {
        ModelInstance& model = _models[0];
        const std::vector<std::shared_ptr<Material>>& materials = RequireMaterials(model);
        if (_state1 == 11)
        {
            for (std::size_t i = 0; i < materials.size(); ++i)
            {
                RequireReference(materials[i]).Lighting = 0;
            }
        }
        Matrix4 transform = Transform;
        Matrix4 scaleTransform = Transform;
        scaleTransform.M11 *= 1.5F;
        scaleTransform.M12 *= 1.5F;
        scaleTransform.M13 *= 1.5F;
        scaleTransform.M21 *= 1.5F;
        scaleTransform.M22 *= 1.5F;
        scaleTransform.M23 *= 1.5F;
        scaleTransform.M31 *= 1.5F;
        scaleTransform.M32 *= 1.5F;
        scaleTransform.M33 *= 1.5F;
        Transform = scaleTransform;
        DrawGeneric();
        Transform = transform;
        if (_state1 == 11)
        {
            for (std::size_t i = 0; i < materials.size(); ++i)
            {
                Material& material = RequireReference(materials[i]);
                material.Lighting = material.InitLighting;
            }
        }
        return true;
    }
}

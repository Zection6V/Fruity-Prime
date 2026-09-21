#include "02_Temroid.hpp"

#include "../../Formats/CollisionDetection.hpp"
#include "../../Formats/EntityEnemy.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Scene.hpp"
#include "../BombEntity.hpp"
#include "../DoorEntity.hpp"
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
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        template <typename TEnum>
        [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
        {
            using Underlying = std::underlying_type_t<TEnum>;
            return (static_cast<Underlying>(value) & static_cast<Underlying>(flag))
                == static_cast<Underlying>(flag);
        }

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

        EnemySpawnEntity* CastSpawner(EntityBase* spawner) noexcept
        {
            EnemySpawnEntity* typedSpawner = dynamic_cast<EnemySpawnEntity*>(spawner);
            assert(typedSpawner != nullptr);
            return typedSpawner;
        }

        [[nodiscard]] PlayerEntity& MainPlayer()
        {
            const std::shared_ptr<PlayerEntity> player = PlayerEntity::Main();
            if (!player)
            {
                throw System::NullReferenceException();
            }
            return *player;
        }

        [[nodiscard]] std::shared_ptr<EnemyInstanceEntity> GetManagedReference(
            Enemy02Entity* enemy, Scene* scene)
        {
            Scene& sceneRef = RequireReference(scene);
            auto enumerator = sceneRef.GetEnemyInstanceEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                std::shared_ptr<EnemyInstanceEntity> current = enumerator.Current();
                if (current.get() == enemy)
                {
                    return current;
                }
            }
            throw SceneDetail::InvalidOperationException();
        }

        [[nodiscard]] Enemy02Entity& RequireEnemy(Enemy02Entity* enemy)
        {
            if (enemy == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *enemy;
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

        [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
        {
            return value.X * value.X + value.Y * value.Y + value.Z * value.Z;
        }

        [[nodiscard]] std::int32_t UncheckedAddInt32(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] std::int32_t UncheckedSubtractInt32(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t bits
                = static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(bits);
        }

        [[nodiscard]] AnimationInfo& RequireAnimInfo(ModelInstance& model)
        {
            return RequireReference(model.AnimInfo);
        }

        [[nodiscard]] const Vector3& IdlePointAt(
            const std::array<Vector3, 4>& points, std::uint8_t index)
        {
            if (static_cast<std::size_t>(index) >= points.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return points[static_cast<std::size_t>(index)];
        }
    }
}

namespace MphRead::Metadata
{
    namespace
    {
        using Entities::EnemyBehavior;
        using Entities::EnemySubroutine;
        using Entities::Enemies::Enemy02Entity;

        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State0{
            {2, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior05)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State1{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior09)},
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior10)},
            {10, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior11)},
            {2, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior12)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State2{
            {3, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State3{
            {4, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State4{
            {5, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State5{
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior02)},
            {8, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior03)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State6{
            {9, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State7{
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior04)},
            {1, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior05)},
            {0, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior06)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State8{
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior07)},
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior02)},
            {6, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior08)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State9{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior00)}
        };
        const std::vector<EnemyBehavior<Enemy02Entity>> Enemy02State10{
            {7, static_cast<bool(*)(Enemy02Entity*)>(&Enemy02Entity::Behavior01)}
        };
    }

    std::vector<EnemySubroutine<Enemy02Entity>> Enemy02Subroutines{
        EnemySubroutine<Enemy02Entity>(Enemy02State0),
        EnemySubroutine<Enemy02Entity>(Enemy02State1),
        EnemySubroutine<Enemy02Entity>(Enemy02State2),
        EnemySubroutine<Enemy02Entity>(Enemy02State3),
        EnemySubroutine<Enemy02Entity>(Enemy02State4),
        EnemySubroutine<Enemy02Entity>(Enemy02State5),
        EnemySubroutine<Enemy02Entity>(Enemy02State6),
        EnemySubroutine<Enemy02Entity>(Enemy02State7),
        EnemySubroutine<Enemy02Entity>(Enemy02State8),
        EnemySubroutine<Enemy02Entity>(Enemy02State9),
        EnemySubroutine<Enemy02Entity>(Enemy02State10)
    };
}

namespace MphRead::Entities::Enemies
{
    Enemy02Entity::Enemy02Entity(EnemyInstanceEntityData data,
        Formats::Culling::NodeRef nodeRef, Scene* scene)
        : EnemyInstanceEntity(data, nodeRef, scene),
          _spawner(CastSpawner(data.Spawner))
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(11);
        (*processes)[0] = [this]() { State00(); };
        (*processes)[1] = [this]() { State01(); };
        (*processes)[2] = [this]() { State02(); };
        (*processes)[3] = [this]() { State03(); };
        (*processes)[4] = [this]() { State04(); };
        (*processes)[5] = [this]() { State05(); };
        (*processes)[6] = [this]() { State06(); };
        (*processes)[7] = [this]() { State07(); };
        (*processes)[8] = [this]() { State08(); };
        (*processes)[9] = [this]() { State09(); };
        (*processes)[10] = [this]() { State10(); };
        _stateProcesses = std::move(processes);
    }

    bool Enemy02Entity::Field1D0() const noexcept
    {
        return _field1D0;
    }

    void Enemy02Entity::EnemyInitialize()
    {
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::OnRadar;
        Flags &= ~EnemyFlags::CollidePlayer;

        EnemySpawnEntity& spawner = RequireReference(_spawner);
        const EnemySpawnFields03 fields = spawner.Data.Fields.S03();
        Vector3 facing = WithY(fields.Facing.ToFloatVector(), 0.0F).Normalized();
        const Vector3 up(0.0F, 1.0F, 0.0F);
        Vector3 position = spawner.Data.Header.Position.ToFloatVector()
            + AddY(fields.Position.ToFloatVector(), 0.5F);
        SetTransform(facing, up, position);
        _boundingRadius = 1.0F;
        _hurtVolumeInit = CollisionVolume(fields.Volume0);
        SetUpModel(Metadata::EnemyModelNames.at(2));
        _idlePoints[0] = static_cast<Vector3>(Position);
        _idlePoints[1] = static_cast<Vector3>(Position);
        _idlePoints[2] = static_cast<Vector3>(Position);
        _idlePoints[3] = static_cast<Vector3>(Position);
        _field1BC = static_cast<Vector3>(Position);
        const float idleX = fields.IdleRange.X.FloatValue();
        const float idleZ = fields.IdleRange.Z.FloatValue();
        facing = FacingVector();
        _idlePoints[1].X += facing.X * idleZ;
        _idlePoints[1].Z += facing.Z * idleZ;
        _idlePoints[2].X += facing.X * idleZ - facing.Z * idleX;
        _idlePoints[2].Z += facing.Z * idleZ + facing.X * idleX;
        _idlePoints[3].X -= facing.Z * idleX;
        _idlePoints[3].Z += facing.X * idleX;
        Func21648A4();
    }

    void Enemy02Entity::Func21648A4()
    {
        if ((_state1 == 0 || _state1 == 7 || _state1 == 10)
            && _state2 != 0 && _state2 != 7 && _state2 != 10)
        {
            std::int32_t count = 0;
            auto enumerator = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
            while (enumerator.MoveNext())
            {
                const std::shared_ptr<EnemyInstanceEntity> enemy = enumerator.Current();
                const std::shared_ptr<Enemy02Entity> temroid
                    = std::dynamic_pointer_cast<Enemy02Entity>(enemy);
                if (temroid && temroid->Field1D0())
                {
                    count = UncheckedAddInt32(count, 1);
                }
            }
            if (count >= 3)
            {
                _state2 = _state1;
                return;
            }
            _field1D0 = true;
        }
        else if (_state1 != 0 && _state1 != 7 && _state1 != 10
            && (_state2 == 0 || _state2 == 7 || _state2 == 10))
        {
            _field1D0 = false;
        }

        if (_state2 == 0)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(0);
            _speed = Vector3::Zero;
        }
        else if (_state2 == 1)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(1);
        }
        else if (_state2 == 2)
        {
            ModelInstance& model = _models[0];
            _soundSource.StopAllSfx();
            model.SetAnimation(10, AnimFlags::NoLoop);
            _speed = Vector3::Zero;
        }
        else if (_state2 == 3)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(11, AnimFlags::NoLoop);
            _field1B8 = 0.0F;
            _field1BC = static_cast<Vector3>(Position);
            PlayerEntity& player = MainPlayer();
            Vector3 playerDelta
                = static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position);
            Vector3 facing = AddY(playerDelta, 0.5F).Normalized();
            SetTransform(facing, UpVector(), Position);
            _speed = Divide(
                ScaleVector(WithY(ScaleVector(facing, 0.3F), 0.0F), -1.0F),
                2.0F);
        }
        else if (_state2 == 4)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(12, AnimFlags::NoLoop);
            PlayerEntity& player = MainPlayer();
            Vector3 playerDelta
                = static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position);
            Vector3 facing = AddY(playerDelta, 0.5F).Normalized();
            _speed = Divide(WithY(ScaleVector(facing, 0.3F), 0.0F), 2.0F);
        }
        else if (_state2 == 5)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(3);
            PlayerEntity& player = MainPlayer();
            Vector3 playerDelta
                = static_cast<Vector3>(player.Position) - static_cast<Vector3>(Position);
            Vector3 facing = AddY(playerDelta, 0.5F).Normalized();
            _speed = Divide(Divide(facing, 2.0F), 2.0F);
            _field170 = 20 * 2;
        }
        else if (_state2 == 6)
        {
            ModelInstance& model = _models[0];
            AnimationInfo& animInfo = RequireAnimInfo(model);
            if (RequireReference(animInfo.Index)[0] == 7)
            {
                model.SetAnimation(9, AnimFlags::NoLoop);
            }
            else
            {
                model.SetAnimation(8, AnimFlags::NoLoop);
            }
        }
        else if (_state2 == 7)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(0);
        }
        else if (_state2 == 8)
        {
            ModelInstance& model = _models[0];
            PlayerEntity& player = MainPlayer();
            Vector3 playerFacing = player.FacingVector();
            Vector3 facing{};
            if (player.IsAltForm())
            {
                model.SetAnimation(7);
                facing = ScaleVector(WithY(playerFacing, 0.0F), -1.0F);
                if (facing.X == 0.0F && facing.Z == 0.0F)
                {
                    facing.X = 1.0F;
                }
            }
            else
            {
                model.SetAnimation(4);
                facing = ScaleVector(playerFacing, -1.0F);
            }
            SetTransform(facing.Normalized(), UpVector(), Position);
            _speed = Vector3::Zero;
            _hitByBomb = false;
            _field170 = 150 * 2;
            _drainDamageTimer = 0;
        }
        else if (_state2 == 9)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(15, AnimFlags::NoLoop);
            _speed = Vector3::Zero;
        }
        else if (_state2 == 10)
        {
            ModelInstance& model = _models[0];
            model.SetAnimation(2);
            _speed = Vector3::Zero;
        }
    }

    bool Enemy02Entity::FloatEqual(float a, float b) noexcept
    {
        return std::fabs(a - b) >= 1.0F / 4096.0F;
    }

    void Enemy02Entity::Func216469C(Vector3 point1, Vector3 point2, float sign)
    {
        Vector3 between = (point1 - point2).Normalized();
        Vector3 facing = FacingVector();
        if (FloatEqual(facing.X, between.X)
            || FloatEqual(facing.Y, between.Y)
            || FloatEqual(facing.Z, between.Z))
        {
            Vector3 prevFacing = facing;
            facing.X += (between.X - facing.X) / 8.0F;
            facing.Z += (between.Z - facing.Z) / 8.0F;
            if (facing.X == 0.0F && facing.Z == 0.0F)
            {
                facing.X = prevFacing.X;
                facing.Z = prevFacing.Z;
            }
            facing.Y = between.Y;
            facing = facing.Normalized();
            if (FloatEqual(facing.X, prevFacing.X)
                && FloatEqual(facing.Z, prevFacing.Z))
            {
                facing.X += 0.125F;
                facing.Z -= 0.125F;
                if (facing.X == 0.0F && facing.Z == 0.0F)
                {
                    facing.X += 0.125F;
                    facing.Z -= 0.125F;
                }
                facing = facing.Normalized();
            }
            _speed = ScaleVector(Divide(ScaleVector(facing, 0.1F), 2.0F), sign);
            facing.Y = 0.0F;
            SetTransform(facing.Normalized(), UpVector(), Position);
        }
    }

    void Enemy02Entity::Detach()
    {
        if (MainPlayer().AttachedEnemy().get() == this)
        {
            MainPlayer().SetAttachedEnemy(nullptr);
        }
        _field1D0 = false;
    }

    void Enemy02Entity::UpdateAttached(PlayerEntity* player)
    {
        PlayerEntity& playerRef = RequireReference(player);
        Vector3 position = RequireReference(playerRef.CameraInfo()).Position
            + Divide(playerRef.FacingVector(), 2.0F);
        SetTransform(ScaleVector(playerRef.FacingVector(), -1.0F), UpVector(), position);
    }

    void Enemy02Entity::EnemyProcess()
    {
        CallStateProcess();
        if (_state1 != 8)
        {
            auto doorEnumerator = RequireReference(_scene).GetDoorEntities().GetEnumerator();
            while (doorEnumerator.MoveNext())
            {
                DoorEntity& door = RequireReference(doorEnumerator.Current());
                const Vector3 doorFacing = door.FacingVector();
                Vector3 between = static_cast<Vector3>(Position) - door.LockPosition();
                const float magSqr = LengthSquared(between);
                if (magSqr < door.RadiusSquared() + _boundingRadius)
                {
                    float dist = door.RadiusSquared() + _boundingRadius - magSqr;
                    if (dist > 0.0F)
                    {
                        if (Vector3::Dot(between, doorFacing) < 0.0F)
                        {
                            dist *= -1.0F;
                        }
                        Position = static_cast<Vector3>(Position) + ScaleVector(doorFacing, dist);
                        Vector3 speed = ScaleVector(_speed, 2.0F);
                        const float dot = -Vector3::Dot(speed, doorFacing);
                        if (dot > 0.0F)
                        {
                            speed = speed + Divide(ScaleVector(doorFacing, dot), 2.0F);
                            if (_state2 == 5)
                            {
                                _field170 = 0;
                            }
                        }
                        (void)speed;
                    }
                }
            }

            ManagedArray<Formats::CollisionResult> results(8);
            const std::int32_t colCount = Formats::CollisionDetection::CheckInRadius(
                Position, _boundingRadius, 8, false, Formats::TestFlags::None, _scene, &results);
            for (std::int32_t i = 0; i < colCount; i++)
            {
                const Formats::CollisionResult result = results[static_cast<std::size_t>(i)];
                float dist;
                if (result.Field0 == 0)
                {
                    dist = _boundingRadius + result.Plane.W
                        - Vector3::Dot(Position, result.Plane.Xyz());
                }
                else
                {
                    dist = _boundingRadius - result.Field14;
                }
                if (dist > 0.0F)
                {
                    Position = static_cast<Vector3>(Position) + ScaleVector(result.Plane.Xyz(), dist);
                    Vector3 speed = ScaleVector(_speed, 2.0F);
                    const float dot = -Vector3::Dot(speed, result.Plane.Xyz());
                    if (dot > 0.0F)
                    {
                        speed = speed + Divide(ScaleVector(result.Plane.Xyz(), dot), 2.0F);
                        if (_state2 == 5)
                        {
                            _field170 = 0;
                        }
                    }
                    (void)speed;
                }
            }
        }
    }

    bool Enemy02Entity::CheckTemroidHitByBomb(BombEntity* bomb)
    {
        if (TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Invincible))
        {
            return false;
        }
        BombEntity& bombRef = RequireReference(bomb);
        if (!TestFlag(bombRef.Flags(), BombFlags::Exploding)
            || TestFlag(bombRef.Flags(), BombFlags::Exploded))
        {
            return false;
        }
        Vector3 between = static_cast<Vector3>(Position)
            - static_cast<Vector3>(bombRef.Position);
        if (LengthSquared(between) > bombRef.Radius() * bombRef.Radius())
        {
            return false;
        }
        _hitByBomb = true;
        return true;
    }

    void Enemy02Entity::State00()
    {
        const Vector3& idlePoint = IdlePointAt(_idlePoints, _field1A4);
        Vector3 between = idlePoint - static_cast<Vector3>(Position);
        if (LengthSquared(between) < 1.0F)
        {
            ++_field1A4;
            if (_field1A4 >= 4)
            {
                _field1A4 = 0;
            }
        }
        Func216469C(
            IdlePointAt(_idlePoints, _field1A4),
            static_cast<Vector3>(Position),
            1.0F);
        State02();
    }

    void Enemy02Entity::State01()
    {
        Func216469C(
            static_cast<Vector3>(MainPlayer().Position),
            static_cast<Vector3>(Position),
            1.0F);
        State02();
    }

    void Enemy02Entity::State02()
    {
        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            MainPlayer().TakeDamage(15, DamageFlags::None, std::nullopt, this);
        }
        if (CallSubroutine<Enemy02Entity>(Metadata::Enemy02Subroutines, this))
        {
            Func21648A4();
        }
    }

    void Enemy02Entity::UpdateHeight(float sign)
    {
        AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
        const std::int32_t frameCount = RequireReference(animInfo.FrameCount)[0];
        _field1B8 += sign * (45.0F / static_cast<float>(frameCount) / 2.0F);
        if (_field1B8 >= 360.0F)
        {
            _field1B8 = 0.0F;
        }
        _speed.Y = _field1BC.Y + std::sin(_field1B8) / 2.0F - Position.Y;
    }

    void Enemy02Entity::State03()
    {
        UpdateHeight(1.0F);
        State02();
    }

    void Enemy02Entity::State04()
    {
        UpdateHeight(-1.0F);
        State02();
    }

    void Enemy02Entity::State05()
    {
        if (CallSubroutine<Enemy02Entity>(Metadata::Enemy02Subroutines, this))
        {
            Func21648A4();
        }
    }

    void Enemy02Entity::State06()
    {
        Func216469C(
            static_cast<Vector3>(Position),
            IdlePointAt(_idlePoints, 0),
            -1.0F);
        if (CallSubroutine<Enemy02Entity>(Metadata::Enemy02Subroutines, this))
        {
            Func21648A4();
        }
    }

    void Enemy02Entity::State07()
    {
        Func216469C(
            IdlePointAt(_idlePoints, 0),
            static_cast<Vector3>(Position),
            1.0F);
        State02();
    }

    void Enemy02Entity::State08()
    {
        ModelInstance& model = _models[0];
        AnimationInfo& modelAnimInfo = RequireAnimInfo(model);
        const std::int32_t animId = RequireReference(modelAnimInfo.Index)[0];
        Vector3 facing = FacingVector();
        const Vector3 playerPos = static_cast<Vector3>(MainPlayer().Position);
        const Vector3 playerFacing = MainPlayer().FacingVector();

        if (MainPlayer().IsMorphing())
        {
            if (facing.Y != 0.0F)
            {
                if (facing.X == 0.0F && facing.Z == 0.0F)
                {
                    facing.X = 1.0F;
                }
            }
            if (animId == 4 || animId == 14)
            {
                model.SetAnimation(13, AnimFlags::NoLoop);
            }
            SetTransform(facing.Normalized(), UpVector(), AddY(playerPos, 0.625F));
        }
        else if (MainPlayer().IsUnmorphing())
        {
            if (facing.Y == 0.0F)
            {
                facing.Y = -playerFacing.Y;
            }
            if (animId == 7 || animId == 13)
            {
                model.SetAnimation(14, AnimFlags::NoLoop);
            }
            AnimationInfo& animInfo = RequireAnimInfo(model);
            const std::int32_t frameCount = RequireReference(animInfo.FrameCount)[0];
            const std::int32_t animFrame = RequireReference(animInfo.Frame)[0];
            const Vector3 cameraPos = RequireReference(MainPlayer().CameraInfo()).Position;
            const std::int32_t frameDelta
                = UncheckedSubtractInt32(frameCount, animFrame);
            Vector3 position = Divide(
                ScaleVector(AddY(playerPos, 0.625F), static_cast<float>(frameDelta))
                    + ScaleVector(cameraPos + Divide(playerFacing, 2.0F),
                        static_cast<float>(animFrame)),
                static_cast<float>(frameCount));
            SetTransform(facing.Normalized(), UpVector(), position);
        }
        else if (MainPlayer().IsAltForm())
        {
            if (facing.Y != 0.0F)
            {
                if (facing.X == 0.0F && facing.Z == 0.0F)
                {
                    facing.X = 1.0F;
                }
            }
            AnimationInfo& animInfo = RequireAnimInfo(model);
            if (animId == 4
                || (animId == 13
                    && TestFlag(RequireReference(animInfo.Flags)[0], AnimFlags::Ended)))
            {
                model.SetAnimation(7);
            }
            SetTransform(facing.Normalized(), UpVector(), AddY(playerPos, 0.625F));
        }
        else
        {
            if (facing.Y == 0.0F)
            {
                facing.Y = -playerFacing.Y;
            }
            AnimationInfo& animInfo = RequireAnimInfo(model);
            if (animId == 7
                || (animId == 14
                    && TestFlag(RequireReference(animInfo.Flags)[0], AnimFlags::Ended)))
            {
                model.SetAnimation(4);
            }
            SetTransform(facing.Normalized(), UpVector(), Position);
        }

        if (_drainDamageTimer > 0)
        {
            _drainDamageTimer--;
        }
        else
        {
            MainPlayer().TakeDamage(
                2, DamageFlags::NoDmgInvuln, std::nullopt, this);
            _drainDamageTimer = 8 * 2;
        }
        if (CallSubroutine<Enemy02Entity>(Metadata::Enemy02Subroutines, this))
        {
            MainPlayer().SetAttachedEnemy(nullptr);
            Func21648A4();
        }
    }

    void Enemy02Entity::State09()
    {
        if (CallSubroutine<Enemy02Entity>(Metadata::Enemy02Subroutines, this))
        {
            Func21648A4();
        }
    }

    void Enemy02Entity::State10()
    {
        State02();
    }

    bool Enemy02Entity::Behavior00()
    {
        AnimationInfo& animInfo = RequireAnimInfo(_models[0]);
        return TestFlag(RequireReference(animInfo.Flags)[0], AnimFlags::Ended);
    }

    bool Enemy02Entity::Behavior01()
    {
        return MainPlayer().AttachedEnemy() == nullptr;
    }

    bool Enemy02Entity::Behavior02()
    {
        if (_field170 > 0)
        {
            _field170--;
            return false;
        }
        return true;
    }

    bool Enemy02Entity::Behavior03()
    {
        if (_health == 0)
        {
            return false;
        }
        const std::int32_t slotIndex = MainPlayer().SlotIndex();
        if (slotIndex < 0 || static_cast<std::size_t>(slotIndex) >= HitPlayers.size())
        {
            throw SceneDetail::IndexOutOfRangeException();
        }
        if (!HitPlayers[static_cast<std::size_t>(slotIndex)])
        {
            return false;
        }
        if (MainPlayer().AttachedEnemy() != nullptr)
        {
            return false;
        }
        MainPlayer().SetAttachedEnemy(GetManagedReference(this, _scene));
        return true;
    }

    bool Enemy02Entity::Behavior04()
    {
        return _timeSinceDamage == 1;
    }

    bool Enemy02Entity::Behavior05()
    {
        Formats::CollisionResult result{};
        if (Formats::CollisionDetection::CheckBetweenPoints(
            Position, MainPlayer().Position,
            Formats::TestFlags::None, _scene, result))
        {
            return false;
        }
        Vector3 between = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        return LengthSquared(between) < 6.5F * 6.5F;
    }

    bool Enemy02Entity::Behavior06()
    {
        Vector3 between = IdlePointAt(_idlePoints, 0) - static_cast<Vector3>(Position);
        return LengthSquared(between) < 1.0F;
    }

    bool Enemy02Entity::Behavior07()
    {
        if (!_hitByBomb)
        {
            return false;
        }
        _hitByBomb = false;
        return true;
    }

    bool Enemy02Entity::Behavior08()
    {
        return MainPlayer().Health() == 0;
    }

    bool Enemy02Entity::Behavior09()
    {
        Formats::CollisionResult result{};
        return Formats::CollisionDetection::CheckBetweenPoints(
            Position, MainPlayer().Position,
            Formats::TestFlags::None, _scene, result);
    }

    bool Enemy02Entity::Behavior10()
    {
        Vector3 between = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        return LengthSquared(between) < 100.0F;
    }

    bool Enemy02Entity::Behavior11()
    {
        return MainPlayer().AttachedEnemy() != nullptr;
    }

    bool Enemy02Entity::Behavior12()
    {
        Vector3 between = static_cast<Vector3>(MainPlayer().Position)
            - static_cast<Vector3>(Position);
        return LengthSquared(between) < 25.0F;
    }

    bool Enemy02Entity::Behavior00(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior00();
    }

    bool Enemy02Entity::Behavior01(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior01();
    }

    bool Enemy02Entity::Behavior02(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior02();
    }

    bool Enemy02Entity::Behavior03(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior03();
    }

    bool Enemy02Entity::Behavior04(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior04();
    }

    bool Enemy02Entity::Behavior05(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior05();
    }

    bool Enemy02Entity::Behavior06(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior06();
    }

    bool Enemy02Entity::Behavior07(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior07();
    }

    bool Enemy02Entity::Behavior08(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior08();
    }

    bool Enemy02Entity::Behavior09(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior09();
    }

    bool Enemy02Entity::Behavior10(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior10();
    }

    bool Enemy02Entity::Behavior11(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior11();
    }

    bool Enemy02Entity::Behavior12(Enemy02Entity* enemy)
    {
        return RequireEnemy(enemy).Behavior12();
    }
}

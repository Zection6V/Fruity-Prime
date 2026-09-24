#include "28_Gorea1B.hpp"
#include "29_GoreaSealSphere1.hpp"
#include "30_Trocra.hpp"
#include "../../Features.hpp"
#include "../../Formats/CollisionDetection.hpp"
#include "../../Metadata/Enemies.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../EnemySpawnEntity.hpp"
#include "../Players/PlayerEntity.hpp"
#include "../../Formats/Types.hpp"
#include <any>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ::OpenTK::Mathematics::CreateRotationY;
using ::OpenTK::Mathematics::Equal;
using ::OpenTK::Mathematics::IdentityMatrix;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::LengthSquared;
using ::OpenTK::Mathematics::MathHelper::DegreesToRadians;
using ::OpenTK::Mathematics::ScaleVector;

namespace MphRead::Entities::Enemies
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        using OpenTK::Mathematics::Vector4;
        template <typename T>
        [[nodiscard]] T &RequireReference(T *value)
        {
            if (value == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }
        template <typename T>
        [[nodiscard]] T &RequireReference(const std::shared_ptr<T> &value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }
        template <typename T>
        [[nodiscard]] const T &RequireReference(const std::shared_ptr<const T> &value)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            return *value;
        }
        template <typename T, std::size_t N>
        [[nodiscard]] T &ArrayAt(std::array<T, N> &values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
        template <typename T, std::size_t N>
        [[nodiscard]] const T &ArrayAt(const std::array<T, N> &values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= N)
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
        template <typename T>
        [[nodiscard]] T &VectorAt(std::vector<T> &values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
        template <typename T>
        [[nodiscard]] const T &VectorAt(const std::vector<T> &values, std::int32_t index)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return values[static_cast<std::size_t>(index)];
        }
        [[nodiscard]] Enemy28Entity &RequireEnemy(Enemy28Entity *enemy) { return RequireReference(enemy); }
        [[nodiscard]] PlayerEntity &MainPlayer() { return RequireReference(PlayerEntity::Main()); }
        [[nodiscard]] Vector3 DivideVector(Vector3 value, float divisor) noexcept { return Vector3(value.X / divisor, value.Y / divisor, value.Z / divisor); }
        [[nodiscard]] Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept
        {
            const float x = axis.X;
            const float y = axis.Y;
            const float z = axis.Z;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const float oneMinus = 1.0F - cosine;
            return Matrix4(Vector4(cosine + x * x * oneMinus, x * y * oneMinus + z * sine, x * z * oneMinus - y * sine, 0.0F), Vector4(y * x * oneMinus - z * sine, cosine + y * y * oneMinus, y * z * oneMinus + x * sine, 0.0F), Vector4(z * x * oneMinus + y * sine, z * y * oneMinus - x * sine, cosine + z * z * oneMinus, 0.0F), Vector4(0.0F, 0.0F, 0.0F, 1.0F));
        }
        [[nodiscard]] std::int32_t AddInt32Unchecked(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t sum = std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(sum);
        }
        [[nodiscard]] std::int32_t SubInt32Unchecked(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t difference = std::bit_cast<std::uint32_t>(left) - std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(difference);
        }
        [[nodiscard]] std::int32_t MulInt32Unchecked(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t product = std::bit_cast<std::uint32_t>(left) * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(product);
        }
        [[nodiscard]] std::int32_t UInt32ToInt32Unchecked(std::uint32_t value) noexcept { return std::bit_cast<std::int32_t>(value); }
        [[nodiscard]] std::int32_t RoundToInt32ToEven(float value) noexcept
        {
            if (!std::isfinite(value))
            {
                return value > 0.0F ? std::numeric_limits<std::int32_t>::max() : std::numeric_limits<std::int32_t>::min();
            }
            const float floorValue = std::floor(value);
            const float fraction = value - floorValue;
            double rounded = floorValue;
            if (fraction > 0.5F)
            {
                rounded = static_cast<double>(floorValue) + 1.0;
            }
            else if (fraction == 0.5F)
            {
                const auto floorInteger = static_cast<std::int64_t>(floorValue);
                rounded = (floorInteger & 1LL) == 0 ? floorValue : static_cast<double>(floorValue) + 1.0;
            }
            if (rounded >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            {
                return std::numeric_limits<std::int32_t>::max();
            }
            if (rounded <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return static_cast<std::int32_t>(rounded);
        }
    }
    Enemy28Entity::Enemy28Entity(EnemyInstanceEntityData data, Formats::Culling::NodeRef nodeRef, Scene *scene) : GoreaEnemyEntityBase(data, nodeRef, scene)
    {
        auto processes = std::make_shared<ManagedArray<std::function<void()>>>(14);
        (*processes)[0] = [this]()
        { State00(); };
        (*processes)[1] = [this]()
        { State01(); };
        (*processes)[2] = [this]()
        { State02(); };
        (*processes)[3] = [this]()
        { State03(); };
        (*processes)[4] = [this]()
        { State04(); };
        (*processes)[5] = [this]()
        { State05(); };
        (*processes)[6] = [this]()
        { State06(); };
        (*processes)[7] = [this]()
        { State07(); };
        (*processes)[8] = [this]()
        { State08(); };
        (*processes)[9] = [this]()
        { State09(); };
        (*processes)[10] = [this]()
        { State10(); };
        (*processes)[11] = [this]()
        { State11(); };
        (*processes)[12] = [this]()
        { State12(); };
        (*processes)[13] = [this]()
        { State13(); };
        _stateProcesses = std::move(processes);
    }
    void Enemy28Entity::EnemyInitialize()
    {
        if (Enemy24Entity *owner = dynamic_cast<Enemy24Entity *>(_owner))
        {
            _gorea1A = owner;
            InitializeCommon(owner->Spawner());
            Flags |= EnemyFlags::OnRadar;
            Flags |= EnemyFlags::Invincible;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            _scanId = 0;
            _state1 = _state2 = 0;
            const Vector3 ownerFacing = owner->FacingVector();
            const Vector3 ownerUp = UpVector();
            const Vector3 ownerPosition = owner->Position;
            SetTransform(ownerFacing, ownerUp, ownerPosition);
            _prevPos = Position;
            _hurtVolumeInit = CollisionVolume(Vector3::Zero, Fixed::ToFloat(4098));
            Scale = owner->Scale;
            _model = &SetUpModel("Gorea1B_lod0");
            _model->NodeAnimIgnoreRoot = true;
            RequireReference(_model->Model()).ComputeNodeMatrices(0);
            _model->SetAnimation(3, AnimFlags::NoLoop);
            _spineNode = RequireReference(_model->Model()).GetNodeByName("Spine_02");
            EnemySpawnEntity &spawner = RequireReference(owner->Spawner());
            _volume = CollisionVolume::Move(CollisionVolume(spawner.Data.Fields.S11().Sphere2Position.ToFloatVector(), spawner.Data.Fields.S11().Sphere2Radius.FloatValue()), Position);
            _damageTimer = MulInt32Unchecked(UInt32ToInt32Unchecked(Rng::GetRandomInt2(13) + 7U), 2);
            _swingTimer = 30 * 2;
            _phasesLeft = 3;
            if (Rng::GetRandomInt2(255) % 2U != 0U)
            {
                _goreaFlags |= Gorea1BFlags::Bit3;
            }
            std::shared_ptr<EnemyInstanceEntity> enemy = EnemySpawnEntity::SpawnEnemy(this, MphRead::EnemyType::GoreaSealSphere1, NodeRef, _scene);
            std::shared_ptr<Enemy29Entity> sealSphere = std::dynamic_pointer_cast<Enemy29Entity>(enemy);
            if (sealSphere)
            {
                RequireReference(_scene).AddEntity(sealSphere);
                _sealSphere = std::move(sealSphere);
            }
            _trickModel = &SetUpModel("goreaMindTrick");
            _grappleModel = &SetUpModel("goreaGrappleBeam");
            _trickModel->Active = false;
            _grappleModel->Active = false;
            _field21E = 120.0F * 2.0F;
            _field24 = 0.65F;
            _field28 = 1.0F / 3.0F;
            _field30 = 1.0F;
            _field34 = 0.25F;
            ResetMaterialColors();
        }
    }
    void Enemy28Entity::Activate()
    {
        _scanId = VectorAt(Metadata::EnemyScanIds, static_cast<std::int32_t>(EnemyType()));
        Flags |= EnemyFlags::Visible;
        Flags |= EnemyFlags::CollidePlayer;
        Flags |= EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::Invincible;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
        Flags |= EnemyFlags::OnRadar;
        _targetFacing = Vector3::Zero;
        _goreaFlags |= Gorea1BFlags::Bit4;
        RequireReference(_sealSphere).Activate();
        ActivateTrocraSpawns();
        UpdateMaterials();
        Enemy24Entity &gorea1A = RequireReference(_gorea1A);
        const Vector3 goreaFacing = gorea1A.FacingVector();
        const Vector3 goreaUp = UpVector();
        const Vector3 goreaPosition = gorea1A.Position;
        SetTransform(goreaFacing, goreaUp, goreaPosition);
    }
    void Enemy28Entity::ActivateTrocraSpawns()
    {
        std::int32_t count = _phasesLeft == 3 ? 1 : 2;
        auto enumerator = RequireReference(_scene).GetEnemySpawnEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EnemySpawnEntity> spawner = enumerator.Current();
            EnemySpawnEntity &spawnerRef = RequireReference(spawner);
            if (spawnerRef.Data.EnemyType == MphRead::EnemyType::Trocra && !TypeExtensions::TestFlag(static_cast<SpawnerFlags>(spawnerRef.Flags), SpawnerFlags::Active))
            {
                const MessageObject param1 = std::make_shared<const std::any>(std::int32_t(0));
                const MessageObject param2 = std::make_shared<const std::any>(std::int32_t(0));
                RequireReference(_scene).SendMessage(Message::Activate, this, spawner.get(), param1, param2);
                if (_phasesLeft != 1 && --count == 0)
                {
                    break;
                }
            }
        }
    }
    void Enemy28Entity::EnemyProcess()
    {
        if (TypeExtensions::TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            const bool flagSet = TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit0);
            if (!flagSet)
            {
                CheckPlayerCollision();
            }
            CallStateProcess();
            if (!flagSet)
            {
                TransformHurtVolumeToNode(_spineNode.get(), Vector3(0.0F, Fixed::ToFloat(970), Fixed::ToFloat(622)));
            }
        }
    }
    void Enemy28Entity::CheckPlayerCollision()
    {
        if (!ArrayAt(HitPlayers, MainPlayer().SlotIndex()))
        {
            return;
        }
        const Vector3 between = TypeExtensions::WithY(static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position), 0.0F);
        PlayerEntity &player = MainPlayer();
        const Vector3 speed = player.Speed();
        const Vector3 speedDelta = DivideVector(between, 4.0F);
        player.SetSpeed(speed + speedDelta);
        MainPlayer().TakeDamage(15, DamageFlags::None, std::nullopt, this);
    }
    void Enemy28Entity::State00()
    {
        ModelInstance &model = RequireReference(_model);
        const std::int32_t anim = (*model.AnimInfo->Index)[0];
        if ((anim == 5 || anim == 6) && AnimationEnded())
        {
            const ColorRgb white(31, 31, 31);
            Enemy29Entity &sealSphere = RequireReference(_sealSphere);
            sealSphere.Ambient = white;
            sealSphere.Diffuse = white;
            model.SetAnimation(7, 0, SetFlags::All, AnimFlags::NoLoop);
        }
        if (CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this))
        {
            RequireReference(_sealSphere).Flags &= ~EnemyFlags::Invincible;
            model.SetAnimation(3, 0, SetFlags::All);
        }
    }
    void Enemy28Entity::State01()
    {
        const Vector3 toCenter = TypeExtensions::WithY(_volume.SpherePosition - static_cast<Vector3>(Position), 0.0F);
        if (LengthSquared(toCenter) > 1.0F / 128.0F)
        {
            _speed = DivideVector(ScaleVector(toCenter.Normalized(), Fixed::ToFloat(109)), 2.0F);
        }
        UpdateTargetFacing();
        if (CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this))
        {
            _speed = Vector3::Zero;
        }
    }
    void Enemy28Entity::UpdateTargetFacing()
    {
        if (!TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit2))
        {
            _targetFacing = TypeExtensions::WithY(static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position), 0.0F);
            if (LengthSquared(_targetFacing) > 1.0F / 128.0F)
            {
                _goreaFlags |= Gorea1BFlags::Bit2;
                _targetFacing = _targetFacing.Normalized();
            }
            else
            {
                _goreaFlags &= ~Gorea1BFlags::Bit2;
                _targetFacing = Vector3::Zero;
            }
        }
        else if (SeekTargetFacing(_targetFacing, 3.0F))
        {
            _goreaFlags &= ~Gorea1BFlags::Bit2;
            _targetFacing = Vector3::Zero;
        }
    }
    void Enemy28Entity::State02()
    {
        EnsureAnimation(3);
        UpdateTargetFacing();
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::State03()
    {
        Func2139F54();
        _speed = DivideVector(ScaleVector(TypeExtensions::WithY(FacingVector(), 0.0F), Fixed::ToFloat(54)), 2.0F);
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::Func2139F54()
    {
        _targetFacing = TypeExtensions::WithY(static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(Position), 0.0F);
        if (LengthSquared(_targetFacing) > 1.0F / 128.0F)
        {
            _targetFacing = _targetFacing.Normalized();
            Func2139FE8();
        }
    }
    void Enemy28Entity::Func2139FE8()
    {
        Vector3 facing = FacingVector();
        if (Vector3::Dot(_targetFacing, facing) < Fixed::ToFloat(4090))
        {
            const float angle = DegreesToRadians(3.0F);
            const Vector3 cross = Vector3::Cross(_targetFacing, facing);
            const Matrix4 rotY = CreateRotationY(angle * (cross.Y > 0.0F ? -1.0F : 1.0F));
            facing = Matrix::Vec3MultMtx4(facing, rotY).Normalized();
            SetTransform(facing, UpVector(), Position);
            if (_grappling)
            {
                const Vector3 first = ArrayAt(_grappleVecs, 0);
                Vector3 last = ArrayAt(_grappleVecs, 23);
                Vector3 between = last - first;
                between = Matrix::Vec3MultMtx4(between, rotY);
                last = between + first;
                Func213C8C4(last);
            }
        }
        else
        {
            SetTransform(_targetFacing, UpVector(), Position);
        }
    }
    void Enemy28Entity::Func213C8C4(Vector3 last)
    {
        const Vector3 first = ArrayAt(_grappleVecs, 0);
        const Vector3 between = last - first;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_grappleVecs.size()); ++i)
        {
            const float factor = static_cast<float>(i) / static_cast<float>(_grappleVecs.size() - 1);
            ArrayAt(_grappleVecs, i) = first + ScaleVector(between, factor);
        }
    }
    void Enemy28Entity::State04()
    {
        ModelInstance &model = RequireReference(_model);
        if (!_grappling)
        {
            _speed = Vector3::Zero;
            model.SetAnimation(1, 0, SetFlags::All, AnimFlags::NoLoop);
            Func213BCB8();
        }
        else if (AnimationEnded())
        {
            model.SetAnimation(3, 0, SetFlags::All);
        }
        Func2139F54();
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::Func213BCB8()
    {
        _grappling = true;
        _field21E = 120.0F * 2.0F;
        const Vector3 target = TypeExtensions::AddY(MainPlayer().Position, 0.05F);
        Func213C8C4(target);
        _field38 = 0.0F;
    }
    void Enemy28Entity::State05()
    {
        Func2139F54();
        UpdateGrappleDrawValues();
        Func213B90C();
        if (AnimationEnded())
        {
            RequireReference(_model).SetAnimation(3, 0, SetFlags::All);
        }
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::UpdateGrappleDrawValues()
    {
        if (_grappling)
        {
            ArrayAt(_grappleVecs, 0) = RequireReference(_sealSphere).Position;
            UpdateGrappleDrawMatrix();
            UpdateGrappleDrawInt();
        }
    }
    void Enemy28Entity::UpdateGrappleDrawMatrix()
    {
        Vector3 between = ArrayAt(_grappleVecs, 23) - ArrayAt(_grappleVecs, 0);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            between = between.Normalized();
            _field21C += 1.5F / 2.0F;
            if (_field21C >= 360.0F)
            {
                _field21C -= 360.0F;
            }
            _grappleMtx = CreateFromAxisAngle(between, DegreesToRadians(_field21C));
        }
        else
        {
            _grappleMtx = IdentityMatrix();
        }
    }
    void Enemy28Entity::UpdateGrappleDrawInt()
    {
        if (static_cast<std::int32_t>(_field38) < static_cast<std::int32_t>(_grappleVecs.size()) - 1)
        {
            _field38 += _field34 / 2.0F;
        }
        _grappleInt += _field28 / 2.0F;
        if (RoundToInt32ToEven(_grappleInt) > static_cast<std::int32_t>(_grappleVecs.size()))
        {
            _grappleInt -= std::fmod(_grappleInt, 1.0F);
        }
    }
    void Enemy28Entity::Func213B90C()
    {
        const Vector3 pos = Func213BF7C();
        Vector3 between = TypeExtensions::AddY(MainPlayer().Position, 0.05F) - pos;
        if (LengthSquared(between) >= 0.25F * 0.25F)
        {
            between = between.Normalized();
            ArrayAt(_grappleVecs, 23) = ArrayAt(_grappleVecs, 23) + DivideVector(ScaleVector(between, 0.3F), 2.0F);
            Formats::CollisionResult result{};
            if (Formats::CollisionDetection::CheckBetweenPoints(
                ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 23),
                Formats::TestFlags::None, _scene, result))
            {
                Vector3 toCollision = result.Position - ArrayAt(_grappleVecs, 0);
                if (Length(toCollision) > 20.0F)
                {
                    toCollision = ScaleVector(toCollision.Normalized(), 20.0F);
                }
                ArrayAt(_grappleVecs, 23) = ArrayAt(_grappleVecs, 0) + toCollision;
            }
            Func213C8C4(ArrayAt(_grappleVecs, 23));
        }
        else
        {
            _field234 = RequireReference(_sealSphere).Damage();
            _field38 = static_cast<float>(_grappleVecs.size() - 1);
            Func213C8C4(pos);
            _goreaFlags |= Gorea1BFlags::Bit1;
            MainPlayer().SetBipedStuck(true);
            Func213BC3C();
            if (_grappleEffect)
            {
                RequireReference(_scene).UnlinkEffectEntry(_grappleEffect);
                _grappleEffect.reset();
            }
            _grappleEffect = SpawnEffectGetEntry(148, ArrayAt(_grappleVecs, 23), true);
            RequireReference(MainPlayer().CameraInfo()).SetShake(1.0F / 6.0F);
            if (TypeExtensions::TestFlag(MainPlayer().Flags1(), PlayerFlags1::AltForm)
                || TypeExtensions::TestFlag(MainPlayer().Flags1(), PlayerFlags1::Morphing))
            {
                MainPlayer().ExitAltForm();
            }
            _field224 = 0.0F;
            const float v9 = (10.0F - (MainPlayer().Position.Y - Position.Y)) / 24.0F;
            const float v10 = v9 * 30.0F * 30.0F;
            if (v10 != 0.0F)
            {
                const float dist = Vector3::Distance(ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 23));
                _field224 = (Fixed::ToFloat(2986) - dist) / v10;
            }
        }
    }
    Vector3 Enemy28Entity::Func213BF7C() { return Func213C458(_field38); }
    Vector3 Enemy28Entity::Func213C458(float index)
    {
        const std::int32_t intIndex = static_cast<std::int32_t>(index);
        Vector3 result = ArrayAt(_grappleVecs, intIndex);
        const float fractional = std::fmod(index, 1.0F);
        if (fractional != 0.0F)
        {
            const Vector3 between = ArrayAt(_grappleVecs, AddInt32Unchecked(intIndex, 1)) - result;
            result = result + ScaleVector(between, fractional);
        }
        return result;
    }
    void Enemy28Entity::Func213BC3C()
    {
        const float dist = Vector3::Distance(ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 23));
        float v4 = 1.0F;
        if (dist > 14.0F)
        {
            float v5 = dist / 17.5F;
            if (v5 > 1.0F)
            {
                v5 = 1.0F;
            }
            v4 = 1.0F - v5 * 0.07F;
        }
        _field30 = v4;
    }
    void Enemy28Entity::State06()
    {
        UpdateGrappleDrawValues();
        _field10 = Vector3(0.0F, 1.0F / 30.0F, 0.0F);
        Func213B678();
        Func213B7E0();
        TickGrappleDamage();
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::Func213B678()
    {
        if (MainPlayer().Position.Y <= Position.Y)
        {
            return;
        }
        const float dist = Vector3::Distance(ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 1));
        if (dist > 1.0F / 128.0F)
        {
            const float v6 = (10.0F - (MainPlayer().Position.Y - Position.Y)) * 30.0F;
            if (v6 > 0.0F)
            {
                const float v7 = Func213B784();
                _field224 = (17.5F - v7) / v6;
                const float v8 = std::abs(dist - Fixed::ToFloat(2986));
                const float v12 = v8 > std::abs(_field224) ? dist + _field224 : Fixed::ToFloat(2986);
                Func213C624(v12);
            }
        }
        MainPlayer().Position = TypeExtensions::AddY(ArrayAt(_grappleVecs, 23), -0.05F);
    }
    float Enemy28Entity::Func213B784()
    {
        float sum = 0.0F;
        for (std::int32_t i = 1; i < static_cast<std::int32_t>(_grappleVecs.size()); ++i)
        {
            sum += Vector3::Distance(ArrayAt(_grappleVecs, i), ArrayAt(_grappleVecs, i - 1));
        }
        return sum;
    }
    void Enemy28Entity::Func213C624(float factor)
    {
        for (std::int32_t i = 1; i < static_cast<std::int32_t>(_grappleVecs.size()); ++i)
        {
            const Vector3 between = ArrayAt(_grappleVecs, i) - ArrayAt(_grappleVecs, i - 1);
            if (LengthSquared(between) > 1.0F / 128.0F)
            {
                const Vector3 scaled = ScaleVector(between.Normalized(), factor);
                const Vector3 remaining = scaled - between;
                for (std::int32_t j = i; j < static_cast<std::int32_t>(_grappleVecs.size()); ++j)
                {
                    ArrayAt(_grappleVecs, j) = ArrayAt(_grappleVecs, j) + remaining;
                }
            }
        }
    }
    void Enemy28Entity::Func213B7E0()
    {
        Func213C4DC();
        const float dist = Vector3::Distance(ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 1));
        Func213C624(dist);
        Vector3 playerPos = TypeExtensions::AddY(ArrayAt(_grappleVecs, 23), -0.05F);
        Vector3 between = ArrayAt(_grappleVecs, 23) - ArrayAt(_grappleVecs, 22);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            between = between.Normalized();
            playerPos = playerPos + ScaleVector(between, -0.5F);
        }
        MainPlayer().Position = playerPos;
        RequireReference(MainPlayer().CameraInfo()).SetShake(0.0F);
    }
    void Enemy28Entity::Func213C4DC()
    {
        const float dist = Vector3::Distance(ArrayAt(_grappleVecs, 0), ArrayAt(_grappleVecs, 1));
        ArrayAt(_grappleVecs, 1) = ArrayAt(_grappleVecs, 1) + DivideVector(_field10, 2.0F);
        if (dist > 1.0F / 128.0F)
        {
            Vector3 start = ArrayAt(_grappleVecs, 1) - ArrayAt(_grappleVecs, 0);
            for (std::int32_t i = 2; i < static_cast<std::int32_t>(_grappleVecs.size()); ++i)
            {
                const Vector3 between = ArrayAt(_grappleVecs, i) - ArrayAt(_grappleVecs, i - 1);
                if (LengthSquared(between) > 1.0F / 128.0F)
                {
                    const Vector3 result = Func204D57C(between, start);
                    Vector3 update = ArrayAt(_grappleVecs, i);
                    update = update + ScaleVector(result - between, _field30);
                    ArrayAt(_grappleVecs, i) = update;
                    start = update - ArrayAt(_grappleVecs, i - 1);
                }
            }
        }
    }
    Vector3 Enemy28Entity::Func204D57C(Vector3 vec, Vector3 axis)
    {
        const float dot1 = Vector3::Dot(axis, axis);
        if (dot1 == 0.0F)
        {
            return Vector3::Zero;
        }
        const float dot2 = Vector3::Dot(vec, axis);
        return DivideVector(ScaleVector(axis, dot2), dot1);
    }
    void Enemy28Entity::TickGrappleDamage()
    {
        if (_damageTimer > 0)
        {
            --_damageTimer;
        }
        if (_damageTimer == 0)
        {
            _damageTimer = MulInt32Unchecked(UInt32ToInt32Unchecked(Rng::GetRandomInt2(13) + 7U), 2);
            MainPlayer().TakeDamage(2, DamageFlags::NoDmgInvuln, std::nullopt, this);
            SpawnEffect(179, MainPlayer().Position);
        }
    }
    void Enemy28Entity::State07()
    {
        Func213B678();
        UpdateGrappleDrawValues();
        Vector3 between;
        if (TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit3))
        {
            between = ArrayAt(_grappleVecs, 0) - ArrayAt(_grappleVecs, 1);
        }
        else
        {
            between = ArrayAt(_grappleVecs, 1) - ArrayAt(_grappleVecs, 0);
        }
        between = TypeExtensions::WithZ(TypeExtensions::WithY(between, 0.0F), -between.Z);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            _field10 = between.Normalized();
        }
        _field10 = ScaleVector(_field10, 0.04F);
        _field10 = _field10 + _speed;
        Func213B7E0();
        TickGrappleDamage();
        if (_holdTimer > 0)
        {
            --_holdTimer;
        }
        if (_holdTimer == 0)
        {
            _holdTimer = 60 * 2;
            _goreaFlags ^= Gorea1BFlags::Bit3;
            _soundSource.PlaySfx(SfxId::GOREA_ATTACK2B_SCR);
        }
        if (!Equal(_speed, Vector3::Zero) && CheckMovementOutsideVolume())
        {
            _speed = Vector3::Zero;
        }
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    bool Enemy28Entity::CheckMovementOutsideVolume() { return !_volume.TestPoint(static_cast<Vector3>(Position) + _speed); }
    void Enemy28Entity::State08()
    {
        UpdateGrappleDrawValues();
        _field10 = Vector3(0.0F, 1.0F / 15.0F, 0.0F);
        Func213B678();
        Func213B7E0();
        TickGrappleDamage();
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::State09()
    {
        UpdateGrappleDrawValues();
        Func213C4DC();
        TickGrappleDamage();
        Func213C624(Fixed::ToFloat(2986));
        _field10 = Vector3(0.0F, -1.0F / 1.5F, 0.0F);
        const Vector3 between = (ArrayAt(_grappleVecs, 0) - ArrayAt(_grappleVecs, 1)).Normalized();
        if (Vector3::Dot(between, _field10.Normalized()) < Fixed::ToFloat(-3956))
        {
            Vector3 vec = TypeExtensions::WithY(_volume.SpherePosition - static_cast<Vector3>(Position), 0.0F);
            if (LengthSquared(vec) >= 1.0F / 128.0F)
            {
                vec = vec.Normalized();
            }
            else
            {
                vec = FacingVector();
            }
            _field10 = ScaleVector(vec, -1.0F / 1.5F);
        }
        MainPlayer().Position = TypeExtensions::AddY(ArrayAt(_grappleVecs, 23), -0.05F);
        RequireReference(MainPlayer().CameraInfo()).SetShake(1.0F / 128.0F);
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
    }
    void Enemy28Entity::State10()
    {
        if (CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this))
        {
            StopGrappling();
        }
    }
    void Enemy28Entity::StopGrappling()
    {
        _goreaFlags &= ~Gorea1BFlags::Bit1;
        if (_grappleEffect)
        {
            RequireReference(_scene).UnlinkEffectEntry(_grappleEffect);
            _grappleEffect.reset();
        }
        MainPlayer().SetBipedStuck(false);
        if (_grappling)
        {
            _grappling = false;
            _soundSource.PlaySfx(SfxId::GOREA_TENTACLE_DIE_SCR);
            const std::int32_t index = static_cast<std::int32_t>(_field38);
            if (index >= 2)
            {
                Vector3 prev = ArrayAt(_grappleVecs, index - 1);
                for (std::int32_t i = index - 2; i >= 0; --i)
                {
                    const Vector3 cur = ArrayAt(_grappleVecs, i);
                    if (i % 2 == 1)
                    {
                        Vector3 between = prev - cur;
                        if (LengthSquared(between) > 1.0F / 128.0F)
                        {
                            between = between.Normalized();
                            Vector3 unitVec(0.0F, 1.0F, 0.0F);
                            const float dot = std::abs(Vector3::Dot(unitVec, between));
                            if (dot > Fixed::ToFloat(4065))
                            {
                                unitVec = Vector3(0.0F, 0.0F, 1.0F);
                            }
                            SpawnEffect(180, cur, between, unitVec);
                        }
                    }
                    prev = cur;
                }
            }
        }
    }
    void Enemy28Entity::State11()
    {
        Func213B2B4();
        Func213AF2C();
        Func2139F54();
        _speed = DivideVector(ScaleVector(TypeExtensions::WithY(FacingVector(), 0.0F), Fixed::ToFloat(54)), 2.0F);
        if (!Equal(_speed, Vector3::Zero) && CheckMovementOutsideVolume())
        {
            _speed = Vector3::Zero;
        }
        (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
        _soundSource.PlayEnvironmentSfx(9);
    }
    void Enemy28Entity::Func213B2B4()
    {
        if (_holdTimer <= 0)
        {
            return;
        }
        bool trocrasAlive = false;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            if (ArrayAt(_trocra, i))
            {
                trocrasAlive = true;
                break;
            }
        }
        if (!trocrasAlive)
        {
            _swingTimer = 0;
        }
        else if (_swingTimer > 0)
        {
            --_swingTimer;
        }
        if (_swingTimer == 0)
        {
            _swingTimer = 30 * 2;
            Func213B348();
        }
    }
    void Enemy28Entity::Func213B348()
    {
        std::int32_t firstDeadIndex = -1;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            if (!ArrayAt(_trocra, i))
            {
                firstDeadIndex = i;
                break;
            }
        }
        if (firstDeadIndex >= 0)
        {
            std::shared_ptr<LinkedListNode<EntityBase>> node = RequireReference(_scene).Entities().FirstNode();
            while (node)
            {
                const std::shared_ptr<EntityBase> &entity = node->Value();
                if (RequireReference(entity).Type == EntityType::EnemyInstance)
                {
                    std::shared_ptr<Enemy30Entity> enemy = std::dynamic_pointer_cast<Enemy30Entity>(entity);
                    if (enemy && enemy->State == 0)
                    {
                        ArrayAt(_trocra, firstDeadIndex) = enemy;
                        break;
                    }
                }
                node = node->Next();
            }
            if (ArrayAt(_trocra, firstDeadIndex))
            {
                assert(node != nullptr);
                node = node->Next();
                while (node)
                {
                    const std::shared_ptr<EntityBase> &entity = node->Value();
                    if (RequireReference(entity).Type == EntityType::EnemyInstance)
                    {
                        std::shared_ptr<Enemy30Entity> enemy = std::dynamic_pointer_cast<Enemy30Entity>(entity);
                        if (enemy && enemy->State == 0)
                        {
                            Vector3 between = static_cast<Vector3>(enemy->Position) - static_cast<Vector3>(Position);
                            if (LengthSquared(between) > 1.0F / 128.0F && Vector3::Dot(between.Normalized(), FacingVector()) < 0.0F)
                            {
                                ArrayAt(_trocra, firstDeadIndex) = enemy;
                                break;
                            }
                        }
                    }
                    node = node->Next();
                }
                Enemy30Entity &trocra = RequireReference(ArrayAt(_trocra, firstDeadIndex));
                trocra.Gorea1B = this;
                trocra.Index = firstDeadIndex;
                trocra.State = 1;
            }
        }
    }
    void Enemy28Entity::Func213AF2C()
    {
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            const std::shared_ptr<Enemy30Entity> &trocra = ArrayAt(_trocra, i);
            if (trocra)
            {
                if (trocra->State == 1 && !Func213B188(trocra.get()))
                {
                    trocra->Field184 = 45 * 2;
                    trocra->State = 2;
                    trocra->Field174 = trocra->Position;
                }
                else if (trocra->State == 2)
                {
                    trocra->Field174 = trocra->Field174 + _speed;
                    if (trocra->Field184 > 0)
                    {
                        --trocra->Field184;
                        const float randomX = Fixed::ToFloat(UInt32ToInt32Unchecked(Rng::GetRandomInt2(4096) - 2048U));
                        const float randomY = Fixed::ToFloat(UInt32ToInt32Unchecked(Rng::GetRandomInt2(4096) - 2048U));
                        const float randomZ = Fixed::ToFloat(UInt32ToInt32Unchecked(Rng::GetRandomInt2(4096) - 2048U));
                        trocra->Position = Vector3(trocra->Field174.X + randomX, trocra->Field174.Y + randomY, trocra->Field174.Z + randomZ);
                    }
                    if (trocra->Field184 == 0)
                    {
                        Vector3 speed = static_cast<Vector3>(MainPlayer().Position) - static_cast<Vector3>(trocra->Position);
                        if (LengthSquared(speed) >= 1.0F / 128.0F)
                        {
                            speed = ScaleVector(speed.Normalized(), 0.7F);
                        }
                        trocra->SetSpeed(DivideVector(speed, 2.0F));
                        trocra->State = 3;
                        _soundSource.PlaySfx(SfxId::GOREA_ATTACK3A);
                        ArrayAt(_trocra, i).reset();
                    }
                }
            }
        }
    }
    bool Enemy28Entity::Func213B188(Enemy30Entity *trocra)
    {
        Enemy30Entity &trocraRef = RequireReference(trocra);
        Vector3 between = TypeExtensions::WithY(static_cast<Vector3>(trocraRef.Position) - static_cast<Vector3>(Position), 0.0F);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            const Vector3 position = TypeExtensions::AddY(_volume.SpherePosition + ScaleVector(between.Normalized(), 5.0F), 10.0F);
            Vector3 direction = position - static_cast<Vector3>(trocraRef.Position);
            if (LengthSquared(direction) > 1.0F / 128.0F)
            {
                direction = direction.Normalized();
                trocraRef.Position = static_cast<Vector3>(trocraRef.Position) + DivideVector(ScaleVector(direction, 1.0F / 7.0F), 2.0F);
                return true;
            }
        }
        return false;
    }
    void Enemy28Entity::State12()
    {
        if (!TypeExtensions::TestFlag(static_cast<EnemyFlags>(Flags), EnemyFlags::Visible))
        {
            return;
        }
        ModelInstance &model = RequireReference(_model);
        const std::int32_t anim = (*model.AnimInfo->Index)[0];
        if (AnimationEnded())
        {
            if (anim == 4)
            {
                model.SetAnimation(2, 0, SetFlags::All, AnimFlags::NoLoop);
            }
            else if (anim == 2)
            {
                std::shared_ptr<Material> material = RequireReference(model.Model()).GetMaterialByName("HeadFullLit");
                Material &materialRef = RequireReference(material);
                Enemy29Entity &sealSphere = RequireReference(_sealSphere);
                sealSphere.Ambient = materialRef.Ambient;
                sealSphere.Diffuse = materialRef.Diffuse;
                model.SetAnimation(8, 0, SetFlags::All, AnimFlags::NoLoop);
            }
            else if (anim == 8)
            {
                Deactivate();
                (void)CallSubroutine<Enemy28Entity>(Metadata::Enemy28Subroutines, this);
            }
        }
        else if (anim == 8 && (*model.AnimInfo->Frame)[0] == 46)
        {
            Scene &scene = RequireReference(_scene);
            if (scene.FrameCount() != 0 && scene.FrameCount() % 2 == 0)
            {
                RequireReference(MainPlayer().CameraInfo()).SetShake(0.75F);
            }
        }
    }
    void Enemy28Entity::Deactivate()
    {
        _scanId = 0;
        Flags &= ~EnemyFlags::Visible;
        Flags &= ~EnemyFlags::CollidePlayer;
        Flags &= ~EnemyFlags::CollideBeam;
        Flags |= EnemyFlags::NoHomingNc;
        Flags |= EnemyFlags::NoHomingCo;
        Flags &= ~EnemyFlags::OnRadar;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            const std::shared_ptr<Enemy30Entity> &trocra = ArrayAt(_trocra, i);
            if (trocra)
            {
                trocra->Explode();
            }
            ArrayAt(_trocra, i).reset();
        }
        _swingTimer = 30 * 2;
        _goreaFlags &= ~Gorea1BFlags::Bit0;
        RequireReference(_sealSphere).Deactivate();
        _field234 = 0;
        RequireReference(_gorea1A).Activate();
    }
    void Enemy28Entity::State13()
    {
        if (!TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit5))
        {
            _goreaFlags |= Gorea1BFlags::Bit5;
            MainPlayer().SetBipedStuck(false);
            _scanId = 0;
            Flags &= ~EnemyFlags::CollidePlayer;
            Flags &= ~EnemyFlags::CollideBeam;
            Flags |= EnemyFlags::Invincible;
            Flags |= EnemyFlags::NoHomingNc;
            Flags |= EnemyFlags::NoHomingCo;
            Flags &= ~EnemyFlags::OnRadar;
            _phasesLeft = 0;
            _goreaFlags |= Gorea1BFlags::Bit0;
            Enemy29Entity &sealSphere = RequireReference(_sealSphere);
            sealSphere.SetScanId(0);
            sealSphere.Flags &= ~EnemyFlags::Visible;
            sealSphere.Flags &= ~EnemyFlags::CollidePlayer;
            sealSphere.Flags &= ~EnemyFlags::CollideBeam;
            sealSphere.Flags |= EnemyFlags::Invincible;
            sealSphere.Flags |= EnemyFlags::NoHomingNc;
            sealSphere.Flags |= EnemyFlags::NoHomingCo;
            _field234 = 0;
            Enemy24Entity &gorea1A = RequireReference(_gorea1A);
            gorea1A.Position = Position;
            gorea1A.Flags &= ~EnemyFlags::Visible;
            gorea1A.Flags &= ~EnemyFlags::CollidePlayer;
            gorea1A.Flags &= ~EnemyFlags::CollideBeam;
            gorea1A.Flags |= EnemyFlags::Invincible;
            gorea1A.Flags |= EnemyFlags::NoHomingNc;
            gorea1A.Flags |= EnemyFlags::NoHomingCo;
            gorea1A.Flags &= ~EnemyFlags::OnRadar;
        }
        if (IsAtEndFrame())
        {
            EnsureAnimation(3, 0, SetFlags::All, AnimFlags::NoLoop);
        }
    }
    bool Enemy28Entity::BehaviorXX() { return AnimationEnded(); }
    bool Enemy28Entity::Behavior00()
    {
        _holdTimer = MulInt32Unchecked(UInt32ToInt32Unchecked(Rng::GetRandomInt2(150) + 150U), 2);
        return true;
    }
    bool Enemy28Entity::Behavior01() { return _volume.TestPoint(Position); }
    bool Enemy28Entity::Behavior02()
    {
        if (_phasesLeft <= 0)
        {
            SpawnEffect(72, RequireReference(_sealSphere).Position);
            RequireReference(_model).SetAnimation(4, 0, SetFlags::All, AnimFlags::NoLoop);
            StopGrappling();
            DeactivateAllTrocraSpawns();
            DestroyAllTrocras();
            RequireReference(GameState::StorySave).CheckpointRoomId = -1;
            RequireReference(GameState::StorySave).CheckpointEntityId = -1;
            if ((RequireReference(RequireReference(GameState::StorySave).TriggerState)[1] & 0x10) != 0
                || Cheats::AlwaysFightGorea2())
            {
                GameState::TransitionRoomId(92);
                RequireReference(_scene).StartMovie(Movie::Gorea2Intro, FadeType::FadeOutInWhite, 45.0F / 30.0F, FadeType::FadeOutInWhite, 45.0F / 30.0F);
            }
            else
            {
                RequireReference(_scene).StartMovies(Movie::BadEndingPart1, Movie::BadEndingPart2, FadeType::FadeOutInWhite, 45.0F / 30.0F, FadeType::FadeOutBlack, 0.0F, static_cast<AfterMovie>(2));
            }
            return true;
        }
        return false;
    }
    void Enemy28Entity::DeactivateAllTrocraSpawns()
    {
        auto enumerator = RequireReference(_scene).GetEnemySpawnEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EnemySpawnEntity> spawner = enumerator.Current();
            EnemySpawnEntity &spawnerRef = RequireReference(spawner);
            if (spawnerRef.Data.EnemyType == MphRead::EnemyType::Trocra)
            {
                const MessageObject param1 = std::make_shared<const std::any>(std::int32_t(0));
                const MessageObject param2 = std::make_shared<const std::any>(std::int32_t(0));
                RequireReference(_scene).SendMessage(Message::SetActive, this, spawner.get(), param1, param2);
            }
        }
    }
    void Enemy28Entity::DestroyAllTrocras()
    {
        auto enumerator = RequireReference(_scene).GetEnemyInstanceEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<EnemyInstanceEntity> enemy = enumerator.Current();
            std::shared_ptr<Enemy30Entity> trocra = std::dynamic_pointer_cast<Enemy30Entity>(enemy);
            if (trocra)
            {
                trocra->Explode();
            }
        }
    }
    bool Enemy28Entity::Behavior03()
    {
        const std::int32_t damage = RequireReference(_sealSphere).Damage();
        const std::int32_t threshold = MulInt32Unchecked(1000, SubInt32Unchecked(4, _phasesLeft));
        if (damage < threshold)
        {
            return false;
        }
        StopGrappling();
        _phasesLeft = SubInt32Unchecked(_phasesLeft, 1);
        if (_phasesLeft <= 0)
        {
            _soundSource.PlaySfx(SfxId::GOREA_1B_DIE2_SCR);
            return false;
        }
        RequireReference(_model).SetAnimation(4, 0, SetFlags::All, AnimFlags::NoLoop);
        RequireReference(_sealSphere).Flags |= EnemyFlags::Invincible;
        SpawnEffect(42, RequireReference(_sealSphere).Position);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            const std::shared_ptr<Enemy30Entity> &trocra = ArrayAt(_trocra, i);
            if (trocra)
            {
                trocra->Explode();
            }
            ArrayAt(_trocra, i).reset();
        }
        _swingTimer = 30 * 2;
        _soundSource.PlaySfx(SfxId::GOREA_1B_DIE_SCR);
        _soundSource.PlaySfx(SfxId::GOREA_TRANSFORM2_SCR);
        return true;
    }
    bool Enemy28Entity::Behavior04()
    {
        _field21E -= 1.0F;
        if (_field21E <= 0.0F)
        {
            _field21E = 120.0F * 2.0F;
            _soundSource.PlaySfx(SfxId::GOREA_ATTACK2);
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior05()
    {
        if (_holdTimer > 0)
        {
            --_holdTimer;
        }
        if (_holdTimer == 0)
        {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
            {
                if (ArrayAt(_trocra, i))
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior06()
    {
        _field21E -= 1.0F;
        if (_field21E <= 0.0F)
        {
            StopGrappling();
            _holdTimer = MulInt32Unchecked(UInt32ToInt32Unchecked(Rng::GetRandomInt2(150) + 150U), 2);
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior07()
    {
        if (TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit1))
        {
            _soundSource.PlaySfx(SfxId::GOREA_ATTACK2A);
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior08() { return SubInt32Unchecked(RequireReference(_sealSphere).Damage(), _field234) >= 35; }
    bool Enemy28Entity::Behavior09()
    {
        if (MainPlayer().Position.Y - Position.Y < 10.0F)
        {
            return false;
        }
        _field30 = 0.9F;
        _swingTimer = 150 * 2;
        _holdTimer = 30 * 2;
        _goreaFlags ^= Gorea1BFlags::Bit3;
        const Vector3 toCenter = TypeExtensions::WithY(static_cast<Vector3>(MainPlayer().Position) - _volume.SpherePosition, 0.0F);
        if (LengthSquared(toCenter) > 1.0F / 128.0F)
        {
            _speed = DivideVector(ScaleVector(toCenter.Normalized(), Fixed::ToFloat(68)), 2.0F);
            if (CheckMovementOutsideVolume())
            {
                _speed = Vector3::Zero;
            }
        }
        _soundSource.PlaySfx(SfxId::GOREA_ATTACK2B_SCR);
        return true;
    }
    bool Enemy28Entity::Behavior10()
    {
        if (_swingTimer > 0)
        {
            --_swingTimer;
        }
        if (_swingTimer == 0)
        {
            _field30 = Fixed::ToFloat(3973);
            _speed = Vector3::Zero;
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior11()
    {
        if (MainPlayer().Position.Y - Position.Y < 22.5F)
        {
            return false;
        }
        _field30 = Fixed::ToFloat(4046);
        _speed = Vector3::Zero;
        return true;
    }
    bool Enemy28Entity::Behavior12()
    {
        bool collided = false;
        Formats::CollisionResult result{};
        const Vector3 prevPosition = MainPlayer().PrevPosition();
        const Vector3 currentPosition = MainPlayer().Position;
        if (Formats::CollisionDetection::CheckBetweenPoints(
            prevPosition, currentPosition, Formats::TestFlags::None, _scene, result))
        {
            MainPlayer().Position = result.Position;
            MainPlayer().HandleCollision(result);
            collided = true;
        }
        else
        {
            const float spawnerY = RequireReference(RequireReference(_gorea1A).Spawner()).Data.Header.Position.ToFloatVector().Y;
            const float yDiff = MainPlayer().Position.Y - spawnerY;
            if (yDiff < 0.0F)
            {
                result = Formats::CollisionResult{};
                result.Field0 = 0;
                result.Plane = Vector4(0.0F, 1.0F, 0.0F, spawnerY);
                result.Field14 = -yDiff;
                result.Position = TypeExtensions::WithY(MainPlayer().Position, spawnerY);
                MainPlayer().HandleCollision(result);
                collided = true;
            }
        }
        if (collided)
        {
            StopGrappling();
            _soundSource.PlaySfx(SfxId::GOREA_ATTACK2C_SCR);
            MainPlayer().TakeDamage(30, DamageFlags::NoDmgInvuln, std::nullopt, this);
            RequireReference(MainPlayer().CameraInfo()).SetShake(0.75F);
            _holdTimer = MulInt32Unchecked(UInt32ToInt32Unchecked(Rng::GetRandomInt2(150) + 150U), 2);
        }
        return collided;
    }
    bool Enemy28Entity::Behavior13()
    {
        if (MainPlayer().Health() != 0 && !TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit2) && CheckFacingAngle(-1.0F, MainPlayer().Position))
        {
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior14()
    {
        const Vector3 sealSpherePosition = RequireReference(_sealSphere).Position;
        const Vector3 playerPosition = MainPlayer().Position;
        if (Vector3::Distance(sealSpherePosition, playerPosition) < 20.0F && CheckFacingAngle(-1.0F, MainPlayer().Position))
        {
            _field21E = 120.0F * 2.0F;
            return true;
        }
        return false;
    }
    bool Enemy28Entity::Behavior15()
    {
        if (CheckMovementOutsideVolume())
        {
            _speed = Vector3::Zero;
            _field21E = 120.0F * 2.0F;
            return true;
        }
        return false;
    }
    bool Enemy28Entity::EnemyTakeDamage(EntityBase *source)
    {
        (void)source;
        _health = std::numeric_limits<std::uint16_t>::max();
        return false;
    }
    void Enemy28Entity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Destroyed && RequireReference(info.Sender).Type == EntityType::EnemyInstance)
        {
            if (Enemy30Entity *trocra = dynamic_cast<Enemy30Entity *>(info.Sender))
            {
                ArrayAt(_trocra, trocra->Index).reset();
            }
        }
    }
    void Enemy28Entity::ResetMaterialColors()
    {
        Model &model = RequireReference(RequireReference(_model).Model());
        const std::vector<std::shared_ptr<Material>> &materials = RequireReference(model.Materials);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(materials.size()); ++i)
        {
            Material &material = RequireReference(VectorAt(materials, i));
            if (material.Name != "L_ShoulderTarget" && material.Name != "R_ShoulderTarget" && material.Name != "BackTarget")
            {
                material.AnimationFlags |= MatAnimFlags::DisableColor;
            }
        }
    }
    void Enemy28Entity::UpdateMaterials()
    {
        Model &model = RequireReference(RequireReference(_model).Model());
        for (std::int32_t i = 0; i < 6; ++i)
        {
            std::shared_ptr<Material> material = model.GetMaterialByName(
                std::string(ArrayAt(_bodyMatNames1, i)));
            Material &materialRef = RequireReference(material);
            materialRef.Ambient = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 2);
            materialRef.Diffuse = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 3);
        }
        for (std::int32_t i = 0; i < 2; ++i)
        {
            std::shared_ptr<Material> material = model.GetMaterialByName(
                std::string(ArrayAt(_bodyMatNames2, i)));
            Material &materialRef = RequireReference(material);
            materialRef.Ambient = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 0);
            materialRef.Diffuse = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 1);
        }
        Enemy29Entity &sealSphere = RequireReference(_sealSphere);
        sealSphere.Ambient = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 0);
        sealSphere.Diffuse = VectorAt(RequireReference(RequireReference(_gorea1A).Colors()), 1);
    }
    void Enemy28Entity::DrawSelf() { DrawGeneric(); }
    bool Enemy28Entity::EnemyGetDrawInfo()
    {
        Scene &scene = RequireReference(_scene);
        if (scene.ProcessFrame())
        {
            std::shared_ptr<Material> material = RequireReference(RequireReference(_model).Model()).GetMaterialByName("ChestCore");
            Material &materialRef = RequireReference(material);
            const std::int32_t maxFrame = 10 * 2;
            const std::int32_t frame = SubInt32Unchecked(maxFrame, RequireReference(_sealSphere).DamageTimer());
            const ColorRgb ambient = RequireReference(_sealSphere).Ambient;
            const ColorRgb diffuse = RequireReference(_sealSphere).Diffuse;
            IncrementMaterialColors(&materialRef, ambient, diffuse, frame, maxFrame);
        }
        _lightOverride = true;
        DrawGeneric();
        _lightOverride = false;
        if (TypeExtensions::TestFlag(_goreaFlags, Gorea1BFlags::Bit1))
        {
            TransformGrappleEffect();
        }
        DrawMindTricks();
        if (_grappling)
        {
            DrawGrappleBeam();
            if (scene.ProcessFrame() && scene.FrameCount() != 0 && scene.FrameCount() % 2 == 0)
            {
                RequireReference(_grappleModel).UpdateAnimFrames();
            }
        }
        return true;
    }
    void Enemy28Entity::TransformGrappleEffect()
    {
        if (_grappleEffect)
        {
            Scene &scene = RequireReference(_scene);
            if (scene.ProcessFrame())
            {
                const Vector3 last = ScaleVector(ArrayAt(_grappleVecs, 23), 0.5F);
                const Vector3 prev = ScaleVector(ArrayAt(_grappleVecs, 22), 0.5F);
                const Vector3 up = UpVector();
                const Vector3 facing = FacingVector();
                _grappleEffect->Transform(up, facing, prev + last);
            }
        }
    }
    void Enemy28Entity::DrawMindTricks()
    {
        Scene &scene = RequireReference(_scene);
        if (scene.ProcessFrame() && scene.FrameCount() != 0 && scene.FrameCount() % 2 == 0)
        {
            RequireReference(_trickModel).UpdateAnimFrames();
        }
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(_trocra.size()); ++i)
        {
            const std::shared_ptr<Enemy30Entity> &trocra = ArrayAt(_trocra, i);
            if (trocra)
            {
                Vector3 between = static_cast<Vector3>(trocra->Position) - static_cast<Vector3>(RequireReference(_sealSphere).Position);
                const float distance = Length(between);
                if (distance > 1.0F / 128.0F)
                {
                    between = between.Normalized();
                    Vector3 unitVec(0.0F, 1.0F, 0.0F);
                    const float dot = std::abs(Vector3::Dot(unitVec, between));
                    if (dot > Fixed::ToFloat(4065))
                    {
                        unitVec = Vector3(0.0F, 0.0F, 1.0F);
                    }
                    Matrix4 transform = GetTransformMatrix(between, unitVec, RequireReference(_sealSphere).Position);
                    transform.M31 *= distance;
                    transform.M32 *= distance;
                    transform.M33 *= distance;
                    UpdateTransforms(RequireReference(_trickModel), transform, 0);
                    GetDrawItems(RequireReference(_trickModel), 0);
                }
            }
        }
    }
    void Enemy28Entity::DrawGrappleBeam()
    {
        const Vector3 vec = Func213BF7C();
        const Vector3 between = vec - ArrayAt(_grappleVecs, 0);
        if (LengthSquared(between) > 1.0F / 128.0F)
        {
            Vector3 cross1 = Vector3::Cross(Vector3(0.0F, 1.0F, 0.0F), between);
            if (LengthSquared(cross1) > 1.0F / 128.0F)
            {
                cross1 = cross1.Normalized();
                Vector3 cross2 = Vector3::Cross(between, cross1);
                cross2 = cross2.Normalized();
                cross1 = Matrix::Vec3MultMtx4(cross1, _grappleMtx);
                cross2 = Matrix::Vec3MultMtx4(cross2, _grappleMtx);
                Vector3 pos{};
                Func213C238(0.0F, pos);
                for (float index = 1.0F; index < _field38; index += 1.0F)
                {
                    DrawGrappleSegment(pos, index, cross2, cross1);
                }
                DrawGrappleSegment(pos, _field38, cross2, cross1);
            }
        }
    }
    void Enemy28Entity::Func213C238(float index, Vector3 &pos)
    {
        const Vector3 first = ArrayAt(_grappleVecs, 0);
        const Vector3 last = ArrayAt(_grappleVecs, 23);
        Vector3 axis(first.Z - last.Z, 0.0F, last.X - first.X);
        if (LengthSquared(axis) <= 1.0F / 128.0F)
        {
            return;
        }
        axis = axis.Normalized();
        const float angle = (index + _grappleInt) * (65535.0F / 24.0F) / 16.0F * (360.0F / 4096.0F);
        const float factor = std::sin(DegreesToRadians(angle)) * _field24;
        axis = ScaleVector(axis, factor);
        axis = Matrix::Vec3MultMtx4(axis, _grappleMtx);
        const Vector3 vec = Func213C458(index);
        pos = vec + axis;
    }
    void Enemy28Entity::DrawGrappleSegment(Vector3 &pos, float index, Vector3 a5, Vector3 a6)
    {
        Vector3 vec{};
        Func213C238(index, vec);
        const Matrix4 transform(Vector4(a6, 0.0F), Vector4(a5, 0.0F), Vector4(vec - pos, 0.0F), Vector4(pos, 1.0F));
        UpdateTransforms(RequireReference(_grappleModel), transform, 0);
        GetDrawItems(RequireReference(_grappleModel), 0);
        pos = vec;
    }
    bool Enemy28Entity::BehaviorXX(Enemy28Entity *enemy) { return RequireEnemy(enemy).BehaviorXX(); }
    bool Enemy28Entity::Behavior00(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior00(); }
    bool Enemy28Entity::Behavior01(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior01(); }
    bool Enemy28Entity::Behavior02(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior02(); }
    bool Enemy28Entity::Behavior03(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior03(); }
    bool Enemy28Entity::Behavior04(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior04(); }
    bool Enemy28Entity::Behavior05(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior05(); }
    bool Enemy28Entity::Behavior06(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior06(); }
    bool Enemy28Entity::Behavior07(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior07(); }
    bool Enemy28Entity::Behavior08(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior08(); }
    bool Enemy28Entity::Behavior09(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior09(); }
    bool Enemy28Entity::Behavior10(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior10(); }
    bool Enemy28Entity::Behavior11(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior11(); }
    bool Enemy28Entity::Behavior12(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior12(); }
    bool Enemy28Entity::Behavior13(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior13(); }
    bool Enemy28Entity::Behavior14(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior14(); }
    bool Enemy28Entity::Behavior15(Enemy28Entity *enemy) { return RequireEnemy(enemy).Behavior15(); }
}

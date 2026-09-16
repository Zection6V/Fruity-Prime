#pragma once

#include "Formats/Types.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <functional>
#include <vector>

namespace MphRead::Memory
{
class Memory;
class SByteArray;
class ByteArray;
class Int16Array;
class UInt16Array;
class Int32Array;
class UInt32Array;
class IntPtrArray;
template <typename T> class U32EnumArray;
template <typename T> class StructArray;

class CEntity;
class CEnemyBase;
class CEnemy24;
class CEnemy25;
class CEnemy26;
class CEnemy27;
class CEnemy28;
class Enemy29Fields;
class CEnemy29;
class CEnemy30;
class CPlatform;
class CObject;
class CPlayerSpawn;
class CDoor;
class CItemSpawn;
class CItemInstance;
class CEnemySpawn;
class CTriggerVolume;
class CAreaVolume;
class CJumpPad;
class CPointModule;
class CMorphCamera;
class COctolithFlag;
class CFlagBase;
class CTeleporter;
class CNodeDefense;
class CLightSource;
class CArtifact;
class CCameraSequence;
class CForceField;
class CBeamEffect;
class CBomb;
class CHalfturret;
class CPlayer;
class AiData;
class AIContext;
class AIData1;
class AIData2;
class AIAggro;
class CBeamProjectile;
class CModel;
class CNodeAnimation;
class EntityCollision;
class EquipInfoPtr;
class SfxParameters;
class CollisionVolume;
class Light;
class LightInfo;
class CameraInfo;
class PlayerControls;
class PlayerInput;
class CameraSequence;
class CameraSequenceKeyframe;
class GameState;
class KioskGameState;
class RoomState;
class StorySaveData;
class SaveType3;
class StatsAndSettings;
class LicenseInfo;
class FriendsRivals;
class RoomDescription;
class EquipInfo;
class AiButton;
class VecFx32;
class MtxFx43;

class MemoryClass
{
public:
    MemoryClass(const MemoryClass&) = delete;
    MemoryClass(MemoryClass&&) = delete;
    MemoryClass& operator=(const MemoryClass&) = delete;
    MemoryClass& operator=(MemoryClass&&) = delete;
    virtual ~MemoryClass() = 0;
    [[nodiscard]] std::intptr_t Address() const noexcept { return _address; }
    [[nodiscard]] bool Equals(const MemoryClass* other) const noexcept;
    [[nodiscard]] std::int32_t GetHashCode() const noexcept;
protected:
    MemoryClass(Memory& memory, std::int32_t address);
    MemoryClass(Memory& memory, std::intptr_t address);
    [[nodiscard]] std::int8_t ReadSByte(std::int32_t offset) const;
    [[nodiscard]] std::uint8_t ReadByte(std::int32_t offset) const;
    [[nodiscard]] std::int16_t ReadInt16(std::int32_t offset) const;
    [[nodiscard]] std::uint16_t ReadUInt16(std::int32_t offset) const;
    [[nodiscard]] std::int32_t ReadInt32(std::int32_t offset) const;
    [[nodiscard]] std::uint32_t ReadUInt32(std::int32_t offset) const;
    [[nodiscard]] std::intptr_t ReadPointer(std::int32_t offset) const;
    [[nodiscard]] MphRead::ColorRgb ReadColor3(std::int32_t offset) const;
    [[nodiscard]] OpenTK::Mathematics::Vector3 ReadVec3(std::int32_t offset) const;
    [[nodiscard]] OpenTK::Mathematics::Vector4 ReadVec4(std::int32_t offset) const;
    [[nodiscard]] OpenTK::Mathematics::Matrix4x3 ReadMtx43(std::int32_t offset) const;
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> ReadClass(std::int32_t offset, const std::function<std::shared_ptr<T>(Memory&, std::int32_t)>& create)
    {
        if (!create) throw System::NullReferenceException();
        return create(_memory, ReadInt32(offset));
    }
    void WriteSByte(std::int32_t offset, std::int8_t value);
    void WriteByte(std::int32_t offset, std::uint8_t value);
    void WriteInt16(std::int32_t offset, std::int16_t value);
    void WriteUInt16(std::int32_t offset, std::uint16_t value);
    void WriteInt32(std::int32_t offset, std::int32_t value);
    void WriteUInt32(std::int32_t offset, std::uint32_t value);
    void WritePointer(std::int32_t offset, std::intptr_t value);
    void WriteColor3(std::int32_t offset, MphRead::ColorRgb value);
    void WriteVec3(std::int32_t offset, OpenTK::Mathematics::Vector3 value);
    void WriteVec4(std::int32_t offset, OpenTK::Mathematics::Vector4 value);
    void WriteMtx43(std::int32_t offset, OpenTK::Mathematics::Matrix4x3 value);
    Memory& _memory;
    const std::int32_t _offset;
private:
    const std::intptr_t _address;
};

[[nodiscard]] bool operator==(const std::shared_ptr<MemoryClass>& left, const std::shared_ptr<MemoryClass>& right) noexcept;
[[nodiscard]] bool operator!=(const std::shared_ptr<MemoryClass>& left, const std::shared_ptr<MemoryClass>& right) noexcept;

#define MPH_MEM_S8(name, off) \
    [[nodiscard]] std::int8_t name() const { return ReadSByte(off); } \
    void name(std::int8_t value) { WriteSByte(off, value); }
#define MPH_MEM_U8(name, off) \
    [[nodiscard]] std::uint8_t name() const { return ReadByte(off); } \
    void name(std::uint8_t value) { WriteByte(off, value); }
#define MPH_MEM_I16(name, off) \
    [[nodiscard]] std::int16_t name() const { return ReadInt16(off); } \
    void name(std::int16_t value) { WriteInt16(off, value); }
#define MPH_MEM_U16(name, off) \
    [[nodiscard]] std::uint16_t name() const { return ReadUInt16(off); } \
    void name(std::uint16_t value) { WriteUInt16(off, value); }
#define MPH_MEM_I32(name, off) \
    [[nodiscard]] std::int32_t name() const { return ReadInt32(off); } \
    void name(std::int32_t value) { WriteInt32(off, value); }
#define MPH_MEM_U32(name, off) \
    [[nodiscard]] std::uint32_t name() const { return ReadUInt32(off); } \
    void name(std::uint32_t value) { WriteUInt32(off, value); }
#define MPH_MEM_PTR(name, off) \
    [[nodiscard]] std::intptr_t name() const { return ReadPointer(off); } \
    void name(std::intptr_t value) { WritePointer(off, value); }
#define MPH_MEM_COLOR(name, off) \
    [[nodiscard]] MphRead::ColorRgb name() const { return ReadColor3(off); } \
    void name(MphRead::ColorRgb value) { WriteColor3(off, value); }
#define MPH_MEM_VEC3(name, off) \
    [[nodiscard]] OpenTK::Mathematics::Vector3 name() const { return ReadVec3(off); } \
    void name(OpenTK::Mathematics::Vector3 value) { WriteVec3(off, value); }
#define MPH_MEM_VEC4(name, off) \
    [[nodiscard]] OpenTK::Mathematics::Vector4 name() const { return ReadVec4(off); } \
    void name(OpenTK::Mathematics::Vector4 value) { WriteVec4(off, value); }
#define MPH_MEM_MTX(name, off) \
    [[nodiscard]] OpenTK::Mathematics::Matrix4x3 name() const { return ReadMtx43(off); } \
    void name(OpenTK::Mathematics::Matrix4x3 value) { WriteMtx43(off, value); }
#define MPH_MEM_ENUM8(type, name, off) \
    [[nodiscard]] ::MphRead::type name() const { return static_cast<::MphRead::type>(ReadByte(off)); } \
    void name(::MphRead::type value) { WriteByte(off, static_cast<std::uint8_t>(value)); }
#define MPH_MEM_ENUM16(type, name, off) \
    [[nodiscard]] ::MphRead::type name() const { return static_cast<::MphRead::type>(ReadUInt16(off)); } \
    void name(::MphRead::type value) { WriteUInt16(off, static_cast<std::uint16_t>(value)); }
#define MPH_MEM_ENUMI32(type, name, off) \
    [[nodiscard]] ::MphRead::type name() const { return static_cast<::MphRead::type>(ReadInt32(off)); } \
    void name(::MphRead::type value) { WriteInt32(off, static_cast<std::int32_t>(value)); }
#define MPH_MEM_ENUM32(type, name, off) \
    [[nodiscard]] ::MphRead::type name() const { return static_cast<::MphRead::type>(ReadUInt32(off)); } \
    void name(::MphRead::type value) { WriteUInt32(off, static_cast<std::uint32_t>(value)); }
#define MPH_MEM_CHILD(type, name) \
private: \
    std::shared_ptr<type> _##name; \
public: \
    [[nodiscard]] std::shared_ptr<type> name() const noexcept { return _##name; }

class CEntity : public MemoryClass
{
public:
    CEntity(Memory& memory, std::int32_t address);
    CEntity(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM16(EntityType, EntityType, 0x0);
    MPH_MEM_U16(EntityId, 0x2);
    MPH_MEM_U16(ScanId, 0x4);
    MPH_MEM_U16(Padding6, 0x6);
    MPH_MEM_PTR(MtxPtr, 0x8);
    MPH_MEM_PTR(Funcs, 0xC);
    MPH_MEM_PTR(Prev, 0x10);
    MPH_MEM_PTR(Next, 0x14);
};

class CEnemyBase : public CEntity
{
public:
    CEnemyBase(Memory& memory, std::int32_t address);
    CEnemyBase(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Flags, 0x18);
    MPH_MEM_ENUM8(EnemyType, Type, 0x1A);
    MPH_MEM_U8(State, 0x1B);
    MPH_MEM_U8(NextSubId, 0x1C);
    MPH_MEM_U8(HealthbarMsgId, 0x1D);
    MPH_MEM_U8(TimeSinceDmg, 0x1E);
    MPH_MEM_U8(HitPlayerBits, 0x1F);
    MPH_MEM_U32(Effectiveness, 0x20);
    MPH_MEM_PTR(Owner, 0x24);
    MPH_MEM_VEC3(LinkInvPos, 0x28);
    MPH_MEM_VEC3(LinkInvVec2, 0x34);
    MPH_MEM_VEC3(LinkInvVec1, 0x40);
    MPH_MEM_VEC3(Pos, 0x4C);
    MPH_MEM_VEC3(PrevPos, 0x58);
    MPH_MEM_VEC3(Speed, 0x64);
    MPH_MEM_VEC3(Vec2, 0x77);
    MPH_MEM_VEC3(Vec1, 0x7C);
    MPH_MEM_I32(BoundingRadius, 0x88);
    MPH_MEM_I32(Scale, 0x10C);
    MPH_MEM_U16(Health, 0x110);
    MPH_MEM_U16(HealthMax, 0x112);
    MPH_MEM_PTR(NodeRef, 0x114);
    MPH_MEM_PTR(Subroutine, 0x164);
    MPH_MEM_U16(Unused168, 0x168);
    MPH_MEM_U16(Padding16A, 0x16A);
    MPH_MEM_I32(Unused16C, 0x16C);
    MPH_MEM_CHILD(CollisionVolume, HurtVolUnxf);
    MPH_MEM_CHILD(CollisionVolume, HurtVol);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CEnemy24 : public CEnemyBase
{
public:
    CEnemy24(Memory& memory, std::int32_t address);
    CEnemy24(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(RegenMdl, 0x170);
    MPH_MEM_PTR(RegenAnim, 0x174);
    MPH_MEM_PTR(Colors, 0x1C0);
    MPH_MEM_PTR(Head, 0x1C4);
    MPH_MEM_PTR(GoreaB, 0x1DC);
    MPH_MEM_PTR(SpineNode, 0x1E0);
    MPH_MEM_I32(Field1E4, 0x1E4);
    MPH_MEM_I32(SpeedFactor, 0x228);
    MPH_MEM_U8(ArmBits, 0x22C);
    MPH_MEM_U8(WeaponId, 0x22D);
    MPH_MEM_U16(Unused22E, 0x22E);
    MPH_MEM_VEC3(TargetFacing, 0x230);
    MPH_MEM_U16(Field23C, 0x23C);
    MPH_MEM_U16(Field23E, 0x23E);
    MPH_MEM_U16(Field240, 0x240);
    MPH_MEM_U16(Field242, 0x242);
    MPH_MEM_U16(Field244, 0x244);
    MPH_MEM_U16(Field246, 0x246);
    MPH_MEM_I32(GoreaFlags, 0x248);
    MPH_MEM_U8(NextState, 0x24C);
    MPH_MEM_U8(Padding24D, 0x24D);
    MPH_MEM_U16(Padding24E, 0x24E);
    MPH_MEM_CHILD(CModel, Regen);
    MPH_MEM_CHILD(IntPtrArray, Arms);
    MPH_MEM_CHILD(IntPtrArray, Legs);
    MPH_MEM_CHILD(CollisionVolume, Volume);
};

class CEnemy25 : public CEnemyBase
{
public:
    CEnemy25(Memory& memory, std::int32_t address);
    CEnemy25(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(AttachNode, 0x170);
    MPH_MEM_PTR(GoreaOwner, 0x174);
    MPH_MEM_PTR(FlashEffect, 0x178);
    MPH_MEM_U16(Damage, 0x17C);
    MPH_MEM_U16(Padding17E, 0x17E);
};

class CEnemy26 : public CEnemyBase
{
public:
    CEnemy26(Memory& memory, std::int32_t address);
    CEnemy26(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(ShoulderNode, 0x170);
    MPH_MEM_PTR(UpperArmNode, 0x174);
    MPH_MEM_PTR(ElbowNode, 0x178);
    MPH_MEM_PTR(GoreaOwner, 0x17C);
    MPH_MEM_PTR(ShotEffect, 0x180);
    MPH_MEM_PTR(DmgEffect, 0x184);
    MPH_MEM_U16(RegenTimer, 0x19C);
    MPH_MEM_U16(ColorInc, 0x19E);
    MPH_MEM_U16(Ammo, 0x1A0);
    MPH_MEM_U16(Cooldown, 0x1A2);
    MPH_MEM_U16(DamageTo, 0x1A4);
    MPH_MEM_U8(Index, 0x1A6);
    MPH_MEM_U8(ArmFlags, 0x1A7);
    MPH_MEM_CHILD(EquipInfo, EquipInfo);
};

class CEnemy27 : public CEnemyBase
{
public:
    CEnemy27(Memory& memory, std::int32_t address);
    CEnemy27(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(KneeNode, 0x170);
    MPH_MEM_PTR(GoreaOwner, 0x174);
    MPH_MEM_U16(Unused178, 0x178);
    MPH_MEM_U8(Index, 0x17A);
    MPH_MEM_U8(Padding17B, 0x17B);
};

class CEnemy28 : public CEnemyBase
{
public:
    CEnemy28(Memory& memory, std::int32_t address);
    CEnemy28(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(SpineNode, 0x170);
    MPH_MEM_PTR(SealSphere, 0x174);
    MPH_MEM_PTR(GoreaOwner, 0x178);
    MPH_MEM_VEC3(TargetFacing, 0x1BC);
    MPH_MEM_U16(Field1C8, 0x1C8);
    MPH_MEM_U16(Field1CA, 0x1CA);
    MPH_MEM_U16(Field1CC, 0x1CC);
    MPH_MEM_U8(PhasesLeft, 0x1CE);
    MPH_MEM_U8(GoreaFlags, 0x1CF);
    MPH_MEM_CHILD(CollisionVolume, Volume);
    MPH_MEM_CHILD(IntPtrArray, Trocras);
};

class Enemy29Fields : public MemoryClass
{
public:
    Enemy29Fields(Memory& memory, std::int32_t address);
    Enemy29Fields(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(VecsReference, 0x0);
    MPH_MEM_PTR(MtxsReference, 0x4);
    MPH_MEM_PTR(IntsReference, 0x8);
    MPH_MEM_PTR(ShortsReference, 0xC);
    MPH_MEM_VEC3(Field10, 0x10);
    MPH_MEM_I32(Count1, 0x1C);
    MPH_MEM_I32(Count2, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Unused2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Unused3C, 0x3C);
    MPH_MEM_I32(Unused40, 0x40);
    MPH_MEM_I32(Unused44, 0x44);
    MPH_MEM_I32(Unused48, 0x48);
    MPH_MEM_I32(Unused4C, 0x4C);
    MPH_MEM_I32(Unused50, 0x50);
    MPH_MEM_I32(Unused54, 0x54);
    MPH_MEM_I32(Unused58, 0x58);
    MPH_MEM_I32(Unused5C, 0x5C);
    MPH_MEM_CHILD(StructArray<VecFx32>, Vecs);
    MPH_MEM_CHILD(StructArray<MtxFx43>, Mtxs);
    MPH_MEM_CHILD(Int32Array, Ints);
    MPH_MEM_CHILD(Int16Array, Shorts);
};

class CEnemy29 : public CEnemyBase
{
public:
    CEnemy29(Memory& memory, std::int32_t address);
    CEnemy29(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(MindTrickMdl, 0x170);
    MPH_MEM_PTR(MindTrickAnim, 0x174);
    MPH_MEM_PTR(GrappleMdl, 0x1C0);
    MPH_MEM_PTR(GrappleAnim, 0x1C4);
    MPH_MEM_PTR(AttachNode, 0x210);
    MPH_MEM_PTR(GoreaOwner, 0x214);
    MPH_MEM_PTR(FieldsReference, 0x218);
    MPH_MEM_U16(Field21C, 0x21C);
    MPH_MEM_U16(Field21E, 0x21E);
    MPH_MEM_I32(Unused220, 0x220);
    MPH_MEM_I32(Field224, 0x224);
    MPH_MEM_U8(Grappling, 0x228);
    MPH_MEM_U8(Padding229, 0x229);
    MPH_MEM_U16(Padding22A, 0x22A);
    MPH_MEM_COLOR(Ambient, 0x22C);
    MPH_MEM_COLOR(Diffuse, 0x22F);
    MPH_MEM_U16(DamageTo, 0x232);
    MPH_MEM_U16(Field234, 0x234);
    MPH_MEM_U16(DmgTimer, 0x236);
    MPH_MEM_U8(Unused238, 0x238);
    MPH_MEM_U8(Padding239, 0x239);
    MPH_MEM_U16(Padding23A, 0x23A);
    MPH_MEM_PTR(GrappleEffect, 0x23C);
    MPH_MEM_CHILD(CModel, MindTrick);
    MPH_MEM_CHILD(CModel, Grapple);
    MPH_MEM_CHILD(Enemy29Fields, Fields);
};

class CEnemy30 : public CEnemyBase
{
public:
    CEnemy30(Memory& memory, std::int32_t address);
    CEnemy30(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(GoreaOwner, 0x170);
    MPH_MEM_VEC3(Field174, 0x174);
    MPH_MEM_I32(Index, 0x180);
    MPH_MEM_U16(Field184, 0x184);
    MPH_MEM_U16(TrocraState, 0x186);
};

class CPlatform : public CEntity
{
public:
    CPlatform(Memory& memory, std::int32_t address);
    CPlatform(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(NoPort, 0x18);
    MPH_MEM_I32(ModelId, 0x1C);
    MPH_MEM_PTR(ScanEventTarget, 0x20);
    MPH_MEM_I32(MovementType, 0x24);
    MPH_MEM_I32(ForCutscene, 0x28);
    MPH_MEM_I32(ReverseType, 0x2C);
    MPH_MEM_ENUMI32(PlatformFlags, PlatformFlags, 0x30);
    MPH_MEM_U16(CollisionDamage, 0x34);
    MPH_MEM_U16(Padding36, 0x36);
    MPH_MEM_VEC3(BeamSpawnDir, 0x38);
    MPH_MEM_I32(BeamIndex, 0x44);
    MPH_MEM_U16(BeamInterval, 0x48);
    MPH_MEM_U16(Padding4A, 0x4A);
    MPH_MEM_I32(BeamOnIntervals, 0x4C);
    MPH_MEM_I32(ResistEffId, 0x50);
    MPH_MEM_I32(Effectiveness, 0x54);
    MPH_MEM_I32(DamageEffId, 0x58);
    MPH_MEM_I32(DeadEffId, 0x5C);
    MPH_MEM_I32(Unused60, 0x60);
    MPH_MEM_I32(Unused64, 0x64);
    MPH_MEM_ENUM32(PlatStateFlags, PlatStateFlags, 0x68);
    MPH_MEM_I32(CollisionBits, 0x6C);
    MPH_MEM_U16(TimeSincePlayerCol, 0x70);
    MPH_MEM_ENUM16(PlatAnimFlags, PlatAnimFlags, 0x72);
    MPH_MEM_I32(CurrentAnimId, 0x74);
    MPH_MEM_I32(CurrentAnim, 0x78);
    MPH_MEM_U8(FromIndex, 0x7C);
    MPH_MEM_U8(ToIndex, 0x7D);
    MPH_MEM_U8(State, 0x7E);
    MPH_MEM_ENUM8(PlatformState, PrevState, 0x7F);
    MPH_MEM_ENUM8(PlatformState, PosCount, 0x80);
    MPH_MEM_U8(Padding81, 0x81);
    MPH_MEM_U16(MoveTimer, 0x82);
    MPH_MEM_U16(RecoilTimer, 0x84);
    MPH_MEM_U16(Health, 0x86);
    MPH_MEM_U16(HealthMax, 0x88);
    MPH_MEM_U16(HalfHealth, 0x8A);
    MPH_MEM_U16(ParentId, 0x8C);
    MPH_MEM_U16(Padding8E, 0x8E);
    MPH_MEM_PTR(Parent, 0x90);
    MPH_MEM_U16(BeamAmmo, 0xA8);
    MPH_MEM_U16(PaddingAA, 0xAA);
    MPH_MEM_I32(UnusedAC, 0xAC);
    MPH_MEM_I32(UnusedB0, 0xB0);
    MPH_MEM_I32(UnusedB4, 0xB4);
    MPH_MEM_U16(BeamTimer, 0xB8);
    MPH_MEM_U16(BeamIntervalIndex, 0xBA);
    MPH_MEM_U16(DrawingBeam, 0xBC);
    MPH_MEM_U16(PaddingBE, 0xBE);
    MPH_MEM_PTR(Data, 0xC0);
    MPH_MEM_PTR(Positions, 0xC4);
    MPH_MEM_PTR(Rotations, 0xC8);
    MPH_MEM_VEC3(PosOffset, 0xCC);
    MPH_MEM_VEC3(VisiblePos, 0xD8);
    MPH_MEM_VEC3(PrevVisiblePos, 0xE4);
    MPH_MEM_VEC3(Position, 0xF0);
    MPH_MEM_VEC3(PrevPosition, 0xFC);
    MPH_MEM_VEC4(CurRotation, 0x108);
    MPH_MEM_VEC4(FromRotation, 0x118);
    MPH_MEM_VEC4(ToRotation, 0x128);
    MPH_MEM_I32(MovePct, 0x138);
    MPH_MEM_I32(MoveInc, 0x13C);
    MPH_MEM_I32(Unused140, 0x140);
    MPH_MEM_I32(Unused144, 0x144);
    MPH_MEM_VEC3(ColMin, 0x148);
    MPH_MEM_VEC3(ColW, 0x154);
    MPH_MEM_VEC3(ColMax, 0x160);
    MPH_MEM_VEC3(Velocity, 0x16C);
    MPH_MEM_I32(Unused178, 0x178);
    MPH_MEM_I32(Unused17C, 0x17C);
    MPH_MEM_I32(Unused180, 0x180);
    MPH_MEM_I32(ForwardSpeed, 0x184);
    MPH_MEM_I32(BackwardSpeed, 0x188);
    MPH_MEM_I32(SfxVolume, 0x18C);
    MPH_MEM_VEC3(Vec2, 0x190);
    MPH_MEM_VEC3(Vec1, 0x19C);
    MPH_MEM_PTR(MtxObj, 0x25C);
    MPH_MEM_U16(AttachNodeIndex, 0x260);
    MPH_MEM_U16(Padding26A, 0x26A);
    MPH_MEM_I32(Unused27C, 0x27C);
    MPH_MEM_I32(Unused280, 0x280);
    MPH_MEM_I32(Unused284, 0x284);
    MPH_MEM_I32(Unused288, 0x288);
    MPH_MEM_I32(Unused28C, 0x28C);
    MPH_MEM_I32(Unused290, 0x290);
    MPH_MEM_PTR(NodeRef, 0x294);
    MPH_MEM_PTR(Port, 0x2E0);
    MPH_MEM_PTR(HitEventTarget, 0x2E4);
    MPH_MEM_PTR(PlayerColEventTarget, 0x2E8);
    MPH_MEM_PTR(DeadEventTarget, 0x2EC);
    MPH_MEM_CHILD(EquipInfoPtr, EquipInfo);
    MPH_MEM_CHILD(EntityCollision, EntityCollision);
    MPH_MEM_CHILD(UInt16Array, Turrets);
    MPH_MEM_CHILD(IntPtrArray, Effects);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(ByteArray, LifetimeEventIndices);
    MPH_MEM_CHILD(IntPtrArray, LifetimeEventTargets);
    MPH_MEM_CHILD(U32EnumArray<Message>, LifetimeEventIds);
    MPH_MEM_CHILD(Int32Array, LifetimeEventParam1s);
    MPH_MEM_CHILD(Int32Array, LifetimeEventParam2s);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CObject : public CEntity
{
public:
    CObject(Memory& memory, std::int32_t address);
    CObject(Memory& memory, std::intptr_t address);
    MPH_MEM_U8(Flags, 0x18);
    MPH_MEM_U8(Field19, 0x19);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_I32(EffectFlags, 0x1C);
    MPH_MEM_U16(LinkedEntity, 0x20);
    MPH_MEM_U16(Field22, 0x22);
    MPH_MEM_VEC3(TempPos, 0x24);
    MPH_MEM_VEC3(TempVec2, 0x30);
    MPH_MEM_VEC3(TempVec1, 0x3C);
    MPH_MEM_U16(AttachNodeIndex, 0x48);
    MPH_MEM_U16(Field4A, 0x4A);
    MPH_MEM_VEC3(Pos, 0x4C);
    MPH_MEM_VEC3(Vec2, 0x58);
    MPH_MEM_VEC3(Vec1, 0x64);
    MPH_MEM_VEC3(SomePos, 0x70);
    MPH_MEM_PTR(Data, 0x7C);
    MPH_MEM_PTR(ScanEventTarget, 0x80);
    MPH_MEM_PTR(NodeRef, 0x84);
    MPH_MEM_I32(EffectProcessing, 0x240);
    MPH_MEM_I32(EffectId, 0x244);
    MPH_MEM_U16(EffectInterval, 0x248);
    MPH_MEM_U16(EffectActive, 0x24A);
    MPH_MEM_I32(EffectOnIntervals, 0x24C);
    MPH_MEM_PTR(Effect, 0x290);
    MPH_MEM_U16(EffectTimer, 0x294);
    MPH_MEM_U16(EffectIntervalIndex, 0x296);
    MPH_MEM_CHILD(StructArray<EntityCollision>, ColStructs);
    MPH_MEM_CHILD(IntPtrArray, MtxObjs);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(CollisionVolume, Volume);
    MPH_MEM_CHILD(SfxParameters, Sfx);
};

class CPlayerSpawn : public CEntity
{
public:
    CPlayerSpawn(Memory& memory, std::int32_t address);
    CPlayerSpawn(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Cooldown, 0x18);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_VEC3(Vec1, 0x1C);
    MPH_MEM_VEC3(Vec2, 0x28);
    MPH_MEM_VEC3(Pos, 0x34);
    MPH_MEM_U8(Initial, 0x40);
    MPH_MEM_U8(Active, 0x41);
    MPH_MEM_U8(TeamIndex, 0x42);
    MPH_MEM_U8(Field43, 0x43);
    MPH_MEM_PTR(NodeRef, 0x44);
};

class CDoor : public CEntity
{
public:
    CDoor(Memory& memory, std::int32_t address);
    CDoor(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Flags, 0x18);
    MPH_MEM_U8(DoorPaletteId, 0x1A);
    MPH_MEM_U8(Field1B, 0x1B);
    MPH_MEM_U8(Field1C, 0x1C);
    MPH_MEM_U8(Field1D, 0x1D);
    MPH_MEM_U16(Field1E, 0x1E);
    MPH_MEM_I32(SomeRoomId, 0x20);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Vec2, 0x30);
    MPH_MEM_VEC3(Pos, 0x3C);
    MPH_MEM_VEC3(LockPos, 0x48);
    MPH_MEM_I32(BoundingRadius, 0x54);
    MPH_MEM_I32(BoundingRadiusSquared, 0x58);
    MPH_MEM_ENUM32(DoorType, DoorType, 0xEC);
    MPH_MEM_I32(TargetRoom, 0xF0);
    MPH_MEM_PTR(Port, 0xF4);
    MPH_MEM_PTR(NodeRef, 0xF8);
    MPH_MEM_PTR(DoorNodeRef, 0xFC);
    MPH_MEM_PTR(Data, 0x104);
    MPH_MEM_CHILD(CModel, DoorModel);
    MPH_MEM_CHILD(CModel, LockModel);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CItemSpawn : public CEntity
{
public:
    CItemSpawn(Memory& memory, std::int32_t address);
    CItemSpawn(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(ItemEntityId, 0x18);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_VEC3(Field1C, 0x1C);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_VEC3(Pos, 0x40);
    MPH_MEM_PTR(Data, 0x4C);
    MPH_MEM_U8(Flags, 0x50);
    MPH_MEM_U8(Field51, 0x51);
    MPH_MEM_U16(Field52, 0x52);
    MPH_MEM_ENUM16(ItemType, Type, 0x54);
    MPH_MEM_U16(HasBase, 0x56);
    MPH_MEM_U16(MaxSpawnCount, 0x58);
    MPH_MEM_U16(SpawnInterval, 0x5A);
    MPH_MEM_U16(SpawnDelay, 0x5C);
    MPH_MEM_U16(SpawnCount, 0x5E);
    MPH_MEM_PTR(NodeRef, 0x60);
    MPH_MEM_PTR(ItemInstance, 0xAC);
    MPH_MEM_PTR(SomeEntity, 0xB0);
    MPH_MEM_I32(FieldB4, 0xB4);
    MPH_MEM_CHILD(CModel, BaseModel);
};

class CItemInstance : public CEntity
{
public:
    CItemInstance(Memory& memory, std::int32_t address);
    CItemInstance(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(ParentId, 0x18);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_VEC3(Field1C, 0x1C);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_VEC3(Pos, 0x40);
    MPH_MEM_ENUM16(ItemType, Type, 0x4C);
    MPH_MEM_I16(DespawnTimer, 0x4E);
    MPH_MEM_U16(RotationAngle, 0x50);
    MPH_MEM_U16(LinkDone, 0x52);
    MPH_MEM_PTR(Effect, 0x54);
    MPH_MEM_PTR(NodeRef, 0xA4);
    MPH_MEM_PTR(ItemBase, 0xA8);
    MPH_MEM_I32(FieldAC, 0xAC);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CEnemySpawn : public CEntity
{
public:
    CEnemySpawn(Memory& memory, std::int32_t address);
    CEnemySpawn(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM8(EnemyType, EnemyType, 0x18);
    MPH_MEM_U8(Padding19, 0x19);
    MPH_MEM_U16(Padding1A, 0x1A);
    MPH_MEM_PTR(RoomNodeRef, 0x1C);
    MPH_MEM_PTR(NodeRef, 0x20);
    MPH_MEM_U16(LinkedEntity, 0x24);
    MPH_MEM_U16(Padding26, 0x26);
    MPH_MEM_VEC3(LinkInvPos, 0x28);
    MPH_MEM_VEC3(LinkInvVec2, 0x34);
    MPH_MEM_VEC3(LinkInvVec1, 0x40);
    MPH_MEM_VEC3(Pos, 0x4C);
    MPH_MEM_VEC3(Vec2, 0x58);
    MPH_MEM_VEC3(Vec1, 0x64);
    MPH_MEM_ENUM8(SpawnerFlags, Flags, 0x70);
    MPH_MEM_U8(SpawnedCount, 0x71);
    MPH_MEM_U8(ActiveCount, 0x72);
    MPH_MEM_U8(Padding73, 0x73);
    MPH_MEM_U16(CooldownTimer, 0x74);
    MPH_MEM_U16(Padding76, 0x76);
    MPH_MEM_I32(ActiveDistSqr, 0x78);
    MPH_MEM_PTR(Entity1, 0x7C);
    MPH_MEM_PTR(Entity2, 0x80);
    MPH_MEM_PTR(Entity3, 0x84);
    MPH_MEM_PTR(Data, 0x88);
};

class CTriggerVolume : public CEntity
{
public:
    CTriggerVolume(Memory& memory, std::int32_t address);
    CTriggerVolume(Memory& memory, std::intptr_t address);
    MPH_MEM_U8(Flags, 0x18);
    MPH_MEM_U8(Type, 0x19);
    MPH_MEM_U16(TriggerDelay, 0x1A);
    MPH_MEM_U16(RequiredStateBit, 0x1C);
    MPH_MEM_U16(Field1E, 0x1E);
    MPH_MEM_I32(TriggerThreshold, 0x20);
    MPH_MEM_I32(TriggersNeeded, 0x24);
    MPH_MEM_ENUM32(TriggerFlags, TriggerFlags, 0x28);
    MPH_MEM_PTR(Data, 0x2C);
    MPH_MEM_PTR(Parent, 0x30);
    MPH_MEM_PTR(Child, 0x34);
    MPH_MEM_CHILD(CollisionVolume, Volume);
};

class CAreaVolume : public CEntity
{
public:
    CAreaVolume(Memory& memory, std::int32_t address);
    CAreaVolume(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Pos, 0x18);
    MPH_MEM_VEC3(Vec2, 0x24);
    MPH_MEM_VEC3(Vec1, 0x30);
    MPH_MEM_U8(Active, 0x3C);
    MPH_MEM_U8(AllowMultiple, 0x3D);
    MPH_MEM_U8(EventDelay, 0x3E);
    MPH_MEM_U8(Padding47, 0x47);
    MPH_MEM_ENUM32(Message, InsideEventId, 0x48);
    MPH_MEM_I32(InsideEventParam1, 0x4C);
    MPH_MEM_I32(InsideEventParam2, 0x50);
    MPH_MEM_ENUM32(Message, ExitEventId, 0x54);
    MPH_MEM_I32(ExitEventParam1, 0x58);
    MPH_MEM_I32(ExitEventParam2, 0x5C);
    MPH_MEM_ENUM32(TriggerFlags, TriggerFlags, 0x60);
    MPH_MEM_U16(Priority, 0x64);
    MPH_MEM_U16(Cooldown, 0x66);
    MPH_MEM_PTR(Parent, 0x70);
    MPH_MEM_PTR(Child, 0x74);
    MPH_MEM_PTR(NodeRef, 0x78);
    MPH_MEM_CHILD(ByteArray, TriggeredSlots);
    MPH_MEM_CHILD(ByteArray, PrioritySlots);
    MPH_MEM_CHILD(UInt16Array, CooldownSlots);
    MPH_MEM_CHILD(CollisionVolume, Volume);
};

class CJumpPad : public CEntity
{
public:
    CJumpPad(Memory& memory, std::int32_t address);
    CJumpPad(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(ParentId, 0x18);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_VEC3(Field1C, 0x1C);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_VEC3(Pos, 0x40);
    MPH_MEM_VEC3(BaseVec2, 0x4C);
    MPH_MEM_VEC3(BaseVec1, 0x58);
    MPH_MEM_MTX(BaseMtx, 0x64);
    MPH_MEM_MTX(BeamMtx, 0x94);
    MPH_MEM_PTR(Data, 0xC4);
    MPH_MEM_U16(CooldownTime, 0xC8);
    MPH_MEM_U16(CooldownTimer, 0xCA);
    MPH_MEM_U8(UsedState, 0xCC);
    MPH_MEM_U8(Flags1, 0xCD);
    MPH_MEM_U16(FieldCE, 0xCE);
    MPH_MEM_ENUM32(TriggerFlags, TriggerFlags, 0xD0);
    MPH_MEM_U16(FieldD4, 0xD4);
    MPH_MEM_U16(Timer, 0xD6);
    MPH_MEM_VEC3(BeamVec, 0xD8);
    MPH_MEM_PTR(NodeRef, 0x164);
    MPH_MEM_I32(BaseId, 0x1F8);
    MPH_MEM_I32(BeamId, 0x1FC);
    MPH_MEM_PTR(NodedataRelated, 0x200);
    MPH_MEM_CHILD(CollisionVolume, FieldE4);
    MPH_MEM_CHILD(CollisionVolume, Volume);
    MPH_MEM_CHILD(CModel, BaseModel);
    MPH_MEM_CHILD(CModel, BeamModel);
};

class CPointModule : public CEntity
{
public:
    CPointModule(Memory& memory, std::int32_t address);
    CPointModule(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1C);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_PTR(Field28, 0x28);
    MPH_MEM_PTR(Field2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_PTR(Field38, 0x38);
    MPH_MEM_I32(Flags, 0x3C);
};

class CMorphCamera : public CEntity
{
public:
    CMorphCamera(Memory& memory, std::int32_t address);
    CMorphCamera(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Pos, 0x18);
    MPH_MEM_PTR(NodeRef, 0x64);
    MPH_MEM_CHILD(CollisionVolume, Volume);
};

class COctolithFlag : public CEntity
{
public:
    COctolithFlag(Memory& memory, std::int32_t address);
    COctolithFlag(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec2, 0x18);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_VEC3(BasePos, 0x3C);
    MPH_MEM_I32(HeightBob, 0x48);
    MPH_MEM_U8(TeamIndex, 0x4C);
    MPH_MEM_U8(Flags, 0x4D);
    MPH_MEM_U16(Field4E, 0x4E);
    MPH_MEM_I32(DespawnTimer, 0x50);
    MPH_MEM_PTR(Player, 0x54);
    MPH_MEM_PTR(LastPlayer, 0x58);
    MPH_MEM_PTR(NodedataRelated, 0xEC);
    MPH_MEM_I32(FieldF0, 0xF0);
    MPH_MEM_CHILD(CModel, BaseModel);
    MPH_MEM_CHILD(CModel, OctoModel);
};

class CFlagBase : public CEntity
{
public:
    CFlagBase(Memory& memory, std::int32_t address);
    CFlagBase(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec2, 0x18);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_U8(TeamId, 0x7C);
    MPH_MEM_U8(Field7D, 0x7D);
    MPH_MEM_U16(Field7E, 0x7E);
    MPH_MEM_PTR(NodedataRelated, 0xC8);
    MPH_MEM_CHILD(CollisionVolume, Volume);
    MPH_MEM_CHILD(CModel, Model);
};

class CTeleporter : public CEntity
{
public:
    CTeleporter(Memory& memory, std::int32_t address);
    CTeleporter(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec2, 0x18);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_VEC3(TargetPos, 0x3C);
    MPH_MEM_U8(Flags, 0x48);
    MPH_MEM_U8(Field49, 0x49);
    MPH_MEM_U8(ArtifactId, 0x4A);
    MPH_MEM_U8(Field4B, 0x4B);
    MPH_MEM_U8(Field4C, 0x4C);
    MPH_MEM_U8(Field4D, 0x4D);
    MPH_MEM_U16(Field4E, 0x4E);
    MPH_MEM_I32(ConnectorId, 0x50);
    MPH_MEM_PTR(NodeRef, 0x54);
    MPH_MEM_PTR(Node, 0x58);
    MPH_MEM_CHILD(CModel, TeleModel);
    MPH_MEM_CHILD(CModel, ArtifactModel);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CNodeDefense : public CEntity
{
public:
    CNodeDefense(Memory& memory, std::int32_t address);
    CNodeDefense(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec2, 0x18);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_I32(Rotation, 0x3C);
    MPH_MEM_I32(RotSpeed, 0x40);
    MPH_MEM_U8(TeamIndex, 0x84);
    MPH_MEM_U8(OccupyingTeam, 0x85);
    MPH_MEM_U8(OccupyFlags, 0x86);
    MPH_MEM_U8(Occupied, 0x87);
    MPH_MEM_U8(Flags, 0x88);
    MPH_MEM_U8(Field89, 0x89);
    MPH_MEM_U16(Field8A, 0x8A);
    MPH_MEM_I32(Progress, 0x8C);
    MPH_MEM_I32(Field90, 0x90);
    MPH_MEM_PTR(SomePlayer, 0x94);
    MPH_MEM_PTR(NodedataRelated, 0x12C);
    MPH_MEM_CHILD(CollisionVolume, Volume);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
    MPH_MEM_CHILD(CModel, RingModel);
    MPH_MEM_CHILD(CModel, NodeModel);
};

class CLightSource : public CEntity
{
public:
    CLightSource(Memory& memory, std::int32_t address);
    CLightSource(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(Data, 0x18);
    MPH_MEM_CHILD(CollisionVolume, Volume);
};

class CArtifact : public CEntity
{
public:
    CArtifact(Memory& memory, std::int32_t address);
    CArtifact(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(LinkedEntityId, 0x18);
    MPH_MEM_U16(Field1A, 0x1A);
    MPH_MEM_VEC3(Field1C, 0x1C);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2C);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_VEC3(Vec2, 0x40);
    MPH_MEM_VEC3(Vec1, 0x4C);
    MPH_MEM_VEC3(Pos, 0x58);
    MPH_MEM_U8(Active, 0x64);
    MPH_MEM_U8(ModelId, 0x65);
    MPH_MEM_U8(ArtifactId, 0x66);
    MPH_MEM_U8(Field67, 0x67);
    MPH_MEM_U16(FoundLinked, 0x68);
    MPH_MEM_U16(Field6A, 0x6A);
    MPH_MEM_PTR(Entity1, 0x6C);
    MPH_MEM_PTR(Entity2, 0x70);
    MPH_MEM_PTR(Entity3, 0x74);
    MPH_MEM_PTR(Data, 0x78);
    MPH_MEM_PTR(NodeRef, 0x10C);
    MPH_MEM_I32(Field114, 0x114);
    MPH_MEM_I32(Field118, 0x118);
    MPH_MEM_I32(Field11C, 0x11C);
    MPH_MEM_I32(Field120, 0x120);
    MPH_MEM_I32(Field124, 0x124);
    MPH_MEM_CHILD(CModel, ArtifactModel);
    MPH_MEM_CHILD(CModel, BaseModel);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CCameraSequence : public CEntity
{
public:
    CCameraSequence(Memory& memory, std::int32_t address);
    CCameraSequence(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(Data, 0x18);
    MPH_MEM_PTR(Entity1, 0x1C);
    MPH_MEM_PTR(Entity2, 0x20);
    MPH_MEM_PTR(EventTarget, 0x24);
    MPH_MEM_U8(Flags, 0x28);
    MPH_MEM_U8(Padding29, 0x29);
    MPH_MEM_U16(DelayTimer, 0x2A);
};

class CForceField : public CEntity
{
public:
    CForceField(Memory& memory, std::int32_t address);
    CForceField(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Normal, 0x18);
    MPH_MEM_U8(Flags, 0x24);
    MPH_MEM_U8(Alpha, 0x25);
    MPH_MEM_U16(Field26, 0x26);
    MPH_MEM_PTR(Data, 0x28);
    MPH_MEM_PTR(NodeRef, 0x2C);
    MPH_MEM_PTR(Lock, 0x30);
    MPH_MEM_VEC3(Vector2, 0x34);
    MPH_MEM_I32(Field40, 0x40);
    MPH_MEM_CHILD(CModel, Model);
};

class CBeamEffect : public CEntity
{
public:
    CBeamEffect(Memory& memory, std::int32_t address);
    CBeamEffect(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec1, 0x18);
    MPH_MEM_VEC3(Vec2, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_PTR(DrawOffset, 0x3C);
    MPH_MEM_VEC3(Speed, 0x40);
    MPH_MEM_U8(Flags, 0x4C);
    MPH_MEM_U8(BeamType, 0x4D);
    MPH_MEM_U16(Lifespan, 0x4E);
    MPH_MEM_U16(Age, 0x50);
    MPH_MEM_U16(Field52, 0x52);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_I32(Field58, 0x58);
    MPH_MEM_I32(Field5C, 0x5C);
    MPH_MEM_I32(Field60, 0x60);
    MPH_MEM_I32(Field64, 0x64);
    MPH_MEM_I32(ScaleX, 0x68);
    MPH_MEM_I32(ScaleY, 0x6C);
    MPH_MEM_I32(ScaleZ, 0x70);
    MPH_MEM_I32(DrawDist, 0x74);
    MPH_MEM_PTR(EffMtxPtr, 0xC0);
    MPH_MEM_CHILD(CModel, Model);
};

class CBomb : public CEntity
{
public:
    CBomb(Memory& memory, std::int32_t address);
    CBomb(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Pos, 0x18);
    MPH_MEM_VEC3(Vec1, 0x24);
    MPH_MEM_VEC3(Vec2, 0x30);
    MPH_MEM_VEC3(SpeedMaybe, 0x3C);
    MPH_MEM_U8(BombType, 0x48);
    MPH_MEM_U8(SiblingCount, 0x49);
    MPH_MEM_U8(Flags, 0x4A);
    MPH_MEM_U8(Field4B, 0x4B);
    MPH_MEM_U16(Countdown, 0x4C);
    MPH_MEM_U16(Field4E, 0x4E);
    MPH_MEM_U16(Field50, 0x50);
    MPH_MEM_U16(Field52, 0x52);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_I32(Field58, 0x58);
    MPH_MEM_I32(Field5C, 0x5C);
    MPH_MEM_PTR(Owner, 0x60);
    MPH_MEM_PTR(OwnerSylux, 0x64);
    MPH_MEM_PTR(RoomNodeRef, 0xB0);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CHalfturret : public CEntity
{
public:
    CHalfturret(Memory& memory, std::int32_t address);
    CHalfturret(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Vec2, 0x18);
    MPH_MEM_VEC3(Field24, 0x24);
    MPH_MEM_VEC3(Pos, 0x30);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_PTR(Owner, 0x40);
    MPH_MEM_PTR(Target, 0x44);
    MPH_MEM_U8(Field67, 0x67);
    MPH_MEM_PTR(BurnEffect, 0x7C);
    MPH_MEM_PTR(NodeRef, 0xC8);
    MPH_MEM_I32(FieldCC, 0xCC);
    MPH_MEM_U16(Health, 0xD0);
    MPH_MEM_U16(FieldD2, 0xD2);
    MPH_MEM_U16(BurnTimer, 0xD4);
    MPH_MEM_U16(FieldD6, 0xD6);
    MPH_MEM_U8(FieldD8, 0xD8);
    MPH_MEM_U8(Frozen, 0xD9);
    MPH_MEM_U8(FieldDA, 0xDA);
    MPH_MEM_U8(FieldDB, 0xDB);
    MPH_MEM_U8(FieldDC, 0xDC);
    MPH_MEM_U8(FieldDD, 0xDD);
    MPH_MEM_U16(FieldDE, 0xDE);
    MPH_MEM_I32(FieldE0, 0xE0);
    MPH_MEM_CHILD(LightInfo, LightInfo);
    MPH_MEM_CHILD(EquipInfoPtr, EquipInfo);
    MPH_MEM_CHILD(CModel, Model);
};

class CPlayer : public CEntity
{
public:
    CPlayer(Memory& memory, std::int32_t address);
    CPlayer(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_VEC3(Pos, 0x1c);
    MPH_MEM_VEC3(PrevPos, 0x28);
    MPH_MEM_VEC3(Speed, 0x34);
    MPH_MEM_VEC3(PrevSpeed, 0x40);
    MPH_MEM_VEC3(Vec2, 0x4c);
    MPH_MEM_VEC3(GunEffVec2, 0x58);
    MPH_MEM_VEC3(Vec1, 0x64);
    MPH_MEM_VEC3(Field70, 0x70);
    MPH_MEM_I32(Field7C, 0x7c);
    MPH_MEM_I32(Field80, 0x80);
    MPH_MEM_I32(Field84, 0x84);
    MPH_MEM_I32(Field88, 0x88);
    MPH_MEM_I32(Field8C, 0x8c);
    MPH_MEM_I32(HSpeedMag, 0x90);
    MPH_MEM_I32(Field94, 0x94);
    MPH_MEM_I32(GravityValue, 0x98);
    MPH_MEM_VEC3(GunEffVec1, 0x9c);
    MPH_MEM_VEC3(AimTargetPos, 0xa8);
    MPH_MEM_VEC3(FieldB4, 0xb4);
    MPH_MEM_VEC3(FieldC0, 0xc0);
    MPH_MEM_VEC3(SomeSpeedLoss, 0xcc);
    MPH_MEM_U16(SomeSpeedCounter, 0xd8);
    MPH_MEM_U16(Energy, 0xda);
    MPH_MEM_U16(EnergyCap, 0xdc);
    MPH_MEM_U16(RecoveryTicksMaybe, 0xde);
    MPH_MEM_U8(SomeTimer1, 0xe0);
    MPH_MEM_U8(SomeTimer2, 0xe1);
    MPH_MEM_U8(DeathCountdownMaybe, 0xe2);
    MPH_MEM_U8(FieldE3, 0xe3);
    MPH_MEM_U16(FieldE4, 0xe4);
    MPH_MEM_U16(FieldE6, 0xe6);
    MPH_MEM_I32(FieldE8, 0xe8);
    MPH_MEM_VEC3(Dir2, 0xec);
    MPH_MEM_U16(JumpPadCountdown, 0xf8);
    MPH_MEM_U8(JumpPadMin5s, 0xfa);
    MPH_MEM_U8(TimeSinceJumpPadMaybe, 0xfb);
    MPH_MEM_I32(FieldFC, 0xfc);
    MPH_MEM_U8(Field104, 0x104);
    MPH_MEM_U8(Field105, 0x105);
    MPH_MEM_U16(Field106, 0x106);
    MPH_MEM_U8(BoostBallCharge, 0x148);
    MPH_MEM_U8(SomeDamage, 0x149);
    MPH_MEM_U8(BoostCooldown, 0x14a);
    MPH_MEM_U8(Field14B, 0x14b);
    MPH_MEM_U16(UniversalAmmo, 0x14c);
    MPH_MEM_U16(Missiles, 0x14e);
    MPH_MEM_U16(UniversalAmmoCap, 0x150);
    MPH_MEM_U16(MissilesCap, 0x152);
    MPH_MEM_U8(Field154, 0x154);
    MPH_MEM_U8(Field155, 0x155);
    MPH_MEM_U16(Field156, 0x156);
    MPH_MEM_U8(WeaponSlot0, 0x158);
    MPH_MEM_U8(WeaponSlot1, 0x159);
    MPH_MEM_U8(WeaponSlot2, 0x15a);
    MPH_MEM_U8(GunAnimation, 0x15b);
    MPH_MEM_VEC3(Field1EC, 0x1ec);
    MPH_MEM_VEC3(MuzzlePos, 0x1f8);
    MPH_MEM_PTR(FurlEffect, 0x204);
    MPH_MEM_PTR(EffectBoost, 0x208);
    MPH_MEM_PTR(EffectMuzzle, 0x20c);
    MPH_MEM_PTR(EffectCharge, 0x210);
    MPH_MEM_PTR(EffectDblDmg, 0x214);
    MPH_MEM_PTR(EffectDeathalt, 0x218);
    MPH_MEM_U16(Field2BC, 0x2bc);
    MPH_MEM_U16(Field2BE, 0x2be);
    MPH_MEM_U8(SmokeAlphaMaybe, 0x350);
    MPH_MEM_U8(Field351, 0x351);
    MPH_MEM_U16(Field352, 0x352);
    MPH_MEM_PTR(Field354, 0x354);
    MPH_MEM_PTR(AttachedEnemy, 0x358);
    MPH_MEM_PTR(Field35C, 0x35c);
    MPH_MEM_U8(Field360, 0x360);
    MPH_MEM_U8(Field361, 0x361);
    MPH_MEM_U16(Field362, 0x362);
    MPH_MEM_ENUM8(Hunter, HunterId, 0x400);
    MPH_MEM_U8(Field401, 0x401);
    MPH_MEM_U16(Field402, 0x402);
    MPH_MEM_PTR(Field404, 0x404);
    MPH_MEM_I32(Field408, 0x408);
    MPH_MEM_I32(Field40C, 0x40c);
    MPH_MEM_I32(Field410, 0x410);
    MPH_MEM_I32(Field414, 0x414);
    MPH_MEM_I32(Field418, 0x418);
    MPH_MEM_I32(Field41C, 0x41c);
    MPH_MEM_I32(Field420, 0x420);
    MPH_MEM_I32(Field424, 0x424);
    MPH_MEM_I32(Field428, 0x428);
    MPH_MEM_I32(Field42C, 0x42c);
    MPH_MEM_I32(Field430, 0x430);
    MPH_MEM_U8(TimeSinceShot, 0x434);
    MPH_MEM_U8(Field435, 0x435);
    MPH_MEM_U16(Field436, 0x436);
    MPH_MEM_U16(Field438, 0x438);
    MPH_MEM_U16(Field43A, 0x43a);
    MPH_MEM_U16(ShockCoilTimer, 0x43c);
    MPH_MEM_U16(Field43E, 0x43e);
    MPH_MEM_PTR(ShockCoilTarget, 0x440);
    MPH_MEM_U8(TimeSinceDmg, 0x444);
    MPH_MEM_U8(TimeSincePickup, 0x445);
    MPH_MEM_U8(TimeSinceHeal, 0x446);
    MPH_MEM_U8(Field447, 0x447);
    MPH_MEM_U8(Field448, 0x448);
    MPH_MEM_U8(Field449, 0x449);
    MPH_MEM_U16(Field44A, 0x44a);
    MPH_MEM_I32(Field44C, 0x44c);
    MPH_MEM_I32(Field450, 0x450);
    MPH_MEM_PTR(RoomNodeRef, 0x454);
    MPH_MEM_U16(RespawnTimer, 0x458);
    MPH_MEM_U16(Field45A, 0x45a);
    MPH_MEM_PTR(Field45C, 0x45c);
    MPH_MEM_U16(Field460, 0x460);
    MPH_MEM_U16(Field462, 0x462);
    MPH_MEM_U8(Field4AC, 0x4ac);
    MPH_MEM_U8(TeamIndex, 0x4ad);
    MPH_MEM_U8(Field4AE, 0x4ae);
    MPH_MEM_U8(Field4AF, 0x4af);
    MPH_MEM_U16(DoubleDamageTimer, 0x4b0);
    MPH_MEM_U16(CloakTimer, 0x4b2);
    MPH_MEM_U16(DeathaltTimer, 0x4b4);
    MPH_MEM_U16(DisruptTimer, 0x4b6);
    MPH_MEM_U16(FreezeTimer, 0x4b8);
    MPH_MEM_U8(TimeSinceFrozen, 0x4ba);
    MPH_MEM_U8(Frozen, 0x4bb);
    MPH_MEM_U8(CurAlpha, 0x4bc);
    MPH_MEM_U8(TargetAlpha, 0x4bd);
    MPH_MEM_U8(ShotCooldownRelated, 0x4be);
    MPH_MEM_U8(Field4BF, 0x4bf);
    MPH_MEM_U8(Field4C0, 0x4c0);
    MPH_MEM_U8(Field4C1, 0x4c1);
    MPH_MEM_U16(Field4C2, 0x4c2);
    MPH_MEM_U32(SomeFlags, 0x4c4);
    MPH_MEM_U32(MoreFlags, 0x4c8);
    MPH_MEM_U16(AbilityFlags, 0x4cc);
    MPH_MEM_U8(CurrentWeapon, 0x4ce);
    MPH_MEM_U8(WeaponSelection, 0x4cf);
    MPH_MEM_U8(SomeWeapon, 0x4d0);
    MPH_MEM_U8(Field4D1, 0x4d1);
    MPH_MEM_U8(AvailableWeapons, 0x4d2);
    MPH_MEM_U8(OmegaCannon, 0x4d3);
    MPH_MEM_U8(AvailableCharges, 0x4d4);
    MPH_MEM_U8(Field4D5, 0x4d5);
    MPH_MEM_U8(ViewType, 0x4d6);
    MPH_MEM_U8(ViewPlayer, 0x4d7);
    MPH_MEM_U8(Field4D8, 0x4d8);
    MPH_MEM_U8(Field4D9, 0x4d9);
    MPH_MEM_U16(Field4DA, 0x4da);
    MPH_MEM_I32(Field4DC, 0x4dc);
    MPH_MEM_I32(Field4E0, 0x4e0);
    MPH_MEM_I32(Field4E4, 0x4e4);
    MPH_MEM_VEC3(Field4E8, 0x4e8);
    MPH_MEM_MTX(Transform, 0x4f4);
    MPH_MEM_I32(Field524, 0x524);
    MPH_MEM_I32(Field528, 0x528);
    MPH_MEM_I32(Field52C, 0x52c);
    MPH_MEM_I32(Field530, 0x530);
    MPH_MEM_I32(Field534, 0x534);
    MPH_MEM_I32(Field538, 0x538);
    MPH_MEM_U8(BombTimer, 0x53c);
    MPH_MEM_U8(BombAmount, 0x53d);
    MPH_MEM_U8(Field53E, 0x53e);
    MPH_MEM_U8(Field53F, 0x53f);
    MPH_MEM_U8(Field540, 0x540);
    MPH_MEM_U8(Field541, 0x541);
    MPH_MEM_U16(Field542, 0x542);
    MPH_MEM_VEC3(Field544, 0x544);
    MPH_MEM_U8(GunIdleMaybe, 0x550);
    MPH_MEM_U8(Field551, 0x551);
    MPH_MEM_U8(Field552, 0x552);
    MPH_MEM_U8(Field553, 0x553);
    MPH_MEM_I32(Field554, 0x554);
    MPH_MEM_I32(Field558, 0x558);
    MPH_MEM_VEC3(PrevCamPos, 0x678);
    MPH_MEM_I32(Field684, 0x684);
    MPH_MEM_I32(Field688, 0x688);
    MPH_MEM_I32(Field68C, 0x68c);
    MPH_MEM_I32(Field690, 0x690);
    MPH_MEM_U8(Field6B3, 0x6b3);
    MPH_MEM_PTR(Field6B4, 0x6b4);
    MPH_MEM_PTR(LastCamPos, 0x6b8);
    MPH_MEM_PTR(NextPointModule, 0x6bc);
    MPH_MEM_PTR(LastJumpPad, 0x6c0);
    MPH_MEM_PTR(OctoFlag, 0x6c4);
    MPH_MEM_PTR(EnemySpawner, 0x6c8);
    MPH_MEM_PTR(LastTarget, 0x6cc);
    MPH_MEM_I32(Field6D0, 0x6d0);
    MPH_MEM_PTR(BurnedBy, 0x6d4);
    MPH_MEM_PTR(EffectBurn, 0x6d8);
    MPH_MEM_U16(BurnTimer, 0x6dc);
    MPH_MEM_U16(TargetOrDamageRelated, 0x6de);
    MPH_MEM_U16(Field6E0, 0x6e0);
    MPH_MEM_U16(Field6E2, 0x6e2);
    MPH_MEM_I32(Field6E4, 0x6e4);
    MPH_MEM_I32(Field6E8, 0x6e8);
    MPH_MEM_I32(Field6EC, 0x6ec);
    MPH_MEM_U32(Effectiveness, 0x6f0);
    MPH_MEM_I32(Field6F4, 0x6f4);
    MPH_MEM_I32(Field6F8, 0x6f8);
    MPH_MEM_I32(Field6FC, 0x6fc);
    MPH_MEM_I32(Field700, 0x700);
    MPH_MEM_U16(AltFormAttackTime, 0x704);
    MPH_MEM_U16(Field706, 0x706);
    MPH_MEM_I32(AltField708, 0x708);
    MPH_MEM_I32(AltField70C, 0x70c);
    MPH_MEM_I32(AltField710, 0x710);
    MPH_MEM_VEC3(Field714, 0x714);
    MPH_MEM_VEC3(Field720, 0x720);
    MPH_MEM_VEC3(Field72C, 0x72c);
    MPH_MEM_I32(Field738, 0x738);
    MPH_MEM_I32(Field73C, 0x73c);
    MPH_MEM_I32(Field740, 0x740);
    MPH_MEM_I32(Field744, 0x744);
    MPH_MEM_I32(Field748, 0x748);
    MPH_MEM_I32(Field74C, 0x74c);
    MPH_MEM_I32(Field750, 0x750);
    MPH_MEM_I32(Field754, 0x754);
    MPH_MEM_I32(Field758, 0x758);
    MPH_MEM_I32(Field75C, 0x75c);
    MPH_MEM_I32(Field760, 0x760);
    MPH_MEM_I32(Field764, 0x764);
    MPH_MEM_I32(Field768, 0x768);
    MPH_MEM_I32(Field76C, 0x76c);
    MPH_MEM_I32(Field770, 0x770);
    MPH_MEM_I32(Field774, 0x774);
    MPH_MEM_I32(Field778, 0x778);
    MPH_MEM_I32(Field77C, 0x77c);
    MPH_MEM_I32(Field780, 0x780);
    MPH_MEM_I32(Field784, 0x784);
    MPH_MEM_I32(Field788, 0x788);
    MPH_MEM_I32(Field78C, 0x78c);
    MPH_MEM_I32(Field790, 0x790);
    MPH_MEM_I32(Field794, 0x794);
    MPH_MEM_I32(Field798, 0x798);
    MPH_MEM_I32(Field79C, 0x79c);
    MPH_MEM_I32(Field7A0, 0x7a0);
    MPH_MEM_I32(Field7A4, 0x7a4);
    MPH_MEM_I32(Field7A8, 0x7a8);
    MPH_MEM_I32(Field7AC, 0x7ac);
    MPH_MEM_I32(Field7B0, 0x7b0);
    MPH_MEM_I32(Field7B4, 0x7b4);
    MPH_MEM_I32(Field7B8, 0x7b8);
    MPH_MEM_I32(Field7BC, 0x7bc);
    MPH_MEM_I32(Field7C0, 0x7c0);
    MPH_MEM_I32(Field7C4, 0x7c4);
    MPH_MEM_I32(Field7C8, 0x7c8);
    MPH_MEM_I32(Field7CC, 0x7cc);
    MPH_MEM_I32(Field7D0, 0x7d0);
    MPH_MEM_VEC3(Field7D4, 0x7d4);
    MPH_MEM_VEC3(Field7E0, 0x7e0);
    MPH_MEM_I32(Field7EC, 0x7ec);
    MPH_MEM_I32(Field7F0, 0x7f0);
    MPH_MEM_I32(Field7F4, 0x7f4);
    MPH_MEM_I32(Field7F8, 0x7f8);
    MPH_MEM_PTR(Lrock01Node, 0x7fc);
    MPH_MEM_PTR(Rrock01Node, 0x800);
    MPH_MEM_PTR(RposrotNode, 0x804);
    MPH_MEM_PTR(Rposrot1Node, 0x808);
    MPH_MEM_I32(Field80C, 0x80c);
    MPH_MEM_I32(Field810, 0x810);
    MPH_MEM_I32(Field814, 0x814);
    MPH_MEM_I32(Field818, 0x818);
    MPH_MEM_I32(Field81C, 0x81c);
    MPH_MEM_I32(Field820, 0x820);
    MPH_MEM_I32(Field824, 0x824);
    MPH_MEM_VEC3(Field828, 0x828);
    MPH_MEM_I32(Field834, 0x834);
    MPH_MEM_I32(Field838, 0x838);
    MPH_MEM_I32(Field83C, 0x83c);
    MPH_MEM_I32(Field840, 0x840);
    MPH_MEM_I32(Field844, 0x844);
    MPH_MEM_PTR(Struct1, 0x848);
    MPH_MEM_U8(LoadFlags, 0x84c);
    MPH_MEM_U8(SlotIndex, 0x84d);
    MPH_MEM_U8(IsBot, 0x84e);
    MPH_MEM_U8(Field84F, 0x84f);
    MPH_MEM_I32(Field9BC, 0x9bc);
    MPH_MEM_I32(Field9C0, 0x9c0);
    MPH_MEM_I32(Field9C4, 0x9c4);
    MPH_MEM_I32(Field9C8, 0x9c8);
    MPH_MEM_I32(Field9CC, 0x9cc);
    MPH_MEM_I32(Field9D0, 0x9d0);
    MPH_MEM_I32(Field9D4, 0x9d4);
    MPH_MEM_I32(Field9D8, 0x9d8);
    MPH_MEM_I32(Field9DC, 0x9dc);
    MPH_MEM_I32(Field9E0, 0x9e0);
    MPH_MEM_I32(Field9E4, 0x9e4);
    MPH_MEM_I32(Field9E8, 0x9e8);
    MPH_MEM_I32(Field9EC, 0x9ec);
    MPH_MEM_I32(Field9F0, 0x9f0);
    MPH_MEM_I32(Field9F4, 0x9f4);
    MPH_MEM_I32(Field9F8, 0x9f8);
    MPH_MEM_I32(Field9FC, 0x9fc);
    MPH_MEM_I32(FieldA00, 0xa00);
    MPH_MEM_I32(FieldA04, 0xa04);
    MPH_MEM_I32(FieldA08, 0xa08);
    MPH_MEM_I32(FieldA0C, 0xa0c);
    MPH_MEM_I32(FieldA10, 0xa10);
    MPH_MEM_I32(FieldA14, 0xa14);
    MPH_MEM_I32(FieldA18, 0xa18);
    MPH_MEM_I32(FieldA1C, 0xa1c);
    MPH_MEM_I32(FieldA20, 0xa20);
    MPH_MEM_I32(FieldA24, 0xa24);
    MPH_MEM_I32(FieldA28, 0xa28);
    MPH_MEM_I32(FieldA2C, 0xa2c);
    MPH_MEM_I32(FieldA30, 0xa30);
    MPH_MEM_I32(FieldA34, 0xa34);
    MPH_MEM_I32(FieldA38, 0xa38);
    MPH_MEM_I32(FieldA3C, 0xa3c);
    MPH_MEM_I32(FieldA40, 0xa40);
    MPH_MEM_I32(FieldA44, 0xa44);
    MPH_MEM_I32(FieldA48, 0xa48);
    MPH_MEM_I32(FieldA4C, 0xa4c);
    MPH_MEM_I32(FieldA50, 0xa50);
    MPH_MEM_I32(FieldA54, 0xa54);
    MPH_MEM_I32(FieldA58, 0xa58);
    MPH_MEM_I32(FieldA5C, 0xa5c);
    MPH_MEM_I32(FieldA60, 0xa60);
    MPH_MEM_I32(FieldA64, 0xa64);
    MPH_MEM_I32(FieldA68, 0xa68);
    MPH_MEM_I32(FieldA6C, 0xa6c);
    MPH_MEM_I32(FieldA70, 0xa70);
    MPH_MEM_I32(FieldA74, 0xa74);
    MPH_MEM_I32(FieldA78, 0xa78);
    MPH_MEM_I32(FieldA7C, 0xa7c);
    MPH_MEM_I32(FieldA80, 0xa80);
    MPH_MEM_I32(FieldA84, 0xa84);
    MPH_MEM_I32(FieldA88, 0xa88);
    MPH_MEM_I32(FieldA8C, 0xa8c);
    MPH_MEM_I32(FieldA90, 0xa90);
    MPH_MEM_I32(FieldA94, 0xa94);
    MPH_MEM_I32(FieldA98, 0xa98);
    MPH_MEM_I32(FieldA9C, 0xa9c);
    MPH_MEM_I32(FieldAA0, 0xaa0);
    MPH_MEM_I32(FieldAA4, 0xaa4);
    MPH_MEM_I32(FieldAA8, 0xaa8);
    MPH_MEM_I32(FieldAAC, 0xaac);
    MPH_MEM_I32(FieldAB0, 0xab0);
    MPH_MEM_I32(FieldAB4, 0xab4);
    MPH_MEM_I32(FieldAB8, 0xab8);
    MPH_MEM_I32(FieldABC, 0xabc);
    MPH_MEM_I32(FieldAC0, 0xac0);
    MPH_MEM_I32(FieldAC4, 0xac4);
    MPH_MEM_I32(FieldAC8, 0xac8);
    MPH_MEM_I32(FieldACC, 0xacc);
    MPH_MEM_I32(FieldAD0, 0xad0);
    MPH_MEM_I32(FieldAD4, 0xad4);
    MPH_MEM_I32(FieldAD8, 0xad8);
    MPH_MEM_I32(FieldADC, 0xadc);
    MPH_MEM_I32(FieldAE0, 0xae0);
    MPH_MEM_I32(FieldAE4, 0xae4);
    MPH_MEM_I32(FieldAE8, 0xae8);
    MPH_MEM_I32(FieldAEC, 0xaec);
    MPH_MEM_I32(FieldAF0, 0xaf0);
    MPH_MEM_I32(FieldAF4, 0xaf4);
    MPH_MEM_I32(FieldAF8, 0xaf8);
    MPH_MEM_I32(FieldAFC, 0xafc);
    MPH_MEM_I32(FieldB00, 0xb00);
    MPH_MEM_I32(FieldB04, 0xb04);
    MPH_MEM_I32(FieldB08, 0xb08);
    MPH_MEM_I32(FieldB0C, 0xb0c);
    MPH_MEM_I32(FieldB10, 0xb10);
    MPH_MEM_I32(FieldB14, 0xb14);
    MPH_MEM_I32(FieldB18, 0xb18);
    MPH_MEM_I32(FieldB1C, 0xb1c);
    MPH_MEM_I32(FieldB20, 0xb20);
    MPH_MEM_I32(FieldB24, 0xb24);
    MPH_MEM_I32(FieldB28, 0xb28);
    MPH_MEM_I32(FieldB2C, 0xb2c);
    MPH_MEM_I32(FieldB30, 0xb30);
    MPH_MEM_I32(FieldB34, 0xb34);
    MPH_MEM_I32(FieldB38, 0xb38);
    MPH_MEM_I32(FieldB3C, 0xb3c);
    MPH_MEM_I32(FieldB40, 0xb40);
    MPH_MEM_I32(FieldB44, 0xb44);
    MPH_MEM_I32(FieldB48, 0xb48);
    MPH_MEM_I32(FieldB4C, 0xb4c);
    MPH_MEM_I32(FieldB50, 0xb50);
    MPH_MEM_I32(FieldB54, 0xb54);
    MPH_MEM_I32(FieldB58, 0xb58);
    MPH_MEM_I32(FieldB5C, 0xb5c);
    MPH_MEM_I32(FieldB60, 0xb60);
    MPH_MEM_I32(FieldB64, 0xb64);
    MPH_MEM_I32(FieldB68, 0xb68);
    MPH_MEM_I32(FieldB6C, 0xb6c);
    MPH_MEM_I32(FieldB70, 0xb70);
    MPH_MEM_I32(FieldB74, 0xb74);
    MPH_MEM_I32(FieldB78, 0xb78);
    MPH_MEM_I32(FieldB7C, 0xb7c);
    MPH_MEM_I32(FieldB80, 0xb80);
    MPH_MEM_I32(FieldB84, 0xb84);
    MPH_MEM_I32(FieldB88, 0xb88);
    MPH_MEM_I32(FieldB8C, 0xb8c);
    MPH_MEM_I32(FieldB90, 0xb90);
    MPH_MEM_I32(FieldB94, 0xb94);
    MPH_MEM_I32(FieldB98, 0xb98);
    MPH_MEM_I32(FieldB9C, 0xb9c);
    MPH_MEM_I32(FieldBA0, 0xba0);
    MPH_MEM_I32(FieldBA4, 0xba4);
    MPH_MEM_I32(FieldBA8, 0xba8);
    MPH_MEM_I32(FieldBAC, 0xbac);
    MPH_MEM_I32(FieldBB0, 0xbb0);
    MPH_MEM_I32(FieldBB4, 0xbb4);
    MPH_MEM_I32(FieldBB8, 0xbb8);
    MPH_MEM_I32(FieldBBC, 0xbbc);
    MPH_MEM_I32(FieldBC0, 0xbc0);
    MPH_MEM_I32(FieldBC4, 0xbc4);
    MPH_MEM_I32(FieldBC8, 0xbc8);
    MPH_MEM_I32(FieldBCC, 0xbcc);
    MPH_MEM_I32(FieldBD0, 0xbd0);
    MPH_MEM_I32(FieldBD4, 0xbd4);
    MPH_MEM_I32(FieldBD8, 0xbd8);
    MPH_MEM_I32(FieldBDC, 0xbdc);
    MPH_MEM_I32(FieldBE0, 0xbe0);
    MPH_MEM_I32(FieldBE4, 0xbe4);
    MPH_MEM_I32(FieldBE8, 0xbe8);
    MPH_MEM_I32(FieldBEC, 0xbec);
    MPH_MEM_I32(FieldBF0, 0xbf0);
    MPH_MEM_I32(FieldBF4, 0xbf4);
    MPH_MEM_I32(FieldBF8, 0xbf8);
    MPH_MEM_I32(FieldBFC, 0xbfc);
    MPH_MEM_I32(FieldC00, 0xc00);
    MPH_MEM_I32(FieldC04, 0xc04);
    MPH_MEM_I32(FieldC08, 0xc08);
    MPH_MEM_I32(FieldC0C, 0xc0c);
    MPH_MEM_I32(FieldC10, 0xc10);
    MPH_MEM_I32(FieldC14, 0xc14);
    MPH_MEM_I32(FieldC18, 0xc18);
    MPH_MEM_I32(FieldC1C, 0xc1c);
    MPH_MEM_I32(FieldC20, 0xc20);
    MPH_MEM_I32(FieldC24, 0xc24);
    MPH_MEM_I32(FieldC28, 0xc28);
    MPH_MEM_I32(FieldC2C, 0xc2c);
    MPH_MEM_I32(FieldC30, 0xc30);
    MPH_MEM_I32(FieldC34, 0xc34);
    MPH_MEM_I32(FieldC38, 0xc38);
    MPH_MEM_I32(FieldC3C, 0xc3c);
    MPH_MEM_I32(FieldC40, 0xc40);
    MPH_MEM_I32(FieldC44, 0xc44);
    MPH_MEM_I32(FieldC48, 0xc48);
    MPH_MEM_I32(FieldC4C, 0xc4c);
    MPH_MEM_I32(FieldC50, 0xc50);
    MPH_MEM_I32(FieldC54, 0xc54);
    MPH_MEM_I32(FieldC58, 0xc58);
    MPH_MEM_I32(FieldC5C, 0xc5c);
    MPH_MEM_I32(FieldC60, 0xc60);
    MPH_MEM_I32(FieldC64, 0xc64);
    MPH_MEM_I32(FieldC68, 0xc68);
    MPH_MEM_I32(FieldC6C, 0xc6c);
    MPH_MEM_I32(FieldC70, 0xc70);
    MPH_MEM_I32(FieldC74, 0xc74);
    MPH_MEM_I32(FieldC78, 0xc78);
    MPH_MEM_I32(FieldC7C, 0xc7c);
    MPH_MEM_I32(FieldC80, 0xc80);
    MPH_MEM_I32(FieldC84, 0xc84);
    MPH_MEM_I32(FieldC88, 0xc88);
    MPH_MEM_I32(FieldC8C, 0xc8c);
    MPH_MEM_I32(FieldC90, 0xc90);
    MPH_MEM_I32(FieldC94, 0xc94);
    MPH_MEM_I32(FieldC98, 0xc98);
    MPH_MEM_I32(FieldC9C, 0xc9c);
    MPH_MEM_I32(FieldCA0, 0xca0);
    MPH_MEM_I32(FieldCA4, 0xca4);
    MPH_MEM_I32(FieldCA8, 0xca8);
    MPH_MEM_I32(FieldCAC, 0xcac);
    MPH_MEM_I32(FieldCB0, 0xcb0);
    MPH_MEM_I32(FieldCB4, 0xcb4);
    MPH_MEM_I32(FieldCB8, 0xcb8);
    MPH_MEM_I32(FieldCBC, 0xcbc);
    MPH_MEM_I32(FieldCC0, 0xcc0);
    MPH_MEM_I32(FieldCC4, 0xcc4);
    MPH_MEM_I32(FieldCC8, 0xcc8);
    MPH_MEM_I32(FieldCCC, 0xccc);
    MPH_MEM_I32(FieldCD0, 0xcd0);
    MPH_MEM_I32(FieldCD4, 0xcd4);
    MPH_MEM_I32(FieldCD8, 0xcd8);
    MPH_MEM_I32(FieldCDC, 0xcdc);
    MPH_MEM_I32(FieldCE0, 0xce0);
    MPH_MEM_I32(FieldCE4, 0xce4);
    MPH_MEM_I32(FieldCE8, 0xce8);
    MPH_MEM_I32(FieldCEC, 0xcec);
    MPH_MEM_I32(FieldCF0, 0xcf0);
    MPH_MEM_I32(FieldCF4, 0xcf4);
    MPH_MEM_I32(FieldCF8, 0xcf8);
    MPH_MEM_I32(FieldCFC, 0xcfc);
    MPH_MEM_I32(FieldD00, 0xd00);
    MPH_MEM_I32(FieldD04, 0xd04);
    MPH_MEM_I32(FieldD08, 0xd08);
    MPH_MEM_I32(FieldD0C, 0xd0c);
    MPH_MEM_I32(FieldD10, 0xd10);
    MPH_MEM_I32(FieldD14, 0xd14);
    MPH_MEM_I32(FieldD18, 0xd18);
    MPH_MEM_I32(FieldD1C, 0xd1c);
    MPH_MEM_I32(FieldD20, 0xd20);
    MPH_MEM_I32(FieldD24, 0xd24);
    MPH_MEM_I32(FieldD28, 0xd28);
    MPH_MEM_I32(FieldD2C, 0xd2c);
    MPH_MEM_I32(FieldD30, 0xd30);
    MPH_MEM_I32(FieldD34, 0xd34);
    MPH_MEM_I32(FieldD38, 0xd38);
    MPH_MEM_I32(FieldD3C, 0xd3c);
    MPH_MEM_I32(FieldD40, 0xd40);
    MPH_MEM_I32(FieldD44, 0xd44);
    MPH_MEM_I32(FieldD48, 0xd48);
    MPH_MEM_I32(FieldD4C, 0xd4c);
    MPH_MEM_I32(FieldD50, 0xd50);
    MPH_MEM_I32(FieldD54, 0xd54);
    MPH_MEM_I32(FieldD58, 0xd58);
    MPH_MEM_I32(FieldD5C, 0xd5c);
    MPH_MEM_I32(FieldD60, 0xd60);
    MPH_MEM_I32(FieldD64, 0xd64);
    MPH_MEM_I32(FieldD68, 0xd68);
    MPH_MEM_I32(FieldD6C, 0xd6c);
    MPH_MEM_I32(FieldD70, 0xd70);
    MPH_MEM_I32(FieldD74, 0xd74);
    MPH_MEM_I32(FieldD78, 0xd78);
    MPH_MEM_I32(FieldD7C, 0xd7c);
    MPH_MEM_I32(FieldD80, 0xd80);
    MPH_MEM_I32(FieldD84, 0xd84);
    MPH_MEM_I32(FieldD88, 0xd88);
    MPH_MEM_I32(FieldD8C, 0xd8c);
    MPH_MEM_I32(FieldD90, 0xd90);
    MPH_MEM_I32(FieldD94, 0xd94);
    MPH_MEM_I32(FieldD98, 0xd98);
    MPH_MEM_I32(FieldD9C, 0xd9c);
    MPH_MEM_I32(FieldDA0, 0xda0);
    MPH_MEM_I32(FieldDA4, 0xda4);
    MPH_MEM_I32(FieldDA8, 0xda8);
    MPH_MEM_I32(FieldDAC, 0xdac);
    MPH_MEM_I32(FieldDB0, 0xdb0);
    MPH_MEM_I32(FieldDB4, 0xdb4);
    MPH_MEM_I32(FieldDB8, 0xdb8);
    MPH_MEM_I32(FieldDBC, 0xdbc);
    MPH_MEM_I32(FieldDC0, 0xdc0);
    MPH_MEM_I32(FieldDC4, 0xdc4);
    MPH_MEM_I32(FieldDC8, 0xdc8);
    MPH_MEM_I32(FieldDCC, 0xdcc);
    MPH_MEM_I32(FieldDD0, 0xdd0);
    MPH_MEM_I32(FieldDD4, 0xdd4);
    MPH_MEM_I32(FieldDD8, 0xdd8);
    MPH_MEM_I32(FieldDDC, 0xddc);
    MPH_MEM_I32(FieldDE0, 0xde0);
    MPH_MEM_I32(FieldDE4, 0xde4);
    MPH_MEM_I32(FieldDE8, 0xde8);
    MPH_MEM_I32(FieldDEC, 0xdec);
    MPH_MEM_I32(FieldDF0, 0xdf0);
    MPH_MEM_I32(FieldDF4, 0xdf4);
    MPH_MEM_I32(FieldDF8, 0xdf8);
    MPH_MEM_I32(FieldDFC, 0xdfc);
    MPH_MEM_I32(FieldE00, 0xe00);
    MPH_MEM_I32(FieldE04, 0xe04);
    MPH_MEM_I32(FieldE08, 0xe08);
    MPH_MEM_I32(FieldE0C, 0xe0c);
    MPH_MEM_I32(FieldE10, 0xe10);
    MPH_MEM_I32(FieldE14, 0xe14);
    MPH_MEM_I32(FieldE18, 0xe18);
    MPH_MEM_I32(FieldE1C, 0xe1c);
    MPH_MEM_I32(FieldE20, 0xe20);
    MPH_MEM_I32(FieldE24, 0xe24);
    MPH_MEM_I32(FieldE28, 0xe28);
    MPH_MEM_I32(FieldE2C, 0xe2c);
    MPH_MEM_I32(FieldE30, 0xe30);
    MPH_MEM_I32(FieldE34, 0xe34);
    MPH_MEM_I32(FieldE38, 0xe38);
    MPH_MEM_I32(FieldE3C, 0xe3c);
    MPH_MEM_I32(FieldE40, 0xe40);
    MPH_MEM_I32(FieldE44, 0xe44);
    MPH_MEM_I32(FieldE48, 0xe48);
    MPH_MEM_I32(FieldE4C, 0xe4c);
    MPH_MEM_I32(FieldE50, 0xe50);
    MPH_MEM_I32(FieldE54, 0xe54);
    MPH_MEM_I32(FieldE58, 0xe58);
    MPH_MEM_I32(FieldE5C, 0xe5c);
    MPH_MEM_I32(FieldE60, 0xe60);
    MPH_MEM_I32(FieldE64, 0xe64);
    MPH_MEM_I32(FieldE68, 0xe68);
    MPH_MEM_I32(FieldE6C, 0xe6c);
    MPH_MEM_I32(FieldE70, 0xe70);
    MPH_MEM_I32(FieldE74, 0xe74);
    MPH_MEM_I32(FieldE78, 0xe78);
    MPH_MEM_I32(FieldE7C, 0xe7c);
    MPH_MEM_I32(FieldE80, 0xe80);
    MPH_MEM_I32(FieldE84, 0xe84);
    MPH_MEM_I32(FieldE88, 0xe88);
    MPH_MEM_I32(FieldE8C, 0xe8c);
    MPH_MEM_I32(FieldE90, 0xe90);
    MPH_MEM_I32(FieldE94, 0xe94);
    MPH_MEM_I32(FieldE98, 0xe98);
    MPH_MEM_I32(FieldE9C, 0xe9c);
    MPH_MEM_I32(FieldEA0, 0xea0);
    MPH_MEM_I32(FieldEA4, 0xea4);
    MPH_MEM_I32(FieldEA8, 0xea8);
    MPH_MEM_I32(FieldEAC, 0xeac);
    MPH_MEM_I32(FieldEB0, 0xeb0);
    MPH_MEM_I32(FieldEB4, 0xeb4);
    MPH_MEM_I32(FieldEB8, 0xeb8);
    MPH_MEM_I32(FieldEBC, 0xebc);
    MPH_MEM_I32(FieldEC0, 0xec0);
    MPH_MEM_I32(FieldEC4, 0xec4);
    MPH_MEM_I32(FieldEC8, 0xec8);
    MPH_MEM_I32(FieldECC, 0xecc);
    MPH_MEM_I32(FieldED0, 0xed0);
    MPH_MEM_I32(FieldED4, 0xed4);
    MPH_MEM_I32(FieldED8, 0xed8);
    MPH_MEM_I32(FieldEDC, 0xedc);
    MPH_MEM_I32(FieldEE0, 0xee0);
    MPH_MEM_I32(FieldEE4, 0xee4);
    MPH_MEM_I32(FieldEE8, 0xee8);
    MPH_MEM_I32(FieldEEC, 0xeec);
    MPH_MEM_I32(FieldEF0, 0xef0);
    MPH_MEM_I32(FieldEF4, 0xef4);
    MPH_MEM_I32(FieldEF8, 0xef8);
    MPH_MEM_I32(FieldEFC, 0xefc);
    MPH_MEM_I32(FieldF00, 0xf00);
    MPH_MEM_I32(FieldF04, 0xf04);
    MPH_MEM_I32(FieldF08, 0xf08);
    MPH_MEM_I32(FieldF0C, 0xf0c);
    MPH_MEM_I32(FieldF10, 0xf10);
    MPH_MEM_I32(FieldF14, 0xf14);
    MPH_MEM_I32(FieldF18, 0xf18);
    MPH_MEM_PTR(AiDataPtr, 0xf1c);
    MPH_MEM_I32(FieldF20, 0xf20);
    MPH_MEM_PTR(Halfturret, 0xf24);
    MPH_MEM_I32(WeaponSfxHandle, 0xf2c);
    MPH_MEM_CHILD(UInt16Array, Field100);
    MPH_MEM_CHILD(CollisionVolume, Collision);
    MPH_MEM_CHILD(CModel, GunModel);
    MPH_MEM_CHILD(CModel, FrozenModel);
    MPH_MEM_CHILD(IntPtrArray, SpineNode);
    MPH_MEM_CHILD(IntPtrArray, ShootNode);
    MPH_MEM_CHILD(CModel, Biped1);
    MPH_MEM_CHILD(CModel, Biped2);
    MPH_MEM_CHILD(CModel, AltForm);
    MPH_MEM_CHILD(CModel, GunSmoke);
    MPH_MEM_CHILD(PlayerControls, Controls);
    MPH_MEM_CHILD(PlayerInput, Input);
    MPH_MEM_CHILD(CameraInfo, CameraInfo);
    MPH_MEM_CHILD(LightInfo, LightInfo);
    MPH_MEM_CHILD(EquipInfoPtr, EquipInfo);
    MPH_MEM_CHILD(CBeamProjectile, BeamHead);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
    MPH_MEM_CHILD(AiData, AiData);
    MPH_MEM_CHILD(StructArray<AIContext>, AIContext);
    MPH_MEM_CHILD(StructArray<AIAggro>, AIAggro);
    [[nodiscard]] std::uint32_t AggroCount() const;
    void AggroCount(std::uint32_t value);
};

class AiData : public MemoryClass
{
public:
    AiData(Memory& memory, std::int32_t address);
    AiData(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(Player, 0x0);
    MPH_MEM_PTR(State, 0x4);
    MPH_MEM_PTR(Shift258, 0x8);
    MPH_MEM_PTR(Shift1020, 0xC);
    MPH_MEM_PTR(Shift2FC, 0x10);
    MPH_MEM_PTR(TargetPlayer, 0x14);
    MPH_MEM_PTR(Players, 0x18);
    MPH_MEM_PTR(TargetHalfturret, 0x1C);
    MPH_MEM_U16(CurNodedataCount, 0x20);
    MPH_MEM_U16(NodedataSetIdx, 0x22);
    MPH_MEM_U16(Field30, 0x30);
    MPH_MEM_U16(Padding32, 0x32);
    MPH_MEM_PTR(CurNodedata, 0x34);
    MPH_MEM_PTR(Nodedata, 0x38);
    MPH_MEM_PTR(Node3C, 0x3C);
    MPH_MEM_PTR(Node40, 0x40);
    MPH_MEM_PTR(Node44, 0x44);
    MPH_MEM_PTR(Node48, 0x48);
    MPH_MEM_U16(Field78, 0x78);
    MPH_MEM_U16(Padding8E, 0x8E);
    MPH_MEM_VEC3(Field90, 0x90);
    MPH_MEM_I32(Field9C, 0x9C);
    MPH_MEM_VEC3(FieldA0, 0xA0);
    MPH_MEM_VEC3(FieldAC, 0xAC);
    MPH_MEM_VEC3(FieldB8, 0xB8);
    MPH_MEM_PTR(ItemSpawnC4, 0xC4);
    MPH_MEM_PTR(ItemC8, 0xC8);
    MPH_MEM_PTR(OctoFlagCC, 0xCC);
    MPH_MEM_PTR(FlagBaseD0, 0xD0);
    MPH_MEM_PTR(OctoFlagD4, 0xD4);
    MPH_MEM_PTR(FlagBaseD8, 0xD8);
    MPH_MEM_PTR(OctoFlagDC, 0xDC);
    MPH_MEM_PTR(FlagBaseE0, 0xE0);
    MPH_MEM_PTR(TargetDefense, 0xE4);
    MPH_MEM_PTR(TargetDoor, 0xE8);
    MPH_MEM_ENUM32(AiFlags2, Flags2, 0xEC);
    MPH_MEM_I32(HalfturretDmg, 0x110);
    MPH_MEM_U8(SlotIndex, 0x114);
    MPH_MEM_U8(QueuedFindEntAction, 0x115);
    MPH_MEM_U16(Field116, 0x116);
    MPH_MEM_I32(Field118, 0x118);
    MPH_MEM_I32(Unused11C, 0x11C);
    MPH_MEM_U16(TouchAimX, 0x2A0);
    MPH_MEM_U16(TouchAimY, 0x2A2);
    MPH_MEM_U16(TouchInputFlag, 0x2A4);
    MPH_MEM_U16(FramesWithTouch, 0x2A6);
    MPH_MEM_U16(FramesWithoutTouch, 0x2A8);
    MPH_MEM_I32(BtnAimX, 0x2EC);
    MPH_MEM_I32(BtnAimY, 0x2F0);
    MPH_MEM_U8(NodedataSelOff, 0x2F4);
    MPH_MEM_U8(NodedataSelOn, 0x2F5);
    MPH_MEM_U16(Padding2F6, 0x2F6);
    MPH_MEM_ENUM32(AiFlags3, Flags3, 0x2F8);
    MPH_MEM_PTR(Personality, 0x101C);
    MPH_MEM_I32(Field1020, 0x1020);
    MPH_MEM_U16(Weapon1, 0x1024);
    MPH_MEM_U16(Weapon2, 0x1026);
    MPH_MEM_U16(FindWeaponIndex, 0x1028);
    MPH_MEM_U16(ShotDelay, 0x102A);
    MPH_MEM_U16(Field102C, 0x102C);
    MPH_MEM_U16(Field102E, 0x102E);
    MPH_MEM_U16(Field1030, 0x1030);
    MPH_MEM_U16(Field1032, 0x1032);
    MPH_MEM_U16(Field1034, 0x1034);
    MPH_MEM_U16(Padding1036, 0x1036);
    MPH_MEM_VEC3(Field1038, 0x1038);
    MPH_MEM_ENUM8(AiFlags4, Flags4, 0x1044);
    MPH_MEM_U8(Padding1045, 0x1045);
    MPH_MEM_U16(Padding1046, 0x1046);
    MPH_MEM_VEC3(Field1048, 0x1048);
    MPH_MEM_VEC3(Field1054, 0x1054);
    MPH_MEM_U32(AggroCount, 0x1060);
    MPH_MEM_U16(HealthThreshold, 0x11F8);
    MPH_MEM_U16(Padding11FA, 0x11FA);
    MPH_MEM_CHILD(UInt16Array, CurNodeTypeIndex);
    MPH_MEM_CHILD(IntPtrArray, Field4C);
    MPH_MEM_CHILD(UInt16Array, Field7A);
    MPH_MEM_CHILD(Int32Array, SlotsHitTotal);
    MPH_MEM_CHILD(Int32Array, SlotsDamageTotal);
    MPH_MEM_CHILD(IntPtrArray, EntList);
    MPH_MEM_CHILD(StructArray<AiButton>, Buttons);
    MPH_MEM_CHILD(StructArray<AiButton>, TouchBtns);
    MPH_MEM_CHILD(Int32Array, FuncTree);
    MPH_MEM_CHILD(Int32Array, Aggro);
    [[nodiscard]] bool Flags1() const { return ReadInt32(0x11F4) != 0; }
    void Flags1(bool value) { WriteInt32(0x11F4, value ? 1 : 0); }
};

class AIContext : public MemoryClass
{
public:
    AIContext(Memory& memory, std::int32_t address);
    AIContext(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Func24Id, 0x0);
    MPH_MEM_U8(Field4, 0x4);
    MPH_MEM_U8(Field5, 0x5);
    MPH_MEM_U8(Field6, 0x6);
    MPH_MEM_U8(Field7, 0x7);
    MPH_MEM_U8(Field8, 0x8);
    MPH_MEM_U8(Field9, 0x9);
    MPH_MEM_U8(FieldA, 0xa);
    MPH_MEM_U8(FieldB, 0xb);
    MPH_MEM_U8(FieldC, 0xc);
    MPH_MEM_U8(FieldD, 0xd);
    MPH_MEM_U8(FieldE, 0xe);
    MPH_MEM_U8(FieldF, 0xf);
    MPH_MEM_U8(Field10, 0x10);
    MPH_MEM_U8(Padding11, 0x11);
    MPH_MEM_U16(Padding12, 0x12);
    MPH_MEM_I32(Field14, 0x14);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1c);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2c);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_VEC3(Field34, 0x34);
    MPH_MEM_I32(Field40, 0x40);
    MPH_MEM_I32(Field44, 0x44);
    MPH_MEM_PTR(CurData1Iter, 0x48);
    MPH_MEM_I32(CallCount, 0x4C);
    MPH_MEM_U8(Depth, 0x50);
    MPH_MEM_U8(Padding51, 0x51);
    MPH_MEM_U16(Padding52, 0x52);
    MPH_MEM_CHILD(Int32Array, Weights);
private:
    std::intptr_t _lastData1Ptr = 0;
    std::shared_ptr<::MphRead::Memory::AIData1> _AIData1;
public:
    [[nodiscard]] std::shared_ptr<::MphRead::Memory::AIData1> AIData1();
};

class AIData1 : public MemoryClass
{
public:
    AIData1(Memory& memory, std::int32_t address);
    AIData1(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Func24Id, 0x0);
    MPH_MEM_I32(Data1Count, 0x4);
    MPH_MEM_PTR(Data1Ptr, 0x8);
    MPH_MEM_I32(Data2Count, 0xC);
    MPH_MEM_PTR(Data2Ptr, 0x10);
    MPH_MEM_I32(Data3Count, 0x14);
    MPH_MEM_PTR(Data3a, 0x18);
    MPH_MEM_I32(Data3bCount, 0x1C);
    MPH_MEM_PTR(Data3b, 0x20);
    MPH_MEM_CHILD(StructArray<AIData1>, Data1);
    MPH_MEM_CHILD(StructArray<AIData2>, Data2);
};

class AIData2 : public MemoryClass
{
public:
    AIData2(Memory& memory, std::int32_t address);
    AIData2(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(FuncIdx, 0x0);
    MPH_MEM_I32(Data4Count, 0x4);
    MPH_MEM_PTR(Data4, 0x8);
    MPH_MEM_I32(Data1SelectIdx, 0xC);
    MPH_MEM_I32(Weight, 0x10);
    MPH_MEM_PTR(Data5, 0x14);
};

class AIAggro : public MemoryClass
{
public:
    AIAggro(Memory& memory, std::int32_t address);
    AIAggro(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Bits1, 0x0);
    MPH_MEM_U16(Bits2, 0x2);
    MPH_MEM_U16(Staleness, 0x4);
    MPH_MEM_U16(Expiration, 0x6);
    MPH_MEM_PTR(Player1, 0x8);
    MPH_MEM_PTR(Player2, 0xC);
    [[nodiscard]] std::int32_t Slot1() const noexcept { return _slot1; }
    void Slot1(std::int32_t value) noexcept { _slot1 = value; }
    [[nodiscard]] std::int32_t Slot2() const noexcept { return _slot2; }
    void Slot2(std::int32_t value) noexcept { _slot2 = value; }
    void UpdateSlots(const std::vector<std::shared_ptr<CPlayer>>& players);
    [[nodiscard]] std::uint8_t VarA2() const;
    [[nodiscard]] std::uint8_t VarA9() const;
    [[nodiscard]] std::uint8_t VarA3() const;
    [[nodiscard]] std::uint8_t VarA4() const;
    [[nodiscard]] std::uint8_t VarA10() const;
    [[nodiscard]] std::uint16_t VarA7() const;
    private:
        std::int32_t _slot1 = 0;
        std::int32_t _slot2 = 0;
    public:
};

class CBeamProjectile : public CEntity
{
public:
    CBeamProjectile(Memory& memory, std::int32_t address);
    CBeamProjectile(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM8(BeamType, Beam, 0x18);
    MPH_MEM_ENUM8(BeamType, BeamKind, 0x19);
    MPH_MEM_U8(DrawFuncId, 0x1A);
    MPH_MEM_U8(ColEffect, 0x1B);
    MPH_MEM_U8(SplashDmgType, 0x1C);
    MPH_MEM_U8(DmgDirType, 0x1D);
    MPH_MEM_U8(Field1E, 0x1E);
    MPH_MEM_U8(SpeedInterpolation, 0x1F);
    MPH_MEM_U8(Afflictions, 0x20);
    MPH_MEM_U8(ListCount, 0x21);
    MPH_MEM_ENUM16(BeamFlags, Flags, 0x22);
    MPH_MEM_U16(Color, 0x24);
    MPH_MEM_U16(Damage, 0x26);
    MPH_MEM_U16(HeadshotDamage, 0x28);
    MPH_MEM_U16(SplashDamage, 0x2A);
    MPH_MEM_U16(Lifespan, 0x2C);
    MPH_MEM_U16(Age, 0x2E);
    MPH_MEM_U16(SpeedDecayTime, 0x30);
    MPH_MEM_U16(Field32, 0x32);
    MPH_MEM_VEC3(PastPos0, 0x34);
    MPH_MEM_VEC3(PastPos1, 0x40);
    MPH_MEM_VEC3(PastPos2, 0x4c);
    MPH_MEM_VEC3(PastPos3, 0x58);
    MPH_MEM_VEC3(PastPos4, 0x64);
    MPH_MEM_VEC3(Vec1, 0x70);
    MPH_MEM_VEC3(Field7C, 0x7c);
    MPH_MEM_VEC3(Vec2, 0x88);
    MPH_MEM_VEC3(CylBack, 0x94);
    MPH_MEM_VEC3(CylFront, 0xa0);
    MPH_MEM_VEC3(SpawnPos, 0xac);
    MPH_MEM_I32(Speed, 0xB8);
    MPH_MEM_I32(InitialSpeed, 0xBC);
    MPH_MEM_I32(FinalSpeed, 0xC0);
    MPH_MEM_VEC3(Velocity, 0xC4);
    MPH_MEM_VEC3(Acceleration, 0xD0);
    MPH_MEM_I32(Homing, 0xDC);
    MPH_MEM_I32(FieldE0, 0xE0);
    MPH_MEM_I32(MaxDist, 0xE4);
    MPH_MEM_I32(FieldE8, 0xE8);
    MPH_MEM_I32(FieldEC, 0xEC);
    MPH_MEM_I32(FieldF0, 0xF0);
    MPH_MEM_I32(Scale, 0xF4);
    MPH_MEM_PTR(Owner, 0xF8);
    MPH_MEM_PTR(RicochetWeapon, 0xFC);
    MPH_MEM_PTR(ListHead, 0x100);
    MPH_MEM_PTR(Target, 0x104);
    MPH_MEM_PTR(NodeRef, 0x150);
    MPH_MEM_CHILD(CModel, Model);
    MPH_MEM_CHILD(SfxParameters, SfxParameters);
};

class CModel : public MemoryClass
{
public:
    CModel(Memory& memory, std::int32_t address);
    CModel(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(Union, 0x0);
    MPH_MEM_PTR(MaterialAnimation, 0x4);
    MPH_MEM_PTR(TextureAnimation, 0x8);
    MPH_MEM_PTR(TexcoordAnimation, 0xC);
    MPH_MEM_PTR(Animation, 0x24);
    MPH_MEM_U16(Animid, 0x28);
    MPH_MEM_U16(Field2A, 0x2A);
    MPH_MEM_U16(RoomAnimId, 0x2C);
    MPH_MEM_U16(Field2E, 0x2E);
    MPH_MEM_U16(AnimFrame, 0x30);
    MPH_MEM_U16(Field32, 0x32);
    MPH_MEM_U16(InitialFrame, 0x34);
    MPH_MEM_U16(Field36, 0x36);
    MPH_MEM_U16(AnimationFlags, 0x38);
    MPH_MEM_U16(Field3A, 0x3A);
    MPH_MEM_U16(Field3C, 0x3C);
    MPH_MEM_U16(Field3E, 0x3E);
    MPH_MEM_U16(NodeAnimIgnoreRoot, 0x40);
    MPH_MEM_U8(NodeAnimDelta, 0x42);
    MPH_MEM_U8(MatAnimDelta, 0x43);
    MPH_MEM_U8(TexAnimDelta, 0x44);
    MPH_MEM_U8(UvAnimDelta, 0x45);
    MPH_MEM_U16(Field46, 0x46);
    MPH_MEM_CHILD(CNodeAnimation, NodeAnimation);
};

class CNodeAnimation : public MemoryClass
{
public:
    CNodeAnimation(Memory& memory, std::int32_t address);
    CNodeAnimation(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(NodeAnimation, 0x0);
    MPH_MEM_I32(NodeAnimFrame, 0x4);
    MPH_MEM_I32(NumNodes, 0x8);
    MPH_MEM_PTR(Nodes, 0xC);
    MPH_MEM_PTR(InitialMtx, 0x10);
};

class EntityCollision : public MemoryClass
{
public:
    EntityCollision(Memory& memory, std::int32_t address);
    EntityCollision(Memory& memory, std::intptr_t address);
    MPH_MEM_MTX(Matrix, 0x0);
    MPH_MEM_MTX(Inverse1, 0x30);
    MPH_MEM_MTX(Inverse2, 0x60);
    MPH_MEM_VEC3(Average, 0x90);
    MPH_MEM_VEC3(Vec2, 0x9C);
    MPH_MEM_I32(MaxDist, 0xA8);
    MPH_MEM_PTR(EntPtr, 0xAC);
    MPH_MEM_PTR(Collision, 0xB0);
};

class EquipInfoPtr : public MemoryClass
{
public:
    EquipInfoPtr(Memory& memory, std::int32_t address);
    EquipInfoPtr(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM8(EquipFlags, Flags, 0x0);
    MPH_MEM_U8(Count, 0x1);
    MPH_MEM_U16(Padding2, 0x2);
    MPH_MEM_PTR(Beams, 0x4);
    MPH_MEM_PTR(WeaponInfo, 0x8);
    MPH_MEM_PTR(AmmoPtr, 0xC);
    MPH_MEM_U16(ChargeLevel, 0x10);
    MPH_MEM_U16(SmokeLevel, 0x12);
};

class EquipInfo : public MemoryClass
{
public:
    EquipInfo(Memory& memory, std::int32_t address);
    EquipInfo(Memory& memory, std::intptr_t address);
    MPH_MEM_U8(Flags, 0x0);
    MPH_MEM_U8(Count, 0x1);
    MPH_MEM_U16(Padding2, 0x2);
    MPH_MEM_PTR(Beams, 0x4);
    MPH_MEM_PTR(WeaponInfo, 0x8);
    MPH_MEM_PTR(AmmoPtr, 0xC);
    MPH_MEM_U16(ChargeLevel, 0x10);
    MPH_MEM_U16(SmokeLevel, 0x12);
};

class SfxParameters : public MemoryClass
{
public:
    SfxParameters(Memory& memory, std::int32_t address);
    SfxParameters(Memory& memory, std::intptr_t address);
    MPH_MEM_S8(Volume, 0x0);
    MPH_MEM_U8(PanX, 0x1);
    MPH_MEM_U16(PanZ, 0x2);
};

class CollisionVolume : public MemoryClass
{
public:
    CollisionVolume(Memory& memory, std::int32_t address);
    CollisionVolume(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM32(VolumeType, Type, 0x0);
    MPH_MEM_VEC3(BoxVec1, 0x4);
    MPH_MEM_VEC3(BoxVec2, 0x10);
    MPH_MEM_VEC3(BoxVec3, 0x1C);
    MPH_MEM_VEC3(BoxPos, 0x28);
    MPH_MEM_VEC3(BoxDot, 0x34);
    MPH_MEM_VEC3(CylinderVec, 0x4);
    MPH_MEM_VEC3(CylinderCenter, 0x10);
    MPH_MEM_I32(CylinderRadius, 0x1C);
    MPH_MEM_I32(CylinderDot, 0x20);
    MPH_MEM_VEC3(SphereCenter, 0x4);
    MPH_MEM_I32(SphereRadius, 0x10);
};

class Light : public MemoryClass
{
public:
    Light(Memory& memory, std::int32_t address);
    Light(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Dir, 0x0);
    MPH_MEM_COLOR(Color, 0xC);
};

class LightInfo : public MemoryClass
{
public:
    LightInfo(Memory& memory, std::int32_t address);
    LightInfo(Memory& memory, std::intptr_t address);
    MPH_MEM_U8(PaddingF, 0xF);
    MPH_MEM_CHILD(Light, Light1);
    MPH_MEM_CHILD(Light, Light2);
};

class CameraInfo : public MemoryClass
{
public:
    CameraInfo(Memory& memory, std::int32_t address);
    CameraInfo(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Pos, 0x0);
    MPH_MEM_VEC3(Vec2, 0xC);
    MPH_MEM_VEC3(Vec3, 0x18);
    MPH_MEM_VEC3(Vec4, 0x24);
    MPH_MEM_VEC3(Vec5, 0x30);
    MPH_MEM_VEC3(Vec6, 0x3C);
    MPH_MEM_I32(Field48, 0x48);
    MPH_MEM_I32(Field4C, 0x4c);
    MPH_MEM_I32(Field50, 0x50);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_MTX(Mtx1, 0x58);
    MPH_MEM_I32(Field88, 0x88);
    MPH_MEM_I32(Field8C, 0x8c);
    MPH_MEM_I32(Field90, 0x90);
    MPH_MEM_I32(Field94, 0x94);
    MPH_MEM_I32(Field98, 0x98);
    MPH_MEM_I32(Field9C, 0x9c);
    MPH_MEM_I32(FieldA0, 0xa0);
    MPH_MEM_I32(FieldA4, 0xa4);
    MPH_MEM_I32(FieldA8, 0xa8);
    MPH_MEM_I32(FieldAC, 0xac);
    MPH_MEM_I32(FieldB0, 0xb0);
    MPH_MEM_I32(FieldB4, 0xb4);
    MPH_MEM_MTX(Mtx2, 0xB8);
    MPH_MEM_I32(Shake, 0xE8);
    MPH_MEM_I32(Fov, 0xEC);
    MPH_MEM_I32(FieldF0, 0xF0);
    MPH_MEM_I32(FieldF4, 0xF4);
    MPH_MEM_I32(NearLr, 0xF8);
    MPH_MEM_I32(NearTb, 0xFC);
    MPH_MEM_I32(NearDist, 0x100);
    MPH_MEM_I32(FarDist, 0x104);
    MPH_MEM_I32(ViewportY2, 0x108);
    MPH_MEM_I32(ViewportY1, 0x10C);
    MPH_MEM_I32(ViewportX1, 0x110);
    MPH_MEM_I32(ViewportX2, 0x114);
    MPH_MEM_PTR(CurNode, 0x118);
};

class PlayerControls : public MemoryClass
{
public:
    PlayerControls(Memory& memory, std::int32_t address);
    PlayerControls(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field0, 0x0);
    MPH_MEM_U16(Field4, 0x4);
    MPH_MEM_U16(Field6, 0x6);
    MPH_MEM_U16(Field8, 0x8);
    MPH_MEM_U16(FieldA, 0xa);
    MPH_MEM_U16(FieldC, 0xc);
    MPH_MEM_U16(FieldE, 0xe);
    MPH_MEM_U16(Field10, 0x10);
    MPH_MEM_U16(Field12, 0x12);
    MPH_MEM_I32(Field14, 0x14);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1c);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_U16(Field24, 0x24);
    MPH_MEM_U16(Field26, 0x26);
    MPH_MEM_U16(Field28, 0x28);
    MPH_MEM_U16(Field2A, 0x2a);
    MPH_MEM_U16(Field2C, 0x2c);
    MPH_MEM_U16(Field2E, 0x2e);
    MPH_MEM_U16(Field30, 0x30);
    MPH_MEM_U16(Field32, 0x32);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3c);
    MPH_MEM_I32(Field40, 0x40);
    MPH_MEM_I32(Field44, 0x44);
    MPH_MEM_I32(Field48, 0x48);
    MPH_MEM_I32(Field4C, 0x4c);
    MPH_MEM_I32(Field50, 0x50);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_I32(Field58, 0x58);
    MPH_MEM_I32(Field5C, 0x5c);
    MPH_MEM_I32(Field60, 0x60);
    MPH_MEM_I32(Field64, 0x64);
    MPH_MEM_I32(Field68, 0x68);
    MPH_MEM_I32(Field6C, 0x6c);
    MPH_MEM_I32(Field70, 0x70);
    MPH_MEM_I32(Field74, 0x74);
    MPH_MEM_I32(Field78, 0x78);
    MPH_MEM_I32(Field7C, 0x7c);
    MPH_MEM_I32(Field80, 0x80);
    MPH_MEM_I32(Field84, 0x84);
    MPH_MEM_I32(Field88, 0x88);
    MPH_MEM_I32(Field8C, 0x8c);
    MPH_MEM_I32(Field90, 0x90);
    MPH_MEM_I32(Field94, 0x94);
    MPH_MEM_I32(Field98, 0x98);
};

class PlayerInput : public MemoryClass
{
public:
    PlayerInput(Memory& memory, std::int32_t address);
    PlayerInput(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Field0, 0x0);
    MPH_MEM_U16(Field2, 0x2);
    MPH_MEM_U16(Field4, 0x4);
    MPH_MEM_U16(Field6, 0x6);
    MPH_MEM_U16(Field8, 0x8);
    MPH_MEM_U16(FieldA, 0xa);
    MPH_MEM_I32(FieldC, 0xc);
    MPH_MEM_I32(Field10, 0x10);
    MPH_MEM_I32(Field14, 0x14);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1c);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_U16(Field24, 0x24);
    MPH_MEM_U8(Field26, 0x26);
    MPH_MEM_U8(Field27, 0x27);
    MPH_MEM_U16(Field28, 0x28);
    MPH_MEM_U16(Field2A, 0x2a);
    MPH_MEM_U16(Field2C, 0x2c);
    MPH_MEM_U16(Field2E, 0x2e);
    MPH_MEM_U16(Field30, 0x30);
    MPH_MEM_U16(Field32, 0x32);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3c);
    MPH_MEM_I32(Field40, 0x40);
    MPH_MEM_I32(Field44, 0x44);
};

class CameraSequence : public MemoryClass
{
public:
    CameraSequence(Memory& memory, std::int32_t address);
    CameraSequence(Memory& memory, std::intptr_t address);
    MPH_MEM_U8(Flags, 0x0);
    MPH_MEM_U8(Version, 0x1);
    MPH_MEM_U8(Field2, 0x2);
    MPH_MEM_U8(Field3, 0x3);
    MPH_MEM_I32(KeyframeElapsed, 0x4);
    MPH_MEM_PTR(Keyframes, 0x8);
    MPH_MEM_PTR(NextKeyframe, 0xC);
    MPH_MEM_PTR(CameraInfoPtr, 0x10);
    MPH_MEM_CHILD(CameraInfo, CameraInfo);
};

class CameraSequenceKeyframe : public MemoryClass
{
public:
    CameraSequenceKeyframe(Memory& memory, std::int32_t address);
    CameraSequenceKeyframe(Memory& memory, std::intptr_t address);
    MPH_MEM_VEC3(Pos, 0x0);
    MPH_MEM_VEC3(ToTarget, 0xC);
    MPH_MEM_I32(Roll, 0x18);
    MPH_MEM_I32(Fov, 0x1C);
    MPH_MEM_I32(MoveTime, 0x20);
    MPH_MEM_I32(HoldTime, 0x24);
    MPH_MEM_I32(FadeInTime, 0x28);
    MPH_MEM_I32(FadeOutTime, 0x2C);
    MPH_MEM_ENUM8(FadeType, FadeInType, 0x30);
    MPH_MEM_ENUM8(FadeType, FadeOutType, 0x31);
    MPH_MEM_U8(PrevFrameInfluence, 0x32);
    MPH_MEM_U8(AfterFrameInfluence, 0x33);
    MPH_MEM_U8(UseEntityTransform, 0x34);
    MPH_MEM_U8(Padding35, 0x35);
    MPH_MEM_U16(Padding36, 0x36);
    MPH_MEM_PTR(Ent1Ref, 0x38);
    MPH_MEM_PTR(Ent2Ref, 0x3C);
    MPH_MEM_PTR(EventTarget, 0x40);
    MPH_MEM_U16(EventId, 0x44);
    MPH_MEM_U16(EventParam, 0x46);
    MPH_MEM_I32(Easing, 0x48);
    MPH_MEM_I32(Unused4C, 0x4C);
    MPH_MEM_I32(Unused50, 0x50);
    MPH_MEM_PTR(NodeRef, 0x54);
    MPH_MEM_PTR(Next, 0x64);
    MPH_MEM_PTR(Prev, 0x68);
    MPH_MEM_U8(Index, 0x6C);
    MPH_MEM_U8(Padding6D, 0x6D);
    MPH_MEM_U16(Padding6E, 0x6E);
    MPH_MEM_CHILD(ByteArray, NodeNameRest);
};

class GameState : public MemoryClass
{
public:
    GameState(Memory& memory, std::int32_t address);
    GameState(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM8(GameMode, GameMode, 0x0);
    MPH_MEM_U8(RoomId, 0x1);
    MPH_MEM_U8(AreaId, 0x2);
    MPH_MEM_U8(PlayerCount, 0x3);
    MPH_MEM_U8(MaxPlayers, 0x4);
    MPH_MEM_U8(CountOfBitsOfSomething, 0x5);
    MPH_MEM_U8(BotCount, 0x6);
    MPH_MEM_U8(Field7, 0x7);
    MPH_MEM_U8(DmgMultIdx, 0xC);
    MPH_MEM_U8(FieldD, 0xD);
    MPH_MEM_U16(SomeFlags, 0xE);
    MPH_MEM_U16(Field10, 0x10);
    MPH_MEM_U16(Field12, 0x12);
    MPH_MEM_U16(PointLimit, 0x14);
    MPH_MEM_U16(Field16, 0x16);
    MPH_MEM_I32(BattleTimeLimit, 0x18);
    MPH_MEM_I32(EscapeState, 0x1C);
    MPH_MEM_I32(TimeLimit, 0x20);
    MPH_MEM_U8(PrimeHunterStats, 0x5C);
    MPH_MEM_U8(PrimeHunter, 0x5D);
    MPH_MEM_U8(RadShowPlayers, 0x5E);
    MPH_MEM_U8(FieldAF, 0x5F);
    MPH_MEM_I32(FieldD0, 0x80);
    MPH_MEM_I32(FieldD4, 0x84);
    MPH_MEM_I32(FieldD8, 0x88);
    MPH_MEM_I32(FieldDC, 0x8c);
    MPH_MEM_I32(FieldE0, 0x90);
    MPH_MEM_I32(FieldE4, 0x94);
    MPH_MEM_I32(FieldE8, 0x98);
    MPH_MEM_I32(FieldEC, 0x9c);
    MPH_MEM_I32(FieldF0, 0xa0);
    MPH_MEM_I32(FieldF4, 0xa4);
    MPH_MEM_I32(FieldF8, 0xa8);
    MPH_MEM_I32(FieldFC, 0xac);
    MPH_MEM_I32(Field100, 0xb0);
    MPH_MEM_I32(Field104, 0xb4);
    MPH_MEM_I32(Field108, 0xb8);
    MPH_MEM_I32(Field10C, 0xbc);
    MPH_MEM_I32(Field110, 0xc0);
    MPH_MEM_I32(Field114, 0xc4);
    MPH_MEM_I32(Field118, 0xc8);
    MPH_MEM_I32(Field11C, 0xcc);
    MPH_MEM_I32(Field120, 0xd0);
    MPH_MEM_I32(Field124, 0xd4);
    MPH_MEM_I32(Field128, 0xd8);
    MPH_MEM_I32(Field12C, 0xdc);
    MPH_MEM_I32(Field130, 0xe0);
    MPH_MEM_I32(Field134, 0xe4);
    MPH_MEM_I32(Field138, 0xe8);
    MPH_MEM_I32(Field13C, 0xec);
    MPH_MEM_I32(Field140, 0xf0);
    MPH_MEM_I32(Field144, 0xf4);
    MPH_MEM_I32(Field148, 0xf8);
    MPH_MEM_I32(Field14C, 0xfc);
    MPH_MEM_I32(Field150, 0x100);
    MPH_MEM_I32(Field154, 0x104);
    MPH_MEM_I32(Field158, 0x108);
    MPH_MEM_I32(Field15C, 0x10c);
    MPH_MEM_I32(Field170, 0x120);
    MPH_MEM_I32(Field174, 0x124);
    MPH_MEM_I32(Field178, 0x128);
    MPH_MEM_I32(Field17C, 0x12C);
    MPH_MEM_I32(Field1B0, 0x160);
    MPH_MEM_I32(Field1B4, 0x164);
    MPH_MEM_I32(Field1B8, 0x168);
    MPH_MEM_I32(Field1BC, 0x16C);
    MPH_MEM_I32(Field240, 0x1F0);
    MPH_MEM_I32(Field244, 0x1F4);
    MPH_MEM_I32(Field248, 0x1F8);
    MPH_MEM_I32(Field24C, 0x1FC);
    MPH_MEM_I32(Field254, 0x204);
    MPH_MEM_U8(Field258, 0x208);
    MPH_MEM_U8(Field259, 0x209);
    MPH_MEM_U16(Field25A, 0x20A);
    MPH_MEM_I32(Field278, 0x228);
    MPH_MEM_I32(Frames, 0x22C);
    MPH_MEM_I32(LoadRoomId, 0x230);
    MPH_MEM_I32(Field284, 0x234);
    MPH_MEM_I32(Field288, 0x238);
    MPH_MEM_U8(LayerId, 0x23C);
    MPH_MEM_U8(Field28D, 0x23D);
    MPH_MEM_U16(Field28E, 0x23E);
    MPH_MEM_CHILD(ByteArray, Field8);
    MPH_MEM_CHILD(Int32Array, Sensitivity);
    MPH_MEM_CHILD(ByteArray, InvertSomething);
    MPH_MEM_CHILD(ByteArray, Field38);
    MPH_MEM_CHILD(ByteArray, Hunters);
    MPH_MEM_CHILD(ByteArray, SuitColors);
    MPH_MEM_CHILD(ByteArray, PlayerNames);
    MPH_MEM_CHILD(ByteArray, BotEncounterState);
    MPH_MEM_CHILD(ByteArray, TeamIds);
    MPH_MEM_CHILD(ByteArray, FieldA0);
    MPH_MEM_CHILD(UInt16Array, BotSpawnerEntIds);
    MPH_MEM_CHILD(Int32Array, PrimeTime);
    MPH_MEM_CHILD(Int32Array, FieldC0);
    MPH_MEM_CHILD(Int32Array, Field160);
    MPH_MEM_CHILD(Int32Array, Field180);
    MPH_MEM_CHILD(Int32Array, Deaths);
    MPH_MEM_CHILD(Int32Array, Field1A0);
    MPH_MEM_CHILD(Int32Array, TeamkillsMaybe);
    MPH_MEM_CHILD(Int32Array, SuicidesMaybe);
    MPH_MEM_CHILD(Int32Array, Field1E0);
    MPH_MEM_CHILD(Int32Array, HeadshotsMaybe);
    MPH_MEM_CHILD(Int32Array, Field200);
    MPH_MEM_CHILD(Int32Array, DmgDealt);
    MPH_MEM_CHILD(Int32Array, DmgMax);
    MPH_MEM_CHILD(Int32Array, BattlePoints);
    MPH_MEM_CHILD(ByteArray, Standings);
    MPH_MEM_CHILD(ByteArray, KillStreaks);
    MPH_MEM_CHILD(UInt16Array, Field260);
    MPH_MEM_CHILD(UInt16Array, Field268);
    MPH_MEM_CHILD(UInt16Array, Field270);
};

class KioskGameState : public MemoryClass
{
public:
    KioskGameState(Memory& memory, std::int32_t address);
    KioskGameState(Memory& memory, std::intptr_t address);
    MPH_MEM_ENUM8(GameMode, GameMode, 0x0);
    MPH_MEM_U8(RoomId, 0x1);
    MPH_MEM_U8(AreaId, 0x2);
    MPH_MEM_U8(PlayerCount, 0x3);
    MPH_MEM_U8(MaxPlayers, 0x4);
    MPH_MEM_U8(BotCount, 0x5);
    MPH_MEM_U8(DmgMultIdx, 0xA);
    MPH_MEM_U8(FieldB, 0xB);
    MPH_MEM_U16(SomeFlags, 0xC);
    MPH_MEM_U16(FieldE, 0xE);
    MPH_MEM_U16(Field10, 0x10);
    MPH_MEM_U16(PointLimit, 0x12);
    MPH_MEM_I32(BattleTimeLimit, 0x14);
    MPH_MEM_I32(EscapeState, 0x18);
    MPH_MEM_I32(TimeLimit, 0x1C);
    MPH_MEM_U8(PrimeHunterStats, 0x58);
    MPH_MEM_U8(PrimeHunter, 0x59);
    MPH_MEM_U8(RadShowPlayers, 0x5A);
    MPH_MEM_U8(FieldAB, 0x5B);
    MPH_MEM_I32(FieldCC, 0x7c);
    MPH_MEM_I32(FieldD0, 0x80);
    MPH_MEM_I32(FieldD4, 0x84);
    MPH_MEM_I32(FieldD8, 0x88);
    MPH_MEM_I32(FieldDC, 0x8c);
    MPH_MEM_I32(FieldE0, 0x90);
    MPH_MEM_I32(FieldE4, 0x94);
    MPH_MEM_I32(FieldE9, 0x98);
    MPH_MEM_I32(FieldEC, 0x9c);
    MPH_MEM_I32(FieldF0, 0xa0);
    MPH_MEM_I32(FieldF4, 0xa4);
    MPH_MEM_I32(FieldF8, 0xa8);
    MPH_MEM_I32(FieldFC, 0xac);
    MPH_MEM_I32(Field100, 0xb0);
    MPH_MEM_I32(Field104, 0xb4);
    MPH_MEM_I32(Field108, 0xb8);
    MPH_MEM_I32(Field10C, 0xbc);
    MPH_MEM_I32(Field110, 0xc0);
    MPH_MEM_I32(Field114, 0xc4);
    MPH_MEM_I32(Field118, 0xc8);
    MPH_MEM_I32(Field11C, 0xcc);
    MPH_MEM_I32(Field120, 0xd0);
    MPH_MEM_I32(Field124, 0xd4);
    MPH_MEM_I32(Field128, 0xd8);
    MPH_MEM_I32(Field12C, 0xdc);
    MPH_MEM_I32(Field130, 0xe0);
    MPH_MEM_I32(Field134, 0xe4);
    MPH_MEM_I32(Field138, 0xe8);
    MPH_MEM_I32(Field13C, 0xec);
    MPH_MEM_I32(Field140, 0xf0);
    MPH_MEM_I32(Field144, 0xf4);
    MPH_MEM_I32(Field148, 0xf8);
    MPH_MEM_I32(Field14C, 0xfc);
    MPH_MEM_I32(Field150, 0x100);
    MPH_MEM_I32(Field154, 0x104);
    MPH_MEM_I32(Field158, 0x108);
    MPH_MEM_I32(Field15C, 0x10c);
    MPH_MEM_I32(Field160, 0x110);
    MPH_MEM_I32(Field164, 0x114);
    MPH_MEM_I32(Field168, 0x118);
    MPH_MEM_I32(Field16C, 0x11c);
    MPH_MEM_I32(Field170, 0x120);
    MPH_MEM_I32(Field174, 0x124);
    MPH_MEM_I32(Field178, 0x128);
    MPH_MEM_I32(Field18C, 0x13C);
    MPH_MEM_I32(Field190, 0x140);
    MPH_MEM_I32(Field194, 0x144);
    MPH_MEM_I32(Field198, 0x148);
    MPH_MEM_I32(Field1EC, 0x19C);
    MPH_MEM_I32(Field1F0, 0x1A0);
    MPH_MEM_I32(Field1F4, 0x1A4);
    MPH_MEM_I32(Field1F8, 0x1A8);
    MPH_MEM_I32(Field25C, 0x20C);
    MPH_MEM_I32(Field260, 0x210);
    MPH_MEM_I32(Field264, 0x214);
    MPH_MEM_I32(Field268, 0x218);
    MPH_MEM_I32(Field270, 0x220);
    MPH_MEM_U8(Field274, 0x224);
    MPH_MEM_U8(Field275, 0x225);
    MPH_MEM_U16(Field276, 0x226);
    MPH_MEM_I32(Field294, 0x244);
    MPH_MEM_I32(Frames, 0x248);
    MPH_MEM_I32(Field29C, 0x24C);
    MPH_MEM_I32(Field2A0, 0x250);
    MPH_MEM_I32(Field2A4, 0x254);
    MPH_MEM_I32(Field2A8, 0x258);
    MPH_MEM_I32(Field2AC, 0x25C);
    MPH_MEM_U8(LayerId, 0x260);
    MPH_MEM_U8(Field2B1, 0x261);
    MPH_MEM_U16(Field2B2, 0x262);
    MPH_MEM_CHILD(ByteArray, Field6);
    MPH_MEM_CHILD(Int32Array, Sensitivity);
    MPH_MEM_CHILD(ByteArray, InvertSomething);
    MPH_MEM_CHILD(ByteArray, Field34);
    MPH_MEM_CHILD(ByteArray, Hunters);
    MPH_MEM_CHILD(ByteArray, SuitColors);
    MPH_MEM_CHILD(ByteArray, PlayerNames);
    MPH_MEM_CHILD(ByteArray, BotEncounterState);
    MPH_MEM_CHILD(ByteArray, TeamIds);
    MPH_MEM_CHILD(ByteArray, Field9C);
    MPH_MEM_CHILD(UInt16Array, BotSpawnerEntIds);
    MPH_MEM_CHILD(Int32Array, PrimeTime);
    MPH_MEM_CHILD(Int32Array, FieldBC);
    MPH_MEM_CHILD(Int32Array, Field17C);
    MPH_MEM_CHILD(Int32Array, Field19C);
    MPH_MEM_CHILD(Int32Array, Deaths);
    MPH_MEM_CHILD(Int32Array, Field1BC);
    MPH_MEM_CHILD(Int32Array, Field1CC);
    MPH_MEM_CHILD(Int32Array, SuicidesMaybe);
    MPH_MEM_CHILD(Int32Array, Field1FC);
    MPH_MEM_CHILD(Int32Array, HeadshotsMaybe);
    MPH_MEM_CHILD(Int32Array, Field21C);
    MPH_MEM_CHILD(Int32Array, DmgDealt);
    MPH_MEM_CHILD(Int32Array, DmgMax);
    MPH_MEM_CHILD(Int32Array, BattlePoints);
    MPH_MEM_CHILD(ByteArray, Field26C);
    MPH_MEM_CHILD(ByteArray, KillStreaks);
    MPH_MEM_CHILD(UInt16Array, Field27C);
    MPH_MEM_CHILD(UInt16Array, Field284);
    MPH_MEM_CHILD(UInt16Array, Field28C);
};

class RoomState : public MemoryClass
{
public:
    RoomState(Memory& memory, std::int32_t address);
    RoomState(Memory& memory, std::intptr_t address);
    MPH_MEM_CHILD(ByteArray, Bits);
};

class StorySaveData : public MemoryClass
{
public:
    StorySaveData(Memory& memory, std::int32_t address);
    StorySaveData(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(Weapons, 0x0);
    MPH_MEM_U8(Padding5, 0x5);
    MPH_MEM_U16(Energy, 0xE);
    MPH_MEM_U16(EnergyCap, 0x10);
    MPH_MEM_U16(GameFlags, 0x12);
    MPH_MEM_U16(Field14, 0x14);
    MPH_MEM_U16(LastCheckpoint, 0x16);
    MPH_MEM_U32(Bosses, 0x18);
    MPH_MEM_U32(Artifacts, 0x1C);
    MPH_MEM_U32(LostOctos, 0x20);
    MPH_MEM_U8(CurOctos, 0x24);
    MPH_MEM_U8(OwnOctos, 0x25);
    MPH_MEM_U8(FoundOctos, 0x26);
    MPH_MEM_I32(FieldFD4, 0xfd4);
    MPH_MEM_I32(FieldFD8, 0xfd8);
    MPH_MEM_I32(FieldFDC, 0xfdc);
    MPH_MEM_I32(FieldFE0, 0xfe0);
    MPH_MEM_I32(FieldFE4, 0xfe4);
    MPH_MEM_I32(FieldFE8, 0xfe8);
    MPH_MEM_I32(FieldFEC, 0xfec);
    MPH_MEM_I32(FieldFF0, 0xff0);
    MPH_MEM_I32(FieldFF4, 0xff4);
    MPH_MEM_I32(FieldFF8, 0xff8);
    MPH_MEM_I32(FieldFFC, 0xffc);
    MPH_MEM_I32(Field1000, 0x1000);
    MPH_MEM_I32(Field1004, 0x1004);
    MPH_MEM_I32(Field1008, 0x1008);
    MPH_MEM_I32(Field100C, 0x100c);
    MPH_MEM_I32(Field1010, 0x1010);
    MPH_MEM_I32(Field1018, 0x1018);
    MPH_MEM_I32(Field105C, 0x105c);
    MPH_MEM_I32(Field1060, 0x1060);
    MPH_MEM_I32(Field1064, 0x1064);
    MPH_MEM_I32(Field1068, 0x1068);
    MPH_MEM_I32(Field106C, 0x106c);
    MPH_MEM_I32(Field1070, 0x1070);
    MPH_MEM_I32(Field1074, 0x1074);
    MPH_MEM_I32(Field1078, 0x1078);
    MPH_MEM_I32(Field107C, 0x107c);
    MPH_MEM_U8(Field1084, 0x1084);
    MPH_MEM_U8(RoomId, 0x1085);
    MPH_MEM_U8(SlotHunterBits, 0x1086);
    MPH_MEM_U8(DefeatedHunters, 0x1087);
    MPH_MEM_I32(HunterKills, 0x1088);
    MPH_MEM_I32(DeathsFromHunter, 0x108C);
    MPH_MEM_I32(DeathTotal, 0x1090);
    MPH_MEM_I32(Field1094, 0x1094);
    MPH_MEM_I32(Field1098, 0x1098);
    MPH_MEM_I32(Field109C, 0x109C);
    MPH_MEM_I32(Field10A0, 0x10A0);
    MPH_MEM_I32(ScanCount, 0x10A4);
    MPH_MEM_I32(EquipData, 0x10A8);
    MPH_MEM_U32(MaxScanCount, 0x10AC);
    MPH_MEM_I32(MaxEquipData, 0x10B0);
    MPH_MEM_CHILD(ByteArray, WeaponSlots);
    MPH_MEM_CHILD(UInt16Array, Ammo);
    MPH_MEM_CHILD(UInt16Array, AmmoCaps);
    MPH_MEM_CHILD(ByteArray, VisitedRooms);
    MPH_MEM_CHILD(Int32Array, VisitedConnectors);
    MPH_MEM_CHILD(StructArray<RoomState>, RoomState);
    MPH_MEM_CHILD(ByteArray, FieldFCC);
    MPH_MEM_CHILD(ByteArray, TriggerStateBits);
    MPH_MEM_CHILD(ByteArray, Logbook);
    MPH_MEM_CHILD(ByteArray, AreaHunters);
};

class SaveType3 : public MemoryClass
{
public:
    SaveType3(Memory& memory, std::int32_t address);
    SaveType3(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field0, 0x0);
    MPH_MEM_I32(Field4, 0x4);
    MPH_MEM_I32(Field8, 0x8);
    MPH_MEM_I32(FieldC, 0xc);
    MPH_MEM_I32(Field10, 0x10);
    MPH_MEM_I32(Field14, 0x14);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1c);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2c);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3c);
};

class StatsAndSettings : public MemoryClass
{
public:
    StatsAndSettings(Memory& memory, std::int32_t address);
    StatsAndSettings(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field0, 0x0);
    MPH_MEM_I32(AreaBits, 0x4);
    MPH_MEM_I32(MultiplayerCharacters, 0x8);
    MPH_MEM_I32(FieldC, 0xC);
    MPH_MEM_I32(Field10, 0x10);
    MPH_MEM_I32(TouchpadSensitivity, 0x14);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_I32(Field1C, 0x1c);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2c);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(Field38, 0x38);
    MPH_MEM_I32(Field3C, 0x3c);
    MPH_MEM_I32(Field40, 0x40);
    MPH_MEM_I32(Field44, 0x44);
    MPH_MEM_I32(Field48, 0x48);
    MPH_MEM_I32(Field4C, 0x4c);
    MPH_MEM_I32(Field50, 0x50);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_I32(Field58, 0x58);
    MPH_MEM_I32(Field5C, 0x5c);
    MPH_MEM_I32(Field60, 0x60);
    MPH_MEM_I32(Field64, 0x64);
    MPH_MEM_I32(Field68, 0x68);
    MPH_MEM_I32(Field6C, 0x6c);
    MPH_MEM_I32(Field70, 0x70);
    MPH_MEM_I32(Field74, 0x74);
    MPH_MEM_I32(Field78, 0x78);
    MPH_MEM_I32(Field7C, 0x7c);
    MPH_MEM_I32(Field80, 0x80);
    MPH_MEM_I32(Field84, 0x84);
    MPH_MEM_I32(Field88, 0x88);
    MPH_MEM_I32(Field8C, 0x8c);
    MPH_MEM_I32(Field90, 0x90);
    MPH_MEM_I32(Field94, 0x94);
    MPH_MEM_I32(Field98, 0x98);
    MPH_MEM_I32(Field9C, 0x9c);
    MPH_MEM_I32(EnemyKillsMaybe, 0xA0);
};

class LicenseInfo : public MemoryClass
{
public:
    LicenseInfo(Memory& memory, std::int32_t address);
    LicenseInfo(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(Field18, 0x18);
    MPH_MEM_U16(RankPoints, 0x1C);
    MPH_MEM_U16(Field1E, 0x1E);
    MPH_MEM_I32(Field20, 0x20);
    MPH_MEM_I32(Field24, 0x24);
    MPH_MEM_I32(Field28, 0x28);
    MPH_MEM_I32(Field2C, 0x2c);
    MPH_MEM_I32(Field30, 0x30);
    MPH_MEM_I32(Field34, 0x34);
    MPH_MEM_I32(HeadshotCount, 0x38);
    MPH_MEM_I32(Field3C, 0x3C);
    MPH_MEM_I32(GamplayTime1, 0x40);
    MPH_MEM_I32(GamplayTime2, 0x44);
    MPH_MEM_I32(GamplayTime3, 0x48);
    MPH_MEM_I32(Field4C, 0x4c);
    MPH_MEM_I32(Field50, 0x50);
    MPH_MEM_I32(Field54, 0x54);
    MPH_MEM_I32(Field58, 0x58);
    MPH_MEM_I32(Field5C, 0x5c);
    MPH_MEM_I32(Field60, 0x60);
    MPH_MEM_I32(Field1D4, 0x1D4);
    MPH_MEM_I32(Field1D8, 0x1D8);
    MPH_MEM_I32(Field1DC, 0x1DC);
    MPH_MEM_I32(Field1E0, 0x1E0);
    MPH_MEM_CHILD(ByteArray, Nickname);
    MPH_MEM_CHILD(Int32Array, Field64);
    MPH_MEM_CHILD(Int32Array, Field74);
    MPH_MEM_CHILD(Int32Array, Field90);
    MPH_MEM_CHILD(Int32Array, FieldAC);
    MPH_MEM_CHILD(Int32Array, FieldD0);
    MPH_MEM_CHILD(Int32Array, Field144);
    MPH_MEM_CHILD(Int32Array, Field1B8);
    MPH_MEM_CHILD(ByteArray, Field1E4);
};

class FriendsRivals : public MemoryClass
{
public:
    FriendsRivals(Memory& memory, std::int32_t address);
    FriendsRivals(Memory& memory, std::intptr_t address);
    MPH_MEM_CHILD(Int32Array, Fields);
};

class RoomDescription : public MemoryClass
{
public:
    RoomDescription(Memory& memory, std::int32_t address);
    RoomDescription(Memory& memory, std::intptr_t address);
    MPH_MEM_PTR(Name, 0x0);
    MPH_MEM_PTR(Model, 0x4);
    MPH_MEM_PTR(Anim, 0x8);
    MPH_MEM_PTR(Tex, 0xC);
    MPH_MEM_PTR(Collision, 0x10);
    MPH_MEM_PTR(Ent, 0x14);
    MPH_MEM_PTR(Nodedata, 0x18);
    MPH_MEM_PTR(RoomNodeName, 0x1C);
    MPH_MEM_I32(BattleTimeLimit, 0x20);
    MPH_MEM_I32(TimeLimit, 0x24);
    MPH_MEM_U16(PointLimit, 0x28);
    MPH_MEM_U16(LayerId, 0x2A);
    MPH_MEM_I32(FarClipDist, 0x2C);
    MPH_MEM_U16(FogEnable, 0x30);
    MPH_MEM_U16(ClearFog, 0x32);
    MPH_MEM_U16(FogColor, 0x34);
    MPH_MEM_U16(Padding36, 0x36);
    MPH_MEM_U32(FogSlope, 0x38);
    MPH_MEM_I32(FogOffset, 0x3C);
    MPH_MEM_COLOR(Light1Color, 0x40);
    MPH_MEM_U8(Padding43, 0x43);
    MPH_MEM_VEC3(Light1Vec, 0x44);
    MPH_MEM_COLOR(Light2Color, 0x50);
    MPH_MEM_U8(Padding53, 0x53);
    MPH_MEM_VEC3(Light2Vec, 0x54);
    MPH_MEM_PTR(InternalName, 0x60);
    MPH_MEM_PTR(Archive, 0x64);
    MPH_MEM_I32(KillHeight, 0x68);
    MPH_MEM_I32(Size, 0x6C);
};

class AiButton : public MemoryClass
{
public:
    AiButton(Memory& memory, std::int32_t address);
    AiButton(Memory& memory, std::intptr_t address);
    MPH_MEM_U16(IsDown, 0x0);
    MPH_MEM_U16(FramesDown, 0x2);
    MPH_MEM_U16(FramesUp, 0x4);
};

class VecFx32 : public MemoryClass
{
public:
    VecFx32(Memory& memory, std::int32_t address);
    VecFx32(Memory& memory, std::intptr_t address);
    MPH_MEM_I32(X, 0x0);
    MPH_MEM_I32(Y, 0x4);
    MPH_MEM_I32(Z, 0x8);
};

class MtxFx43 : public MemoryClass
{
public:
    MtxFx43(Memory& memory, std::int32_t address);
    MtxFx43(Memory& memory, std::intptr_t address);
    MPH_MEM_CHILD(Int32Array, M);
    MPH_MEM_CHILD(VecFx32, Row0);
    MPH_MEM_CHILD(VecFx32, Row1);
    MPH_MEM_CHILD(VecFx32, Row2);
    MPH_MEM_CHILD(VecFx32, Row3);
};

#undef MPH_MEM_S8
#undef MPH_MEM_U8
#undef MPH_MEM_I16
#undef MPH_MEM_U16
#undef MPH_MEM_I32
#undef MPH_MEM_U32
#undef MPH_MEM_PTR
#undef MPH_MEM_COLOR
#undef MPH_MEM_VEC3
#undef MPH_MEM_VEC4
#undef MPH_MEM_MTX
#undef MPH_MEM_ENUM8
#undef MPH_MEM_ENUM16
#undef MPH_MEM_ENUMI32
#undef MPH_MEM_ENUM32
#undef MPH_MEM_CHILD
}

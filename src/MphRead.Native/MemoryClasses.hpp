#pragma once

#include "Formats/enum_tables.hpp"
#include "Mods/Network/map_rotation.hpp"
#include "Memory.hpp"
#include "Formats/Types.hpp"
#include "Entities/Players/PlayerAi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fruityprime::memory {

struct FieldInfo {
    std::string name;
    std::size_t offset = 0;
    std::size_t size = 0;
};

class Layout {
public:
    [[nodiscard]] bool add(std::string name, std::size_t offset,
                          std::size_t size);
    [[nodiscard]] const FieldInfo* find(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<FieldInfo>& fields() const noexcept
        { return fields_; }

private:
    std::vector<FieldInfo> fields_;
};

class Object {
public:
    Object(Buffer& buffer, const Layout& layout,
           std::size_t base_offset = 0) noexcept
        : buffer_(&buffer), layout_(&layout), base_offset_(base_offset) {}

    [[nodiscard]] std::uint32_t read_u32(std::string_view field) const;
    void write_u32(std::string_view field, std::uint32_t value);
    [[nodiscard]] const Layout& layout() const noexcept { return *layout_; }
    [[nodiscard]] std::size_t base_offset() const noexcept
        { return base_offset_; }

private:
    [[nodiscard]] const FieldInfo& require_field(std::string_view field) const;
    Buffer* buffer_ = nullptr;
    const Layout* layout_ = nullptr;
    std::size_t base_offset_ = 0;
};

// Native counterpart of MemoryClasses.cs::MemoryClass.  DS pointers
// remain 32-bit values; base_offset is the checked Buffer coordinate.
class MemoryClass {
public:
    MemoryClass(Buffer& buffer, std::size_t base_offset,
                std::uint32_t address = 0) noexcept
        : buffer_(&buffer), base_offset_(base_offset), address_(address) {}
    MemoryClass(Buffer& buffer, std::uint32_t address) noexcept
        : buffer_(&buffer), base_offset_(offset_for_address(buffer, address)),
          address_(address) {}
    virtual ~MemoryClass() = default;

    [[nodiscard]] std::uint32_t address() const noexcept { return address_; }
    [[nodiscard]] std::uint32_t Address() const noexcept { return address_; }
    [[nodiscard]] std::size_t base_offset() const noexcept { return base_offset_; }

    friend bool operator==(const MemoryClass& left,
                           const MemoryClass& right) noexcept {
        return left.address_ == right.address_;
    }
    friend bool operator!=(const MemoryClass& left,
                           const MemoryClass& right) noexcept {
        return !(left == right);
    }

    [[nodiscard]] std::int8_t read_i8(std::size_t offset = 0) const {
        return static_cast<std::int8_t>(buffer().read_u8(at(offset)));
    }
    [[nodiscard]] std::uint8_t read_u8(std::size_t offset = 0) const {
        return buffer().read_u8(at(offset));
    }
    [[nodiscard]] std::int16_t read_i16(std::size_t offset = 0) const {
        return buffer().read_i16_le(at(offset));
    }
    [[nodiscard]] std::uint16_t read_u16(std::size_t offset = 0) const {
        return buffer().read_u16_le(at(offset));
    }
    [[nodiscard]] std::int32_t read_i32(std::size_t offset = 0) const {
        return buffer().read_i32_le(at(offset));
    }
    [[nodiscard]] std::uint32_t read_u32(std::size_t offset = 0) const {
        return buffer().read_u32_le(at(offset));
    }
    [[nodiscard]] float read_f32(std::size_t offset = 0) const {
        return buffer().read_f32_le(at(offset));
    }

    void write_i8(std::size_t offset, std::int8_t value) {
        buffer().write_u8(at(offset), static_cast<std::uint8_t>(value));
    }
    void write_u8(std::size_t offset, std::uint8_t value) {
        buffer().write_u8(at(offset), value);
    }
    void write_i16(std::size_t offset, std::int16_t value) {
        buffer().write_u16_le(at(offset), static_cast<std::uint16_t>(value));
    }
    void write_u16(std::size_t offset, std::uint16_t value) {
        buffer().write_u16_le(at(offset), value);
    }
    void write_i32(std::size_t offset, std::int32_t value) {
        buffer().write_u32_le(at(offset), static_cast<std::uint32_t>(value));
    }
    void write_u32(std::size_t offset, std::uint32_t value) {
        buffer().write_u32_le(at(offset), value);
    }
    void write_f32(std::size_t offset, float value) {
        buffer().write_f32_le(at(offset), value);
    }

    [[nodiscard]] std::uint32_t read_pointer(std::size_t offset = 0) const {
        return read_u32(offset);
    }
    void write_pointer(std::size_t offset, std::uint32_t value) {
        write_u32(offset, value);
    }
    [[nodiscard]] formats::ColorRgb read_color3(std::size_t offset = 0) const {
        return {read_u8(offset), read_u8(offset + 1), read_u8(offset + 2)};
    }
    void write_color3(std::size_t offset, formats::ColorRgb value) {
        write_u8(offset, value.red); write_u8(offset + 1, value.green);
        write_u8(offset + 2, value.blue);
    }
    [[nodiscard]] formats::Vector3 read_vec3(std::size_t offset = 0) const {
        return {static_cast<float>(read_i32(offset)) / 4096.0F,
                static_cast<float>(read_i32(offset + 4)) / 4096.0F,
                static_cast<float>(read_i32(offset + 8)) / 4096.0F};
    }
    void write_vec3(std::size_t offset, formats::Vector3 value) {
        write_i32(offset, fixed(value.x)); write_i32(offset + 4, fixed(value.y));
        write_i32(offset + 8, fixed(value.z));
    }
    [[nodiscard]] formats::Vector4 read_vec4(std::size_t offset = 0) const {
        return {static_cast<float>(read_i32(offset)) / 4096.0F,
                static_cast<float>(read_i32(offset + 4)) / 4096.0F,
                static_cast<float>(read_i32(offset + 8)) / 4096.0F,
                static_cast<float>(read_i32(offset + 12)) / 4096.0F};
    }
    void write_vec4(std::size_t offset, formats::Vector4 value) {
        write_i32(offset, fixed(value.x)); write_i32(offset + 4, fixed(value.y));
        write_i32(offset + 8, fixed(value.z)); write_i32(offset + 12, fixed(value.w));
    }
    [[nodiscard]] formats::Matrix4x3 read_mtx43(std::size_t offset = 0) const {
        const auto a = read_vec3(offset); const auto b = read_vec3(offset + 12);
        const auto c = read_vec3(offset + 24); const auto d = read_vec3(offset + 36);
        return {a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, d.x, d.y, d.z};
    }
    void write_mtx43(std::size_t offset, formats::Matrix4x3 value) {
        write_vec3(offset, {value.m11, value.m12, value.m13});
        write_vec3(offset + 12, {value.m21, value.m22, value.m23});
        write_vec3(offset + 24, {value.m31, value.m32, value.m33});
        write_vec3(offset + 36, {value.m41, value.m42, value.m43});
    }

    static constexpr std::uint32_t AddressBase = 0x02000000U;
    [[nodiscard]] static std::size_t offset_for_address(
        const Buffer& buffer, std::uint32_t address) noexcept {
        (void)buffer;
        if (address >= AddressBase) return static_cast<std::size_t>(address - AddressBase);
        return static_cast<std::size_t>(address);
    }

protected:
    [[nodiscard]] Buffer& buffer() noexcept { return *buffer_; }
    [[nodiscard]] const Buffer& buffer() const noexcept { return *buffer_; }
    [[nodiscard]] std::size_t at(std::size_t offset) const noexcept {
        return base_offset_ + offset;
    }
    [[nodiscard]] std::size_t offset_for_address(std::uint32_t address) const noexcept {
        return offset_for_address(*buffer_, address);
    }

private:
    [[nodiscard]] static std::int32_t fixed(float value) noexcept {
        const double scaled = static_cast<double>(value) * 4096.0;
        if (!std::isfinite(scaled)) return 0;
        if (scaled <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
            return std::numeric_limits<std::int32_t>::min();
        if (scaled >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
            return std::numeric_limits<std::int32_t>::max();
        return static_cast<std::int32_t>(scaled);
    }
    Buffer* buffer_ = nullptr;
    std::size_t base_offset_ = 0;
    std::uint32_t address_ = 0;
};

// C# MemoryArray<T> equivalent.  Every element remains a live view of
// the backing Buffer; arrays never copy cartridge memory.
class MemoryArrayBase : public MemoryClass {
public:
    [[nodiscard]] std::size_t length() const noexcept { return length_; }
    [[nodiscard]] std::size_t Length() const noexcept { return length_; }

protected:
    MemoryArrayBase(Buffer& buffer, std::size_t base_offset,
                     std::size_t length, std::uint32_t address);
    MemoryArrayBase(Buffer& buffer, std::uint32_t address,
                     std::size_t length);
    void require_index(std::size_t index) const;
    [[nodiscard]] std::size_t element_offset(std::size_t index,
                                             std::size_t width) const;

private:
    std::size_t length_ = 0;
};

template <typename T>
class ScalarArray : public MemoryArrayBase {
public:
    ScalarArray(Buffer& buffer, std::size_t base_offset,
                std::size_t length, std::uint32_t address = 0)
        : MemoryArrayBase(buffer, base_offset, length, address) {}
    ScalarArray(Buffer& buffer, std::uint32_t address,
                std::size_t length)
        : MemoryArrayBase(buffer, address, length) {}

    [[nodiscard]] T get(std::size_t index) const {
        require_index(index);
        const auto offset = element_offset(index, sizeof(T));
        if constexpr (std::is_same_v<T, std::int8_t>) return read_i8(offset);
        else if constexpr (std::is_same_v<T, std::uint8_t>) return read_u8(offset);
        else if constexpr (std::is_same_v<T, std::int16_t>) return read_i16(offset);
        else if constexpr (std::is_same_v<T, std::uint16_t>) return read_u16(offset);
        else if constexpr (std::is_same_v<T, std::int32_t>) return read_i32(offset);
        else if constexpr (std::is_same_v<T, std::uint32_t>) return read_u32(offset);
        else if constexpr (std::is_same_v<T, float>) return read_f32(offset);
        else static_assert(std::is_same_v<T, void>, "unsupported MemoryArray scalar");
    }

    void set(std::size_t index, T value) {
        require_index(index);
        const auto offset = element_offset(index, sizeof(T));
        if constexpr (std::is_same_v<T, std::int8_t>) write_i8(offset, value);
        else if constexpr (std::is_same_v<T, std::uint8_t>) write_u8(offset, value);
        else if constexpr (std::is_same_v<T, std::int16_t>) write_i16(offset, value);
        else if constexpr (std::is_same_v<T, std::uint16_t>) write_u16(offset, value);
        else if constexpr (std::is_same_v<T, std::int32_t>) write_i32(offset, value);
        else if constexpr (std::is_same_v<T, std::uint32_t>) write_u32(offset, value);
        else if constexpr (std::is_same_v<T, float>) write_f32(offset, value);
        else static_assert(std::is_same_v<T, void>, "unsupported MemoryArray scalar");
    }

    [[nodiscard]] T operator[](std::size_t index) const { return get(index); }
    void set_at(std::size_t index, T value) { set(index, value); }
};

using SByteArray = ScalarArray<std::int8_t>;
using ByteArray = ScalarArray<std::uint8_t>;
using Int16Array = ScalarArray<std::int16_t>;
using UInt16Array = ScalarArray<std::uint16_t>;
using Int32Array = ScalarArray<std::int32_t>;
using UInt32Array = ScalarArray<std::uint32_t>;
using IntPtrArray = ScalarArray<std::uint32_t>;

template <typename Enum>
class U32EnumArray : public MemoryArrayBase {
public:
    U32EnumArray(Buffer& buffer, std::size_t base_offset,
                  std::size_t length, std::uint32_t address = 0)
        : MemoryArrayBase(buffer, base_offset, length, address) {}
    [[nodiscard]] Enum get(std::size_t index) const {
        return static_cast<Enum>(read_u32(element_offset(index, 4)));
    }
    void set(std::size_t index, Enum value) {
        write_u32(element_offset(index, 4), static_cast<std::uint32_t>(value));
    }
    [[nodiscard]] Enum operator[](std::size_t index) const { return get(index); }
};

template <typename T>
class StructArray : public MemoryArrayBase {
public:
    StructArray(Buffer& buffer, std::size_t base_offset,
                std::size_t length, std::size_t element_size,
                std::uint32_t address = 0)
        : MemoryArrayBase(buffer, base_offset, length, address) {
        items_.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            items_.push_back(std::make_unique<T>(
                buffer, base_offset + i * element_size,
                address + static_cast<std::uint32_t>(i * element_size)));
        }
    }
    StructArray(Buffer& buffer, std::uint32_t address,
                std::size_t length, std::size_t element_size)
        : StructArray(buffer, MemoryClass::offset_for_address(buffer, address),
                       length, element_size, address) {}

    [[nodiscard]] T& get(std::size_t index) {
        require_index(index);
        return *items_[index];
    }
    [[nodiscard]] const T& get(std::size_t index) const {
        require_index(index);
        return *items_[index];
    }
    [[nodiscard]] T& operator[](std::size_t index) { return get(index); }
    [[nodiscard]] const T& operator[](std::size_t index) const { return get(index); }
    void set(std::size_t, const T&) {
        throw std::logic_error("embedded MemoryClass writes are not supported");
    }

private:
    std::vector<std::unique_ptr<T>> items_;
};

// Forward declarations allow the managed source order to stay visible
// even when a class contains an embedded view of a later class.
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

class CEntity : public MemoryClass {
public:
    CEntity(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEntity(Buffer& buffer, std::uint32_t address);
    ~CEntity() override;

    [[nodiscard]] formats::EntityType EntityType() const { return static_cast<formats::EntityType>(read_u16(0x0)); }
    void EntityType(formats::EntityType value) { write_u16(0x0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] formats::EntityType entity_type() const { return EntityType(); }
    void entity_type(formats::EntityType value) { EntityType(value); }
    [[nodiscard]] std::uint16_t EntityId() const { return read_u16(0x2); }
    void EntityId(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t entity_id() const { return EntityId(); }
    void entity_id(std::uint16_t value) { EntityId(value); }
    [[nodiscard]] std::uint16_t ScanId() const { return read_u16(0x4); }
    void ScanId(std::uint16_t value) { write_u16(0x4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t scan_id() const { return ScanId(); }
    void scan_id(std::uint16_t value) { ScanId(value); }
    [[nodiscard]] std::uint16_t Padding6() const { return read_u16(0x6); }
    void Padding6(std::uint16_t value) { write_u16(0x6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding6() const { return Padding6(); }
    void padding6(std::uint16_t value) { Padding6(value); }
    [[nodiscard]] std::uint32_t MtxPtr() const { return read_pointer(0x8); }
    void MtxPtr(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t mtx_ptr() const { return MtxPtr(); }
    void mtx_ptr(std::uint32_t value) { MtxPtr(value); }
    [[nodiscard]] std::uint32_t Funcs() const { return read_pointer(0xC); }
    void Funcs(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t funcs() const { return Funcs(); }
    void funcs(std::uint32_t value) { Funcs(value); }
    [[nodiscard]] std::uint32_t Prev() const { return read_pointer(0x10); }
    void Prev(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t prev() const { return Prev(); }
    void prev(std::uint32_t value) { Prev(value); }
    [[nodiscard]] std::uint32_t Next() const { return read_pointer(0x14); }
    void Next(std::uint32_t value) { write_pointer(0x14, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t next() const { return Next(); }
    void next(std::uint32_t value) { Next(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x2;
    static constexpr std::size_t _off2 = 0x4;
    static constexpr std::size_t _off3 = 0x6;
    static constexpr std::size_t _off4 = 0x8;
    static constexpr std::size_t _off5 = 0xC;
    static constexpr std::size_t _off6 = 0x10;
    static constexpr std::size_t _off7 = 0x14;
};

class CEnemyBase : public CEntity {
public:
    CEnemyBase(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemyBase(Buffer& buffer, std::uint32_t address);
    ~CEnemyBase() override;

    [[nodiscard]] std::uint16_t Flags() const { return read_u16(0x18); }
    void Flags(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t flags() const { return Flags(); }
    void flags(std::uint16_t value) { Flags(value); }
    [[nodiscard]] formats::EnemyType Type() const { return static_cast<formats::EnemyType>(read_u8(0x1A)); }
    void Type(formats::EnemyType value) { write_u8(0x1A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::EnemyType type() const { return Type(); }
    void type(formats::EnemyType value) { Type(value); }
    [[nodiscard]] std::uint8_t State() const { return read_u8(0x1B); }
    void State(std::uint8_t value) { write_u8(0x1B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t state() const { return State(); }
    void state(std::uint8_t value) { State(value); }
    [[nodiscard]] std::uint8_t NextSubId() const { return read_u8(0x1C); }
    void NextSubId(std::uint8_t value) { write_u8(0x1C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t next_sub_id() const { return NextSubId(); }
    void next_sub_id(std::uint8_t value) { NextSubId(value); }
    [[nodiscard]] std::uint8_t HealthbarMsgId() const { return read_u8(0x1D); }
    void HealthbarMsgId(std::uint8_t value) { write_u8(0x1D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t healthbar_msg_id() const { return HealthbarMsgId(); }
    void healthbar_msg_id(std::uint8_t value) { HealthbarMsgId(value); }
    [[nodiscard]] std::uint8_t TimeSinceDmg() const { return read_u8(0x1E); }
    void TimeSinceDmg(std::uint8_t value) { write_u8(0x1E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_dmg() const { return TimeSinceDmg(); }
    void time_since_dmg(std::uint8_t value) { TimeSinceDmg(value); }
    [[nodiscard]] std::uint8_t HitPlayerBits() const { return read_u8(0x1F); }
    void HitPlayerBits(std::uint8_t value) { write_u8(0x1F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t hit_player_bits() const { return HitPlayerBits(); }
    void hit_player_bits(std::uint8_t value) { HitPlayerBits(value); }
    [[nodiscard]] std::uint32_t Effectiveness() const { return read_u32(0x20); }
    void Effectiveness(std::uint32_t value) { write_u32(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effectiveness() const { return Effectiveness(); }
    void effectiveness(std::uint32_t value) { Effectiveness(value); }
    [[nodiscard]] std::uint32_t Owner() const { return read_pointer(0x24); }
    void Owner(std::uint32_t value) { write_pointer(0x24, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t owner() const { return Owner(); }
    void owner(std::uint32_t value) { Owner(value); }
    [[nodiscard]] formats::Vector3 LinkInvPos() const { return read_vec3(0x28); }
    void LinkInvPos(formats::Vector3 value) { write_vec3(0x28, value); }
    [[nodiscard]] formats::Vector3 link_inv_pos() const { return LinkInvPos(); }
    void link_inv_pos(formats::Vector3 value) { LinkInvPos(value); }
    [[nodiscard]] formats::Vector3 LinkInvVec2() const { return read_vec3(0x34); }
    void LinkInvVec2(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 link_inv_vec2() const { return LinkInvVec2(); }
    void link_inv_vec2(formats::Vector3 value) { LinkInvVec2(value); }
    [[nodiscard]] formats::Vector3 LinkInvVec1() const { return read_vec3(0x40); }
    void LinkInvVec1(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 link_inv_vec1() const { return LinkInvVec1(); }
    void link_inv_vec1(formats::Vector3 value) { LinkInvVec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x4C); }
    void Pos(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 PrevPos() const { return read_vec3(0x58); }
    void PrevPos(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 prev_pos() const { return PrevPos(); }
    void prev_pos(formats::Vector3 value) { PrevPos(value); }
    [[nodiscard]] formats::Vector3 Speed() const { return read_vec3(0x64); }
    void Speed(formats::Vector3 value) { write_vec3(0x64, value); }
    [[nodiscard]] formats::Vector3 speed() const { return Speed(); }
    void speed(formats::Vector3 value) { Speed(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x77); }
    void Vec2(formats::Vector3 value) { write_vec3(0x77, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x7C); }
    void Vec1(formats::Vector3 value) { write_vec3(0x7C, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] std::int32_t BoundingRadius() const { return read_i32(0x88); }
    void BoundingRadius(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t bounding_radius() const { return BoundingRadius(); }
    void bounding_radius(std::int32_t value) { BoundingRadius(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& HurtVolUnxf() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& HurtVolUnxf() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& hurt_vol_unxf() noexcept { return HurtVolUnxf(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& hurt_vol_unxf() const noexcept { return HurtVolUnxf(); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& HurtVol() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& HurtVol() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& hurt_vol() noexcept { return HurtVol(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& hurt_vol() const noexcept { return HurtVol(); }
    [[nodiscard]] std::int32_t Scale() const { return read_i32(0x10C); }
    void Scale(std::int32_t value) { write_i32(0x10C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scale() const { return Scale(); }
    void scale(std::int32_t value) { Scale(value); }
    [[nodiscard]] std::uint16_t Health() const { return read_u16(0x110); }
    void Health(std::uint16_t value) { write_u16(0x110, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health() const { return Health(); }
    void health(std::uint16_t value) { Health(value); }
    [[nodiscard]] std::uint16_t HealthMax() const { return read_u16(0x112); }
    void HealthMax(std::uint16_t value) { write_u16(0x112, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health_max() const { return HealthMax(); }
    void health_max(std::uint16_t value) { HealthMax(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x114); }
    void NodeRef(std::uint32_t value) { write_pointer(0x114, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] std::uint32_t Subroutine() const { return read_pointer(0x164); }
    void Subroutine(std::uint32_t value) { write_pointer(0x164, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t subroutine() const { return Subroutine(); }
    void subroutine(std::uint32_t value) { Subroutine(value); }
    [[nodiscard]] std::uint16_t Unused168() const { return read_u16(0x168); }
    void Unused168(std::uint16_t value) { write_u16(0x168, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t unused168() const { return Unused168(); }
    void unused168(std::uint16_t value) { Unused168(value); }
    [[nodiscard]] std::uint16_t Padding16A() const { return read_u16(0x16A); }
    void Padding16A(std::uint16_t value) { write_u16(0x16A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding16_a() const { return Padding16A(); }
    void padding16_a(std::uint16_t value) { Padding16A(value); }
    [[nodiscard]] std::int32_t Unused16C() const { return read_i32(0x16C); }
    void Unused16C(std::int32_t value) { write_i32(0x16C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused16_c() const { return Unused16C(); }
    void unused16_c(std::int32_t value) { Unused16C(value); }

private:
    static constexpr std::size_t _off1 = 0x18;
    static constexpr std::size_t _off2 = 0x1A;
    static constexpr std::size_t _off3 = 0x1B;
    static constexpr std::size_t _off4 = 0x1C;
    static constexpr std::size_t _off5 = 0x1D;
    static constexpr std::size_t _off6 = 0x1E;
    static constexpr std::size_t _off7 = 0x1F;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x24;
    static constexpr std::size_t _off10 = 0x28;
    static constexpr std::size_t _off11 = 0x34;
    static constexpr std::size_t _off12 = 0x40;
    static constexpr std::size_t _off13 = 0x4C;
    static constexpr std::size_t _off14 = 0x58;
    static constexpr std::size_t _off15 = 0x64;
    static constexpr std::size_t _off16 = 0x77;
    static constexpr std::size_t _off17 = 0x7C;
    static constexpr std::size_t _off18 = 0x88;
    static constexpr std::size_t _off19 = 0x8C;
    static constexpr std::size_t _off20 = 0xCC;
    static constexpr std::size_t _off21 = 0x10C;
    static constexpr std::size_t _off22 = 0x110;
    static constexpr std::size_t _off23 = 0x112;
    static constexpr std::size_t _off24 = 0x114;
    static constexpr std::size_t _off25 = 0x118;
    static constexpr std::size_t _off26 = 0x160;
    static constexpr std::size_t _off27 = 0x164;
    static constexpr std::size_t _off28 = 0x168;
    static constexpr std::size_t _off29 = 0x16A;
    static constexpr std::size_t _off30 = 0x16C;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> hurt_vol_unxf_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> hurt_vol_;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CEnemy24 : public CEnemyBase {
public:
    CEnemy24(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy24(Buffer& buffer, std::uint32_t address);
    ~CEnemy24() override;

    [[nodiscard]] std::uint32_t RegenMdl() const { return read_pointer(0x170); }
    void RegenMdl(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t regen_mdl() const { return RegenMdl(); }
    void regen_mdl(std::uint32_t value) { RegenMdl(value); }
    [[nodiscard]] std::uint32_t RegenAnim() const { return read_pointer(0x174); }
    void RegenAnim(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t regen_anim() const { return RegenAnim(); }
    void regen_anim(std::uint32_t value) { RegenAnim(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Regen() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Regen() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& regen() noexcept { return Regen(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& regen() const noexcept { return Regen(); }
    [[nodiscard]] std::uint32_t Colors() const { return read_pointer(0x1C0); }
    void Colors(std::uint32_t value) { write_pointer(0x1C0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t colors() const { return Colors(); }
    void colors(std::uint32_t value) { Colors(value); }
    [[nodiscard]] std::uint32_t Head() const { return read_pointer(0x1C4); }
    void Head(std::uint32_t value) { write_pointer(0x1C4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t head() const { return Head(); }
    void head(std::uint32_t value) { Head(value); }
    [[nodiscard]] IntPtrArray& Arms() noexcept;
    [[nodiscard]] const IntPtrArray& Arms() const noexcept;
    [[nodiscard]] IntPtrArray& arms() noexcept { return Arms(); }
    [[nodiscard]] const IntPtrArray& arms() const noexcept { return Arms(); }
    [[nodiscard]] IntPtrArray& Legs() noexcept;
    [[nodiscard]] const IntPtrArray& Legs() const noexcept;
    [[nodiscard]] IntPtrArray& legs() noexcept { return Legs(); }
    [[nodiscard]] const IntPtrArray& legs() const noexcept { return Legs(); }
    [[nodiscard]] std::uint32_t GoreaB() const { return read_pointer(0x1DC); }
    void GoreaB(std::uint32_t value) { write_pointer(0x1DC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_b() const { return GoreaB(); }
    void gorea_b(std::uint32_t value) { GoreaB(value); }
    [[nodiscard]] std::uint32_t SpineNode() const { return read_pointer(0x1E0); }
    void SpineNode(std::uint32_t value) { write_pointer(0x1E0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t spine_node() const { return SpineNode(); }
    void spine_node(std::uint32_t value) { SpineNode(value); }
    [[nodiscard]] std::int32_t Field1E4() const { return read_i32(0x1E4); }
    void Field1E4(std::int32_t value) { write_i32(0x1E4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_e4() const { return Field1E4(); }
    void field1_e4(std::int32_t value) { Field1E4(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::int32_t SpeedFactor() const { return read_i32(0x228); }
    void SpeedFactor(std::int32_t value) { write_i32(0x228, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t speed_factor() const { return SpeedFactor(); }
    void speed_factor(std::int32_t value) { SpeedFactor(value); }
    [[nodiscard]] std::uint8_t ArmBits() const { return read_u8(0x22C); }
    void ArmBits(std::uint8_t value) { write_u8(0x22C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t arm_bits() const { return ArmBits(); }
    void arm_bits(std::uint8_t value) { ArmBits(value); }
    [[nodiscard]] std::uint8_t WeaponId() const { return read_u8(0x22D); }
    void WeaponId(std::uint8_t value) { write_u8(0x22D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t weapon_id() const { return WeaponId(); }
    void weapon_id(std::uint8_t value) { WeaponId(value); }
    [[nodiscard]] std::uint16_t Unused22E() const { return read_u16(0x22E); }
    void Unused22E(std::uint16_t value) { write_u16(0x22E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t unused22_e() const { return Unused22E(); }
    void unused22_e(std::uint16_t value) { Unused22E(value); }
    [[nodiscard]] formats::Vector3 TargetFacing() const { return read_vec3(0x230); }
    void TargetFacing(formats::Vector3 value) { write_vec3(0x230, value); }
    [[nodiscard]] formats::Vector3 target_facing() const { return TargetFacing(); }
    void target_facing(formats::Vector3 value) { TargetFacing(value); }
    [[nodiscard]] std::uint16_t Field23C() const { return read_u16(0x23C); }
    void Field23C(std::uint16_t value) { write_u16(0x23C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field23_c() const { return Field23C(); }
    void field23_c(std::uint16_t value) { Field23C(value); }
    [[nodiscard]] std::uint16_t Field23E() const { return read_u16(0x23E); }
    void Field23E(std::uint16_t value) { write_u16(0x23E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field23_e() const { return Field23E(); }
    void field23_e(std::uint16_t value) { Field23E(value); }
    [[nodiscard]] std::uint16_t Field240() const { return read_u16(0x240); }
    void Field240(std::uint16_t value) { write_u16(0x240, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field240() const { return Field240(); }
    void field240(std::uint16_t value) { Field240(value); }
    [[nodiscard]] std::uint16_t Field242() const { return read_u16(0x242); }
    void Field242(std::uint16_t value) { write_u16(0x242, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field242() const { return Field242(); }
    void field242(std::uint16_t value) { Field242(value); }
    [[nodiscard]] std::uint16_t Field244() const { return read_u16(0x244); }
    void Field244(std::uint16_t value) { write_u16(0x244, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field244() const { return Field244(); }
    void field244(std::uint16_t value) { Field244(value); }
    [[nodiscard]] std::uint16_t Field246() const { return read_u16(0x246); }
    void Field246(std::uint16_t value) { write_u16(0x246, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field246() const { return Field246(); }
    void field246(std::uint16_t value) { Field246(value); }
    [[nodiscard]] std::int32_t GoreaFlags() const { return read_i32(0x248); }
    void GoreaFlags(std::int32_t value) { write_i32(0x248, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t gorea_flags() const { return GoreaFlags(); }
    void gorea_flags(std::int32_t value) { GoreaFlags(value); }
    [[nodiscard]] std::uint8_t NextState() const { return read_u8(0x24C); }
    void NextState(std::uint8_t value) { write_u8(0x24C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t next_state() const { return NextState(); }
    void next_state(std::uint8_t value) { NextState(value); }
    [[nodiscard]] std::uint8_t Padding24D() const { return read_u8(0x24D); }
    void Padding24D(std::uint8_t value) { write_u8(0x24D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding24_d() const { return Padding24D(); }
    void padding24_d(std::uint8_t value) { Padding24D(value); }
    [[nodiscard]] std::uint16_t Padding24E() const { return read_u16(0x24E); }
    void Padding24E(std::uint16_t value) { write_u16(0x24E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding24_e() const { return Padding24E(); }
    void padding24_e(std::uint16_t value) { Padding24E(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x1C0;
    static constexpr std::size_t _off4 = 0x1C4;
    static constexpr std::size_t _off5 = 0x1C8;
    static constexpr std::size_t _off6 = 0x1D0;
    static constexpr std::size_t _off7 = 0x1DC;
    static constexpr std::size_t _off8 = 0x1E0;
    static constexpr std::size_t _off9 = 0x1E4;
    static constexpr std::size_t _off10 = 0x1E8;
    static constexpr std::size_t _off11 = 0x228;
    static constexpr std::size_t _off12 = 0x22C;
    static constexpr std::size_t _off13 = 0x22D;
    static constexpr std::size_t _off14 = 0x22E;
    static constexpr std::size_t _off15 = 0x230;
    static constexpr std::size_t _off16 = 0x23C;
    static constexpr std::size_t _off17 = 0x23E;
    static constexpr std::size_t _off18 = 0x240;
    static constexpr std::size_t _off19 = 0x242;
    static constexpr std::size_t _off20 = 0x244;
    static constexpr std::size_t _off21 = 0x246;
    static constexpr std::size_t _off22 = 0x248;
    static constexpr std::size_t _off23 = 0x24C;
    static constexpr std::size_t _off24 = 0x24D;
    static constexpr std::size_t _off25 = 0x24E;
    std::unique_ptr<::fruityprime::memory::CModel> regen_;
    std::unique_ptr<IntPtrArray> arms_;
    std::unique_ptr<IntPtrArray> legs_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
};

class CEnemy25 : public CEnemyBase {
public:
    CEnemy25(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy25(Buffer& buffer, std::uint32_t address);
    ~CEnemy25() override;

    [[nodiscard]] std::uint32_t AttachNode() const { return read_pointer(0x170); }
    void AttachNode(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t attach_node() const { return AttachNode(); }
    void attach_node(std::uint32_t value) { AttachNode(value); }
    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x174); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] std::uint32_t FlashEffect() const { return read_pointer(0x178); }
    void FlashEffect(std::uint32_t value) { write_pointer(0x178, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t flash_effect() const { return FlashEffect(); }
    void flash_effect(std::uint32_t value) { FlashEffect(value); }
    [[nodiscard]] std::uint16_t Damage() const { return read_u16(0x17C); }
    void Damage(std::uint16_t value) { write_u16(0x17C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t damage() const { return Damage(); }
    void damage(std::uint16_t value) { Damage(value); }
    [[nodiscard]] std::uint16_t Padding17E() const { return read_u16(0x17E); }
    void Padding17E(std::uint16_t value) { write_u16(0x17E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding17_e() const { return Padding17E(); }
    void padding17_e(std::uint16_t value) { Padding17E(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x17C;
    static constexpr std::size_t _off4 = 0x17E;
};

class CEnemy26 : public CEnemyBase {
public:
    CEnemy26(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy26(Buffer& buffer, std::uint32_t address);
    ~CEnemy26() override;

    [[nodiscard]] std::uint32_t ShoulderNode() const { return read_pointer(0x170); }
    void ShoulderNode(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shoulder_node() const { return ShoulderNode(); }
    void shoulder_node(std::uint32_t value) { ShoulderNode(value); }
    [[nodiscard]] std::uint32_t UpperArmNode() const { return read_pointer(0x174); }
    void UpperArmNode(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t upper_arm_node() const { return UpperArmNode(); }
    void upper_arm_node(std::uint32_t value) { UpperArmNode(value); }
    [[nodiscard]] std::uint32_t ElbowNode() const { return read_pointer(0x178); }
    void ElbowNode(std::uint32_t value) { write_pointer(0x178, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t elbow_node() const { return ElbowNode(); }
    void elbow_node(std::uint32_t value) { ElbowNode(value); }
    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x17C); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x17C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] std::uint32_t ShotEffect() const { return read_pointer(0x180); }
    void ShotEffect(std::uint32_t value) { write_pointer(0x180, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shot_effect() const { return ShotEffect(); }
    void shot_effect(std::uint32_t value) { ShotEffect(value); }
    [[nodiscard]] std::uint32_t DmgEffect() const { return read_pointer(0x184); }
    void DmgEffect(std::uint32_t value) { write_pointer(0x184, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t dmg_effect() const { return DmgEffect(); }
    void dmg_effect(std::uint32_t value) { DmgEffect(value); }
    [[nodiscard]] ::fruityprime::memory::EquipInfo& EquipInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::EquipInfo& EquipInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::EquipInfo& equip_info() noexcept { return EquipInfo(); }
    [[nodiscard]] const ::fruityprime::memory::EquipInfo& equip_info() const noexcept { return EquipInfo(); }
    [[nodiscard]] std::uint16_t RegenTimer() const { return read_u16(0x19C); }
    void RegenTimer(std::uint16_t value) { write_u16(0x19C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t regen_timer() const { return RegenTimer(); }
    void regen_timer(std::uint16_t value) { RegenTimer(value); }
    [[nodiscard]] std::uint16_t ColorInc() const { return read_u16(0x19E); }
    void ColorInc(std::uint16_t value) { write_u16(0x19E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t color_inc() const { return ColorInc(); }
    void color_inc(std::uint16_t value) { ColorInc(value); }
    [[nodiscard]] std::uint16_t Ammo() const { return read_u16(0x1A0); }
    void Ammo(std::uint16_t value) { write_u16(0x1A0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t ammo() const { return Ammo(); }
    void ammo(std::uint16_t value) { Ammo(value); }
    [[nodiscard]] std::uint16_t Cooldown() const { return read_u16(0x1A2); }
    void Cooldown(std::uint16_t value) { write_u16(0x1A2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown() const { return Cooldown(); }
    void cooldown(std::uint16_t value) { Cooldown(value); }
    [[nodiscard]] std::uint16_t DamageTo() const { return read_u16(0x1A4); }
    void DamageTo(std::uint16_t value) { write_u16(0x1A4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t damage_to() const { return DamageTo(); }
    void damage_to(std::uint16_t value) { DamageTo(value); }
    [[nodiscard]] std::uint8_t Index() const { return read_u8(0x1A6); }
    void Index(std::uint8_t value) { write_u8(0x1A6, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t index() const { return Index(); }
    void index(std::uint8_t value) { Index(value); }
    [[nodiscard]] std::uint8_t ArmFlags() const { return read_u8(0x1A7); }
    void ArmFlags(std::uint8_t value) { write_u8(0x1A7, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t arm_flags() const { return ArmFlags(); }
    void arm_flags(std::uint8_t value) { ArmFlags(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x17C;
    static constexpr std::size_t _off4 = 0x180;
    static constexpr std::size_t _off5 = 0x184;
    static constexpr std::size_t _off6 = 0x188;
    static constexpr std::size_t _off7 = 0x19C;
    static constexpr std::size_t _off8 = 0x19E;
    static constexpr std::size_t _off9 = 0x1A0;
    static constexpr std::size_t _off10 = 0x1A2;
    static constexpr std::size_t _off11 = 0x1A4;
    static constexpr std::size_t _off12 = 0x1A6;
    static constexpr std::size_t _off13 = 0x1A7;
    std::unique_ptr<::fruityprime::memory::EquipInfo> equip_info_;
};

class CEnemy27 : public CEnemyBase {
public:
    CEnemy27(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy27(Buffer& buffer, std::uint32_t address);
    ~CEnemy27() override;

    [[nodiscard]] std::uint32_t KneeNode() const { return read_pointer(0x170); }
    void KneeNode(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t knee_node() const { return KneeNode(); }
    void knee_node(std::uint32_t value) { KneeNode(value); }
    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x174); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] std::uint16_t Unused178() const { return read_u16(0x178); }
    void Unused178(std::uint16_t value) { write_u16(0x178, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t unused178() const { return Unused178(); }
    void unused178(std::uint16_t value) { Unused178(value); }
    [[nodiscard]] std::uint8_t Index() const { return read_u8(0x17A); }
    void Index(std::uint8_t value) { write_u8(0x17A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t index() const { return Index(); }
    void index(std::uint8_t value) { Index(value); }
    [[nodiscard]] std::uint8_t Padding17B() const { return read_u8(0x17B); }
    void Padding17B(std::uint8_t value) { write_u8(0x17B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding17_b() const { return Padding17B(); }
    void padding17_b(std::uint8_t value) { Padding17B(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x17A;
    static constexpr std::size_t _off4 = 0x17B;
};

class CEnemy28 : public CEnemyBase {
public:
    CEnemy28(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy28(Buffer& buffer, std::uint32_t address);
    ~CEnemy28() override;

    [[nodiscard]] std::uint32_t SpineNode() const { return read_pointer(0x170); }
    void SpineNode(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t spine_node() const { return SpineNode(); }
    void spine_node(std::uint32_t value) { SpineNode(value); }
    [[nodiscard]] std::uint32_t SealSphere() const { return read_pointer(0x174); }
    void SealSphere(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t seal_sphere() const { return SealSphere(); }
    void seal_sphere(std::uint32_t value) { SealSphere(value); }
    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x178); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x178, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] formats::Vector3 TargetFacing() const { return read_vec3(0x1BC); }
    void TargetFacing(formats::Vector3 value) { write_vec3(0x1BC, value); }
    [[nodiscard]] formats::Vector3 target_facing() const { return TargetFacing(); }
    void target_facing(formats::Vector3 value) { TargetFacing(value); }
    [[nodiscard]] std::uint16_t Field1C8() const { return read_u16(0x1C8); }
    void Field1C8(std::uint16_t value) { write_u16(0x1C8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_c8() const { return Field1C8(); }
    void field1_c8(std::uint16_t value) { Field1C8(value); }
    [[nodiscard]] std::uint16_t Field1CA() const { return read_u16(0x1CA); }
    void Field1CA(std::uint16_t value) { write_u16(0x1CA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_c_a() const { return Field1CA(); }
    void field1_c_a(std::uint16_t value) { Field1CA(value); }
    [[nodiscard]] std::uint16_t Field1CC() const { return read_u16(0x1CC); }
    void Field1CC(std::uint16_t value) { write_u16(0x1CC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_c_c() const { return Field1CC(); }
    void field1_c_c(std::uint16_t value) { Field1CC(value); }
    [[nodiscard]] std::uint8_t PhasesLeft() const { return read_u8(0x1CE); }
    void PhasesLeft(std::uint8_t value) { write_u8(0x1CE, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t phases_left() const { return PhasesLeft(); }
    void phases_left(std::uint8_t value) { PhasesLeft(value); }
    [[nodiscard]] std::uint8_t GoreaFlags() const { return read_u8(0x1CF); }
    void GoreaFlags(std::uint8_t value) { write_u8(0x1CF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t gorea_flags() const { return GoreaFlags(); }
    void gorea_flags(std::uint8_t value) { GoreaFlags(value); }
    [[nodiscard]] IntPtrArray& Trocras() noexcept;
    [[nodiscard]] const IntPtrArray& Trocras() const noexcept;
    [[nodiscard]] IntPtrArray& trocras() noexcept { return Trocras(); }
    [[nodiscard]] const IntPtrArray& trocras() const noexcept { return Trocras(); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x17C;
    static constexpr std::size_t _off4 = 0x1BC;
    static constexpr std::size_t _off5 = 0x1C8;
    static constexpr std::size_t _off6 = 0x1CA;
    static constexpr std::size_t _off7 = 0x1CC;
    static constexpr std::size_t _off8 = 0x1CE;
    static constexpr std::size_t _off9 = 0x1CF;
    static constexpr std::size_t _off10 = 0x1D0;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
    std::unique_ptr<IntPtrArray> trocras_;
};

class Enemy29Fields : public MemoryClass {
public:
    Enemy29Fields(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    Enemy29Fields(Buffer& buffer, std::uint32_t address);
    ~Enemy29Fields() override;

    [[nodiscard]] std::uint32_t VecsReference() const { return read_pointer(0x0); }
    void VecsReference(std::uint32_t value) { write_pointer(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t vecs_reference() const { return VecsReference(); }
    void vecs_reference(std::uint32_t value) { VecsReference(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::VecFx32>& Vecs() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::VecFx32>& Vecs() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::VecFx32>& vecs() noexcept { return Vecs(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::VecFx32>& vecs() const noexcept { return Vecs(); }
    [[nodiscard]] std::uint32_t MtxsReference() const { return read_pointer(0x4); }
    void MtxsReference(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t mtxs_reference() const { return MtxsReference(); }
    void mtxs_reference(std::uint32_t value) { MtxsReference(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::MtxFx43>& Mtxs() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::MtxFx43>& Mtxs() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::MtxFx43>& mtxs() noexcept { return Mtxs(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::MtxFx43>& mtxs() const noexcept { return Mtxs(); }
    [[nodiscard]] std::uint32_t IntsReference() const { return read_pointer(0x8); }
    void IntsReference(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ints_reference() const { return IntsReference(); }
    void ints_reference(std::uint32_t value) { IntsReference(value); }
    [[nodiscard]] Int32Array& Ints() noexcept;
    [[nodiscard]] const Int32Array& Ints() const noexcept;
    [[nodiscard]] Int32Array& ints() noexcept { return Ints(); }
    [[nodiscard]] const Int32Array& ints() const noexcept { return Ints(); }
    [[nodiscard]] std::uint32_t ShortsReference() const { return read_pointer(0xC); }
    void ShortsReference(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shorts_reference() const { return ShortsReference(); }
    void shorts_reference(std::uint32_t value) { ShortsReference(value); }
    [[nodiscard]] Int16Array& Shorts() noexcept;
    [[nodiscard]] const Int16Array& Shorts() const noexcept;
    [[nodiscard]] Int16Array& shorts() noexcept { return Shorts(); }
    [[nodiscard]] const Int16Array& shorts() const noexcept { return Shorts(); }
    [[nodiscard]] formats::Vector3 Field10() const { return read_vec3(0x10); }
    void Field10(formats::Vector3 value) { write_vec3(0x10, value); }
    [[nodiscard]] formats::Vector3 field10() const { return Field10(); }
    void field10(formats::Vector3 value) { Field10(value); }
    [[nodiscard]] std::int32_t Count1() const { return read_i32(0x1C); }
    void Count1(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t count1() const { return Count1(); }
    void count1(std::int32_t value) { Count1(value); }
    [[nodiscard]] std::int32_t Count2() const { return read_i32(0x20); }
    void Count2(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t count2() const { return Count2(); }
    void count2(std::int32_t value) { Count2(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Unused2C() const { return read_i32(0x2C); }
    void Unused2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused2_c() const { return Unused2C(); }
    void unused2_c(std::int32_t value) { Unused2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Unused3C() const { return read_i32(0x3C); }
    void Unused3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused3_c() const { return Unused3C(); }
    void unused3_c(std::int32_t value) { Unused3C(value); }
    [[nodiscard]] std::int32_t Unused40() const { return read_i32(0x40); }
    void Unused40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused40() const { return Unused40(); }
    void unused40(std::int32_t value) { Unused40(value); }
    [[nodiscard]] std::int32_t Unused44() const { return read_i32(0x44); }
    void Unused44(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused44() const { return Unused44(); }
    void unused44(std::int32_t value) { Unused44(value); }
    [[nodiscard]] std::int32_t Unused48() const { return read_i32(0x48); }
    void Unused48(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused48() const { return Unused48(); }
    void unused48(std::int32_t value) { Unused48(value); }
    [[nodiscard]] std::int32_t Unused4C() const { return read_i32(0x4C); }
    void Unused4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused4_c() const { return Unused4C(); }
    void unused4_c(std::int32_t value) { Unused4C(value); }
    [[nodiscard]] std::int32_t Unused50() const { return read_i32(0x50); }
    void Unused50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused50() const { return Unused50(); }
    void unused50(std::int32_t value) { Unused50(value); }
    [[nodiscard]] std::int32_t Unused54() const { return read_i32(0x54); }
    void Unused54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused54() const { return Unused54(); }
    void unused54(std::int32_t value) { Unused54(value); }
    [[nodiscard]] std::int32_t Unused58() const { return read_i32(0x58); }
    void Unused58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused58() const { return Unused58(); }
    void unused58(std::int32_t value) { Unused58(value); }
    [[nodiscard]] std::int32_t Unused5C() const { return read_i32(0x5C); }
    void Unused5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused5_c() const { return Unused5C(); }
    void unused5_c(std::int32_t value) { Unused5C(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x1C;
    static constexpr std::size_t _off6 = 0x20;
    static constexpr std::size_t _off7 = 0x24;
    static constexpr std::size_t _off8 = 0x28;
    static constexpr std::size_t _off9 = 0x2C;
    static constexpr std::size_t _off10 = 0x30;
    static constexpr std::size_t _off11 = 0x34;
    static constexpr std::size_t _off12 = 0x38;
    static constexpr std::size_t _off13 = 0x3C;
    static constexpr std::size_t _off14 = 0x40;
    static constexpr std::size_t _off15 = 0x44;
    static constexpr std::size_t _off16 = 0x48;
    static constexpr std::size_t _off17 = 0x4C;
    static constexpr std::size_t _off18 = 0x50;
    static constexpr std::size_t _off19 = 0x54;
    static constexpr std::size_t _off20 = 0x58;
    static constexpr std::size_t _off21 = 0x5C;
    std::unique_ptr<StructArray<::fruityprime::memory::VecFx32>> vecs_;
    std::unique_ptr<StructArray<::fruityprime::memory::MtxFx43>> mtxs_;
    std::unique_ptr<Int32Array> ints_;
    std::unique_ptr<Int16Array> shorts_;
};

class CEnemy29 : public CEnemyBase {
public:
    CEnemy29(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy29(Buffer& buffer, std::uint32_t address);
    ~CEnemy29() override;

    [[nodiscard]] std::uint32_t MindTrickMdl() const { return read_pointer(0x170); }
    void MindTrickMdl(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t mind_trick_mdl() const { return MindTrickMdl(); }
    void mind_trick_mdl(std::uint32_t value) { MindTrickMdl(value); }
    [[nodiscard]] std::uint32_t MindTrickAnim() const { return read_pointer(0x174); }
    void MindTrickAnim(std::uint32_t value) { write_pointer(0x174, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t mind_trick_anim() const { return MindTrickAnim(); }
    void mind_trick_anim(std::uint32_t value) { MindTrickAnim(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& MindTrick() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& MindTrick() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& mind_trick() noexcept { return MindTrick(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& mind_trick() const noexcept { return MindTrick(); }
    [[nodiscard]] std::uint32_t GrappleMdl() const { return read_pointer(0x1C0); }
    void GrappleMdl(std::uint32_t value) { write_pointer(0x1C0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t grapple_mdl() const { return GrappleMdl(); }
    void grapple_mdl(std::uint32_t value) { GrappleMdl(value); }
    [[nodiscard]] std::uint32_t GrappleAnim() const { return read_pointer(0x1C4); }
    void GrappleAnim(std::uint32_t value) { write_pointer(0x1C4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t grapple_anim() const { return GrappleAnim(); }
    void grapple_anim(std::uint32_t value) { GrappleAnim(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Grapple() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Grapple() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& grapple() noexcept { return Grapple(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& grapple() const noexcept { return Grapple(); }
    [[nodiscard]] std::uint32_t AttachNode() const { return read_pointer(0x210); }
    void AttachNode(std::uint32_t value) { write_pointer(0x210, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t attach_node() const { return AttachNode(); }
    void attach_node(std::uint32_t value) { AttachNode(value); }
    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x214); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x214, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] std::uint32_t FieldsReference() const { return read_pointer(0x218); }
    void FieldsReference(std::uint32_t value) { write_pointer(0x218, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t fields_reference() const { return FieldsReference(); }
    void fields_reference(std::uint32_t value) { FieldsReference(value); }
    [[nodiscard]] ::fruityprime::memory::Enemy29Fields& Fields() noexcept;
    [[nodiscard]] const ::fruityprime::memory::Enemy29Fields& Fields() const noexcept;
    [[nodiscard]] ::fruityprime::memory::Enemy29Fields& fields() noexcept { return Fields(); }
    [[nodiscard]] const ::fruityprime::memory::Enemy29Fields& fields() const noexcept { return Fields(); }
    [[nodiscard]] std::uint16_t Field21C() const { return read_u16(0x21C); }
    void Field21C(std::uint16_t value) { write_u16(0x21C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field21_c() const { return Field21C(); }
    void field21_c(std::uint16_t value) { Field21C(value); }
    [[nodiscard]] std::uint16_t Field21E() const { return read_u16(0x21E); }
    void Field21E(std::uint16_t value) { write_u16(0x21E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field21_e() const { return Field21E(); }
    void field21_e(std::uint16_t value) { Field21E(value); }
    [[nodiscard]] std::int32_t Unused220() const { return read_i32(0x220); }
    void Unused220(std::int32_t value) { write_i32(0x220, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused220() const { return Unused220(); }
    void unused220(std::int32_t value) { Unused220(value); }
    [[nodiscard]] std::int32_t Field224() const { return read_i32(0x224); }
    void Field224(std::int32_t value) { write_i32(0x224, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field224() const { return Field224(); }
    void field224(std::int32_t value) { Field224(value); }
    [[nodiscard]] std::uint8_t Grappling() const { return read_u8(0x228); }
    void Grappling(std::uint8_t value) { write_u8(0x228, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t grappling() const { return Grappling(); }
    void grappling(std::uint8_t value) { Grappling(value); }
    [[nodiscard]] std::uint8_t Padding229() const { return read_u8(0x229); }
    void Padding229(std::uint8_t value) { write_u8(0x229, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding229() const { return Padding229(); }
    void padding229(std::uint8_t value) { Padding229(value); }
    [[nodiscard]] std::uint16_t Padding22A() const { return read_u16(0x22A); }
    void Padding22A(std::uint16_t value) { write_u16(0x22A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding22_a() const { return Padding22A(); }
    void padding22_a(std::uint16_t value) { Padding22A(value); }
    [[nodiscard]] formats::ColorRgb Ambient() const { return read_color3(0x22C); }
    void Ambient(formats::ColorRgb value) { write_color3(0x22C, value); }
    [[nodiscard]] formats::ColorRgb ambient() const { return Ambient(); }
    void ambient(formats::ColorRgb value) { Ambient(value); }
    [[nodiscard]] formats::ColorRgb Diffuse() const { return read_color3(0x22F); }
    void Diffuse(formats::ColorRgb value) { write_color3(0x22F, value); }
    [[nodiscard]] formats::ColorRgb diffuse() const { return Diffuse(); }
    void diffuse(formats::ColorRgb value) { Diffuse(value); }
    [[nodiscard]] std::uint16_t DamageTo() const { return read_u16(0x232); }
    void DamageTo(std::uint16_t value) { write_u16(0x232, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t damage_to() const { return DamageTo(); }
    void damage_to(std::uint16_t value) { DamageTo(value); }
    [[nodiscard]] std::uint16_t Field234() const { return read_u16(0x234); }
    void Field234(std::uint16_t value) { write_u16(0x234, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field234() const { return Field234(); }
    void field234(std::uint16_t value) { Field234(value); }
    [[nodiscard]] std::uint16_t DmgTimer() const { return read_u16(0x236); }
    void DmgTimer(std::uint16_t value) { write_u16(0x236, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t dmg_timer() const { return DmgTimer(); }
    void dmg_timer(std::uint16_t value) { DmgTimer(value); }
    [[nodiscard]] std::uint8_t Unused238() const { return read_u8(0x238); }
    void Unused238(std::uint8_t value) { write_u8(0x238, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t unused238() const { return Unused238(); }
    void unused238(std::uint8_t value) { Unused238(value); }
    [[nodiscard]] std::uint8_t Padding239() const { return read_u8(0x239); }
    void Padding239(std::uint8_t value) { write_u8(0x239, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding239() const { return Padding239(); }
    void padding239(std::uint8_t value) { Padding239(value); }
    [[nodiscard]] std::uint16_t Padding23A() const { return read_u16(0x23A); }
    void Padding23A(std::uint16_t value) { write_u16(0x23A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding23_a() const { return Padding23A(); }
    void padding23_a(std::uint16_t value) { Padding23A(value); }
    [[nodiscard]] std::uint32_t GrappleEffect() const { return read_pointer(0x23C); }
    void GrappleEffect(std::uint32_t value) { write_pointer(0x23C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t grapple_effect() const { return GrappleEffect(); }
    void grapple_effect(std::uint32_t value) { GrappleEffect(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x178;
    static constexpr std::size_t _off3 = 0x1C0;
    static constexpr std::size_t _off4 = 0x1C4;
    static constexpr std::size_t _off5 = 0x1C8;
    static constexpr std::size_t _off6 = 0x210;
    static constexpr std::size_t _off7 = 0x214;
    static constexpr std::size_t _off8 = 0x218;
    static constexpr std::size_t _off9 = 0x21C;
    static constexpr std::size_t _off10 = 0x21E;
    static constexpr std::size_t _off11 = 0x220;
    static constexpr std::size_t _off12 = 0x224;
    static constexpr std::size_t _off13 = 0x228;
    static constexpr std::size_t _off14 = 0x229;
    static constexpr std::size_t _off15 = 0x22A;
    static constexpr std::size_t _off16 = 0x22C;
    static constexpr std::size_t _off17 = 0x22F;
    static constexpr std::size_t _off18 = 0x232;
    static constexpr std::size_t _off19 = 0x234;
    static constexpr std::size_t _off20 = 0x236;
    static constexpr std::size_t _off21 = 0x238;
    static constexpr std::size_t _off22 = 0x239;
    static constexpr std::size_t _off23 = 0x23A;
    static constexpr std::size_t _off24 = 0x23C;
    std::unique_ptr<::fruityprime::memory::CModel> mind_trick_;
    std::unique_ptr<::fruityprime::memory::CModel> grapple_;
    std::unique_ptr<::fruityprime::memory::Enemy29Fields> fields_;
};

class CEnemy30 : public CEnemyBase {
public:
    CEnemy30(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemy30(Buffer& buffer, std::uint32_t address);
    ~CEnemy30() override;

    [[nodiscard]] std::uint32_t GoreaOwner() const { return read_pointer(0x170); }
    void GoreaOwner(std::uint32_t value) { write_pointer(0x170, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t gorea_owner() const { return GoreaOwner(); }
    void gorea_owner(std::uint32_t value) { GoreaOwner(value); }
    [[nodiscard]] formats::Vector3 Field174() const { return read_vec3(0x174); }
    void Field174(formats::Vector3 value) { write_vec3(0x174, value); }
    [[nodiscard]] formats::Vector3 field174() const { return Field174(); }
    void field174(formats::Vector3 value) { Field174(value); }
    [[nodiscard]] std::int32_t Index() const { return read_i32(0x180); }
    void Index(std::int32_t value) { write_i32(0x180, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t index() const { return Index(); }
    void index(std::int32_t value) { Index(value); }
    [[nodiscard]] std::uint16_t Field184() const { return read_u16(0x184); }
    void Field184(std::uint16_t value) { write_u16(0x184, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field184() const { return Field184(); }
    void field184(std::uint16_t value) { Field184(value); }
    [[nodiscard]] std::uint16_t TrocraState() const { return read_u16(0x186); }
    void TrocraState(std::uint16_t value) { write_u16(0x186, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t trocra_state() const { return TrocraState(); }
    void trocra_state(std::uint16_t value) { TrocraState(value); }

private:
    static constexpr std::size_t _off0 = 0x170;
    static constexpr std::size_t _off1 = 0x174;
    static constexpr std::size_t _off2 = 0x180;
    static constexpr std::size_t _off3 = 0x184;
    static constexpr std::size_t _off4 = 0x186;
};

class CPlatform : public CEntity {
public:
    CPlatform(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CPlatform(Buffer& buffer, std::uint32_t address);
    ~CPlatform() override;

    [[nodiscard]] std::int32_t NoPort() const { return read_i32(0x18); }
    void NoPort(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t no_port() const { return NoPort(); }
    void no_port(std::int32_t value) { NoPort(value); }
    [[nodiscard]] std::int32_t ModelId() const { return read_i32(0x1C); }
    void ModelId(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t model_id() const { return ModelId(); }
    void model_id(std::int32_t value) { ModelId(value); }
    [[nodiscard]] std::uint32_t ScanEventTarget() const { return read_pointer(0x20); }
    void ScanEventTarget(std::uint32_t value) { write_pointer(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t scan_event_target() const { return ScanEventTarget(); }
    void scan_event_target(std::uint32_t value) { ScanEventTarget(value); }
    [[nodiscard]] std::int32_t MovementType() const { return read_i32(0x24); }
    void MovementType(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t movement_type() const { return MovementType(); }
    void movement_type(std::int32_t value) { MovementType(value); }
    [[nodiscard]] std::int32_t ForCutscene() const { return read_i32(0x28); }
    void ForCutscene(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t for_cutscene() const { return ForCutscene(); }
    void for_cutscene(std::int32_t value) { ForCutscene(value); }
    [[nodiscard]] std::int32_t ReverseType() const { return read_i32(0x2C); }
    void ReverseType(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t reverse_type() const { return ReverseType(); }
    void reverse_type(std::int32_t value) { ReverseType(value); }
    [[nodiscard]] formats::PlatformFlags Flags() const { return static_cast<formats::PlatformFlags>(read_i32(0x30)); }
    void Flags(formats::PlatformFlags value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] formats::PlatformFlags flags() const { return Flags(); }
    void flags(formats::PlatformFlags value) { Flags(value); }
    [[nodiscard]] std::uint16_t CollisionDamage() const { return read_u16(0x34); }
    void CollisionDamage(std::uint16_t value) { write_u16(0x34, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t collision_damage() const { return CollisionDamage(); }
    void collision_damage(std::uint16_t value) { CollisionDamage(value); }
    [[nodiscard]] std::uint16_t Padding36() const { return read_u16(0x36); }
    void Padding36(std::uint16_t value) { write_u16(0x36, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding36() const { return Padding36(); }
    void padding36(std::uint16_t value) { Padding36(value); }
    [[nodiscard]] formats::Vector3 BeamSpawnDir() const { return read_vec3(0x38); }
    void BeamSpawnDir(formats::Vector3 value) { write_vec3(0x38, value); }
    [[nodiscard]] formats::Vector3 beam_spawn_dir() const { return BeamSpawnDir(); }
    void beam_spawn_dir(formats::Vector3 value) { BeamSpawnDir(value); }
    [[nodiscard]] std::int32_t BeamIndex() const { return read_i32(0x44); }
    void BeamIndex(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t beam_index() const { return BeamIndex(); }
    void beam_index(std::int32_t value) { BeamIndex(value); }
    [[nodiscard]] std::uint16_t BeamInterval() const { return read_u16(0x48); }
    void BeamInterval(std::uint16_t value) { write_u16(0x48, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t beam_interval() const { return BeamInterval(); }
    void beam_interval(std::uint16_t value) { BeamInterval(value); }
    [[nodiscard]] std::uint16_t Padding4A() const { return read_u16(0x4A); }
    void Padding4A(std::uint16_t value) { write_u16(0x4A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding4_a() const { return Padding4A(); }
    void padding4_a(std::uint16_t value) { Padding4A(value); }
    [[nodiscard]] std::int32_t BeamOnIntervals() const { return read_i32(0x4C); }
    void BeamOnIntervals(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t beam_on_intervals() const { return BeamOnIntervals(); }
    void beam_on_intervals(std::int32_t value) { BeamOnIntervals(value); }
    [[nodiscard]] std::int32_t ResistEffId() const { return read_i32(0x50); }
    void ResistEffId(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t resist_eff_id() const { return ResistEffId(); }
    void resist_eff_id(std::int32_t value) { ResistEffId(value); }
    [[nodiscard]] std::int32_t Effectiveness() const { return read_i32(0x54); }
    void Effectiveness(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t effectiveness() const { return Effectiveness(); }
    void effectiveness(std::int32_t value) { Effectiveness(value); }
    [[nodiscard]] std::int32_t DamageEffId() const { return read_i32(0x58); }
    void DamageEffId(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t damage_eff_id() const { return DamageEffId(); }
    void damage_eff_id(std::int32_t value) { DamageEffId(value); }
    [[nodiscard]] std::int32_t DeadEffId() const { return read_i32(0x5C); }
    void DeadEffId(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t dead_eff_id() const { return DeadEffId(); }
    void dead_eff_id(std::int32_t value) { DeadEffId(value); }
    [[nodiscard]] std::int32_t Unused60() const { return read_i32(0x60); }
    void Unused60(std::int32_t value) { write_i32(0x60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused60() const { return Unused60(); }
    void unused60(std::int32_t value) { Unused60(value); }
    [[nodiscard]] std::int32_t Unused64() const { return read_i32(0x64); }
    void Unused64(std::int32_t value) { write_i32(0x64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused64() const { return Unused64(); }
    void unused64(std::int32_t value) { Unused64(value); }
    [[nodiscard]] formats::PlatStateFlags StateFlags() const { return static_cast<formats::PlatStateFlags>(read_u32(0x68)); }
    void StateFlags(formats::PlatStateFlags value) { write_u32(0x68, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::PlatStateFlags state_flags() const { return StateFlags(); }
    void state_flags(formats::PlatStateFlags value) { StateFlags(value); }
    [[nodiscard]] std::int32_t CollisionBits() const { return read_i32(0x6C); }
    void CollisionBits(std::int32_t value) { write_i32(0x6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t collision_bits() const { return CollisionBits(); }
    void collision_bits(std::int32_t value) { CollisionBits(value); }
    [[nodiscard]] std::uint16_t TimeSincePlayerCol() const { return read_u16(0x70); }
    void TimeSincePlayerCol(std::uint16_t value) { write_u16(0x70, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t time_since_player_col() const { return TimeSincePlayerCol(); }
    void time_since_player_col(std::uint16_t value) { TimeSincePlayerCol(value); }
    [[nodiscard]] formats::PlatAnimFlags AnimFlags() const { return static_cast<formats::PlatAnimFlags>(read_u16(0x72)); }
    void AnimFlags(formats::PlatAnimFlags value) { write_u16(0x72, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] formats::PlatAnimFlags anim_flags() const { return AnimFlags(); }
    void anim_flags(formats::PlatAnimFlags value) { AnimFlags(value); }
    [[nodiscard]] std::int32_t CurrentAnimId() const { return read_i32(0x74); }
    void CurrentAnimId(std::int32_t value) { write_i32(0x74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t current_anim_id() const { return CurrentAnimId(); }
    void current_anim_id(std::int32_t value) { CurrentAnimId(value); }
    [[nodiscard]] std::int32_t CurrentAnim() const { return read_i32(0x78); }
    void CurrentAnim(std::int32_t value) { write_i32(0x78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t current_anim() const { return CurrentAnim(); }
    void current_anim(std::int32_t value) { CurrentAnim(value); }
    [[nodiscard]] std::uint8_t FromIndex() const { return read_u8(0x7C); }
    void FromIndex(std::uint8_t value) { write_u8(0x7C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t from_index() const { return FromIndex(); }
    void from_index(std::uint8_t value) { FromIndex(value); }
    [[nodiscard]] std::uint8_t ToIndex() const { return read_u8(0x7D); }
    void ToIndex(std::uint8_t value) { write_u8(0x7D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t to_index() const { return ToIndex(); }
    void to_index(std::uint8_t value) { ToIndex(value); }
    [[nodiscard]] std::uint8_t State() const { return read_u8(0x7E); }
    void State(std::uint8_t value) { write_u8(0x7E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t state() const { return State(); }
    void state(std::uint8_t value) { State(value); }
    [[nodiscard]] formats::PlatformState PrevState() const { return static_cast<formats::PlatformState>(read_u8(0x7F)); }
    void PrevState(formats::PlatformState value) { write_u8(0x7F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::PlatformState prev_state() const { return PrevState(); }
    void prev_state(formats::PlatformState value) { PrevState(value); }
    [[nodiscard]] formats::PlatformState PosCount() const { return static_cast<formats::PlatformState>(read_u8(0x80)); }
    void PosCount(formats::PlatformState value) { write_u8(0x80, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::PlatformState pos_count() const { return PosCount(); }
    void pos_count(formats::PlatformState value) { PosCount(value); }
    [[nodiscard]] std::uint8_t Padding81() const { return read_u8(0x81); }
    void Padding81(std::uint8_t value) { write_u8(0x81, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding81() const { return Padding81(); }
    void padding81(std::uint8_t value) { Padding81(value); }
    [[nodiscard]] std::uint16_t MoveTimer() const { return read_u16(0x82); }
    void MoveTimer(std::uint16_t value) { write_u16(0x82, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t move_timer() const { return MoveTimer(); }
    void move_timer(std::uint16_t value) { MoveTimer(value); }
    [[nodiscard]] std::uint16_t RecoilTimer() const { return read_u16(0x84); }
    void RecoilTimer(std::uint16_t value) { write_u16(0x84, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t recoil_timer() const { return RecoilTimer(); }
    void recoil_timer(std::uint16_t value) { RecoilTimer(value); }
    [[nodiscard]] std::uint16_t Health() const { return read_u16(0x86); }
    void Health(std::uint16_t value) { write_u16(0x86, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health() const { return Health(); }
    void health(std::uint16_t value) { Health(value); }
    [[nodiscard]] std::uint16_t HealthMax() const { return read_u16(0x88); }
    void HealthMax(std::uint16_t value) { write_u16(0x88, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health_max() const { return HealthMax(); }
    void health_max(std::uint16_t value) { HealthMax(value); }
    [[nodiscard]] std::uint16_t HalfHealth() const { return read_u16(0x8A); }
    void HalfHealth(std::uint16_t value) { write_u16(0x8A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t half_health() const { return HalfHealth(); }
    void half_health(std::uint16_t value) { HalfHealth(value); }
    [[nodiscard]] std::uint16_t ParentId() const { return read_u16(0x8C); }
    void ParentId(std::uint16_t value) { write_u16(0x8C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t parent_id() const { return ParentId(); }
    void parent_id(std::uint16_t value) { ParentId(value); }
    [[nodiscard]] std::uint16_t Padding8E() const { return read_u16(0x8E); }
    void Padding8E(std::uint16_t value) { write_u16(0x8E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding8_e() const { return Padding8E(); }
    void padding8_e(std::uint16_t value) { Padding8E(value); }
    [[nodiscard]] std::uint32_t Parent() const { return read_pointer(0x90); }
    void Parent(std::uint32_t value) { write_pointer(0x90, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t parent() const { return Parent(); }
    void parent(std::uint32_t value) { Parent(value); }
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& EquipInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& EquipInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& equip_info() noexcept { return EquipInfo(); }
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& equip_info() const noexcept { return EquipInfo(); }
    [[nodiscard]] std::uint16_t BeamAmmo() const { return read_u16(0xA8); }
    void BeamAmmo(std::uint16_t value) { write_u16(0xA8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t beam_ammo() const { return BeamAmmo(); }
    void beam_ammo(std::uint16_t value) { BeamAmmo(value); }
    [[nodiscard]] std::uint16_t PaddingAA() const { return read_u16(0xAA); }
    void PaddingAA(std::uint16_t value) { write_u16(0xAA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding_a_a() const { return PaddingAA(); }
    void padding_a_a(std::uint16_t value) { PaddingAA(value); }
    [[nodiscard]] std::int32_t UnusedAC() const { return read_i32(0xAC); }
    void UnusedAC(std::int32_t value) { write_i32(0xAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused_a_c() const { return UnusedAC(); }
    void unused_a_c(std::int32_t value) { UnusedAC(value); }
    [[nodiscard]] std::int32_t UnusedB0() const { return read_i32(0xB0); }
    void UnusedB0(std::int32_t value) { write_i32(0xB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused_b0() const { return UnusedB0(); }
    void unused_b0(std::int32_t value) { UnusedB0(value); }
    [[nodiscard]] std::int32_t UnusedB4() const { return read_i32(0xB4); }
    void UnusedB4(std::int32_t value) { write_i32(0xB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused_b4() const { return UnusedB4(); }
    void unused_b4(std::int32_t value) { UnusedB4(value); }
    [[nodiscard]] std::uint16_t BeamTimer() const { return read_u16(0xB8); }
    void BeamTimer(std::uint16_t value) { write_u16(0xB8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t beam_timer() const { return BeamTimer(); }
    void beam_timer(std::uint16_t value) { BeamTimer(value); }
    [[nodiscard]] std::uint16_t BeamIntervalIndex() const { return read_u16(0xBA); }
    void BeamIntervalIndex(std::uint16_t value) { write_u16(0xBA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t beam_interval_index() const { return BeamIntervalIndex(); }
    void beam_interval_index(std::uint16_t value) { BeamIntervalIndex(value); }
    [[nodiscard]] std::uint16_t DrawingBeam() const { return read_u16(0xBC); }
    void DrawingBeam(std::uint16_t value) { write_u16(0xBC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t drawing_beam() const { return DrawingBeam(); }
    void drawing_beam(std::uint16_t value) { DrawingBeam(value); }
    [[nodiscard]] std::uint16_t PaddingBE() const { return read_u16(0xBE); }
    void PaddingBE(std::uint16_t value) { write_u16(0xBE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding_b_e() const { return PaddingBE(); }
    void padding_b_e(std::uint16_t value) { PaddingBE(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0xC0); }
    void Data(std::uint32_t value) { write_pointer(0xC0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint32_t Positions() const { return read_pointer(0xC4); }
    void Positions(std::uint32_t value) { write_pointer(0xC4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t positions() const { return Positions(); }
    void positions(std::uint32_t value) { Positions(value); }
    [[nodiscard]] std::uint32_t Rotations() const { return read_pointer(0xC8); }
    void Rotations(std::uint32_t value) { write_pointer(0xC8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t rotations() const { return Rotations(); }
    void rotations(std::uint32_t value) { Rotations(value); }
    [[nodiscard]] formats::Vector3 PosOffset() const { return read_vec3(0xCC); }
    void PosOffset(formats::Vector3 value) { write_vec3(0xCC, value); }
    [[nodiscard]] formats::Vector3 pos_offset() const { return PosOffset(); }
    void pos_offset(formats::Vector3 value) { PosOffset(value); }
    [[nodiscard]] formats::Vector3 VisiblePos() const { return read_vec3(0xD8); }
    void VisiblePos(formats::Vector3 value) { write_vec3(0xD8, value); }
    [[nodiscard]] formats::Vector3 visible_pos() const { return VisiblePos(); }
    void visible_pos(formats::Vector3 value) { VisiblePos(value); }
    [[nodiscard]] formats::Vector3 PrevVisiblePos() const { return read_vec3(0xE4); }
    void PrevVisiblePos(formats::Vector3 value) { write_vec3(0xE4, value); }
    [[nodiscard]] formats::Vector3 prev_visible_pos() const { return PrevVisiblePos(); }
    void prev_visible_pos(formats::Vector3 value) { PrevVisiblePos(value); }
    [[nodiscard]] formats::Vector3 Position() const { return read_vec3(0xF0); }
    void Position(formats::Vector3 value) { write_vec3(0xF0, value); }
    [[nodiscard]] formats::Vector3 position() const { return Position(); }
    void position(formats::Vector3 value) { Position(value); }
    [[nodiscard]] formats::Vector3 PrevPosition() const { return read_vec3(0xFC); }
    void PrevPosition(formats::Vector3 value) { write_vec3(0xFC, value); }
    [[nodiscard]] formats::Vector3 prev_position() const { return PrevPosition(); }
    void prev_position(formats::Vector3 value) { PrevPosition(value); }
    [[nodiscard]] formats::Vector4 CurRotation() const { return read_vec4(0x108); }
    void CurRotation(formats::Vector4 value) { write_vec4(0x108, value); }
    [[nodiscard]] formats::Vector4 cur_rotation() const { return CurRotation(); }
    void cur_rotation(formats::Vector4 value) { CurRotation(value); }
    [[nodiscard]] formats::Vector4 FromRotation() const { return read_vec4(0x118); }
    void FromRotation(formats::Vector4 value) { write_vec4(0x118, value); }
    [[nodiscard]] formats::Vector4 from_rotation() const { return FromRotation(); }
    void from_rotation(formats::Vector4 value) { FromRotation(value); }
    [[nodiscard]] formats::Vector4 ToRotation() const { return read_vec4(0x128); }
    void ToRotation(formats::Vector4 value) { write_vec4(0x128, value); }
    [[nodiscard]] formats::Vector4 to_rotation() const { return ToRotation(); }
    void to_rotation(formats::Vector4 value) { ToRotation(value); }
    [[nodiscard]] std::int32_t MovePct() const { return read_i32(0x138); }
    void MovePct(std::int32_t value) { write_i32(0x138, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t move_pct() const { return MovePct(); }
    void move_pct(std::int32_t value) { MovePct(value); }
    [[nodiscard]] std::int32_t MoveInc() const { return read_i32(0x13C); }
    void MoveInc(std::int32_t value) { write_i32(0x13C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t move_inc() const { return MoveInc(); }
    void move_inc(std::int32_t value) { MoveInc(value); }
    [[nodiscard]] std::int32_t Unused140() const { return read_i32(0x140); }
    void Unused140(std::int32_t value) { write_i32(0x140, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused140() const { return Unused140(); }
    void unused140(std::int32_t value) { Unused140(value); }
    [[nodiscard]] std::int32_t Unused144() const { return read_i32(0x144); }
    void Unused144(std::int32_t value) { write_i32(0x144, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused144() const { return Unused144(); }
    void unused144(std::int32_t value) { Unused144(value); }
    [[nodiscard]] formats::Vector3 ColMin() const { return read_vec3(0x148); }
    void ColMin(formats::Vector3 value) { write_vec3(0x148, value); }
    [[nodiscard]] formats::Vector3 col_min() const { return ColMin(); }
    void col_min(formats::Vector3 value) { ColMin(value); }
    [[nodiscard]] formats::Vector3 ColW() const { return read_vec3(0x154); }
    void ColW(formats::Vector3 value) { write_vec3(0x154, value); }
    [[nodiscard]] formats::Vector3 col_w() const { return ColW(); }
    void col_w(formats::Vector3 value) { ColW(value); }
    [[nodiscard]] formats::Vector3 ColMax() const { return read_vec3(0x160); }
    void ColMax(formats::Vector3 value) { write_vec3(0x160, value); }
    [[nodiscard]] formats::Vector3 col_max() const { return ColMax(); }
    void col_max(formats::Vector3 value) { ColMax(value); }
    [[nodiscard]] formats::Vector3 Velocity() const { return read_vec3(0x16C); }
    void Velocity(formats::Vector3 value) { write_vec3(0x16C, value); }
    [[nodiscard]] formats::Vector3 velocity() const { return Velocity(); }
    void velocity(formats::Vector3 value) { Velocity(value); }
    [[nodiscard]] std::int32_t Unused178() const { return read_i32(0x178); }
    void Unused178(std::int32_t value) { write_i32(0x178, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused178() const { return Unused178(); }
    void unused178(std::int32_t value) { Unused178(value); }
    [[nodiscard]] std::int32_t Unused17C() const { return read_i32(0x17C); }
    void Unused17C(std::int32_t value) { write_i32(0x17C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused17_c() const { return Unused17C(); }
    void unused17_c(std::int32_t value) { Unused17C(value); }
    [[nodiscard]] std::int32_t Unused180() const { return read_i32(0x180); }
    void Unused180(std::int32_t value) { write_i32(0x180, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused180() const { return Unused180(); }
    void unused180(std::int32_t value) { Unused180(value); }
    [[nodiscard]] std::int32_t ForwardSpeed() const { return read_i32(0x184); }
    void ForwardSpeed(std::int32_t value) { write_i32(0x184, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t forward_speed() const { return ForwardSpeed(); }
    void forward_speed(std::int32_t value) { ForwardSpeed(value); }
    [[nodiscard]] std::int32_t BackwardSpeed() const { return read_i32(0x188); }
    void BackwardSpeed(std::int32_t value) { write_i32(0x188, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t backward_speed() const { return BackwardSpeed(); }
    void backward_speed(std::int32_t value) { BackwardSpeed(value); }
    [[nodiscard]] std::int32_t SfxVolume() const { return read_i32(0x18C); }
    void SfxVolume(std::int32_t value) { write_i32(0x18C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t sfx_volume() const { return SfxVolume(); }
    void sfx_volume(std::int32_t value) { SfxVolume(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x190); }
    void Vec2(formats::Vector3 value) { write_vec3(0x190, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x19C); }
    void Vec1(formats::Vector3 value) { write_vec3(0x19C, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] ::fruityprime::memory::EntityCollision& EntityCollision() noexcept;
    [[nodiscard]] const ::fruityprime::memory::EntityCollision& EntityCollision() const noexcept;
    [[nodiscard]] ::fruityprime::memory::EntityCollision& entity_collision() noexcept { return EntityCollision(); }
    [[nodiscard]] const ::fruityprime::memory::EntityCollision& entity_collision() const noexcept { return EntityCollision(); }
    [[nodiscard]] std::uint32_t MtxObj() const { return read_pointer(0x25C); }
    void MtxObj(std::uint32_t value) { write_pointer(0x25C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t mtx_obj() const { return MtxObj(); }
    void mtx_obj(std::uint32_t value) { MtxObj(value); }
    [[nodiscard]] std::uint16_t AttachNodeIndex() const { return read_u16(0x260); }
    void AttachNodeIndex(std::uint16_t value) { write_u16(0x260, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t attach_node_index() const { return AttachNodeIndex(); }
    void attach_node_index(std::uint16_t value) { AttachNodeIndex(value); }
    [[nodiscard]] UInt16Array& Turrets() noexcept;
    [[nodiscard]] const UInt16Array& Turrets() const noexcept;
    [[nodiscard]] UInt16Array& turrets() noexcept { return Turrets(); }
    [[nodiscard]] const UInt16Array& turrets() const noexcept { return Turrets(); }
    [[nodiscard]] std::uint16_t Padding26A() const { return read_u16(0x26A); }
    void Padding26A(std::uint16_t value) { write_u16(0x26A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding26_a() const { return Padding26A(); }
    void padding26_a(std::uint16_t value) { Padding26A(value); }
    [[nodiscard]] IntPtrArray& Effects() noexcept;
    [[nodiscard]] const IntPtrArray& Effects() const noexcept;
    [[nodiscard]] IntPtrArray& effects() noexcept { return Effects(); }
    [[nodiscard]] const IntPtrArray& effects() const noexcept { return Effects(); }
    [[nodiscard]] std::int32_t Unused27C() const { return read_i32(0x27C); }
    void Unused27C(std::int32_t value) { write_i32(0x27C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused27_c() const { return Unused27C(); }
    void unused27_c(std::int32_t value) { Unused27C(value); }
    [[nodiscard]] std::int32_t Unused280() const { return read_i32(0x280); }
    void Unused280(std::int32_t value) { write_i32(0x280, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused280() const { return Unused280(); }
    void unused280(std::int32_t value) { Unused280(value); }
    [[nodiscard]] std::int32_t Unused284() const { return read_i32(0x284); }
    void Unused284(std::int32_t value) { write_i32(0x284, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused284() const { return Unused284(); }
    void unused284(std::int32_t value) { Unused284(value); }
    [[nodiscard]] std::int32_t Unused288() const { return read_i32(0x288); }
    void Unused288(std::int32_t value) { write_i32(0x288, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused288() const { return Unused288(); }
    void unused288(std::int32_t value) { Unused288(value); }
    [[nodiscard]] std::int32_t Unused28C() const { return read_i32(0x28C); }
    void Unused28C(std::int32_t value) { write_i32(0x28C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused28_c() const { return Unused28C(); }
    void unused28_c(std::int32_t value) { Unused28C(value); }
    [[nodiscard]] std::int32_t Unused290() const { return read_i32(0x290); }
    void Unused290(std::int32_t value) { write_i32(0x290, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused290() const { return Unused290(); }
    void unused290(std::int32_t value) { Unused290(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x294); }
    void NodeRef(std::uint32_t value) { write_pointer(0x294, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t Port() const { return read_pointer(0x2E0); }
    void Port(std::uint32_t value) { write_pointer(0x2E0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t port() const { return Port(); }
    void port(std::uint32_t value) { Port(value); }
    [[nodiscard]] std::uint32_t HitEventTarget() const { return read_pointer(0x2E4); }
    void HitEventTarget(std::uint32_t value) { write_pointer(0x2E4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t hit_event_target() const { return HitEventTarget(); }
    void hit_event_target(std::uint32_t value) { HitEventTarget(value); }
    [[nodiscard]] std::uint32_t PlayerColEventTarget() const { return read_pointer(0x2E8); }
    void PlayerColEventTarget(std::uint32_t value) { write_pointer(0x2E8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t player_col_event_target() const { return PlayerColEventTarget(); }
    void player_col_event_target(std::uint32_t value) { PlayerColEventTarget(value); }
    [[nodiscard]] std::uint32_t DeadEventTarget() const { return read_pointer(0x2EC); }
    void DeadEventTarget(std::uint32_t value) { write_pointer(0x2EC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t dead_event_target() const { return DeadEventTarget(); }
    void dead_event_target(std::uint32_t value) { DeadEventTarget(value); }
    [[nodiscard]] ByteArray& LifetimeEventIndices() noexcept;
    [[nodiscard]] const ByteArray& LifetimeEventIndices() const noexcept;
    [[nodiscard]] ByteArray& lifetime_event_indices() noexcept { return LifetimeEventIndices(); }
    [[nodiscard]] const ByteArray& lifetime_event_indices() const noexcept { return LifetimeEventIndices(); }
    [[nodiscard]] IntPtrArray& LifetimeEventTargets() noexcept;
    [[nodiscard]] const IntPtrArray& LifetimeEventTargets() const noexcept;
    [[nodiscard]] IntPtrArray& lifetime_event_targets() noexcept { return LifetimeEventTargets(); }
    [[nodiscard]] const IntPtrArray& lifetime_event_targets() const noexcept { return LifetimeEventTargets(); }
    [[nodiscard]] U32EnumArray<formats::Message>& LifetimeEventIds() noexcept;
    [[nodiscard]] const U32EnumArray<formats::Message>& LifetimeEventIds() const noexcept;
    [[nodiscard]] U32EnumArray<formats::Message>& lifetime_event_ids() noexcept { return LifetimeEventIds(); }
    [[nodiscard]] const U32EnumArray<formats::Message>& lifetime_event_ids() const noexcept { return LifetimeEventIds(); }
    [[nodiscard]] Int32Array& LifetimeEventParam1s() noexcept;
    [[nodiscard]] const Int32Array& LifetimeEventParam1s() const noexcept;
    [[nodiscard]] Int32Array& lifetime_event_param1s() noexcept { return LifetimeEventParam1s(); }
    [[nodiscard]] const Int32Array& lifetime_event_param1s() const noexcept { return LifetimeEventParam1s(); }
    [[nodiscard]] Int32Array& LifetimeEventParam2s() noexcept;
    [[nodiscard]] const Int32Array& LifetimeEventParam2s() const noexcept;
    [[nodiscard]] Int32Array& lifetime_event_param2s() noexcept { return LifetimeEventParam2s(); }
    [[nodiscard]] const Int32Array& lifetime_event_param2s() const noexcept { return LifetimeEventParam2s(); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1C;
    static constexpr std::size_t _off2 = 0x20;
    static constexpr std::size_t _off3 = 0x24;
    static constexpr std::size_t _off4 = 0x28;
    static constexpr std::size_t _off5 = 0x2C;
    static constexpr std::size_t _off6 = 0x30;
    static constexpr std::size_t _off7 = 0x34;
    static constexpr std::size_t _off8 = 0x36;
    static constexpr std::size_t _off9 = 0x38;
    static constexpr std::size_t _off10 = 0x44;
    static constexpr std::size_t _off11 = 0x48;
    static constexpr std::size_t _off12 = 0x4A;
    static constexpr std::size_t _off13 = 0x4C;
    static constexpr std::size_t _off14 = 0x50;
    static constexpr std::size_t _off15 = 0x54;
    static constexpr std::size_t _off16 = 0x58;
    static constexpr std::size_t _off17 = 0x5C;
    static constexpr std::size_t _off18 = 0x60;
    static constexpr std::size_t _off19 = 0x64;
    static constexpr std::size_t _off20 = 0x68;
    static constexpr std::size_t _off21 = 0x6C;
    static constexpr std::size_t _off22 = 0x70;
    static constexpr std::size_t _off23 = 0x72;
    static constexpr std::size_t _off24 = 0x74;
    static constexpr std::size_t _off25 = 0x78;
    static constexpr std::size_t _off26 = 0x7C;
    static constexpr std::size_t _off27 = 0x7D;
    static constexpr std::size_t _off28 = 0x7E;
    static constexpr std::size_t _off29 = 0x7F;
    static constexpr std::size_t _off30 = 0x80;
    static constexpr std::size_t _off31 = 0x81;
    static constexpr std::size_t _off32 = 0x82;
    static constexpr std::size_t _off33 = 0x84;
    static constexpr std::size_t _off34 = 0x86;
    static constexpr std::size_t _off35 = 0x88;
    static constexpr std::size_t _off36 = 0x8A;
    static constexpr std::size_t _off37 = 0x8C;
    static constexpr std::size_t _off38 = 0x8E;
    static constexpr std::size_t _off39 = 0x90;
    static constexpr std::size_t _off40 = 0x94;
    static constexpr std::size_t _off41 = 0xA8;
    static constexpr std::size_t _off42 = 0xAA;
    static constexpr std::size_t _off43 = 0xAC;
    static constexpr std::size_t _off44 = 0xB0;
    static constexpr std::size_t _off45 = 0xB4;
    static constexpr std::size_t _off46 = 0xB8;
    static constexpr std::size_t _off47 = 0xBA;
    static constexpr std::size_t _off48 = 0xBC;
    static constexpr std::size_t _off49 = 0xBE;
    static constexpr std::size_t _off50 = 0xC0;
    static constexpr std::size_t _off51 = 0xC4;
    static constexpr std::size_t _off52 = 0xC8;
    static constexpr std::size_t _off53 = 0xCC;
    static constexpr std::size_t _off54 = 0xD8;
    static constexpr std::size_t _off55 = 0xE4;
    static constexpr std::size_t _off56 = 0xF0;
    static constexpr std::size_t _off57 = 0xFC;
    static constexpr std::size_t _off58 = 0x108;
    static constexpr std::size_t _off59 = 0x118;
    static constexpr std::size_t _off60 = 0x128;
    static constexpr std::size_t _off61 = 0x138;
    static constexpr std::size_t _off62 = 0x13C;
    static constexpr std::size_t _off63 = 0x140;
    static constexpr std::size_t _off64 = 0x144;
    static constexpr std::size_t _off65 = 0x148;
    static constexpr std::size_t _off66 = 0x154;
    static constexpr std::size_t _off67 = 0x160;
    static constexpr std::size_t _off68 = 0x16C;
    static constexpr std::size_t _off69 = 0x178;
    static constexpr std::size_t _off70 = 0x17C;
    static constexpr std::size_t _off71 = 0x180;
    static constexpr std::size_t _off72 = 0x184;
    static constexpr std::size_t _off73 = 0x188;
    static constexpr std::size_t _off74 = 0x18C;
    static constexpr std::size_t _off75 = 0x190;
    static constexpr std::size_t _off76 = 0x19C;
    static constexpr std::size_t _off77 = 0x1A8;
    static constexpr std::size_t _off78 = 0x25C;
    static constexpr std::size_t _off79 = 0x260;
    static constexpr std::size_t _off80 = 0x262;
    static constexpr std::size_t _off81 = 0x26A;
    static constexpr std::size_t _off82 = 0x26C;
    static constexpr std::size_t _off83 = 0x27C;
    static constexpr std::size_t _off84 = 0x280;
    static constexpr std::size_t _off85 = 0x284;
    static constexpr std::size_t _off86 = 0x288;
    static constexpr std::size_t _off87 = 0x28C;
    static constexpr std::size_t _off88 = 0x290;
    static constexpr std::size_t _off89 = 0x294;
    static constexpr std::size_t _off90 = 0x298;
    static constexpr std::size_t _off91 = 0x2E0;
    static constexpr std::size_t _off92 = 0x2E4;
    static constexpr std::size_t _off93 = 0x2E8;
    static constexpr std::size_t _off94 = 0x2EC;
    static constexpr std::size_t _off95 = 0x2F0;
    static constexpr std::size_t _off99 = 0x2F4;
    static constexpr std::size_t _off100 = 0x304;
    static constexpr std::size_t _off101 = 0x314;
    static constexpr std::size_t _off102 = 0x324;
    static constexpr std::size_t _off103 = 0x334;
    std::unique_ptr<::fruityprime::memory::EquipInfoPtr> equip_info_;
    std::unique_ptr<::fruityprime::memory::EntityCollision> entity_collision_;
    std::unique_ptr<UInt16Array> turrets_;
    std::unique_ptr<IntPtrArray> effects_;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<ByteArray> lifetime_event_indices_;
    std::unique_ptr<IntPtrArray> lifetime_event_targets_;
    std::unique_ptr<U32EnumArray<formats::Message>> lifetime_event_ids_;
    std::unique_ptr<Int32Array> lifetime_event_param1s_;
    std::unique_ptr<Int32Array> lifetime_event_param2s_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CObject : public CEntity {
public:
    CObject(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CObject(Buffer& buffer, std::uint32_t address);
    ~CObject() override;

    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x18); }
    void Flags(std::uint8_t value) { write_u8(0x18, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Field19() const { return read_u8(0x19); }
    void Field19(std::uint8_t value) { write_u8(0x19, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field19() const { return Field19(); }
    void field19(std::uint8_t value) { Field19(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] std::int32_t EffectFlags() const { return read_i32(0x1C); }
    void EffectFlags(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t effect_flags() const { return EffectFlags(); }
    void effect_flags(std::int32_t value) { EffectFlags(value); }
    [[nodiscard]] std::uint16_t LinkedEntity() const { return read_u16(0x20); }
    void LinkedEntity(std::uint16_t value) { write_u16(0x20, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t linked_entity() const { return LinkedEntity(); }
    void linked_entity(std::uint16_t value) { LinkedEntity(value); }
    [[nodiscard]] std::uint16_t Field22() const { return read_u16(0x22); }
    void Field22(std::uint16_t value) { write_u16(0x22, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field22() const { return Field22(); }
    void field22(std::uint16_t value) { Field22(value); }
    [[nodiscard]] formats::Vector3 TempPos() const { return read_vec3(0x24); }
    void TempPos(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 temp_pos() const { return TempPos(); }
    void temp_pos(formats::Vector3 value) { TempPos(value); }
    [[nodiscard]] formats::Vector3 TempVec2() const { return read_vec3(0x30); }
    void TempVec2(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 temp_vec2() const { return TempVec2(); }
    void temp_vec2(formats::Vector3 value) { TempVec2(value); }
    [[nodiscard]] formats::Vector3 TempVec1() const { return read_vec3(0x3C); }
    void TempVec1(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 temp_vec1() const { return TempVec1(); }
    void temp_vec1(formats::Vector3 value) { TempVec1(value); }
    [[nodiscard]] std::uint16_t AttachNodeIndex() const { return read_u16(0x48); }
    void AttachNodeIndex(std::uint16_t value) { write_u16(0x48, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t attach_node_index() const { return AttachNodeIndex(); }
    void attach_node_index(std::uint16_t value) { AttachNodeIndex(value); }
    [[nodiscard]] std::uint16_t Field4A() const { return read_u16(0x4A); }
    void Field4A(std::uint16_t value) { write_u16(0x4A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_a() const { return Field4A(); }
    void field4_a(std::uint16_t value) { Field4A(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x4C); }
    void Pos(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x58); }
    void Vec2(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x64); }
    void Vec1(formats::Vector3 value) { write_vec3(0x64, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 SomePos() const { return read_vec3(0x70); }
    void SomePos(formats::Vector3 value) { write_vec3(0x70, value); }
    [[nodiscard]] formats::Vector3 some_pos() const { return SomePos(); }
    void some_pos(formats::Vector3 value) { SomePos(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x7C); }
    void Data(std::uint32_t value) { write_pointer(0x7C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint32_t ScanEventTarget() const { return read_pointer(0x80); }
    void ScanEventTarget(std::uint32_t value) { write_pointer(0x80, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t scan_event_target() const { return ScanEventTarget(); }
    void scan_event_target(std::uint32_t value) { ScanEventTarget(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x84); }
    void NodeRef(std::uint32_t value) { write_pointer(0x84, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::EntityCollision>& ColStructs() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::EntityCollision>& ColStructs() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::EntityCollision>& col_structs() noexcept { return ColStructs(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::EntityCollision>& col_structs() const noexcept { return ColStructs(); }
    [[nodiscard]] IntPtrArray& MtxObjs() noexcept;
    [[nodiscard]] const IntPtrArray& MtxObjs() const noexcept;
    [[nodiscard]] IntPtrArray& mtx_objs() noexcept { return MtxObjs(); }
    [[nodiscard]] const IntPtrArray& mtx_objs() const noexcept { return MtxObjs(); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::int32_t EffectProcessing() const { return read_i32(0x240); }
    void EffectProcessing(std::int32_t value) { write_i32(0x240, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t effect_processing() const { return EffectProcessing(); }
    void effect_processing(std::int32_t value) { EffectProcessing(value); }
    [[nodiscard]] std::int32_t EffectId() const { return read_i32(0x244); }
    void EffectId(std::int32_t value) { write_i32(0x244, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t effect_id() const { return EffectId(); }
    void effect_id(std::int32_t value) { EffectId(value); }
    [[nodiscard]] std::uint16_t EffectInterval() const { return read_u16(0x248); }
    void EffectInterval(std::uint16_t value) { write_u16(0x248, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t effect_interval() const { return EffectInterval(); }
    void effect_interval(std::uint16_t value) { EffectInterval(value); }
    [[nodiscard]] std::uint16_t EffectActive() const { return read_u16(0x24A); }
    void EffectActive(std::uint16_t value) { write_u16(0x24A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t effect_active() const { return EffectActive(); }
    void effect_active(std::uint16_t value) { EffectActive(value); }
    [[nodiscard]] std::int32_t EffectOnIntervals() const { return read_i32(0x24C); }
    void EffectOnIntervals(std::int32_t value) { write_i32(0x24C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t effect_on_intervals() const { return EffectOnIntervals(); }
    void effect_on_intervals(std::int32_t value) { EffectOnIntervals(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::uint32_t Effect() const { return read_pointer(0x290); }
    void Effect(std::uint32_t value) { write_pointer(0x290, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect() const { return Effect(); }
    void effect(std::uint32_t value) { Effect(value); }
    [[nodiscard]] std::uint16_t EffectTimer() const { return read_u16(0x294); }
    void EffectTimer(std::uint16_t value) { write_u16(0x294, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t effect_timer() const { return EffectTimer(); }
    void effect_timer(std::uint16_t value) { EffectTimer(value); }
    [[nodiscard]] std::uint16_t EffectIntervalIndex() const { return read_u16(0x296); }
    void EffectIntervalIndex(std::uint16_t value) { write_u16(0x296, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t effect_interval_index() const { return EffectIntervalIndex(); }
    void effect_interval_index(std::uint16_t value) { EffectIntervalIndex(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& Sfx() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& Sfx() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx() noexcept { return Sfx(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx() const noexcept { return Sfx(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x19;
    static constexpr std::size_t _off2 = 0x1A;
    static constexpr std::size_t _off3 = 0x1C;
    static constexpr std::size_t _off4 = 0x20;
    static constexpr std::size_t _off5 = 0x22;
    static constexpr std::size_t _off6 = 0x24;
    static constexpr std::size_t _off7 = 0x30;
    static constexpr std::size_t _off8 = 0x3C;
    static constexpr std::size_t _off9 = 0x48;
    static constexpr std::size_t _off10 = 0x4A;
    static constexpr std::size_t _off11 = 0x4C;
    static constexpr std::size_t _off12 = 0x58;
    static constexpr std::size_t _off13 = 0x64;
    static constexpr std::size_t _off14 = 0x70;
    static constexpr std::size_t _off15 = 0x7C;
    static constexpr std::size_t _off16 = 0x80;
    static constexpr std::size_t _off17 = 0x84;
    static constexpr std::size_t _off18 = 0x88;
    static constexpr std::size_t _off19 = 0x1F0;
    static constexpr std::size_t _off20 = 0x1F8;
    static constexpr std::size_t _off21 = 0x240;
    static constexpr std::size_t _off22 = 0x244;
    static constexpr std::size_t _off23 = 0x248;
    static constexpr std::size_t _off24 = 0x24A;
    static constexpr std::size_t _off25 = 0x24C;
    static constexpr std::size_t _off26 = 0x250;
    static constexpr std::size_t _off27 = 0x290;
    static constexpr std::size_t _off28 = 0x294;
    static constexpr std::size_t _off29 = 0x296;
    static constexpr std::size_t _off30 = 0x298;
    std::unique_ptr<StructArray<::fruityprime::memory::EntityCollision>> col_structs_;
    std::unique_ptr<IntPtrArray> mtx_objs_;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_;
};

class CPlayerSpawn : public CEntity {
public:
    CPlayerSpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CPlayerSpawn(Buffer& buffer, std::uint32_t address);
    ~CPlayerSpawn() override;

    [[nodiscard]] std::uint16_t Cooldown() const { return read_u16(0x18); }
    void Cooldown(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown() const { return Cooldown(); }
    void cooldown(std::uint16_t value) { Cooldown(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x1C); }
    void Vec1(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x28); }
    void Vec2(formats::Vector3 value) { write_vec3(0x28, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x34); }
    void Pos(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::uint8_t Initial() const { return read_u8(0x40); }
    void Initial(std::uint8_t value) { write_u8(0x40, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t initial() const { return Initial(); }
    void initial(std::uint8_t value) { Initial(value); }
    [[nodiscard]] std::uint8_t Active() const { return read_u8(0x41); }
    void Active(std::uint8_t value) { write_u8(0x41, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t active() const { return Active(); }
    void active(std::uint8_t value) { Active(value); }
    [[nodiscard]] std::uint8_t TeamIndex() const { return read_u8(0x42); }
    void TeamIndex(std::uint8_t value) { write_u8(0x42, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t team_index() const { return TeamIndex(); }
    void team_index(std::uint8_t value) { TeamIndex(value); }
    [[nodiscard]] std::uint8_t Field43() const { return read_u8(0x43); }
    void Field43(std::uint8_t value) { write_u8(0x43, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field43() const { return Field43(); }
    void field43(std::uint8_t value) { Field43(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x44); }
    void NodeRef(std::uint32_t value) { write_pointer(0x44, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x28;
    static constexpr std::size_t _off4 = 0x34;
    static constexpr std::size_t _off5 = 0x40;
    static constexpr std::size_t _off6 = 0x41;
    static constexpr std::size_t _off7 = 0x42;
    static constexpr std::size_t _off8 = 0x43;
    static constexpr std::size_t _off9 = 0x44;
};

class CDoor : public CEntity {
public:
    CDoor(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CDoor(Buffer& buffer, std::uint32_t address);
    ~CDoor() override;

    [[nodiscard]] std::uint16_t Flags() const { return read_u16(0x18); }
    void Flags(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t flags() const { return Flags(); }
    void flags(std::uint16_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t DoorPaletteId() const { return read_u8(0x1A); }
    void DoorPaletteId(std::uint8_t value) { write_u8(0x1A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t door_palette_id() const { return DoorPaletteId(); }
    void door_palette_id(std::uint8_t value) { DoorPaletteId(value); }
    [[nodiscard]] std::uint8_t Field1B() const { return read_u8(0x1B); }
    void Field1B(std::uint8_t value) { write_u8(0x1B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field1_b() const { return Field1B(); }
    void field1_b(std::uint8_t value) { Field1B(value); }
    [[nodiscard]] std::uint8_t Field1C() const { return read_u8(0x1C); }
    void Field1C(std::uint8_t value) { write_u8(0x1C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field1_c() const { return Field1C(); }
    void field1_c(std::uint8_t value) { Field1C(value); }
    [[nodiscard]] std::uint8_t Field1D() const { return read_u8(0x1D); }
    void Field1D(std::uint8_t value) { write_u8(0x1D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field1_d() const { return Field1D(); }
    void field1_d(std::uint8_t value) { Field1D(value); }
    [[nodiscard]] std::uint16_t Field1E() const { return read_u16(0x1E); }
    void Field1E(std::uint16_t value) { write_u16(0x1E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_e() const { return Field1E(); }
    void field1_e(std::uint16_t value) { Field1E(value); }
    [[nodiscard]] std::int32_t SomeRoomId() const { return read_i32(0x20); }
    void SomeRoomId(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t some_room_id() const { return SomeRoomId(); }
    void some_room_id(std::int32_t value) { SomeRoomId(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x30); }
    void Vec2(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x3C); }
    void Pos(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 LockPos() const { return read_vec3(0x48); }
    void LockPos(formats::Vector3 value) { write_vec3(0x48, value); }
    [[nodiscard]] formats::Vector3 lock_pos() const { return LockPos(); }
    void lock_pos(formats::Vector3 value) { LockPos(value); }
    [[nodiscard]] std::int32_t BoundingRadius() const { return read_i32(0x54); }
    void BoundingRadius(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t bounding_radius() const { return BoundingRadius(); }
    void bounding_radius(std::int32_t value) { BoundingRadius(value); }
    [[nodiscard]] std::int32_t BoundingRadiusSquared() const { return read_i32(0x58); }
    void BoundingRadiusSquared(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t bounding_radius_squared() const { return BoundingRadiusSquared(); }
    void bounding_radius_squared(std::int32_t value) { BoundingRadiusSquared(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& DoorModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& DoorModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& door_model() noexcept { return DoorModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& door_model() const noexcept { return DoorModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& LockModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& LockModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& lock_model() noexcept { return LockModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& lock_model() const noexcept { return LockModel(); }
    [[nodiscard]] formats::DoorType DoorType() const { return static_cast<formats::DoorType>(read_u32(0xEC)); }
    void DoorType(formats::DoorType value) { write_u32(0xEC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::DoorType door_type() const { return DoorType(); }
    void door_type(formats::DoorType value) { DoorType(value); }
    [[nodiscard]] std::int32_t TargetRoom() const { return read_i32(0xF0); }
    void TargetRoom(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t target_room() const { return TargetRoom(); }
    void target_room(std::int32_t value) { TargetRoom(value); }
    [[nodiscard]] std::uint32_t Port() const { return read_pointer(0xF4); }
    void Port(std::uint32_t value) { write_pointer(0xF4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t port() const { return Port(); }
    void port(std::uint32_t value) { Port(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0xF8); }
    void NodeRef(std::uint32_t value) { write_pointer(0xF8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::uint32_t DoorNodeRef() const { return read_pointer(0xFC); }
    void DoorNodeRef(std::uint32_t value) { write_pointer(0xFC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t door_node_ref() const { return DoorNodeRef(); }
    void door_node_ref(std::uint32_t value) { DoorNodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x104); }
    void Data(std::uint32_t value) { write_pointer(0x104, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1B;
    static constexpr std::size_t _off3 = 0x1C;
    static constexpr std::size_t _off4 = 0x1D;
    static constexpr std::size_t _off5 = 0x1E;
    static constexpr std::size_t _off6 = 0x20;
    static constexpr std::size_t _off7 = 0x24;
    static constexpr std::size_t _off8 = 0x30;
    static constexpr std::size_t _off9 = 0x3C;
    static constexpr std::size_t _off10 = 0x48;
    static constexpr std::size_t _off11 = 0x54;
    static constexpr std::size_t _off12 = 0x58;
    static constexpr std::size_t _off13 = 0x5C;
    static constexpr std::size_t _off14 = 0xA4;
    static constexpr std::size_t _off15 = 0xEC;
    static constexpr std::size_t _off16 = 0xF0;
    static constexpr std::size_t _off17 = 0xF4;
    static constexpr std::size_t _off18 = 0xF8;
    static constexpr std::size_t _off19 = 0xFC;
    static constexpr std::size_t _off20 = 0x100;
    static constexpr std::size_t _off21 = 0x104;
    std::unique_ptr<::fruityprime::memory::CModel> door_model_;
    std::unique_ptr<::fruityprime::memory::CModel> lock_model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CItemSpawn : public CEntity {
public:
    CItemSpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CItemSpawn(Buffer& buffer, std::uint32_t address);
    ~CItemSpawn() override;

    [[nodiscard]] std::uint16_t ItemEntityId() const { return read_u16(0x18); }
    void ItemEntityId(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t item_entity_id() const { return ItemEntityId(); }
    void item_entity_id(std::uint16_t value) { ItemEntityId(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] formats::Vector3 Field1C() const { return read_vec3(0x1C); }
    void Field1C(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 field1_c() const { return Field1C(); }
    void field1_c(formats::Vector3 value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x40); }
    void Pos(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x4C); }
    void Data(std::uint32_t value) { write_pointer(0x4C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x50); }
    void Flags(std::uint8_t value) { write_u8(0x50, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Field51() const { return read_u8(0x51); }
    void Field51(std::uint8_t value) { write_u8(0x51, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field51() const { return Field51(); }
    void field51(std::uint8_t value) { Field51(value); }
    [[nodiscard]] std::uint16_t Field52() const { return read_u16(0x52); }
    void Field52(std::uint16_t value) { write_u16(0x52, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field52() const { return Field52(); }
    void field52(std::uint16_t value) { Field52(value); }
    [[nodiscard]] formats::ItemType Type() const { return static_cast<formats::ItemType>(read_u16(0x54)); }
    void Type(formats::ItemType value) { write_u16(0x54, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] formats::ItemType type() const { return Type(); }
    void type(formats::ItemType value) { Type(value); }
    [[nodiscard]] std::uint16_t HasBase() const { return read_u16(0x56); }
    void HasBase(std::uint16_t value) { write_u16(0x56, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t has_base() const { return HasBase(); }
    void has_base(std::uint16_t value) { HasBase(value); }
    [[nodiscard]] std::uint16_t MaxSpawnCount() const { return read_u16(0x58); }
    void MaxSpawnCount(std::uint16_t value) { write_u16(0x58, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t max_spawn_count() const { return MaxSpawnCount(); }
    void max_spawn_count(std::uint16_t value) { MaxSpawnCount(value); }
    [[nodiscard]] std::uint16_t SpawnInterval() const { return read_u16(0x5A); }
    void SpawnInterval(std::uint16_t value) { write_u16(0x5A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t spawn_interval() const { return SpawnInterval(); }
    void spawn_interval(std::uint16_t value) { SpawnInterval(value); }
    [[nodiscard]] std::uint16_t SpawnDelay() const { return read_u16(0x5C); }
    void SpawnDelay(std::uint16_t value) { write_u16(0x5C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t spawn_delay() const { return SpawnDelay(); }
    void spawn_delay(std::uint16_t value) { SpawnDelay(value); }
    [[nodiscard]] std::uint16_t SpawnCount() const { return read_u16(0x5E); }
    void SpawnCount(std::uint16_t value) { write_u16(0x5E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t spawn_count() const { return SpawnCount(); }
    void spawn_count(std::uint16_t value) { SpawnCount(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x60); }
    void NodeRef(std::uint32_t value) { write_pointer(0x60, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& BaseModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& BaseModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& base_model() noexcept { return BaseModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& base_model() const noexcept { return BaseModel(); }
    [[nodiscard]] std::uint32_t ItemInstance() const { return read_pointer(0xAC); }
    void ItemInstance(std::uint32_t value) { write_pointer(0xAC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t item_instance() const { return ItemInstance(); }
    void item_instance(std::uint32_t value) { ItemInstance(value); }
    [[nodiscard]] std::uint32_t SomeEntity() const { return read_pointer(0xB0); }
    void SomeEntity(std::uint32_t value) { write_pointer(0xB0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t some_entity() const { return SomeEntity(); }
    void some_entity(std::uint32_t value) { SomeEntity(value); }
    [[nodiscard]] std::int32_t FieldB4() const { return read_i32(0xB4); }
    void FieldB4(std::int32_t value) { write_i32(0xB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b4() const { return FieldB4(); }
    void field_b4(std::int32_t value) { FieldB4(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x28;
    static constexpr std::size_t _off4 = 0x2C;
    static constexpr std::size_t _off5 = 0x30;
    static constexpr std::size_t _off6 = 0x34;
    static constexpr std::size_t _off7 = 0x38;
    static constexpr std::size_t _off8 = 0x3C;
    static constexpr std::size_t _off9 = 0x40;
    static constexpr std::size_t _off10 = 0x4C;
    static constexpr std::size_t _off11 = 0x50;
    static constexpr std::size_t _off12 = 0x51;
    static constexpr std::size_t _off13 = 0x52;
    static constexpr std::size_t _off14 = 0x54;
    static constexpr std::size_t _off15 = 0x56;
    static constexpr std::size_t _off16 = 0x58;
    static constexpr std::size_t _off17 = 0x5A;
    static constexpr std::size_t _off18 = 0x5C;
    static constexpr std::size_t _off19 = 0x5E;
    static constexpr std::size_t _off20 = 0x60;
    static constexpr std::size_t _off21 = 0x64;
    static constexpr std::size_t _off22 = 0xAC;
    static constexpr std::size_t _off23 = 0xB0;
    static constexpr std::size_t _off24 = 0xB4;
    std::unique_ptr<::fruityprime::memory::CModel> base_model_;
};

class CItemInstance : public CEntity {
public:
    CItemInstance(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CItemInstance(Buffer& buffer, std::uint32_t address);
    ~CItemInstance() override;

    [[nodiscard]] std::uint16_t ParentId() const { return read_u16(0x18); }
    void ParentId(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t parent_id() const { return ParentId(); }
    void parent_id(std::uint16_t value) { ParentId(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] formats::Vector3 Field1C() const { return read_vec3(0x1C); }
    void Field1C(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 field1_c() const { return Field1C(); }
    void field1_c(formats::Vector3 value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x40); }
    void Pos(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::ItemType Type() const { return static_cast<formats::ItemType>(read_u16(0x4C)); }
    void Type(formats::ItemType value) { write_u16(0x4C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] formats::ItemType type() const { return Type(); }
    void type(formats::ItemType value) { Type(value); }
    [[nodiscard]] std::int16_t DespawnTimer() const { return read_i16(0x4E); }
    void DespawnTimer(std::int16_t value) { write_i16(0x4E, static_cast<std::int16_t>(value)); }
    [[nodiscard]] std::int16_t despawn_timer() const { return DespawnTimer(); }
    void despawn_timer(std::int16_t value) { DespawnTimer(value); }
    [[nodiscard]] std::uint16_t RotationAngle() const { return read_u16(0x50); }
    void RotationAngle(std::uint16_t value) { write_u16(0x50, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t rotation_angle() const { return RotationAngle(); }
    void rotation_angle(std::uint16_t value) { RotationAngle(value); }
    [[nodiscard]] std::uint16_t LinkDone() const { return read_u16(0x52); }
    void LinkDone(std::uint16_t value) { write_u16(0x52, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t link_done() const { return LinkDone(); }
    void link_done(std::uint16_t value) { LinkDone(value); }
    [[nodiscard]] std::uint32_t Effect() const { return read_pointer(0x54); }
    void Effect(std::uint32_t value) { write_pointer(0x54, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect() const { return Effect(); }
    void effect(std::uint32_t value) { Effect(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0xA4); }
    void NodeRef(std::uint32_t value) { write_pointer(0xA4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::uint32_t ItemBase() const { return read_pointer(0xA8); }
    void ItemBase(std::uint32_t value) { write_pointer(0xA8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t item_base() const { return ItemBase(); }
    void item_base(std::uint32_t value) { ItemBase(value); }
    [[nodiscard]] std::int32_t FieldAC() const { return read_i32(0xAC); }
    void FieldAC(std::int32_t value) { write_i32(0xAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c() const { return FieldAC(); }
    void field_a_c(std::int32_t value) { FieldAC(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x28;
    static constexpr std::size_t _off4 = 0x2C;
    static constexpr std::size_t _off5 = 0x30;
    static constexpr std::size_t _off6 = 0x34;
    static constexpr std::size_t _off7 = 0x38;
    static constexpr std::size_t _off8 = 0x3C;
    static constexpr std::size_t _off9 = 0x40;
    static constexpr std::size_t _off10 = 0x4C;
    static constexpr std::size_t _off11 = 0x4E;
    static constexpr std::size_t _off12 = 0x50;
    static constexpr std::size_t _off13 = 0x52;
    static constexpr std::size_t _off14 = 0x54;
    static constexpr std::size_t _off15 = 0x58;
    static constexpr std::size_t _off16 = 0xA0;
    static constexpr std::size_t _off17 = 0xA4;
    static constexpr std::size_t _off18 = 0xA8;
    static constexpr std::size_t _off19 = 0xAC;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CEnemySpawn : public CEntity {
public:
    CEnemySpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CEnemySpawn(Buffer& buffer, std::uint32_t address);
    ~CEnemySpawn() override;

    [[nodiscard]] formats::EnemyType EnemyType() const { return static_cast<formats::EnemyType>(read_u8(0x18)); }
    void EnemyType(formats::EnemyType value) { write_u8(0x18, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::EnemyType enemy_type() const { return EnemyType(); }
    void enemy_type(formats::EnemyType value) { EnemyType(value); }
    [[nodiscard]] std::uint8_t Padding19() const { return read_u8(0x19); }
    void Padding19(std::uint8_t value) { write_u8(0x19, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding19() const { return Padding19(); }
    void padding19(std::uint8_t value) { Padding19(value); }
    [[nodiscard]] std::uint16_t Padding1A() const { return read_u16(0x1A); }
    void Padding1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding1_a() const { return Padding1A(); }
    void padding1_a(std::uint16_t value) { Padding1A(value); }
    [[nodiscard]] std::uint32_t RoomNodeRef() const { return read_pointer(0x1C); }
    void RoomNodeRef(std::uint32_t value) { write_pointer(0x1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t room_node_ref() const { return RoomNodeRef(); }
    void room_node_ref(std::uint32_t value) { RoomNodeRef(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x20); }
    void NodeRef(std::uint32_t value) { write_pointer(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::uint16_t LinkedEntity() const { return read_u16(0x24); }
    void LinkedEntity(std::uint16_t value) { write_u16(0x24, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t linked_entity() const { return LinkedEntity(); }
    void linked_entity(std::uint16_t value) { LinkedEntity(value); }
    [[nodiscard]] std::uint16_t Padding26() const { return read_u16(0x26); }
    void Padding26(std::uint16_t value) { write_u16(0x26, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding26() const { return Padding26(); }
    void padding26(std::uint16_t value) { Padding26(value); }
    [[nodiscard]] formats::Vector3 LinkInvPos() const { return read_vec3(0x28); }
    void LinkInvPos(formats::Vector3 value) { write_vec3(0x28, value); }
    [[nodiscard]] formats::Vector3 link_inv_pos() const { return LinkInvPos(); }
    void link_inv_pos(formats::Vector3 value) { LinkInvPos(value); }
    [[nodiscard]] formats::Vector3 LinkInvVec2() const { return read_vec3(0x34); }
    void LinkInvVec2(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 link_inv_vec2() const { return LinkInvVec2(); }
    void link_inv_vec2(formats::Vector3 value) { LinkInvVec2(value); }
    [[nodiscard]] formats::Vector3 LinkInvVec1() const { return read_vec3(0x40); }
    void LinkInvVec1(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 link_inv_vec1() const { return LinkInvVec1(); }
    void link_inv_vec1(formats::Vector3 value) { LinkInvVec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x4C); }
    void Pos(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x58); }
    void Vec2(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x64); }
    void Vec1(formats::Vector3 value) { write_vec3(0x64, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::SpawnerFlags Flags() const { return static_cast<formats::SpawnerFlags>(read_u8(0x70)); }
    void Flags(formats::SpawnerFlags value) { write_u8(0x70, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::SpawnerFlags flags() const { return Flags(); }
    void flags(formats::SpawnerFlags value) { Flags(value); }
    [[nodiscard]] std::uint8_t SpawnedCount() const { return read_u8(0x71); }
    void SpawnedCount(std::uint8_t value) { write_u8(0x71, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t spawned_count() const { return SpawnedCount(); }
    void spawned_count(std::uint8_t value) { SpawnedCount(value); }
    [[nodiscard]] std::uint8_t ActiveCount() const { return read_u8(0x72); }
    void ActiveCount(std::uint8_t value) { write_u8(0x72, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t active_count() const { return ActiveCount(); }
    void active_count(std::uint8_t value) { ActiveCount(value); }
    [[nodiscard]] std::uint8_t Padding73() const { return read_u8(0x73); }
    void Padding73(std::uint8_t value) { write_u8(0x73, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding73() const { return Padding73(); }
    void padding73(std::uint8_t value) { Padding73(value); }
    [[nodiscard]] std::uint16_t CooldownTimer() const { return read_u16(0x74); }
    void CooldownTimer(std::uint16_t value) { write_u16(0x74, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown_timer() const { return CooldownTimer(); }
    void cooldown_timer(std::uint16_t value) { CooldownTimer(value); }
    [[nodiscard]] std::uint16_t Padding76() const { return read_u16(0x76); }
    void Padding76(std::uint16_t value) { write_u16(0x76, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding76() const { return Padding76(); }
    void padding76(std::uint16_t value) { Padding76(value); }
    [[nodiscard]] std::int32_t ActiveDistSqr() const { return read_i32(0x78); }
    void ActiveDistSqr(std::int32_t value) { write_i32(0x78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t active_dist_sqr() const { return ActiveDistSqr(); }
    void active_dist_sqr(std::int32_t value) { ActiveDistSqr(value); }
    [[nodiscard]] std::uint32_t Entity1() const { return read_pointer(0x7C); }
    void Entity1(std::uint32_t value) { write_pointer(0x7C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity1() const { return Entity1(); }
    void entity1(std::uint32_t value) { Entity1(value); }
    [[nodiscard]] std::uint32_t Entity2() const { return read_pointer(0x80); }
    void Entity2(std::uint32_t value) { write_pointer(0x80, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity2() const { return Entity2(); }
    void entity2(std::uint32_t value) { Entity2(value); }
    [[nodiscard]] std::uint32_t Entity3() const { return read_pointer(0x84); }
    void Entity3(std::uint32_t value) { write_pointer(0x84, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity3() const { return Entity3(); }
    void entity3(std::uint32_t value) { Entity3(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x88); }
    void Data(std::uint32_t value) { write_pointer(0x88, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x19;
    static constexpr std::size_t _off2 = 0x1A;
    static constexpr std::size_t _off4 = 0x1C;
    static constexpr std::size_t _off5 = 0x20;
    static constexpr std::size_t _off6 = 0x24;
    static constexpr std::size_t _off7 = 0x26;
    static constexpr std::size_t _off8 = 0x28;
    static constexpr std::size_t _off9 = 0x34;
    static constexpr std::size_t _off10 = 0x40;
    static constexpr std::size_t _off11 = 0x4C;
    static constexpr std::size_t _off12 = 0x58;
    static constexpr std::size_t _off13 = 0x64;
    static constexpr std::size_t _off14 = 0x70;
    static constexpr std::size_t _off15 = 0x71;
    static constexpr std::size_t _off16 = 0x72;
    static constexpr std::size_t _off17 = 0x73;
    static constexpr std::size_t _off18 = 0x74;
    static constexpr std::size_t _off19 = 0x76;
    static constexpr std::size_t _off20 = 0x78;
    static constexpr std::size_t _off21 = 0x7C;
    static constexpr std::size_t _off22 = 0x80;
    static constexpr std::size_t _off23 = 0x84;
    static constexpr std::size_t _off24 = 0x88;
};

class CTriggerVolume : public CEntity {
public:
    CTriggerVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CTriggerVolume(Buffer& buffer, std::uint32_t address);
    ~CTriggerVolume() override;

    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x18); }
    void Flags(std::uint8_t value) { write_u8(0x18, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Type() const { return read_u8(0x19); }
    void Type(std::uint8_t value) { write_u8(0x19, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t type() const { return Type(); }
    void type(std::uint8_t value) { Type(value); }
    [[nodiscard]] std::uint16_t TriggerDelay() const { return read_u16(0x1A); }
    void TriggerDelay(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t trigger_delay() const { return TriggerDelay(); }
    void trigger_delay(std::uint16_t value) { TriggerDelay(value); }
    [[nodiscard]] std::uint16_t RequiredStateBit() const { return read_u16(0x1C); }
    void RequiredStateBit(std::uint16_t value) { write_u16(0x1C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t required_state_bit() const { return RequiredStateBit(); }
    void required_state_bit(std::uint16_t value) { RequiredStateBit(value); }
    [[nodiscard]] std::uint16_t Field1E() const { return read_u16(0x1E); }
    void Field1E(std::uint16_t value) { write_u16(0x1E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_e() const { return Field1E(); }
    void field1_e(std::uint16_t value) { Field1E(value); }
    [[nodiscard]] std::int32_t TriggerThreshold() const { return read_i32(0x20); }
    void TriggerThreshold(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t trigger_threshold() const { return TriggerThreshold(); }
    void trigger_threshold(std::int32_t value) { TriggerThreshold(value); }
    [[nodiscard]] std::int32_t TriggersNeeded() const { return read_i32(0x24); }
    void TriggersNeeded(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t triggers_needed() const { return TriggersNeeded(); }
    void triggers_needed(std::int32_t value) { TriggersNeeded(value); }
    [[nodiscard]] formats::TriggerFlags TriggerFlags() const { return static_cast<formats::TriggerFlags>(read_u32(0x28)); }
    void TriggerFlags(formats::TriggerFlags value) { write_u32(0x28, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::TriggerFlags trigger_flags() const { return TriggerFlags(); }
    void trigger_flags(formats::TriggerFlags value) { TriggerFlags(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x2C); }
    void Data(std::uint32_t value) { write_pointer(0x2C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint32_t Parent() const { return read_pointer(0x30); }
    void Parent(std::uint32_t value) { write_pointer(0x30, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t parent() const { return Parent(); }
    void parent(std::uint32_t value) { Parent(value); }
    [[nodiscard]] std::uint32_t Child() const { return read_pointer(0x34); }
    void Child(std::uint32_t value) { write_pointer(0x34, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t child() const { return Child(); }
    void child(std::uint32_t value) { Child(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x19;
    static constexpr std::size_t _off2 = 0x1A;
    static constexpr std::size_t _off3 = 0x1C;
    static constexpr std::size_t _off4 = 0x1E;
    static constexpr std::size_t _off5 = 0x20;
    static constexpr std::size_t _off6 = 0x24;
    static constexpr std::size_t _off7 = 0x28;
    static constexpr std::size_t _off8 = 0x2C;
    static constexpr std::size_t _off9 = 0x30;
    static constexpr std::size_t _off10 = 0x34;
    static constexpr std::size_t _off11 = 0x38;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
};

class CAreaVolume : public CEntity {
public:
    CAreaVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CAreaVolume(Buffer& buffer, std::uint32_t address);
    ~CAreaVolume() override;

    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x18); }
    void Pos(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x24); }
    void Vec2(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x30); }
    void Vec1(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] std::uint8_t Active() const { return read_u8(0x3C); }
    void Active(std::uint8_t value) { write_u8(0x3C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t active() const { return Active(); }
    void active(std::uint8_t value) { Active(value); }
    [[nodiscard]] std::uint8_t AllowMultiple() const { return read_u8(0x3D); }
    void AllowMultiple(std::uint8_t value) { write_u8(0x3D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t allow_multiple() const { return AllowMultiple(); }
    void allow_multiple(std::uint8_t value) { AllowMultiple(value); }
    [[nodiscard]] std::uint8_t EventDelay() const { return read_u8(0x3E); }
    void EventDelay(std::uint8_t value) { write_u8(0x3E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t event_delay() const { return EventDelay(); }
    void event_delay(std::uint8_t value) { EventDelay(value); }
    [[nodiscard]] ByteArray& TriggeredSlots() noexcept;
    [[nodiscard]] const ByteArray& TriggeredSlots() const noexcept;
    [[nodiscard]] ByteArray& triggered_slots() noexcept { return TriggeredSlots(); }
    [[nodiscard]] const ByteArray& triggered_slots() const noexcept { return TriggeredSlots(); }
    [[nodiscard]] ByteArray& PrioritySlots() noexcept;
    [[nodiscard]] const ByteArray& PrioritySlots() const noexcept;
    [[nodiscard]] ByteArray& priority_slots() noexcept { return PrioritySlots(); }
    [[nodiscard]] const ByteArray& priority_slots() const noexcept { return PrioritySlots(); }
    [[nodiscard]] std::uint8_t Padding47() const { return read_u8(0x47); }
    void Padding47(std::uint8_t value) { write_u8(0x47, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding47() const { return Padding47(); }
    void padding47(std::uint8_t value) { Padding47(value); }
    [[nodiscard]] formats::Message InsideEventId() const { return static_cast<formats::Message>(read_u32(0x48)); }
    void InsideEventId(formats::Message value) { write_u32(0x48, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::Message inside_event_id() const { return InsideEventId(); }
    void inside_event_id(formats::Message value) { InsideEventId(value); }
    [[nodiscard]] std::int32_t InsideEventParam1() const { return read_i32(0x4C); }
    void InsideEventParam1(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t inside_event_param1() const { return InsideEventParam1(); }
    void inside_event_param1(std::int32_t value) { InsideEventParam1(value); }
    [[nodiscard]] std::int32_t InsideEventParam2() const { return read_i32(0x50); }
    void InsideEventParam2(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t inside_event_param2() const { return InsideEventParam2(); }
    void inside_event_param2(std::int32_t value) { InsideEventParam2(value); }
    [[nodiscard]] formats::Message ExitEventId() const { return static_cast<formats::Message>(read_u32(0x54)); }
    void ExitEventId(formats::Message value) { write_u32(0x54, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::Message exit_event_id() const { return ExitEventId(); }
    void exit_event_id(formats::Message value) { ExitEventId(value); }
    [[nodiscard]] std::int32_t ExitEventParam1() const { return read_i32(0x58); }
    void ExitEventParam1(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t exit_event_param1() const { return ExitEventParam1(); }
    void exit_event_param1(std::int32_t value) { ExitEventParam1(value); }
    [[nodiscard]] std::int32_t ExitEventParam2() const { return read_i32(0x5C); }
    void ExitEventParam2(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t exit_event_param2() const { return ExitEventParam2(); }
    void exit_event_param2(std::int32_t value) { ExitEventParam2(value); }
    [[nodiscard]] formats::TriggerFlags TriggerFlags() const { return static_cast<formats::TriggerFlags>(read_u32(0x60)); }
    void TriggerFlags(formats::TriggerFlags value) { write_u32(0x60, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::TriggerFlags trigger_flags() const { return TriggerFlags(); }
    void trigger_flags(formats::TriggerFlags value) { TriggerFlags(value); }
    [[nodiscard]] std::uint16_t Priority() const { return read_u16(0x64); }
    void Priority(std::uint16_t value) { write_u16(0x64, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t priority() const { return Priority(); }
    void priority(std::uint16_t value) { Priority(value); }
    [[nodiscard]] std::uint16_t Cooldown() const { return read_u16(0x66); }
    void Cooldown(std::uint16_t value) { write_u16(0x66, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown() const { return Cooldown(); }
    void cooldown(std::uint16_t value) { Cooldown(value); }
    [[nodiscard]] UInt16Array& CooldownSlots() noexcept;
    [[nodiscard]] const UInt16Array& CooldownSlots() const noexcept;
    [[nodiscard]] UInt16Array& cooldown_slots() noexcept { return CooldownSlots(); }
    [[nodiscard]] const UInt16Array& cooldown_slots() const noexcept { return CooldownSlots(); }
    [[nodiscard]] std::uint32_t Parent() const { return read_pointer(0x70); }
    void Parent(std::uint32_t value) { write_pointer(0x70, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t parent() const { return Parent(); }
    void parent(std::uint32_t value) { Parent(value); }
    [[nodiscard]] std::uint32_t Child() const { return read_pointer(0x74); }
    void Child(std::uint32_t value) { write_pointer(0x74, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t child() const { return Child(); }
    void child(std::uint32_t value) { Child(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x78); }
    void NodeRef(std::uint32_t value) { write_pointer(0x78, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x3D;
    static constexpr std::size_t _off5 = 0x3E;
    static constexpr std::size_t _off6 = 0x3F;
    static constexpr std::size_t _off7 = 0x43;
    static constexpr std::size_t _off8 = 0x47;
    static constexpr std::size_t _off9 = 0x48;
    static constexpr std::size_t _off10 = 0x4C;
    static constexpr std::size_t _off11 = 0x50;
    static constexpr std::size_t _off12 = 0x54;
    static constexpr std::size_t _off13 = 0x58;
    static constexpr std::size_t _off14 = 0x5C;
    static constexpr std::size_t _off15 = 0x60;
    static constexpr std::size_t _off16 = 0x64;
    static constexpr std::size_t _off17 = 0x66;
    static constexpr std::size_t _off18 = 0x68;
    static constexpr std::size_t _off19 = 0x70;
    static constexpr std::size_t _off20 = 0x74;
    static constexpr std::size_t _off21 = 0x78;
    static constexpr std::size_t _off22 = 0x7C;
    std::unique_ptr<ByteArray> triggered_slots_;
    std::unique_ptr<ByteArray> priority_slots_;
    std::unique_ptr<UInt16Array> cooldown_slots_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
};

class CJumpPad : public CEntity {
public:
    CJumpPad(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CJumpPad(Buffer& buffer, std::uint32_t address);
    ~CJumpPad() override;

    [[nodiscard]] std::uint16_t ParentId() const { return read_u16(0x18); }
    void ParentId(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t parent_id() const { return ParentId(); }
    void parent_id(std::uint16_t value) { ParentId(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] formats::Vector3 Field1C() const { return read_vec3(0x1C); }
    void Field1C(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 field1_c() const { return Field1C(); }
    void field1_c(formats::Vector3 value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x40); }
    void Pos(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 BaseVec2() const { return read_vec3(0x4C); }
    void BaseVec2(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 base_vec2() const { return BaseVec2(); }
    void base_vec2(formats::Vector3 value) { BaseVec2(value); }
    [[nodiscard]] formats::Vector3 BaseVec1() const { return read_vec3(0x58); }
    void BaseVec1(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 base_vec1() const { return BaseVec1(); }
    void base_vec1(formats::Vector3 value) { BaseVec1(value); }
    [[nodiscard]] formats::Matrix4x3 BaseMtx() const { return read_mtx43(0x64); }
    void BaseMtx(formats::Matrix4x3 value) { write_mtx43(0x64, value); }
    [[nodiscard]] formats::Matrix4x3 base_mtx() const { return BaseMtx(); }
    void base_mtx(formats::Matrix4x3 value) { BaseMtx(value); }
    [[nodiscard]] formats::Matrix4x3 BeamMtx() const { return read_mtx43(0x94); }
    void BeamMtx(formats::Matrix4x3 value) { write_mtx43(0x94, value); }
    [[nodiscard]] formats::Matrix4x3 beam_mtx() const { return BeamMtx(); }
    void beam_mtx(formats::Matrix4x3 value) { BeamMtx(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0xC4); }
    void Data(std::uint32_t value) { write_pointer(0xC4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint16_t CooldownTime() const { return read_u16(0xC8); }
    void CooldownTime(std::uint16_t value) { write_u16(0xC8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown_time() const { return CooldownTime(); }
    void cooldown_time(std::uint16_t value) { CooldownTime(value); }
    [[nodiscard]] std::uint16_t CooldownTimer() const { return read_u16(0xCA); }
    void CooldownTimer(std::uint16_t value) { write_u16(0xCA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cooldown_timer() const { return CooldownTimer(); }
    void cooldown_timer(std::uint16_t value) { CooldownTimer(value); }
    [[nodiscard]] std::uint8_t UsedState() const { return read_u8(0xCC); }
    void UsedState(std::uint8_t value) { write_u8(0xCC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t used_state() const { return UsedState(); }
    void used_state(std::uint8_t value) { UsedState(value); }
    [[nodiscard]] std::uint8_t Flags1() const { return read_u8(0xCD); }
    void Flags1(std::uint8_t value) { write_u8(0xCD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags1() const { return Flags1(); }
    void flags1(std::uint8_t value) { Flags1(value); }
    [[nodiscard]] std::uint16_t FieldCE() const { return read_u16(0xCE); }
    void FieldCE(std::uint16_t value) { write_u16(0xCE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_c_e() const { return FieldCE(); }
    void field_c_e(std::uint16_t value) { FieldCE(value); }
    [[nodiscard]] formats::TriggerFlags TriggerFlags() const { return static_cast<formats::TriggerFlags>(read_u32(0xD0)); }
    void TriggerFlags(formats::TriggerFlags value) { write_u32(0xD0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::TriggerFlags trigger_flags() const { return TriggerFlags(); }
    void trigger_flags(formats::TriggerFlags value) { TriggerFlags(value); }
    [[nodiscard]] std::uint16_t FieldD4() const { return read_u16(0xD4); }
    void FieldD4(std::uint16_t value) { write_u16(0xD4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_d4() const { return FieldD4(); }
    void field_d4(std::uint16_t value) { FieldD4(value); }
    [[nodiscard]] std::uint16_t Timer() const { return read_u16(0xD6); }
    void Timer(std::uint16_t value) { write_u16(0xD6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t timer() const { return Timer(); }
    void timer(std::uint16_t value) { Timer(value); }
    [[nodiscard]] formats::Vector3 BeamVec() const { return read_vec3(0xD8); }
    void BeamVec(formats::Vector3 value) { write_vec3(0xD8, value); }
    [[nodiscard]] formats::Vector3 beam_vec() const { return BeamVec(); }
    void beam_vec(formats::Vector3 value) { BeamVec(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& FieldE4() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& FieldE4() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& field_e4() noexcept { return FieldE4(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& field_e4() const noexcept { return FieldE4(); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x164); }
    void NodeRef(std::uint32_t value) { write_pointer(0x164, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& BaseModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& BaseModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& base_model() noexcept { return BaseModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& base_model() const noexcept { return BaseModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& BeamModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& BeamModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& beam_model() noexcept { return BeamModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& beam_model() const noexcept { return BeamModel(); }
    [[nodiscard]] std::int32_t BaseId() const { return read_i32(0x1F8); }
    void BaseId(std::int32_t value) { write_i32(0x1F8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t base_id() const { return BaseId(); }
    void base_id(std::int32_t value) { BaseId(value); }
    [[nodiscard]] std::int32_t BeamId() const { return read_i32(0x1FC); }
    void BeamId(std::int32_t value) { write_i32(0x1FC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t beam_id() const { return BeamId(); }
    void beam_id(std::int32_t value) { BeamId(value); }
    [[nodiscard]] std::uint32_t NodedataRelated() const { return read_pointer(0x200); }
    void NodedataRelated(std::uint32_t value) { write_pointer(0x200, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata_related() const { return NodedataRelated(); }
    void nodedata_related(std::uint32_t value) { NodedataRelated(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x28;
    static constexpr std::size_t _off4 = 0x2C;
    static constexpr std::size_t _off5 = 0x30;
    static constexpr std::size_t _off6 = 0x34;
    static constexpr std::size_t _off7 = 0x38;
    static constexpr std::size_t _off8 = 0x3C;
    static constexpr std::size_t _off9 = 0x40;
    static constexpr std::size_t _off10 = 0x4C;
    static constexpr std::size_t _off11 = 0x58;
    static constexpr std::size_t _off12 = 0x64;
    static constexpr std::size_t _off13 = 0x94;
    static constexpr std::size_t _off14 = 0xC4;
    static constexpr std::size_t _off15 = 0xC8;
    static constexpr std::size_t _off16 = 0xCA;
    static constexpr std::size_t _off17 = 0xCC;
    static constexpr std::size_t _off18 = 0xCD;
    static constexpr std::size_t _off19 = 0xCE;
    static constexpr std::size_t _off20 = 0xD0;
    static constexpr std::size_t _off21 = 0xD4;
    static constexpr std::size_t _off22 = 0xD6;
    static constexpr std::size_t _off23 = 0xD8;
    static constexpr std::size_t _off24 = 0xE4;
    static constexpr std::size_t _off25 = 0x124;
    static constexpr std::size_t _off26 = 0x164;
    static constexpr std::size_t _off27 = 0x168;
    static constexpr std::size_t _off28 = 0x1B0;
    static constexpr std::size_t _off29 = 0x1F8;
    static constexpr std::size_t _off30 = 0x1FC;
    static constexpr std::size_t _off31 = 0x200;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> field_e4_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
    std::unique_ptr<::fruityprime::memory::CModel> base_model_;
    std::unique_ptr<::fruityprime::memory::CModel> beam_model_;
};

class CPointModule : public CEntity {
public:
    CPointModule(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CPointModule(Buffer& buffer, std::uint32_t address);
    ~CPointModule() override;

    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::uint32_t Field28() const { return read_pointer(0x28); }
    void Field28(std::uint32_t value) { write_pointer(0x28, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field28() const { return Field28(); }
    void field28(std::uint32_t value) { Field28(value); }
    [[nodiscard]] std::uint32_t Field2C() const { return read_pointer(0x2C); }
    void Field2C(std::uint32_t value) { write_pointer(0x2C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field2_c() const { return Field2C(); }
    void field2_c(std::uint32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::uint32_t Field38() const { return read_pointer(0x38); }
    void Field38(std::uint32_t value) { write_pointer(0x38, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field38() const { return Field38(); }
    void field38(std::uint32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Flags() const { return read_i32(0x3C); }
    void Flags(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t flags() const { return Flags(); }
    void flags(std::int32_t value) { Flags(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1C;
    static constexpr std::size_t _off2 = 0x20;
    static constexpr std::size_t _off3 = 0x24;
    static constexpr std::size_t _off4 = 0x28;
    static constexpr std::size_t _off5 = 0x2C;
    static constexpr std::size_t _off6 = 0x30;
    static constexpr std::size_t _off7 = 0x34;
    static constexpr std::size_t _off8 = 0x38;
    static constexpr std::size_t _off9 = 0x3C;
};

class CMorphCamera : public CEntity {
public:
    CMorphCamera(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CMorphCamera(Buffer& buffer, std::uint32_t address);
    ~CMorphCamera() override;

    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x18); }
    void Pos(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x64); }
    void NodeRef(std::uint32_t value) { write_pointer(0x64, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x64;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
};

class COctolithFlag : public CEntity {
public:
    COctolithFlag(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    COctolithFlag(Buffer& buffer, std::uint32_t address);
    ~COctolithFlag() override;

    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x18); }
    void Vec2(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 BasePos() const { return read_vec3(0x3C); }
    void BasePos(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 base_pos() const { return BasePos(); }
    void base_pos(formats::Vector3 value) { BasePos(value); }
    [[nodiscard]] std::int32_t HeightBob() const { return read_i32(0x48); }
    void HeightBob(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t height_bob() const { return HeightBob(); }
    void height_bob(std::int32_t value) { HeightBob(value); }
    [[nodiscard]] std::uint8_t TeamIndex() const { return read_u8(0x4C); }
    void TeamIndex(std::uint8_t value) { write_u8(0x4C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t team_index() const { return TeamIndex(); }
    void team_index(std::uint8_t value) { TeamIndex(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x4D); }
    void Flags(std::uint8_t value) { write_u8(0x4D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint16_t Field4E() const { return read_u16(0x4E); }
    void Field4E(std::uint16_t value) { write_u16(0x4E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_e() const { return Field4E(); }
    void field4_e(std::uint16_t value) { Field4E(value); }
    [[nodiscard]] std::int32_t DespawnTimer() const { return read_i32(0x50); }
    void DespawnTimer(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t despawn_timer() const { return DespawnTimer(); }
    void despawn_timer(std::int32_t value) { DespawnTimer(value); }
    [[nodiscard]] std::uint32_t Player() const { return read_pointer(0x54); }
    void Player(std::uint32_t value) { write_pointer(0x54, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t player() const { return Player(); }
    void player(std::uint32_t value) { Player(value); }
    [[nodiscard]] std::uint32_t LastPlayer() const { return read_pointer(0x58); }
    void LastPlayer(std::uint32_t value) { write_pointer(0x58, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t last_player() const { return LastPlayer(); }
    void last_player(std::uint32_t value) { LastPlayer(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& BaseModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& BaseModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& base_model() noexcept { return BaseModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& base_model() const noexcept { return BaseModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& OctoModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& OctoModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& octo_model() noexcept { return OctoModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& octo_model() const noexcept { return OctoModel(); }
    [[nodiscard]] std::uint32_t NodedataRelated() const { return read_pointer(0xEC); }
    void NodedataRelated(std::uint32_t value) { write_pointer(0xEC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata_related() const { return NodedataRelated(); }
    void nodedata_related(std::uint32_t value) { NodedataRelated(value); }
    [[nodiscard]] std::int32_t FieldF0() const { return read_i32(0xF0); }
    void FieldF0(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0() const { return FieldF0(); }
    void field_f0(std::int32_t value) { FieldF0(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x48;
    static constexpr std::size_t _off5 = 0x4C;
    static constexpr std::size_t _off6 = 0x4D;
    static constexpr std::size_t _off7 = 0x4E;
    static constexpr std::size_t _off8 = 0x50;
    static constexpr std::size_t _off9 = 0x54;
    static constexpr std::size_t _off10 = 0x58;
    static constexpr std::size_t _off11 = 0x5C;
    static constexpr std::size_t _off12 = 0xA4;
    static constexpr std::size_t _off13 = 0xEC;
    static constexpr std::size_t _off14 = 0xF0;
    std::unique_ptr<::fruityprime::memory::CModel> base_model_;
    std::unique_ptr<::fruityprime::memory::CModel> octo_model_;
};

class CFlagBase : public CEntity {
public:
    CFlagBase(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CFlagBase(Buffer& buffer, std::uint32_t address);
    ~CFlagBase() override;

    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x18); }
    void Vec2(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::uint8_t TeamId() const { return read_u8(0x7C); }
    void TeamId(std::uint8_t value) { write_u8(0x7C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t team_id() const { return TeamId(); }
    void team_id(std::uint8_t value) { TeamId(value); }
    [[nodiscard]] std::uint8_t Field7D() const { return read_u8(0x7D); }
    void Field7D(std::uint8_t value) { write_u8(0x7D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field7_d() const { return Field7D(); }
    void field7_d(std::uint8_t value) { Field7D(value); }
    [[nodiscard]] std::uint16_t Field7E() const { return read_u16(0x7E); }
    void Field7E(std::uint16_t value) { write_u16(0x7E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field7_e() const { return Field7E(); }
    void field7_e(std::uint16_t value) { Field7E(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t NodedataRelated() const { return read_pointer(0xC8); }
    void NodedataRelated(std::uint32_t value) { write_pointer(0xC8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata_related() const { return NodedataRelated(); }
    void nodedata_related(std::uint32_t value) { NodedataRelated(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x7C;
    static constexpr std::size_t _off5 = 0x7D;
    static constexpr std::size_t _off6 = 0x7E;
    static constexpr std::size_t _off7 = 0x80;
    static constexpr std::size_t _off8 = 0xC8;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
};

class CTeleporter : public CEntity {
public:
    CTeleporter(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CTeleporter(Buffer& buffer, std::uint32_t address);
    ~CTeleporter() override;

    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x18); }
    void Vec2(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 TargetPos() const { return read_vec3(0x3C); }
    void TargetPos(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 target_pos() const { return TargetPos(); }
    void target_pos(formats::Vector3 value) { TargetPos(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x48); }
    void Flags(std::uint8_t value) { write_u8(0x48, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Field49() const { return read_u8(0x49); }
    void Field49(std::uint8_t value) { write_u8(0x49, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field49() const { return Field49(); }
    void field49(std::uint8_t value) { Field49(value); }
    [[nodiscard]] std::uint8_t ArtifactId() const { return read_u8(0x4A); }
    void ArtifactId(std::uint8_t value) { write_u8(0x4A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t artifact_id() const { return ArtifactId(); }
    void artifact_id(std::uint8_t value) { ArtifactId(value); }
    [[nodiscard]] std::uint8_t Field4B() const { return read_u8(0x4B); }
    void Field4B(std::uint8_t value) { write_u8(0x4B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_b() const { return Field4B(); }
    void field4_b(std::uint8_t value) { Field4B(value); }
    [[nodiscard]] std::uint8_t Field4C() const { return read_u8(0x4C); }
    void Field4C(std::uint8_t value) { write_u8(0x4C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_c() const { return Field4C(); }
    void field4_c(std::uint8_t value) { Field4C(value); }
    [[nodiscard]] std::uint8_t Field4D() const { return read_u8(0x4D); }
    void Field4D(std::uint8_t value) { write_u8(0x4D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_d() const { return Field4D(); }
    void field4_d(std::uint8_t value) { Field4D(value); }
    [[nodiscard]] std::uint16_t Field4E() const { return read_u16(0x4E); }
    void Field4E(std::uint16_t value) { write_u16(0x4E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_e() const { return Field4E(); }
    void field4_e(std::uint16_t value) { Field4E(value); }
    [[nodiscard]] std::int32_t ConnectorId() const { return read_i32(0x50); }
    void ConnectorId(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t connector_id() const { return ConnectorId(); }
    void connector_id(std::int32_t value) { ConnectorId(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x54); }
    void NodeRef(std::uint32_t value) { write_pointer(0x54, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::uint32_t Node() const { return read_pointer(0x58); }
    void Node(std::uint32_t value) { write_pointer(0x58, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node() const { return Node(); }
    void node(std::uint32_t value) { Node(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& TeleModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& TeleModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& tele_model() noexcept { return TeleModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& tele_model() const noexcept { return TeleModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& ArtifactModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& ArtifactModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& artifact_model() noexcept { return ArtifactModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& artifact_model() const noexcept { return ArtifactModel(); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x48;
    static constexpr std::size_t _off5 = 0x49;
    static constexpr std::size_t _off6 = 0x4A;
    static constexpr std::size_t _off7 = 0x4B;
    static constexpr std::size_t _off8 = 0x4C;
    static constexpr std::size_t _off9 = 0x4D;
    static constexpr std::size_t _off10 = 0x4E;
    static constexpr std::size_t _off11 = 0x50;
    static constexpr std::size_t _off12 = 0x54;
    static constexpr std::size_t _off13 = 0x58;
    static constexpr std::size_t _off14 = 0x5C;
    static constexpr std::size_t _off15 = 0xA4;
    static constexpr std::size_t _off16 = 0xEC;
    std::unique_ptr<::fruityprime::memory::CModel> tele_model_;
    std::unique_ptr<::fruityprime::memory::CModel> artifact_model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CNodeDefense : public CEntity {
public:
    CNodeDefense(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CNodeDefense(Buffer& buffer, std::uint32_t address);
    ~CNodeDefense() override;

    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x18); }
    void Vec2(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::int32_t Rotation() const { return read_i32(0x3C); }
    void Rotation(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t rotation() const { return Rotation(); }
    void rotation(std::int32_t value) { Rotation(value); }
    [[nodiscard]] std::int32_t RotSpeed() const { return read_i32(0x40); }
    void RotSpeed(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t rot_speed() const { return RotSpeed(); }
    void rot_speed(std::int32_t value) { RotSpeed(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }
    [[nodiscard]] std::uint8_t TeamIndex() const { return read_u8(0x84); }
    void TeamIndex(std::uint8_t value) { write_u8(0x84, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t team_index() const { return TeamIndex(); }
    void team_index(std::uint8_t value) { TeamIndex(value); }
    [[nodiscard]] std::uint8_t OccupyingTeam() const { return read_u8(0x85); }
    void OccupyingTeam(std::uint8_t value) { write_u8(0x85, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t occupying_team() const { return OccupyingTeam(); }
    void occupying_team(std::uint8_t value) { OccupyingTeam(value); }
    [[nodiscard]] std::uint8_t OccupyFlags() const { return read_u8(0x86); }
    void OccupyFlags(std::uint8_t value) { write_u8(0x86, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t occupy_flags() const { return OccupyFlags(); }
    void occupy_flags(std::uint8_t value) { OccupyFlags(value); }
    [[nodiscard]] std::uint8_t Occupied() const { return read_u8(0x87); }
    void Occupied(std::uint8_t value) { write_u8(0x87, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t occupied() const { return Occupied(); }
    void occupied(std::uint8_t value) { Occupied(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x88); }
    void Flags(std::uint8_t value) { write_u8(0x88, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Field89() const { return read_u8(0x89); }
    void Field89(std::uint8_t value) { write_u8(0x89, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field89() const { return Field89(); }
    void field89(std::uint8_t value) { Field89(value); }
    [[nodiscard]] std::uint16_t Field8A() const { return read_u16(0x8A); }
    void Field8A(std::uint16_t value) { write_u16(0x8A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field8_a() const { return Field8A(); }
    void field8_a(std::uint16_t value) { Field8A(value); }
    [[nodiscard]] std::int32_t Progress() const { return read_i32(0x8C); }
    void Progress(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t progress() const { return Progress(); }
    void progress(std::int32_t value) { Progress(value); }
    [[nodiscard]] std::int32_t Field90() const { return read_i32(0x90); }
    void Field90(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field90() const { return Field90(); }
    void field90(std::int32_t value) { Field90(value); }
    [[nodiscard]] std::uint32_t SomePlayer() const { return read_pointer(0x94); }
    void SomePlayer(std::uint32_t value) { write_pointer(0x94, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t some_player() const { return SomePlayer(); }
    void some_player(std::uint32_t value) { SomePlayer(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] ::fruityprime::memory::CModel& RingModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& RingModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& ring_model() noexcept { return RingModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& ring_model() const noexcept { return RingModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& NodeModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& NodeModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& node_model() noexcept { return NodeModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& node_model() const noexcept { return NodeModel(); }
    [[nodiscard]] std::uint32_t NodedataRelated() const { return read_pointer(0x12C); }
    void NodedataRelated(std::uint32_t value) { write_pointer(0x12C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata_related() const { return NodedataRelated(); }
    void nodedata_related(std::uint32_t value) { NodedataRelated(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x40;
    static constexpr std::size_t _off5 = 0x44;
    static constexpr std::size_t _off6 = 0x84;
    static constexpr std::size_t _off7 = 0x85;
    static constexpr std::size_t _off8 = 0x86;
    static constexpr std::size_t _off9 = 0x87;
    static constexpr std::size_t _off10 = 0x88;
    static constexpr std::size_t _off11 = 0x89;
    static constexpr std::size_t _off12 = 0x8A;
    static constexpr std::size_t _off13 = 0x8C;
    static constexpr std::size_t _off14 = 0x90;
    static constexpr std::size_t _off15 = 0x94;
    static constexpr std::size_t _off16 = 0x98;
    static constexpr std::size_t _off17 = 0x9C;
    static constexpr std::size_t _off18 = 0xE4;
    static constexpr std::size_t _off19 = 0x12C;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
    std::unique_ptr<::fruityprime::memory::CModel> ring_model_;
    std::unique_ptr<::fruityprime::memory::CModel> node_model_;
};

class CLightSource : public CEntity {
public:
    CLightSource(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CLightSource(Buffer& buffer, std::uint32_t address);
    ~CLightSource() override;

    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x18); }
    void Data(std::uint32_t value) { write_pointer(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Volume() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Volume() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& volume() noexcept { return Volume(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& volume() const noexcept { return Volume(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1C;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> volume_;
};

class CArtifact : public CEntity {
public:
    CArtifact(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CArtifact(Buffer& buffer, std::uint32_t address);
    ~CArtifact() override;

    [[nodiscard]] std::uint16_t LinkedEntityId() const { return read_u16(0x18); }
    void LinkedEntityId(std::uint16_t value) { write_u16(0x18, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t linked_entity_id() const { return LinkedEntityId(); }
    void linked_entity_id(std::uint16_t value) { LinkedEntityId(value); }
    [[nodiscard]] std::uint16_t Field1A() const { return read_u16(0x1A); }
    void Field1A(std::uint16_t value) { write_u16(0x1A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_a() const { return Field1A(); }
    void field1_a(std::uint16_t value) { Field1A(value); }
    [[nodiscard]] formats::Vector3 Field1C() const { return read_vec3(0x1C); }
    void Field1C(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 field1_c() const { return Field1C(); }
    void field1_c(formats::Vector3 value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x40); }
    void Vec2(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x4C); }
    void Vec1(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x58); }
    void Pos(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::uint8_t Active() const { return read_u8(0x64); }
    void Active(std::uint8_t value) { write_u8(0x64, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t active() const { return Active(); }
    void active(std::uint8_t value) { Active(value); }
    [[nodiscard]] std::uint8_t ModelId() const { return read_u8(0x65); }
    void ModelId(std::uint8_t value) { write_u8(0x65, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t model_id() const { return ModelId(); }
    void model_id(std::uint8_t value) { ModelId(value); }
    [[nodiscard]] std::uint8_t ArtifactId() const { return read_u8(0x66); }
    void ArtifactId(std::uint8_t value) { write_u8(0x66, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t artifact_id() const { return ArtifactId(); }
    void artifact_id(std::uint8_t value) { ArtifactId(value); }
    [[nodiscard]] std::uint8_t Field67() const { return read_u8(0x67); }
    void Field67(std::uint8_t value) { write_u8(0x67, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field67() const { return Field67(); }
    void field67(std::uint8_t value) { Field67(value); }
    [[nodiscard]] std::uint16_t FoundLinked() const { return read_u16(0x68); }
    void FoundLinked(std::uint16_t value) { write_u16(0x68, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t found_linked() const { return FoundLinked(); }
    void found_linked(std::uint16_t value) { FoundLinked(value); }
    [[nodiscard]] std::uint16_t Field6A() const { return read_u16(0x6A); }
    void Field6A(std::uint16_t value) { write_u16(0x6A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field6_a() const { return Field6A(); }
    void field6_a(std::uint16_t value) { Field6A(value); }
    [[nodiscard]] std::uint32_t Entity1() const { return read_pointer(0x6C); }
    void Entity1(std::uint32_t value) { write_pointer(0x6C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity1() const { return Entity1(); }
    void entity1(std::uint32_t value) { Entity1(value); }
    [[nodiscard]] std::uint32_t Entity2() const { return read_pointer(0x70); }
    void Entity2(std::uint32_t value) { write_pointer(0x70, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity2() const { return Entity2(); }
    void entity2(std::uint32_t value) { Entity2(value); }
    [[nodiscard]] std::uint32_t Entity3() const { return read_pointer(0x74); }
    void Entity3(std::uint32_t value) { write_pointer(0x74, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity3() const { return Entity3(); }
    void entity3(std::uint32_t value) { Entity3(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x78); }
    void Data(std::uint32_t value) { write_pointer(0x78, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& ArtifactModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& ArtifactModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& artifact_model() noexcept { return ArtifactModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& artifact_model() const noexcept { return ArtifactModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& BaseModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& BaseModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& base_model() noexcept { return BaseModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& base_model() const noexcept { return BaseModel(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x10C); }
    void NodeRef(std::uint32_t value) { write_pointer(0x10C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] std::int32_t Field114() const { return read_i32(0x114); }
    void Field114(std::int32_t value) { write_i32(0x114, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field114() const { return Field114(); }
    void field114(std::int32_t value) { Field114(value); }
    [[nodiscard]] std::int32_t Field118() const { return read_i32(0x118); }
    void Field118(std::int32_t value) { write_i32(0x118, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field118() const { return Field118(); }
    void field118(std::int32_t value) { Field118(value); }
    [[nodiscard]] std::int32_t Field11C() const { return read_i32(0x11C); }
    void Field11C(std::int32_t value) { write_i32(0x11C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field11_c() const { return Field11C(); }
    void field11_c(std::int32_t value) { Field11C(value); }
    [[nodiscard]] std::int32_t Field120() const { return read_i32(0x120); }
    void Field120(std::int32_t value) { write_i32(0x120, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field120() const { return Field120(); }
    void field120(std::int32_t value) { Field120(value); }
    [[nodiscard]] std::int32_t Field124() const { return read_i32(0x124); }
    void Field124(std::int32_t value) { write_i32(0x124, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field124() const { return Field124(); }
    void field124(std::int32_t value) { Field124(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1A;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x28;
    static constexpr std::size_t _off4 = 0x2C;
    static constexpr std::size_t _off5 = 0x30;
    static constexpr std::size_t _off6 = 0x34;
    static constexpr std::size_t _off7 = 0x38;
    static constexpr std::size_t _off8 = 0x3C;
    static constexpr std::size_t _off9 = 0x40;
    static constexpr std::size_t _off10 = 0x4C;
    static constexpr std::size_t _off11 = 0x58;
    static constexpr std::size_t _off12 = 0x64;
    static constexpr std::size_t _off13 = 0x65;
    static constexpr std::size_t _off14 = 0x66;
    static constexpr std::size_t _off15 = 0x67;
    static constexpr std::size_t _off16 = 0x68;
    static constexpr std::size_t _off17 = 0x6A;
    static constexpr std::size_t _off18 = 0x6C;
    static constexpr std::size_t _off19 = 0x70;
    static constexpr std::size_t _off20 = 0x74;
    static constexpr std::size_t _off21 = 0x78;
    static constexpr std::size_t _off22 = 0x7C;
    static constexpr std::size_t _off23 = 0xC4;
    static constexpr std::size_t _off24 = 0x10C;
    static constexpr std::size_t _off25 = 0x110;
    static constexpr std::size_t _off26 = 0x114;
    static constexpr std::size_t _off27 = 0x118;
    static constexpr std::size_t _off28 = 0x11C;
    static constexpr std::size_t _off29 = 0x120;
    static constexpr std::size_t _off30 = 0x124;
    std::unique_ptr<::fruityprime::memory::CModel> artifact_model_;
    std::unique_ptr<::fruityprime::memory::CModel> base_model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CCameraSequence : public CEntity {
public:
    CCameraSequence(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CCameraSequence(Buffer& buffer, std::uint32_t address);
    ~CCameraSequence() override;

    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x18); }
    void Data(std::uint32_t value) { write_pointer(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint32_t Entity1() const { return read_pointer(0x1C); }
    void Entity1(std::uint32_t value) { write_pointer(0x1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity1() const { return Entity1(); }
    void entity1(std::uint32_t value) { Entity1(value); }
    [[nodiscard]] std::uint32_t Entity2() const { return read_pointer(0x20); }
    void Entity2(std::uint32_t value) { write_pointer(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t entity2() const { return Entity2(); }
    void entity2(std::uint32_t value) { Entity2(value); }
    [[nodiscard]] std::uint32_t EventTarget() const { return read_pointer(0x24); }
    void EventTarget(std::uint32_t value) { write_pointer(0x24, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t event_target() const { return EventTarget(); }
    void event_target(std::uint32_t value) { EventTarget(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x28); }
    void Flags(std::uint8_t value) { write_u8(0x28, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Padding29() const { return read_u8(0x29); }
    void Padding29(std::uint8_t value) { write_u8(0x29, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding29() const { return Padding29(); }
    void padding29(std::uint8_t value) { Padding29(value); }
    [[nodiscard]] std::uint16_t DelayTimer() const { return read_u16(0x2A); }
    void DelayTimer(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t delay_timer() const { return DelayTimer(); }
    void delay_timer(std::uint16_t value) { DelayTimer(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1C;
    static constexpr std::size_t _off2 = 0x20;
    static constexpr std::size_t _off3 = 0x24;
    static constexpr std::size_t _off4 = 0x28;
    static constexpr std::size_t _off5 = 0x29;
    static constexpr std::size_t _off6 = 0x2A;
};

class CForceField : public CEntity {
public:
    CForceField(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CForceField(Buffer& buffer, std::uint32_t address);
    ~CForceField() override;

    [[nodiscard]] formats::Vector3 Normal() const { return read_vec3(0x18); }
    void Normal(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 normal() const { return Normal(); }
    void normal(formats::Vector3 value) { Normal(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x24); }
    void Flags(std::uint8_t value) { write_u8(0x24, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Alpha() const { return read_u8(0x25); }
    void Alpha(std::uint8_t value) { write_u8(0x25, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t alpha() const { return Alpha(); }
    void alpha(std::uint8_t value) { Alpha(value); }
    [[nodiscard]] std::uint16_t Field26() const { return read_u16(0x26); }
    void Field26(std::uint16_t value) { write_u16(0x26, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field26() const { return Field26(); }
    void field26(std::uint16_t value) { Field26(value); }
    [[nodiscard]] std::uint32_t Data() const { return read_pointer(0x28); }
    void Data(std::uint32_t value) { write_pointer(0x28, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data() const { return Data(); }
    void data(std::uint32_t value) { Data(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x2C); }
    void NodeRef(std::uint32_t value) { write_pointer(0x2C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::uint32_t Lock() const { return read_pointer(0x30); }
    void Lock(std::uint32_t value) { write_pointer(0x30, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t lock() const { return Lock(); }
    void lock(std::uint32_t value) { Lock(value); }
    [[nodiscard]] formats::Vector3 Vector2() const { return read_vec3(0x34); }
    void Vector2(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 vector2() const { return Vector2(); }
    void vector2(formats::Vector3 value) { Vector2(value); }
    [[nodiscard]] std::int32_t Field40() const { return read_i32(0x40); }
    void Field40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40() const { return Field40(); }
    void field40(std::int32_t value) { Field40(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x25;
    static constexpr std::size_t _off3 = 0x26;
    static constexpr std::size_t _off4 = 0x28;
    static constexpr std::size_t _off5 = 0x2C;
    static constexpr std::size_t _off6 = 0x30;
    static constexpr std::size_t _off7 = 0x34;
    static constexpr std::size_t _off8 = 0x40;
    static constexpr std::size_t _off9 = 0x44;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
};

class CBeamEffect : public CEntity {
public:
    CBeamEffect(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CBeamEffect(Buffer& buffer, std::uint32_t address);
    ~CBeamEffect() override;

    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x18); }
    void Vec1(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x24); }
    void Vec2(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::uint32_t DrawOffset() const { return read_pointer(0x3C); }
    void DrawOffset(std::uint32_t value) { write_pointer(0x3C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t draw_offset() const { return DrawOffset(); }
    void draw_offset(std::uint32_t value) { DrawOffset(value); }
    [[nodiscard]] formats::Vector3 Speed() const { return read_vec3(0x40); }
    void Speed(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 speed() const { return Speed(); }
    void speed(formats::Vector3 value) { Speed(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x4C); }
    void Flags(std::uint8_t value) { write_u8(0x4C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t BeamType() const { return read_u8(0x4D); }
    void BeamType(std::uint8_t value) { write_u8(0x4D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t beam_type() const { return BeamType(); }
    void beam_type(std::uint8_t value) { BeamType(value); }
    [[nodiscard]] std::uint16_t Lifespan() const { return read_u16(0x4E); }
    void Lifespan(std::uint16_t value) { write_u16(0x4E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t lifespan() const { return Lifespan(); }
    void lifespan(std::uint16_t value) { Lifespan(value); }
    [[nodiscard]] std::uint16_t Age() const { return read_u16(0x50); }
    void Age(std::uint16_t value) { write_u16(0x50, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t age() const { return Age(); }
    void age(std::uint16_t value) { Age(value); }
    [[nodiscard]] std::uint16_t Field52() const { return read_u16(0x52); }
    void Field52(std::uint16_t value) { write_u16(0x52, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field52() const { return Field52(); }
    void field52(std::uint16_t value) { Field52(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] std::int32_t Field58() const { return read_i32(0x58); }
    void Field58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field58() const { return Field58(); }
    void field58(std::int32_t value) { Field58(value); }
    [[nodiscard]] std::int32_t Field5C() const { return read_i32(0x5C); }
    void Field5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field5_c() const { return Field5C(); }
    void field5_c(std::int32_t value) { Field5C(value); }
    [[nodiscard]] std::int32_t Field60() const { return read_i32(0x60); }
    void Field60(std::int32_t value) { write_i32(0x60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field60() const { return Field60(); }
    void field60(std::int32_t value) { Field60(value); }
    [[nodiscard]] std::int32_t Field64() const { return read_i32(0x64); }
    void Field64(std::int32_t value) { write_i32(0x64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field64() const { return Field64(); }
    void field64(std::int32_t value) { Field64(value); }
    [[nodiscard]] std::int32_t ScaleX() const { return read_i32(0x68); }
    void ScaleX(std::int32_t value) { write_i32(0x68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scale_x() const { return ScaleX(); }
    void scale_x(std::int32_t value) { ScaleX(value); }
    [[nodiscard]] std::int32_t ScaleY() const { return read_i32(0x6C); }
    void ScaleY(std::int32_t value) { write_i32(0x6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scale_y() const { return ScaleY(); }
    void scale_y(std::int32_t value) { ScaleY(value); }
    [[nodiscard]] std::int32_t ScaleZ() const { return read_i32(0x70); }
    void ScaleZ(std::int32_t value) { write_i32(0x70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scale_z() const { return ScaleZ(); }
    void scale_z(std::int32_t value) { ScaleZ(value); }
    [[nodiscard]] std::int32_t DrawDist() const { return read_i32(0x74); }
    void DrawDist(std::int32_t value) { write_i32(0x74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t draw_dist() const { return DrawDist(); }
    void draw_dist(std::int32_t value) { DrawDist(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t EffMtxPtr() const { return read_pointer(0xC0); }
    void EffMtxPtr(std::uint32_t value) { write_pointer(0xC0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t eff_mtx_ptr() const { return EffMtxPtr(); }
    void eff_mtx_ptr(std::uint32_t value) { EffMtxPtr(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x40;
    static constexpr std::size_t _off5 = 0x4C;
    static constexpr std::size_t _off6 = 0x4D;
    static constexpr std::size_t _off7 = 0x4E;
    static constexpr std::size_t _off8 = 0x50;
    static constexpr std::size_t _off9 = 0x52;
    static constexpr std::size_t _off10 = 0x54;
    static constexpr std::size_t _off11 = 0x58;
    static constexpr std::size_t _off12 = 0x5C;
    static constexpr std::size_t _off13 = 0x60;
    static constexpr std::size_t _off14 = 0x64;
    static constexpr std::size_t _off15 = 0x68;
    static constexpr std::size_t _off16 = 0x6C;
    static constexpr std::size_t _off17 = 0x70;
    static constexpr std::size_t _off18 = 0x74;
    static constexpr std::size_t _off19 = 0x78;
    static constexpr std::size_t _off20 = 0xC0;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
};

class CBomb : public CEntity {
public:
    CBomb(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CBomb(Buffer& buffer, std::uint32_t address);
    ~CBomb() override;

    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x18); }
    void Pos(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x24); }
    void Vec1(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x30); }
    void Vec2(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 SpeedMaybe() const { return read_vec3(0x3C); }
    void SpeedMaybe(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 speed_maybe() const { return SpeedMaybe(); }
    void speed_maybe(formats::Vector3 value) { SpeedMaybe(value); }
    [[nodiscard]] std::uint8_t BombType() const { return read_u8(0x48); }
    void BombType(std::uint8_t value) { write_u8(0x48, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t bomb_type() const { return BombType(); }
    void bomb_type(std::uint8_t value) { BombType(value); }
    [[nodiscard]] std::uint8_t SiblingCount() const { return read_u8(0x49); }
    void SiblingCount(std::uint8_t value) { write_u8(0x49, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t sibling_count() const { return SiblingCount(); }
    void sibling_count(std::uint8_t value) { SiblingCount(value); }
    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x4A); }
    void Flags(std::uint8_t value) { write_u8(0x4A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Field4B() const { return read_u8(0x4B); }
    void Field4B(std::uint8_t value) { write_u8(0x4B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_b() const { return Field4B(); }
    void field4_b(std::uint8_t value) { Field4B(value); }
    [[nodiscard]] std::uint16_t Countdown() const { return read_u16(0x4C); }
    void Countdown(std::uint16_t value) { write_u16(0x4C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t countdown() const { return Countdown(); }
    void countdown(std::uint16_t value) { Countdown(value); }
    [[nodiscard]] std::uint16_t Field4E() const { return read_u16(0x4E); }
    void Field4E(std::uint16_t value) { write_u16(0x4E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_e() const { return Field4E(); }
    void field4_e(std::uint16_t value) { Field4E(value); }
    [[nodiscard]] std::uint16_t Field50() const { return read_u16(0x50); }
    void Field50(std::uint16_t value) { write_u16(0x50, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field50() const { return Field50(); }
    void field50(std::uint16_t value) { Field50(value); }
    [[nodiscard]] std::uint16_t Field52() const { return read_u16(0x52); }
    void Field52(std::uint16_t value) { write_u16(0x52, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field52() const { return Field52(); }
    void field52(std::uint16_t value) { Field52(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] std::int32_t Field58() const { return read_i32(0x58); }
    void Field58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field58() const { return Field58(); }
    void field58(std::int32_t value) { Field58(value); }
    [[nodiscard]] std::int32_t Field5C() const { return read_i32(0x5C); }
    void Field5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field5_c() const { return Field5C(); }
    void field5_c(std::int32_t value) { Field5C(value); }
    [[nodiscard]] std::uint32_t Owner() const { return read_pointer(0x60); }
    void Owner(std::uint32_t value) { write_pointer(0x60, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t owner() const { return Owner(); }
    void owner(std::uint32_t value) { Owner(value); }
    [[nodiscard]] std::uint32_t OwnerSylux() const { return read_pointer(0x64); }
    void OwnerSylux(std::uint32_t value) { write_pointer(0x64, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t owner_sylux() const { return OwnerSylux(); }
    void owner_sylux(std::uint32_t value) { OwnerSylux(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t RoomNodeRef() const { return read_pointer(0xB0); }
    void RoomNodeRef(std::uint32_t value) { write_pointer(0xB0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t room_node_ref() const { return RoomNodeRef(); }
    void room_node_ref(std::uint32_t value) { RoomNodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x48;
    static constexpr std::size_t _off5 = 0x49;
    static constexpr std::size_t _off6 = 0x4A;
    static constexpr std::size_t _off7 = 0x4B;
    static constexpr std::size_t _off8 = 0x4C;
    static constexpr std::size_t _off9 = 0x4E;
    static constexpr std::size_t _off10 = 0x50;
    static constexpr std::size_t _off11 = 0x52;
    static constexpr std::size_t _off12 = 0x54;
    static constexpr std::size_t _off13 = 0x58;
    static constexpr std::size_t _off14 = 0x5C;
    static constexpr std::size_t _off15 = 0x60;
    static constexpr std::size_t _off16 = 0x64;
    static constexpr std::size_t _off17 = 0x68;
    static constexpr std::size_t _off18 = 0xB0;
    static constexpr std::size_t _off19 = 0xB4;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CHalfturret : public CEntity {
public:
    CHalfturret(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CHalfturret(Buffer& buffer, std::uint32_t address);
    ~CHalfturret() override;

    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x18); }
    void Vec2(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Field24() const { return read_vec3(0x24); }
    void Field24(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 field24() const { return Field24(); }
    void field24(formats::Vector3 value) { Field24(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x30); }
    void Pos(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] std::uint32_t Owner() const { return read_pointer(0x40); }
    void Owner(std::uint32_t value) { write_pointer(0x40, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t owner() const { return Owner(); }
    void owner(std::uint32_t value) { Owner(value); }
    [[nodiscard]] std::uint32_t Target() const { return read_pointer(0x44); }
    void Target(std::uint32_t value) { write_pointer(0x44, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target() const { return Target(); }
    void target(std::uint32_t value) { Target(value); }
    [[nodiscard]] ::fruityprime::memory::LightInfo& LightInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::LightInfo& LightInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::LightInfo& light_info() noexcept { return LightInfo(); }
    [[nodiscard]] const ::fruityprime::memory::LightInfo& light_info() const noexcept { return LightInfo(); }
    [[nodiscard]] std::uint8_t Field67() const { return read_u8(0x67); }
    void Field67(std::uint8_t value) { write_u8(0x67, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field67() const { return Field67(); }
    void field67(std::uint8_t value) { Field67(value); }
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& EquipInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& EquipInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& equip_info() noexcept { return EquipInfo(); }
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& equip_info() const noexcept { return EquipInfo(); }
    [[nodiscard]] std::uint32_t BurnEffect() const { return read_pointer(0x7C); }
    void BurnEffect(std::uint32_t value) { write_pointer(0x7C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t burn_effect() const { return BurnEffect(); }
    void burn_effect(std::uint32_t value) { BurnEffect(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0xC8); }
    void NodeRef(std::uint32_t value) { write_pointer(0xC8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] std::int32_t FieldCC() const { return read_i32(0xCC); }
    void FieldCC(std::int32_t value) { write_i32(0xCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c() const { return FieldCC(); }
    void field_c_c(std::int32_t value) { FieldCC(value); }
    [[nodiscard]] std::uint16_t Health() const { return read_u16(0xD0); }
    void Health(std::uint16_t value) { write_u16(0xD0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health() const { return Health(); }
    void health(std::uint16_t value) { Health(value); }
    [[nodiscard]] std::uint16_t FieldD2() const { return read_u16(0xD2); }
    void FieldD2(std::uint16_t value) { write_u16(0xD2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_d2() const { return FieldD2(); }
    void field_d2(std::uint16_t value) { FieldD2(value); }
    [[nodiscard]] std::uint16_t BurnTimer() const { return read_u16(0xD4); }
    void BurnTimer(std::uint16_t value) { write_u16(0xD4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t burn_timer() const { return BurnTimer(); }
    void burn_timer(std::uint16_t value) { BurnTimer(value); }
    [[nodiscard]] std::uint16_t FieldD6() const { return read_u16(0xD6); }
    void FieldD6(std::uint16_t value) { write_u16(0xD6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_d6() const { return FieldD6(); }
    void field_d6(std::uint16_t value) { FieldD6(value); }
    [[nodiscard]] std::uint8_t FieldD8() const { return read_u8(0xD8); }
    void FieldD8(std::uint8_t value) { write_u8(0xD8, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d8() const { return FieldD8(); }
    void field_d8(std::uint8_t value) { FieldD8(value); }
    [[nodiscard]] std::uint8_t Frozen() const { return read_u8(0xD9); }
    void Frozen(std::uint8_t value) { write_u8(0xD9, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t frozen() const { return Frozen(); }
    void frozen(std::uint8_t value) { Frozen(value); }
    [[nodiscard]] std::uint8_t FieldDA() const { return read_u8(0xDA); }
    void FieldDA(std::uint8_t value) { write_u8(0xDA, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d_a() const { return FieldDA(); }
    void field_d_a(std::uint8_t value) { FieldDA(value); }
    [[nodiscard]] std::uint8_t FieldDB() const { return read_u8(0xDB); }
    void FieldDB(std::uint8_t value) { write_u8(0xDB, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d_b() const { return FieldDB(); }
    void field_d_b(std::uint8_t value) { FieldDB(value); }
    [[nodiscard]] std::uint8_t FieldDC() const { return read_u8(0xDC); }
    void FieldDC(std::uint8_t value) { write_u8(0xDC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d_c() const { return FieldDC(); }
    void field_d_c(std::uint8_t value) { FieldDC(value); }
    [[nodiscard]] std::uint8_t FieldDD() const { return read_u8(0xDD); }
    void FieldDD(std::uint8_t value) { write_u8(0xDD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d_d() const { return FieldDD(); }
    void field_d_d(std::uint8_t value) { FieldDD(value); }
    [[nodiscard]] std::uint16_t FieldDE() const { return read_u16(0xDE); }
    void FieldDE(std::uint16_t value) { write_u16(0xDE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_d_e() const { return FieldDE(); }
    void field_d_e(std::uint16_t value) { FieldDE(value); }
    [[nodiscard]] std::int32_t FieldE0() const { return read_i32(0xE0); }
    void FieldE0(std::int32_t value) { write_i32(0xE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e0() const { return FieldE0(); }
    void field_e0(std::int32_t value) { FieldE0(value); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x24;
    static constexpr std::size_t _off2 = 0x30;
    static constexpr std::size_t _off3 = 0x3C;
    static constexpr std::size_t _off4 = 0x40;
    static constexpr std::size_t _off5 = 0x44;
    static constexpr std::size_t _off6 = 0x48;
    static constexpr std::size_t _off7 = 0x67;
    static constexpr std::size_t _off8 = 0x68;
    static constexpr std::size_t _off9 = 0x7C;
    static constexpr std::size_t _off10 = 0x80;
    static constexpr std::size_t _off11 = 0xC8;
    static constexpr std::size_t _off12 = 0xCC;
    static constexpr std::size_t _off13 = 0xD0;
    static constexpr std::size_t _off14 = 0xD2;
    static constexpr std::size_t _off15 = 0xD4;
    static constexpr std::size_t _off16 = 0xD6;
    static constexpr std::size_t _off17 = 0xD8;
    static constexpr std::size_t _off18 = 0xD9;
    static constexpr std::size_t _off19 = 0xDA;
    static constexpr std::size_t _off20 = 0xDB;
    static constexpr std::size_t _off21 = 0xDC;
    static constexpr std::size_t _off22 = 0xDD;
    static constexpr std::size_t _off23 = 0xDE;
    static constexpr std::size_t _off24 = 0xE0;
    std::unique_ptr<::fruityprime::memory::LightInfo> light_info_;
    std::unique_ptr<::fruityprime::memory::EquipInfoPtr> equip_info_;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
};

class CPlayer : public CEntity {
public:
    CPlayer(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CPlayer(Buffer& buffer, std::uint32_t address);
    ~CPlayer() override;

    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x1C); }
    void Pos(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 PrevPos() const { return read_vec3(0x28); }
    void PrevPos(formats::Vector3 value) { write_vec3(0x28, value); }
    [[nodiscard]] formats::Vector3 prev_pos() const { return PrevPos(); }
    void prev_pos(formats::Vector3 value) { PrevPos(value); }
    [[nodiscard]] formats::Vector3 Speed() const { return read_vec3(0x34); }
    void Speed(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 speed() const { return Speed(); }
    void speed(formats::Vector3 value) { Speed(value); }
    [[nodiscard]] formats::Vector3 PrevSpeed() const { return read_vec3(0x40); }
    void PrevSpeed(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 prev_speed() const { return PrevSpeed(); }
    void prev_speed(formats::Vector3 value) { PrevSpeed(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x4C); }
    void Vec2(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 GunEffVec2() const { return read_vec3(0x58); }
    void GunEffVec2(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 gun_eff_vec2() const { return GunEffVec2(); }
    void gun_eff_vec2(formats::Vector3 value) { GunEffVec2(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x64); }
    void Vec1(formats::Vector3 value) { write_vec3(0x64, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Field70() const { return read_vec3(0x70); }
    void Field70(formats::Vector3 value) { write_vec3(0x70, value); }
    [[nodiscard]] formats::Vector3 field70() const { return Field70(); }
    void field70(formats::Vector3 value) { Field70(value); }
    [[nodiscard]] std::int32_t Field7C() const { return read_i32(0x7C); }
    void Field7C(std::int32_t value) { write_i32(0x7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c() const { return Field7C(); }
    void field7_c(std::int32_t value) { Field7C(value); }
    [[nodiscard]] std::int32_t Field80() const { return read_i32(0x80); }
    void Field80(std::int32_t value) { write_i32(0x80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field80() const { return Field80(); }
    void field80(std::int32_t value) { Field80(value); }
    [[nodiscard]] std::int32_t Field84() const { return read_i32(0x84); }
    void Field84(std::int32_t value) { write_i32(0x84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field84() const { return Field84(); }
    void field84(std::int32_t value) { Field84(value); }
    [[nodiscard]] std::int32_t Field88() const { return read_i32(0x88); }
    void Field88(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field88() const { return Field88(); }
    void field88(std::int32_t value) { Field88(value); }
    [[nodiscard]] std::int32_t Field8C() const { return read_i32(0x8C); }
    void Field8C(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field8_c() const { return Field8C(); }
    void field8_c(std::int32_t value) { Field8C(value); }
    [[nodiscard]] std::int32_t HSpeedMag() const { return read_i32(0x90); }
    void HSpeedMag(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t h_speed_mag() const { return HSpeedMag(); }
    void h_speed_mag(std::int32_t value) { HSpeedMag(value); }
    [[nodiscard]] std::int32_t Field94() const { return read_i32(0x94); }
    void Field94(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field94() const { return Field94(); }
    void field94(std::int32_t value) { Field94(value); }
    [[nodiscard]] std::int32_t GravityValue() const { return read_i32(0x98); }
    void GravityValue(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t gravity_value() const { return GravityValue(); }
    void gravity_value(std::int32_t value) { GravityValue(value); }
    [[nodiscard]] formats::Vector3 GunEffVec1() const { return read_vec3(0x9C); }
    void GunEffVec1(formats::Vector3 value) { write_vec3(0x9C, value); }
    [[nodiscard]] formats::Vector3 gun_eff_vec1() const { return GunEffVec1(); }
    void gun_eff_vec1(formats::Vector3 value) { GunEffVec1(value); }
    [[nodiscard]] formats::Vector3 AimTargetPos() const { return read_vec3(0xA8); }
    void AimTargetPos(formats::Vector3 value) { write_vec3(0xA8, value); }
    [[nodiscard]] formats::Vector3 aim_target_pos() const { return AimTargetPos(); }
    void aim_target_pos(formats::Vector3 value) { AimTargetPos(value); }
    [[nodiscard]] formats::Vector3 FieldB4() const { return read_vec3(0xB4); }
    void FieldB4(formats::Vector3 value) { write_vec3(0xB4, value); }
    [[nodiscard]] formats::Vector3 field_b4() const { return FieldB4(); }
    void field_b4(formats::Vector3 value) { FieldB4(value); }
    [[nodiscard]] formats::Vector3 FieldC0() const { return read_vec3(0xC0); }
    void FieldC0(formats::Vector3 value) { write_vec3(0xC0, value); }
    [[nodiscard]] formats::Vector3 field_c0() const { return FieldC0(); }
    void field_c0(formats::Vector3 value) { FieldC0(value); }
    [[nodiscard]] formats::Vector3 SomeSpeedLoss() const { return read_vec3(0xCC); }
    void SomeSpeedLoss(formats::Vector3 value) { write_vec3(0xCC, value); }
    [[nodiscard]] formats::Vector3 some_speed_loss() const { return SomeSpeedLoss(); }
    void some_speed_loss(formats::Vector3 value) { SomeSpeedLoss(value); }
    [[nodiscard]] std::uint16_t SomeSpeedCounter() const { return read_u16(0xD8); }
    void SomeSpeedCounter(std::uint16_t value) { write_u16(0xD8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t some_speed_counter() const { return SomeSpeedCounter(); }
    void some_speed_counter(std::uint16_t value) { SomeSpeedCounter(value); }
    [[nodiscard]] std::uint16_t Energy() const { return read_u16(0xDA); }
    void Energy(std::uint16_t value) { write_u16(0xDA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t energy() const { return Energy(); }
    void energy(std::uint16_t value) { Energy(value); }
    [[nodiscard]] std::uint16_t EnergyCap() const { return read_u16(0xDC); }
    void EnergyCap(std::uint16_t value) { write_u16(0xDC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t energy_cap() const { return EnergyCap(); }
    void energy_cap(std::uint16_t value) { EnergyCap(value); }
    [[nodiscard]] std::uint16_t RecoveryTicksMaybe() const { return read_u16(0xDE); }
    void RecoveryTicksMaybe(std::uint16_t value) { write_u16(0xDE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t recovery_ticks_maybe() const { return RecoveryTicksMaybe(); }
    void recovery_ticks_maybe(std::uint16_t value) { RecoveryTicksMaybe(value); }
    [[nodiscard]] std::uint8_t SomeTimer1() const { return read_u8(0xE0); }
    void SomeTimer1(std::uint8_t value) { write_u8(0xE0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t some_timer1() const { return SomeTimer1(); }
    void some_timer1(std::uint8_t value) { SomeTimer1(value); }
    [[nodiscard]] std::uint8_t SomeTimer2() const { return read_u8(0xE1); }
    void SomeTimer2(std::uint8_t value) { write_u8(0xE1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t some_timer2() const { return SomeTimer2(); }
    void some_timer2(std::uint8_t value) { SomeTimer2(value); }
    [[nodiscard]] std::uint8_t DeathCountdownMaybe() const { return read_u8(0xE2); }
    void DeathCountdownMaybe(std::uint8_t value) { write_u8(0xE2, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t death_countdown_maybe() const { return DeathCountdownMaybe(); }
    void death_countdown_maybe(std::uint8_t value) { DeathCountdownMaybe(value); }
    [[nodiscard]] std::uint8_t FieldE3() const { return read_u8(0xE3); }
    void FieldE3(std::uint8_t value) { write_u8(0xE3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_e3() const { return FieldE3(); }
    void field_e3(std::uint8_t value) { FieldE3(value); }
    [[nodiscard]] std::uint16_t FieldE4() const { return read_u16(0xE4); }
    void FieldE4(std::uint16_t value) { write_u16(0xE4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_e4() const { return FieldE4(); }
    void field_e4(std::uint16_t value) { FieldE4(value); }
    [[nodiscard]] std::uint16_t FieldE6() const { return read_u16(0xE6); }
    void FieldE6(std::uint16_t value) { write_u16(0xE6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_e6() const { return FieldE6(); }
    void field_e6(std::uint16_t value) { FieldE6(value); }
    [[nodiscard]] std::int32_t FieldE8() const { return read_i32(0xE8); }
    void FieldE8(std::int32_t value) { write_i32(0xE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e8() const { return FieldE8(); }
    void field_e8(std::int32_t value) { FieldE8(value); }
    [[nodiscard]] formats::Vector3 Dir2() const { return read_vec3(0xEC); }
    void Dir2(formats::Vector3 value) { write_vec3(0xEC, value); }
    [[nodiscard]] formats::Vector3 dir2() const { return Dir2(); }
    void dir2(formats::Vector3 value) { Dir2(value); }
    [[nodiscard]] std::uint16_t JumpPadCountdown() const { return read_u16(0xF8); }
    void JumpPadCountdown(std::uint16_t value) { write_u16(0xF8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t jump_pad_countdown() const { return JumpPadCountdown(); }
    void jump_pad_countdown(std::uint16_t value) { JumpPadCountdown(value); }
    [[nodiscard]] std::uint8_t JumpPadMin5s() const { return read_u8(0xFA); }
    void JumpPadMin5s(std::uint8_t value) { write_u8(0xFA, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t jump_pad_min5s() const { return JumpPadMin5s(); }
    void jump_pad_min5s(std::uint8_t value) { JumpPadMin5s(value); }
    [[nodiscard]] std::uint8_t TimeSinceJumpPadMaybe() const { return read_u8(0xFB); }
    void TimeSinceJumpPadMaybe(std::uint8_t value) { write_u8(0xFB, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_jump_pad_maybe() const { return TimeSinceJumpPadMaybe(); }
    void time_since_jump_pad_maybe(std::uint8_t value) { TimeSinceJumpPadMaybe(value); }
    [[nodiscard]] std::int32_t FieldFC() const { return read_i32(0xFC); }
    void FieldFC(std::int32_t value) { write_i32(0xFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_c() const { return FieldFC(); }
    void field_f_c(std::int32_t value) { FieldFC(value); }
    [[nodiscard]] UInt16Array& Field100() noexcept;
    [[nodiscard]] const UInt16Array& Field100() const noexcept;
    [[nodiscard]] UInt16Array& field100() noexcept { return Field100(); }
    [[nodiscard]] const UInt16Array& field100() const noexcept { return Field100(); }
    [[nodiscard]] std::uint8_t Field104() const { return read_u8(0x104); }
    void Field104(std::uint8_t value) { write_u8(0x104, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field104() const { return Field104(); }
    void field104(std::uint8_t value) { Field104(value); }
    [[nodiscard]] std::uint8_t Field105() const { return read_u8(0x105); }
    void Field105(std::uint8_t value) { write_u8(0x105, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field105() const { return Field105(); }
    void field105(std::uint8_t value) { Field105(value); }
    [[nodiscard]] std::uint16_t Field106() const { return read_u16(0x106); }
    void Field106(std::uint16_t value) { write_u16(0x106, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field106() const { return Field106(); }
    void field106(std::uint16_t value) { Field106(value); }
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& Collision() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& Collision() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CollisionVolume& collision() noexcept { return Collision(); }
    [[nodiscard]] const ::fruityprime::memory::CollisionVolume& collision() const noexcept { return Collision(); }
    [[nodiscard]] std::uint8_t BoostBallCharge() const { return read_u8(0x148); }
    void BoostBallCharge(std::uint8_t value) { write_u8(0x148, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t boost_ball_charge() const { return BoostBallCharge(); }
    void boost_ball_charge(std::uint8_t value) { BoostBallCharge(value); }
    [[nodiscard]] std::uint8_t SomeDamage() const { return read_u8(0x149); }
    void SomeDamage(std::uint8_t value) { write_u8(0x149, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t some_damage() const { return SomeDamage(); }
    void some_damage(std::uint8_t value) { SomeDamage(value); }
    [[nodiscard]] std::uint8_t BoostCooldown() const { return read_u8(0x14A); }
    void BoostCooldown(std::uint8_t value) { write_u8(0x14A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t boost_cooldown() const { return BoostCooldown(); }
    void boost_cooldown(std::uint8_t value) { BoostCooldown(value); }
    [[nodiscard]] std::uint8_t Field14B() const { return read_u8(0x14B); }
    void Field14B(std::uint8_t value) { write_u8(0x14B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field14_b() const { return Field14B(); }
    void field14_b(std::uint8_t value) { Field14B(value); }
    [[nodiscard]] std::uint16_t UniversalAmmo() const { return read_u16(0x14C); }
    void UniversalAmmo(std::uint16_t value) { write_u16(0x14C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t universal_ammo() const { return UniversalAmmo(); }
    void universal_ammo(std::uint16_t value) { UniversalAmmo(value); }
    [[nodiscard]] std::uint16_t Missiles() const { return read_u16(0x14E); }
    void Missiles(std::uint16_t value) { write_u16(0x14E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t missiles() const { return Missiles(); }
    void missiles(std::uint16_t value) { Missiles(value); }
    [[nodiscard]] std::uint16_t UniversalAmmoCap() const { return read_u16(0x150); }
    void UniversalAmmoCap(std::uint16_t value) { write_u16(0x150, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t universal_ammo_cap() const { return UniversalAmmoCap(); }
    void universal_ammo_cap(std::uint16_t value) { UniversalAmmoCap(value); }
    [[nodiscard]] std::uint16_t MissilesCap() const { return read_u16(0x152); }
    void MissilesCap(std::uint16_t value) { write_u16(0x152, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t missiles_cap() const { return MissilesCap(); }
    void missiles_cap(std::uint16_t value) { MissilesCap(value); }
    [[nodiscard]] std::uint8_t Field154() const { return read_u8(0x154); }
    void Field154(std::uint8_t value) { write_u8(0x154, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field154() const { return Field154(); }
    void field154(std::uint8_t value) { Field154(value); }
    [[nodiscard]] std::uint8_t Field155() const { return read_u8(0x155); }
    void Field155(std::uint8_t value) { write_u8(0x155, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field155() const { return Field155(); }
    void field155(std::uint8_t value) { Field155(value); }
    [[nodiscard]] std::uint16_t Field156() const { return read_u16(0x156); }
    void Field156(std::uint16_t value) { write_u16(0x156, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field156() const { return Field156(); }
    void field156(std::uint16_t value) { Field156(value); }
    [[nodiscard]] std::uint8_t WeaponSlot0() const { return read_u8(0x158); }
    void WeaponSlot0(std::uint8_t value) { write_u8(0x158, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t weapon_slot0() const { return WeaponSlot0(); }
    void weapon_slot0(std::uint8_t value) { WeaponSlot0(value); }
    [[nodiscard]] std::uint8_t WeaponSlot1() const { return read_u8(0x159); }
    void WeaponSlot1(std::uint8_t value) { write_u8(0x159, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t weapon_slot1() const { return WeaponSlot1(); }
    void weapon_slot1(std::uint8_t value) { WeaponSlot1(value); }
    [[nodiscard]] std::uint8_t WeaponSlot2() const { return read_u8(0x15A); }
    void WeaponSlot2(std::uint8_t value) { write_u8(0x15A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t weapon_slot2() const { return WeaponSlot2(); }
    void weapon_slot2(std::uint8_t value) { WeaponSlot2(value); }
    [[nodiscard]] std::uint8_t GunAnimation() const { return read_u8(0x15B); }
    void GunAnimation(std::uint8_t value) { write_u8(0x15B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t gun_animation() const { return GunAnimation(); }
    void gun_animation(std::uint8_t value) { GunAnimation(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& GunModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& GunModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& gun_model() noexcept { return GunModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& gun_model() const noexcept { return GunModel(); }
    [[nodiscard]] ::fruityprime::memory::CModel& FrozenModel() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& FrozenModel() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& frozen_model() noexcept { return FrozenModel(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& frozen_model() const noexcept { return FrozenModel(); }
    [[nodiscard]] formats::Vector3 Field1EC() const { return read_vec3(0x1EC); }
    void Field1EC(formats::Vector3 value) { write_vec3(0x1EC, value); }
    [[nodiscard]] formats::Vector3 field1_e_c() const { return Field1EC(); }
    void field1_e_c(formats::Vector3 value) { Field1EC(value); }
    [[nodiscard]] formats::Vector3 MuzzlePos() const { return read_vec3(0x1F8); }
    void MuzzlePos(formats::Vector3 value) { write_vec3(0x1F8, value); }
    [[nodiscard]] formats::Vector3 muzzle_pos() const { return MuzzlePos(); }
    void muzzle_pos(formats::Vector3 value) { MuzzlePos(value); }
    [[nodiscard]] std::uint32_t FurlEffect() const { return read_pointer(0x204); }
    void FurlEffect(std::uint32_t value) { write_pointer(0x204, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t furl_effect() const { return FurlEffect(); }
    void furl_effect(std::uint32_t value) { FurlEffect(value); }
    [[nodiscard]] std::uint32_t EffectBoost() const { return read_pointer(0x208); }
    void EffectBoost(std::uint32_t value) { write_pointer(0x208, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_boost() const { return EffectBoost(); }
    void effect_boost(std::uint32_t value) { EffectBoost(value); }
    [[nodiscard]] std::uint32_t EffectMuzzle() const { return read_pointer(0x20C); }
    void EffectMuzzle(std::uint32_t value) { write_pointer(0x20C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_muzzle() const { return EffectMuzzle(); }
    void effect_muzzle(std::uint32_t value) { EffectMuzzle(value); }
    [[nodiscard]] std::uint32_t EffectCharge() const { return read_pointer(0x210); }
    void EffectCharge(std::uint32_t value) { write_pointer(0x210, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_charge() const { return EffectCharge(); }
    void effect_charge(std::uint32_t value) { EffectCharge(value); }
    [[nodiscard]] std::uint32_t EffectDblDmg() const { return read_pointer(0x214); }
    void EffectDblDmg(std::uint32_t value) { write_pointer(0x214, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_dbl_dmg() const { return EffectDblDmg(); }
    void effect_dbl_dmg(std::uint32_t value) { EffectDblDmg(value); }
    [[nodiscard]] std::uint32_t EffectDeathalt() const { return read_pointer(0x218); }
    void EffectDeathalt(std::uint32_t value) { write_pointer(0x218, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_deathalt() const { return EffectDeathalt(); }
    void effect_deathalt(std::uint32_t value) { EffectDeathalt(value); }
    [[nodiscard]] IntPtrArray& SpineNode() noexcept;
    [[nodiscard]] const IntPtrArray& SpineNode() const noexcept;
    [[nodiscard]] IntPtrArray& spine_node() noexcept { return SpineNode(); }
    [[nodiscard]] const IntPtrArray& spine_node() const noexcept { return SpineNode(); }
    [[nodiscard]] IntPtrArray& ShootNode() noexcept;
    [[nodiscard]] const IntPtrArray& ShootNode() const noexcept;
    [[nodiscard]] IntPtrArray& shoot_node() noexcept { return ShootNode(); }
    [[nodiscard]] const IntPtrArray& shoot_node() const noexcept { return ShootNode(); }
    [[nodiscard]] ::fruityprime::memory::CModel& Biped1() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Biped1() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& biped1() noexcept { return Biped1(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& biped1() const noexcept { return Biped1(); }
    [[nodiscard]] ::fruityprime::memory::CModel& Biped2() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Biped2() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& biped2() noexcept { return Biped2(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& biped2() const noexcept { return Biped2(); }
    [[nodiscard]] std::uint16_t Field2BC() const { return read_u16(0x2BC); }
    void Field2BC(std::uint16_t value) { write_u16(0x2BC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_b_c() const { return Field2BC(); }
    void field2_b_c(std::uint16_t value) { Field2BC(value); }
    [[nodiscard]] std::uint16_t Field2BE() const { return read_u16(0x2BE); }
    void Field2BE(std::uint16_t value) { write_u16(0x2BE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_b_e() const { return Field2BE(); }
    void field2_b_e(std::uint16_t value) { Field2BE(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& AltForm() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& AltForm() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& alt_form() noexcept { return AltForm(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& alt_form() const noexcept { return AltForm(); }
    [[nodiscard]] ::fruityprime::memory::CModel& GunSmoke() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& GunSmoke() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& gun_smoke() noexcept { return GunSmoke(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& gun_smoke() const noexcept { return GunSmoke(); }
    [[nodiscard]] std::uint8_t SmokeAlphaMaybe() const { return read_u8(0x350); }
    void SmokeAlphaMaybe(std::uint8_t value) { write_u8(0x350, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t smoke_alpha_maybe() const { return SmokeAlphaMaybe(); }
    void smoke_alpha_maybe(std::uint8_t value) { SmokeAlphaMaybe(value); }
    [[nodiscard]] std::uint8_t Field351() const { return read_u8(0x351); }
    void Field351(std::uint8_t value) { write_u8(0x351, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field351() const { return Field351(); }
    void field351(std::uint8_t value) { Field351(value); }
    [[nodiscard]] std::uint16_t Field352() const { return read_u16(0x352); }
    void Field352(std::uint16_t value) { write_u16(0x352, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field352() const { return Field352(); }
    void field352(std::uint16_t value) { Field352(value); }
    [[nodiscard]] std::uint32_t Field354() const { return read_pointer(0x354); }
    void Field354(std::uint32_t value) { write_pointer(0x354, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field354() const { return Field354(); }
    void field354(std::uint32_t value) { Field354(value); }
    [[nodiscard]] std::uint32_t AttachedEnemy() const { return read_pointer(0x358); }
    void AttachedEnemy(std::uint32_t value) { write_pointer(0x358, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t attached_enemy() const { return AttachedEnemy(); }
    void attached_enemy(std::uint32_t value) { AttachedEnemy(value); }
    [[nodiscard]] std::uint32_t Field35C() const { return read_pointer(0x35C); }
    void Field35C(std::uint32_t value) { write_pointer(0x35C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field35_c() const { return Field35C(); }
    void field35_c(std::uint32_t value) { Field35C(value); }
    [[nodiscard]] std::uint8_t Field360() const { return read_u8(0x360); }
    void Field360(std::uint8_t value) { write_u8(0x360, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field360() const { return Field360(); }
    void field360(std::uint8_t value) { Field360(value); }
    [[nodiscard]] std::uint8_t Field361() const { return read_u8(0x361); }
    void Field361(std::uint8_t value) { write_u8(0x361, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field361() const { return Field361(); }
    void field361(std::uint8_t value) { Field361(value); }
    [[nodiscard]] std::uint16_t Field362() const { return read_u16(0x362); }
    void Field362(std::uint16_t value) { write_u16(0x362, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field362() const { return Field362(); }
    void field362(std::uint16_t value) { Field362(value); }
    [[nodiscard]] ::fruityprime::memory::PlayerControls& Controls() noexcept;
    [[nodiscard]] const ::fruityprime::memory::PlayerControls& Controls() const noexcept;
    [[nodiscard]] ::fruityprime::memory::PlayerControls& controls() noexcept { return Controls(); }
    [[nodiscard]] const ::fruityprime::memory::PlayerControls& controls() const noexcept { return Controls(); }
    [[nodiscard]] formats::Hunter HunterId() const { return static_cast<formats::Hunter>(read_u8(0x400)); }
    void HunterId(formats::Hunter value) { write_u8(0x400, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::Hunter hunter_id() const { return HunterId(); }
    void hunter_id(formats::Hunter value) { HunterId(value); }
    [[nodiscard]] std::uint8_t Field401() const { return read_u8(0x401); }
    void Field401(std::uint8_t value) { write_u8(0x401, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field401() const { return Field401(); }
    void field401(std::uint8_t value) { Field401(value); }
    [[nodiscard]] std::uint16_t Field402() const { return read_u16(0x402); }
    void Field402(std::uint16_t value) { write_u16(0x402, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field402() const { return Field402(); }
    void field402(std::uint16_t value) { Field402(value); }
    [[nodiscard]] std::uint32_t Field404() const { return read_pointer(0x404); }
    void Field404(std::uint32_t value) { write_pointer(0x404, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field404() const { return Field404(); }
    void field404(std::uint32_t value) { Field404(value); }
    [[nodiscard]] std::int32_t Field408() const { return read_i32(0x408); }
    void Field408(std::int32_t value) { write_i32(0x408, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field408() const { return Field408(); }
    void field408(std::int32_t value) { Field408(value); }
    [[nodiscard]] std::int32_t Field40C() const { return read_i32(0x40C); }
    void Field40C(std::int32_t value) { write_i32(0x40C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40_c() const { return Field40C(); }
    void field40_c(std::int32_t value) { Field40C(value); }
    [[nodiscard]] std::int32_t Field410() const { return read_i32(0x410); }
    void Field410(std::int32_t value) { write_i32(0x410, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field410() const { return Field410(); }
    void field410(std::int32_t value) { Field410(value); }
    [[nodiscard]] std::int32_t Field414() const { return read_i32(0x414); }
    void Field414(std::int32_t value) { write_i32(0x414, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field414() const { return Field414(); }
    void field414(std::int32_t value) { Field414(value); }
    [[nodiscard]] std::int32_t Field418() const { return read_i32(0x418); }
    void Field418(std::int32_t value) { write_i32(0x418, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field418() const { return Field418(); }
    void field418(std::int32_t value) { Field418(value); }
    [[nodiscard]] std::int32_t Field41C() const { return read_i32(0x41C); }
    void Field41C(std::int32_t value) { write_i32(0x41C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field41_c() const { return Field41C(); }
    void field41_c(std::int32_t value) { Field41C(value); }
    [[nodiscard]] std::int32_t Field420() const { return read_i32(0x420); }
    void Field420(std::int32_t value) { write_i32(0x420, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field420() const { return Field420(); }
    void field420(std::int32_t value) { Field420(value); }
    [[nodiscard]] std::int32_t Field424() const { return read_i32(0x424); }
    void Field424(std::int32_t value) { write_i32(0x424, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field424() const { return Field424(); }
    void field424(std::int32_t value) { Field424(value); }
    [[nodiscard]] std::int32_t Field428() const { return read_i32(0x428); }
    void Field428(std::int32_t value) { write_i32(0x428, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field428() const { return Field428(); }
    void field428(std::int32_t value) { Field428(value); }
    [[nodiscard]] std::int32_t Field42C() const { return read_i32(0x42C); }
    void Field42C(std::int32_t value) { write_i32(0x42C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field42_c() const { return Field42C(); }
    void field42_c(std::int32_t value) { Field42C(value); }
    [[nodiscard]] std::int32_t Field430() const { return read_i32(0x430); }
    void Field430(std::int32_t value) { write_i32(0x430, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field430() const { return Field430(); }
    void field430(std::int32_t value) { Field430(value); }
    [[nodiscard]] std::uint8_t TimeSinceShot() const { return read_u8(0x434); }
    void TimeSinceShot(std::uint8_t value) { write_u8(0x434, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_shot() const { return TimeSinceShot(); }
    void time_since_shot(std::uint8_t value) { TimeSinceShot(value); }
    [[nodiscard]] std::uint8_t Field435() const { return read_u8(0x435); }
    void Field435(std::uint8_t value) { write_u8(0x435, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field435() const { return Field435(); }
    void field435(std::uint8_t value) { Field435(value); }
    [[nodiscard]] std::uint16_t Field436() const { return read_u16(0x436); }
    void Field436(std::uint16_t value) { write_u16(0x436, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field436() const { return Field436(); }
    void field436(std::uint16_t value) { Field436(value); }
    [[nodiscard]] std::uint16_t Field438() const { return read_u16(0x438); }
    void Field438(std::uint16_t value) { write_u16(0x438, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field438() const { return Field438(); }
    void field438(std::uint16_t value) { Field438(value); }
    [[nodiscard]] std::uint16_t Field43A() const { return read_u16(0x43A); }
    void Field43A(std::uint16_t value) { write_u16(0x43A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field43_a() const { return Field43A(); }
    void field43_a(std::uint16_t value) { Field43A(value); }
    [[nodiscard]] std::uint16_t ShockCoilTimer() const { return read_u16(0x43C); }
    void ShockCoilTimer(std::uint16_t value) { write_u16(0x43C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t shock_coil_timer() const { return ShockCoilTimer(); }
    void shock_coil_timer(std::uint16_t value) { ShockCoilTimer(value); }
    [[nodiscard]] std::uint16_t Field43E() const { return read_u16(0x43E); }
    void Field43E(std::uint16_t value) { write_u16(0x43E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field43_e() const { return Field43E(); }
    void field43_e(std::uint16_t value) { Field43E(value); }
    [[nodiscard]] std::uint32_t ShockCoilTarget() const { return read_pointer(0x440); }
    void ShockCoilTarget(std::uint32_t value) { write_pointer(0x440, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shock_coil_target() const { return ShockCoilTarget(); }
    void shock_coil_target(std::uint32_t value) { ShockCoilTarget(value); }
    [[nodiscard]] std::uint8_t TimeSinceDmg() const { return read_u8(0x444); }
    void TimeSinceDmg(std::uint8_t value) { write_u8(0x444, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_dmg() const { return TimeSinceDmg(); }
    void time_since_dmg(std::uint8_t value) { TimeSinceDmg(value); }
    [[nodiscard]] std::uint8_t TimeSincePickup() const { return read_u8(0x445); }
    void TimeSincePickup(std::uint8_t value) { write_u8(0x445, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_pickup() const { return TimeSincePickup(); }
    void time_since_pickup(std::uint8_t value) { TimeSincePickup(value); }
    [[nodiscard]] std::uint8_t TimeSinceHeal() const { return read_u8(0x446); }
    void TimeSinceHeal(std::uint8_t value) { write_u8(0x446, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_heal() const { return TimeSinceHeal(); }
    void time_since_heal(std::uint8_t value) { TimeSinceHeal(value); }
    [[nodiscard]] std::uint8_t Field447() const { return read_u8(0x447); }
    void Field447(std::uint8_t value) { write_u8(0x447, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field447() const { return Field447(); }
    void field447(std::uint8_t value) { Field447(value); }
    [[nodiscard]] std::uint8_t Field448() const { return read_u8(0x448); }
    void Field448(std::uint8_t value) { write_u8(0x448, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field448() const { return Field448(); }
    void field448(std::uint8_t value) { Field448(value); }
    [[nodiscard]] std::uint8_t Field449() const { return read_u8(0x449); }
    void Field449(std::uint8_t value) { write_u8(0x449, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field449() const { return Field449(); }
    void field449(std::uint8_t value) { Field449(value); }
    [[nodiscard]] std::uint16_t Field44A() const { return read_u16(0x44A); }
    void Field44A(std::uint16_t value) { write_u16(0x44A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field44_a() const { return Field44A(); }
    void field44_a(std::uint16_t value) { Field44A(value); }
    [[nodiscard]] std::int32_t Field44C() const { return read_i32(0x44C); }
    void Field44C(std::int32_t value) { write_i32(0x44C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field44_c() const { return Field44C(); }
    void field44_c(std::int32_t value) { Field44C(value); }
    [[nodiscard]] std::int32_t Field450() const { return read_i32(0x450); }
    void Field450(std::int32_t value) { write_i32(0x450, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field450() const { return Field450(); }
    void field450(std::int32_t value) { Field450(value); }
    [[nodiscard]] std::uint32_t RoomNodeRef() const { return read_pointer(0x454); }
    void RoomNodeRef(std::uint32_t value) { write_pointer(0x454, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t room_node_ref() const { return RoomNodeRef(); }
    void room_node_ref(std::uint32_t value) { RoomNodeRef(value); }
    [[nodiscard]] std::uint16_t RespawnTimer() const { return read_u16(0x458); }
    void RespawnTimer(std::uint16_t value) { write_u16(0x458, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t respawn_timer() const { return RespawnTimer(); }
    void respawn_timer(std::uint16_t value) { RespawnTimer(value); }
    [[nodiscard]] std::uint16_t Field45A() const { return read_u16(0x45A); }
    void Field45A(std::uint16_t value) { write_u16(0x45A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field45_a() const { return Field45A(); }
    void field45_a(std::uint16_t value) { Field45A(value); }
    [[nodiscard]] std::uint32_t Field45C() const { return read_pointer(0x45C); }
    void Field45C(std::uint32_t value) { write_pointer(0x45C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field45_c() const { return Field45C(); }
    void field45_c(std::uint32_t value) { Field45C(value); }
    [[nodiscard]] std::uint16_t Field460() const { return read_u16(0x460); }
    void Field460(std::uint16_t value) { write_u16(0x460, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field460() const { return Field460(); }
    void field460(std::uint16_t value) { Field460(value); }
    [[nodiscard]] std::uint16_t Field462() const { return read_u16(0x462); }
    void Field462(std::uint16_t value) { write_u16(0x462, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field462() const { return Field462(); }
    void field462(std::uint16_t value) { Field462(value); }
    [[nodiscard]] ::fruityprime::memory::PlayerInput& Input() noexcept;
    [[nodiscard]] const ::fruityprime::memory::PlayerInput& Input() const noexcept;
    [[nodiscard]] ::fruityprime::memory::PlayerInput& input() noexcept { return Input(); }
    [[nodiscard]] const ::fruityprime::memory::PlayerInput& input() const noexcept { return Input(); }
    [[nodiscard]] std::uint8_t Field4AC() const { return read_u8(0x4AC); }
    void Field4AC(std::uint8_t value) { write_u8(0x4AC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_a_c() const { return Field4AC(); }
    void field4_a_c(std::uint8_t value) { Field4AC(value); }
    [[nodiscard]] std::uint8_t TeamIndex() const { return read_u8(0x4AD); }
    void TeamIndex(std::uint8_t value) { write_u8(0x4AD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t team_index() const { return TeamIndex(); }
    void team_index(std::uint8_t value) { TeamIndex(value); }
    [[nodiscard]] std::uint8_t Field4AE() const { return read_u8(0x4AE); }
    void Field4AE(std::uint8_t value) { write_u8(0x4AE, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_a_e() const { return Field4AE(); }
    void field4_a_e(std::uint8_t value) { Field4AE(value); }
    [[nodiscard]] std::uint8_t Field4AF() const { return read_u8(0x4AF); }
    void Field4AF(std::uint8_t value) { write_u8(0x4AF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_a_f() const { return Field4AF(); }
    void field4_a_f(std::uint8_t value) { Field4AF(value); }
    [[nodiscard]] std::uint16_t DoubleDamageTimer() const { return read_u16(0x4B0); }
    void DoubleDamageTimer(std::uint16_t value) { write_u16(0x4B0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t double_damage_timer() const { return DoubleDamageTimer(); }
    void double_damage_timer(std::uint16_t value) { DoubleDamageTimer(value); }
    [[nodiscard]] std::uint16_t CloakTimer() const { return read_u16(0x4B2); }
    void CloakTimer(std::uint16_t value) { write_u16(0x4B2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cloak_timer() const { return CloakTimer(); }
    void cloak_timer(std::uint16_t value) { CloakTimer(value); }
    [[nodiscard]] std::uint16_t DeathaltTimer() const { return read_u16(0x4B4); }
    void DeathaltTimer(std::uint16_t value) { write_u16(0x4B4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t deathalt_timer() const { return DeathaltTimer(); }
    void deathalt_timer(std::uint16_t value) { DeathaltTimer(value); }
    [[nodiscard]] std::uint16_t DisruptTimer() const { return read_u16(0x4B6); }
    void DisruptTimer(std::uint16_t value) { write_u16(0x4B6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t disrupt_timer() const { return DisruptTimer(); }
    void disrupt_timer(std::uint16_t value) { DisruptTimer(value); }
    [[nodiscard]] std::uint16_t FreezeTimer() const { return read_u16(0x4B8); }
    void FreezeTimer(std::uint16_t value) { write_u16(0x4B8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t freeze_timer() const { return FreezeTimer(); }
    void freeze_timer(std::uint16_t value) { FreezeTimer(value); }
    [[nodiscard]] std::uint8_t TimeSinceFrozen() const { return read_u8(0x4BA); }
    void TimeSinceFrozen(std::uint8_t value) { write_u8(0x4BA, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t time_since_frozen() const { return TimeSinceFrozen(); }
    void time_since_frozen(std::uint8_t value) { TimeSinceFrozen(value); }
    [[nodiscard]] std::uint8_t Frozen() const { return read_u8(0x4BB); }
    void Frozen(std::uint8_t value) { write_u8(0x4BB, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t frozen() const { return Frozen(); }
    void frozen(std::uint8_t value) { Frozen(value); }
    [[nodiscard]] std::uint8_t CurAlpha() const { return read_u8(0x4BC); }
    void CurAlpha(std::uint8_t value) { write_u8(0x4BC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t cur_alpha() const { return CurAlpha(); }
    void cur_alpha(std::uint8_t value) { CurAlpha(value); }
    [[nodiscard]] std::uint8_t TargetAlpha() const { return read_u8(0x4BD); }
    void TargetAlpha(std::uint8_t value) { write_u8(0x4BD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t target_alpha() const { return TargetAlpha(); }
    void target_alpha(std::uint8_t value) { TargetAlpha(value); }
    [[nodiscard]] std::uint8_t ShotCooldownRelated() const { return read_u8(0x4BE); }
    void ShotCooldownRelated(std::uint8_t value) { write_u8(0x4BE, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t shot_cooldown_related() const { return ShotCooldownRelated(); }
    void shot_cooldown_related(std::uint8_t value) { ShotCooldownRelated(value); }
    [[nodiscard]] std::uint8_t Field4BF() const { return read_u8(0x4BF); }
    void Field4BF(std::uint8_t value) { write_u8(0x4BF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_b_f() const { return Field4BF(); }
    void field4_b_f(std::uint8_t value) { Field4BF(value); }
    [[nodiscard]] std::uint8_t Field4C0() const { return read_u8(0x4C0); }
    void Field4C0(std::uint8_t value) { write_u8(0x4C0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_c0() const { return Field4C0(); }
    void field4_c0(std::uint8_t value) { Field4C0(value); }
    [[nodiscard]] std::uint8_t Field4C1() const { return read_u8(0x4C1); }
    void Field4C1(std::uint8_t value) { write_u8(0x4C1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_c1() const { return Field4C1(); }
    void field4_c1(std::uint8_t value) { Field4C1(value); }
    [[nodiscard]] std::uint16_t Field4C2() const { return read_u16(0x4C2); }
    void Field4C2(std::uint16_t value) { write_u16(0x4C2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_c2() const { return Field4C2(); }
    void field4_c2(std::uint16_t value) { Field4C2(value); }
    [[nodiscard]] std::uint32_t SomeFlags() const { return read_u32(0x4C4); }
    void SomeFlags(std::uint32_t value) { write_u32(0x4C4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t some_flags() const { return SomeFlags(); }
    void some_flags(std::uint32_t value) { SomeFlags(value); }
    [[nodiscard]] std::uint32_t MoreFlags() const { return read_u32(0x4C8); }
    void MoreFlags(std::uint32_t value) { write_u32(0x4C8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t more_flags() const { return MoreFlags(); }
    void more_flags(std::uint32_t value) { MoreFlags(value); }
    [[nodiscard]] std::uint16_t AbilityFlags() const { return read_u16(0x4CC); }
    void AbilityFlags(std::uint16_t value) { write_u16(0x4CC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t ability_flags() const { return AbilityFlags(); }
    void ability_flags(std::uint16_t value) { AbilityFlags(value); }
    [[nodiscard]] std::uint8_t CurrentWeapon() const { return read_u8(0x4CE); }
    void CurrentWeapon(std::uint8_t value) { write_u8(0x4CE, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t current_weapon() const { return CurrentWeapon(); }
    void current_weapon(std::uint8_t value) { CurrentWeapon(value); }
    [[nodiscard]] std::uint8_t WeaponSelection() const { return read_u8(0x4CF); }
    void WeaponSelection(std::uint8_t value) { write_u8(0x4CF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t weapon_selection() const { return WeaponSelection(); }
    void weapon_selection(std::uint8_t value) { WeaponSelection(value); }
    [[nodiscard]] std::uint8_t SomeWeapon() const { return read_u8(0x4D0); }
    void SomeWeapon(std::uint8_t value) { write_u8(0x4D0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t some_weapon() const { return SomeWeapon(); }
    void some_weapon(std::uint8_t value) { SomeWeapon(value); }
    [[nodiscard]] std::uint8_t Field4D1() const { return read_u8(0x4D1); }
    void Field4D1(std::uint8_t value) { write_u8(0x4D1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_d1() const { return Field4D1(); }
    void field4_d1(std::uint8_t value) { Field4D1(value); }
    [[nodiscard]] std::uint8_t AvailableWeapons() const { return read_u8(0x4D2); }
    void AvailableWeapons(std::uint8_t value) { write_u8(0x4D2, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t available_weapons() const { return AvailableWeapons(); }
    void available_weapons(std::uint8_t value) { AvailableWeapons(value); }
    [[nodiscard]] std::uint8_t OmegaCannon() const { return read_u8(0x4D3); }
    void OmegaCannon(std::uint8_t value) { write_u8(0x4D3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t omega_cannon() const { return OmegaCannon(); }
    void omega_cannon(std::uint8_t value) { OmegaCannon(value); }
    [[nodiscard]] std::uint8_t AvailableCharges() const { return read_u8(0x4D4); }
    void AvailableCharges(std::uint8_t value) { write_u8(0x4D4, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t available_charges() const { return AvailableCharges(); }
    void available_charges(std::uint8_t value) { AvailableCharges(value); }
    [[nodiscard]] std::uint8_t Field4D5() const { return read_u8(0x4D5); }
    void Field4D5(std::uint8_t value) { write_u8(0x4D5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_d5() const { return Field4D5(); }
    void field4_d5(std::uint8_t value) { Field4D5(value); }
    [[nodiscard]] std::uint8_t ViewType() const { return read_u8(0x4D6); }
    void ViewType(std::uint8_t value) { write_u8(0x4D6, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t view_type() const { return ViewType(); }
    void view_type(std::uint8_t value) { ViewType(value); }
    [[nodiscard]] std::uint8_t ViewPlayer() const { return read_u8(0x4D7); }
    void ViewPlayer(std::uint8_t value) { write_u8(0x4D7, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t view_player() const { return ViewPlayer(); }
    void view_player(std::uint8_t value) { ViewPlayer(value); }
    [[nodiscard]] std::uint8_t Field4D8() const { return read_u8(0x4D8); }
    void Field4D8(std::uint8_t value) { write_u8(0x4D8, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_d8() const { return Field4D8(); }
    void field4_d8(std::uint8_t value) { Field4D8(value); }
    [[nodiscard]] std::uint8_t Field4D9() const { return read_u8(0x4D9); }
    void Field4D9(std::uint8_t value) { write_u8(0x4D9, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4_d9() const { return Field4D9(); }
    void field4_d9(std::uint8_t value) { Field4D9(value); }
    [[nodiscard]] std::uint16_t Field4DA() const { return read_u16(0x4DA); }
    void Field4DA(std::uint16_t value) { write_u16(0x4DA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4_d_a() const { return Field4DA(); }
    void field4_d_a(std::uint16_t value) { Field4DA(value); }
    [[nodiscard]] std::int32_t Field4DC() const { return read_i32(0x4DC); }
    void Field4DC(std::int32_t value) { write_i32(0x4DC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_d_c() const { return Field4DC(); }
    void field4_d_c(std::int32_t value) { Field4DC(value); }
    [[nodiscard]] std::int32_t Field4E0() const { return read_i32(0x4E0); }
    void Field4E0(std::int32_t value) { write_i32(0x4E0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_e0() const { return Field4E0(); }
    void field4_e0(std::int32_t value) { Field4E0(value); }
    [[nodiscard]] std::int32_t Field4E4() const { return read_i32(0x4E4); }
    void Field4E4(std::int32_t value) { write_i32(0x4E4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_e4() const { return Field4E4(); }
    void field4_e4(std::int32_t value) { Field4E4(value); }
    [[nodiscard]] formats::Vector3 Field4E8() const { return read_vec3(0x4E8); }
    void Field4E8(formats::Vector3 value) { write_vec3(0x4E8, value); }
    [[nodiscard]] formats::Vector3 field4_e8() const { return Field4E8(); }
    void field4_e8(formats::Vector3 value) { Field4E8(value); }
    [[nodiscard]] formats::Matrix4x3 Transform() const { return read_mtx43(0x4F4); }
    void Transform(formats::Matrix4x3 value) { write_mtx43(0x4F4, value); }
    [[nodiscard]] formats::Matrix4x3 transform() const { return Transform(); }
    void transform(formats::Matrix4x3 value) { Transform(value); }
    [[nodiscard]] std::int32_t Field524() const { return read_i32(0x524); }
    void Field524(std::int32_t value) { write_i32(0x524, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field524() const { return Field524(); }
    void field524(std::int32_t value) { Field524(value); }
    [[nodiscard]] std::int32_t Field528() const { return read_i32(0x528); }
    void Field528(std::int32_t value) { write_i32(0x528, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field528() const { return Field528(); }
    void field528(std::int32_t value) { Field528(value); }
    [[nodiscard]] std::int32_t Field52C() const { return read_i32(0x52C); }
    void Field52C(std::int32_t value) { write_i32(0x52C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field52_c() const { return Field52C(); }
    void field52_c(std::int32_t value) { Field52C(value); }
    [[nodiscard]] std::int32_t Field530() const { return read_i32(0x530); }
    void Field530(std::int32_t value) { write_i32(0x530, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field530() const { return Field530(); }
    void field530(std::int32_t value) { Field530(value); }
    [[nodiscard]] std::int32_t Field534() const { return read_i32(0x534); }
    void Field534(std::int32_t value) { write_i32(0x534, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field534() const { return Field534(); }
    void field534(std::int32_t value) { Field534(value); }
    [[nodiscard]] std::int32_t Field538() const { return read_i32(0x538); }
    void Field538(std::int32_t value) { write_i32(0x538, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field538() const { return Field538(); }
    void field538(std::int32_t value) { Field538(value); }
    [[nodiscard]] std::uint8_t BombTimer() const { return read_u8(0x53C); }
    void BombTimer(std::uint8_t value) { write_u8(0x53C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t bomb_timer() const { return BombTimer(); }
    void bomb_timer(std::uint8_t value) { BombTimer(value); }
    [[nodiscard]] std::uint8_t BombAmount() const { return read_u8(0x53D); }
    void BombAmount(std::uint8_t value) { write_u8(0x53D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t bomb_amount() const { return BombAmount(); }
    void bomb_amount(std::uint8_t value) { BombAmount(value); }
    [[nodiscard]] std::uint8_t Field53E() const { return read_u8(0x53E); }
    void Field53E(std::uint8_t value) { write_u8(0x53E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field53_e() const { return Field53E(); }
    void field53_e(std::uint8_t value) { Field53E(value); }
    [[nodiscard]] std::uint8_t Field53F() const { return read_u8(0x53F); }
    void Field53F(std::uint8_t value) { write_u8(0x53F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field53_f() const { return Field53F(); }
    void field53_f(std::uint8_t value) { Field53F(value); }
    [[nodiscard]] std::uint8_t Field540() const { return read_u8(0x540); }
    void Field540(std::uint8_t value) { write_u8(0x540, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field540() const { return Field540(); }
    void field540(std::uint8_t value) { Field540(value); }
    [[nodiscard]] std::uint8_t Field541() const { return read_u8(0x541); }
    void Field541(std::uint8_t value) { write_u8(0x541, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field541() const { return Field541(); }
    void field541(std::uint8_t value) { Field541(value); }
    [[nodiscard]] std::uint16_t Field542() const { return read_u16(0x542); }
    void Field542(std::uint16_t value) { write_u16(0x542, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field542() const { return Field542(); }
    void field542(std::uint16_t value) { Field542(value); }
    [[nodiscard]] formats::Vector3 Field544() const { return read_vec3(0x544); }
    void Field544(formats::Vector3 value) { write_vec3(0x544, value); }
    [[nodiscard]] formats::Vector3 field544() const { return Field544(); }
    void field544(formats::Vector3 value) { Field544(value); }
    [[nodiscard]] std::uint8_t GunIdleMaybe() const { return read_u8(0x550); }
    void GunIdleMaybe(std::uint8_t value) { write_u8(0x550, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t gun_idle_maybe() const { return GunIdleMaybe(); }
    void gun_idle_maybe(std::uint8_t value) { GunIdleMaybe(value); }
    [[nodiscard]] std::uint8_t Field551() const { return read_u8(0x551); }
    void Field551(std::uint8_t value) { write_u8(0x551, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field551() const { return Field551(); }
    void field551(std::uint8_t value) { Field551(value); }
    [[nodiscard]] std::uint8_t Field552() const { return read_u8(0x552); }
    void Field552(std::uint8_t value) { write_u8(0x552, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field552() const { return Field552(); }
    void field552(std::uint8_t value) { Field552(value); }
    [[nodiscard]] std::uint8_t Field553() const { return read_u8(0x553); }
    void Field553(std::uint8_t value) { write_u8(0x553, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field553() const { return Field553(); }
    void field553(std::uint8_t value) { Field553(value); }
    [[nodiscard]] std::int32_t Field554() const { return read_i32(0x554); }
    void Field554(std::int32_t value) { write_i32(0x554, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field554() const { return Field554(); }
    void field554(std::int32_t value) { Field554(value); }
    [[nodiscard]] std::int32_t Field558() const { return read_i32(0x558); }
    void Field558(std::int32_t value) { write_i32(0x558, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field558() const { return Field558(); }
    void field558(std::int32_t value) { Field558(value); }
    [[nodiscard]] ::fruityprime::memory::CameraInfo& CameraInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CameraInfo& CameraInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CameraInfo& camera_info() noexcept { return CameraInfo(); }
    [[nodiscard]] const ::fruityprime::memory::CameraInfo& camera_info() const noexcept { return CameraInfo(); }
    [[nodiscard]] formats::Vector3 PrevCamPos() const { return read_vec3(0x678); }
    void PrevCamPos(formats::Vector3 value) { write_vec3(0x678, value); }
    [[nodiscard]] formats::Vector3 prev_cam_pos() const { return PrevCamPos(); }
    void prev_cam_pos(formats::Vector3 value) { PrevCamPos(value); }
    [[nodiscard]] std::int32_t Field684() const { return read_i32(0x684); }
    void Field684(std::int32_t value) { write_i32(0x684, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field684() const { return Field684(); }
    void field684(std::int32_t value) { Field684(value); }
    [[nodiscard]] std::int32_t Field688() const { return read_i32(0x688); }
    void Field688(std::int32_t value) { write_i32(0x688, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field688() const { return Field688(); }
    void field688(std::int32_t value) { Field688(value); }
    [[nodiscard]] std::int32_t Field68C() const { return read_i32(0x68C); }
    void Field68C(std::int32_t value) { write_i32(0x68C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field68_c() const { return Field68C(); }
    void field68_c(std::int32_t value) { Field68C(value); }
    [[nodiscard]] std::int32_t Field690() const { return read_i32(0x690); }
    void Field690(std::int32_t value) { write_i32(0x690, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field690() const { return Field690(); }
    void field690(std::int32_t value) { Field690(value); }
    [[nodiscard]] ::fruityprime::memory::LightInfo& LightInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::LightInfo& LightInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::LightInfo& light_info() noexcept { return LightInfo(); }
    [[nodiscard]] const ::fruityprime::memory::LightInfo& light_info() const noexcept { return LightInfo(); }
    [[nodiscard]] std::uint8_t Field6B3() const { return read_u8(0x6B3); }
    void Field6B3(std::uint8_t value) { write_u8(0x6B3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field6_b3() const { return Field6B3(); }
    void field6_b3(std::uint8_t value) { Field6B3(value); }
    [[nodiscard]] std::uint32_t Field6B4() const { return read_pointer(0x6B4); }
    void Field6B4(std::uint32_t value) { write_pointer(0x6B4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t field6_b4() const { return Field6B4(); }
    void field6_b4(std::uint32_t value) { Field6B4(value); }
    [[nodiscard]] std::uint32_t LastCamPos() const { return read_pointer(0x6B8); }
    void LastCamPos(std::uint32_t value) { write_pointer(0x6B8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t last_cam_pos() const { return LastCamPos(); }
    void last_cam_pos(std::uint32_t value) { LastCamPos(value); }
    [[nodiscard]] std::uint32_t NextPointModule() const { return read_pointer(0x6BC); }
    void NextPointModule(std::uint32_t value) { write_pointer(0x6BC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t next_point_module() const { return NextPointModule(); }
    void next_point_module(std::uint32_t value) { NextPointModule(value); }
    [[nodiscard]] std::uint32_t LastJumpPad() const { return read_pointer(0x6C0); }
    void LastJumpPad(std::uint32_t value) { write_pointer(0x6C0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t last_jump_pad() const { return LastJumpPad(); }
    void last_jump_pad(std::uint32_t value) { LastJumpPad(value); }
    [[nodiscard]] std::uint32_t OctoFlag() const { return read_pointer(0x6C4); }
    void OctoFlag(std::uint32_t value) { write_pointer(0x6C4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t octo_flag() const { return OctoFlag(); }
    void octo_flag(std::uint32_t value) { OctoFlag(value); }
    [[nodiscard]] std::uint32_t EnemySpawner() const { return read_pointer(0x6C8); }
    void EnemySpawner(std::uint32_t value) { write_pointer(0x6C8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t enemy_spawner() const { return EnemySpawner(); }
    void enemy_spawner(std::uint32_t value) { EnemySpawner(value); }
    [[nodiscard]] std::uint32_t LastTarget() const { return read_pointer(0x6CC); }
    void LastTarget(std::uint32_t value) { write_pointer(0x6CC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t last_target() const { return LastTarget(); }
    void last_target(std::uint32_t value) { LastTarget(value); }
    [[nodiscard]] std::int32_t Field6D0() const { return read_i32(0x6D0); }
    void Field6D0(std::int32_t value) { write_i32(0x6D0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_d0() const { return Field6D0(); }
    void field6_d0(std::int32_t value) { Field6D0(value); }
    [[nodiscard]] std::uint32_t BurnedBy() const { return read_pointer(0x6D4); }
    void BurnedBy(std::uint32_t value) { write_pointer(0x6D4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t burned_by() const { return BurnedBy(); }
    void burned_by(std::uint32_t value) { BurnedBy(value); }
    [[nodiscard]] std::uint32_t EffectBurn() const { return read_pointer(0x6D8); }
    void EffectBurn(std::uint32_t value) { write_pointer(0x6D8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effect_burn() const { return EffectBurn(); }
    void effect_burn(std::uint32_t value) { EffectBurn(value); }
    [[nodiscard]] std::uint16_t BurnTimer() const { return read_u16(0x6DC); }
    void BurnTimer(std::uint16_t value) { write_u16(0x6DC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t burn_timer() const { return BurnTimer(); }
    void burn_timer(std::uint16_t value) { BurnTimer(value); }
    [[nodiscard]] std::uint16_t TargetOrDamageRelated() const { return read_u16(0x6DE); }
    void TargetOrDamageRelated(std::uint16_t value) { write_u16(0x6DE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t target_or_damage_related() const { return TargetOrDamageRelated(); }
    void target_or_damage_related(std::uint16_t value) { TargetOrDamageRelated(value); }
    [[nodiscard]] std::uint16_t Field6E0() const { return read_u16(0x6E0); }
    void Field6E0(std::uint16_t value) { write_u16(0x6E0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field6_e0() const { return Field6E0(); }
    void field6_e0(std::uint16_t value) { Field6E0(value); }
    [[nodiscard]] std::uint16_t Field6E2() const { return read_u16(0x6E2); }
    void Field6E2(std::uint16_t value) { write_u16(0x6E2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field6_e2() const { return Field6E2(); }
    void field6_e2(std::uint16_t value) { Field6E2(value); }
    [[nodiscard]] std::int32_t Field6E4() const { return read_i32(0x6E4); }
    void Field6E4(std::int32_t value) { write_i32(0x6E4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_e4() const { return Field6E4(); }
    void field6_e4(std::int32_t value) { Field6E4(value); }
    [[nodiscard]] std::int32_t Field6E8() const { return read_i32(0x6E8); }
    void Field6E8(std::int32_t value) { write_i32(0x6E8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_e8() const { return Field6E8(); }
    void field6_e8(std::int32_t value) { Field6E8(value); }
    [[nodiscard]] std::int32_t Field6EC() const { return read_i32(0x6EC); }
    void Field6EC(std::int32_t value) { write_i32(0x6EC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_e_c() const { return Field6EC(); }
    void field6_e_c(std::int32_t value) { Field6EC(value); }
    [[nodiscard]] std::uint32_t Effectiveness() const { return read_u32(0x6F0); }
    void Effectiveness(std::uint32_t value) { write_u32(0x6F0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t effectiveness() const { return Effectiveness(); }
    void effectiveness(std::uint32_t value) { Effectiveness(value); }
    [[nodiscard]] std::int32_t Field6F4() const { return read_i32(0x6F4); }
    void Field6F4(std::int32_t value) { write_i32(0x6F4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_f4() const { return Field6F4(); }
    void field6_f4(std::int32_t value) { Field6F4(value); }
    [[nodiscard]] std::int32_t Field6F8() const { return read_i32(0x6F8); }
    void Field6F8(std::int32_t value) { write_i32(0x6F8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_f8() const { return Field6F8(); }
    void field6_f8(std::int32_t value) { Field6F8(value); }
    [[nodiscard]] std::int32_t Field6FC() const { return read_i32(0x6FC); }
    void Field6FC(std::int32_t value) { write_i32(0x6FC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_f_c() const { return Field6FC(); }
    void field6_f_c(std::int32_t value) { Field6FC(value); }
    [[nodiscard]] std::int32_t Field700() const { return read_i32(0x700); }
    void Field700(std::int32_t value) { write_i32(0x700, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field700() const { return Field700(); }
    void field700(std::int32_t value) { Field700(value); }
    [[nodiscard]] std::uint16_t AltFormAttackTime() const { return read_u16(0x704); }
    void AltFormAttackTime(std::uint16_t value) { write_u16(0x704, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t alt_form_attack_time() const { return AltFormAttackTime(); }
    void alt_form_attack_time(std::uint16_t value) { AltFormAttackTime(value); }
    [[nodiscard]] std::uint16_t Field706() const { return read_u16(0x706); }
    void Field706(std::uint16_t value) { write_u16(0x706, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field706() const { return Field706(); }
    void field706(std::uint16_t value) { Field706(value); }
    [[nodiscard]] std::int32_t AltField708() const { return read_i32(0x708); }
    void AltField708(std::int32_t value) { write_i32(0x708, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t alt_field708() const { return AltField708(); }
    void alt_field708(std::int32_t value) { AltField708(value); }
    [[nodiscard]] std::int32_t AltField70C() const { return read_i32(0x70C); }
    void AltField70C(std::int32_t value) { write_i32(0x70C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t alt_field70_c() const { return AltField70C(); }
    void alt_field70_c(std::int32_t value) { AltField70C(value); }
    [[nodiscard]] std::int32_t AltField710() const { return read_i32(0x710); }
    void AltField710(std::int32_t value) { write_i32(0x710, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t alt_field710() const { return AltField710(); }
    void alt_field710(std::int32_t value) { AltField710(value); }
    [[nodiscard]] formats::Vector3 Field714() const { return read_vec3(0x714); }
    void Field714(formats::Vector3 value) { write_vec3(0x714, value); }
    [[nodiscard]] formats::Vector3 field714() const { return Field714(); }
    void field714(formats::Vector3 value) { Field714(value); }
    [[nodiscard]] formats::Vector3 Field720() const { return read_vec3(0x720); }
    void Field720(formats::Vector3 value) { write_vec3(0x720, value); }
    [[nodiscard]] formats::Vector3 field720() const { return Field720(); }
    void field720(formats::Vector3 value) { Field720(value); }
    [[nodiscard]] formats::Vector3 Field72C() const { return read_vec3(0x72C); }
    void Field72C(formats::Vector3 value) { write_vec3(0x72C, value); }
    [[nodiscard]] formats::Vector3 field72_c() const { return Field72C(); }
    void field72_c(formats::Vector3 value) { Field72C(value); }
    [[nodiscard]] std::int32_t Field738() const { return read_i32(0x738); }
    void Field738(std::int32_t value) { write_i32(0x738, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field738() const { return Field738(); }
    void field738(std::int32_t value) { Field738(value); }
    [[nodiscard]] std::int32_t Field73C() const { return read_i32(0x73C); }
    void Field73C(std::int32_t value) { write_i32(0x73C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field73_c() const { return Field73C(); }
    void field73_c(std::int32_t value) { Field73C(value); }
    [[nodiscard]] std::int32_t Field740() const { return read_i32(0x740); }
    void Field740(std::int32_t value) { write_i32(0x740, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field740() const { return Field740(); }
    void field740(std::int32_t value) { Field740(value); }
    [[nodiscard]] std::int32_t Field744() const { return read_i32(0x744); }
    void Field744(std::int32_t value) { write_i32(0x744, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field744() const { return Field744(); }
    void field744(std::int32_t value) { Field744(value); }
    [[nodiscard]] std::int32_t Field748() const { return read_i32(0x748); }
    void Field748(std::int32_t value) { write_i32(0x748, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field748() const { return Field748(); }
    void field748(std::int32_t value) { Field748(value); }
    [[nodiscard]] std::int32_t Field74C() const { return read_i32(0x74C); }
    void Field74C(std::int32_t value) { write_i32(0x74C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field74_c() const { return Field74C(); }
    void field74_c(std::int32_t value) { Field74C(value); }
    [[nodiscard]] std::int32_t Field750() const { return read_i32(0x750); }
    void Field750(std::int32_t value) { write_i32(0x750, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field750() const { return Field750(); }
    void field750(std::int32_t value) { Field750(value); }
    [[nodiscard]] std::int32_t Field754() const { return read_i32(0x754); }
    void Field754(std::int32_t value) { write_i32(0x754, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field754() const { return Field754(); }
    void field754(std::int32_t value) { Field754(value); }
    [[nodiscard]] std::int32_t Field758() const { return read_i32(0x758); }
    void Field758(std::int32_t value) { write_i32(0x758, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field758() const { return Field758(); }
    void field758(std::int32_t value) { Field758(value); }
    [[nodiscard]] std::int32_t Field75C() const { return read_i32(0x75C); }
    void Field75C(std::int32_t value) { write_i32(0x75C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field75_c() const { return Field75C(); }
    void field75_c(std::int32_t value) { Field75C(value); }
    [[nodiscard]] std::int32_t Field760() const { return read_i32(0x760); }
    void Field760(std::int32_t value) { write_i32(0x760, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field760() const { return Field760(); }
    void field760(std::int32_t value) { Field760(value); }
    [[nodiscard]] std::int32_t Field764() const { return read_i32(0x764); }
    void Field764(std::int32_t value) { write_i32(0x764, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field764() const { return Field764(); }
    void field764(std::int32_t value) { Field764(value); }
    [[nodiscard]] std::int32_t Field768() const { return read_i32(0x768); }
    void Field768(std::int32_t value) { write_i32(0x768, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field768() const { return Field768(); }
    void field768(std::int32_t value) { Field768(value); }
    [[nodiscard]] std::int32_t Field76C() const { return read_i32(0x76C); }
    void Field76C(std::int32_t value) { write_i32(0x76C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field76_c() const { return Field76C(); }
    void field76_c(std::int32_t value) { Field76C(value); }
    [[nodiscard]] std::int32_t Field770() const { return read_i32(0x770); }
    void Field770(std::int32_t value) { write_i32(0x770, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field770() const { return Field770(); }
    void field770(std::int32_t value) { Field770(value); }
    [[nodiscard]] std::int32_t Field774() const { return read_i32(0x774); }
    void Field774(std::int32_t value) { write_i32(0x774, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field774() const { return Field774(); }
    void field774(std::int32_t value) { Field774(value); }
    [[nodiscard]] std::int32_t Field778() const { return read_i32(0x778); }
    void Field778(std::int32_t value) { write_i32(0x778, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field778() const { return Field778(); }
    void field778(std::int32_t value) { Field778(value); }
    [[nodiscard]] std::int32_t Field77C() const { return read_i32(0x77C); }
    void Field77C(std::int32_t value) { write_i32(0x77C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field77_c() const { return Field77C(); }
    void field77_c(std::int32_t value) { Field77C(value); }
    [[nodiscard]] std::int32_t Field780() const { return read_i32(0x780); }
    void Field780(std::int32_t value) { write_i32(0x780, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field780() const { return Field780(); }
    void field780(std::int32_t value) { Field780(value); }
    [[nodiscard]] std::int32_t Field784() const { return read_i32(0x784); }
    void Field784(std::int32_t value) { write_i32(0x784, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field784() const { return Field784(); }
    void field784(std::int32_t value) { Field784(value); }
    [[nodiscard]] std::int32_t Field788() const { return read_i32(0x788); }
    void Field788(std::int32_t value) { write_i32(0x788, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field788() const { return Field788(); }
    void field788(std::int32_t value) { Field788(value); }
    [[nodiscard]] std::int32_t Field78C() const { return read_i32(0x78C); }
    void Field78C(std::int32_t value) { write_i32(0x78C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field78_c() const { return Field78C(); }
    void field78_c(std::int32_t value) { Field78C(value); }
    [[nodiscard]] std::int32_t Field790() const { return read_i32(0x790); }
    void Field790(std::int32_t value) { write_i32(0x790, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field790() const { return Field790(); }
    void field790(std::int32_t value) { Field790(value); }
    [[nodiscard]] std::int32_t Field794() const { return read_i32(0x794); }
    void Field794(std::int32_t value) { write_i32(0x794, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field794() const { return Field794(); }
    void field794(std::int32_t value) { Field794(value); }
    [[nodiscard]] std::int32_t Field798() const { return read_i32(0x798); }
    void Field798(std::int32_t value) { write_i32(0x798, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field798() const { return Field798(); }
    void field798(std::int32_t value) { Field798(value); }
    [[nodiscard]] std::int32_t Field79C() const { return read_i32(0x79C); }
    void Field79C(std::int32_t value) { write_i32(0x79C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field79_c() const { return Field79C(); }
    void field79_c(std::int32_t value) { Field79C(value); }
    [[nodiscard]] std::int32_t Field7A0() const { return read_i32(0x7A0); }
    void Field7A0(std::int32_t value) { write_i32(0x7A0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_a0() const { return Field7A0(); }
    void field7_a0(std::int32_t value) { Field7A0(value); }
    [[nodiscard]] std::int32_t Field7A4() const { return read_i32(0x7A4); }
    void Field7A4(std::int32_t value) { write_i32(0x7A4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_a4() const { return Field7A4(); }
    void field7_a4(std::int32_t value) { Field7A4(value); }
    [[nodiscard]] std::int32_t Field7A8() const { return read_i32(0x7A8); }
    void Field7A8(std::int32_t value) { write_i32(0x7A8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_a8() const { return Field7A8(); }
    void field7_a8(std::int32_t value) { Field7A8(value); }
    [[nodiscard]] std::int32_t Field7AC() const { return read_i32(0x7AC); }
    void Field7AC(std::int32_t value) { write_i32(0x7AC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_a_c() const { return Field7AC(); }
    void field7_a_c(std::int32_t value) { Field7AC(value); }
    [[nodiscard]] std::int32_t Field7B0() const { return read_i32(0x7B0); }
    void Field7B0(std::int32_t value) { write_i32(0x7B0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_b0() const { return Field7B0(); }
    void field7_b0(std::int32_t value) { Field7B0(value); }
    [[nodiscard]] std::int32_t Field7B4() const { return read_i32(0x7B4); }
    void Field7B4(std::int32_t value) { write_i32(0x7B4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_b4() const { return Field7B4(); }
    void field7_b4(std::int32_t value) { Field7B4(value); }
    [[nodiscard]] std::int32_t Field7B8() const { return read_i32(0x7B8); }
    void Field7B8(std::int32_t value) { write_i32(0x7B8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_b8() const { return Field7B8(); }
    void field7_b8(std::int32_t value) { Field7B8(value); }
    [[nodiscard]] std::int32_t Field7BC() const { return read_i32(0x7BC); }
    void Field7BC(std::int32_t value) { write_i32(0x7BC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_b_c() const { return Field7BC(); }
    void field7_b_c(std::int32_t value) { Field7BC(value); }
    [[nodiscard]] std::int32_t Field7C0() const { return read_i32(0x7C0); }
    void Field7C0(std::int32_t value) { write_i32(0x7C0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c0() const { return Field7C0(); }
    void field7_c0(std::int32_t value) { Field7C0(value); }
    [[nodiscard]] std::int32_t Field7C4() const { return read_i32(0x7C4); }
    void Field7C4(std::int32_t value) { write_i32(0x7C4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c4() const { return Field7C4(); }
    void field7_c4(std::int32_t value) { Field7C4(value); }
    [[nodiscard]] std::int32_t Field7C8() const { return read_i32(0x7C8); }
    void Field7C8(std::int32_t value) { write_i32(0x7C8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c8() const { return Field7C8(); }
    void field7_c8(std::int32_t value) { Field7C8(value); }
    [[nodiscard]] std::int32_t Field7CC() const { return read_i32(0x7CC); }
    void Field7CC(std::int32_t value) { write_i32(0x7CC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c_c() const { return Field7CC(); }
    void field7_c_c(std::int32_t value) { Field7CC(value); }
    [[nodiscard]] std::int32_t Field7D0() const { return read_i32(0x7D0); }
    void Field7D0(std::int32_t value) { write_i32(0x7D0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_d0() const { return Field7D0(); }
    void field7_d0(std::int32_t value) { Field7D0(value); }
    [[nodiscard]] formats::Vector3 Field7D4() const { return read_vec3(0x7D4); }
    void Field7D4(formats::Vector3 value) { write_vec3(0x7D4, value); }
    [[nodiscard]] formats::Vector3 field7_d4() const { return Field7D4(); }
    void field7_d4(formats::Vector3 value) { Field7D4(value); }
    [[nodiscard]] formats::Vector3 Field7E0() const { return read_vec3(0x7E0); }
    void Field7E0(formats::Vector3 value) { write_vec3(0x7E0, value); }
    [[nodiscard]] formats::Vector3 field7_e0() const { return Field7E0(); }
    void field7_e0(formats::Vector3 value) { Field7E0(value); }
    [[nodiscard]] std::int32_t Field7EC() const { return read_i32(0x7EC); }
    void Field7EC(std::int32_t value) { write_i32(0x7EC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_e_c() const { return Field7EC(); }
    void field7_e_c(std::int32_t value) { Field7EC(value); }
    [[nodiscard]] std::int32_t Field7F0() const { return read_i32(0x7F0); }
    void Field7F0(std::int32_t value) { write_i32(0x7F0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_f0() const { return Field7F0(); }
    void field7_f0(std::int32_t value) { Field7F0(value); }
    [[nodiscard]] std::int32_t Field7F4() const { return read_i32(0x7F4); }
    void Field7F4(std::int32_t value) { write_i32(0x7F4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_f4() const { return Field7F4(); }
    void field7_f4(std::int32_t value) { Field7F4(value); }
    [[nodiscard]] std::int32_t Field7F8() const { return read_i32(0x7F8); }
    void Field7F8(std::int32_t value) { write_i32(0x7F8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_f8() const { return Field7F8(); }
    void field7_f8(std::int32_t value) { Field7F8(value); }
    [[nodiscard]] std::uint32_t Lrock01Node() const { return read_pointer(0x7FC); }
    void Lrock01Node(std::uint32_t value) { write_pointer(0x7FC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t lrock01_node() const { return Lrock01Node(); }
    void lrock01_node(std::uint32_t value) { Lrock01Node(value); }
    [[nodiscard]] std::uint32_t Rrock01Node() const { return read_pointer(0x800); }
    void Rrock01Node(std::uint32_t value) { write_pointer(0x800, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t rrock01_node() const { return Rrock01Node(); }
    void rrock01_node(std::uint32_t value) { Rrock01Node(value); }
    [[nodiscard]] std::uint32_t RposrotNode() const { return read_pointer(0x804); }
    void RposrotNode(std::uint32_t value) { write_pointer(0x804, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t rposrot_node() const { return RposrotNode(); }
    void rposrot_node(std::uint32_t value) { RposrotNode(value); }
    [[nodiscard]] std::uint32_t Rposrot1Node() const { return read_pointer(0x808); }
    void Rposrot1Node(std::uint32_t value) { write_pointer(0x808, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t rposrot1_node() const { return Rposrot1Node(); }
    void rposrot1_node(std::uint32_t value) { Rposrot1Node(value); }
    [[nodiscard]] std::int32_t Field80C() const { return read_i32(0x80C); }
    void Field80C(std::int32_t value) { write_i32(0x80C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field80_c() const { return Field80C(); }
    void field80_c(std::int32_t value) { Field80C(value); }
    [[nodiscard]] std::int32_t Field810() const { return read_i32(0x810); }
    void Field810(std::int32_t value) { write_i32(0x810, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field810() const { return Field810(); }
    void field810(std::int32_t value) { Field810(value); }
    [[nodiscard]] std::int32_t Field814() const { return read_i32(0x814); }
    void Field814(std::int32_t value) { write_i32(0x814, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field814() const { return Field814(); }
    void field814(std::int32_t value) { Field814(value); }
    [[nodiscard]] std::int32_t Field818() const { return read_i32(0x818); }
    void Field818(std::int32_t value) { write_i32(0x818, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field818() const { return Field818(); }
    void field818(std::int32_t value) { Field818(value); }
    [[nodiscard]] std::int32_t Field81C() const { return read_i32(0x81C); }
    void Field81C(std::int32_t value) { write_i32(0x81C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field81_c() const { return Field81C(); }
    void field81_c(std::int32_t value) { Field81C(value); }
    [[nodiscard]] std::int32_t Field820() const { return read_i32(0x820); }
    void Field820(std::int32_t value) { write_i32(0x820, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field820() const { return Field820(); }
    void field820(std::int32_t value) { Field820(value); }
    [[nodiscard]] std::int32_t Field824() const { return read_i32(0x824); }
    void Field824(std::int32_t value) { write_i32(0x824, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field824() const { return Field824(); }
    void field824(std::int32_t value) { Field824(value); }
    [[nodiscard]] formats::Vector3 Field828() const { return read_vec3(0x828); }
    void Field828(formats::Vector3 value) { write_vec3(0x828, value); }
    [[nodiscard]] formats::Vector3 field828() const { return Field828(); }
    void field828(formats::Vector3 value) { Field828(value); }
    [[nodiscard]] std::int32_t Field834() const { return read_i32(0x834); }
    void Field834(std::int32_t value) { write_i32(0x834, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field834() const { return Field834(); }
    void field834(std::int32_t value) { Field834(value); }
    [[nodiscard]] std::int32_t Field838() const { return read_i32(0x838); }
    void Field838(std::int32_t value) { write_i32(0x838, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field838() const { return Field838(); }
    void field838(std::int32_t value) { Field838(value); }
    [[nodiscard]] std::int32_t Field83C() const { return read_i32(0x83C); }
    void Field83C(std::int32_t value) { write_i32(0x83C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field83_c() const { return Field83C(); }
    void field83_c(std::int32_t value) { Field83C(value); }
    [[nodiscard]] std::int32_t Field840() const { return read_i32(0x840); }
    void Field840(std::int32_t value) { write_i32(0x840, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field840() const { return Field840(); }
    void field840(std::int32_t value) { Field840(value); }
    [[nodiscard]] std::int32_t Field844() const { return read_i32(0x844); }
    void Field844(std::int32_t value) { write_i32(0x844, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field844() const { return Field844(); }
    void field844(std::int32_t value) { Field844(value); }
    [[nodiscard]] std::uint32_t Struct1() const { return read_pointer(0x848); }
    void Struct1(std::uint32_t value) { write_pointer(0x848, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t struct1() const { return Struct1(); }
    void struct1(std::uint32_t value) { Struct1(value); }
    [[nodiscard]] std::uint8_t LoadFlags() const { return read_u8(0x84C); }
    void LoadFlags(std::uint8_t value) { write_u8(0x84C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t load_flags() const { return LoadFlags(); }
    void load_flags(std::uint8_t value) { LoadFlags(value); }
    [[nodiscard]] std::uint8_t SlotIndex() const { return read_u8(0x84D); }
    void SlotIndex(std::uint8_t value) { write_u8(0x84D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t slot_index() const { return SlotIndex(); }
    void slot_index(std::uint8_t value) { SlotIndex(value); }
    [[nodiscard]] std::uint8_t IsBot() const { return read_u8(0x84E); }
    void IsBot(std::uint8_t value) { write_u8(0x84E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t is_bot() const { return IsBot(); }
    void is_bot(std::uint8_t value) { IsBot(value); }
    [[nodiscard]] std::uint8_t Field84F() const { return read_u8(0x84F); }
    void Field84F(std::uint8_t value) { write_u8(0x84F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field84_f() const { return Field84F(); }
    void field84_f(std::uint8_t value) { Field84F(value); }
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& EquipInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& EquipInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::EquipInfoPtr& equip_info() noexcept { return EquipInfo(); }
    [[nodiscard]] const ::fruityprime::memory::EquipInfoPtr& equip_info() const noexcept { return EquipInfo(); }
    [[nodiscard]] ::fruityprime::memory::CBeamProjectile& BeamHead() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CBeamProjectile& BeamHead() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CBeamProjectile& beam_head() noexcept { return BeamHead(); }
    [[nodiscard]] const ::fruityprime::memory::CBeamProjectile& beam_head() const noexcept { return BeamHead(); }
    [[nodiscard]] std::int32_t Field9BC() const { return read_i32(0x9BC); }
    void Field9BC(std::int32_t value) { write_i32(0x9BC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_b_c() const { return Field9BC(); }
    void field9_b_c(std::int32_t value) { Field9BC(value); }
    [[nodiscard]] std::int32_t Field9C0() const { return read_i32(0x9C0); }
    void Field9C0(std::int32_t value) { write_i32(0x9C0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c0() const { return Field9C0(); }
    void field9_c0(std::int32_t value) { Field9C0(value); }
    [[nodiscard]] std::int32_t Field9C4() const { return read_i32(0x9C4); }
    void Field9C4(std::int32_t value) { write_i32(0x9C4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c4() const { return Field9C4(); }
    void field9_c4(std::int32_t value) { Field9C4(value); }
    [[nodiscard]] std::int32_t Field9C8() const { return read_i32(0x9C8); }
    void Field9C8(std::int32_t value) { write_i32(0x9C8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c8() const { return Field9C8(); }
    void field9_c8(std::int32_t value) { Field9C8(value); }
    [[nodiscard]] std::int32_t Field9CC() const { return read_i32(0x9CC); }
    void Field9CC(std::int32_t value) { write_i32(0x9CC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c_c() const { return Field9CC(); }
    void field9_c_c(std::int32_t value) { Field9CC(value); }
    [[nodiscard]] std::int32_t Field9D0() const { return read_i32(0x9D0); }
    void Field9D0(std::int32_t value) { write_i32(0x9D0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_d0() const { return Field9D0(); }
    void field9_d0(std::int32_t value) { Field9D0(value); }
    [[nodiscard]] std::int32_t Field9D4() const { return read_i32(0x9D4); }
    void Field9D4(std::int32_t value) { write_i32(0x9D4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_d4() const { return Field9D4(); }
    void field9_d4(std::int32_t value) { Field9D4(value); }
    [[nodiscard]] std::int32_t Field9D8() const { return read_i32(0x9D8); }
    void Field9D8(std::int32_t value) { write_i32(0x9D8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_d8() const { return Field9D8(); }
    void field9_d8(std::int32_t value) { Field9D8(value); }
    [[nodiscard]] std::int32_t Field9DC() const { return read_i32(0x9DC); }
    void Field9DC(std::int32_t value) { write_i32(0x9DC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_d_c() const { return Field9DC(); }
    void field9_d_c(std::int32_t value) { Field9DC(value); }
    [[nodiscard]] std::int32_t Field9E0() const { return read_i32(0x9E0); }
    void Field9E0(std::int32_t value) { write_i32(0x9E0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_e0() const { return Field9E0(); }
    void field9_e0(std::int32_t value) { Field9E0(value); }
    [[nodiscard]] std::int32_t Field9E4() const { return read_i32(0x9E4); }
    void Field9E4(std::int32_t value) { write_i32(0x9E4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_e4() const { return Field9E4(); }
    void field9_e4(std::int32_t value) { Field9E4(value); }
    [[nodiscard]] std::int32_t Field9E8() const { return read_i32(0x9E8); }
    void Field9E8(std::int32_t value) { write_i32(0x9E8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_e8() const { return Field9E8(); }
    void field9_e8(std::int32_t value) { Field9E8(value); }
    [[nodiscard]] std::int32_t Field9EC() const { return read_i32(0x9EC); }
    void Field9EC(std::int32_t value) { write_i32(0x9EC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_e_c() const { return Field9EC(); }
    void field9_e_c(std::int32_t value) { Field9EC(value); }
    [[nodiscard]] std::int32_t Field9F0() const { return read_i32(0x9F0); }
    void Field9F0(std::int32_t value) { write_i32(0x9F0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_f0() const { return Field9F0(); }
    void field9_f0(std::int32_t value) { Field9F0(value); }
    [[nodiscard]] std::int32_t Field9F4() const { return read_i32(0x9F4); }
    void Field9F4(std::int32_t value) { write_i32(0x9F4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_f4() const { return Field9F4(); }
    void field9_f4(std::int32_t value) { Field9F4(value); }
    [[nodiscard]] std::int32_t Field9F8() const { return read_i32(0x9F8); }
    void Field9F8(std::int32_t value) { write_i32(0x9F8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_f8() const { return Field9F8(); }
    void field9_f8(std::int32_t value) { Field9F8(value); }
    [[nodiscard]] std::int32_t Field9FC() const { return read_i32(0x9FC); }
    void Field9FC(std::int32_t value) { write_i32(0x9FC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_f_c() const { return Field9FC(); }
    void field9_f_c(std::int32_t value) { Field9FC(value); }
    [[nodiscard]] std::int32_t FieldA00() const { return read_i32(0xA00); }
    void FieldA00(std::int32_t value) { write_i32(0xA00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a00() const { return FieldA00(); }
    void field_a00(std::int32_t value) { FieldA00(value); }
    [[nodiscard]] std::int32_t FieldA04() const { return read_i32(0xA04); }
    void FieldA04(std::int32_t value) { write_i32(0xA04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a04() const { return FieldA04(); }
    void field_a04(std::int32_t value) { FieldA04(value); }
    [[nodiscard]] std::int32_t FieldA08() const { return read_i32(0xA08); }
    void FieldA08(std::int32_t value) { write_i32(0xA08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a08() const { return FieldA08(); }
    void field_a08(std::int32_t value) { FieldA08(value); }
    [[nodiscard]] std::int32_t FieldA0C() const { return read_i32(0xA0C); }
    void FieldA0C(std::int32_t value) { write_i32(0xA0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a0_c() const { return FieldA0C(); }
    void field_a0_c(std::int32_t value) { FieldA0C(value); }
    [[nodiscard]] std::int32_t FieldA10() const { return read_i32(0xA10); }
    void FieldA10(std::int32_t value) { write_i32(0xA10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a10() const { return FieldA10(); }
    void field_a10(std::int32_t value) { FieldA10(value); }
    [[nodiscard]] std::int32_t FieldA14() const { return read_i32(0xA14); }
    void FieldA14(std::int32_t value) { write_i32(0xA14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a14() const { return FieldA14(); }
    void field_a14(std::int32_t value) { FieldA14(value); }
    [[nodiscard]] std::int32_t FieldA18() const { return read_i32(0xA18); }
    void FieldA18(std::int32_t value) { write_i32(0xA18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a18() const { return FieldA18(); }
    void field_a18(std::int32_t value) { FieldA18(value); }
    [[nodiscard]] std::int32_t FieldA1C() const { return read_i32(0xA1C); }
    void FieldA1C(std::int32_t value) { write_i32(0xA1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a1_c() const { return FieldA1C(); }
    void field_a1_c(std::int32_t value) { FieldA1C(value); }
    [[nodiscard]] std::int32_t FieldA20() const { return read_i32(0xA20); }
    void FieldA20(std::int32_t value) { write_i32(0xA20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a20() const { return FieldA20(); }
    void field_a20(std::int32_t value) { FieldA20(value); }
    [[nodiscard]] std::int32_t FieldA24() const { return read_i32(0xA24); }
    void FieldA24(std::int32_t value) { write_i32(0xA24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a24() const { return FieldA24(); }
    void field_a24(std::int32_t value) { FieldA24(value); }
    [[nodiscard]] std::int32_t FieldA28() const { return read_i32(0xA28); }
    void FieldA28(std::int32_t value) { write_i32(0xA28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a28() const { return FieldA28(); }
    void field_a28(std::int32_t value) { FieldA28(value); }
    [[nodiscard]] std::int32_t FieldA2C() const { return read_i32(0xA2C); }
    void FieldA2C(std::int32_t value) { write_i32(0xA2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a2_c() const { return FieldA2C(); }
    void field_a2_c(std::int32_t value) { FieldA2C(value); }
    [[nodiscard]] std::int32_t FieldA30() const { return read_i32(0xA30); }
    void FieldA30(std::int32_t value) { write_i32(0xA30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a30() const { return FieldA30(); }
    void field_a30(std::int32_t value) { FieldA30(value); }
    [[nodiscard]] std::int32_t FieldA34() const { return read_i32(0xA34); }
    void FieldA34(std::int32_t value) { write_i32(0xA34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a34() const { return FieldA34(); }
    void field_a34(std::int32_t value) { FieldA34(value); }
    [[nodiscard]] std::int32_t FieldA38() const { return read_i32(0xA38); }
    void FieldA38(std::int32_t value) { write_i32(0xA38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a38() const { return FieldA38(); }
    void field_a38(std::int32_t value) { FieldA38(value); }
    [[nodiscard]] std::int32_t FieldA3C() const { return read_i32(0xA3C); }
    void FieldA3C(std::int32_t value) { write_i32(0xA3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a3_c() const { return FieldA3C(); }
    void field_a3_c(std::int32_t value) { FieldA3C(value); }
    [[nodiscard]] std::int32_t FieldA40() const { return read_i32(0xA40); }
    void FieldA40(std::int32_t value) { write_i32(0xA40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a40() const { return FieldA40(); }
    void field_a40(std::int32_t value) { FieldA40(value); }
    [[nodiscard]] std::int32_t FieldA44() const { return read_i32(0xA44); }
    void FieldA44(std::int32_t value) { write_i32(0xA44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a44() const { return FieldA44(); }
    void field_a44(std::int32_t value) { FieldA44(value); }
    [[nodiscard]] std::int32_t FieldA48() const { return read_i32(0xA48); }
    void FieldA48(std::int32_t value) { write_i32(0xA48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a48() const { return FieldA48(); }
    void field_a48(std::int32_t value) { FieldA48(value); }
    [[nodiscard]] std::int32_t FieldA4C() const { return read_i32(0xA4C); }
    void FieldA4C(std::int32_t value) { write_i32(0xA4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a4_c() const { return FieldA4C(); }
    void field_a4_c(std::int32_t value) { FieldA4C(value); }
    [[nodiscard]] std::int32_t FieldA50() const { return read_i32(0xA50); }
    void FieldA50(std::int32_t value) { write_i32(0xA50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a50() const { return FieldA50(); }
    void field_a50(std::int32_t value) { FieldA50(value); }
    [[nodiscard]] std::int32_t FieldA54() const { return read_i32(0xA54); }
    void FieldA54(std::int32_t value) { write_i32(0xA54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a54() const { return FieldA54(); }
    void field_a54(std::int32_t value) { FieldA54(value); }
    [[nodiscard]] std::int32_t FieldA58() const { return read_i32(0xA58); }
    void FieldA58(std::int32_t value) { write_i32(0xA58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a58() const { return FieldA58(); }
    void field_a58(std::int32_t value) { FieldA58(value); }
    [[nodiscard]] std::int32_t FieldA5C() const { return read_i32(0xA5C); }
    void FieldA5C(std::int32_t value) { write_i32(0xA5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a5_c() const { return FieldA5C(); }
    void field_a5_c(std::int32_t value) { FieldA5C(value); }
    [[nodiscard]] std::int32_t FieldA60() const { return read_i32(0xA60); }
    void FieldA60(std::int32_t value) { write_i32(0xA60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a60() const { return FieldA60(); }
    void field_a60(std::int32_t value) { FieldA60(value); }
    [[nodiscard]] std::int32_t FieldA64() const { return read_i32(0xA64); }
    void FieldA64(std::int32_t value) { write_i32(0xA64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a64() const { return FieldA64(); }
    void field_a64(std::int32_t value) { FieldA64(value); }
    [[nodiscard]] std::int32_t FieldA68() const { return read_i32(0xA68); }
    void FieldA68(std::int32_t value) { write_i32(0xA68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a68() const { return FieldA68(); }
    void field_a68(std::int32_t value) { FieldA68(value); }
    [[nodiscard]] std::int32_t FieldA6C() const { return read_i32(0xA6C); }
    void FieldA6C(std::int32_t value) { write_i32(0xA6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a6_c() const { return FieldA6C(); }
    void field_a6_c(std::int32_t value) { FieldA6C(value); }
    [[nodiscard]] std::int32_t FieldA70() const { return read_i32(0xA70); }
    void FieldA70(std::int32_t value) { write_i32(0xA70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a70() const { return FieldA70(); }
    void field_a70(std::int32_t value) { FieldA70(value); }
    [[nodiscard]] std::int32_t FieldA74() const { return read_i32(0xA74); }
    void FieldA74(std::int32_t value) { write_i32(0xA74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a74() const { return FieldA74(); }
    void field_a74(std::int32_t value) { FieldA74(value); }
    [[nodiscard]] std::int32_t FieldA78() const { return read_i32(0xA78); }
    void FieldA78(std::int32_t value) { write_i32(0xA78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a78() const { return FieldA78(); }
    void field_a78(std::int32_t value) { FieldA78(value); }
    [[nodiscard]] std::int32_t FieldA7C() const { return read_i32(0xA7C); }
    void FieldA7C(std::int32_t value) { write_i32(0xA7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a7_c() const { return FieldA7C(); }
    void field_a7_c(std::int32_t value) { FieldA7C(value); }
    [[nodiscard]] std::int32_t FieldA80() const { return read_i32(0xA80); }
    void FieldA80(std::int32_t value) { write_i32(0xA80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a80() const { return FieldA80(); }
    void field_a80(std::int32_t value) { FieldA80(value); }
    [[nodiscard]] std::int32_t FieldA84() const { return read_i32(0xA84); }
    void FieldA84(std::int32_t value) { write_i32(0xA84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a84() const { return FieldA84(); }
    void field_a84(std::int32_t value) { FieldA84(value); }
    [[nodiscard]] std::int32_t FieldA88() const { return read_i32(0xA88); }
    void FieldA88(std::int32_t value) { write_i32(0xA88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a88() const { return FieldA88(); }
    void field_a88(std::int32_t value) { FieldA88(value); }
    [[nodiscard]] std::int32_t FieldA8C() const { return read_i32(0xA8C); }
    void FieldA8C(std::int32_t value) { write_i32(0xA8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a8_c() const { return FieldA8C(); }
    void field_a8_c(std::int32_t value) { FieldA8C(value); }
    [[nodiscard]] std::int32_t FieldA90() const { return read_i32(0xA90); }
    void FieldA90(std::int32_t value) { write_i32(0xA90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a90() const { return FieldA90(); }
    void field_a90(std::int32_t value) { FieldA90(value); }
    [[nodiscard]] std::int32_t FieldA94() const { return read_i32(0xA94); }
    void FieldA94(std::int32_t value) { write_i32(0xA94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a94() const { return FieldA94(); }
    void field_a94(std::int32_t value) { FieldA94(value); }
    [[nodiscard]] std::int32_t FieldA98() const { return read_i32(0xA98); }
    void FieldA98(std::int32_t value) { write_i32(0xA98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a98() const { return FieldA98(); }
    void field_a98(std::int32_t value) { FieldA98(value); }
    [[nodiscard]] std::int32_t FieldA9C() const { return read_i32(0xA9C); }
    void FieldA9C(std::int32_t value) { write_i32(0xA9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a9_c() const { return FieldA9C(); }
    void field_a9_c(std::int32_t value) { FieldA9C(value); }
    [[nodiscard]] std::int32_t FieldAA0() const { return read_i32(0xAA0); }
    void FieldAA0(std::int32_t value) { write_i32(0xAA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_a0() const { return FieldAA0(); }
    void field_a_a0(std::int32_t value) { FieldAA0(value); }
    [[nodiscard]] std::int32_t FieldAA4() const { return read_i32(0xAA4); }
    void FieldAA4(std::int32_t value) { write_i32(0xAA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_a4() const { return FieldAA4(); }
    void field_a_a4(std::int32_t value) { FieldAA4(value); }
    [[nodiscard]] std::int32_t FieldAA8() const { return read_i32(0xAA8); }
    void FieldAA8(std::int32_t value) { write_i32(0xAA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_a8() const { return FieldAA8(); }
    void field_a_a8(std::int32_t value) { FieldAA8(value); }
    [[nodiscard]] std::int32_t FieldAAC() const { return read_i32(0xAAC); }
    void FieldAAC(std::int32_t value) { write_i32(0xAAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_a_c() const { return FieldAAC(); }
    void field_a_a_c(std::int32_t value) { FieldAAC(value); }
    [[nodiscard]] std::int32_t FieldAB0() const { return read_i32(0xAB0); }
    void FieldAB0(std::int32_t value) { write_i32(0xAB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_b0() const { return FieldAB0(); }
    void field_a_b0(std::int32_t value) { FieldAB0(value); }
    [[nodiscard]] std::int32_t FieldAB4() const { return read_i32(0xAB4); }
    void FieldAB4(std::int32_t value) { write_i32(0xAB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_b4() const { return FieldAB4(); }
    void field_a_b4(std::int32_t value) { FieldAB4(value); }
    [[nodiscard]] std::int32_t FieldAB8() const { return read_i32(0xAB8); }
    void FieldAB8(std::int32_t value) { write_i32(0xAB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_b8() const { return FieldAB8(); }
    void field_a_b8(std::int32_t value) { FieldAB8(value); }
    [[nodiscard]] std::int32_t FieldABC() const { return read_i32(0xABC); }
    void FieldABC(std::int32_t value) { write_i32(0xABC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_b_c() const { return FieldABC(); }
    void field_a_b_c(std::int32_t value) { FieldABC(value); }
    [[nodiscard]] std::int32_t FieldAC0() const { return read_i32(0xAC0); }
    void FieldAC0(std::int32_t value) { write_i32(0xAC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c0() const { return FieldAC0(); }
    void field_a_c0(std::int32_t value) { FieldAC0(value); }
    [[nodiscard]] std::int32_t FieldAC4() const { return read_i32(0xAC4); }
    void FieldAC4(std::int32_t value) { write_i32(0xAC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c4() const { return FieldAC4(); }
    void field_a_c4(std::int32_t value) { FieldAC4(value); }
    [[nodiscard]] std::int32_t FieldAC8() const { return read_i32(0xAC8); }
    void FieldAC8(std::int32_t value) { write_i32(0xAC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c8() const { return FieldAC8(); }
    void field_a_c8(std::int32_t value) { FieldAC8(value); }
    [[nodiscard]] std::int32_t FieldACC() const { return read_i32(0xACC); }
    void FieldACC(std::int32_t value) { write_i32(0xACC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c_c() const { return FieldACC(); }
    void field_a_c_c(std::int32_t value) { FieldACC(value); }
    [[nodiscard]] std::int32_t FieldAD0() const { return read_i32(0xAD0); }
    void FieldAD0(std::int32_t value) { write_i32(0xAD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_d0() const { return FieldAD0(); }
    void field_a_d0(std::int32_t value) { FieldAD0(value); }
    [[nodiscard]] std::int32_t FieldAD4() const { return read_i32(0xAD4); }
    void FieldAD4(std::int32_t value) { write_i32(0xAD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_d4() const { return FieldAD4(); }
    void field_a_d4(std::int32_t value) { FieldAD4(value); }
    [[nodiscard]] std::int32_t FieldAD8() const { return read_i32(0xAD8); }
    void FieldAD8(std::int32_t value) { write_i32(0xAD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_d8() const { return FieldAD8(); }
    void field_a_d8(std::int32_t value) { FieldAD8(value); }
    [[nodiscard]] std::int32_t FieldADC() const { return read_i32(0xADC); }
    void FieldADC(std::int32_t value) { write_i32(0xADC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_d_c() const { return FieldADC(); }
    void field_a_d_c(std::int32_t value) { FieldADC(value); }
    [[nodiscard]] std::int32_t FieldAE0() const { return read_i32(0xAE0); }
    void FieldAE0(std::int32_t value) { write_i32(0xAE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_e0() const { return FieldAE0(); }
    void field_a_e0(std::int32_t value) { FieldAE0(value); }
    [[nodiscard]] std::int32_t FieldAE4() const { return read_i32(0xAE4); }
    void FieldAE4(std::int32_t value) { write_i32(0xAE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_e4() const { return FieldAE4(); }
    void field_a_e4(std::int32_t value) { FieldAE4(value); }
    [[nodiscard]] std::int32_t FieldAE8() const { return read_i32(0xAE8); }
    void FieldAE8(std::int32_t value) { write_i32(0xAE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_e8() const { return FieldAE8(); }
    void field_a_e8(std::int32_t value) { FieldAE8(value); }
    [[nodiscard]] std::int32_t FieldAEC() const { return read_i32(0xAEC); }
    void FieldAEC(std::int32_t value) { write_i32(0xAEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_e_c() const { return FieldAEC(); }
    void field_a_e_c(std::int32_t value) { FieldAEC(value); }
    [[nodiscard]] std::int32_t FieldAF0() const { return read_i32(0xAF0); }
    void FieldAF0(std::int32_t value) { write_i32(0xAF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_f0() const { return FieldAF0(); }
    void field_a_f0(std::int32_t value) { FieldAF0(value); }
    [[nodiscard]] std::int32_t FieldAF4() const { return read_i32(0xAF4); }
    void FieldAF4(std::int32_t value) { write_i32(0xAF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_f4() const { return FieldAF4(); }
    void field_a_f4(std::int32_t value) { FieldAF4(value); }
    [[nodiscard]] std::int32_t FieldAF8() const { return read_i32(0xAF8); }
    void FieldAF8(std::int32_t value) { write_i32(0xAF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_f8() const { return FieldAF8(); }
    void field_a_f8(std::int32_t value) { FieldAF8(value); }
    [[nodiscard]] std::int32_t FieldAFC() const { return read_i32(0xAFC); }
    void FieldAFC(std::int32_t value) { write_i32(0xAFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_f_c() const { return FieldAFC(); }
    void field_a_f_c(std::int32_t value) { FieldAFC(value); }
    [[nodiscard]] std::int32_t FieldB00() const { return read_i32(0xB00); }
    void FieldB00(std::int32_t value) { write_i32(0xB00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b00() const { return FieldB00(); }
    void field_b00(std::int32_t value) { FieldB00(value); }
    [[nodiscard]] std::int32_t FieldB04() const { return read_i32(0xB04); }
    void FieldB04(std::int32_t value) { write_i32(0xB04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b04() const { return FieldB04(); }
    void field_b04(std::int32_t value) { FieldB04(value); }
    [[nodiscard]] std::int32_t FieldB08() const { return read_i32(0xB08); }
    void FieldB08(std::int32_t value) { write_i32(0xB08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b08() const { return FieldB08(); }
    void field_b08(std::int32_t value) { FieldB08(value); }
    [[nodiscard]] std::int32_t FieldB0C() const { return read_i32(0xB0C); }
    void FieldB0C(std::int32_t value) { write_i32(0xB0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b0_c() const { return FieldB0C(); }
    void field_b0_c(std::int32_t value) { FieldB0C(value); }
    [[nodiscard]] std::int32_t FieldB10() const { return read_i32(0xB10); }
    void FieldB10(std::int32_t value) { write_i32(0xB10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b10() const { return FieldB10(); }
    void field_b10(std::int32_t value) { FieldB10(value); }
    [[nodiscard]] std::int32_t FieldB14() const { return read_i32(0xB14); }
    void FieldB14(std::int32_t value) { write_i32(0xB14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b14() const { return FieldB14(); }
    void field_b14(std::int32_t value) { FieldB14(value); }
    [[nodiscard]] std::int32_t FieldB18() const { return read_i32(0xB18); }
    void FieldB18(std::int32_t value) { write_i32(0xB18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b18() const { return FieldB18(); }
    void field_b18(std::int32_t value) { FieldB18(value); }
    [[nodiscard]] std::int32_t FieldB1C() const { return read_i32(0xB1C); }
    void FieldB1C(std::int32_t value) { write_i32(0xB1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b1_c() const { return FieldB1C(); }
    void field_b1_c(std::int32_t value) { FieldB1C(value); }
    [[nodiscard]] std::int32_t FieldB20() const { return read_i32(0xB20); }
    void FieldB20(std::int32_t value) { write_i32(0xB20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b20() const { return FieldB20(); }
    void field_b20(std::int32_t value) { FieldB20(value); }
    [[nodiscard]] std::int32_t FieldB24() const { return read_i32(0xB24); }
    void FieldB24(std::int32_t value) { write_i32(0xB24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b24() const { return FieldB24(); }
    void field_b24(std::int32_t value) { FieldB24(value); }
    [[nodiscard]] std::int32_t FieldB28() const { return read_i32(0xB28); }
    void FieldB28(std::int32_t value) { write_i32(0xB28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b28() const { return FieldB28(); }
    void field_b28(std::int32_t value) { FieldB28(value); }
    [[nodiscard]] std::int32_t FieldB2C() const { return read_i32(0xB2C); }
    void FieldB2C(std::int32_t value) { write_i32(0xB2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b2_c() const { return FieldB2C(); }
    void field_b2_c(std::int32_t value) { FieldB2C(value); }
    [[nodiscard]] std::int32_t FieldB30() const { return read_i32(0xB30); }
    void FieldB30(std::int32_t value) { write_i32(0xB30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b30() const { return FieldB30(); }
    void field_b30(std::int32_t value) { FieldB30(value); }
    [[nodiscard]] std::int32_t FieldB34() const { return read_i32(0xB34); }
    void FieldB34(std::int32_t value) { write_i32(0xB34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b34() const { return FieldB34(); }
    void field_b34(std::int32_t value) { FieldB34(value); }
    [[nodiscard]] std::int32_t FieldB38() const { return read_i32(0xB38); }
    void FieldB38(std::int32_t value) { write_i32(0xB38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b38() const { return FieldB38(); }
    void field_b38(std::int32_t value) { FieldB38(value); }
    [[nodiscard]] std::int32_t FieldB3C() const { return read_i32(0xB3C); }
    void FieldB3C(std::int32_t value) { write_i32(0xB3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b3_c() const { return FieldB3C(); }
    void field_b3_c(std::int32_t value) { FieldB3C(value); }
    [[nodiscard]] std::int32_t FieldB40() const { return read_i32(0xB40); }
    void FieldB40(std::int32_t value) { write_i32(0xB40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b40() const { return FieldB40(); }
    void field_b40(std::int32_t value) { FieldB40(value); }
    [[nodiscard]] std::int32_t FieldB44() const { return read_i32(0xB44); }
    void FieldB44(std::int32_t value) { write_i32(0xB44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b44() const { return FieldB44(); }
    void field_b44(std::int32_t value) { FieldB44(value); }
    [[nodiscard]] std::int32_t FieldB48() const { return read_i32(0xB48); }
    void FieldB48(std::int32_t value) { write_i32(0xB48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b48() const { return FieldB48(); }
    void field_b48(std::int32_t value) { FieldB48(value); }
    [[nodiscard]] std::int32_t FieldB4C() const { return read_i32(0xB4C); }
    void FieldB4C(std::int32_t value) { write_i32(0xB4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b4_c() const { return FieldB4C(); }
    void field_b4_c(std::int32_t value) { FieldB4C(value); }
    [[nodiscard]] std::int32_t FieldB50() const { return read_i32(0xB50); }
    void FieldB50(std::int32_t value) { write_i32(0xB50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b50() const { return FieldB50(); }
    void field_b50(std::int32_t value) { FieldB50(value); }
    [[nodiscard]] std::int32_t FieldB54() const { return read_i32(0xB54); }
    void FieldB54(std::int32_t value) { write_i32(0xB54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b54() const { return FieldB54(); }
    void field_b54(std::int32_t value) { FieldB54(value); }
    [[nodiscard]] std::int32_t FieldB58() const { return read_i32(0xB58); }
    void FieldB58(std::int32_t value) { write_i32(0xB58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b58() const { return FieldB58(); }
    void field_b58(std::int32_t value) { FieldB58(value); }
    [[nodiscard]] std::int32_t FieldB5C() const { return read_i32(0xB5C); }
    void FieldB5C(std::int32_t value) { write_i32(0xB5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b5_c() const { return FieldB5C(); }
    void field_b5_c(std::int32_t value) { FieldB5C(value); }
    [[nodiscard]] std::int32_t FieldB60() const { return read_i32(0xB60); }
    void FieldB60(std::int32_t value) { write_i32(0xB60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b60() const { return FieldB60(); }
    void field_b60(std::int32_t value) { FieldB60(value); }
    [[nodiscard]] std::int32_t FieldB64() const { return read_i32(0xB64); }
    void FieldB64(std::int32_t value) { write_i32(0xB64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b64() const { return FieldB64(); }
    void field_b64(std::int32_t value) { FieldB64(value); }
    [[nodiscard]] std::int32_t FieldB68() const { return read_i32(0xB68); }
    void FieldB68(std::int32_t value) { write_i32(0xB68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b68() const { return FieldB68(); }
    void field_b68(std::int32_t value) { FieldB68(value); }
    [[nodiscard]] std::int32_t FieldB6C() const { return read_i32(0xB6C); }
    void FieldB6C(std::int32_t value) { write_i32(0xB6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b6_c() const { return FieldB6C(); }
    void field_b6_c(std::int32_t value) { FieldB6C(value); }
    [[nodiscard]] std::int32_t FieldB70() const { return read_i32(0xB70); }
    void FieldB70(std::int32_t value) { write_i32(0xB70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b70() const { return FieldB70(); }
    void field_b70(std::int32_t value) { FieldB70(value); }
    [[nodiscard]] std::int32_t FieldB74() const { return read_i32(0xB74); }
    void FieldB74(std::int32_t value) { write_i32(0xB74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b74() const { return FieldB74(); }
    void field_b74(std::int32_t value) { FieldB74(value); }
    [[nodiscard]] std::int32_t FieldB78() const { return read_i32(0xB78); }
    void FieldB78(std::int32_t value) { write_i32(0xB78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b78() const { return FieldB78(); }
    void field_b78(std::int32_t value) { FieldB78(value); }
    [[nodiscard]] std::int32_t FieldB7C() const { return read_i32(0xB7C); }
    void FieldB7C(std::int32_t value) { write_i32(0xB7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b7_c() const { return FieldB7C(); }
    void field_b7_c(std::int32_t value) { FieldB7C(value); }
    [[nodiscard]] std::int32_t FieldB80() const { return read_i32(0xB80); }
    void FieldB80(std::int32_t value) { write_i32(0xB80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b80() const { return FieldB80(); }
    void field_b80(std::int32_t value) { FieldB80(value); }
    [[nodiscard]] std::int32_t FieldB84() const { return read_i32(0xB84); }
    void FieldB84(std::int32_t value) { write_i32(0xB84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b84() const { return FieldB84(); }
    void field_b84(std::int32_t value) { FieldB84(value); }
    [[nodiscard]] std::int32_t FieldB88() const { return read_i32(0xB88); }
    void FieldB88(std::int32_t value) { write_i32(0xB88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b88() const { return FieldB88(); }
    void field_b88(std::int32_t value) { FieldB88(value); }
    [[nodiscard]] std::int32_t FieldB8C() const { return read_i32(0xB8C); }
    void FieldB8C(std::int32_t value) { write_i32(0xB8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b8_c() const { return FieldB8C(); }
    void field_b8_c(std::int32_t value) { FieldB8C(value); }
    [[nodiscard]] std::int32_t FieldB90() const { return read_i32(0xB90); }
    void FieldB90(std::int32_t value) { write_i32(0xB90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b90() const { return FieldB90(); }
    void field_b90(std::int32_t value) { FieldB90(value); }
    [[nodiscard]] std::int32_t FieldB94() const { return read_i32(0xB94); }
    void FieldB94(std::int32_t value) { write_i32(0xB94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b94() const { return FieldB94(); }
    void field_b94(std::int32_t value) { FieldB94(value); }
    [[nodiscard]] std::int32_t FieldB98() const { return read_i32(0xB98); }
    void FieldB98(std::int32_t value) { write_i32(0xB98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b98() const { return FieldB98(); }
    void field_b98(std::int32_t value) { FieldB98(value); }
    [[nodiscard]] std::int32_t FieldB9C() const { return read_i32(0xB9C); }
    void FieldB9C(std::int32_t value) { write_i32(0xB9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b9_c() const { return FieldB9C(); }
    void field_b9_c(std::int32_t value) { FieldB9C(value); }
    [[nodiscard]] std::int32_t FieldBA0() const { return read_i32(0xBA0); }
    void FieldBA0(std::int32_t value) { write_i32(0xBA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_a0() const { return FieldBA0(); }
    void field_b_a0(std::int32_t value) { FieldBA0(value); }
    [[nodiscard]] std::int32_t FieldBA4() const { return read_i32(0xBA4); }
    void FieldBA4(std::int32_t value) { write_i32(0xBA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_a4() const { return FieldBA4(); }
    void field_b_a4(std::int32_t value) { FieldBA4(value); }
    [[nodiscard]] std::int32_t FieldBA8() const { return read_i32(0xBA8); }
    void FieldBA8(std::int32_t value) { write_i32(0xBA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_a8() const { return FieldBA8(); }
    void field_b_a8(std::int32_t value) { FieldBA8(value); }
    [[nodiscard]] std::int32_t FieldBAC() const { return read_i32(0xBAC); }
    void FieldBAC(std::int32_t value) { write_i32(0xBAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_a_c() const { return FieldBAC(); }
    void field_b_a_c(std::int32_t value) { FieldBAC(value); }
    [[nodiscard]] std::int32_t FieldBB0() const { return read_i32(0xBB0); }
    void FieldBB0(std::int32_t value) { write_i32(0xBB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_b0() const { return FieldBB0(); }
    void field_b_b0(std::int32_t value) { FieldBB0(value); }
    [[nodiscard]] std::int32_t FieldBB4() const { return read_i32(0xBB4); }
    void FieldBB4(std::int32_t value) { write_i32(0xBB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_b4() const { return FieldBB4(); }
    void field_b_b4(std::int32_t value) { FieldBB4(value); }
    [[nodiscard]] std::int32_t FieldBB8() const { return read_i32(0xBB8); }
    void FieldBB8(std::int32_t value) { write_i32(0xBB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_b8() const { return FieldBB8(); }
    void field_b_b8(std::int32_t value) { FieldBB8(value); }
    [[nodiscard]] std::int32_t FieldBBC() const { return read_i32(0xBBC); }
    void FieldBBC(std::int32_t value) { write_i32(0xBBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_b_c() const { return FieldBBC(); }
    void field_b_b_c(std::int32_t value) { FieldBBC(value); }
    [[nodiscard]] std::int32_t FieldBC0() const { return read_i32(0xBC0); }
    void FieldBC0(std::int32_t value) { write_i32(0xBC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_c0() const { return FieldBC0(); }
    void field_b_c0(std::int32_t value) { FieldBC0(value); }
    [[nodiscard]] std::int32_t FieldBC4() const { return read_i32(0xBC4); }
    void FieldBC4(std::int32_t value) { write_i32(0xBC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_c4() const { return FieldBC4(); }
    void field_b_c4(std::int32_t value) { FieldBC4(value); }
    [[nodiscard]] std::int32_t FieldBC8() const { return read_i32(0xBC8); }
    void FieldBC8(std::int32_t value) { write_i32(0xBC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_c8() const { return FieldBC8(); }
    void field_b_c8(std::int32_t value) { FieldBC8(value); }
    [[nodiscard]] std::int32_t FieldBCC() const { return read_i32(0xBCC); }
    void FieldBCC(std::int32_t value) { write_i32(0xBCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_c_c() const { return FieldBCC(); }
    void field_b_c_c(std::int32_t value) { FieldBCC(value); }
    [[nodiscard]] std::int32_t FieldBD0() const { return read_i32(0xBD0); }
    void FieldBD0(std::int32_t value) { write_i32(0xBD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_d0() const { return FieldBD0(); }
    void field_b_d0(std::int32_t value) { FieldBD0(value); }
    [[nodiscard]] std::int32_t FieldBD4() const { return read_i32(0xBD4); }
    void FieldBD4(std::int32_t value) { write_i32(0xBD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_d4() const { return FieldBD4(); }
    void field_b_d4(std::int32_t value) { FieldBD4(value); }
    [[nodiscard]] std::int32_t FieldBD8() const { return read_i32(0xBD8); }
    void FieldBD8(std::int32_t value) { write_i32(0xBD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_d8() const { return FieldBD8(); }
    void field_b_d8(std::int32_t value) { FieldBD8(value); }
    [[nodiscard]] std::int32_t FieldBDC() const { return read_i32(0xBDC); }
    void FieldBDC(std::int32_t value) { write_i32(0xBDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_d_c() const { return FieldBDC(); }
    void field_b_d_c(std::int32_t value) { FieldBDC(value); }
    [[nodiscard]] std::int32_t FieldBE0() const { return read_i32(0xBE0); }
    void FieldBE0(std::int32_t value) { write_i32(0xBE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_e0() const { return FieldBE0(); }
    void field_b_e0(std::int32_t value) { FieldBE0(value); }
    [[nodiscard]] std::int32_t FieldBE4() const { return read_i32(0xBE4); }
    void FieldBE4(std::int32_t value) { write_i32(0xBE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_e4() const { return FieldBE4(); }
    void field_b_e4(std::int32_t value) { FieldBE4(value); }
    [[nodiscard]] std::int32_t FieldBE8() const { return read_i32(0xBE8); }
    void FieldBE8(std::int32_t value) { write_i32(0xBE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_e8() const { return FieldBE8(); }
    void field_b_e8(std::int32_t value) { FieldBE8(value); }
    [[nodiscard]] std::int32_t FieldBEC() const { return read_i32(0xBEC); }
    void FieldBEC(std::int32_t value) { write_i32(0xBEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_e_c() const { return FieldBEC(); }
    void field_b_e_c(std::int32_t value) { FieldBEC(value); }
    [[nodiscard]] std::int32_t FieldBF0() const { return read_i32(0xBF0); }
    void FieldBF0(std::int32_t value) { write_i32(0xBF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_f0() const { return FieldBF0(); }
    void field_b_f0(std::int32_t value) { FieldBF0(value); }
    [[nodiscard]] std::int32_t FieldBF4() const { return read_i32(0xBF4); }
    void FieldBF4(std::int32_t value) { write_i32(0xBF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_f4() const { return FieldBF4(); }
    void field_b_f4(std::int32_t value) { FieldBF4(value); }
    [[nodiscard]] std::int32_t FieldBF8() const { return read_i32(0xBF8); }
    void FieldBF8(std::int32_t value) { write_i32(0xBF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_f8() const { return FieldBF8(); }
    void field_b_f8(std::int32_t value) { FieldBF8(value); }
    [[nodiscard]] std::int32_t FieldBFC() const { return read_i32(0xBFC); }
    void FieldBFC(std::int32_t value) { write_i32(0xBFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b_f_c() const { return FieldBFC(); }
    void field_b_f_c(std::int32_t value) { FieldBFC(value); }
    [[nodiscard]] std::int32_t FieldC00() const { return read_i32(0xC00); }
    void FieldC00(std::int32_t value) { write_i32(0xC00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c00() const { return FieldC00(); }
    void field_c00(std::int32_t value) { FieldC00(value); }
    [[nodiscard]] std::int32_t FieldC04() const { return read_i32(0xC04); }
    void FieldC04(std::int32_t value) { write_i32(0xC04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c04() const { return FieldC04(); }
    void field_c04(std::int32_t value) { FieldC04(value); }
    [[nodiscard]] std::int32_t FieldC08() const { return read_i32(0xC08); }
    void FieldC08(std::int32_t value) { write_i32(0xC08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c08() const { return FieldC08(); }
    void field_c08(std::int32_t value) { FieldC08(value); }
    [[nodiscard]] std::int32_t FieldC0C() const { return read_i32(0xC0C); }
    void FieldC0C(std::int32_t value) { write_i32(0xC0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c0_c() const { return FieldC0C(); }
    void field_c0_c(std::int32_t value) { FieldC0C(value); }
    [[nodiscard]] std::int32_t FieldC10() const { return read_i32(0xC10); }
    void FieldC10(std::int32_t value) { write_i32(0xC10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c10() const { return FieldC10(); }
    void field_c10(std::int32_t value) { FieldC10(value); }
    [[nodiscard]] std::int32_t FieldC14() const { return read_i32(0xC14); }
    void FieldC14(std::int32_t value) { write_i32(0xC14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c14() const { return FieldC14(); }
    void field_c14(std::int32_t value) { FieldC14(value); }
    [[nodiscard]] std::int32_t FieldC18() const { return read_i32(0xC18); }
    void FieldC18(std::int32_t value) { write_i32(0xC18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c18() const { return FieldC18(); }
    void field_c18(std::int32_t value) { FieldC18(value); }
    [[nodiscard]] std::int32_t FieldC1C() const { return read_i32(0xC1C); }
    void FieldC1C(std::int32_t value) { write_i32(0xC1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c1_c() const { return FieldC1C(); }
    void field_c1_c(std::int32_t value) { FieldC1C(value); }
    [[nodiscard]] std::int32_t FieldC20() const { return read_i32(0xC20); }
    void FieldC20(std::int32_t value) { write_i32(0xC20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c20() const { return FieldC20(); }
    void field_c20(std::int32_t value) { FieldC20(value); }
    [[nodiscard]] std::int32_t FieldC24() const { return read_i32(0xC24); }
    void FieldC24(std::int32_t value) { write_i32(0xC24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c24() const { return FieldC24(); }
    void field_c24(std::int32_t value) { FieldC24(value); }
    [[nodiscard]] std::int32_t FieldC28() const { return read_i32(0xC28); }
    void FieldC28(std::int32_t value) { write_i32(0xC28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c28() const { return FieldC28(); }
    void field_c28(std::int32_t value) { FieldC28(value); }
    [[nodiscard]] std::int32_t FieldC2C() const { return read_i32(0xC2C); }
    void FieldC2C(std::int32_t value) { write_i32(0xC2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c2_c() const { return FieldC2C(); }
    void field_c2_c(std::int32_t value) { FieldC2C(value); }
    [[nodiscard]] std::int32_t FieldC30() const { return read_i32(0xC30); }
    void FieldC30(std::int32_t value) { write_i32(0xC30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c30() const { return FieldC30(); }
    void field_c30(std::int32_t value) { FieldC30(value); }
    [[nodiscard]] std::int32_t FieldC34() const { return read_i32(0xC34); }
    void FieldC34(std::int32_t value) { write_i32(0xC34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c34() const { return FieldC34(); }
    void field_c34(std::int32_t value) { FieldC34(value); }
    [[nodiscard]] std::int32_t FieldC38() const { return read_i32(0xC38); }
    void FieldC38(std::int32_t value) { write_i32(0xC38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c38() const { return FieldC38(); }
    void field_c38(std::int32_t value) { FieldC38(value); }
    [[nodiscard]] std::int32_t FieldC3C() const { return read_i32(0xC3C); }
    void FieldC3C(std::int32_t value) { write_i32(0xC3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c3_c() const { return FieldC3C(); }
    void field_c3_c(std::int32_t value) { FieldC3C(value); }
    [[nodiscard]] std::int32_t FieldC40() const { return read_i32(0xC40); }
    void FieldC40(std::int32_t value) { write_i32(0xC40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c40() const { return FieldC40(); }
    void field_c40(std::int32_t value) { FieldC40(value); }
    [[nodiscard]] std::int32_t FieldC44() const { return read_i32(0xC44); }
    void FieldC44(std::int32_t value) { write_i32(0xC44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c44() const { return FieldC44(); }
    void field_c44(std::int32_t value) { FieldC44(value); }
    [[nodiscard]] std::int32_t FieldC48() const { return read_i32(0xC48); }
    void FieldC48(std::int32_t value) { write_i32(0xC48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c48() const { return FieldC48(); }
    void field_c48(std::int32_t value) { FieldC48(value); }
    [[nodiscard]] std::int32_t FieldC4C() const { return read_i32(0xC4C); }
    void FieldC4C(std::int32_t value) { write_i32(0xC4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c4_c() const { return FieldC4C(); }
    void field_c4_c(std::int32_t value) { FieldC4C(value); }
    [[nodiscard]] std::int32_t FieldC50() const { return read_i32(0xC50); }
    void FieldC50(std::int32_t value) { write_i32(0xC50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c50() const { return FieldC50(); }
    void field_c50(std::int32_t value) { FieldC50(value); }
    [[nodiscard]] std::int32_t FieldC54() const { return read_i32(0xC54); }
    void FieldC54(std::int32_t value) { write_i32(0xC54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c54() const { return FieldC54(); }
    void field_c54(std::int32_t value) { FieldC54(value); }
    [[nodiscard]] std::int32_t FieldC58() const { return read_i32(0xC58); }
    void FieldC58(std::int32_t value) { write_i32(0xC58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c58() const { return FieldC58(); }
    void field_c58(std::int32_t value) { FieldC58(value); }
    [[nodiscard]] std::int32_t FieldC5C() const { return read_i32(0xC5C); }
    void FieldC5C(std::int32_t value) { write_i32(0xC5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c5_c() const { return FieldC5C(); }
    void field_c5_c(std::int32_t value) { FieldC5C(value); }
    [[nodiscard]] std::int32_t FieldC60() const { return read_i32(0xC60); }
    void FieldC60(std::int32_t value) { write_i32(0xC60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c60() const { return FieldC60(); }
    void field_c60(std::int32_t value) { FieldC60(value); }
    [[nodiscard]] std::int32_t FieldC64() const { return read_i32(0xC64); }
    void FieldC64(std::int32_t value) { write_i32(0xC64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c64() const { return FieldC64(); }
    void field_c64(std::int32_t value) { FieldC64(value); }
    [[nodiscard]] std::int32_t FieldC68() const { return read_i32(0xC68); }
    void FieldC68(std::int32_t value) { write_i32(0xC68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c68() const { return FieldC68(); }
    void field_c68(std::int32_t value) { FieldC68(value); }
    [[nodiscard]] std::int32_t FieldC6C() const { return read_i32(0xC6C); }
    void FieldC6C(std::int32_t value) { write_i32(0xC6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c6_c() const { return FieldC6C(); }
    void field_c6_c(std::int32_t value) { FieldC6C(value); }
    [[nodiscard]] std::int32_t FieldC70() const { return read_i32(0xC70); }
    void FieldC70(std::int32_t value) { write_i32(0xC70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c70() const { return FieldC70(); }
    void field_c70(std::int32_t value) { FieldC70(value); }
    [[nodiscard]] std::int32_t FieldC74() const { return read_i32(0xC74); }
    void FieldC74(std::int32_t value) { write_i32(0xC74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c74() const { return FieldC74(); }
    void field_c74(std::int32_t value) { FieldC74(value); }
    [[nodiscard]] std::int32_t FieldC78() const { return read_i32(0xC78); }
    void FieldC78(std::int32_t value) { write_i32(0xC78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c78() const { return FieldC78(); }
    void field_c78(std::int32_t value) { FieldC78(value); }
    [[nodiscard]] std::int32_t FieldC7C() const { return read_i32(0xC7C); }
    void FieldC7C(std::int32_t value) { write_i32(0xC7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c7_c() const { return FieldC7C(); }
    void field_c7_c(std::int32_t value) { FieldC7C(value); }
    [[nodiscard]] std::int32_t FieldC80() const { return read_i32(0xC80); }
    void FieldC80(std::int32_t value) { write_i32(0xC80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c80() const { return FieldC80(); }
    void field_c80(std::int32_t value) { FieldC80(value); }
    [[nodiscard]] std::int32_t FieldC84() const { return read_i32(0xC84); }
    void FieldC84(std::int32_t value) { write_i32(0xC84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c84() const { return FieldC84(); }
    void field_c84(std::int32_t value) { FieldC84(value); }
    [[nodiscard]] std::int32_t FieldC88() const { return read_i32(0xC88); }
    void FieldC88(std::int32_t value) { write_i32(0xC88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c88() const { return FieldC88(); }
    void field_c88(std::int32_t value) { FieldC88(value); }
    [[nodiscard]] std::int32_t FieldC8C() const { return read_i32(0xC8C); }
    void FieldC8C(std::int32_t value) { write_i32(0xC8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c8_c() const { return FieldC8C(); }
    void field_c8_c(std::int32_t value) { FieldC8C(value); }
    [[nodiscard]] std::int32_t FieldC90() const { return read_i32(0xC90); }
    void FieldC90(std::int32_t value) { write_i32(0xC90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c90() const { return FieldC90(); }
    void field_c90(std::int32_t value) { FieldC90(value); }
    [[nodiscard]] std::int32_t FieldC94() const { return read_i32(0xC94); }
    void FieldC94(std::int32_t value) { write_i32(0xC94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c94() const { return FieldC94(); }
    void field_c94(std::int32_t value) { FieldC94(value); }
    [[nodiscard]] std::int32_t FieldC98() const { return read_i32(0xC98); }
    void FieldC98(std::int32_t value) { write_i32(0xC98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c98() const { return FieldC98(); }
    void field_c98(std::int32_t value) { FieldC98(value); }
    [[nodiscard]] std::int32_t FieldC9C() const { return read_i32(0xC9C); }
    void FieldC9C(std::int32_t value) { write_i32(0xC9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c9_c() const { return FieldC9C(); }
    void field_c9_c(std::int32_t value) { FieldC9C(value); }
    [[nodiscard]] std::int32_t FieldCA0() const { return read_i32(0xCA0); }
    void FieldCA0(std::int32_t value) { write_i32(0xCA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_a0() const { return FieldCA0(); }
    void field_c_a0(std::int32_t value) { FieldCA0(value); }
    [[nodiscard]] std::int32_t FieldCA4() const { return read_i32(0xCA4); }
    void FieldCA4(std::int32_t value) { write_i32(0xCA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_a4() const { return FieldCA4(); }
    void field_c_a4(std::int32_t value) { FieldCA4(value); }
    [[nodiscard]] std::int32_t FieldCA8() const { return read_i32(0xCA8); }
    void FieldCA8(std::int32_t value) { write_i32(0xCA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_a8() const { return FieldCA8(); }
    void field_c_a8(std::int32_t value) { FieldCA8(value); }
    [[nodiscard]] std::int32_t FieldCAC() const { return read_i32(0xCAC); }
    void FieldCAC(std::int32_t value) { write_i32(0xCAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_a_c() const { return FieldCAC(); }
    void field_c_a_c(std::int32_t value) { FieldCAC(value); }
    [[nodiscard]] std::int32_t FieldCB0() const { return read_i32(0xCB0); }
    void FieldCB0(std::int32_t value) { write_i32(0xCB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_b0() const { return FieldCB0(); }
    void field_c_b0(std::int32_t value) { FieldCB0(value); }
    [[nodiscard]] std::int32_t FieldCB4() const { return read_i32(0xCB4); }
    void FieldCB4(std::int32_t value) { write_i32(0xCB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_b4() const { return FieldCB4(); }
    void field_c_b4(std::int32_t value) { FieldCB4(value); }
    [[nodiscard]] std::int32_t FieldCB8() const { return read_i32(0xCB8); }
    void FieldCB8(std::int32_t value) { write_i32(0xCB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_b8() const { return FieldCB8(); }
    void field_c_b8(std::int32_t value) { FieldCB8(value); }
    [[nodiscard]] std::int32_t FieldCBC() const { return read_i32(0xCBC); }
    void FieldCBC(std::int32_t value) { write_i32(0xCBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_b_c() const { return FieldCBC(); }
    void field_c_b_c(std::int32_t value) { FieldCBC(value); }
    [[nodiscard]] std::int32_t FieldCC0() const { return read_i32(0xCC0); }
    void FieldCC0(std::int32_t value) { write_i32(0xCC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c0() const { return FieldCC0(); }
    void field_c_c0(std::int32_t value) { FieldCC0(value); }
    [[nodiscard]] std::int32_t FieldCC4() const { return read_i32(0xCC4); }
    void FieldCC4(std::int32_t value) { write_i32(0xCC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c4() const { return FieldCC4(); }
    void field_c_c4(std::int32_t value) { FieldCC4(value); }
    [[nodiscard]] std::int32_t FieldCC8() const { return read_i32(0xCC8); }
    void FieldCC8(std::int32_t value) { write_i32(0xCC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c8() const { return FieldCC8(); }
    void field_c_c8(std::int32_t value) { FieldCC8(value); }
    [[nodiscard]] std::int32_t FieldCCC() const { return read_i32(0xCCC); }
    void FieldCCC(std::int32_t value) { write_i32(0xCCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c_c() const { return FieldCCC(); }
    void field_c_c_c(std::int32_t value) { FieldCCC(value); }
    [[nodiscard]] std::int32_t FieldCD0() const { return read_i32(0xCD0); }
    void FieldCD0(std::int32_t value) { write_i32(0xCD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_d0() const { return FieldCD0(); }
    void field_c_d0(std::int32_t value) { FieldCD0(value); }
    [[nodiscard]] std::int32_t FieldCD4() const { return read_i32(0xCD4); }
    void FieldCD4(std::int32_t value) { write_i32(0xCD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_d4() const { return FieldCD4(); }
    void field_c_d4(std::int32_t value) { FieldCD4(value); }
    [[nodiscard]] std::int32_t FieldCD8() const { return read_i32(0xCD8); }
    void FieldCD8(std::int32_t value) { write_i32(0xCD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_d8() const { return FieldCD8(); }
    void field_c_d8(std::int32_t value) { FieldCD8(value); }
    [[nodiscard]] std::int32_t FieldCDC() const { return read_i32(0xCDC); }
    void FieldCDC(std::int32_t value) { write_i32(0xCDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_d_c() const { return FieldCDC(); }
    void field_c_d_c(std::int32_t value) { FieldCDC(value); }
    [[nodiscard]] std::int32_t FieldCE0() const { return read_i32(0xCE0); }
    void FieldCE0(std::int32_t value) { write_i32(0xCE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_e0() const { return FieldCE0(); }
    void field_c_e0(std::int32_t value) { FieldCE0(value); }
    [[nodiscard]] std::int32_t FieldCE4() const { return read_i32(0xCE4); }
    void FieldCE4(std::int32_t value) { write_i32(0xCE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_e4() const { return FieldCE4(); }
    void field_c_e4(std::int32_t value) { FieldCE4(value); }
    [[nodiscard]] std::int32_t FieldCE8() const { return read_i32(0xCE8); }
    void FieldCE8(std::int32_t value) { write_i32(0xCE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_e8() const { return FieldCE8(); }
    void field_c_e8(std::int32_t value) { FieldCE8(value); }
    [[nodiscard]] std::int32_t FieldCEC() const { return read_i32(0xCEC); }
    void FieldCEC(std::int32_t value) { write_i32(0xCEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_e_c() const { return FieldCEC(); }
    void field_c_e_c(std::int32_t value) { FieldCEC(value); }
    [[nodiscard]] std::int32_t FieldCF0() const { return read_i32(0xCF0); }
    void FieldCF0(std::int32_t value) { write_i32(0xCF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_f0() const { return FieldCF0(); }
    void field_c_f0(std::int32_t value) { FieldCF0(value); }
    [[nodiscard]] std::int32_t FieldCF4() const { return read_i32(0xCF4); }
    void FieldCF4(std::int32_t value) { write_i32(0xCF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_f4() const { return FieldCF4(); }
    void field_c_f4(std::int32_t value) { FieldCF4(value); }
    [[nodiscard]] std::int32_t FieldCF8() const { return read_i32(0xCF8); }
    void FieldCF8(std::int32_t value) { write_i32(0xCF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_f8() const { return FieldCF8(); }
    void field_c_f8(std::int32_t value) { FieldCF8(value); }
    [[nodiscard]] std::int32_t FieldCFC() const { return read_i32(0xCFC); }
    void FieldCFC(std::int32_t value) { write_i32(0xCFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_f_c() const { return FieldCFC(); }
    void field_c_f_c(std::int32_t value) { FieldCFC(value); }
    [[nodiscard]] std::int32_t FieldD00() const { return read_i32(0xD00); }
    void FieldD00(std::int32_t value) { write_i32(0xD00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d00() const { return FieldD00(); }
    void field_d00(std::int32_t value) { FieldD00(value); }
    [[nodiscard]] std::int32_t FieldD04() const { return read_i32(0xD04); }
    void FieldD04(std::int32_t value) { write_i32(0xD04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d04() const { return FieldD04(); }
    void field_d04(std::int32_t value) { FieldD04(value); }
    [[nodiscard]] std::int32_t FieldD08() const { return read_i32(0xD08); }
    void FieldD08(std::int32_t value) { write_i32(0xD08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d08() const { return FieldD08(); }
    void field_d08(std::int32_t value) { FieldD08(value); }
    [[nodiscard]] std::int32_t FieldD0C() const { return read_i32(0xD0C); }
    void FieldD0C(std::int32_t value) { write_i32(0xD0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d0_c() const { return FieldD0C(); }
    void field_d0_c(std::int32_t value) { FieldD0C(value); }
    [[nodiscard]] std::int32_t FieldD10() const { return read_i32(0xD10); }
    void FieldD10(std::int32_t value) { write_i32(0xD10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d10() const { return FieldD10(); }
    void field_d10(std::int32_t value) { FieldD10(value); }
    [[nodiscard]] std::int32_t FieldD14() const { return read_i32(0xD14); }
    void FieldD14(std::int32_t value) { write_i32(0xD14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d14() const { return FieldD14(); }
    void field_d14(std::int32_t value) { FieldD14(value); }
    [[nodiscard]] std::int32_t FieldD18() const { return read_i32(0xD18); }
    void FieldD18(std::int32_t value) { write_i32(0xD18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d18() const { return FieldD18(); }
    void field_d18(std::int32_t value) { FieldD18(value); }
    [[nodiscard]] std::int32_t FieldD1C() const { return read_i32(0xD1C); }
    void FieldD1C(std::int32_t value) { write_i32(0xD1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d1_c() const { return FieldD1C(); }
    void field_d1_c(std::int32_t value) { FieldD1C(value); }
    [[nodiscard]] std::int32_t FieldD20() const { return read_i32(0xD20); }
    void FieldD20(std::int32_t value) { write_i32(0xD20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d20() const { return FieldD20(); }
    void field_d20(std::int32_t value) { FieldD20(value); }
    [[nodiscard]] std::int32_t FieldD24() const { return read_i32(0xD24); }
    void FieldD24(std::int32_t value) { write_i32(0xD24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d24() const { return FieldD24(); }
    void field_d24(std::int32_t value) { FieldD24(value); }
    [[nodiscard]] std::int32_t FieldD28() const { return read_i32(0xD28); }
    void FieldD28(std::int32_t value) { write_i32(0xD28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d28() const { return FieldD28(); }
    void field_d28(std::int32_t value) { FieldD28(value); }
    [[nodiscard]] std::int32_t FieldD2C() const { return read_i32(0xD2C); }
    void FieldD2C(std::int32_t value) { write_i32(0xD2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d2_c() const { return FieldD2C(); }
    void field_d2_c(std::int32_t value) { FieldD2C(value); }
    [[nodiscard]] std::int32_t FieldD30() const { return read_i32(0xD30); }
    void FieldD30(std::int32_t value) { write_i32(0xD30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d30() const { return FieldD30(); }
    void field_d30(std::int32_t value) { FieldD30(value); }
    [[nodiscard]] std::int32_t FieldD34() const { return read_i32(0xD34); }
    void FieldD34(std::int32_t value) { write_i32(0xD34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d34() const { return FieldD34(); }
    void field_d34(std::int32_t value) { FieldD34(value); }
    [[nodiscard]] std::int32_t FieldD38() const { return read_i32(0xD38); }
    void FieldD38(std::int32_t value) { write_i32(0xD38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d38() const { return FieldD38(); }
    void field_d38(std::int32_t value) { FieldD38(value); }
    [[nodiscard]] std::int32_t FieldD3C() const { return read_i32(0xD3C); }
    void FieldD3C(std::int32_t value) { write_i32(0xD3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d3_c() const { return FieldD3C(); }
    void field_d3_c(std::int32_t value) { FieldD3C(value); }
    [[nodiscard]] std::int32_t FieldD40() const { return read_i32(0xD40); }
    void FieldD40(std::int32_t value) { write_i32(0xD40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d40() const { return FieldD40(); }
    void field_d40(std::int32_t value) { FieldD40(value); }
    [[nodiscard]] std::int32_t FieldD44() const { return read_i32(0xD44); }
    void FieldD44(std::int32_t value) { write_i32(0xD44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d44() const { return FieldD44(); }
    void field_d44(std::int32_t value) { FieldD44(value); }
    [[nodiscard]] std::int32_t FieldD48() const { return read_i32(0xD48); }
    void FieldD48(std::int32_t value) { write_i32(0xD48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d48() const { return FieldD48(); }
    void field_d48(std::int32_t value) { FieldD48(value); }
    [[nodiscard]] std::int32_t FieldD4C() const { return read_i32(0xD4C); }
    void FieldD4C(std::int32_t value) { write_i32(0xD4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d4_c() const { return FieldD4C(); }
    void field_d4_c(std::int32_t value) { FieldD4C(value); }
    [[nodiscard]] std::int32_t FieldD50() const { return read_i32(0xD50); }
    void FieldD50(std::int32_t value) { write_i32(0xD50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d50() const { return FieldD50(); }
    void field_d50(std::int32_t value) { FieldD50(value); }
    [[nodiscard]] std::int32_t FieldD54() const { return read_i32(0xD54); }
    void FieldD54(std::int32_t value) { write_i32(0xD54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d54() const { return FieldD54(); }
    void field_d54(std::int32_t value) { FieldD54(value); }
    [[nodiscard]] std::int32_t FieldD58() const { return read_i32(0xD58); }
    void FieldD58(std::int32_t value) { write_i32(0xD58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d58() const { return FieldD58(); }
    void field_d58(std::int32_t value) { FieldD58(value); }
    [[nodiscard]] std::int32_t FieldD5C() const { return read_i32(0xD5C); }
    void FieldD5C(std::int32_t value) { write_i32(0xD5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d5_c() const { return FieldD5C(); }
    void field_d5_c(std::int32_t value) { FieldD5C(value); }
    [[nodiscard]] std::int32_t FieldD60() const { return read_i32(0xD60); }
    void FieldD60(std::int32_t value) { write_i32(0xD60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d60() const { return FieldD60(); }
    void field_d60(std::int32_t value) { FieldD60(value); }
    [[nodiscard]] std::int32_t FieldD64() const { return read_i32(0xD64); }
    void FieldD64(std::int32_t value) { write_i32(0xD64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d64() const { return FieldD64(); }
    void field_d64(std::int32_t value) { FieldD64(value); }
    [[nodiscard]] std::int32_t FieldD68() const { return read_i32(0xD68); }
    void FieldD68(std::int32_t value) { write_i32(0xD68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d68() const { return FieldD68(); }
    void field_d68(std::int32_t value) { FieldD68(value); }
    [[nodiscard]] std::int32_t FieldD6C() const { return read_i32(0xD6C); }
    void FieldD6C(std::int32_t value) { write_i32(0xD6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d6_c() const { return FieldD6C(); }
    void field_d6_c(std::int32_t value) { FieldD6C(value); }
    [[nodiscard]] std::int32_t FieldD70() const { return read_i32(0xD70); }
    void FieldD70(std::int32_t value) { write_i32(0xD70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d70() const { return FieldD70(); }
    void field_d70(std::int32_t value) { FieldD70(value); }
    [[nodiscard]] std::int32_t FieldD74() const { return read_i32(0xD74); }
    void FieldD74(std::int32_t value) { write_i32(0xD74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d74() const { return FieldD74(); }
    void field_d74(std::int32_t value) { FieldD74(value); }
    [[nodiscard]] std::int32_t FieldD78() const { return read_i32(0xD78); }
    void FieldD78(std::int32_t value) { write_i32(0xD78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d78() const { return FieldD78(); }
    void field_d78(std::int32_t value) { FieldD78(value); }
    [[nodiscard]] std::int32_t FieldD7C() const { return read_i32(0xD7C); }
    void FieldD7C(std::int32_t value) { write_i32(0xD7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d7_c() const { return FieldD7C(); }
    void field_d7_c(std::int32_t value) { FieldD7C(value); }
    [[nodiscard]] std::int32_t FieldD80() const { return read_i32(0xD80); }
    void FieldD80(std::int32_t value) { write_i32(0xD80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d80() const { return FieldD80(); }
    void field_d80(std::int32_t value) { FieldD80(value); }
    [[nodiscard]] std::int32_t FieldD84() const { return read_i32(0xD84); }
    void FieldD84(std::int32_t value) { write_i32(0xD84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d84() const { return FieldD84(); }
    void field_d84(std::int32_t value) { FieldD84(value); }
    [[nodiscard]] std::int32_t FieldD88() const { return read_i32(0xD88); }
    void FieldD88(std::int32_t value) { write_i32(0xD88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d88() const { return FieldD88(); }
    void field_d88(std::int32_t value) { FieldD88(value); }
    [[nodiscard]] std::int32_t FieldD8C() const { return read_i32(0xD8C); }
    void FieldD8C(std::int32_t value) { write_i32(0xD8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d8_c() const { return FieldD8C(); }
    void field_d8_c(std::int32_t value) { FieldD8C(value); }
    [[nodiscard]] std::int32_t FieldD90() const { return read_i32(0xD90); }
    void FieldD90(std::int32_t value) { write_i32(0xD90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d90() const { return FieldD90(); }
    void field_d90(std::int32_t value) { FieldD90(value); }
    [[nodiscard]] std::int32_t FieldD94() const { return read_i32(0xD94); }
    void FieldD94(std::int32_t value) { write_i32(0xD94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d94() const { return FieldD94(); }
    void field_d94(std::int32_t value) { FieldD94(value); }
    [[nodiscard]] std::int32_t FieldD98() const { return read_i32(0xD98); }
    void FieldD98(std::int32_t value) { write_i32(0xD98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d98() const { return FieldD98(); }
    void field_d98(std::int32_t value) { FieldD98(value); }
    [[nodiscard]] std::int32_t FieldD9C() const { return read_i32(0xD9C); }
    void FieldD9C(std::int32_t value) { write_i32(0xD9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d9_c() const { return FieldD9C(); }
    void field_d9_c(std::int32_t value) { FieldD9C(value); }
    [[nodiscard]] std::int32_t FieldDA0() const { return read_i32(0xDA0); }
    void FieldDA0(std::int32_t value) { write_i32(0xDA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_a0() const { return FieldDA0(); }
    void field_d_a0(std::int32_t value) { FieldDA0(value); }
    [[nodiscard]] std::int32_t FieldDA4() const { return read_i32(0xDA4); }
    void FieldDA4(std::int32_t value) { write_i32(0xDA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_a4() const { return FieldDA4(); }
    void field_d_a4(std::int32_t value) { FieldDA4(value); }
    [[nodiscard]] std::int32_t FieldDA8() const { return read_i32(0xDA8); }
    void FieldDA8(std::int32_t value) { write_i32(0xDA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_a8() const { return FieldDA8(); }
    void field_d_a8(std::int32_t value) { FieldDA8(value); }
    [[nodiscard]] std::int32_t FieldDAC() const { return read_i32(0xDAC); }
    void FieldDAC(std::int32_t value) { write_i32(0xDAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_a_c() const { return FieldDAC(); }
    void field_d_a_c(std::int32_t value) { FieldDAC(value); }
    [[nodiscard]] std::int32_t FieldDB0() const { return read_i32(0xDB0); }
    void FieldDB0(std::int32_t value) { write_i32(0xDB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_b0() const { return FieldDB0(); }
    void field_d_b0(std::int32_t value) { FieldDB0(value); }
    [[nodiscard]] std::int32_t FieldDB4() const { return read_i32(0xDB4); }
    void FieldDB4(std::int32_t value) { write_i32(0xDB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_b4() const { return FieldDB4(); }
    void field_d_b4(std::int32_t value) { FieldDB4(value); }
    [[nodiscard]] std::int32_t FieldDB8() const { return read_i32(0xDB8); }
    void FieldDB8(std::int32_t value) { write_i32(0xDB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_b8() const { return FieldDB8(); }
    void field_d_b8(std::int32_t value) { FieldDB8(value); }
    [[nodiscard]] std::int32_t FieldDBC() const { return read_i32(0xDBC); }
    void FieldDBC(std::int32_t value) { write_i32(0xDBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_b_c() const { return FieldDBC(); }
    void field_d_b_c(std::int32_t value) { FieldDBC(value); }
    [[nodiscard]] std::int32_t FieldDC0() const { return read_i32(0xDC0); }
    void FieldDC0(std::int32_t value) { write_i32(0xDC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c0() const { return FieldDC0(); }
    void field_d_c0(std::int32_t value) { FieldDC0(value); }
    [[nodiscard]] std::int32_t FieldDC4() const { return read_i32(0xDC4); }
    void FieldDC4(std::int32_t value) { write_i32(0xDC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c4() const { return FieldDC4(); }
    void field_d_c4(std::int32_t value) { FieldDC4(value); }
    [[nodiscard]] std::int32_t FieldDC8() const { return read_i32(0xDC8); }
    void FieldDC8(std::int32_t value) { write_i32(0xDC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c8() const { return FieldDC8(); }
    void field_d_c8(std::int32_t value) { FieldDC8(value); }
    [[nodiscard]] std::int32_t FieldDCC() const { return read_i32(0xDCC); }
    void FieldDCC(std::int32_t value) { write_i32(0xDCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c_c() const { return FieldDCC(); }
    void field_d_c_c(std::int32_t value) { FieldDCC(value); }
    [[nodiscard]] std::int32_t FieldDD0() const { return read_i32(0xDD0); }
    void FieldDD0(std::int32_t value) { write_i32(0xDD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_d0() const { return FieldDD0(); }
    void field_d_d0(std::int32_t value) { FieldDD0(value); }
    [[nodiscard]] std::int32_t FieldDD4() const { return read_i32(0xDD4); }
    void FieldDD4(std::int32_t value) { write_i32(0xDD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_d4() const { return FieldDD4(); }
    void field_d_d4(std::int32_t value) { FieldDD4(value); }
    [[nodiscard]] std::int32_t FieldDD8() const { return read_i32(0xDD8); }
    void FieldDD8(std::int32_t value) { write_i32(0xDD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_d8() const { return FieldDD8(); }
    void field_d_d8(std::int32_t value) { FieldDD8(value); }
    [[nodiscard]] std::int32_t FieldDDC() const { return read_i32(0xDDC); }
    void FieldDDC(std::int32_t value) { write_i32(0xDDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_d_c() const { return FieldDDC(); }
    void field_d_d_c(std::int32_t value) { FieldDDC(value); }
    [[nodiscard]] std::int32_t FieldDE0() const { return read_i32(0xDE0); }
    void FieldDE0(std::int32_t value) { write_i32(0xDE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_e0() const { return FieldDE0(); }
    void field_d_e0(std::int32_t value) { FieldDE0(value); }
    [[nodiscard]] std::int32_t FieldDE4() const { return read_i32(0xDE4); }
    void FieldDE4(std::int32_t value) { write_i32(0xDE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_e4() const { return FieldDE4(); }
    void field_d_e4(std::int32_t value) { FieldDE4(value); }
    [[nodiscard]] std::int32_t FieldDE8() const { return read_i32(0xDE8); }
    void FieldDE8(std::int32_t value) { write_i32(0xDE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_e8() const { return FieldDE8(); }
    void field_d_e8(std::int32_t value) { FieldDE8(value); }
    [[nodiscard]] std::int32_t FieldDEC() const { return read_i32(0xDEC); }
    void FieldDEC(std::int32_t value) { write_i32(0xDEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_e_c() const { return FieldDEC(); }
    void field_d_e_c(std::int32_t value) { FieldDEC(value); }
    [[nodiscard]] std::int32_t FieldDF0() const { return read_i32(0xDF0); }
    void FieldDF0(std::int32_t value) { write_i32(0xDF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_f0() const { return FieldDF0(); }
    void field_d_f0(std::int32_t value) { FieldDF0(value); }
    [[nodiscard]] std::int32_t FieldDF4() const { return read_i32(0xDF4); }
    void FieldDF4(std::int32_t value) { write_i32(0xDF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_f4() const { return FieldDF4(); }
    void field_d_f4(std::int32_t value) { FieldDF4(value); }
    [[nodiscard]] std::int32_t FieldDF8() const { return read_i32(0xDF8); }
    void FieldDF8(std::int32_t value) { write_i32(0xDF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_f8() const { return FieldDF8(); }
    void field_d_f8(std::int32_t value) { FieldDF8(value); }
    [[nodiscard]] std::int32_t FieldDFC() const { return read_i32(0xDFC); }
    void FieldDFC(std::int32_t value) { write_i32(0xDFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_f_c() const { return FieldDFC(); }
    void field_d_f_c(std::int32_t value) { FieldDFC(value); }
    [[nodiscard]] std::int32_t FieldE00() const { return read_i32(0xE00); }
    void FieldE00(std::int32_t value) { write_i32(0xE00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e00() const { return FieldE00(); }
    void field_e00(std::int32_t value) { FieldE00(value); }
    [[nodiscard]] std::int32_t FieldE04() const { return read_i32(0xE04); }
    void FieldE04(std::int32_t value) { write_i32(0xE04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e04() const { return FieldE04(); }
    void field_e04(std::int32_t value) { FieldE04(value); }
    [[nodiscard]] std::int32_t FieldE08() const { return read_i32(0xE08); }
    void FieldE08(std::int32_t value) { write_i32(0xE08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e08() const { return FieldE08(); }
    void field_e08(std::int32_t value) { FieldE08(value); }
    [[nodiscard]] std::int32_t FieldE0C() const { return read_i32(0xE0C); }
    void FieldE0C(std::int32_t value) { write_i32(0xE0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e0_c() const { return FieldE0C(); }
    void field_e0_c(std::int32_t value) { FieldE0C(value); }
    [[nodiscard]] std::int32_t FieldE10() const { return read_i32(0xE10); }
    void FieldE10(std::int32_t value) { write_i32(0xE10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e10() const { return FieldE10(); }
    void field_e10(std::int32_t value) { FieldE10(value); }
    [[nodiscard]] std::int32_t FieldE14() const { return read_i32(0xE14); }
    void FieldE14(std::int32_t value) { write_i32(0xE14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e14() const { return FieldE14(); }
    void field_e14(std::int32_t value) { FieldE14(value); }
    [[nodiscard]] std::int32_t FieldE18() const { return read_i32(0xE18); }
    void FieldE18(std::int32_t value) { write_i32(0xE18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e18() const { return FieldE18(); }
    void field_e18(std::int32_t value) { FieldE18(value); }
    [[nodiscard]] std::int32_t FieldE1C() const { return read_i32(0xE1C); }
    void FieldE1C(std::int32_t value) { write_i32(0xE1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e1_c() const { return FieldE1C(); }
    void field_e1_c(std::int32_t value) { FieldE1C(value); }
    [[nodiscard]] std::int32_t FieldE20() const { return read_i32(0xE20); }
    void FieldE20(std::int32_t value) { write_i32(0xE20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e20() const { return FieldE20(); }
    void field_e20(std::int32_t value) { FieldE20(value); }
    [[nodiscard]] std::int32_t FieldE24() const { return read_i32(0xE24); }
    void FieldE24(std::int32_t value) { write_i32(0xE24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e24() const { return FieldE24(); }
    void field_e24(std::int32_t value) { FieldE24(value); }
    [[nodiscard]] std::int32_t FieldE28() const { return read_i32(0xE28); }
    void FieldE28(std::int32_t value) { write_i32(0xE28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e28() const { return FieldE28(); }
    void field_e28(std::int32_t value) { FieldE28(value); }
    [[nodiscard]] std::int32_t FieldE2C() const { return read_i32(0xE2C); }
    void FieldE2C(std::int32_t value) { write_i32(0xE2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e2_c() const { return FieldE2C(); }
    void field_e2_c(std::int32_t value) { FieldE2C(value); }
    [[nodiscard]] std::int32_t FieldE30() const { return read_i32(0xE30); }
    void FieldE30(std::int32_t value) { write_i32(0xE30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e30() const { return FieldE30(); }
    void field_e30(std::int32_t value) { FieldE30(value); }
    [[nodiscard]] std::int32_t FieldE34() const { return read_i32(0xE34); }
    void FieldE34(std::int32_t value) { write_i32(0xE34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e34() const { return FieldE34(); }
    void field_e34(std::int32_t value) { FieldE34(value); }
    [[nodiscard]] std::int32_t FieldE38() const { return read_i32(0xE38); }
    void FieldE38(std::int32_t value) { write_i32(0xE38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e38() const { return FieldE38(); }
    void field_e38(std::int32_t value) { FieldE38(value); }
    [[nodiscard]] std::int32_t FieldE3C() const { return read_i32(0xE3C); }
    void FieldE3C(std::int32_t value) { write_i32(0xE3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e3_c() const { return FieldE3C(); }
    void field_e3_c(std::int32_t value) { FieldE3C(value); }
    [[nodiscard]] std::int32_t FieldE40() const { return read_i32(0xE40); }
    void FieldE40(std::int32_t value) { write_i32(0xE40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e40() const { return FieldE40(); }
    void field_e40(std::int32_t value) { FieldE40(value); }
    [[nodiscard]] std::int32_t FieldE44() const { return read_i32(0xE44); }
    void FieldE44(std::int32_t value) { write_i32(0xE44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e44() const { return FieldE44(); }
    void field_e44(std::int32_t value) { FieldE44(value); }
    [[nodiscard]] std::int32_t FieldE48() const { return read_i32(0xE48); }
    void FieldE48(std::int32_t value) { write_i32(0xE48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e48() const { return FieldE48(); }
    void field_e48(std::int32_t value) { FieldE48(value); }
    [[nodiscard]] std::int32_t FieldE4C() const { return read_i32(0xE4C); }
    void FieldE4C(std::int32_t value) { write_i32(0xE4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e4_c() const { return FieldE4C(); }
    void field_e4_c(std::int32_t value) { FieldE4C(value); }
    [[nodiscard]] std::int32_t FieldE50() const { return read_i32(0xE50); }
    void FieldE50(std::int32_t value) { write_i32(0xE50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e50() const { return FieldE50(); }
    void field_e50(std::int32_t value) { FieldE50(value); }
    [[nodiscard]] std::int32_t FieldE54() const { return read_i32(0xE54); }
    void FieldE54(std::int32_t value) { write_i32(0xE54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e54() const { return FieldE54(); }
    void field_e54(std::int32_t value) { FieldE54(value); }
    [[nodiscard]] std::int32_t FieldE58() const { return read_i32(0xE58); }
    void FieldE58(std::int32_t value) { write_i32(0xE58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e58() const { return FieldE58(); }
    void field_e58(std::int32_t value) { FieldE58(value); }
    [[nodiscard]] std::int32_t FieldE5C() const { return read_i32(0xE5C); }
    void FieldE5C(std::int32_t value) { write_i32(0xE5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e5_c() const { return FieldE5C(); }
    void field_e5_c(std::int32_t value) { FieldE5C(value); }
    [[nodiscard]] std::int32_t FieldE60() const { return read_i32(0xE60); }
    void FieldE60(std::int32_t value) { write_i32(0xE60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e60() const { return FieldE60(); }
    void field_e60(std::int32_t value) { FieldE60(value); }
    [[nodiscard]] std::int32_t FieldE64() const { return read_i32(0xE64); }
    void FieldE64(std::int32_t value) { write_i32(0xE64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e64() const { return FieldE64(); }
    void field_e64(std::int32_t value) { FieldE64(value); }
    [[nodiscard]] std::int32_t FieldE68() const { return read_i32(0xE68); }
    void FieldE68(std::int32_t value) { write_i32(0xE68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e68() const { return FieldE68(); }
    void field_e68(std::int32_t value) { FieldE68(value); }
    [[nodiscard]] std::int32_t FieldE6C() const { return read_i32(0xE6C); }
    void FieldE6C(std::int32_t value) { write_i32(0xE6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e6_c() const { return FieldE6C(); }
    void field_e6_c(std::int32_t value) { FieldE6C(value); }
    [[nodiscard]] std::int32_t FieldE70() const { return read_i32(0xE70); }
    void FieldE70(std::int32_t value) { write_i32(0xE70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e70() const { return FieldE70(); }
    void field_e70(std::int32_t value) { FieldE70(value); }
    [[nodiscard]] std::int32_t FieldE74() const { return read_i32(0xE74); }
    void FieldE74(std::int32_t value) { write_i32(0xE74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e74() const { return FieldE74(); }
    void field_e74(std::int32_t value) { FieldE74(value); }
    [[nodiscard]] std::int32_t FieldE78() const { return read_i32(0xE78); }
    void FieldE78(std::int32_t value) { write_i32(0xE78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e78() const { return FieldE78(); }
    void field_e78(std::int32_t value) { FieldE78(value); }
    [[nodiscard]] std::int32_t FieldE7C() const { return read_i32(0xE7C); }
    void FieldE7C(std::int32_t value) { write_i32(0xE7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e7_c() const { return FieldE7C(); }
    void field_e7_c(std::int32_t value) { FieldE7C(value); }
    [[nodiscard]] std::int32_t FieldE80() const { return read_i32(0xE80); }
    void FieldE80(std::int32_t value) { write_i32(0xE80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e80() const { return FieldE80(); }
    void field_e80(std::int32_t value) { FieldE80(value); }
    [[nodiscard]] std::int32_t FieldE84() const { return read_i32(0xE84); }
    void FieldE84(std::int32_t value) { write_i32(0xE84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e84() const { return FieldE84(); }
    void field_e84(std::int32_t value) { FieldE84(value); }
    [[nodiscard]] std::int32_t FieldE88() const { return read_i32(0xE88); }
    void FieldE88(std::int32_t value) { write_i32(0xE88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e88() const { return FieldE88(); }
    void field_e88(std::int32_t value) { FieldE88(value); }
    [[nodiscard]] std::int32_t FieldE8C() const { return read_i32(0xE8C); }
    void FieldE8C(std::int32_t value) { write_i32(0xE8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e8_c() const { return FieldE8C(); }
    void field_e8_c(std::int32_t value) { FieldE8C(value); }
    [[nodiscard]] std::int32_t FieldE90() const { return read_i32(0xE90); }
    void FieldE90(std::int32_t value) { write_i32(0xE90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e90() const { return FieldE90(); }
    void field_e90(std::int32_t value) { FieldE90(value); }
    [[nodiscard]] std::int32_t FieldE94() const { return read_i32(0xE94); }
    void FieldE94(std::int32_t value) { write_i32(0xE94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e94() const { return FieldE94(); }
    void field_e94(std::int32_t value) { FieldE94(value); }
    [[nodiscard]] std::int32_t FieldE98() const { return read_i32(0xE98); }
    void FieldE98(std::int32_t value) { write_i32(0xE98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e98() const { return FieldE98(); }
    void field_e98(std::int32_t value) { FieldE98(value); }
    [[nodiscard]] std::int32_t FieldE9C() const { return read_i32(0xE9C); }
    void FieldE9C(std::int32_t value) { write_i32(0xE9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e9_c() const { return FieldE9C(); }
    void field_e9_c(std::int32_t value) { FieldE9C(value); }
    [[nodiscard]] std::int32_t FieldEA0() const { return read_i32(0xEA0); }
    void FieldEA0(std::int32_t value) { write_i32(0xEA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_a0() const { return FieldEA0(); }
    void field_e_a0(std::int32_t value) { FieldEA0(value); }
    [[nodiscard]] std::int32_t FieldEA4() const { return read_i32(0xEA4); }
    void FieldEA4(std::int32_t value) { write_i32(0xEA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_a4() const { return FieldEA4(); }
    void field_e_a4(std::int32_t value) { FieldEA4(value); }
    [[nodiscard]] std::int32_t FieldEA8() const { return read_i32(0xEA8); }
    void FieldEA8(std::int32_t value) { write_i32(0xEA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_a8() const { return FieldEA8(); }
    void field_e_a8(std::int32_t value) { FieldEA8(value); }
    [[nodiscard]] std::int32_t FieldEAC() const { return read_i32(0xEAC); }
    void FieldEAC(std::int32_t value) { write_i32(0xEAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_a_c() const { return FieldEAC(); }
    void field_e_a_c(std::int32_t value) { FieldEAC(value); }
    [[nodiscard]] std::int32_t FieldEB0() const { return read_i32(0xEB0); }
    void FieldEB0(std::int32_t value) { write_i32(0xEB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_b0() const { return FieldEB0(); }
    void field_e_b0(std::int32_t value) { FieldEB0(value); }
    [[nodiscard]] std::int32_t FieldEB4() const { return read_i32(0xEB4); }
    void FieldEB4(std::int32_t value) { write_i32(0xEB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_b4() const { return FieldEB4(); }
    void field_e_b4(std::int32_t value) { FieldEB4(value); }
    [[nodiscard]] std::int32_t FieldEB8() const { return read_i32(0xEB8); }
    void FieldEB8(std::int32_t value) { write_i32(0xEB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_b8() const { return FieldEB8(); }
    void field_e_b8(std::int32_t value) { FieldEB8(value); }
    [[nodiscard]] std::int32_t FieldEBC() const { return read_i32(0xEBC); }
    void FieldEBC(std::int32_t value) { write_i32(0xEBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_b_c() const { return FieldEBC(); }
    void field_e_b_c(std::int32_t value) { FieldEBC(value); }
    [[nodiscard]] std::int32_t FieldEC0() const { return read_i32(0xEC0); }
    void FieldEC0(std::int32_t value) { write_i32(0xEC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c0() const { return FieldEC0(); }
    void field_e_c0(std::int32_t value) { FieldEC0(value); }
    [[nodiscard]] std::int32_t FieldEC4() const { return read_i32(0xEC4); }
    void FieldEC4(std::int32_t value) { write_i32(0xEC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c4() const { return FieldEC4(); }
    void field_e_c4(std::int32_t value) { FieldEC4(value); }
    [[nodiscard]] std::int32_t FieldEC8() const { return read_i32(0xEC8); }
    void FieldEC8(std::int32_t value) { write_i32(0xEC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c8() const { return FieldEC8(); }
    void field_e_c8(std::int32_t value) { FieldEC8(value); }
    [[nodiscard]] std::int32_t FieldECC() const { return read_i32(0xECC); }
    void FieldECC(std::int32_t value) { write_i32(0xECC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c_c() const { return FieldECC(); }
    void field_e_c_c(std::int32_t value) { FieldECC(value); }
    [[nodiscard]] std::int32_t FieldED0() const { return read_i32(0xED0); }
    void FieldED0(std::int32_t value) { write_i32(0xED0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_d0() const { return FieldED0(); }
    void field_e_d0(std::int32_t value) { FieldED0(value); }
    [[nodiscard]] std::int32_t FieldED4() const { return read_i32(0xED4); }
    void FieldED4(std::int32_t value) { write_i32(0xED4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_d4() const { return FieldED4(); }
    void field_e_d4(std::int32_t value) { FieldED4(value); }
    [[nodiscard]] std::int32_t FieldED8() const { return read_i32(0xED8); }
    void FieldED8(std::int32_t value) { write_i32(0xED8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_d8() const { return FieldED8(); }
    void field_e_d8(std::int32_t value) { FieldED8(value); }
    [[nodiscard]] std::int32_t FieldEDC() const { return read_i32(0xEDC); }
    void FieldEDC(std::int32_t value) { write_i32(0xEDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_d_c() const { return FieldEDC(); }
    void field_e_d_c(std::int32_t value) { FieldEDC(value); }
    [[nodiscard]] std::int32_t FieldEE0() const { return read_i32(0xEE0); }
    void FieldEE0(std::int32_t value) { write_i32(0xEE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_e0() const { return FieldEE0(); }
    void field_e_e0(std::int32_t value) { FieldEE0(value); }
    [[nodiscard]] std::int32_t FieldEE4() const { return read_i32(0xEE4); }
    void FieldEE4(std::int32_t value) { write_i32(0xEE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_e4() const { return FieldEE4(); }
    void field_e_e4(std::int32_t value) { FieldEE4(value); }
    [[nodiscard]] std::int32_t FieldEE8() const { return read_i32(0xEE8); }
    void FieldEE8(std::int32_t value) { write_i32(0xEE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_e8() const { return FieldEE8(); }
    void field_e_e8(std::int32_t value) { FieldEE8(value); }
    [[nodiscard]] std::int32_t FieldEEC() const { return read_i32(0xEEC); }
    void FieldEEC(std::int32_t value) { write_i32(0xEEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_e_c() const { return FieldEEC(); }
    void field_e_e_c(std::int32_t value) { FieldEEC(value); }
    [[nodiscard]] std::int32_t FieldEF0() const { return read_i32(0xEF0); }
    void FieldEF0(std::int32_t value) { write_i32(0xEF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_f0() const { return FieldEF0(); }
    void field_e_f0(std::int32_t value) { FieldEF0(value); }
    [[nodiscard]] std::int32_t FieldEF4() const { return read_i32(0xEF4); }
    void FieldEF4(std::int32_t value) { write_i32(0xEF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_f4() const { return FieldEF4(); }
    void field_e_f4(std::int32_t value) { FieldEF4(value); }
    [[nodiscard]] std::int32_t FieldEF8() const { return read_i32(0xEF8); }
    void FieldEF8(std::int32_t value) { write_i32(0xEF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_f8() const { return FieldEF8(); }
    void field_e_f8(std::int32_t value) { FieldEF8(value); }
    [[nodiscard]] std::int32_t FieldEFC() const { return read_i32(0xEFC); }
    void FieldEFC(std::int32_t value) { write_i32(0xEFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_f_c() const { return FieldEFC(); }
    void field_e_f_c(std::int32_t value) { FieldEFC(value); }
    [[nodiscard]] std::int32_t FieldF00() const { return read_i32(0xF00); }
    void FieldF00(std::int32_t value) { write_i32(0xF00, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f00() const { return FieldF00(); }
    void field_f00(std::int32_t value) { FieldF00(value); }
    [[nodiscard]] std::int32_t FieldF04() const { return read_i32(0xF04); }
    void FieldF04(std::int32_t value) { write_i32(0xF04, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f04() const { return FieldF04(); }
    void field_f04(std::int32_t value) { FieldF04(value); }
    [[nodiscard]] std::int32_t FieldF08() const { return read_i32(0xF08); }
    void FieldF08(std::int32_t value) { write_i32(0xF08, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f08() const { return FieldF08(); }
    void field_f08(std::int32_t value) { FieldF08(value); }
    [[nodiscard]] std::int32_t FieldF0C() const { return read_i32(0xF0C); }
    void FieldF0C(std::int32_t value) { write_i32(0xF0C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0_c() const { return FieldF0C(); }
    void field_f0_c(std::int32_t value) { FieldF0C(value); }
    [[nodiscard]] std::int32_t FieldF10() const { return read_i32(0xF10); }
    void FieldF10(std::int32_t value) { write_i32(0xF10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f10() const { return FieldF10(); }
    void field_f10(std::int32_t value) { FieldF10(value); }
    [[nodiscard]] std::int32_t FieldF14() const { return read_i32(0xF14); }
    void FieldF14(std::int32_t value) { write_i32(0xF14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f14() const { return FieldF14(); }
    void field_f14(std::int32_t value) { FieldF14(value); }
    [[nodiscard]] std::int32_t FieldF18() const { return read_i32(0xF18); }
    void FieldF18(std::int32_t value) { write_i32(0xF18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f18() const { return FieldF18(); }
    void field_f18(std::int32_t value) { FieldF18(value); }
    [[nodiscard]] std::uint32_t AiDataPtr() const { return read_pointer(0xF1C); }
    void AiDataPtr(std::uint32_t value) { write_pointer(0xF1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ai_data_ptr() const { return AiDataPtr(); }
    void ai_data_ptr(std::uint32_t value) { AiDataPtr(value); }
    [[nodiscard]] ::fruityprime::memory::AiData* AiData() noexcept;
    [[nodiscard]] const ::fruityprime::memory::AiData* AiData() const noexcept;
    [[nodiscard]] ::fruityprime::memory::AiData* ai_data() noexcept { return AiData(); }
    [[nodiscard]] const ::fruityprime::memory::AiData* ai_data() const noexcept { return AiData(); }
    [[nodiscard]] std::int32_t FieldF20() const { return read_i32(0xF20); }
    void FieldF20(std::int32_t value) { write_i32(0xF20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f20() const { return FieldF20(); }
    void field_f20(std::int32_t value) { FieldF20(value); }
    [[nodiscard]] std::uint32_t Halfturret() const { return read_pointer(0xF24); }
    void Halfturret(std::uint32_t value) { write_pointer(0xF24, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t halfturret() const { return Halfturret(); }
    void halfturret(std::uint32_t value) { Halfturret(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }
    [[nodiscard]] std::int32_t WeaponSfxHandle() const { return read_i32(0xF2C); }
    void WeaponSfxHandle(std::int32_t value) { write_i32(0xF2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t weapon_sfx_handle() const { return WeaponSfxHandle(); }
    void weapon_sfx_handle(std::int32_t value) { WeaponSfxHandle(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::AIContext>* AIContext() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AIContext>* AIContext() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AIContext>* a_i_context() noexcept { return AIContext(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AIContext>* a_i_context() const noexcept { return AIContext(); }
    [[nodiscard]] std::uint32_t AggroCount() const;
    void AggroCount(std::uint32_t value);
    [[nodiscard]] std::uint32_t aggro_count() const { return AggroCount(); }
    void aggro_count(std::uint32_t value) { AggroCount(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::AIAggro>* AIAggro() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AIAggro>* AIAggro() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AIAggro>* a_i_aggro() noexcept { return AIAggro(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AIAggro>* a_i_aggro() const noexcept { return AIAggro(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x1C;
    static constexpr std::size_t _off2 = 0x28;
    static constexpr std::size_t _off3 = 0x34;
    static constexpr std::size_t _off4 = 0x40;
    static constexpr std::size_t _off5 = 0x4C;
    static constexpr std::size_t _off6 = 0x58;
    static constexpr std::size_t _off7 = 0x64;
    static constexpr std::size_t _off8 = 0x70;
    static constexpr std::size_t _off9 = 0x7C;
    static constexpr std::size_t _off10 = 0x80;
    static constexpr std::size_t _off11 = 0x84;
    static constexpr std::size_t _off12 = 0x88;
    static constexpr std::size_t _off13 = 0x8C;
    static constexpr std::size_t _off14 = 0x90;
    static constexpr std::size_t _off15 = 0x94;
    static constexpr std::size_t _off16 = 0x98;
    static constexpr std::size_t _off17 = 0x9C;
    static constexpr std::size_t _off18 = 0xA8;
    static constexpr std::size_t _off19 = 0xB4;
    static constexpr std::size_t _off20 = 0xC0;
    static constexpr std::size_t _off21 = 0xCC;
    static constexpr std::size_t _off22 = 0xD8;
    static constexpr std::size_t _off23 = 0xDA;
    static constexpr std::size_t _off24 = 0xDC;
    static constexpr std::size_t _off25 = 0xDE;
    static constexpr std::size_t _off26 = 0xE0;
    static constexpr std::size_t _off27 = 0xE1;
    static constexpr std::size_t _off28 = 0xE2;
    static constexpr std::size_t _off29 = 0xE3;
    static constexpr std::size_t _off30 = 0xE4;
    static constexpr std::size_t _off31 = 0xE6;
    static constexpr std::size_t _off32 = 0xE8;
    static constexpr std::size_t _off33 = 0xEC;
    static constexpr std::size_t _off34 = 0xF8;
    static constexpr std::size_t _off35 = 0xFA;
    static constexpr std::size_t _off36 = 0xFB;
    static constexpr std::size_t _off37 = 0xFC;
    static constexpr std::size_t _off38 = 0x100;
    static constexpr std::size_t _off39 = 0x104;
    static constexpr std::size_t _off40 = 0x105;
    static constexpr std::size_t _off41 = 0x106;
    static constexpr std::size_t _off42 = 0x108;
    static constexpr std::size_t _off43 = 0x148;
    static constexpr std::size_t _off44 = 0x149;
    static constexpr std::size_t _off45 = 0x14A;
    static constexpr std::size_t _off46 = 0x14B;
    static constexpr std::size_t _off47 = 0x14C;
    static constexpr std::size_t _off48 = 0x14E;
    static constexpr std::size_t _off49 = 0x150;
    static constexpr std::size_t _off50 = 0x152;
    static constexpr std::size_t _off51 = 0x154;
    static constexpr std::size_t _off52 = 0x155;
    static constexpr std::size_t _off53 = 0x156;
    static constexpr std::size_t _off54 = 0x158;
    static constexpr std::size_t _off55 = 0x159;
    static constexpr std::size_t _off56 = 0x15A;
    static constexpr std::size_t _off57 = 0x15B;
    static constexpr std::size_t _off58 = 0x15C;
    static constexpr std::size_t _off59 = 0x1A4;
    static constexpr std::size_t _off60 = 0x1EC;
    static constexpr std::size_t _off61 = 0x1F8;
    static constexpr std::size_t _off62 = 0x204;
    static constexpr std::size_t _off63 = 0x208;
    static constexpr std::size_t _off64 = 0x20C;
    static constexpr std::size_t _off65 = 0x210;
    static constexpr std::size_t _off66 = 0x214;
    static constexpr std::size_t _off67 = 0x218;
    static constexpr std::size_t _off68 = 0x21C;
    static constexpr std::size_t _off69 = 0x224;
    static constexpr std::size_t _off70 = 0x22C;
    static constexpr std::size_t _off71 = 0x274;
    static constexpr std::size_t _off72 = 0x2BC;
    static constexpr std::size_t _off73 = 0x2BE;
    static constexpr std::size_t _off74 = 0x2C0;
    static constexpr std::size_t _off75 = 0x308;
    static constexpr std::size_t _off76 = 0x350;
    static constexpr std::size_t _off77 = 0x351;
    static constexpr std::size_t _off78 = 0x352;
    static constexpr std::size_t _off79 = 0x354;
    static constexpr std::size_t _off80 = 0x358;
    static constexpr std::size_t _off81 = 0x35C;
    static constexpr std::size_t _off82 = 0x360;
    static constexpr std::size_t _off83 = 0x361;
    static constexpr std::size_t _off84 = 0x362;
    static constexpr std::size_t _off85 = 0x364;
    static constexpr std::size_t _off86 = 0x400;
    static constexpr std::size_t _off87 = 0x401;
    static constexpr std::size_t _off88 = 0x402;
    static constexpr std::size_t _off89 = 0x404;
    static constexpr std::size_t _off90 = 0x408;
    static constexpr std::size_t _off91 = 0x40C;
    static constexpr std::size_t _off92 = 0x410;
    static constexpr std::size_t _off93 = 0x414;
    static constexpr std::size_t _off94 = 0x418;
    static constexpr std::size_t _off95 = 0x41C;
    static constexpr std::size_t _off96 = 0x420;
    static constexpr std::size_t _off97 = 0x424;
    static constexpr std::size_t _off98 = 0x428;
    static constexpr std::size_t _off99 = 0x42C;
    static constexpr std::size_t _off100 = 0x430;
    static constexpr std::size_t _off101 = 0x434;
    static constexpr std::size_t _off102 = 0x435;
    static constexpr std::size_t _off103 = 0x436;
    static constexpr std::size_t _off104 = 0x438;
    static constexpr std::size_t _off105 = 0x43A;
    static constexpr std::size_t _off106 = 0x43C;
    static constexpr std::size_t _off107 = 0x43E;
    static constexpr std::size_t _off108 = 0x440;
    static constexpr std::size_t _off109 = 0x444;
    static constexpr std::size_t _off110 = 0x445;
    static constexpr std::size_t _off111 = 0x446;
    static constexpr std::size_t _off112 = 0x447;
    static constexpr std::size_t _off113 = 0x448;
    static constexpr std::size_t _off114 = 0x449;
    static constexpr std::size_t _off115 = 0x44A;
    static constexpr std::size_t _off116 = 0x44C;
    static constexpr std::size_t _off117 = 0x450;
    static constexpr std::size_t _off118 = 0x454;
    static constexpr std::size_t _off119 = 0x458;
    static constexpr std::size_t _off120 = 0x45A;
    static constexpr std::size_t _off121 = 0x45C;
    static constexpr std::size_t _off122 = 0x460;
    static constexpr std::size_t _off123 = 0x462;
    static constexpr std::size_t _off124 = 0x464;
    static constexpr std::size_t _off125 = 0x4AC;
    static constexpr std::size_t _off126 = 0x4AD;
    static constexpr std::size_t _off127 = 0x4AE;
    static constexpr std::size_t _off128 = 0x4AF;
    static constexpr std::size_t _off129 = 0x4B0;
    static constexpr std::size_t _off130 = 0x4B2;
    static constexpr std::size_t _off131 = 0x4B4;
    static constexpr std::size_t _off132 = 0x4B6;
    static constexpr std::size_t _off133 = 0x4B8;
    static constexpr std::size_t _off134 = 0x4BA;
    static constexpr std::size_t _off135 = 0x4BB;
    static constexpr std::size_t _off136 = 0x4BC;
    static constexpr std::size_t _off137 = 0x4BD;
    static constexpr std::size_t _off138 = 0x4BE;
    static constexpr std::size_t _off139 = 0x4BF;
    static constexpr std::size_t _off140 = 0x4C0;
    static constexpr std::size_t _off141 = 0x4C1;
    static constexpr std::size_t _off142 = 0x4C2;
    static constexpr std::size_t _off143 = 0x4C4;
    static constexpr std::size_t _off144 = 0x4C8;
    static constexpr std::size_t _off145 = 0x4CC;
    static constexpr std::size_t _off146 = 0x4CE;
    static constexpr std::size_t _off147 = 0x4CF;
    static constexpr std::size_t _off148 = 0x4D0;
    static constexpr std::size_t _off149 = 0x4D1;
    static constexpr std::size_t _off150 = 0x4D2;
    static constexpr std::size_t _off151 = 0x4D3;
    static constexpr std::size_t _off152 = 0x4D4;
    static constexpr std::size_t _off153 = 0x4D5;
    static constexpr std::size_t _off154 = 0x4D6;
    static constexpr std::size_t _off155 = 0x4D7;
    static constexpr std::size_t _off156 = 0x4D8;
    static constexpr std::size_t _off157 = 0x4D9;
    static constexpr std::size_t _off158 = 0x4DA;
    static constexpr std::size_t _off159 = 0x4DC;
    static constexpr std::size_t _off160 = 0x4E0;
    static constexpr std::size_t _off161 = 0x4E4;
    static constexpr std::size_t _off162 = 0x4E8;
    static constexpr std::size_t _off163 = 0x4F4;
    static constexpr std::size_t _off164 = 0x524;
    static constexpr std::size_t _off165 = 0x528;
    static constexpr std::size_t _off166 = 0x52C;
    static constexpr std::size_t _off167 = 0x530;
    static constexpr std::size_t _off168 = 0x534;
    static constexpr std::size_t _off169 = 0x538;
    static constexpr std::size_t _off170 = 0x53C;
    static constexpr std::size_t _off171 = 0x53D;
    static constexpr std::size_t _off172 = 0x53E;
    static constexpr std::size_t _off173 = 0x53F;
    static constexpr std::size_t _off174 = 0x540;
    static constexpr std::size_t _off175 = 0x541;
    static constexpr std::size_t _off176 = 0x542;
    static constexpr std::size_t _off177 = 0x544;
    static constexpr std::size_t _off178 = 0x550;
    static constexpr std::size_t _off179 = 0x551;
    static constexpr std::size_t _off180 = 0x552;
    static constexpr std::size_t _off181 = 0x553;
    static constexpr std::size_t _off182 = 0x554;
    static constexpr std::size_t _off183 = 0x558;
    static constexpr std::size_t _off184 = 0x55C;
    static constexpr std::size_t _off185 = 0x678;
    static constexpr std::size_t _off186 = 0x684;
    static constexpr std::size_t _off187 = 0x688;
    static constexpr std::size_t _off188 = 0x68C;
    static constexpr std::size_t _off189 = 0x690;
    static constexpr std::size_t _off190 = 0x694;
    static constexpr std::size_t _off191 = 0x6B3;
    static constexpr std::size_t _off192 = 0x6B4;
    static constexpr std::size_t _off193 = 0x6B8;
    static constexpr std::size_t _off194 = 0x6BC;
    static constexpr std::size_t _off195 = 0x6C0;
    static constexpr std::size_t _off196 = 0x6C4;
    static constexpr std::size_t _off197 = 0x6C8;
    static constexpr std::size_t _off198 = 0x6CC;
    static constexpr std::size_t _off199 = 0x6D0;
    static constexpr std::size_t _off200 = 0x6D4;
    static constexpr std::size_t _off201 = 0x6D8;
    static constexpr std::size_t _off202 = 0x6DC;
    static constexpr std::size_t _off203 = 0x6DE;
    static constexpr std::size_t _off204 = 0x6E0;
    static constexpr std::size_t _off205 = 0x6E2;
    static constexpr std::size_t _off206 = 0x6E4;
    static constexpr std::size_t _off207 = 0x6E8;
    static constexpr std::size_t _off208 = 0x6EC;
    static constexpr std::size_t _off209 = 0x6F0;
    static constexpr std::size_t _off210 = 0x6F4;
    static constexpr std::size_t _off211 = 0x6F8;
    static constexpr std::size_t _off212 = 0x6FC;
    static constexpr std::size_t _off213 = 0x700;
    static constexpr std::size_t _off214 = 0x704;
    static constexpr std::size_t _off215 = 0x706;
    static constexpr std::size_t _off216 = 0x708;
    static constexpr std::size_t _off217 = 0x70C;
    static constexpr std::size_t _off218 = 0x710;
    static constexpr std::size_t _off219 = 0x714;
    static constexpr std::size_t _off220 = 0x720;
    static constexpr std::size_t _off221 = 0x72C;
    static constexpr std::size_t _off222 = 0x738;
    static constexpr std::size_t _off223 = 0x73C;
    static constexpr std::size_t _off224 = 0x740;
    static constexpr std::size_t _off225 = 0x744;
    static constexpr std::size_t _off226 = 0x748;
    static constexpr std::size_t _off227 = 0x74C;
    static constexpr std::size_t _off228 = 0x750;
    static constexpr std::size_t _off229 = 0x754;
    static constexpr std::size_t _off230 = 0x758;
    static constexpr std::size_t _off231 = 0x75C;
    static constexpr std::size_t _off232 = 0x760;
    static constexpr std::size_t _off233 = 0x764;
    static constexpr std::size_t _off234 = 0x768;
    static constexpr std::size_t _off235 = 0x76C;
    static constexpr std::size_t _off236 = 0x770;
    static constexpr std::size_t _off237 = 0x774;
    static constexpr std::size_t _off238 = 0x778;
    static constexpr std::size_t _off239 = 0x77C;
    static constexpr std::size_t _off240 = 0x780;
    static constexpr std::size_t _off241 = 0x784;
    static constexpr std::size_t _off242 = 0x788;
    static constexpr std::size_t _off243 = 0x78C;
    static constexpr std::size_t _off244 = 0x790;
    static constexpr std::size_t _off245 = 0x794;
    static constexpr std::size_t _off246 = 0x798;
    static constexpr std::size_t _off247 = 0x79C;
    static constexpr std::size_t _off248 = 0x7A0;
    static constexpr std::size_t _off249 = 0x7A4;
    static constexpr std::size_t _off250 = 0x7A8;
    static constexpr std::size_t _off251 = 0x7AC;
    static constexpr std::size_t _off252 = 0x7B0;
    static constexpr std::size_t _off253 = 0x7B4;
    static constexpr std::size_t _off254 = 0x7B8;
    static constexpr std::size_t _off255 = 0x7BC;
    static constexpr std::size_t _off256 = 0x7C0;
    static constexpr std::size_t _off257 = 0x7C4;
    static constexpr std::size_t _off258 = 0x7C8;
    static constexpr std::size_t _off259 = 0x7CC;
    static constexpr std::size_t _off260 = 0x7D0;
    static constexpr std::size_t _off261 = 0x7D4;
    static constexpr std::size_t _off262 = 0x7E0;
    static constexpr std::size_t _off263 = 0x7EC;
    static constexpr std::size_t _off264 = 0x7F0;
    static constexpr std::size_t _off265 = 0x7F4;
    static constexpr std::size_t _off266 = 0x7F8;
    static constexpr std::size_t _off267 = 0x7FC;
    static constexpr std::size_t _off268 = 0x800;
    static constexpr std::size_t _off269 = 0x804;
    static constexpr std::size_t _off270 = 0x808;
    static constexpr std::size_t _off271 = 0x80C;
    static constexpr std::size_t _off272 = 0x810;
    static constexpr std::size_t _off273 = 0x814;
    static constexpr std::size_t _off274 = 0x818;
    static constexpr std::size_t _off275 = 0x81C;
    static constexpr std::size_t _off276 = 0x820;
    static constexpr std::size_t _off277 = 0x824;
    static constexpr std::size_t _off278 = 0x828;
    static constexpr std::size_t _off279 = 0x834;
    static constexpr std::size_t _off280 = 0x838;
    static constexpr std::size_t _off281 = 0x83C;
    static constexpr std::size_t _off282 = 0x840;
    static constexpr std::size_t _off283 = 0x844;
    static constexpr std::size_t _off284 = 0x848;
    static constexpr std::size_t _off285 = 0x84C;
    static constexpr std::size_t _off286 = 0x84D;
    static constexpr std::size_t _off287 = 0x84E;
    static constexpr std::size_t _off288 = 0x84F;
    static constexpr std::size_t _off289 = 0x850;
    static constexpr std::size_t _off290 = 0x864;
    static constexpr std::size_t _off291 = 0x9BC;
    static constexpr std::size_t _off292 = 0x9C0;
    static constexpr std::size_t _off293 = 0x9C4;
    static constexpr std::size_t _off294 = 0x9C8;
    static constexpr std::size_t _off295 = 0x9CC;
    static constexpr std::size_t _off296 = 0x9D0;
    static constexpr std::size_t _off297 = 0x9D4;
    static constexpr std::size_t _off298 = 0x9D8;
    static constexpr std::size_t _off299 = 0x9DC;
    static constexpr std::size_t _off300 = 0x9E0;
    static constexpr std::size_t _off301 = 0x9E4;
    static constexpr std::size_t _off302 = 0x9E8;
    static constexpr std::size_t _off303 = 0x9EC;
    static constexpr std::size_t _off304 = 0x9F0;
    static constexpr std::size_t _off305 = 0x9F4;
    static constexpr std::size_t _off306 = 0x9F8;
    static constexpr std::size_t _off307 = 0x9FC;
    static constexpr std::size_t _off308 = 0xA00;
    static constexpr std::size_t _off309 = 0xA04;
    static constexpr std::size_t _off310 = 0xA08;
    static constexpr std::size_t _off311 = 0xA0C;
    static constexpr std::size_t _off312 = 0xA10;
    static constexpr std::size_t _off313 = 0xA14;
    static constexpr std::size_t _off314 = 0xA18;
    static constexpr std::size_t _off315 = 0xA1C;
    static constexpr std::size_t _off316 = 0xA20;
    static constexpr std::size_t _off317 = 0xA24;
    static constexpr std::size_t _off318 = 0xA28;
    static constexpr std::size_t _off319 = 0xA2C;
    static constexpr std::size_t _off320 = 0xA30;
    static constexpr std::size_t _off321 = 0xA34;
    static constexpr std::size_t _off322 = 0xA38;
    static constexpr std::size_t _off323 = 0xA3C;
    static constexpr std::size_t _off324 = 0xA40;
    static constexpr std::size_t _off325 = 0xA44;
    static constexpr std::size_t _off326 = 0xA48;
    static constexpr std::size_t _off327 = 0xA4C;
    static constexpr std::size_t _off328 = 0xA50;
    static constexpr std::size_t _off329 = 0xA54;
    static constexpr std::size_t _off330 = 0xA58;
    static constexpr std::size_t _off331 = 0xA5C;
    static constexpr std::size_t _off332 = 0xA60;
    static constexpr std::size_t _off333 = 0xA64;
    static constexpr std::size_t _off334 = 0xA68;
    static constexpr std::size_t _off335 = 0xA6C;
    static constexpr std::size_t _off336 = 0xA70;
    static constexpr std::size_t _off337 = 0xA74;
    static constexpr std::size_t _off338 = 0xA78;
    static constexpr std::size_t _off339 = 0xA7C;
    static constexpr std::size_t _off340 = 0xA80;
    static constexpr std::size_t _off341 = 0xA84;
    static constexpr std::size_t _off342 = 0xA88;
    static constexpr std::size_t _off343 = 0xA8C;
    static constexpr std::size_t _off344 = 0xA90;
    static constexpr std::size_t _off345 = 0xA94;
    static constexpr std::size_t _off346 = 0xA98;
    static constexpr std::size_t _off347 = 0xA9C;
    static constexpr std::size_t _off348 = 0xAA0;
    static constexpr std::size_t _off349 = 0xAA4;
    static constexpr std::size_t _off350 = 0xAA8;
    static constexpr std::size_t _off351 = 0xAAC;
    static constexpr std::size_t _off352 = 0xAB0;
    static constexpr std::size_t _off353 = 0xAB4;
    static constexpr std::size_t _off354 = 0xAB8;
    static constexpr std::size_t _off355 = 0xABC;
    static constexpr std::size_t _off356 = 0xAC0;
    static constexpr std::size_t _off357 = 0xAC4;
    static constexpr std::size_t _off358 = 0xAC8;
    static constexpr std::size_t _off359 = 0xACC;
    static constexpr std::size_t _off360 = 0xAD0;
    static constexpr std::size_t _off361 = 0xAD4;
    static constexpr std::size_t _off362 = 0xAD8;
    static constexpr std::size_t _off363 = 0xADC;
    static constexpr std::size_t _off364 = 0xAE0;
    static constexpr std::size_t _off365 = 0xAE4;
    static constexpr std::size_t _off366 = 0xAE8;
    static constexpr std::size_t _off367 = 0xAEC;
    static constexpr std::size_t _off368 = 0xAF0;
    static constexpr std::size_t _off369 = 0xAF4;
    static constexpr std::size_t _off370 = 0xAF8;
    static constexpr std::size_t _off371 = 0xAFC;
    static constexpr std::size_t _off372 = 0xB00;
    static constexpr std::size_t _off373 = 0xB04;
    static constexpr std::size_t _off374 = 0xB08;
    static constexpr std::size_t _off375 = 0xB0C;
    static constexpr std::size_t _off376 = 0xB10;
    static constexpr std::size_t _off377 = 0xB14;
    static constexpr std::size_t _off378 = 0xB18;
    static constexpr std::size_t _off379 = 0xB1C;
    static constexpr std::size_t _off380 = 0xB20;
    static constexpr std::size_t _off381 = 0xB24;
    static constexpr std::size_t _off382 = 0xB28;
    static constexpr std::size_t _off383 = 0xB2C;
    static constexpr std::size_t _off384 = 0xB30;
    static constexpr std::size_t _off385 = 0xB34;
    static constexpr std::size_t _off386 = 0xB38;
    static constexpr std::size_t _off387 = 0xB3C;
    static constexpr std::size_t _off388 = 0xB40;
    static constexpr std::size_t _off389 = 0xB44;
    static constexpr std::size_t _off390 = 0xB48;
    static constexpr std::size_t _off391 = 0xB4C;
    static constexpr std::size_t _off392 = 0xB50;
    static constexpr std::size_t _off393 = 0xB54;
    static constexpr std::size_t _off394 = 0xB58;
    static constexpr std::size_t _off395 = 0xB5C;
    static constexpr std::size_t _off396 = 0xB60;
    static constexpr std::size_t _off397 = 0xB64;
    static constexpr std::size_t _off398 = 0xB68;
    static constexpr std::size_t _off399 = 0xB6C;
    static constexpr std::size_t _off400 = 0xB70;
    static constexpr std::size_t _off401 = 0xB74;
    static constexpr std::size_t _off402 = 0xB78;
    static constexpr std::size_t _off403 = 0xB7C;
    static constexpr std::size_t _off404 = 0xB80;
    static constexpr std::size_t _off405 = 0xB84;
    static constexpr std::size_t _off406 = 0xB88;
    static constexpr std::size_t _off407 = 0xB8C;
    static constexpr std::size_t _off408 = 0xB90;
    static constexpr std::size_t _off409 = 0xB94;
    static constexpr std::size_t _off410 = 0xB98;
    static constexpr std::size_t _off411 = 0xB9C;
    static constexpr std::size_t _off412 = 0xBA0;
    static constexpr std::size_t _off413 = 0xBA4;
    static constexpr std::size_t _off414 = 0xBA8;
    static constexpr std::size_t _off415 = 0xBAC;
    static constexpr std::size_t _off416 = 0xBB0;
    static constexpr std::size_t _off417 = 0xBB4;
    static constexpr std::size_t _off418 = 0xBB8;
    static constexpr std::size_t _off419 = 0xBBC;
    static constexpr std::size_t _off420 = 0xBC0;
    static constexpr std::size_t _off421 = 0xBC4;
    static constexpr std::size_t _off422 = 0xBC8;
    static constexpr std::size_t _off423 = 0xBCC;
    static constexpr std::size_t _off424 = 0xBD0;
    static constexpr std::size_t _off425 = 0xBD4;
    static constexpr std::size_t _off426 = 0xBD8;
    static constexpr std::size_t _off427 = 0xBDC;
    static constexpr std::size_t _off428 = 0xBE0;
    static constexpr std::size_t _off429 = 0xBE4;
    static constexpr std::size_t _off430 = 0xBE8;
    static constexpr std::size_t _off431 = 0xBEC;
    static constexpr std::size_t _off432 = 0xBF0;
    static constexpr std::size_t _off433 = 0xBF4;
    static constexpr std::size_t _off434 = 0xBF8;
    static constexpr std::size_t _off435 = 0xBFC;
    static constexpr std::size_t _off436 = 0xC00;
    static constexpr std::size_t _off437 = 0xC04;
    static constexpr std::size_t _off438 = 0xC08;
    static constexpr std::size_t _off439 = 0xC0C;
    static constexpr std::size_t _off440 = 0xC10;
    static constexpr std::size_t _off441 = 0xC14;
    static constexpr std::size_t _off442 = 0xC18;
    static constexpr std::size_t _off443 = 0xC1C;
    static constexpr std::size_t _off444 = 0xC20;
    static constexpr std::size_t _off445 = 0xC24;
    static constexpr std::size_t _off446 = 0xC28;
    static constexpr std::size_t _off447 = 0xC2C;
    static constexpr std::size_t _off448 = 0xC30;
    static constexpr std::size_t _off449 = 0xC34;
    static constexpr std::size_t _off450 = 0xC38;
    static constexpr std::size_t _off451 = 0xC3C;
    static constexpr std::size_t _off452 = 0xC40;
    static constexpr std::size_t _off453 = 0xC44;
    static constexpr std::size_t _off454 = 0xC48;
    static constexpr std::size_t _off455 = 0xC4C;
    static constexpr std::size_t _off456 = 0xC50;
    static constexpr std::size_t _off457 = 0xC54;
    static constexpr std::size_t _off458 = 0xC58;
    static constexpr std::size_t _off459 = 0xC5C;
    static constexpr std::size_t _off460 = 0xC60;
    static constexpr std::size_t _off461 = 0xC64;
    static constexpr std::size_t _off462 = 0xC68;
    static constexpr std::size_t _off463 = 0xC6C;
    static constexpr std::size_t _off464 = 0xC70;
    static constexpr std::size_t _off465 = 0xC74;
    static constexpr std::size_t _off466 = 0xC78;
    static constexpr std::size_t _off467 = 0xC7C;
    static constexpr std::size_t _off468 = 0xC80;
    static constexpr std::size_t _off469 = 0xC84;
    static constexpr std::size_t _off470 = 0xC88;
    static constexpr std::size_t _off471 = 0xC8C;
    static constexpr std::size_t _off472 = 0xC90;
    static constexpr std::size_t _off473 = 0xC94;
    static constexpr std::size_t _off474 = 0xC98;
    static constexpr std::size_t _off475 = 0xC9C;
    static constexpr std::size_t _off476 = 0xCA0;
    static constexpr std::size_t _off477 = 0xCA4;
    static constexpr std::size_t _off478 = 0xCA8;
    static constexpr std::size_t _off479 = 0xCAC;
    static constexpr std::size_t _off480 = 0xCB0;
    static constexpr std::size_t _off481 = 0xCB4;
    static constexpr std::size_t _off482 = 0xCB8;
    static constexpr std::size_t _off483 = 0xCBC;
    static constexpr std::size_t _off484 = 0xCC0;
    static constexpr std::size_t _off485 = 0xCC4;
    static constexpr std::size_t _off486 = 0xCC8;
    static constexpr std::size_t _off487 = 0xCCC;
    static constexpr std::size_t _off488 = 0xCD0;
    static constexpr std::size_t _off489 = 0xCD4;
    static constexpr std::size_t _off490 = 0xCD8;
    static constexpr std::size_t _off491 = 0xCDC;
    static constexpr std::size_t _off492 = 0xCE0;
    static constexpr std::size_t _off493 = 0xCE4;
    static constexpr std::size_t _off494 = 0xCE8;
    static constexpr std::size_t _off495 = 0xCEC;
    static constexpr std::size_t _off496 = 0xCF0;
    static constexpr std::size_t _off497 = 0xCF4;
    static constexpr std::size_t _off498 = 0xCF8;
    static constexpr std::size_t _off499 = 0xCFC;
    static constexpr std::size_t _off500 = 0xD00;
    static constexpr std::size_t _off501 = 0xD04;
    static constexpr std::size_t _off502 = 0xD08;
    static constexpr std::size_t _off503 = 0xD0C;
    static constexpr std::size_t _off504 = 0xD10;
    static constexpr std::size_t _off505 = 0xD14;
    static constexpr std::size_t _off506 = 0xD18;
    static constexpr std::size_t _off507 = 0xD1C;
    static constexpr std::size_t _off508 = 0xD20;
    static constexpr std::size_t _off509 = 0xD24;
    static constexpr std::size_t _off510 = 0xD28;
    static constexpr std::size_t _off511 = 0xD2C;
    static constexpr std::size_t _off512 = 0xD30;
    static constexpr std::size_t _off513 = 0xD34;
    static constexpr std::size_t _off514 = 0xD38;
    static constexpr std::size_t _off515 = 0xD3C;
    static constexpr std::size_t _off516 = 0xD40;
    static constexpr std::size_t _off517 = 0xD44;
    static constexpr std::size_t _off518 = 0xD48;
    static constexpr std::size_t _off519 = 0xD4C;
    static constexpr std::size_t _off520 = 0xD50;
    static constexpr std::size_t _off521 = 0xD54;
    static constexpr std::size_t _off522 = 0xD58;
    static constexpr std::size_t _off523 = 0xD5C;
    static constexpr std::size_t _off524 = 0xD60;
    static constexpr std::size_t _off525 = 0xD64;
    static constexpr std::size_t _off526 = 0xD68;
    static constexpr std::size_t _off527 = 0xD6C;
    static constexpr std::size_t _off528 = 0xD70;
    static constexpr std::size_t _off529 = 0xD74;
    static constexpr std::size_t _off530 = 0xD78;
    static constexpr std::size_t _off531 = 0xD7C;
    static constexpr std::size_t _off532 = 0xD80;
    static constexpr std::size_t _off533 = 0xD84;
    static constexpr std::size_t _off534 = 0xD88;
    static constexpr std::size_t _off535 = 0xD8C;
    static constexpr std::size_t _off536 = 0xD90;
    static constexpr std::size_t _off537 = 0xD94;
    static constexpr std::size_t _off538 = 0xD98;
    static constexpr std::size_t _off539 = 0xD9C;
    static constexpr std::size_t _off540 = 0xDA0;
    static constexpr std::size_t _off541 = 0xDA4;
    static constexpr std::size_t _off542 = 0xDA8;
    static constexpr std::size_t _off543 = 0xDAC;
    static constexpr std::size_t _off544 = 0xDB0;
    static constexpr std::size_t _off545 = 0xDB4;
    static constexpr std::size_t _off546 = 0xDB8;
    static constexpr std::size_t _off547 = 0xDBC;
    static constexpr std::size_t _off548 = 0xDC0;
    static constexpr std::size_t _off549 = 0xDC4;
    static constexpr std::size_t _off550 = 0xDC8;
    static constexpr std::size_t _off551 = 0xDCC;
    static constexpr std::size_t _off552 = 0xDD0;
    static constexpr std::size_t _off553 = 0xDD4;
    static constexpr std::size_t _off554 = 0xDD8;
    static constexpr std::size_t _off555 = 0xDDC;
    static constexpr std::size_t _off556 = 0xDE0;
    static constexpr std::size_t _off557 = 0xDE4;
    static constexpr std::size_t _off558 = 0xDE8;
    static constexpr std::size_t _off559 = 0xDEC;
    static constexpr std::size_t _off560 = 0xDF0;
    static constexpr std::size_t _off561 = 0xDF4;
    static constexpr std::size_t _off562 = 0xDF8;
    static constexpr std::size_t _off563 = 0xDFC;
    static constexpr std::size_t _off564 = 0xE00;
    static constexpr std::size_t _off565 = 0xE04;
    static constexpr std::size_t _off566 = 0xE08;
    static constexpr std::size_t _off567 = 0xE0C;
    static constexpr std::size_t _off568 = 0xE10;
    static constexpr std::size_t _off569 = 0xE14;
    static constexpr std::size_t _off570 = 0xE18;
    static constexpr std::size_t _off571 = 0xE1C;
    static constexpr std::size_t _off572 = 0xE20;
    static constexpr std::size_t _off573 = 0xE24;
    static constexpr std::size_t _off574 = 0xE28;
    static constexpr std::size_t _off575 = 0xE2C;
    static constexpr std::size_t _off576 = 0xE30;
    static constexpr std::size_t _off577 = 0xE34;
    static constexpr std::size_t _off578 = 0xE38;
    static constexpr std::size_t _off579 = 0xE3C;
    static constexpr std::size_t _off580 = 0xE40;
    static constexpr std::size_t _off581 = 0xE44;
    static constexpr std::size_t _off582 = 0xE48;
    static constexpr std::size_t _off583 = 0xE4C;
    static constexpr std::size_t _off584 = 0xE50;
    static constexpr std::size_t _off585 = 0xE54;
    static constexpr std::size_t _off586 = 0xE58;
    static constexpr std::size_t _off587 = 0xE5C;
    static constexpr std::size_t _off588 = 0xE60;
    static constexpr std::size_t _off589 = 0xE64;
    static constexpr std::size_t _off590 = 0xE68;
    static constexpr std::size_t _off591 = 0xE6C;
    static constexpr std::size_t _off592 = 0xE70;
    static constexpr std::size_t _off593 = 0xE74;
    static constexpr std::size_t _off594 = 0xE78;
    static constexpr std::size_t _off595 = 0xE7C;
    static constexpr std::size_t _off596 = 0xE80;
    static constexpr std::size_t _off597 = 0xE84;
    static constexpr std::size_t _off598 = 0xE88;
    static constexpr std::size_t _off599 = 0xE8C;
    static constexpr std::size_t _off600 = 0xE90;
    static constexpr std::size_t _off601 = 0xE94;
    static constexpr std::size_t _off602 = 0xE98;
    static constexpr std::size_t _off603 = 0xE9C;
    static constexpr std::size_t _off604 = 0xEA0;
    static constexpr std::size_t _off605 = 0xEA4;
    static constexpr std::size_t _off606 = 0xEA8;
    static constexpr std::size_t _off607 = 0xEAC;
    static constexpr std::size_t _off608 = 0xEB0;
    static constexpr std::size_t _off609 = 0xEB4;
    static constexpr std::size_t _off610 = 0xEB8;
    static constexpr std::size_t _off611 = 0xEBC;
    static constexpr std::size_t _off612 = 0xEC0;
    static constexpr std::size_t _off613 = 0xEC4;
    static constexpr std::size_t _off614 = 0xEC8;
    static constexpr std::size_t _off615 = 0xECC;
    static constexpr std::size_t _off616 = 0xED0;
    static constexpr std::size_t _off617 = 0xED4;
    static constexpr std::size_t _off618 = 0xED8;
    static constexpr std::size_t _off619 = 0xEDC;
    static constexpr std::size_t _off620 = 0xEE0;
    static constexpr std::size_t _off621 = 0xEE4;
    static constexpr std::size_t _off622 = 0xEE8;
    static constexpr std::size_t _off623 = 0xEEC;
    static constexpr std::size_t _off624 = 0xEF0;
    static constexpr std::size_t _off625 = 0xEF4;
    static constexpr std::size_t _off626 = 0xEF8;
    static constexpr std::size_t _off627 = 0xEFC;
    static constexpr std::size_t _off628 = 0xF00;
    static constexpr std::size_t _off629 = 0xF04;
    static constexpr std::size_t _off630 = 0xF08;
    static constexpr std::size_t _off631 = 0xF0C;
    static constexpr std::size_t _off632 = 0xF10;
    static constexpr std::size_t _off633 = 0xF14;
    static constexpr std::size_t _off634 = 0xF18;
    static constexpr std::size_t _off635 = 0xF1C;
    static constexpr std::size_t _off636 = 0xF20;
    static constexpr std::size_t _off637 = 0xF24;
    static constexpr std::size_t _off638 = 0xF28;
    static constexpr std::size_t _off639 = 0xF2C;
    std::unique_ptr<UInt16Array> field100_;
    std::unique_ptr<::fruityprime::memory::CollisionVolume> collision_;
    std::unique_ptr<::fruityprime::memory::CModel> gun_model_;
    std::unique_ptr<::fruityprime::memory::CModel> frozen_model_;
    std::unique_ptr<IntPtrArray> spine_node_;
    std::unique_ptr<IntPtrArray> shoot_node_;
    std::unique_ptr<::fruityprime::memory::CModel> biped1_;
    std::unique_ptr<::fruityprime::memory::CModel> biped2_;
    std::unique_ptr<::fruityprime::memory::CModel> alt_form_;
    std::unique_ptr<::fruityprime::memory::CModel> gun_smoke_;
    std::unique_ptr<::fruityprime::memory::PlayerControls> controls_;
    std::unique_ptr<::fruityprime::memory::PlayerInput> input_;
    std::unique_ptr<::fruityprime::memory::CameraInfo> camera_info_;
    std::unique_ptr<::fruityprime::memory::LightInfo> light_info_;
    std::unique_ptr<::fruityprime::memory::EquipInfoPtr> equip_info_;
    std::unique_ptr<::fruityprime::memory::CBeamProjectile> beam_head_;
    std::unique_ptr<::fruityprime::memory::AiData> ai_data_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
    std::unique_ptr<StructArray<::fruityprime::memory::AIContext>> a_i_context_;
    std::unique_ptr<StructArray<::fruityprime::memory::AIAggro>> a_i_aggro_;
};

class AiData : public MemoryClass {
public:
    AiData(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AiData(Buffer& buffer, std::uint32_t address);
    ~AiData() override;

    [[nodiscard]] std::uint32_t Player() const { return read_pointer(0x0); }
    void Player(std::uint32_t value) { write_pointer(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t player() const { return Player(); }
    void player(std::uint32_t value) { Player(value); }
    [[nodiscard]] std::uint32_t State() const { return read_pointer(0x4); }
    void State(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t state() const { return State(); }
    void state(std::uint32_t value) { State(value); }
    [[nodiscard]] std::uint32_t Shift258() const { return read_pointer(0x8); }
    void Shift258(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shift258() const { return Shift258(); }
    void shift258(std::uint32_t value) { Shift258(value); }
    [[nodiscard]] std::uint32_t Shift1020() const { return read_pointer(0xC); }
    void Shift1020(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shift1020() const { return Shift1020(); }
    void shift1020(std::uint32_t value) { Shift1020(value); }
    [[nodiscard]] std::uint32_t Shift2FC() const { return read_pointer(0x10); }
    void Shift2FC(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t shift2_f_c() const { return Shift2FC(); }
    void shift2_f_c(std::uint32_t value) { Shift2FC(value); }
    [[nodiscard]] std::uint32_t TargetPlayer() const { return read_pointer(0x14); }
    void TargetPlayer(std::uint32_t value) { write_pointer(0x14, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target_player() const { return TargetPlayer(); }
    void target_player(std::uint32_t value) { TargetPlayer(value); }
    [[nodiscard]] std::uint32_t Players() const { return read_pointer(0x18); }
    void Players(std::uint32_t value) { write_pointer(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t players() const { return Players(); }
    void players(std::uint32_t value) { Players(value); }
    [[nodiscard]] std::uint32_t TargetHalfturret() const { return read_pointer(0x1C); }
    void TargetHalfturret(std::uint32_t value) { write_pointer(0x1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target_halfturret() const { return TargetHalfturret(); }
    void target_halfturret(std::uint32_t value) { TargetHalfturret(value); }
    [[nodiscard]] std::uint16_t CurNodedataCount() const { return read_u16(0x20); }
    void CurNodedataCount(std::uint16_t value) { write_u16(0x20, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t cur_nodedata_count() const { return CurNodedataCount(); }
    void cur_nodedata_count(std::uint16_t value) { CurNodedataCount(value); }
    [[nodiscard]] std::uint16_t NodedataSetIdx() const { return read_u16(0x22); }
    void NodedataSetIdx(std::uint16_t value) { write_u16(0x22, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t nodedata_set_idx() const { return NodedataSetIdx(); }
    void nodedata_set_idx(std::uint16_t value) { NodedataSetIdx(value); }
    [[nodiscard]] UInt16Array& CurNodeTypeIndex() noexcept;
    [[nodiscard]] const UInt16Array& CurNodeTypeIndex() const noexcept;
    [[nodiscard]] UInt16Array& cur_node_type_index() noexcept { return CurNodeTypeIndex(); }
    [[nodiscard]] const UInt16Array& cur_node_type_index() const noexcept { return CurNodeTypeIndex(); }
    [[nodiscard]] std::uint16_t Field30() const { return read_u16(0x30); }
    void Field30(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field30() const { return Field30(); }
    void field30(std::uint16_t value) { Field30(value); }
    [[nodiscard]] std::uint16_t Padding32() const { return read_u16(0x32); }
    void Padding32(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding32() const { return Padding32(); }
    void padding32(std::uint16_t value) { Padding32(value); }
    [[nodiscard]] std::uint32_t CurNodedata() const { return read_pointer(0x34); }
    void CurNodedata(std::uint32_t value) { write_pointer(0x34, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t cur_nodedata() const { return CurNodedata(); }
    void cur_nodedata(std::uint32_t value) { CurNodedata(value); }
    [[nodiscard]] std::uint32_t Nodedata() const { return read_pointer(0x38); }
    void Nodedata(std::uint32_t value) { write_pointer(0x38, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata() const { return Nodedata(); }
    void nodedata(std::uint32_t value) { Nodedata(value); }
    [[nodiscard]] std::uint32_t Node3C() const { return read_pointer(0x3C); }
    void Node3C(std::uint32_t value) { write_pointer(0x3C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node3_c() const { return Node3C(); }
    void node3_c(std::uint32_t value) { Node3C(value); }
    [[nodiscard]] std::uint32_t Node40() const { return read_pointer(0x40); }
    void Node40(std::uint32_t value) { write_pointer(0x40, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node40() const { return Node40(); }
    void node40(std::uint32_t value) { Node40(value); }
    [[nodiscard]] std::uint32_t Node44() const { return read_pointer(0x44); }
    void Node44(std::uint32_t value) { write_pointer(0x44, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node44() const { return Node44(); }
    void node44(std::uint32_t value) { Node44(value); }
    [[nodiscard]] std::uint32_t Node48() const { return read_pointer(0x48); }
    void Node48(std::uint32_t value) { write_pointer(0x48, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node48() const { return Node48(); }
    void node48(std::uint32_t value) { Node48(value); }
    [[nodiscard]] IntPtrArray& Field4C() noexcept;
    [[nodiscard]] const IntPtrArray& Field4C() const noexcept;
    [[nodiscard]] IntPtrArray& field4_c() noexcept { return Field4C(); }
    [[nodiscard]] const IntPtrArray& field4_c() const noexcept { return Field4C(); }
    [[nodiscard]] std::uint16_t Field78() const { return read_u16(0x78); }
    void Field78(std::uint16_t value) { write_u16(0x78, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field78() const { return Field78(); }
    void field78(std::uint16_t value) { Field78(value); }
    [[nodiscard]] UInt16Array& Field7A() noexcept;
    [[nodiscard]] const UInt16Array& Field7A() const noexcept;
    [[nodiscard]] UInt16Array& field7_a() noexcept { return Field7A(); }
    [[nodiscard]] const UInt16Array& field7_a() const noexcept { return Field7A(); }
    [[nodiscard]] std::uint16_t Padding8E() const { return read_u16(0x8E); }
    void Padding8E(std::uint16_t value) { write_u16(0x8E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding8_e() const { return Padding8E(); }
    void padding8_e(std::uint16_t value) { Padding8E(value); }
    [[nodiscard]] formats::Vector3 Field90() const { return read_vec3(0x90); }
    void Field90(formats::Vector3 value) { write_vec3(0x90, value); }
    [[nodiscard]] formats::Vector3 field90() const { return Field90(); }
    void field90(formats::Vector3 value) { Field90(value); }
    [[nodiscard]] std::int32_t Field9C() const { return read_i32(0x9C); }
    void Field9C(std::int32_t value) { write_i32(0x9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c() const { return Field9C(); }
    void field9_c(std::int32_t value) { Field9C(value); }
    [[nodiscard]] formats::Vector3 FieldA0() const { return read_vec3(0xA0); }
    void FieldA0(formats::Vector3 value) { write_vec3(0xA0, value); }
    [[nodiscard]] formats::Vector3 field_a0() const { return FieldA0(); }
    void field_a0(formats::Vector3 value) { FieldA0(value); }
    [[nodiscard]] formats::Vector3 FieldAC() const { return read_vec3(0xAC); }
    void FieldAC(formats::Vector3 value) { write_vec3(0xAC, value); }
    [[nodiscard]] formats::Vector3 field_a_c() const { return FieldAC(); }
    void field_a_c(formats::Vector3 value) { FieldAC(value); }
    [[nodiscard]] formats::Vector3 FieldB8() const { return read_vec3(0xB8); }
    void FieldB8(formats::Vector3 value) { write_vec3(0xB8, value); }
    [[nodiscard]] formats::Vector3 field_b8() const { return FieldB8(); }
    void field_b8(formats::Vector3 value) { FieldB8(value); }
    [[nodiscard]] std::uint32_t ItemSpawnC4() const { return read_pointer(0xC4); }
    void ItemSpawnC4(std::uint32_t value) { write_pointer(0xC4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t item_spawn_c4() const { return ItemSpawnC4(); }
    void item_spawn_c4(std::uint32_t value) { ItemSpawnC4(value); }
    [[nodiscard]] std::uint32_t ItemC8() const { return read_pointer(0xC8); }
    void ItemC8(std::uint32_t value) { write_pointer(0xC8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t item_c8() const { return ItemC8(); }
    void item_c8(std::uint32_t value) { ItemC8(value); }
    [[nodiscard]] std::uint32_t OctoFlagCC() const { return read_pointer(0xCC); }
    void OctoFlagCC(std::uint32_t value) { write_pointer(0xCC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t octo_flag_c_c() const { return OctoFlagCC(); }
    void octo_flag_c_c(std::uint32_t value) { OctoFlagCC(value); }
    [[nodiscard]] std::uint32_t FlagBaseD0() const { return read_pointer(0xD0); }
    void FlagBaseD0(std::uint32_t value) { write_pointer(0xD0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t flag_base_d0() const { return FlagBaseD0(); }
    void flag_base_d0(std::uint32_t value) { FlagBaseD0(value); }
    [[nodiscard]] std::uint32_t OctoFlagD4() const { return read_pointer(0xD4); }
    void OctoFlagD4(std::uint32_t value) { write_pointer(0xD4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t octo_flag_d4() const { return OctoFlagD4(); }
    void octo_flag_d4(std::uint32_t value) { OctoFlagD4(value); }
    [[nodiscard]] std::uint32_t FlagBaseD8() const { return read_pointer(0xD8); }
    void FlagBaseD8(std::uint32_t value) { write_pointer(0xD8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t flag_base_d8() const { return FlagBaseD8(); }
    void flag_base_d8(std::uint32_t value) { FlagBaseD8(value); }
    [[nodiscard]] std::uint32_t OctoFlagDC() const { return read_pointer(0xDC); }
    void OctoFlagDC(std::uint32_t value) { write_pointer(0xDC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t octo_flag_d_c() const { return OctoFlagDC(); }
    void octo_flag_d_c(std::uint32_t value) { OctoFlagDC(value); }
    [[nodiscard]] std::uint32_t FlagBaseE0() const { return read_pointer(0xE0); }
    void FlagBaseE0(std::uint32_t value) { write_pointer(0xE0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t flag_base_e0() const { return FlagBaseE0(); }
    void flag_base_e0(std::uint32_t value) { FlagBaseE0(value); }
    [[nodiscard]] std::uint32_t TargetDefense() const { return read_pointer(0xE4); }
    void TargetDefense(std::uint32_t value) { write_pointer(0xE4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target_defense() const { return TargetDefense(); }
    void target_defense(std::uint32_t value) { TargetDefense(value); }
    [[nodiscard]] std::uint32_t TargetDoor() const { return read_pointer(0xE8); }
    void TargetDoor(std::uint32_t value) { write_pointer(0xE8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target_door() const { return TargetDoor(); }
    void target_door(std::uint32_t value) { TargetDoor(value); }
    [[nodiscard]] ::fruityprime::players::AiFlags2 Flags2() const { return static_cast<::fruityprime::players::AiFlags2>(read_u32(0xEC)); }
    void Flags2(::fruityprime::players::AiFlags2 value) { write_u32(0xEC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] ::fruityprime::players::AiFlags2 flags2() const { return Flags2(); }
    void flags2(::fruityprime::players::AiFlags2 value) { Flags2(value); }
    [[nodiscard]] Int32Array& SlotsHitTotal() noexcept;
    [[nodiscard]] const Int32Array& SlotsHitTotal() const noexcept;
    [[nodiscard]] Int32Array& slots_hit_total() noexcept { return SlotsHitTotal(); }
    [[nodiscard]] const Int32Array& slots_hit_total() const noexcept { return SlotsHitTotal(); }
    [[nodiscard]] Int32Array& SlotsDamageTotal() noexcept;
    [[nodiscard]] const Int32Array& SlotsDamageTotal() const noexcept;
    [[nodiscard]] Int32Array& slots_damage_total() noexcept { return SlotsDamageTotal(); }
    [[nodiscard]] const Int32Array& slots_damage_total() const noexcept { return SlotsDamageTotal(); }
    [[nodiscard]] std::int32_t HalfturretDmg() const { return read_i32(0x110); }
    void HalfturretDmg(std::int32_t value) { write_i32(0x110, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t halfturret_dmg() const { return HalfturretDmg(); }
    void halfturret_dmg(std::int32_t value) { HalfturretDmg(value); }
    [[nodiscard]] std::uint8_t SlotIndex() const { return read_u8(0x114); }
    void SlotIndex(std::uint8_t value) { write_u8(0x114, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t slot_index() const { return SlotIndex(); }
    void slot_index(std::uint8_t value) { SlotIndex(value); }
    [[nodiscard]] std::uint8_t QueuedFindEntAction() const { return read_u8(0x115); }
    void QueuedFindEntAction(std::uint8_t value) { write_u8(0x115, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t queued_find_ent_action() const { return QueuedFindEntAction(); }
    void queued_find_ent_action(std::uint8_t value) { QueuedFindEntAction(value); }
    [[nodiscard]] std::uint16_t Field116() const { return read_u16(0x116); }
    void Field116(std::uint16_t value) { write_u16(0x116, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field116() const { return Field116(); }
    void field116(std::uint16_t value) { Field116(value); }
    [[nodiscard]] std::int32_t Field118() const { return read_i32(0x118); }
    void Field118(std::int32_t value) { write_i32(0x118, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field118() const { return Field118(); }
    void field118(std::int32_t value) { Field118(value); }
    [[nodiscard]] std::int32_t Unused11C() const { return read_i32(0x11C); }
    void Unused11C(std::int32_t value) { write_i32(0x11C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused11_c() const { return Unused11C(); }
    void unused11_c(std::int32_t value) { Unused11C(value); }
    [[nodiscard]] IntPtrArray& EntList() noexcept;
    [[nodiscard]] const IntPtrArray& EntList() const noexcept;
    [[nodiscard]] IntPtrArray& ent_list() noexcept { return EntList(); }
    [[nodiscard]] const IntPtrArray& ent_list() const noexcept { return EntList(); }
    [[nodiscard]] StructArray<::fruityprime::memory::AiButton>& Buttons() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AiButton>& Buttons() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AiButton>& buttons() noexcept { return Buttons(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AiButton>& buttons() const noexcept { return Buttons(); }
    [[nodiscard]] std::uint16_t TouchAimX() const { return read_u16(0x2A0); }
    void TouchAimX(std::uint16_t value) { write_u16(0x2A0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t touch_aim_x() const { return TouchAimX(); }
    void touch_aim_x(std::uint16_t value) { TouchAimX(value); }
    [[nodiscard]] std::uint16_t TouchAimY() const { return read_u16(0x2A2); }
    void TouchAimY(std::uint16_t value) { write_u16(0x2A2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t touch_aim_y() const { return TouchAimY(); }
    void touch_aim_y(std::uint16_t value) { TouchAimY(value); }
    [[nodiscard]] std::uint16_t TouchInputFlag() const { return read_u16(0x2A4); }
    void TouchInputFlag(std::uint16_t value) { write_u16(0x2A4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t touch_input_flag() const { return TouchInputFlag(); }
    void touch_input_flag(std::uint16_t value) { TouchInputFlag(value); }
    [[nodiscard]] std::uint16_t FramesWithTouch() const { return read_u16(0x2A6); }
    void FramesWithTouch(std::uint16_t value) { write_u16(0x2A6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t frames_with_touch() const { return FramesWithTouch(); }
    void frames_with_touch(std::uint16_t value) { FramesWithTouch(value); }
    [[nodiscard]] std::uint16_t FramesWithoutTouch() const { return read_u16(0x2A8); }
    void FramesWithoutTouch(std::uint16_t value) { write_u16(0x2A8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t frames_without_touch() const { return FramesWithoutTouch(); }
    void frames_without_touch(std::uint16_t value) { FramesWithoutTouch(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::AiButton>& TouchBtns() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AiButton>& TouchBtns() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AiButton>& touch_btns() noexcept { return TouchBtns(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AiButton>& touch_btns() const noexcept { return TouchBtns(); }
    [[nodiscard]] std::int32_t BtnAimX() const { return read_i32(0x2EC); }
    void BtnAimX(std::int32_t value) { write_i32(0x2EC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t btn_aim_x() const { return BtnAimX(); }
    void btn_aim_x(std::int32_t value) { BtnAimX(value); }
    [[nodiscard]] std::int32_t BtnAimY() const { return read_i32(0x2F0); }
    void BtnAimY(std::int32_t value) { write_i32(0x2F0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t btn_aim_y() const { return BtnAimY(); }
    void btn_aim_y(std::int32_t value) { BtnAimY(value); }
    [[nodiscard]] std::uint8_t NodedataSelOff() const { return read_u8(0x2F4); }
    void NodedataSelOff(std::uint8_t value) { write_u8(0x2F4, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t nodedata_sel_off() const { return NodedataSelOff(); }
    void nodedata_sel_off(std::uint8_t value) { NodedataSelOff(value); }
    [[nodiscard]] std::uint8_t NodedataSelOn() const { return read_u8(0x2F5); }
    void NodedataSelOn(std::uint8_t value) { write_u8(0x2F5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t nodedata_sel_on() const { return NodedataSelOn(); }
    void nodedata_sel_on(std::uint8_t value) { NodedataSelOn(value); }
    [[nodiscard]] std::uint16_t Padding2F6() const { return read_u16(0x2F6); }
    void Padding2F6(std::uint16_t value) { write_u16(0x2F6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding2_f6() const { return Padding2F6(); }
    void padding2_f6(std::uint16_t value) { Padding2F6(value); }
    [[nodiscard]] ::fruityprime::players::AiFlags3 Flags3() const { return static_cast<::fruityprime::players::AiFlags3>(read_u32(0x2F8)); }
    void Flags3(::fruityprime::players::AiFlags3 value) { write_u32(0x2F8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] ::fruityprime::players::AiFlags3 flags3() const { return Flags3(); }
    void flags3(::fruityprime::players::AiFlags3 value) { Flags3(value); }
    [[nodiscard]] Int32Array& FuncTree() noexcept;
    [[nodiscard]] const Int32Array& FuncTree() const noexcept;
    [[nodiscard]] Int32Array& func_tree() noexcept { return FuncTree(); }
    [[nodiscard]] const Int32Array& func_tree() const noexcept { return FuncTree(); }
    [[nodiscard]] std::uint32_t Personality() const { return read_pointer(0x101C); }
    void Personality(std::uint32_t value) { write_pointer(0x101C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t personality() const { return Personality(); }
    void personality(std::uint32_t value) { Personality(value); }
    [[nodiscard]] std::int32_t Field1020() const { return read_i32(0x1020); }
    void Field1020(std::int32_t value) { write_i32(0x1020, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1020() const { return Field1020(); }
    void field1020(std::int32_t value) { Field1020(value); }
    [[nodiscard]] std::uint16_t Weapon1() const { return read_u16(0x1024); }
    void Weapon1(std::uint16_t value) { write_u16(0x1024, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t weapon1() const { return Weapon1(); }
    void weapon1(std::uint16_t value) { Weapon1(value); }
    [[nodiscard]] std::uint16_t Weapon2() const { return read_u16(0x1026); }
    void Weapon2(std::uint16_t value) { write_u16(0x1026, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t weapon2() const { return Weapon2(); }
    void weapon2(std::uint16_t value) { Weapon2(value); }
    [[nodiscard]] std::uint16_t FindWeaponIndex() const { return read_u16(0x1028); }
    void FindWeaponIndex(std::uint16_t value) { write_u16(0x1028, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t find_weapon_index() const { return FindWeaponIndex(); }
    void find_weapon_index(std::uint16_t value) { FindWeaponIndex(value); }
    [[nodiscard]] std::uint16_t ShotDelay() const { return read_u16(0x102A); }
    void ShotDelay(std::uint16_t value) { write_u16(0x102A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t shot_delay() const { return ShotDelay(); }
    void shot_delay(std::uint16_t value) { ShotDelay(value); }
    [[nodiscard]] std::uint16_t Field102C() const { return read_u16(0x102C); }
    void Field102C(std::uint16_t value) { write_u16(0x102C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field102_c() const { return Field102C(); }
    void field102_c(std::uint16_t value) { Field102C(value); }
    [[nodiscard]] std::uint16_t Field102E() const { return read_u16(0x102E); }
    void Field102E(std::uint16_t value) { write_u16(0x102E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field102_e() const { return Field102E(); }
    void field102_e(std::uint16_t value) { Field102E(value); }
    [[nodiscard]] std::uint16_t Field1030() const { return read_u16(0x1030); }
    void Field1030(std::uint16_t value) { write_u16(0x1030, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1030() const { return Field1030(); }
    void field1030(std::uint16_t value) { Field1030(value); }
    [[nodiscard]] std::uint16_t Field1032() const { return read_u16(0x1032); }
    void Field1032(std::uint16_t value) { write_u16(0x1032, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1032() const { return Field1032(); }
    void field1032(std::uint16_t value) { Field1032(value); }
    [[nodiscard]] std::uint16_t Field1034() const { return read_u16(0x1034); }
    void Field1034(std::uint16_t value) { write_u16(0x1034, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1034() const { return Field1034(); }
    void field1034(std::uint16_t value) { Field1034(value); }
    [[nodiscard]] std::uint16_t Padding1036() const { return read_u16(0x1036); }
    void Padding1036(std::uint16_t value) { write_u16(0x1036, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding1036() const { return Padding1036(); }
    void padding1036(std::uint16_t value) { Padding1036(value); }
    [[nodiscard]] formats::Vector3 Field1038() const { return read_vec3(0x1038); }
    void Field1038(formats::Vector3 value) { write_vec3(0x1038, value); }
    [[nodiscard]] formats::Vector3 field1038() const { return Field1038(); }
    void field1038(formats::Vector3 value) { Field1038(value); }
    [[nodiscard]] ::fruityprime::players::AiFlags4 Flags4() const { return static_cast<::fruityprime::players::AiFlags4>(read_u8(0x1044)); }
    void Flags4(::fruityprime::players::AiFlags4 value) { write_u8(0x1044, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] ::fruityprime::players::AiFlags4 flags4() const { return Flags4(); }
    void flags4(::fruityprime::players::AiFlags4 value) { Flags4(value); }
    [[nodiscard]] std::uint8_t Padding1045() const { return read_u8(0x1045); }
    void Padding1045(std::uint8_t value) { write_u8(0x1045, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding1045() const { return Padding1045(); }
    void padding1045(std::uint8_t value) { Padding1045(value); }
    [[nodiscard]] std::uint16_t Padding1046() const { return read_u16(0x1046); }
    void Padding1046(std::uint16_t value) { write_u16(0x1046, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding1046() const { return Padding1046(); }
    void padding1046(std::uint16_t value) { Padding1046(value); }
    [[nodiscard]] formats::Vector3 Field1048() const { return read_vec3(0x1048); }
    void Field1048(formats::Vector3 value) { write_vec3(0x1048, value); }
    [[nodiscard]] formats::Vector3 field1048() const { return Field1048(); }
    void field1048(formats::Vector3 value) { Field1048(value); }
    [[nodiscard]] formats::Vector3 Field1054() const { return read_vec3(0x1054); }
    void Field1054(formats::Vector3 value) { write_vec3(0x1054, value); }
    [[nodiscard]] formats::Vector3 field1054() const { return Field1054(); }
    void field1054(formats::Vector3 value) { Field1054(value); }
    [[nodiscard]] std::uint32_t AggroCount() const { return read_u32(0x1060); }
    void AggroCount(std::uint32_t value) { write_u32(0x1060, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t aggro_count() const { return AggroCount(); }
    void aggro_count(std::uint32_t value) { AggroCount(value); }
    [[nodiscard]] Int32Array& Aggro() noexcept;
    [[nodiscard]] const Int32Array& Aggro() const noexcept;
    [[nodiscard]] Int32Array& aggro() noexcept { return Aggro(); }
    [[nodiscard]] const Int32Array& aggro() const noexcept { return Aggro(); }
    [[nodiscard]] bool Flags1() const { return read_i32(0x11F4) != 0; }
    void Flags1(bool value) { write_i32(0x11F4, value ? 1 : 0); }
    [[nodiscard]] bool flags1() const { return Flags1(); }
    void flags1(bool value) { Flags1(value); }
    [[nodiscard]] std::uint16_t HealthThreshold() const { return read_u16(0x11F8); }
    void HealthThreshold(std::uint16_t value) { write_u16(0x11F8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t health_threshold() const { return HealthThreshold(); }
    void health_threshold(std::uint16_t value) { HealthThreshold(value); }
    [[nodiscard]] std::uint16_t Padding11FA() const { return read_u16(0x11FA); }
    void Padding11FA(std::uint16_t value) { write_u16(0x11FA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding11_f_a() const { return Padding11FA(); }
    void padding11_f_a(std::uint16_t value) { Padding11FA(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
    static constexpr std::size_t _off6 = 0x18;
    static constexpr std::size_t _off7 = 0x1C;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x22;
    static constexpr std::size_t _off10 = 0x24;
    static constexpr std::size_t _off11 = 0x30;
    static constexpr std::size_t _off12 = 0x32;
    static constexpr std::size_t _off13 = 0x34;
    static constexpr std::size_t _off14 = 0x38;
    static constexpr std::size_t _off15 = 0x3C;
    static constexpr std::size_t _off16 = 0x40;
    static constexpr std::size_t _off17 = 0x44;
    static constexpr std::size_t _off18 = 0x48;
    static constexpr std::size_t _off19 = 0x4C;
    static constexpr std::size_t _off20 = 0x78;
    static constexpr std::size_t _off21 = 0x7A;
    static constexpr std::size_t _off22 = 0x8E;
    static constexpr std::size_t _off23 = 0x90;
    static constexpr std::size_t _off24 = 0x9C;
    static constexpr std::size_t _off25 = 0xA0;
    static constexpr std::size_t _off26 = 0xAC;
    static constexpr std::size_t _off27 = 0xB8;
    static constexpr std::size_t _off28 = 0xC4;
    static constexpr std::size_t _off29 = 0xC8;
    static constexpr std::size_t _off30 = 0xCC;
    static constexpr std::size_t _off31 = 0xD0;
    static constexpr std::size_t _off32 = 0xD4;
    static constexpr std::size_t _off33 = 0xD8;
    static constexpr std::size_t _off34 = 0xDC;
    static constexpr std::size_t _off35 = 0xE0;
    static constexpr std::size_t _off36 = 0xE4;
    static constexpr std::size_t _off37 = 0xE8;
    static constexpr std::size_t _off38 = 0xEC;
    static constexpr std::size_t _off39 = 0xF0;
    static constexpr std::size_t _off40 = 0x100;
    static constexpr std::size_t _off41 = 0x110;
    static constexpr std::size_t _off42 = 0x114;
    static constexpr std::size_t _off43 = 0x115;
    static constexpr std::size_t _off44 = 0x116;
    static constexpr std::size_t _off45 = 0x118;
    static constexpr std::size_t _off46 = 0x11C;
    static constexpr std::size_t _off47 = 0x120;
    static constexpr std::size_t _off48 = 0x258;
    static constexpr std::size_t _off49 = 0x2A0;
    static constexpr std::size_t _off50 = 0x2A2;
    static constexpr std::size_t _off51 = 0x2A4;
    static constexpr std::size_t _off52 = 0x2A6;
    static constexpr std::size_t _off53 = 0x2A8;
    static constexpr std::size_t _off54 = 0x2AA;
    static constexpr std::size_t _off55 = 0x2EC;
    static constexpr std::size_t _off56 = 0x2F0;
    static constexpr std::size_t _off57 = 0x2F4;
    static constexpr std::size_t _off58 = 0x2F5;
    static constexpr std::size_t _off59 = 0x2F6;
    static constexpr std::size_t _off60 = 0x2F8;
    static constexpr std::size_t _off61 = 0x2FC;
    static constexpr std::size_t _off62 = 0x101C;
    static constexpr std::size_t _off63 = 0x1020;
    static constexpr std::size_t _off64 = 0x1024;
    static constexpr std::size_t _off65 = 0x1026;
    static constexpr std::size_t _off66 = 0x1028;
    static constexpr std::size_t _off67 = 0x102A;
    static constexpr std::size_t _off68 = 0x102C;
    static constexpr std::size_t _off69 = 0x102E;
    static constexpr std::size_t _off70 = 0x1030;
    static constexpr std::size_t _off71 = 0x1032;
    static constexpr std::size_t _off72 = 0x1034;
    static constexpr std::size_t _off73 = 0x1036;
    static constexpr std::size_t _off74 = 0x1038;
    static constexpr std::size_t _off75 = 0x1044;
    static constexpr std::size_t _off76 = 0x1045;
    static constexpr std::size_t _off77 = 0x1046;
    static constexpr std::size_t _off78 = 0x1048;
    static constexpr std::size_t _off79 = 0x1054;
    static constexpr std::size_t _off80 = 0x1060;
    static constexpr std::size_t _off81 = 0x1064;
    static constexpr std::size_t _off82 = 0x11F4;
    static constexpr std::size_t _off83 = 0x11F8;
    static constexpr std::size_t _off84 = 0x11FA;
    std::unique_ptr<UInt16Array> cur_node_type_index_;
    std::unique_ptr<IntPtrArray> field4_c_;
    std::unique_ptr<UInt16Array> field7_a_;
    std::unique_ptr<Int32Array> slots_hit_total_;
    std::unique_ptr<Int32Array> slots_damage_total_;
    std::unique_ptr<IntPtrArray> ent_list_;
    std::unique_ptr<StructArray<::fruityprime::memory::AiButton>> buttons_;
    std::unique_ptr<StructArray<::fruityprime::memory::AiButton>> touch_btns_;
    std::unique_ptr<Int32Array> func_tree_;
    std::unique_ptr<Int32Array> aggro_;
};

class AIContext : public MemoryClass {
public:
    AIContext(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AIContext(Buffer& buffer, std::uint32_t address);
    ~AIContext() override;

    [[nodiscard]] std::int32_t Func24Id() const { return read_i32(0x0); }
    void Func24Id(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t func24_id() const { return Func24Id(); }
    void func24_id(std::int32_t value) { Func24Id(value); }
    [[nodiscard]] std::uint8_t Field4() const { return read_u8(0x4); }
    void Field4(std::uint8_t value) { write_u8(0x4, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field4() const { return Field4(); }
    void field4(std::uint8_t value) { Field4(value); }
    [[nodiscard]] std::uint8_t Field5() const { return read_u8(0x5); }
    void Field5(std::uint8_t value) { write_u8(0x5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field5() const { return Field5(); }
    void field5(std::uint8_t value) { Field5(value); }
    [[nodiscard]] std::uint8_t Field6() const { return read_u8(0x6); }
    void Field6(std::uint8_t value) { write_u8(0x6, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field6() const { return Field6(); }
    void field6(std::uint8_t value) { Field6(value); }
    [[nodiscard]] std::uint8_t Field7() const { return read_u8(0x7); }
    void Field7(std::uint8_t value) { write_u8(0x7, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field7() const { return Field7(); }
    void field7(std::uint8_t value) { Field7(value); }
    [[nodiscard]] std::uint8_t Field8() const { return read_u8(0x8); }
    void Field8(std::uint8_t value) { write_u8(0x8, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field8() const { return Field8(); }
    void field8(std::uint8_t value) { Field8(value); }
    [[nodiscard]] std::uint8_t Field9() const { return read_u8(0x9); }
    void Field9(std::uint8_t value) { write_u8(0x9, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field9() const { return Field9(); }
    void field9(std::uint8_t value) { Field9(value); }
    [[nodiscard]] std::uint8_t FieldA() const { return read_u8(0xA); }
    void FieldA(std::uint8_t value) { write_u8(0xA, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_a() const { return FieldA(); }
    void field_a(std::uint8_t value) { FieldA(value); }
    [[nodiscard]] std::uint8_t FieldB() const { return read_u8(0xB); }
    void FieldB(std::uint8_t value) { write_u8(0xB, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_b() const { return FieldB(); }
    void field_b(std::uint8_t value) { FieldB(value); }
    [[nodiscard]] std::uint8_t FieldC() const { return read_u8(0xC); }
    void FieldC(std::uint8_t value) { write_u8(0xC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_c() const { return FieldC(); }
    void field_c(std::uint8_t value) { FieldC(value); }
    [[nodiscard]] std::uint8_t FieldD() const { return read_u8(0xD); }
    void FieldD(std::uint8_t value) { write_u8(0xD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d() const { return FieldD(); }
    void field_d(std::uint8_t value) { FieldD(value); }
    [[nodiscard]] std::uint8_t FieldE() const { return read_u8(0xE); }
    void FieldE(std::uint8_t value) { write_u8(0xE, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_e() const { return FieldE(); }
    void field_e(std::uint8_t value) { FieldE(value); }
    [[nodiscard]] std::uint8_t FieldF() const { return read_u8(0xF); }
    void FieldF(std::uint8_t value) { write_u8(0xF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_f() const { return FieldF(); }
    void field_f(std::uint8_t value) { FieldF(value); }
    [[nodiscard]] std::uint8_t Field10() const { return read_u8(0x10); }
    void Field10(std::uint8_t value) { write_u8(0x10, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field10() const { return Field10(); }
    void field10(std::uint8_t value) { Field10(value); }
    [[nodiscard]] std::uint8_t Padding11() const { return read_u8(0x11); }
    void Padding11(std::uint8_t value) { write_u8(0x11, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding11() const { return Padding11(); }
    void padding11(std::uint8_t value) { Padding11(value); }
    [[nodiscard]] std::uint16_t Padding12() const { return read_u16(0x12); }
    void Padding12(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding12() const { return Padding12(); }
    void padding12(std::uint16_t value) { Padding12(value); }
    [[nodiscard]] std::int32_t Field14() const { return read_i32(0x14); }
    void Field14(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14() const { return Field14(); }
    void field14(std::int32_t value) { Field14(value); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] formats::Vector3 Field34() const { return read_vec3(0x34); }
    void Field34(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 field34() const { return Field34(); }
    void field34(formats::Vector3 value) { Field34(value); }
    [[nodiscard]] std::int32_t Field40() const { return read_i32(0x40); }
    void Field40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40() const { return Field40(); }
    void field40(std::int32_t value) { Field40(value); }
    [[nodiscard]] std::int32_t Field44() const { return read_i32(0x44); }
    void Field44(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field44() const { return Field44(); }
    void field44(std::int32_t value) { Field44(value); }
    [[nodiscard]] std::uint32_t CurData1Iter() const { return read_pointer(0x48); }
    void CurData1Iter(std::uint32_t value) { write_pointer(0x48, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t cur_data1_iter() const { return CurData1Iter(); }
    void cur_data1_iter(std::uint32_t value) { CurData1Iter(value); }
    [[nodiscard]] std::int32_t CallCount() const { return read_i32(0x4C); }
    void CallCount(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t call_count() const { return CallCount(); }
    void call_count(std::int32_t value) { CallCount(value); }
    [[nodiscard]] std::uint8_t Depth() const { return read_u8(0x50); }
    void Depth(std::uint8_t value) { write_u8(0x50, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t depth() const { return Depth(); }
    void depth(std::uint8_t value) { Depth(value); }
    [[nodiscard]] std::uint8_t Padding51() const { return read_u8(0x51); }
    void Padding51(std::uint8_t value) { write_u8(0x51, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding51() const { return Padding51(); }
    void padding51(std::uint8_t value) { Padding51(value); }
    [[nodiscard]] std::uint16_t Padding52() const { return read_u16(0x52); }
    void Padding52(std::uint16_t value) { write_u16(0x52, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding52() const { return Padding52(); }
    void padding52(std::uint16_t value) { Padding52(value); }
    [[nodiscard]] Int32Array& Weights() noexcept;
    [[nodiscard]] const Int32Array& Weights() const noexcept;
    [[nodiscard]] Int32Array& weights() noexcept { return Weights(); }
    [[nodiscard]] const Int32Array& weights() const noexcept { return Weights(); }
    [[nodiscard]] ::fruityprime::memory::AIData1* AIData1() noexcept;
    [[nodiscard]] const ::fruityprime::memory::AIData1* AIData1() const noexcept;
    [[nodiscard]] ::fruityprime::memory::AIData1* ai_data1() noexcept { return AIData1(); }
    [[nodiscard]] const ::fruityprime::memory::AIData1* ai_data1() const noexcept { return AIData1(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x5;
    static constexpr std::size_t _off3 = 0x6;
    static constexpr std::size_t _off4 = 0x7;
    static constexpr std::size_t _off5 = 0x8;
    static constexpr std::size_t _off6 = 0x9;
    static constexpr std::size_t _off7 = 0xA;
    static constexpr std::size_t _off8 = 0xB;
    static constexpr std::size_t _off9 = 0xC;
    static constexpr std::size_t _off10 = 0xD;
    static constexpr std::size_t _off11 = 0xE;
    static constexpr std::size_t _off12 = 0xF;
    static constexpr std::size_t _off13 = 0x10;
    static constexpr std::size_t _off14 = 0x11;
    static constexpr std::size_t _off15 = 0x12;
    static constexpr std::size_t _off16 = 0x14;
    static constexpr std::size_t _off17 = 0x18;
    static constexpr std::size_t _off18 = 0x1C;
    static constexpr std::size_t _off19 = 0x20;
    static constexpr std::size_t _off20 = 0x24;
    static constexpr std::size_t _off21 = 0x28;
    static constexpr std::size_t _off22 = 0x2C;
    static constexpr std::size_t _off23 = 0x30;
    static constexpr std::size_t _off24 = 0x34;
    static constexpr std::size_t _off25 = 0x40;
    static constexpr std::size_t _off26 = 0x44;
    static constexpr std::size_t _off27 = 0x48;
    static constexpr std::size_t _off28 = 0x4C;
    static constexpr std::size_t _off29 = 0x50;
    static constexpr std::size_t _off30 = 0x51;
    static constexpr std::size_t _off31 = 0x52;
    static constexpr std::size_t _off32 = 0x54;
    std::unique_ptr<Int32Array> weights_;
    std::uint32_t last_data1_ptr_ = 0;
    std::unique_ptr<::fruityprime::memory::AIData1> data1_cache_; 
};

class AIData1 : public MemoryClass {
public:
    AIData1(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AIData1(Buffer& buffer, std::uint32_t address);
    ~AIData1() override;

    [[nodiscard]] std::int32_t Func24Id() const { return read_i32(0x0); }
    void Func24Id(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t func24_id() const { return Func24Id(); }
    void func24_id(std::int32_t value) { Func24Id(value); }
    [[nodiscard]] std::int32_t Data1Count() const { return read_i32(0x4); }
    void Data1Count(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data1_count() const { return Data1Count(); }
    void data1_count(std::int32_t value) { Data1Count(value); }
    [[nodiscard]] std::uint32_t Data1Ptr() const { return read_pointer(0x8); }
    void Data1Ptr(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data1_ptr() const { return Data1Ptr(); }
    void data1_ptr(std::uint32_t value) { Data1Ptr(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::AIData1>* Data1() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AIData1>* Data1() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AIData1>* data1() noexcept { return Data1(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AIData1>* data1() const noexcept { return Data1(); }
    [[nodiscard]] std::int32_t Data2Count() const { return read_i32(0xC); }
    void Data2Count(std::int32_t value) { write_i32(0xC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data2_count() const { return Data2Count(); }
    void data2_count(std::int32_t value) { Data2Count(value); }
    [[nodiscard]] std::uint32_t Data2Ptr() const { return read_pointer(0x10); }
    void Data2Ptr(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data2_ptr() const { return Data2Ptr(); }
    void data2_ptr(std::uint32_t value) { Data2Ptr(value); }
    [[nodiscard]] StructArray<::fruityprime::memory::AIData2>* Data2() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::AIData2>* Data2() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::AIData2>* data2() noexcept { return Data2(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::AIData2>* data2() const noexcept { return Data2(); }
    [[nodiscard]] std::int32_t Data3Count() const { return read_i32(0x14); }
    void Data3Count(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data3_count() const { return Data3Count(); }
    void data3_count(std::int32_t value) { Data3Count(value); }
    [[nodiscard]] std::uint32_t Data3a() const { return read_pointer(0x18); }
    void Data3a(std::uint32_t value) { write_pointer(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data3a() const { return Data3a(); }
    void data3a(std::uint32_t value) { Data3a(value); }
    [[nodiscard]] std::int32_t Data3bCount() const { return read_i32(0x1C); }
    void Data3bCount(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data3b_count() const { return Data3bCount(); }
    void data3b_count(std::int32_t value) { Data3bCount(value); }
    [[nodiscard]] std::uint32_t Data3b() const { return read_pointer(0x20); }
    void Data3b(std::uint32_t value) { write_pointer(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data3b() const { return Data3b(); }
    void data3b(std::uint32_t value) { Data3b(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
    static constexpr std::size_t _off6 = 0x18;
    static constexpr std::size_t _off7 = 0x1C;
    static constexpr std::size_t _off8 = 0x20;
    std::unique_ptr<StructArray<::fruityprime::memory::AIData1>> data1_;
    std::unique_ptr<StructArray<::fruityprime::memory::AIData2>> data2_;
};

class AIData2 : public MemoryClass {
public:
    AIData2(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AIData2(Buffer& buffer, std::uint32_t address);
    ~AIData2() override;

    [[nodiscard]] std::int32_t FuncIdx() const { return read_i32(0x0); }
    void FuncIdx(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t func_idx() const { return FuncIdx(); }
    void func_idx(std::int32_t value) { FuncIdx(value); }
    [[nodiscard]] std::int32_t Data4Count() const { return read_i32(0x4); }
    void Data4Count(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data4_count() const { return Data4Count(); }
    void data4_count(std::int32_t value) { Data4Count(value); }
    [[nodiscard]] std::uint32_t Data4() const { return read_pointer(0x8); }
    void Data4(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data4() const { return Data4(); }
    void data4(std::uint32_t value) { Data4(value); }
    [[nodiscard]] std::int32_t Data1SelectIdx() const { return read_i32(0xC); }
    void Data1SelectIdx(std::int32_t value) { write_i32(0xC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t data1_select_idx() const { return Data1SelectIdx(); }
    void data1_select_idx(std::int32_t value) { Data1SelectIdx(value); }
    [[nodiscard]] std::int32_t Weight() const { return read_i32(0x10); }
    void Weight(std::int32_t value) { write_i32(0x10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t weight() const { return Weight(); }
    void weight(std::int32_t value) { Weight(value); }
    [[nodiscard]] std::uint32_t Data5() const { return read_pointer(0x14); }
    void Data5(std::uint32_t value) { write_pointer(0x14, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t data5() const { return Data5(); }
    void data5(std::uint32_t value) { Data5(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
};

class AIAggro : public MemoryClass {
public:
    AIAggro(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AIAggro(Buffer& buffer, std::uint32_t address);
    ~AIAggro() override;

    [[nodiscard]] std::uint16_t Bits1() const { return read_u16(0x0); }
    void Bits1(std::uint16_t value) { write_u16(0x0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t bits1() const { return Bits1(); }
    void bits1(std::uint16_t value) { Bits1(value); }
    [[nodiscard]] std::uint16_t Bits2() const { return read_u16(0x2); }
    void Bits2(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t bits2() const { return Bits2(); }
    void bits2(std::uint16_t value) { Bits2(value); }
    [[nodiscard]] std::uint16_t Staleness() const { return read_u16(0x4); }
    void Staleness(std::uint16_t value) { write_u16(0x4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t staleness() const { return Staleness(); }
    void staleness(std::uint16_t value) { Staleness(value); }
    [[nodiscard]] std::uint16_t Expiration() const { return read_u16(0x6); }
    void Expiration(std::uint16_t value) { write_u16(0x6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t expiration() const { return Expiration(); }
    void expiration(std::uint16_t value) { Expiration(value); }
    [[nodiscard]] std::uint32_t Player1() const { return read_pointer(0x8); }
    void Player1(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t player1() const { return Player1(); }
    void player1(std::uint32_t value) { Player1(value); }
    [[nodiscard]] std::uint32_t Player2() const { return read_pointer(0xC); }
    void Player2(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t player2() const { return Player2(); }
    void player2(std::uint32_t value) { Player2(value); }
    [[nodiscard]] std::int32_t Slot1() const noexcept { return slot1_; }
    void Slot1(std::int32_t value) noexcept { slot1_ = value; }
    [[nodiscard]] std::int32_t slot1() const noexcept { return Slot1(); }
    void slot1(std::int32_t value) noexcept { Slot1(value); }
    [[nodiscard]] std::int32_t Slot2() const noexcept { return slot2_; }
    void Slot2(std::int32_t value) noexcept { slot2_ = value; }
    [[nodiscard]] std::int32_t slot2() const noexcept { return Slot2(); }
    void slot2(std::int32_t value) noexcept { Slot2(value); }
    [[nodiscard]] std::uint8_t VarA2() const;
    [[nodiscard]] std::uint8_t var_a2() const { return VarA2(); }
    [[nodiscard]] std::uint8_t VarA9() const;
    [[nodiscard]] std::uint8_t var_a9() const { return VarA9(); }
    [[nodiscard]] std::uint8_t VarA3() const;
    [[nodiscard]] std::uint8_t var_a3() const { return VarA3(); }
    [[nodiscard]] std::uint8_t VarA4() const;
    [[nodiscard]] std::uint8_t var_a4() const { return VarA4(); }
    [[nodiscard]] std::uint8_t VarA10() const;
    [[nodiscard]] std::uint8_t var_a10() const { return VarA10(); }
    [[nodiscard]] std::uint16_t VarA7() const;
    [[nodiscard]] std::uint16_t var_a7() const { return VarA7(); }
    void UpdateSlots(const std::array<::fruityprime::memory::CPlayer*, 4>& players);
    void update_slots(const std::array<::fruityprime::memory::CPlayer*, 4>& players) { UpdateSlots(players); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x2;
    static constexpr std::size_t _off2 = 0x4;
    static constexpr std::size_t _off3 = 0x6;
    static constexpr std::size_t _off4 = 0x8;
    static constexpr std::size_t _off5 = 0xC;
    std::int32_t slot1_ = -1;
    std::int32_t slot2_ = -1;
};

class CBeamProjectile : public CEntity {
public:
    CBeamProjectile(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CBeamProjectile(Buffer& buffer, std::uint32_t address);
    ~CBeamProjectile() override;

    [[nodiscard]] formats::BeamType Beam() const { return static_cast<formats::BeamType>(read_u8(0x18)); }
    void Beam(formats::BeamType value) { write_u8(0x18, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::BeamType beam() const { return Beam(); }
    void beam(formats::BeamType value) { Beam(value); }
    [[nodiscard]] formats::BeamType BeamKind() const { return static_cast<formats::BeamType>(read_u8(0x19)); }
    void BeamKind(formats::BeamType value) { write_u8(0x19, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::BeamType beam_kind() const { return BeamKind(); }
    void beam_kind(formats::BeamType value) { BeamKind(value); }
    [[nodiscard]] std::uint8_t DrawFuncId() const { return read_u8(0x1A); }
    void DrawFuncId(std::uint8_t value) { write_u8(0x1A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t draw_func_id() const { return DrawFuncId(); }
    void draw_func_id(std::uint8_t value) { DrawFuncId(value); }
    [[nodiscard]] std::uint8_t ColEffect() const { return read_u8(0x1B); }
    void ColEffect(std::uint8_t value) { write_u8(0x1B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t col_effect() const { return ColEffect(); }
    void col_effect(std::uint8_t value) { ColEffect(value); }
    [[nodiscard]] std::uint8_t SplashDmgType() const { return read_u8(0x1C); }
    void SplashDmgType(std::uint8_t value) { write_u8(0x1C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t splash_dmg_type() const { return SplashDmgType(); }
    void splash_dmg_type(std::uint8_t value) { SplashDmgType(value); }
    [[nodiscard]] std::uint8_t DmgDirType() const { return read_u8(0x1D); }
    void DmgDirType(std::uint8_t value) { write_u8(0x1D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t dmg_dir_type() const { return DmgDirType(); }
    void dmg_dir_type(std::uint8_t value) { DmgDirType(value); }
    [[nodiscard]] std::uint8_t Field1E() const { return read_u8(0x1E); }
    void Field1E(std::uint8_t value) { write_u8(0x1E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field1_e() const { return Field1E(); }
    void field1_e(std::uint8_t value) { Field1E(value); }
    [[nodiscard]] std::uint8_t SpeedInterpolation() const { return read_u8(0x1F); }
    void SpeedInterpolation(std::uint8_t value) { write_u8(0x1F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t speed_interpolation() const { return SpeedInterpolation(); }
    void speed_interpolation(std::uint8_t value) { SpeedInterpolation(value); }
    [[nodiscard]] std::uint8_t Afflictions() const { return read_u8(0x20); }
    void Afflictions(std::uint8_t value) { write_u8(0x20, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t afflictions() const { return Afflictions(); }
    void afflictions(std::uint8_t value) { Afflictions(value); }
    [[nodiscard]] std::uint8_t ListCount() const { return read_u8(0x21); }
    void ListCount(std::uint8_t value) { write_u8(0x21, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t list_count() const { return ListCount(); }
    void list_count(std::uint8_t value) { ListCount(value); }
    [[nodiscard]] formats::BeamFlags Flags() const { return static_cast<formats::BeamFlags>(read_u16(0x22)); }
    void Flags(formats::BeamFlags value) { write_u16(0x22, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] formats::BeamFlags flags() const { return Flags(); }
    void flags(formats::BeamFlags value) { Flags(value); }
    [[nodiscard]] std::uint16_t Color() const { return read_u16(0x24); }
    void Color(std::uint16_t value) { write_u16(0x24, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t color() const { return Color(); }
    void color(std::uint16_t value) { Color(value); }
    [[nodiscard]] std::uint16_t Damage() const { return read_u16(0x26); }
    void Damage(std::uint16_t value) { write_u16(0x26, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t damage() const { return Damage(); }
    void damage(std::uint16_t value) { Damage(value); }
    [[nodiscard]] std::uint16_t HeadshotDamage() const { return read_u16(0x28); }
    void HeadshotDamage(std::uint16_t value) { write_u16(0x28, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t headshot_damage() const { return HeadshotDamage(); }
    void headshot_damage(std::uint16_t value) { HeadshotDamage(value); }
    [[nodiscard]] std::uint16_t SplashDamage() const { return read_u16(0x2A); }
    void SplashDamage(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t splash_damage() const { return SplashDamage(); }
    void splash_damage(std::uint16_t value) { SplashDamage(value); }
    [[nodiscard]] std::uint16_t Lifespan() const { return read_u16(0x2C); }
    void Lifespan(std::uint16_t value) { write_u16(0x2C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t lifespan() const { return Lifespan(); }
    void lifespan(std::uint16_t value) { Lifespan(value); }
    [[nodiscard]] std::uint16_t Age() const { return read_u16(0x2E); }
    void Age(std::uint16_t value) { write_u16(0x2E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t age() const { return Age(); }
    void age(std::uint16_t value) { Age(value); }
    [[nodiscard]] std::uint16_t SpeedDecayTime() const { return read_u16(0x30); }
    void SpeedDecayTime(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t speed_decay_time() const { return SpeedDecayTime(); }
    void speed_decay_time(std::uint16_t value) { SpeedDecayTime(value); }
    [[nodiscard]] std::uint16_t Field32() const { return read_u16(0x32); }
    void Field32(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field32() const { return Field32(); }
    void field32(std::uint16_t value) { Field32(value); }
    [[nodiscard]] formats::Vector3 PastPos0() const { return read_vec3(0x34); }
    void PastPos0(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 past_pos0() const { return PastPos0(); }
    void past_pos0(formats::Vector3 value) { PastPos0(value); }
    [[nodiscard]] formats::Vector3 PastPos1() const { return read_vec3(0x40); }
    void PastPos1(formats::Vector3 value) { write_vec3(0x40, value); }
    [[nodiscard]] formats::Vector3 past_pos1() const { return PastPos1(); }
    void past_pos1(formats::Vector3 value) { PastPos1(value); }
    [[nodiscard]] formats::Vector3 PastPos2() const { return read_vec3(0x4C); }
    void PastPos2(formats::Vector3 value) { write_vec3(0x4C, value); }
    [[nodiscard]] formats::Vector3 past_pos2() const { return PastPos2(); }
    void past_pos2(formats::Vector3 value) { PastPos2(value); }
    [[nodiscard]] formats::Vector3 PastPos3() const { return read_vec3(0x58); }
    void PastPos3(formats::Vector3 value) { write_vec3(0x58, value); }
    [[nodiscard]] formats::Vector3 past_pos3() const { return PastPos3(); }
    void past_pos3(formats::Vector3 value) { PastPos3(value); }
    [[nodiscard]] formats::Vector3 PastPos4() const { return read_vec3(0x64); }
    void PastPos4(formats::Vector3 value) { write_vec3(0x64, value); }
    [[nodiscard]] formats::Vector3 past_pos4() const { return PastPos4(); }
    void past_pos4(formats::Vector3 value) { PastPos4(value); }
    [[nodiscard]] formats::Vector3 Vec1() const { return read_vec3(0x70); }
    void Vec1(formats::Vector3 value) { write_vec3(0x70, value); }
    [[nodiscard]] formats::Vector3 vec1() const { return Vec1(); }
    void vec1(formats::Vector3 value) { Vec1(value); }
    [[nodiscard]] formats::Vector3 Field7C() const { return read_vec3(0x7C); }
    void Field7C(formats::Vector3 value) { write_vec3(0x7C, value); }
    [[nodiscard]] formats::Vector3 field7_c() const { return Field7C(); }
    void field7_c(formats::Vector3 value) { Field7C(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x88); }
    void Vec2(formats::Vector3 value) { write_vec3(0x88, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 CylBack() const { return read_vec3(0x94); }
    void CylBack(formats::Vector3 value) { write_vec3(0x94, value); }
    [[nodiscard]] formats::Vector3 cyl_back() const { return CylBack(); }
    void cyl_back(formats::Vector3 value) { CylBack(value); }
    [[nodiscard]] formats::Vector3 CylFront() const { return read_vec3(0xA0); }
    void CylFront(formats::Vector3 value) { write_vec3(0xA0, value); }
    [[nodiscard]] formats::Vector3 cyl_front() const { return CylFront(); }
    void cyl_front(formats::Vector3 value) { CylFront(value); }
    [[nodiscard]] formats::Vector3 SpawnPos() const { return read_vec3(0xAC); }
    void SpawnPos(formats::Vector3 value) { write_vec3(0xAC, value); }
    [[nodiscard]] formats::Vector3 spawn_pos() const { return SpawnPos(); }
    void spawn_pos(formats::Vector3 value) { SpawnPos(value); }
    [[nodiscard]] std::int32_t Speed() const { return read_i32(0xB8); }
    void Speed(std::int32_t value) { write_i32(0xB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t speed() const { return Speed(); }
    void speed(std::int32_t value) { Speed(value); }
    [[nodiscard]] std::int32_t InitialSpeed() const { return read_i32(0xBC); }
    void InitialSpeed(std::int32_t value) { write_i32(0xBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t initial_speed() const { return InitialSpeed(); }
    void initial_speed(std::int32_t value) { InitialSpeed(value); }
    [[nodiscard]] std::int32_t FinalSpeed() const { return read_i32(0xC0); }
    void FinalSpeed(std::int32_t value) { write_i32(0xC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t final_speed() const { return FinalSpeed(); }
    void final_speed(std::int32_t value) { FinalSpeed(value); }
    [[nodiscard]] formats::Vector3 Velocity() const { return read_vec3(0xC4); }
    void Velocity(formats::Vector3 value) { write_vec3(0xC4, value); }
    [[nodiscard]] formats::Vector3 velocity() const { return Velocity(); }
    void velocity(formats::Vector3 value) { Velocity(value); }
    [[nodiscard]] formats::Vector3 Acceleration() const { return read_vec3(0xD0); }
    void Acceleration(formats::Vector3 value) { write_vec3(0xD0, value); }
    [[nodiscard]] formats::Vector3 acceleration() const { return Acceleration(); }
    void acceleration(formats::Vector3 value) { Acceleration(value); }
    [[nodiscard]] std::int32_t Homing() const { return read_i32(0xDC); }
    void Homing(std::int32_t value) { write_i32(0xDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t homing() const { return Homing(); }
    void homing(std::int32_t value) { Homing(value); }
    [[nodiscard]] std::int32_t FieldE0() const { return read_i32(0xE0); }
    void FieldE0(std::int32_t value) { write_i32(0xE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e0() const { return FieldE0(); }
    void field_e0(std::int32_t value) { FieldE0(value); }
    [[nodiscard]] std::int32_t MaxDist() const { return read_i32(0xE4); }
    void MaxDist(std::int32_t value) { write_i32(0xE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t max_dist() const { return MaxDist(); }
    void max_dist(std::int32_t value) { MaxDist(value); }
    [[nodiscard]] std::int32_t FieldE8() const { return read_i32(0xE8); }
    void FieldE8(std::int32_t value) { write_i32(0xE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e8() const { return FieldE8(); }
    void field_e8(std::int32_t value) { FieldE8(value); }
    [[nodiscard]] std::int32_t FieldEC() const { return read_i32(0xEC); }
    void FieldEC(std::int32_t value) { write_i32(0xEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c() const { return FieldEC(); }
    void field_e_c(std::int32_t value) { FieldEC(value); }
    [[nodiscard]] std::int32_t FieldF0() const { return read_i32(0xF0); }
    void FieldF0(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0() const { return FieldF0(); }
    void field_f0(std::int32_t value) { FieldF0(value); }
    [[nodiscard]] std::int32_t Scale() const { return read_i32(0xF4); }
    void Scale(std::int32_t value) { write_i32(0xF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scale() const { return Scale(); }
    void scale(std::int32_t value) { Scale(value); }
    [[nodiscard]] std::uint32_t Owner() const { return read_pointer(0xF8); }
    void Owner(std::uint32_t value) { write_pointer(0xF8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t owner() const { return Owner(); }
    void owner(std::uint32_t value) { Owner(value); }
    [[nodiscard]] std::uint32_t RicochetWeapon() const { return read_pointer(0xFC); }
    void RicochetWeapon(std::uint32_t value) { write_pointer(0xFC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ricochet_weapon() const { return RicochetWeapon(); }
    void ricochet_weapon(std::uint32_t value) { RicochetWeapon(value); }
    [[nodiscard]] std::uint32_t ListHead() const { return read_pointer(0x100); }
    void ListHead(std::uint32_t value) { write_pointer(0x100, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t list_head() const { return ListHead(); }
    void list_head(std::uint32_t value) { ListHead(value); }
    [[nodiscard]] std::uint32_t Target() const { return read_pointer(0x104); }
    void Target(std::uint32_t value) { write_pointer(0x104, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t target() const { return Target(); }
    void target(std::uint32_t value) { Target(value); }
    [[nodiscard]] ::fruityprime::memory::CModel& Model() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CModel& Model() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CModel& model() noexcept { return Model(); }
    [[nodiscard]] const ::fruityprime::memory::CModel& model() const noexcept { return Model(); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x150); }
    void NodeRef(std::uint32_t value) { write_pointer(0x150, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ::fruityprime::memory::SfxParameters& SfxParameters() noexcept;
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& SfxParameters() const noexcept;
    [[nodiscard]] ::fruityprime::memory::SfxParameters& sfx_parameters() noexcept { return SfxParameters(); }
    [[nodiscard]] const ::fruityprime::memory::SfxParameters& sfx_parameters() const noexcept { return SfxParameters(); }

private:
    static constexpr std::size_t _off0 = 0x18;
    static constexpr std::size_t _off1 = 0x19;
    static constexpr std::size_t _off2 = 0x1A;
    static constexpr std::size_t _off3 = 0x1B;
    static constexpr std::size_t _off4 = 0x1C;
    static constexpr std::size_t _off5 = 0x1D;
    static constexpr std::size_t _off6 = 0x1E;
    static constexpr std::size_t _off7 = 0x1F;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x21;
    static constexpr std::size_t _off10 = 0x22;
    static constexpr std::size_t _off11 = 0x24;
    static constexpr std::size_t _off12 = 0x26;
    static constexpr std::size_t _off13 = 0x28;
    static constexpr std::size_t _off14 = 0x2A;
    static constexpr std::size_t _off15 = 0x2C;
    static constexpr std::size_t _off16 = 0x2E;
    static constexpr std::size_t _off17 = 0x30;
    static constexpr std::size_t _off18 = 0x32;
    static constexpr std::size_t _off19 = 0x34;
    static constexpr std::size_t _off20 = 0x40;
    static constexpr std::size_t _off21 = 0x4C;
    static constexpr std::size_t _off22 = 0x58;
    static constexpr std::size_t _off23 = 0x64;
    static constexpr std::size_t _off24 = 0x70;
    static constexpr std::size_t _off25 = 0x7C;
    static constexpr std::size_t _off26 = 0x88;
    static constexpr std::size_t _off27 = 0x94;
    static constexpr std::size_t _off28 = 0xA0;
    static constexpr std::size_t _off29 = 0xAC;
    static constexpr std::size_t _off30 = 0xB8;
    static constexpr std::size_t _off31 = 0xBC;
    static constexpr std::size_t _off32 = 0xC0;
    static constexpr std::size_t _off33 = 0xC4;
    static constexpr std::size_t _off34 = 0xD0;
    static constexpr std::size_t _off35 = 0xDC;
    static constexpr std::size_t _off36 = 0xE0;
    static constexpr std::size_t _off37 = 0xE4;
    static constexpr std::size_t _off38 = 0xE8;
    static constexpr std::size_t _off39 = 0xEC;
    static constexpr std::size_t _off40 = 0xF0;
    static constexpr std::size_t _off41 = 0xF4;
    static constexpr std::size_t _off42 = 0xF8;
    static constexpr std::size_t _off43 = 0xFC;
    static constexpr std::size_t _off44 = 0x100;
    static constexpr std::size_t _off45 = 0x104;
    static constexpr std::size_t _off46 = 0x108;
    static constexpr std::size_t _off47 = 0x150;
    static constexpr std::size_t _off48 = 0x154;
    std::unique_ptr<::fruityprime::memory::CModel> model_;
    std::unique_ptr<::fruityprime::memory::SfxParameters> sfx_parameters_;
};

class CModel : public MemoryClass {
public:
    CModel(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CModel(Buffer& buffer, std::uint32_t address);
    ~CModel() override;

    [[nodiscard]] std::uint32_t Union() const { return read_pointer(0x0); }
    void Union(std::uint32_t value) { write_pointer(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t union_() const { return Union(); }
    void union_(std::uint32_t value) { Union(value); }
    [[nodiscard]] std::uint32_t MaterialAnimation() const { return read_pointer(0x4); }
    void MaterialAnimation(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t material_animation() const { return MaterialAnimation(); }
    void material_animation(std::uint32_t value) { MaterialAnimation(value); }
    [[nodiscard]] std::uint32_t TextureAnimation() const { return read_pointer(0x8); }
    void TextureAnimation(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t texture_animation() const { return TextureAnimation(); }
    void texture_animation(std::uint32_t value) { TextureAnimation(value); }
    [[nodiscard]] std::uint32_t TexcoordAnimation() const { return read_pointer(0xC); }
    void TexcoordAnimation(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t texcoord_animation() const { return TexcoordAnimation(); }
    void texcoord_animation(std::uint32_t value) { TexcoordAnimation(value); }
    [[nodiscard]] ::fruityprime::memory::CNodeAnimation& NodeAnimation() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CNodeAnimation& NodeAnimation() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CNodeAnimation& node_animation() noexcept { return NodeAnimation(); }
    [[nodiscard]] const ::fruityprime::memory::CNodeAnimation& node_animation() const noexcept { return NodeAnimation(); }
    [[nodiscard]] std::uint32_t Animation() const { return read_pointer(0x24); }
    void Animation(std::uint32_t value) { write_pointer(0x24, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t animation() const { return Animation(); }
    void animation(std::uint32_t value) { Animation(value); }
    [[nodiscard]] std::uint16_t Animid() const { return read_u16(0x28); }
    void Animid(std::uint16_t value) { write_u16(0x28, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t animid() const { return Animid(); }
    void animid(std::uint16_t value) { Animid(value); }
    [[nodiscard]] std::uint16_t Field2A() const { return read_u16(0x2A); }
    void Field2A(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_a() const { return Field2A(); }
    void field2_a(std::uint16_t value) { Field2A(value); }
    [[nodiscard]] std::uint16_t RoomAnimId() const { return read_u16(0x2C); }
    void RoomAnimId(std::uint16_t value) { write_u16(0x2C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t room_anim_id() const { return RoomAnimId(); }
    void room_anim_id(std::uint16_t value) { RoomAnimId(value); }
    [[nodiscard]] std::uint16_t Field2E() const { return read_u16(0x2E); }
    void Field2E(std::uint16_t value) { write_u16(0x2E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_e() const { return Field2E(); }
    void field2_e(std::uint16_t value) { Field2E(value); }
    [[nodiscard]] std::uint16_t AnimFrame() const { return read_u16(0x30); }
    void AnimFrame(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t anim_frame() const { return AnimFrame(); }
    void anim_frame(std::uint16_t value) { AnimFrame(value); }
    [[nodiscard]] std::uint16_t Field32() const { return read_u16(0x32); }
    void Field32(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field32() const { return Field32(); }
    void field32(std::uint16_t value) { Field32(value); }
    [[nodiscard]] std::uint16_t InitialFrame() const { return read_u16(0x34); }
    void InitialFrame(std::uint16_t value) { write_u16(0x34, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t initial_frame() const { return InitialFrame(); }
    void initial_frame(std::uint16_t value) { InitialFrame(value); }
    [[nodiscard]] std::uint16_t Field36() const { return read_u16(0x36); }
    void Field36(std::uint16_t value) { write_u16(0x36, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field36() const { return Field36(); }
    void field36(std::uint16_t value) { Field36(value); }
    [[nodiscard]] std::uint16_t AnimationFlags() const { return read_u16(0x38); }
    void AnimationFlags(std::uint16_t value) { write_u16(0x38, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t animation_flags() const { return AnimationFlags(); }
    void animation_flags(std::uint16_t value) { AnimationFlags(value); }
    [[nodiscard]] std::uint16_t Field3A() const { return read_u16(0x3A); }
    void Field3A(std::uint16_t value) { write_u16(0x3A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field3_a() const { return Field3A(); }
    void field3_a(std::uint16_t value) { Field3A(value); }
    [[nodiscard]] std::uint16_t Field3C() const { return read_u16(0x3C); }
    void Field3C(std::uint16_t value) { write_u16(0x3C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field3_c() const { return Field3C(); }
    void field3_c(std::uint16_t value) { Field3C(value); }
    [[nodiscard]] std::uint16_t Field3E() const { return read_u16(0x3E); }
    void Field3E(std::uint16_t value) { write_u16(0x3E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field3_e() const { return Field3E(); }
    void field3_e(std::uint16_t value) { Field3E(value); }
    [[nodiscard]] std::uint16_t NodeAnimIgnoreRoot() const { return read_u16(0x40); }
    void NodeAnimIgnoreRoot(std::uint16_t value) { write_u16(0x40, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t node_anim_ignore_root() const { return NodeAnimIgnoreRoot(); }
    void node_anim_ignore_root(std::uint16_t value) { NodeAnimIgnoreRoot(value); }
    [[nodiscard]] std::uint8_t NodeAnimDelta() const { return read_u8(0x42); }
    void NodeAnimDelta(std::uint8_t value) { write_u8(0x42, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t node_anim_delta() const { return NodeAnimDelta(); }
    void node_anim_delta(std::uint8_t value) { NodeAnimDelta(value); }
    [[nodiscard]] std::uint8_t MatAnimDelta() const { return read_u8(0x43); }
    void MatAnimDelta(std::uint8_t value) { write_u8(0x43, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t mat_anim_delta() const { return MatAnimDelta(); }
    void mat_anim_delta(std::uint8_t value) { MatAnimDelta(value); }
    [[nodiscard]] std::uint8_t TexAnimDelta() const { return read_u8(0x44); }
    void TexAnimDelta(std::uint8_t value) { write_u8(0x44, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t tex_anim_delta() const { return TexAnimDelta(); }
    void tex_anim_delta(std::uint8_t value) { TexAnimDelta(value); }
    [[nodiscard]] std::uint8_t UvAnimDelta() const { return read_u8(0x45); }
    void UvAnimDelta(std::uint8_t value) { write_u8(0x45, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t uv_anim_delta() const { return UvAnimDelta(); }
    void uv_anim_delta(std::uint8_t value) { UvAnimDelta(value); }
    [[nodiscard]] std::uint16_t Field46() const { return read_u16(0x46); }
    void Field46(std::uint16_t value) { write_u16(0x46, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field46() const { return Field46(); }
    void field46(std::uint16_t value) { Field46(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x24;
    static constexpr std::size_t _off6 = 0x28;
    static constexpr std::size_t _off7 = 0x2A;
    static constexpr std::size_t _off8 = 0x2C;
    static constexpr std::size_t _off9 = 0x2E;
    static constexpr std::size_t _off10 = 0x30;
    static constexpr std::size_t _off11 = 0x32;
    static constexpr std::size_t _off12 = 0x34;
    static constexpr std::size_t _off13 = 0x36;
    static constexpr std::size_t _off14 = 0x38;
    static constexpr std::size_t _off15 = 0x3A;
    static constexpr std::size_t _off16 = 0x3C;
    static constexpr std::size_t _off17 = 0x3E;
    static constexpr std::size_t _off18 = 0x40;
    static constexpr std::size_t _off19 = 0x42;
    static constexpr std::size_t _off20 = 0x43;
    static constexpr std::size_t _off21 = 0x44;
    static constexpr std::size_t _off22 = 0x45;
    static constexpr std::size_t _off23 = 0x46;
    std::unique_ptr<::fruityprime::memory::CNodeAnimation> node_animation_;
};

class CNodeAnimation : public MemoryClass {
public:
    CNodeAnimation(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CNodeAnimation(Buffer& buffer, std::uint32_t address);
    ~CNodeAnimation() override;

    [[nodiscard]] std::uint32_t NodeAnimation() const { return read_pointer(0x0); }
    void NodeAnimation(std::uint32_t value) { write_pointer(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_animation() const { return NodeAnimation(); }
    void node_animation(std::uint32_t value) { NodeAnimation(value); }
    [[nodiscard]] std::int32_t NodeAnimFrame() const { return read_i32(0x4); }
    void NodeAnimFrame(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t node_anim_frame() const { return NodeAnimFrame(); }
    void node_anim_frame(std::int32_t value) { NodeAnimFrame(value); }
    [[nodiscard]] std::int32_t NumNodes() const { return read_i32(0x8); }
    void NumNodes(std::int32_t value) { write_i32(0x8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t num_nodes() const { return NumNodes(); }
    void num_nodes(std::int32_t value) { NumNodes(value); }
    [[nodiscard]] std::uint32_t Nodes() const { return read_pointer(0xC); }
    void Nodes(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodes() const { return Nodes(); }
    void nodes(std::uint32_t value) { Nodes(value); }
    [[nodiscard]] std::uint32_t InitialMtx() const { return read_pointer(0x10); }
    void InitialMtx(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t initial_mtx() const { return InitialMtx(); }
    void initial_mtx(std::uint32_t value) { InitialMtx(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
};

class EntityCollision : public MemoryClass {
public:
    EntityCollision(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    EntityCollision(Buffer& buffer, std::uint32_t address);
    ~EntityCollision() override;

    [[nodiscard]] formats::Matrix4x3 Matrix() const { return read_mtx43(0x0); }
    void Matrix(formats::Matrix4x3 value) { write_mtx43(0x0, value); }
    [[nodiscard]] formats::Matrix4x3 matrix() const { return Matrix(); }
    void matrix(formats::Matrix4x3 value) { Matrix(value); }
    [[nodiscard]] formats::Matrix4x3 Inverse1() const { return read_mtx43(0x30); }
    void Inverse1(formats::Matrix4x3 value) { write_mtx43(0x30, value); }
    [[nodiscard]] formats::Matrix4x3 inverse1() const { return Inverse1(); }
    void inverse1(formats::Matrix4x3 value) { Inverse1(value); }
    [[nodiscard]] formats::Matrix4x3 Inverse2() const { return read_mtx43(0x60); }
    void Inverse2(formats::Matrix4x3 value) { write_mtx43(0x60, value); }
    [[nodiscard]] formats::Matrix4x3 inverse2() const { return Inverse2(); }
    void inverse2(formats::Matrix4x3 value) { Inverse2(value); }
    [[nodiscard]] formats::Vector3 Average() const { return read_vec3(0x90); }
    void Average(formats::Vector3 value) { write_vec3(0x90, value); }
    [[nodiscard]] formats::Vector3 average() const { return Average(); }
    void average(formats::Vector3 value) { Average(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0x9C); }
    void Vec2(formats::Vector3 value) { write_vec3(0x9C, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] std::int32_t MaxDist() const { return read_i32(0xA8); }
    void MaxDist(std::int32_t value) { write_i32(0xA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t max_dist() const { return MaxDist(); }
    void max_dist(std::int32_t value) { MaxDist(value); }
    [[nodiscard]] std::uint32_t EntPtr() const { return read_pointer(0xAC); }
    void EntPtr(std::uint32_t value) { write_pointer(0xAC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ent_ptr() const { return EntPtr(); }
    void ent_ptr(std::uint32_t value) { EntPtr(value); }
    [[nodiscard]] std::uint32_t Collision() const { return read_pointer(0xB0); }
    void Collision(std::uint32_t value) { write_pointer(0xB0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t collision() const { return Collision(); }
    void collision(std::uint32_t value) { Collision(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x30;
    static constexpr std::size_t _off2 = 0x60;
    static constexpr std::size_t _off3 = 0x90;
    static constexpr std::size_t _off4 = 0x9C;
    static constexpr std::size_t _off5 = 0xA8;
    static constexpr std::size_t _off6 = 0xAC;
    static constexpr std::size_t _off7 = 0xB0;
};

class EquipInfoPtr : public MemoryClass {
public:
    EquipInfoPtr(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    EquipInfoPtr(Buffer& buffer, std::uint32_t address);
    ~EquipInfoPtr() override;

    [[nodiscard]] formats::EquipFlags Flags() const { return static_cast<formats::EquipFlags>(read_u8(0x0)); }
    void Flags(formats::EquipFlags value) { write_u8(0x0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::EquipFlags flags() const { return Flags(); }
    void flags(formats::EquipFlags value) { Flags(value); }
    [[nodiscard]] std::uint8_t Count() const { return read_u8(0x1); }
    void Count(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t count() const { return Count(); }
    void count(std::uint8_t value) { Count(value); }
    [[nodiscard]] std::uint16_t Padding2() const { return read_u16(0x2); }
    void Padding2(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding2() const { return Padding2(); }
    void padding2(std::uint16_t value) { Padding2(value); }
    [[nodiscard]] std::uint32_t Beams() const { return read_pointer(0x4); }
    void Beams(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t beams() const { return Beams(); }
    void beams(std::uint32_t value) { Beams(value); }
    [[nodiscard]] std::uint32_t WeaponInfo() const { return read_pointer(0x8); }
    void WeaponInfo(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t weapon_info() const { return WeaponInfo(); }
    void weapon_info(std::uint32_t value) { WeaponInfo(value); }
    [[nodiscard]] std::uint32_t AmmoPtr() const { return read_pointer(0xC); }
    void AmmoPtr(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ammo_ptr() const { return AmmoPtr(); }
    void ammo_ptr(std::uint32_t value) { AmmoPtr(value); }
    [[nodiscard]] std::uint16_t ChargeLevel() const { return read_u16(0x10); }
    void ChargeLevel(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t charge_level() const { return ChargeLevel(); }
    void charge_level(std::uint16_t value) { ChargeLevel(value); }
    [[nodiscard]] std::uint16_t SmokeLevel() const { return read_u16(0x12); }
    void SmokeLevel(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t smoke_level() const { return SmokeLevel(); }
    void smoke_level(std::uint16_t value) { SmokeLevel(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
    static constexpr std::size_t _off3 = 0x4;
    static constexpr std::size_t _off4 = 0x8;
    static constexpr std::size_t _off5 = 0xC;
    static constexpr std::size_t _off6 = 0x10;
    static constexpr std::size_t _off7 = 0x12;
};

class SfxParameters : public MemoryClass {
public:
    SfxParameters(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    SfxParameters(Buffer& buffer, std::uint32_t address);
    ~SfxParameters() override;

    [[nodiscard]] std::int8_t Volume() const { return read_i8(0x0); }
    void Volume(std::int8_t value) { write_i8(0x0, static_cast<std::int8_t>(value)); }
    [[nodiscard]] std::int8_t volume() const { return Volume(); }
    void volume(std::int8_t value) { Volume(value); }
    [[nodiscard]] std::uint8_t PanX() const { return read_u8(0x1); }
    void PanX(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t pan_x() const { return PanX(); }
    void pan_x(std::uint8_t value) { PanX(value); }
    [[nodiscard]] std::uint16_t PanZ() const { return read_u16(0x2); }
    void PanZ(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t pan_z() const { return PanZ(); }
    void pan_z(std::uint16_t value) { PanZ(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
};

class CollisionVolume : public MemoryClass {
public:
    CollisionVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CollisionVolume(Buffer& buffer, std::uint32_t address);
    ~CollisionVolume() override;

    [[nodiscard]] formats::VolumeType Type() const { return static_cast<formats::VolumeType>(read_u32(0x0)); }
    void Type(formats::VolumeType value) { write_u32(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] formats::VolumeType type() const { return Type(); }
    void type(formats::VolumeType value) { Type(value); }
    [[nodiscard]] formats::Vector3 BoxVec1() const { return read_vec3(0x4); }
    void BoxVec1(formats::Vector3 value) { write_vec3(0x4, value); }
    [[nodiscard]] formats::Vector3 box_vec1() const { return BoxVec1(); }
    void box_vec1(formats::Vector3 value) { BoxVec1(value); }
    [[nodiscard]] formats::Vector3 BoxVec2() const { return read_vec3(0x10); }
    void BoxVec2(formats::Vector3 value) { write_vec3(0x10, value); }
    [[nodiscard]] formats::Vector3 box_vec2() const { return BoxVec2(); }
    void box_vec2(formats::Vector3 value) { BoxVec2(value); }
    [[nodiscard]] formats::Vector3 BoxVec3() const { return read_vec3(0x1C); }
    void BoxVec3(formats::Vector3 value) { write_vec3(0x1C, value); }
    [[nodiscard]] formats::Vector3 box_vec3() const { return BoxVec3(); }
    void box_vec3(formats::Vector3 value) { BoxVec3(value); }
    [[nodiscard]] formats::Vector3 BoxPos() const { return read_vec3(0x28); }
    void BoxPos(formats::Vector3 value) { write_vec3(0x28, value); }
    [[nodiscard]] formats::Vector3 box_pos() const { return BoxPos(); }
    void box_pos(formats::Vector3 value) { BoxPos(value); }
    [[nodiscard]] formats::Vector3 BoxDot() const { return read_vec3(0x34); }
    void BoxDot(formats::Vector3 value) { write_vec3(0x34, value); }
    [[nodiscard]] formats::Vector3 box_dot() const { return BoxDot(); }
    void box_dot(formats::Vector3 value) { BoxDot(value); }
    [[nodiscard]] formats::Vector3 CylinderVec() const { return read_vec3(0x4); }
    void CylinderVec(formats::Vector3 value) { write_vec3(0x4, value); }
    [[nodiscard]] formats::Vector3 cylinder_vec() const { return CylinderVec(); }
    void cylinder_vec(formats::Vector3 value) { CylinderVec(value); }
    [[nodiscard]] formats::Vector3 CylinderCenter() const { return read_vec3(0x10); }
    void CylinderCenter(formats::Vector3 value) { write_vec3(0x10, value); }
    [[nodiscard]] formats::Vector3 cylinder_center() const { return CylinderCenter(); }
    void cylinder_center(formats::Vector3 value) { CylinderCenter(value); }
    [[nodiscard]] std::int32_t CylinderRadius() const { return read_i32(0x1C); }
    void CylinderRadius(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t cylinder_radius() const { return CylinderRadius(); }
    void cylinder_radius(std::int32_t value) { CylinderRadius(value); }
    [[nodiscard]] std::int32_t CylinderDot() const { return read_i32(0x20); }
    void CylinderDot(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t cylinder_dot() const { return CylinderDot(); }
    void cylinder_dot(std::int32_t value) { CylinderDot(value); }
    [[nodiscard]] formats::Vector3 SphereCenter() const { return read_vec3(0x4); }
    void SphereCenter(formats::Vector3 value) { write_vec3(0x4, value); }
    [[nodiscard]] formats::Vector3 sphere_center() const { return SphereCenter(); }
    void sphere_center(formats::Vector3 value) { SphereCenter(value); }
    [[nodiscard]] std::int32_t SphereRadius() const { return read_i32(0x10); }
    void SphereRadius(std::int32_t value) { write_i32(0x10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t sphere_radius() const { return SphereRadius(); }
    void sphere_radius(std::int32_t value) { SphereRadius(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _box0 = 0x4;
    static constexpr std::size_t _box1 = 0x10;
    static constexpr std::size_t _box2 = 0x1C;
    static constexpr std::size_t _box3 = 0x28;
    static constexpr std::size_t _box4 = 0x34;
    static constexpr std::size_t _cyl0 = 0x4;
    static constexpr std::size_t _cyl1 = 0x10;
    static constexpr std::size_t _cyl2 = 0x1C;
    static constexpr std::size_t _cyl3 = 0x20;
    static constexpr std::size_t _sph0 = 0x4;
    static constexpr std::size_t _sph1 = 0x10;
};

class Light : public MemoryClass {
public:
    Light(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    Light(Buffer& buffer, std::uint32_t address);
    ~Light() override;

    [[nodiscard]] formats::Vector3 Dir() const { return read_vec3(0x0); }
    void Dir(formats::Vector3 value) { write_vec3(0x0, value); }
    [[nodiscard]] formats::Vector3 dir() const { return Dir(); }
    void dir(formats::Vector3 value) { Dir(value); }
    [[nodiscard]] formats::ColorRgb Color() const { return read_color3(0xC); }
    void Color(formats::ColorRgb value) { write_color3(0xC, value); }
    [[nodiscard]] formats::ColorRgb color() const { return Color(); }
    void color(formats::ColorRgb value) { Color(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0xC;
};

class LightInfo : public MemoryClass {
public:
    LightInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    LightInfo(Buffer& buffer, std::uint32_t address);
    ~LightInfo() override;

    [[nodiscard]] ::fruityprime::memory::Light& Light1() noexcept;
    [[nodiscard]] const ::fruityprime::memory::Light& Light1() const noexcept;
    [[nodiscard]] ::fruityprime::memory::Light& light1() noexcept { return Light1(); }
    [[nodiscard]] const ::fruityprime::memory::Light& light1() const noexcept { return Light1(); }
    [[nodiscard]] std::uint8_t PaddingF() const { return read_u8(0xF); }
    void PaddingF(std::uint8_t value) { write_u8(0xF, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding_f() const { return PaddingF(); }
    void padding_f(std::uint8_t value) { PaddingF(value); }
    [[nodiscard]] ::fruityprime::memory::Light& Light2() noexcept;
    [[nodiscard]] const ::fruityprime::memory::Light& Light2() const noexcept;
    [[nodiscard]] ::fruityprime::memory::Light& light2() noexcept { return Light2(); }
    [[nodiscard]] const ::fruityprime::memory::Light& light2() const noexcept { return Light2(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0xF;
    static constexpr std::size_t _off2 = 0x10;
    std::unique_ptr<::fruityprime::memory::Light> light1_;
    std::unique_ptr<::fruityprime::memory::Light> light2_;
};

class CameraInfo : public MemoryClass {
public:
    CameraInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CameraInfo(Buffer& buffer, std::uint32_t address);
    ~CameraInfo() override;

    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x0); }
    void Pos(formats::Vector3 value) { write_vec3(0x0, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 Vec2() const { return read_vec3(0xC); }
    void Vec2(formats::Vector3 value) { write_vec3(0xC, value); }
    [[nodiscard]] formats::Vector3 vec2() const { return Vec2(); }
    void vec2(formats::Vector3 value) { Vec2(value); }
    [[nodiscard]] formats::Vector3 Vec3() const { return read_vec3(0x18); }
    void Vec3(formats::Vector3 value) { write_vec3(0x18, value); }
    [[nodiscard]] formats::Vector3 vec3() const { return Vec3(); }
    void vec3(formats::Vector3 value) { Vec3(value); }
    [[nodiscard]] formats::Vector3 Vec4() const { return read_vec3(0x24); }
    void Vec4(formats::Vector3 value) { write_vec3(0x24, value); }
    [[nodiscard]] formats::Vector3 vec4() const { return Vec4(); }
    void vec4(formats::Vector3 value) { Vec4(value); }
    [[nodiscard]] formats::Vector3 Vec5() const { return read_vec3(0x30); }
    void Vec5(formats::Vector3 value) { write_vec3(0x30, value); }
    [[nodiscard]] formats::Vector3 vec5() const { return Vec5(); }
    void vec5(formats::Vector3 value) { Vec5(value); }
    [[nodiscard]] formats::Vector3 Vec6() const { return read_vec3(0x3C); }
    void Vec6(formats::Vector3 value) { write_vec3(0x3C, value); }
    [[nodiscard]] formats::Vector3 vec6() const { return Vec6(); }
    void vec6(formats::Vector3 value) { Vec6(value); }
    [[nodiscard]] std::int32_t Field48() const { return read_i32(0x48); }
    void Field48(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field48() const { return Field48(); }
    void field48(std::int32_t value) { Field48(value); }
    [[nodiscard]] std::int32_t Field4C() const { return read_i32(0x4C); }
    void Field4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_c() const { return Field4C(); }
    void field4_c(std::int32_t value) { Field4C(value); }
    [[nodiscard]] std::int32_t Field50() const { return read_i32(0x50); }
    void Field50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field50() const { return Field50(); }
    void field50(std::int32_t value) { Field50(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] formats::Matrix4x3 Mtx1() const { return read_mtx43(0x58); }
    void Mtx1(formats::Matrix4x3 value) { write_mtx43(0x58, value); }
    [[nodiscard]] formats::Matrix4x3 mtx1() const { return Mtx1(); }
    void mtx1(formats::Matrix4x3 value) { Mtx1(value); }
    [[nodiscard]] std::int32_t Field88() const { return read_i32(0x88); }
    void Field88(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field88() const { return Field88(); }
    void field88(std::int32_t value) { Field88(value); }
    [[nodiscard]] std::int32_t Field8C() const { return read_i32(0x8C); }
    void Field8C(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field8_c() const { return Field8C(); }
    void field8_c(std::int32_t value) { Field8C(value); }
    [[nodiscard]] std::int32_t Field90() const { return read_i32(0x90); }
    void Field90(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field90() const { return Field90(); }
    void field90(std::int32_t value) { Field90(value); }
    [[nodiscard]] std::int32_t Field94() const { return read_i32(0x94); }
    void Field94(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field94() const { return Field94(); }
    void field94(std::int32_t value) { Field94(value); }
    [[nodiscard]] std::int32_t Field98() const { return read_i32(0x98); }
    void Field98(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field98() const { return Field98(); }
    void field98(std::int32_t value) { Field98(value); }
    [[nodiscard]] std::int32_t Field9C() const { return read_i32(0x9C); }
    void Field9C(std::int32_t value) { write_i32(0x9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c() const { return Field9C(); }
    void field9_c(std::int32_t value) { Field9C(value); }
    [[nodiscard]] std::int32_t FieldA0() const { return read_i32(0xA0); }
    void FieldA0(std::int32_t value) { write_i32(0xA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a0() const { return FieldA0(); }
    void field_a0(std::int32_t value) { FieldA0(value); }
    [[nodiscard]] std::int32_t FieldA4() const { return read_i32(0xA4); }
    void FieldA4(std::int32_t value) { write_i32(0xA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a4() const { return FieldA4(); }
    void field_a4(std::int32_t value) { FieldA4(value); }
    [[nodiscard]] std::int32_t FieldA8() const { return read_i32(0xA8); }
    void FieldA8(std::int32_t value) { write_i32(0xA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a8() const { return FieldA8(); }
    void field_a8(std::int32_t value) { FieldA8(value); }
    [[nodiscard]] std::int32_t FieldAC() const { return read_i32(0xAC); }
    void FieldAC(std::int32_t value) { write_i32(0xAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_a_c() const { return FieldAC(); }
    void field_a_c(std::int32_t value) { FieldAC(value); }
    [[nodiscard]] std::int32_t FieldB0() const { return read_i32(0xB0); }
    void FieldB0(std::int32_t value) { write_i32(0xB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b0() const { return FieldB0(); }
    void field_b0(std::int32_t value) { FieldB0(value); }
    [[nodiscard]] std::int32_t FieldB4() const { return read_i32(0xB4); }
    void FieldB4(std::int32_t value) { write_i32(0xB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_b4() const { return FieldB4(); }
    void field_b4(std::int32_t value) { FieldB4(value); }
    [[nodiscard]] formats::Matrix4x3 Mtx2() const { return read_mtx43(0xB8); }
    void Mtx2(formats::Matrix4x3 value) { write_mtx43(0xB8, value); }
    [[nodiscard]] formats::Matrix4x3 mtx2() const { return Mtx2(); }
    void mtx2(formats::Matrix4x3 value) { Mtx2(value); }
    [[nodiscard]] std::int32_t Shake() const { return read_i32(0xE8); }
    void Shake(std::int32_t value) { write_i32(0xE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t shake() const { return Shake(); }
    void shake(std::int32_t value) { Shake(value); }
    [[nodiscard]] std::int32_t Fov() const { return read_i32(0xEC); }
    void Fov(std::int32_t value) { write_i32(0xEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t fov() const { return Fov(); }
    void fov(std::int32_t value) { Fov(value); }
    [[nodiscard]] std::int32_t FieldF0() const { return read_i32(0xF0); }
    void FieldF0(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0() const { return FieldF0(); }
    void field_f0(std::int32_t value) { FieldF0(value); }
    [[nodiscard]] std::int32_t FieldF4() const { return read_i32(0xF4); }
    void FieldF4(std::int32_t value) { write_i32(0xF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f4() const { return FieldF4(); }
    void field_f4(std::int32_t value) { FieldF4(value); }
    [[nodiscard]] std::int32_t NearLr() const { return read_i32(0xF8); }
    void NearLr(std::int32_t value) { write_i32(0xF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t near_lr() const { return NearLr(); }
    void near_lr(std::int32_t value) { NearLr(value); }
    [[nodiscard]] std::int32_t NearTb() const { return read_i32(0xFC); }
    void NearTb(std::int32_t value) { write_i32(0xFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t near_tb() const { return NearTb(); }
    void near_tb(std::int32_t value) { NearTb(value); }
    [[nodiscard]] std::int32_t NearDist() const { return read_i32(0x100); }
    void NearDist(std::int32_t value) { write_i32(0x100, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t near_dist() const { return NearDist(); }
    void near_dist(std::int32_t value) { NearDist(value); }
    [[nodiscard]] std::int32_t FarDist() const { return read_i32(0x104); }
    void FarDist(std::int32_t value) { write_i32(0x104, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t far_dist() const { return FarDist(); }
    void far_dist(std::int32_t value) { FarDist(value); }
    [[nodiscard]] std::int32_t ViewportY2() const { return read_i32(0x108); }
    void ViewportY2(std::int32_t value) { write_i32(0x108, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t viewport_y2() const { return ViewportY2(); }
    void viewport_y2(std::int32_t value) { ViewportY2(value); }
    [[nodiscard]] std::int32_t ViewportY1() const { return read_i32(0x10C); }
    void ViewportY1(std::int32_t value) { write_i32(0x10C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t viewport_y1() const { return ViewportY1(); }
    void viewport_y1(std::int32_t value) { ViewportY1(value); }
    [[nodiscard]] std::int32_t ViewportX1() const { return read_i32(0x110); }
    void ViewportX1(std::int32_t value) { write_i32(0x110, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t viewport_x1() const { return ViewportX1(); }
    void viewport_x1(std::int32_t value) { ViewportX1(value); }
    [[nodiscard]] std::int32_t ViewportX2() const { return read_i32(0x114); }
    void ViewportX2(std::int32_t value) { write_i32(0x114, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t viewport_x2() const { return ViewportX2(); }
    void viewport_x2(std::int32_t value) { ViewportX2(value); }
    [[nodiscard]] std::uint32_t CurNode() const { return read_pointer(0x118); }
    void CurNode(std::uint32_t value) { write_pointer(0x118, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t cur_node() const { return CurNode(); }
    void cur_node(std::uint32_t value) { CurNode(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0xC;
    static constexpr std::size_t _off2 = 0x18;
    static constexpr std::size_t _off3 = 0x24;
    static constexpr std::size_t _off4 = 0x30;
    static constexpr std::size_t _off5 = 0x3C;
    static constexpr std::size_t _off6 = 0x48;
    static constexpr std::size_t _off7 = 0x4C;
    static constexpr std::size_t _off8 = 0x50;
    static constexpr std::size_t _off9 = 0x54;
    static constexpr std::size_t _off10 = 0x58;
    static constexpr std::size_t _off11 = 0x88;
    static constexpr std::size_t _off12 = 0x8C;
    static constexpr std::size_t _off13 = 0x90;
    static constexpr std::size_t _off14 = 0x94;
    static constexpr std::size_t _off15 = 0x98;
    static constexpr std::size_t _off16 = 0x9C;
    static constexpr std::size_t _off17 = 0xA0;
    static constexpr std::size_t _off18 = 0xA4;
    static constexpr std::size_t _off19 = 0xA8;
    static constexpr std::size_t _off20 = 0xAC;
    static constexpr std::size_t _off21 = 0xB0;
    static constexpr std::size_t _off22 = 0xB4;
    static constexpr std::size_t _off23 = 0xB8;
    static constexpr std::size_t _off24 = 0xE8;
    static constexpr std::size_t _off25 = 0xEC;
    static constexpr std::size_t _off26 = 0xF0;
    static constexpr std::size_t _off27 = 0xF4;
    static constexpr std::size_t _off28 = 0xF8;
    static constexpr std::size_t _off29 = 0xFC;
    static constexpr std::size_t _off30 = 0x100;
    static constexpr std::size_t _off31 = 0x104;
    static constexpr std::size_t _off32 = 0x108;
    static constexpr std::size_t _off33 = 0x10C;
    static constexpr std::size_t _off34 = 0x110;
    static constexpr std::size_t _off35 = 0x114;
    static constexpr std::size_t _off36 = 0x118;
};

class PlayerControls : public MemoryClass {
public:
    PlayerControls(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    PlayerControls(Buffer& buffer, std::uint32_t address);
    ~PlayerControls() override;

    [[nodiscard]] std::int32_t Field0() const { return read_i32(0x0); }
    void Field0(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field0() const { return Field0(); }
    void field0(std::int32_t value) { Field0(value); }
    [[nodiscard]] std::uint16_t Field4() const { return read_u16(0x4); }
    void Field4(std::uint16_t value) { write_u16(0x4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4() const { return Field4(); }
    void field4(std::uint16_t value) { Field4(value); }
    [[nodiscard]] std::uint16_t Field6() const { return read_u16(0x6); }
    void Field6(std::uint16_t value) { write_u16(0x6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field6() const { return Field6(); }
    void field6(std::uint16_t value) { Field6(value); }
    [[nodiscard]] std::uint16_t Field8() const { return read_u16(0x8); }
    void Field8(std::uint16_t value) { write_u16(0x8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field8() const { return Field8(); }
    void field8(std::uint16_t value) { Field8(value); }
    [[nodiscard]] std::uint16_t FieldA() const { return read_u16(0xA); }
    void FieldA(std::uint16_t value) { write_u16(0xA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_a() const { return FieldA(); }
    void field_a(std::uint16_t value) { FieldA(value); }
    [[nodiscard]] std::uint16_t FieldC() const { return read_u16(0xC); }
    void FieldC(std::uint16_t value) { write_u16(0xC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_c() const { return FieldC(); }
    void field_c(std::uint16_t value) { FieldC(value); }
    [[nodiscard]] std::uint16_t FieldE() const { return read_u16(0xE); }
    void FieldE(std::uint16_t value) { write_u16(0xE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_e() const { return FieldE(); }
    void field_e(std::uint16_t value) { FieldE(value); }
    [[nodiscard]] std::uint16_t Field10() const { return read_u16(0x10); }
    void Field10(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field10() const { return Field10(); }
    void field10(std::uint16_t value) { Field10(value); }
    [[nodiscard]] std::uint16_t Field12() const { return read_u16(0x12); }
    void Field12(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field12() const { return Field12(); }
    void field12(std::uint16_t value) { Field12(value); }
    [[nodiscard]] std::int32_t Field14() const { return read_i32(0x14); }
    void Field14(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14() const { return Field14(); }
    void field14(std::int32_t value) { Field14(value); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::uint16_t Field24() const { return read_u16(0x24); }
    void Field24(std::uint16_t value) { write_u16(0x24, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field24() const { return Field24(); }
    void field24(std::uint16_t value) { Field24(value); }
    [[nodiscard]] std::uint16_t Field26() const { return read_u16(0x26); }
    void Field26(std::uint16_t value) { write_u16(0x26, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field26() const { return Field26(); }
    void field26(std::uint16_t value) { Field26(value); }
    [[nodiscard]] std::uint16_t Field28() const { return read_u16(0x28); }
    void Field28(std::uint16_t value) { write_u16(0x28, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field28() const { return Field28(); }
    void field28(std::uint16_t value) { Field28(value); }
    [[nodiscard]] std::uint16_t Field2A() const { return read_u16(0x2A); }
    void Field2A(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_a() const { return Field2A(); }
    void field2_a(std::uint16_t value) { Field2A(value); }
    [[nodiscard]] std::uint16_t Field2C() const { return read_u16(0x2C); }
    void Field2C(std::uint16_t value) { write_u16(0x2C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_c() const { return Field2C(); }
    void field2_c(std::uint16_t value) { Field2C(value); }
    [[nodiscard]] std::uint16_t Field2E() const { return read_u16(0x2E); }
    void Field2E(std::uint16_t value) { write_u16(0x2E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_e() const { return Field2E(); }
    void field2_e(std::uint16_t value) { Field2E(value); }
    [[nodiscard]] std::uint16_t Field30() const { return read_u16(0x30); }
    void Field30(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field30() const { return Field30(); }
    void field30(std::uint16_t value) { Field30(value); }
    [[nodiscard]] std::uint16_t Field32() const { return read_u16(0x32); }
    void Field32(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field32() const { return Field32(); }
    void field32(std::uint16_t value) { Field32(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] std::int32_t Field40() const { return read_i32(0x40); }
    void Field40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40() const { return Field40(); }
    void field40(std::int32_t value) { Field40(value); }
    [[nodiscard]] std::int32_t Field44() const { return read_i32(0x44); }
    void Field44(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field44() const { return Field44(); }
    void field44(std::int32_t value) { Field44(value); }
    [[nodiscard]] std::int32_t Field48() const { return read_i32(0x48); }
    void Field48(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field48() const { return Field48(); }
    void field48(std::int32_t value) { Field48(value); }
    [[nodiscard]] std::int32_t Field4C() const { return read_i32(0x4C); }
    void Field4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_c() const { return Field4C(); }
    void field4_c(std::int32_t value) { Field4C(value); }
    [[nodiscard]] std::int32_t Field50() const { return read_i32(0x50); }
    void Field50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field50() const { return Field50(); }
    void field50(std::int32_t value) { Field50(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] std::int32_t Field58() const { return read_i32(0x58); }
    void Field58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field58() const { return Field58(); }
    void field58(std::int32_t value) { Field58(value); }
    [[nodiscard]] std::int32_t Field5C() const { return read_i32(0x5C); }
    void Field5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field5_c() const { return Field5C(); }
    void field5_c(std::int32_t value) { Field5C(value); }
    [[nodiscard]] std::int32_t Field60() const { return read_i32(0x60); }
    void Field60(std::int32_t value) { write_i32(0x60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field60() const { return Field60(); }
    void field60(std::int32_t value) { Field60(value); }
    [[nodiscard]] std::int32_t Field64() const { return read_i32(0x64); }
    void Field64(std::int32_t value) { write_i32(0x64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field64() const { return Field64(); }
    void field64(std::int32_t value) { Field64(value); }
    [[nodiscard]] std::int32_t Field68() const { return read_i32(0x68); }
    void Field68(std::int32_t value) { write_i32(0x68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field68() const { return Field68(); }
    void field68(std::int32_t value) { Field68(value); }
    [[nodiscard]] std::int32_t Field6C() const { return read_i32(0x6C); }
    void Field6C(std::int32_t value) { write_i32(0x6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_c() const { return Field6C(); }
    void field6_c(std::int32_t value) { Field6C(value); }
    [[nodiscard]] std::int32_t Field70() const { return read_i32(0x70); }
    void Field70(std::int32_t value) { write_i32(0x70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field70() const { return Field70(); }
    void field70(std::int32_t value) { Field70(value); }
    [[nodiscard]] std::int32_t Field74() const { return read_i32(0x74); }
    void Field74(std::int32_t value) { write_i32(0x74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field74() const { return Field74(); }
    void field74(std::int32_t value) { Field74(value); }
    [[nodiscard]] std::int32_t Field78() const { return read_i32(0x78); }
    void Field78(std::int32_t value) { write_i32(0x78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field78() const { return Field78(); }
    void field78(std::int32_t value) { Field78(value); }
    [[nodiscard]] std::int32_t Field7C() const { return read_i32(0x7C); }
    void Field7C(std::int32_t value) { write_i32(0x7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c() const { return Field7C(); }
    void field7_c(std::int32_t value) { Field7C(value); }
    [[nodiscard]] std::int32_t Field80() const { return read_i32(0x80); }
    void Field80(std::int32_t value) { write_i32(0x80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field80() const { return Field80(); }
    void field80(std::int32_t value) { Field80(value); }
    [[nodiscard]] std::int32_t Field84() const { return read_i32(0x84); }
    void Field84(std::int32_t value) { write_i32(0x84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field84() const { return Field84(); }
    void field84(std::int32_t value) { Field84(value); }
    [[nodiscard]] std::int32_t Field88() const { return read_i32(0x88); }
    void Field88(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field88() const { return Field88(); }
    void field88(std::int32_t value) { Field88(value); }
    [[nodiscard]] std::int32_t Field8C() const { return read_i32(0x8C); }
    void Field8C(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field8_c() const { return Field8C(); }
    void field8_c(std::int32_t value) { Field8C(value); }
    [[nodiscard]] std::int32_t Field90() const { return read_i32(0x90); }
    void Field90(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field90() const { return Field90(); }
    void field90(std::int32_t value) { Field90(value); }
    [[nodiscard]] std::int32_t Field94() const { return read_i32(0x94); }
    void Field94(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field94() const { return Field94(); }
    void field94(std::int32_t value) { Field94(value); }
    [[nodiscard]] std::int32_t Field98() const { return read_i32(0x98); }
    void Field98(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field98() const { return Field98(); }
    void field98(std::int32_t value) { Field98(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x6;
    static constexpr std::size_t _off3 = 0x8;
    static constexpr std::size_t _off4 = 0xA;
    static constexpr std::size_t _off5 = 0xC;
    static constexpr std::size_t _off6 = 0xE;
    static constexpr std::size_t _off7 = 0x10;
    static constexpr std::size_t _off8 = 0x12;
    static constexpr std::size_t _off9 = 0x14;
    static constexpr std::size_t _off10 = 0x18;
    static constexpr std::size_t _off11 = 0x1C;
    static constexpr std::size_t _off12 = 0x20;
    static constexpr std::size_t _off13 = 0x24;
    static constexpr std::size_t _off14 = 0x26;
    static constexpr std::size_t _off15 = 0x28;
    static constexpr std::size_t _off16 = 0x2A;
    static constexpr std::size_t _off17 = 0x2C;
    static constexpr std::size_t _off18 = 0x2E;
    static constexpr std::size_t _off19 = 0x30;
    static constexpr std::size_t _off20 = 0x32;
    static constexpr std::size_t _off21 = 0x34;
    static constexpr std::size_t _off22 = 0x38;
    static constexpr std::size_t _off23 = 0x3C;
    static constexpr std::size_t _off24 = 0x40;
    static constexpr std::size_t _off25 = 0x44;
    static constexpr std::size_t _off26 = 0x48;
    static constexpr std::size_t _off27 = 0x4C;
    static constexpr std::size_t _off28 = 0x50;
    static constexpr std::size_t _off29 = 0x54;
    static constexpr std::size_t _off30 = 0x58;
    static constexpr std::size_t _off31 = 0x5C;
    static constexpr std::size_t _off32 = 0x60;
    static constexpr std::size_t _off33 = 0x64;
    static constexpr std::size_t _off34 = 0x68;
    static constexpr std::size_t _off35 = 0x6C;
    static constexpr std::size_t _off36 = 0x70;
    static constexpr std::size_t _off37 = 0x74;
    static constexpr std::size_t _off38 = 0x78;
    static constexpr std::size_t _off39 = 0x7C;
    static constexpr std::size_t _off40 = 0x80;
    static constexpr std::size_t _off41 = 0x84;
    static constexpr std::size_t _off42 = 0x88;
    static constexpr std::size_t _off43 = 0x8C;
    static constexpr std::size_t _off44 = 0x90;
    static constexpr std::size_t _off45 = 0x94;
    static constexpr std::size_t _off46 = 0x98;
};

class PlayerInput : public MemoryClass {
public:
    PlayerInput(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    PlayerInput(Buffer& buffer, std::uint32_t address);
    ~PlayerInput() override;

    [[nodiscard]] std::uint16_t Field0() const { return read_u16(0x0); }
    void Field0(std::uint16_t value) { write_u16(0x0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field0() const { return Field0(); }
    void field0(std::uint16_t value) { Field0(value); }
    [[nodiscard]] std::uint16_t Field2() const { return read_u16(0x2); }
    void Field2(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2() const { return Field2(); }
    void field2(std::uint16_t value) { Field2(value); }
    [[nodiscard]] std::uint16_t Field4() const { return read_u16(0x4); }
    void Field4(std::uint16_t value) { write_u16(0x4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field4() const { return Field4(); }
    void field4(std::uint16_t value) { Field4(value); }
    [[nodiscard]] std::uint16_t Field6() const { return read_u16(0x6); }
    void Field6(std::uint16_t value) { write_u16(0x6, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field6() const { return Field6(); }
    void field6(std::uint16_t value) { Field6(value); }
    [[nodiscard]] std::uint16_t Field8() const { return read_u16(0x8); }
    void Field8(std::uint16_t value) { write_u16(0x8, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field8() const { return Field8(); }
    void field8(std::uint16_t value) { Field8(value); }
    [[nodiscard]] std::uint16_t FieldA() const { return read_u16(0xA); }
    void FieldA(std::uint16_t value) { write_u16(0xA, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_a() const { return FieldA(); }
    void field_a(std::uint16_t value) { FieldA(value); }
    [[nodiscard]] std::int32_t FieldC() const { return read_i32(0xC); }
    void FieldC(std::int32_t value) { write_i32(0xC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c() const { return FieldC(); }
    void field_c(std::int32_t value) { FieldC(value); }
    [[nodiscard]] std::int32_t Field10() const { return read_i32(0x10); }
    void Field10(std::int32_t value) { write_i32(0x10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10() const { return Field10(); }
    void field10(std::int32_t value) { Field10(value); }
    [[nodiscard]] std::int32_t Field14() const { return read_i32(0x14); }
    void Field14(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14() const { return Field14(); }
    void field14(std::int32_t value) { Field14(value); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::uint16_t Field24() const { return read_u16(0x24); }
    void Field24(std::uint16_t value) { write_u16(0x24, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field24() const { return Field24(); }
    void field24(std::uint16_t value) { Field24(value); }
    [[nodiscard]] std::uint8_t Field26() const { return read_u8(0x26); }
    void Field26(std::uint8_t value) { write_u8(0x26, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field26() const { return Field26(); }
    void field26(std::uint8_t value) { Field26(value); }
    [[nodiscard]] std::uint8_t Field27() const { return read_u8(0x27); }
    void Field27(std::uint8_t value) { write_u8(0x27, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field27() const { return Field27(); }
    void field27(std::uint8_t value) { Field27(value); }
    [[nodiscard]] std::uint16_t Field28() const { return read_u16(0x28); }
    void Field28(std::uint16_t value) { write_u16(0x28, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field28() const { return Field28(); }
    void field28(std::uint16_t value) { Field28(value); }
    [[nodiscard]] std::uint16_t Field2A() const { return read_u16(0x2A); }
    void Field2A(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_a() const { return Field2A(); }
    void field2_a(std::uint16_t value) { Field2A(value); }
    [[nodiscard]] std::uint16_t Field2C() const { return read_u16(0x2C); }
    void Field2C(std::uint16_t value) { write_u16(0x2C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_c() const { return Field2C(); }
    void field2_c(std::uint16_t value) { Field2C(value); }
    [[nodiscard]] std::uint16_t Field2E() const { return read_u16(0x2E); }
    void Field2E(std::uint16_t value) { write_u16(0x2E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_e() const { return Field2E(); }
    void field2_e(std::uint16_t value) { Field2E(value); }
    [[nodiscard]] std::uint16_t Field30() const { return read_u16(0x30); }
    void Field30(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field30() const { return Field30(); }
    void field30(std::uint16_t value) { Field30(value); }
    [[nodiscard]] std::uint16_t Field32() const { return read_u16(0x32); }
    void Field32(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field32() const { return Field32(); }
    void field32(std::uint16_t value) { Field32(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] std::int32_t Field40() const { return read_i32(0x40); }
    void Field40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40() const { return Field40(); }
    void field40(std::int32_t value) { Field40(value); }
    [[nodiscard]] std::int32_t Field44() const { return read_i32(0x44); }
    void Field44(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field44() const { return Field44(); }
    void field44(std::int32_t value) { Field44(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x2;
    static constexpr std::size_t _off2 = 0x4;
    static constexpr std::size_t _off3 = 0x6;
    static constexpr std::size_t _off4 = 0x8;
    static constexpr std::size_t _off5 = 0xA;
    static constexpr std::size_t _off6 = 0xC;
    static constexpr std::size_t _off7 = 0x10;
    static constexpr std::size_t _off8 = 0x14;
    static constexpr std::size_t _off9 = 0x18;
    static constexpr std::size_t _off10 = 0x1C;
    static constexpr std::size_t _off11 = 0x20;
    static constexpr std::size_t _off12 = 0x24;
    static constexpr std::size_t _off13 = 0x26;
    static constexpr std::size_t _off14 = 0x27;
    static constexpr std::size_t _off15 = 0x28;
    static constexpr std::size_t _off16 = 0x2A;
    static constexpr std::size_t _off17 = 0x2C;
    static constexpr std::size_t _off18 = 0x2E;
    static constexpr std::size_t _off19 = 0x30;
    static constexpr std::size_t _off20 = 0x32;
    static constexpr std::size_t _off21 = 0x34;
    static constexpr std::size_t _off22 = 0x38;
    static constexpr std::size_t _off23 = 0x3C;
    static constexpr std::size_t _off24 = 0x40;
    static constexpr std::size_t _off25 = 0x44;
};

class CameraSequence : public MemoryClass {
public:
    CameraSequence(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CameraSequence(Buffer& buffer, std::uint32_t address);
    ~CameraSequence() override;

    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x0); }
    void Flags(std::uint8_t value) { write_u8(0x0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Version() const { return read_u8(0x1); }
    void Version(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t version() const { return Version(); }
    void version(std::uint8_t value) { Version(value); }
    [[nodiscard]] std::uint8_t Field2() const { return read_u8(0x2); }
    void Field2(std::uint8_t value) { write_u8(0x2, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field2() const { return Field2(); }
    void field2(std::uint8_t value) { Field2(value); }
    [[nodiscard]] std::uint8_t Field3() const { return read_u8(0x3); }
    void Field3(std::uint8_t value) { write_u8(0x3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field3() const { return Field3(); }
    void field3(std::uint8_t value) { Field3(value); }
    [[nodiscard]] std::int32_t KeyframeElapsed() const { return read_i32(0x4); }
    void KeyframeElapsed(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t keyframe_elapsed() const { return KeyframeElapsed(); }
    void keyframe_elapsed(std::int32_t value) { KeyframeElapsed(value); }
    [[nodiscard]] std::uint32_t Keyframes() const { return read_pointer(0x8); }
    void Keyframes(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t keyframes() const { return Keyframes(); }
    void keyframes(std::uint32_t value) { Keyframes(value); }
    [[nodiscard]] std::uint32_t NextKeyframe() const { return read_pointer(0xC); }
    void NextKeyframe(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t next_keyframe() const { return NextKeyframe(); }
    void next_keyframe(std::uint32_t value) { NextKeyframe(value); }
    [[nodiscard]] std::uint32_t CameraInfoPtr() const { return read_pointer(0x10); }
    void CameraInfoPtr(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t camera_info_ptr() const { return CameraInfoPtr(); }
    void camera_info_ptr(std::uint32_t value) { CameraInfoPtr(value); }
    [[nodiscard]] ::fruityprime::memory::CameraInfo& CameraInfo() noexcept;
    [[nodiscard]] const ::fruityprime::memory::CameraInfo& CameraInfo() const noexcept;
    [[nodiscard]] ::fruityprime::memory::CameraInfo& camera_info() noexcept { return CameraInfo(); }
    [[nodiscard]] const ::fruityprime::memory::CameraInfo& camera_info() const noexcept { return CameraInfo(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
    static constexpr std::size_t _off3 = 0x3;
    static constexpr std::size_t _off4 = 0x4;
    static constexpr std::size_t _off5 = 0x8;
    static constexpr std::size_t _off6 = 0xC;
    static constexpr std::size_t _off7 = 0x10;
    static constexpr std::size_t _off8 = 0x14;
    std::unique_ptr<::fruityprime::memory::CameraInfo> camera_info_;
};

class CameraSequenceKeyframe : public MemoryClass {
public:
    CameraSequenceKeyframe(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    CameraSequenceKeyframe(Buffer& buffer, std::uint32_t address);
    ~CameraSequenceKeyframe() override;

    [[nodiscard]] formats::Vector3 Pos() const { return read_vec3(0x0); }
    void Pos(formats::Vector3 value) { write_vec3(0x0, value); }
    [[nodiscard]] formats::Vector3 pos() const { return Pos(); }
    void pos(formats::Vector3 value) { Pos(value); }
    [[nodiscard]] formats::Vector3 ToTarget() const { return read_vec3(0xC); }
    void ToTarget(formats::Vector3 value) { write_vec3(0xC, value); }
    [[nodiscard]] formats::Vector3 to_target() const { return ToTarget(); }
    void to_target(formats::Vector3 value) { ToTarget(value); }
    [[nodiscard]] std::int32_t Roll() const { return read_i32(0x18); }
    void Roll(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t roll() const { return Roll(); }
    void roll(std::int32_t value) { Roll(value); }
    [[nodiscard]] std::int32_t Fov() const { return read_i32(0x1C); }
    void Fov(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t fov() const { return Fov(); }
    void fov(std::int32_t value) { Fov(value); }
    [[nodiscard]] std::int32_t MoveTime() const { return read_i32(0x20); }
    void MoveTime(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t move_time() const { return MoveTime(); }
    void move_time(std::int32_t value) { MoveTime(value); }
    [[nodiscard]] std::int32_t HoldTime() const { return read_i32(0x24); }
    void HoldTime(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t hold_time() const { return HoldTime(); }
    void hold_time(std::int32_t value) { HoldTime(value); }
    [[nodiscard]] std::int32_t FadeInTime() const { return read_i32(0x28); }
    void FadeInTime(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t fade_in_time() const { return FadeInTime(); }
    void fade_in_time(std::int32_t value) { FadeInTime(value); }
    [[nodiscard]] std::int32_t FadeOutTime() const { return read_i32(0x2C); }
    void FadeOutTime(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t fade_out_time() const { return FadeOutTime(); }
    void fade_out_time(std::int32_t value) { FadeOutTime(value); }
    [[nodiscard]] formats::FadeType FadeInType() const { return static_cast<formats::FadeType>(read_u8(0x30)); }
    void FadeInType(formats::FadeType value) { write_u8(0x30, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::FadeType fade_in_type() const { return FadeInType(); }
    void fade_in_type(formats::FadeType value) { FadeInType(value); }
    [[nodiscard]] formats::FadeType FadeOutType() const { return static_cast<formats::FadeType>(read_u8(0x31)); }
    void FadeOutType(formats::FadeType value) { write_u8(0x31, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] formats::FadeType fade_out_type() const { return FadeOutType(); }
    void fade_out_type(formats::FadeType value) { FadeOutType(value); }
    [[nodiscard]] std::uint8_t PrevFrameInfluence() const { return read_u8(0x32); }
    void PrevFrameInfluence(std::uint8_t value) { write_u8(0x32, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t prev_frame_influence() const { return PrevFrameInfluence(); }
    void prev_frame_influence(std::uint8_t value) { PrevFrameInfluence(value); }
    [[nodiscard]] std::uint8_t AfterFrameInfluence() const { return read_u8(0x33); }
    void AfterFrameInfluence(std::uint8_t value) { write_u8(0x33, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t after_frame_influence() const { return AfterFrameInfluence(); }
    void after_frame_influence(std::uint8_t value) { AfterFrameInfluence(value); }
    [[nodiscard]] std::uint8_t UseEntityTransform() const { return read_u8(0x34); }
    void UseEntityTransform(std::uint8_t value) { write_u8(0x34, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t use_entity_transform() const { return UseEntityTransform(); }
    void use_entity_transform(std::uint8_t value) { UseEntityTransform(value); }
    [[nodiscard]] std::uint8_t Padding35() const { return read_u8(0x35); }
    void Padding35(std::uint8_t value) { write_u8(0x35, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding35() const { return Padding35(); }
    void padding35(std::uint8_t value) { Padding35(value); }
    [[nodiscard]] std::uint16_t Padding36() const { return read_u16(0x36); }
    void Padding36(std::uint16_t value) { write_u16(0x36, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding36() const { return Padding36(); }
    void padding36(std::uint16_t value) { Padding36(value); }
    [[nodiscard]] std::uint32_t Ent1Ref() const { return read_pointer(0x38); }
    void Ent1Ref(std::uint32_t value) { write_pointer(0x38, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ent1_ref() const { return Ent1Ref(); }
    void ent1_ref(std::uint32_t value) { Ent1Ref(value); }
    [[nodiscard]] std::uint32_t Ent2Ref() const { return read_pointer(0x3C); }
    void Ent2Ref(std::uint32_t value) { write_pointer(0x3C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ent2_ref() const { return Ent2Ref(); }
    void ent2_ref(std::uint32_t value) { Ent2Ref(value); }
    [[nodiscard]] std::uint32_t EventTarget() const { return read_pointer(0x40); }
    void EventTarget(std::uint32_t value) { write_pointer(0x40, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t event_target() const { return EventTarget(); }
    void event_target(std::uint32_t value) { EventTarget(value); }
    [[nodiscard]] std::uint16_t EventId() const { return read_u16(0x44); }
    void EventId(std::uint16_t value) { write_u16(0x44, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t event_id() const { return EventId(); }
    void event_id(std::uint16_t value) { EventId(value); }
    [[nodiscard]] std::uint16_t EventParam() const { return read_u16(0x46); }
    void EventParam(std::uint16_t value) { write_u16(0x46, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t event_param() const { return EventParam(); }
    void event_param(std::uint16_t value) { EventParam(value); }
    [[nodiscard]] std::int32_t Easing() const { return read_i32(0x48); }
    void Easing(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t easing() const { return Easing(); }
    void easing(std::int32_t value) { Easing(value); }
    [[nodiscard]] std::int32_t Unused4C() const { return read_i32(0x4C); }
    void Unused4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused4_c() const { return Unused4C(); }
    void unused4_c(std::int32_t value) { Unused4C(value); }
    [[nodiscard]] std::int32_t Unused50() const { return read_i32(0x50); }
    void Unused50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t unused50() const { return Unused50(); }
    void unused50(std::int32_t value) { Unused50(value); }
    [[nodiscard]] std::uint32_t NodeRef() const { return read_pointer(0x54); }
    void NodeRef(std::uint32_t value) { write_pointer(0x54, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t node_ref() const { return NodeRef(); }
    void node_ref(std::uint32_t value) { NodeRef(value); }
    [[nodiscard]] ByteArray& NodeNameRest() noexcept;
    [[nodiscard]] const ByteArray& NodeNameRest() const noexcept;
    [[nodiscard]] ByteArray& node_name_rest() noexcept { return NodeNameRest(); }
    [[nodiscard]] const ByteArray& node_name_rest() const noexcept { return NodeNameRest(); }
    [[nodiscard]] std::uint32_t Next() const { return read_pointer(0x64); }
    void Next(std::uint32_t value) { write_pointer(0x64, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t next() const { return Next(); }
    void next(std::uint32_t value) { Next(value); }
    [[nodiscard]] std::uint32_t Prev() const { return read_pointer(0x68); }
    void Prev(std::uint32_t value) { write_pointer(0x68, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t prev() const { return Prev(); }
    void prev(std::uint32_t value) { Prev(value); }
    [[nodiscard]] std::uint8_t Index() const { return read_u8(0x6C); }
    void Index(std::uint8_t value) { write_u8(0x6C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t index() const { return Index(); }
    void index(std::uint8_t value) { Index(value); }
    [[nodiscard]] std::uint8_t Padding6D() const { return read_u8(0x6D); }
    void Padding6D(std::uint8_t value) { write_u8(0x6D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding6_d() const { return Padding6D(); }
    void padding6_d(std::uint8_t value) { Padding6D(value); }
    [[nodiscard]] std::uint16_t Padding6E() const { return read_u16(0x6E); }
    void Padding6E(std::uint16_t value) { write_u16(0x6E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding6_e() const { return Padding6E(); }
    void padding6_e(std::uint16_t value) { Padding6E(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0xC;
    static constexpr std::size_t _off2 = 0x18;
    static constexpr std::size_t _off3 = 0x1C;
    static constexpr std::size_t _off4 = 0x20;
    static constexpr std::size_t _off5 = 0x24;
    static constexpr std::size_t _off6 = 0x28;
    static constexpr std::size_t _off7 = 0x2C;
    static constexpr std::size_t _off8 = 0x30;
    static constexpr std::size_t _off9 = 0x31;
    static constexpr std::size_t _off10 = 0x32;
    static constexpr std::size_t _off11 = 0x33;
    static constexpr std::size_t _off12 = 0x34;
    static constexpr std::size_t _off13 = 0x35;
    static constexpr std::size_t _off14 = 0x36;
    static constexpr std::size_t _off15 = 0x38;
    static constexpr std::size_t _off16 = 0x3C;
    static constexpr std::size_t _off17 = 0x40;
    static constexpr std::size_t _off18 = 0x44;
    static constexpr std::size_t _off19 = 0x46;
    static constexpr std::size_t _off20 = 0x48;
    static constexpr std::size_t _off21 = 0x4C;
    static constexpr std::size_t _off22 = 0x50;
    static constexpr std::size_t _off23 = 0x54;
    static constexpr std::size_t _off24 = 0x58;
    static constexpr std::size_t _off25 = 0x64;
    static constexpr std::size_t _off26 = 0x68;
    static constexpr std::size_t _off27 = 0x6C;
    static constexpr std::size_t _off28 = 0x6D;
    static constexpr std::size_t _off29 = 0x6E;
    std::unique_ptr<ByteArray> node_name_rest_;
};

class GameState : public MemoryClass {
public:
    GameState(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    GameState(Buffer& buffer, std::uint32_t address);
    ~GameState() override;

    [[nodiscard]] ::fruityprime::GameMode GameMode() const { return static_cast<::fruityprime::GameMode>(read_u8(0x0)); }
    void GameMode(::fruityprime::GameMode value) { write_u8(0x0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] ::fruityprime::GameMode game_mode() const { return GameMode(); }
    void game_mode(::fruityprime::GameMode value) { GameMode(value); }
    [[nodiscard]] std::uint8_t RoomId() const { return read_u8(0x1); }
    void RoomId(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t room_id() const { return RoomId(); }
    void room_id(std::uint8_t value) { RoomId(value); }
    [[nodiscard]] std::uint8_t AreaId() const { return read_u8(0x2); }
    void AreaId(std::uint8_t value) { write_u8(0x2, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t area_id() const { return AreaId(); }
    void area_id(std::uint8_t value) { AreaId(value); }
    [[nodiscard]] std::uint8_t PlayerCount() const { return read_u8(0x3); }
    void PlayerCount(std::uint8_t value) { write_u8(0x3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t player_count() const { return PlayerCount(); }
    void player_count(std::uint8_t value) { PlayerCount(value); }
    [[nodiscard]] std::uint8_t MaxPlayers() const { return read_u8(0x4); }
    void MaxPlayers(std::uint8_t value) { write_u8(0x4, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t max_players() const { return MaxPlayers(); }
    void max_players(std::uint8_t value) { MaxPlayers(value); }
    [[nodiscard]] std::uint8_t CountOfBitsOfSomething() const { return read_u8(0x5); }
    void CountOfBitsOfSomething(std::uint8_t value) { write_u8(0x5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t count_of_bits_of_something() const { return CountOfBitsOfSomething(); }
    void count_of_bits_of_something(std::uint8_t value) { CountOfBitsOfSomething(value); }
    [[nodiscard]] std::uint8_t BotCount() const { return read_u8(0x6); }
    void BotCount(std::uint8_t value) { write_u8(0x6, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t bot_count() const { return BotCount(); }
    void bot_count(std::uint8_t value) { BotCount(value); }
    [[nodiscard]] std::uint8_t Field7() const { return read_u8(0x7); }
    void Field7(std::uint8_t value) { write_u8(0x7, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field7() const { return Field7(); }
    void field7(std::uint8_t value) { Field7(value); }
    [[nodiscard]] ByteArray& Field8() noexcept;
    [[nodiscard]] const ByteArray& Field8() const noexcept;
    [[nodiscard]] ByteArray& field8() noexcept { return Field8(); }
    [[nodiscard]] const ByteArray& field8() const noexcept { return Field8(); }
    [[nodiscard]] std::uint8_t DmgMultIdx() const { return read_u8(0xC); }
    void DmgMultIdx(std::uint8_t value) { write_u8(0xC, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t dmg_mult_idx() const { return DmgMultIdx(); }
    void dmg_mult_idx(std::uint8_t value) { DmgMultIdx(value); }
    [[nodiscard]] std::uint8_t FieldD() const { return read_u8(0xD); }
    void FieldD(std::uint8_t value) { write_u8(0xD, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_d() const { return FieldD(); }
    void field_d(std::uint8_t value) { FieldD(value); }
    [[nodiscard]] std::uint16_t SomeFlags() const { return read_u16(0xE); }
    void SomeFlags(std::uint16_t value) { write_u16(0xE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t some_flags() const { return SomeFlags(); }
    void some_flags(std::uint16_t value) { SomeFlags(value); }
    [[nodiscard]] std::uint16_t Field10() const { return read_u16(0x10); }
    void Field10(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field10() const { return Field10(); }
    void field10(std::uint16_t value) { Field10(value); }
    [[nodiscard]] std::uint16_t Field12() const { return read_u16(0x12); }
    void Field12(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field12() const { return Field12(); }
    void field12(std::uint16_t value) { Field12(value); }
    [[nodiscard]] std::uint16_t PointLimit() const { return read_u16(0x14); }
    void PointLimit(std::uint16_t value) { write_u16(0x14, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t point_limit() const { return PointLimit(); }
    void point_limit(std::uint16_t value) { PointLimit(value); }
    [[nodiscard]] std::uint16_t Field16() const { return read_u16(0x16); }
    void Field16(std::uint16_t value) { write_u16(0x16, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field16() const { return Field16(); }
    void field16(std::uint16_t value) { Field16(value); }
    [[nodiscard]] std::int32_t BattleTimeLimit() const { return read_i32(0x18); }
    void BattleTimeLimit(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t battle_time_limit() const { return BattleTimeLimit(); }
    void battle_time_limit(std::int32_t value) { BattleTimeLimit(value); }
    [[nodiscard]] std::int32_t EscapeState() const { return read_i32(0x1C); }
    void EscapeState(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t escape_state() const { return EscapeState(); }
    void escape_state(std::int32_t value) { EscapeState(value); }
    [[nodiscard]] std::int32_t TimeLimit() const { return read_i32(0x20); }
    void TimeLimit(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t time_limit() const { return TimeLimit(); }
    void time_limit(std::int32_t value) { TimeLimit(value); }
    [[nodiscard]] Int32Array& Sensitivity() noexcept;
    [[nodiscard]] const Int32Array& Sensitivity() const noexcept;
    [[nodiscard]] Int32Array& sensitivity() noexcept { return Sensitivity(); }
    [[nodiscard]] const Int32Array& sensitivity() const noexcept { return Sensitivity(); }
    [[nodiscard]] ByteArray& InvertSomething() noexcept;
    [[nodiscard]] const ByteArray& InvertSomething() const noexcept;
    [[nodiscard]] ByteArray& invert_something() noexcept { return InvertSomething(); }
    [[nodiscard]] const ByteArray& invert_something() const noexcept { return InvertSomething(); }
    [[nodiscard]] ByteArray& Field38() noexcept;
    [[nodiscard]] const ByteArray& Field38() const noexcept;
    [[nodiscard]] ByteArray& field38() noexcept { return Field38(); }
    [[nodiscard]] const ByteArray& field38() const noexcept { return Field38(); }
    [[nodiscard]] ByteArray& Hunters() noexcept;
    [[nodiscard]] const ByteArray& Hunters() const noexcept;
    [[nodiscard]] ByteArray& hunters() noexcept { return Hunters(); }
    [[nodiscard]] const ByteArray& hunters() const noexcept { return Hunters(); }
    [[nodiscard]] ByteArray& SuitColors() noexcept;
    [[nodiscard]] const ByteArray& SuitColors() const noexcept;
    [[nodiscard]] ByteArray& suit_colors() noexcept { return SuitColors(); }
    [[nodiscard]] const ByteArray& suit_colors() const noexcept { return SuitColors(); }
    [[nodiscard]] ByteArray& PlayerNames() noexcept;
    [[nodiscard]] const ByteArray& PlayerNames() const noexcept;
    [[nodiscard]] ByteArray& player_names() noexcept { return PlayerNames(); }
    [[nodiscard]] const ByteArray& player_names() const noexcept { return PlayerNames(); }
    [[nodiscard]] ByteArray& BotEncounterState() noexcept;
    [[nodiscard]] const ByteArray& BotEncounterState() const noexcept;
    [[nodiscard]] ByteArray& bot_encounter_state() noexcept { return BotEncounterState(); }
    [[nodiscard]] const ByteArray& bot_encounter_state() const noexcept { return BotEncounterState(); }
    [[nodiscard]] ByteArray& TeamIds() noexcept;
    [[nodiscard]] const ByteArray& TeamIds() const noexcept;
    [[nodiscard]] ByteArray& team_ids() noexcept { return TeamIds(); }
    [[nodiscard]] const ByteArray& team_ids() const noexcept { return TeamIds(); }
    [[nodiscard]] ByteArray& FieldA0() noexcept;
    [[nodiscard]] const ByteArray& FieldA0() const noexcept;
    [[nodiscard]] ByteArray& field_a0() noexcept { return FieldA0(); }
    [[nodiscard]] const ByteArray& field_a0() const noexcept { return FieldA0(); }
    [[nodiscard]] UInt16Array& BotSpawnerEntIds() noexcept;
    [[nodiscard]] const UInt16Array& BotSpawnerEntIds() const noexcept;
    [[nodiscard]] UInt16Array& bot_spawner_ent_ids() noexcept { return BotSpawnerEntIds(); }
    [[nodiscard]] const UInt16Array& bot_spawner_ent_ids() const noexcept { return BotSpawnerEntIds(); }
    [[nodiscard]] std::uint8_t PrimeHunterStats() const { return read_u8(0x5C); }
    void PrimeHunterStats(std::uint8_t value) { write_u8(0x5C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t prime_hunter_stats() const { return PrimeHunterStats(); }
    void prime_hunter_stats(std::uint8_t value) { PrimeHunterStats(value); }
    [[nodiscard]] std::uint8_t PrimeHunter() const { return read_u8(0x5D); }
    void PrimeHunter(std::uint8_t value) { write_u8(0x5D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t prime_hunter() const { return PrimeHunter(); }
    void prime_hunter(std::uint8_t value) { PrimeHunter(value); }
    [[nodiscard]] std::uint8_t RadShowPlayers() const { return read_u8(0x5E); }
    void RadShowPlayers(std::uint8_t value) { write_u8(0x5E, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t rad_show_players() const { return RadShowPlayers(); }
    void rad_show_players(std::uint8_t value) { RadShowPlayers(value); }
    [[nodiscard]] std::uint8_t FieldAF() const { return read_u8(0x5F); }
    void FieldAF(std::uint8_t value) { write_u8(0x5F, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_a_f() const { return FieldAF(); }
    void field_a_f(std::uint8_t value) { FieldAF(value); }
    [[nodiscard]] Int32Array& PrimeTime() noexcept;
    [[nodiscard]] const Int32Array& PrimeTime() const noexcept;
    [[nodiscard]] Int32Array& prime_time() noexcept { return PrimeTime(); }
    [[nodiscard]] const Int32Array& prime_time() const noexcept { return PrimeTime(); }
    [[nodiscard]] Int32Array& FieldC0() noexcept;
    [[nodiscard]] const Int32Array& FieldC0() const noexcept;
    [[nodiscard]] Int32Array& field_c0() noexcept { return FieldC0(); }
    [[nodiscard]] const Int32Array& field_c0() const noexcept { return FieldC0(); }
    [[nodiscard]] std::int32_t FieldD0() const { return read_i32(0x80); }
    void FieldD0(std::int32_t value) { write_i32(0x80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d0() const { return FieldD0(); }
    void field_d0(std::int32_t value) { FieldD0(value); }
    [[nodiscard]] std::int32_t FieldD4() const { return read_i32(0x84); }
    void FieldD4(std::int32_t value) { write_i32(0x84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d4() const { return FieldD4(); }
    void field_d4(std::int32_t value) { FieldD4(value); }
    [[nodiscard]] std::int32_t FieldD8() const { return read_i32(0x88); }
    void FieldD8(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d8() const { return FieldD8(); }
    void field_d8(std::int32_t value) { FieldD8(value); }
    [[nodiscard]] std::int32_t FieldDC() const { return read_i32(0x8C); }
    void FieldDC(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c() const { return FieldDC(); }
    void field_d_c(std::int32_t value) { FieldDC(value); }
    [[nodiscard]] std::int32_t FieldE0() const { return read_i32(0x90); }
    void FieldE0(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e0() const { return FieldE0(); }
    void field_e0(std::int32_t value) { FieldE0(value); }
    [[nodiscard]] std::int32_t FieldE4() const { return read_i32(0x94); }
    void FieldE4(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e4() const { return FieldE4(); }
    void field_e4(std::int32_t value) { FieldE4(value); }
    [[nodiscard]] std::int32_t FieldE8() const { return read_i32(0x98); }
    void FieldE8(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e8() const { return FieldE8(); }
    void field_e8(std::int32_t value) { FieldE8(value); }
    [[nodiscard]] std::int32_t FieldEC() const { return read_i32(0x9C); }
    void FieldEC(std::int32_t value) { write_i32(0x9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c() const { return FieldEC(); }
    void field_e_c(std::int32_t value) { FieldEC(value); }
    [[nodiscard]] std::int32_t FieldF0() const { return read_i32(0xA0); }
    void FieldF0(std::int32_t value) { write_i32(0xA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0() const { return FieldF0(); }
    void field_f0(std::int32_t value) { FieldF0(value); }
    [[nodiscard]] std::int32_t FieldF4() const { return read_i32(0xA4); }
    void FieldF4(std::int32_t value) { write_i32(0xA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f4() const { return FieldF4(); }
    void field_f4(std::int32_t value) { FieldF4(value); }
    [[nodiscard]] std::int32_t FieldF8() const { return read_i32(0xA8); }
    void FieldF8(std::int32_t value) { write_i32(0xA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f8() const { return FieldF8(); }
    void field_f8(std::int32_t value) { FieldF8(value); }
    [[nodiscard]] std::int32_t FieldFC() const { return read_i32(0xAC); }
    void FieldFC(std::int32_t value) { write_i32(0xAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_c() const { return FieldFC(); }
    void field_f_c(std::int32_t value) { FieldFC(value); }
    [[nodiscard]] std::int32_t Field100() const { return read_i32(0xB0); }
    void Field100(std::int32_t value) { write_i32(0xB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field100() const { return Field100(); }
    void field100(std::int32_t value) { Field100(value); }
    [[nodiscard]] std::int32_t Field104() const { return read_i32(0xB4); }
    void Field104(std::int32_t value) { write_i32(0xB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field104() const { return Field104(); }
    void field104(std::int32_t value) { Field104(value); }
    [[nodiscard]] std::int32_t Field108() const { return read_i32(0xB8); }
    void Field108(std::int32_t value) { write_i32(0xB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field108() const { return Field108(); }
    void field108(std::int32_t value) { Field108(value); }
    [[nodiscard]] std::int32_t Field10C() const { return read_i32(0xBC); }
    void Field10C(std::int32_t value) { write_i32(0xBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10_c() const { return Field10C(); }
    void field10_c(std::int32_t value) { Field10C(value); }
    [[nodiscard]] std::int32_t Field110() const { return read_i32(0xC0); }
    void Field110(std::int32_t value) { write_i32(0xC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field110() const { return Field110(); }
    void field110(std::int32_t value) { Field110(value); }
    [[nodiscard]] std::int32_t Field114() const { return read_i32(0xC4); }
    void Field114(std::int32_t value) { write_i32(0xC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field114() const { return Field114(); }
    void field114(std::int32_t value) { Field114(value); }
    [[nodiscard]] std::int32_t Field118() const { return read_i32(0xC8); }
    void Field118(std::int32_t value) { write_i32(0xC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field118() const { return Field118(); }
    void field118(std::int32_t value) { Field118(value); }
    [[nodiscard]] std::int32_t Field11C() const { return read_i32(0xCC); }
    void Field11C(std::int32_t value) { write_i32(0xCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field11_c() const { return Field11C(); }
    void field11_c(std::int32_t value) { Field11C(value); }
    [[nodiscard]] std::int32_t Field120() const { return read_i32(0xD0); }
    void Field120(std::int32_t value) { write_i32(0xD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field120() const { return Field120(); }
    void field120(std::int32_t value) { Field120(value); }
    [[nodiscard]] std::int32_t Field124() const { return read_i32(0xD4); }
    void Field124(std::int32_t value) { write_i32(0xD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field124() const { return Field124(); }
    void field124(std::int32_t value) { Field124(value); }
    [[nodiscard]] std::int32_t Field128() const { return read_i32(0xD8); }
    void Field128(std::int32_t value) { write_i32(0xD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field128() const { return Field128(); }
    void field128(std::int32_t value) { Field128(value); }
    [[nodiscard]] std::int32_t Field12C() const { return read_i32(0xDC); }
    void Field12C(std::int32_t value) { write_i32(0xDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field12_c() const { return Field12C(); }
    void field12_c(std::int32_t value) { Field12C(value); }
    [[nodiscard]] std::int32_t Field130() const { return read_i32(0xE0); }
    void Field130(std::int32_t value) { write_i32(0xE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field130() const { return Field130(); }
    void field130(std::int32_t value) { Field130(value); }
    [[nodiscard]] std::int32_t Field134() const { return read_i32(0xE4); }
    void Field134(std::int32_t value) { write_i32(0xE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field134() const { return Field134(); }
    void field134(std::int32_t value) { Field134(value); }
    [[nodiscard]] std::int32_t Field138() const { return read_i32(0xE8); }
    void Field138(std::int32_t value) { write_i32(0xE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field138() const { return Field138(); }
    void field138(std::int32_t value) { Field138(value); }
    [[nodiscard]] std::int32_t Field13C() const { return read_i32(0xEC); }
    void Field13C(std::int32_t value) { write_i32(0xEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field13_c() const { return Field13C(); }
    void field13_c(std::int32_t value) { Field13C(value); }
    [[nodiscard]] std::int32_t Field140() const { return read_i32(0xF0); }
    void Field140(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field140() const { return Field140(); }
    void field140(std::int32_t value) { Field140(value); }
    [[nodiscard]] std::int32_t Field144() const { return read_i32(0xF4); }
    void Field144(std::int32_t value) { write_i32(0xF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field144() const { return Field144(); }
    void field144(std::int32_t value) { Field144(value); }
    [[nodiscard]] std::int32_t Field148() const { return read_i32(0xF8); }
    void Field148(std::int32_t value) { write_i32(0xF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field148() const { return Field148(); }
    void field148(std::int32_t value) { Field148(value); }
    [[nodiscard]] std::int32_t Field14C() const { return read_i32(0xFC); }
    void Field14C(std::int32_t value) { write_i32(0xFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14_c() const { return Field14C(); }
    void field14_c(std::int32_t value) { Field14C(value); }
    [[nodiscard]] std::int32_t Field150() const { return read_i32(0x100); }
    void Field150(std::int32_t value) { write_i32(0x100, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field150() const { return Field150(); }
    void field150(std::int32_t value) { Field150(value); }
    [[nodiscard]] std::int32_t Field154() const { return read_i32(0x104); }
    void Field154(std::int32_t value) { write_i32(0x104, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field154() const { return Field154(); }
    void field154(std::int32_t value) { Field154(value); }
    [[nodiscard]] std::int32_t Field158() const { return read_i32(0x108); }
    void Field158(std::int32_t value) { write_i32(0x108, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field158() const { return Field158(); }
    void field158(std::int32_t value) { Field158(value); }
    [[nodiscard]] std::int32_t Field15C() const { return read_i32(0x10C); }
    void Field15C(std::int32_t value) { write_i32(0x10C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field15_c() const { return Field15C(); }
    void field15_c(std::int32_t value) { Field15C(value); }
    [[nodiscard]] Int32Array& Field160() noexcept;
    [[nodiscard]] const Int32Array& Field160() const noexcept;
    [[nodiscard]] Int32Array& field160() noexcept { return Field160(); }
    [[nodiscard]] const Int32Array& field160() const noexcept { return Field160(); }
    [[nodiscard]] std::int32_t Field170() const { return read_i32(0x120); }
    void Field170(std::int32_t value) { write_i32(0x120, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field170() const { return Field170(); }
    void field170(std::int32_t value) { Field170(value); }
    [[nodiscard]] std::int32_t Field174() const { return read_i32(0x124); }
    void Field174(std::int32_t value) { write_i32(0x124, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field174() const { return Field174(); }
    void field174(std::int32_t value) { Field174(value); }
    [[nodiscard]] std::int32_t Field178() const { return read_i32(0x128); }
    void Field178(std::int32_t value) { write_i32(0x128, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field178() const { return Field178(); }
    void field178(std::int32_t value) { Field178(value); }
    [[nodiscard]] std::int32_t Field17C() const { return read_i32(0x12C); }
    void Field17C(std::int32_t value) { write_i32(0x12C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field17_c() const { return Field17C(); }
    void field17_c(std::int32_t value) { Field17C(value); }
    [[nodiscard]] Int32Array& Field180() noexcept;
    [[nodiscard]] const Int32Array& Field180() const noexcept;
    [[nodiscard]] Int32Array& field180() noexcept { return Field180(); }
    [[nodiscard]] const Int32Array& field180() const noexcept { return Field180(); }
    [[nodiscard]] Int32Array& Deaths() noexcept;
    [[nodiscard]] const Int32Array& Deaths() const noexcept;
    [[nodiscard]] Int32Array& deaths() noexcept { return Deaths(); }
    [[nodiscard]] const Int32Array& deaths() const noexcept { return Deaths(); }
    [[nodiscard]] Int32Array& Field1A0() noexcept;
    [[nodiscard]] const Int32Array& Field1A0() const noexcept;
    [[nodiscard]] Int32Array& field1_a0() noexcept { return Field1A0(); }
    [[nodiscard]] const Int32Array& field1_a0() const noexcept { return Field1A0(); }
    [[nodiscard]] std::int32_t Field1B0() const { return read_i32(0x160); }
    void Field1B0(std::int32_t value) { write_i32(0x160, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_b0() const { return Field1B0(); }
    void field1_b0(std::int32_t value) { Field1B0(value); }
    [[nodiscard]] std::int32_t Field1B4() const { return read_i32(0x164); }
    void Field1B4(std::int32_t value) { write_i32(0x164, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_b4() const { return Field1B4(); }
    void field1_b4(std::int32_t value) { Field1B4(value); }
    [[nodiscard]] std::int32_t Field1B8() const { return read_i32(0x168); }
    void Field1B8(std::int32_t value) { write_i32(0x168, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_b8() const { return Field1B8(); }
    void field1_b8(std::int32_t value) { Field1B8(value); }
    [[nodiscard]] std::int32_t Field1BC() const { return read_i32(0x16C); }
    void Field1BC(std::int32_t value) { write_i32(0x16C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_b_c() const { return Field1BC(); }
    void field1_b_c(std::int32_t value) { Field1BC(value); }
    [[nodiscard]] Int32Array& TeamkillsMaybe() noexcept;
    [[nodiscard]] const Int32Array& TeamkillsMaybe() const noexcept;
    [[nodiscard]] Int32Array& teamkills_maybe() noexcept { return TeamkillsMaybe(); }
    [[nodiscard]] const Int32Array& teamkills_maybe() const noexcept { return TeamkillsMaybe(); }
    [[nodiscard]] Int32Array& SuicidesMaybe() noexcept;
    [[nodiscard]] const Int32Array& SuicidesMaybe() const noexcept;
    [[nodiscard]] Int32Array& suicides_maybe() noexcept { return SuicidesMaybe(); }
    [[nodiscard]] const Int32Array& suicides_maybe() const noexcept { return SuicidesMaybe(); }
    [[nodiscard]] Int32Array& Field1E0() noexcept;
    [[nodiscard]] const Int32Array& Field1E0() const noexcept;
    [[nodiscard]] Int32Array& field1_e0() noexcept { return Field1E0(); }
    [[nodiscard]] const Int32Array& field1_e0() const noexcept { return Field1E0(); }
    [[nodiscard]] Int32Array& HeadshotsMaybe() noexcept;
    [[nodiscard]] const Int32Array& HeadshotsMaybe() const noexcept;
    [[nodiscard]] Int32Array& headshots_maybe() noexcept { return HeadshotsMaybe(); }
    [[nodiscard]] const Int32Array& headshots_maybe() const noexcept { return HeadshotsMaybe(); }
    [[nodiscard]] Int32Array& Field200() noexcept;
    [[nodiscard]] const Int32Array& Field200() const noexcept;
    [[nodiscard]] Int32Array& field200() noexcept { return Field200(); }
    [[nodiscard]] const Int32Array& field200() const noexcept { return Field200(); }
    [[nodiscard]] Int32Array& DmgDealt() noexcept;
    [[nodiscard]] const Int32Array& DmgDealt() const noexcept;
    [[nodiscard]] Int32Array& dmg_dealt() noexcept { return DmgDealt(); }
    [[nodiscard]] const Int32Array& dmg_dealt() const noexcept { return DmgDealt(); }
    [[nodiscard]] Int32Array& DmgMax() noexcept;
    [[nodiscard]] const Int32Array& DmgMax() const noexcept;
    [[nodiscard]] Int32Array& dmg_max() noexcept { return DmgMax(); }
    [[nodiscard]] const Int32Array& dmg_max() const noexcept { return DmgMax(); }
    [[nodiscard]] Int32Array& BattlePoints() noexcept;
    [[nodiscard]] const Int32Array& BattlePoints() const noexcept;
    [[nodiscard]] Int32Array& battle_points() noexcept { return BattlePoints(); }
    [[nodiscard]] const Int32Array& battle_points() const noexcept { return BattlePoints(); }
    [[nodiscard]] std::int32_t Field240() const { return read_i32(0x1F0); }
    void Field240(std::int32_t value) { write_i32(0x1F0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field240() const { return Field240(); }
    void field240(std::int32_t value) { Field240(value); }
    [[nodiscard]] std::int32_t Field244() const { return read_i32(0x1F4); }
    void Field244(std::int32_t value) { write_i32(0x1F4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field244() const { return Field244(); }
    void field244(std::int32_t value) { Field244(value); }
    [[nodiscard]] std::int32_t Field248() const { return read_i32(0x1F8); }
    void Field248(std::int32_t value) { write_i32(0x1F8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field248() const { return Field248(); }
    void field248(std::int32_t value) { Field248(value); }
    [[nodiscard]] std::int32_t Field24C() const { return read_i32(0x1FC); }
    void Field24C(std::int32_t value) { write_i32(0x1FC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24_c() const { return Field24C(); }
    void field24_c(std::int32_t value) { Field24C(value); }
    [[nodiscard]] ByteArray& Standings() noexcept;
    [[nodiscard]] const ByteArray& Standings() const noexcept;
    [[nodiscard]] ByteArray& standings() noexcept { return Standings(); }
    [[nodiscard]] const ByteArray& standings() const noexcept { return Standings(); }
    [[nodiscard]] std::int32_t Field254() const { return read_i32(0x204); }
    void Field254(std::int32_t value) { write_i32(0x204, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field254() const { return Field254(); }
    void field254(std::int32_t value) { Field254(value); }
    [[nodiscard]] std::uint8_t Field258() const { return read_u8(0x208); }
    void Field258(std::uint8_t value) { write_u8(0x208, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field258() const { return Field258(); }
    void field258(std::uint8_t value) { Field258(value); }
    [[nodiscard]] std::uint8_t Field259() const { return read_u8(0x209); }
    void Field259(std::uint8_t value) { write_u8(0x209, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field259() const { return Field259(); }
    void field259(std::uint8_t value) { Field259(value); }
    [[nodiscard]] std::uint16_t Field25A() const { return read_u16(0x20A); }
    void Field25A(std::uint16_t value) { write_u16(0x20A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field25_a() const { return Field25A(); }
    void field25_a(std::uint16_t value) { Field25A(value); }
    [[nodiscard]] ByteArray& KillStreaks() noexcept;
    [[nodiscard]] const ByteArray& KillStreaks() const noexcept;
    [[nodiscard]] ByteArray& kill_streaks() noexcept { return KillStreaks(); }
    [[nodiscard]] const ByteArray& kill_streaks() const noexcept { return KillStreaks(); }
    [[nodiscard]] UInt16Array& Field260() noexcept;
    [[nodiscard]] const UInt16Array& Field260() const noexcept;
    [[nodiscard]] UInt16Array& field260() noexcept { return Field260(); }
    [[nodiscard]] const UInt16Array& field260() const noexcept { return Field260(); }
    [[nodiscard]] UInt16Array& Field268() noexcept;
    [[nodiscard]] const UInt16Array& Field268() const noexcept;
    [[nodiscard]] UInt16Array& field268() noexcept { return Field268(); }
    [[nodiscard]] const UInt16Array& field268() const noexcept { return Field268(); }
    [[nodiscard]] UInt16Array& Field270() noexcept;
    [[nodiscard]] const UInt16Array& Field270() const noexcept;
    [[nodiscard]] UInt16Array& field270() noexcept { return Field270(); }
    [[nodiscard]] const UInt16Array& field270() const noexcept { return Field270(); }
    [[nodiscard]] std::int32_t Field278() const { return read_i32(0x228); }
    void Field278(std::int32_t value) { write_i32(0x228, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field278() const { return Field278(); }
    void field278(std::int32_t value) { Field278(value); }
    [[nodiscard]] std::int32_t Frames() const { return read_i32(0x22C); }
    void Frames(std::int32_t value) { write_i32(0x22C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t frames() const { return Frames(); }
    void frames(std::int32_t value) { Frames(value); }
    [[nodiscard]] std::int32_t LoadRoomId() const { return read_i32(0x230); }
    void LoadRoomId(std::int32_t value) { write_i32(0x230, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t load_room_id() const { return LoadRoomId(); }
    void load_room_id(std::int32_t value) { LoadRoomId(value); }
    [[nodiscard]] std::int32_t Field284() const { return read_i32(0x234); }
    void Field284(std::int32_t value) { write_i32(0x234, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field284() const { return Field284(); }
    void field284(std::int32_t value) { Field284(value); }
    [[nodiscard]] std::int32_t Field288() const { return read_i32(0x238); }
    void Field288(std::int32_t value) { write_i32(0x238, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field288() const { return Field288(); }
    void field288(std::int32_t value) { Field288(value); }
    [[nodiscard]] std::uint8_t LayerId() const { return read_u8(0x23C); }
    void LayerId(std::uint8_t value) { write_u8(0x23C, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t layer_id() const { return LayerId(); }
    void layer_id(std::uint8_t value) { LayerId(value); }
    [[nodiscard]] std::uint8_t Field28D() const { return read_u8(0x23D); }
    void Field28D(std::uint8_t value) { write_u8(0x23D, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field28_d() const { return Field28D(); }
    void field28_d(std::uint8_t value) { Field28D(value); }
    [[nodiscard]] std::uint16_t Field28E() const { return read_u16(0x23E); }
    void Field28E(std::uint16_t value) { write_u16(0x23E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field28_e() const { return Field28E(); }
    void field28_e(std::uint16_t value) { Field28E(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
    static constexpr std::size_t _off3 = 0x3;
    static constexpr std::size_t _off4 = 0x4;
    static constexpr std::size_t _off5 = 0x5;
    static constexpr std::size_t _off6 = 0x6;
    static constexpr std::size_t _off7 = 0x7;
    static constexpr std::size_t _off8 = 0x8;
    static constexpr std::size_t _off9 = 0xC;
    static constexpr std::size_t _off10 = 0xD;
    static constexpr std::size_t _off11 = 0xE;
    static constexpr std::size_t _off12 = 0x10;
    static constexpr std::size_t _off13 = 0x12;
    static constexpr std::size_t _off14 = 0x14;
    static constexpr std::size_t _off15 = 0x16;
    static constexpr std::size_t _off16 = 0x18;
    static constexpr std::size_t _off17 = 0x1C;
    static constexpr std::size_t _off18 = 0x20;
    static constexpr std::size_t _off19 = 0x24;
    static constexpr std::size_t _off20 = 0x34;
    static constexpr std::size_t _off21 = 0x38;
    static constexpr std::size_t _off22 = 0x3C;
    static constexpr std::size_t _off23 = 0x40;
    static constexpr std::size_t _off24 = 0x44;
    static constexpr std::size_t _off25 = 0x48;
    static constexpr std::size_t _off26 = 0x4C;
    static constexpr std::size_t _off27 = 0x50;
    static constexpr std::size_t _off28 = 0x54;
    static constexpr std::size_t _off29 = 0x5C;
    static constexpr std::size_t _off30 = 0x5D;
    static constexpr std::size_t _off31 = 0x5E;
    static constexpr std::size_t _off32 = 0x5F;
    static constexpr std::size_t _off33 = 0x60;
    static constexpr std::size_t _off34 = 0x70;
    static constexpr std::size_t _off35 = 0x80;
    static constexpr std::size_t _off36 = 0x84;
    static constexpr std::size_t _off37 = 0x88;
    static constexpr std::size_t _off38 = 0x8C;
    static constexpr std::size_t _off39 = 0x90;
    static constexpr std::size_t _off40 = 0x94;
    static constexpr std::size_t _off41 = 0x98;
    static constexpr std::size_t _off42 = 0x9C;
    static constexpr std::size_t _off43 = 0xA0;
    static constexpr std::size_t _off44 = 0xA4;
    static constexpr std::size_t _off45 = 0xA8;
    static constexpr std::size_t _off46 = 0xAC;
    static constexpr std::size_t _off47 = 0xB0;
    static constexpr std::size_t _off48 = 0xB4;
    static constexpr std::size_t _off49 = 0xB8;
    static constexpr std::size_t _off50 = 0xBC;
    static constexpr std::size_t _off51 = 0xC0;
    static constexpr std::size_t _off52 = 0xC4;
    static constexpr std::size_t _off53 = 0xC8;
    static constexpr std::size_t _off54 = 0xCC;
    static constexpr std::size_t _off55 = 0xD0;
    static constexpr std::size_t _off56 = 0xD4;
    static constexpr std::size_t _off57 = 0xD8;
    static constexpr std::size_t _off58 = 0xDC;
    static constexpr std::size_t _off59 = 0xE0;
    static constexpr std::size_t _off60 = 0xE4;
    static constexpr std::size_t _off61 = 0xE8;
    static constexpr std::size_t _off62 = 0xEC;
    static constexpr std::size_t _off63 = 0xF0;
    static constexpr std::size_t _off64 = 0xF4;
    static constexpr std::size_t _off65 = 0xF8;
    static constexpr std::size_t _off66 = 0xFC;
    static constexpr std::size_t _off67 = 0x100;
    static constexpr std::size_t _off68 = 0x104;
    static constexpr std::size_t _off69 = 0x108;
    static constexpr std::size_t _off70 = 0x10C;
    static constexpr std::size_t _off71 = 0x110;
    static constexpr std::size_t _off72 = 0x120;
    static constexpr std::size_t _off73 = 0x124;
    static constexpr std::size_t _off74 = 0x128;
    static constexpr std::size_t _off75 = 0x12C;
    static constexpr std::size_t _off76 = 0x130;
    static constexpr std::size_t _off77 = 0x140;
    static constexpr std::size_t _off78 = 0x150;
    static constexpr std::size_t _off79 = 0x160;
    static constexpr std::size_t _off80 = 0x164;
    static constexpr std::size_t _off81 = 0x168;
    static constexpr std::size_t _off82 = 0x16C;
    static constexpr std::size_t _off83 = 0x170;
    static constexpr std::size_t _off84 = 0x180;
    static constexpr std::size_t _off85 = 0x190;
    static constexpr std::size_t _off86 = 0x1A0;
    static constexpr std::size_t _off87 = 0x1B0;
    static constexpr std::size_t _off88 = 0x1C0;
    static constexpr std::size_t _off89 = 0x1D0;
    static constexpr std::size_t _off90 = 0x1E0;
    static constexpr std::size_t _off91 = 0x1F0;
    static constexpr std::size_t _off92 = 0x1F4;
    static constexpr std::size_t _off93 = 0x1F8;
    static constexpr std::size_t _off94 = 0x1FC;
    static constexpr std::size_t _off95 = 0x200;
    static constexpr std::size_t _off96 = 0x204;
    static constexpr std::size_t _off97 = 0x208;
    static constexpr std::size_t _off98 = 0x209;
    static constexpr std::size_t _off99 = 0x20A;
    static constexpr std::size_t _off100 = 0x20C;
    static constexpr std::size_t _off101 = 0x210;
    static constexpr std::size_t _off102 = 0x218;
    static constexpr std::size_t _off103 = 0x220;
    static constexpr std::size_t _off104 = 0x228;
    static constexpr std::size_t _off105 = 0x22C;
    static constexpr std::size_t _off106 = 0x230;
    static constexpr std::size_t _off107 = 0x234;
    static constexpr std::size_t _off108 = 0x238;
    static constexpr std::size_t _off109 = 0x23C;
    static constexpr std::size_t _off110 = 0x23D;
    static constexpr std::size_t _off111 = 0x23E;
    std::unique_ptr<ByteArray> field8_;
    std::unique_ptr<Int32Array> sensitivity_;
    std::unique_ptr<ByteArray> invert_something_;
    std::unique_ptr<ByteArray> field38_;
    std::unique_ptr<ByteArray> hunters_;
    std::unique_ptr<ByteArray> suit_colors_;
    std::unique_ptr<ByteArray> player_names_;
    std::unique_ptr<ByteArray> bot_encounter_state_;
    std::unique_ptr<ByteArray> team_ids_;
    std::unique_ptr<ByteArray> field_a0_;
    std::unique_ptr<UInt16Array> bot_spawner_ent_ids_;
    std::unique_ptr<Int32Array> prime_time_;
    std::unique_ptr<Int32Array> field_c0_;
    std::unique_ptr<Int32Array> field160_;
    std::unique_ptr<Int32Array> field180_;
    std::unique_ptr<Int32Array> deaths_;
    std::unique_ptr<Int32Array> field1_a0_;
    std::unique_ptr<Int32Array> teamkills_maybe_;
    std::unique_ptr<Int32Array> suicides_maybe_;
    std::unique_ptr<Int32Array> field1_e0_;
    std::unique_ptr<Int32Array> headshots_maybe_;
    std::unique_ptr<Int32Array> field200_;
    std::unique_ptr<Int32Array> dmg_dealt_;
    std::unique_ptr<Int32Array> dmg_max_;
    std::unique_ptr<Int32Array> battle_points_;
    std::unique_ptr<ByteArray> standings_;
    std::unique_ptr<ByteArray> kill_streaks_;
    std::unique_ptr<UInt16Array> field260_;
    std::unique_ptr<UInt16Array> field268_;
    std::unique_ptr<UInt16Array> field270_;
};

class KioskGameState : public MemoryClass {
public:
    KioskGameState(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    KioskGameState(Buffer& buffer, std::uint32_t address);
    ~KioskGameState() override;

    [[nodiscard]] ::fruityprime::GameMode GameMode() const { return static_cast<::fruityprime::GameMode>(read_u8(0x0)); }
    void GameMode(::fruityprime::GameMode value) { write_u8(0x0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] ::fruityprime::GameMode game_mode() const { return GameMode(); }
    void game_mode(::fruityprime::GameMode value) { GameMode(value); }
    [[nodiscard]] std::uint8_t RoomId() const { return read_u8(0x1); }
    void RoomId(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t room_id() const { return RoomId(); }
    void room_id(std::uint8_t value) { RoomId(value); }
    [[nodiscard]] std::uint8_t AreaId() const { return read_u8(0x2); }
    void AreaId(std::uint8_t value) { write_u8(0x2, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t area_id() const { return AreaId(); }
    void area_id(std::uint8_t value) { AreaId(value); }
    [[nodiscard]] std::uint8_t PlayerCount() const { return read_u8(0x3); }
    void PlayerCount(std::uint8_t value) { write_u8(0x3, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t player_count() const { return PlayerCount(); }
    void player_count(std::uint8_t value) { PlayerCount(value); }
    [[nodiscard]] std::uint8_t MaxPlayers() const { return read_u8(0x4); }
    void MaxPlayers(std::uint8_t value) { write_u8(0x4, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t max_players() const { return MaxPlayers(); }
    void max_players(std::uint8_t value) { MaxPlayers(value); }
    [[nodiscard]] std::uint8_t BotCount() const { return read_u8(0x5); }
    void BotCount(std::uint8_t value) { write_u8(0x5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t bot_count() const { return BotCount(); }
    void bot_count(std::uint8_t value) { BotCount(value); }
    [[nodiscard]] ByteArray& Field6() noexcept;
    [[nodiscard]] const ByteArray& Field6() const noexcept;
    [[nodiscard]] ByteArray& field6() noexcept { return Field6(); }
    [[nodiscard]] const ByteArray& field6() const noexcept { return Field6(); }
    [[nodiscard]] std::uint8_t DmgMultIdx() const { return read_u8(0xA); }
    void DmgMultIdx(std::uint8_t value) { write_u8(0xA, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t dmg_mult_idx() const { return DmgMultIdx(); }
    void dmg_mult_idx(std::uint8_t value) { DmgMultIdx(value); }
    [[nodiscard]] std::uint8_t FieldB() const { return read_u8(0xB); }
    void FieldB(std::uint8_t value) { write_u8(0xB, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_b() const { return FieldB(); }
    void field_b(std::uint8_t value) { FieldB(value); }
    [[nodiscard]] std::uint16_t SomeFlags() const { return read_u16(0xC); }
    void SomeFlags(std::uint16_t value) { write_u16(0xC, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t some_flags() const { return SomeFlags(); }
    void some_flags(std::uint16_t value) { SomeFlags(value); }
    [[nodiscard]] std::uint16_t FieldE() const { return read_u16(0xE); }
    void FieldE(std::uint16_t value) { write_u16(0xE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field_e() const { return FieldE(); }
    void field_e(std::uint16_t value) { FieldE(value); }
    [[nodiscard]] std::uint16_t Field10() const { return read_u16(0x10); }
    void Field10(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field10() const { return Field10(); }
    void field10(std::uint16_t value) { Field10(value); }
    [[nodiscard]] std::uint16_t PointLimit() const { return read_u16(0x12); }
    void PointLimit(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t point_limit() const { return PointLimit(); }
    void point_limit(std::uint16_t value) { PointLimit(value); }
    [[nodiscard]] std::int32_t BattleTimeLimit() const { return read_i32(0x14); }
    void BattleTimeLimit(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t battle_time_limit() const { return BattleTimeLimit(); }
    void battle_time_limit(std::int32_t value) { BattleTimeLimit(value); }
    [[nodiscard]] std::int32_t EscapeState() const { return read_i32(0x18); }
    void EscapeState(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t escape_state() const { return EscapeState(); }
    void escape_state(std::int32_t value) { EscapeState(value); }
    [[nodiscard]] std::int32_t TimeLimit() const { return read_i32(0x1C); }
    void TimeLimit(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t time_limit() const { return TimeLimit(); }
    void time_limit(std::int32_t value) { TimeLimit(value); }
    [[nodiscard]] Int32Array& Sensitivity() noexcept;
    [[nodiscard]] const Int32Array& Sensitivity() const noexcept;
    [[nodiscard]] Int32Array& sensitivity() noexcept { return Sensitivity(); }
    [[nodiscard]] const Int32Array& sensitivity() const noexcept { return Sensitivity(); }
    [[nodiscard]] ByteArray& InvertSomething() noexcept;
    [[nodiscard]] const ByteArray& InvertSomething() const noexcept;
    [[nodiscard]] ByteArray& invert_something() noexcept { return InvertSomething(); }
    [[nodiscard]] const ByteArray& invert_something() const noexcept { return InvertSomething(); }
    [[nodiscard]] ByteArray& Field34() noexcept;
    [[nodiscard]] const ByteArray& Field34() const noexcept;
    [[nodiscard]] ByteArray& field34() noexcept { return Field34(); }
    [[nodiscard]] const ByteArray& field34() const noexcept { return Field34(); }
    [[nodiscard]] ByteArray& Hunters() noexcept;
    [[nodiscard]] const ByteArray& Hunters() const noexcept;
    [[nodiscard]] ByteArray& hunters() noexcept { return Hunters(); }
    [[nodiscard]] const ByteArray& hunters() const noexcept { return Hunters(); }
    [[nodiscard]] ByteArray& SuitColors() noexcept;
    [[nodiscard]] const ByteArray& SuitColors() const noexcept;
    [[nodiscard]] ByteArray& suit_colors() noexcept { return SuitColors(); }
    [[nodiscard]] const ByteArray& suit_colors() const noexcept { return SuitColors(); }
    [[nodiscard]] ByteArray& PlayerNames() noexcept;
    [[nodiscard]] const ByteArray& PlayerNames() const noexcept;
    [[nodiscard]] ByteArray& player_names() noexcept { return PlayerNames(); }
    [[nodiscard]] const ByteArray& player_names() const noexcept { return PlayerNames(); }
    [[nodiscard]] ByteArray& BotEncounterState() noexcept;
    [[nodiscard]] const ByteArray& BotEncounterState() const noexcept;
    [[nodiscard]] ByteArray& bot_encounter_state() noexcept { return BotEncounterState(); }
    [[nodiscard]] const ByteArray& bot_encounter_state() const noexcept { return BotEncounterState(); }
    [[nodiscard]] ByteArray& TeamIds() noexcept;
    [[nodiscard]] const ByteArray& TeamIds() const noexcept;
    [[nodiscard]] ByteArray& team_ids() noexcept { return TeamIds(); }
    [[nodiscard]] const ByteArray& team_ids() const noexcept { return TeamIds(); }
    [[nodiscard]] ByteArray& Field9C() noexcept;
    [[nodiscard]] const ByteArray& Field9C() const noexcept;
    [[nodiscard]] ByteArray& field9_c() noexcept { return Field9C(); }
    [[nodiscard]] const ByteArray& field9_c() const noexcept { return Field9C(); }
    [[nodiscard]] UInt16Array& BotSpawnerEntIds() noexcept;
    [[nodiscard]] const UInt16Array& BotSpawnerEntIds() const noexcept;
    [[nodiscard]] UInt16Array& bot_spawner_ent_ids() noexcept { return BotSpawnerEntIds(); }
    [[nodiscard]] const UInt16Array& bot_spawner_ent_ids() const noexcept { return BotSpawnerEntIds(); }
    [[nodiscard]] std::uint8_t PrimeHunterStats() const { return read_u8(0x58); }
    void PrimeHunterStats(std::uint8_t value) { write_u8(0x58, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t prime_hunter_stats() const { return PrimeHunterStats(); }
    void prime_hunter_stats(std::uint8_t value) { PrimeHunterStats(value); }
    [[nodiscard]] std::uint8_t PrimeHunter() const { return read_u8(0x59); }
    void PrimeHunter(std::uint8_t value) { write_u8(0x59, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t prime_hunter() const { return PrimeHunter(); }
    void prime_hunter(std::uint8_t value) { PrimeHunter(value); }
    [[nodiscard]] std::uint8_t RadShowPlayers() const { return read_u8(0x5A); }
    void RadShowPlayers(std::uint8_t value) { write_u8(0x5A, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t rad_show_players() const { return RadShowPlayers(); }
    void rad_show_players(std::uint8_t value) { RadShowPlayers(value); }
    [[nodiscard]] std::uint8_t FieldAB() const { return read_u8(0x5B); }
    void FieldAB(std::uint8_t value) { write_u8(0x5B, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field_a_b() const { return FieldAB(); }
    void field_a_b(std::uint8_t value) { FieldAB(value); }
    [[nodiscard]] Int32Array& PrimeTime() noexcept;
    [[nodiscard]] const Int32Array& PrimeTime() const noexcept;
    [[nodiscard]] Int32Array& prime_time() noexcept { return PrimeTime(); }
    [[nodiscard]] const Int32Array& prime_time() const noexcept { return PrimeTime(); }
    [[nodiscard]] Int32Array& FieldBC() noexcept;
    [[nodiscard]] const Int32Array& FieldBC() const noexcept;
    [[nodiscard]] Int32Array& field_b_c() noexcept { return FieldBC(); }
    [[nodiscard]] const Int32Array& field_b_c() const noexcept { return FieldBC(); }
    [[nodiscard]] std::int32_t FieldCC() const { return read_i32(0x7C); }
    void FieldCC(std::int32_t value) { write_i32(0x7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c_c() const { return FieldCC(); }
    void field_c_c(std::int32_t value) { FieldCC(value); }
    [[nodiscard]] std::int32_t FieldD0() const { return read_i32(0x80); }
    void FieldD0(std::int32_t value) { write_i32(0x80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d0() const { return FieldD0(); }
    void field_d0(std::int32_t value) { FieldD0(value); }
    [[nodiscard]] std::int32_t FieldD4() const { return read_i32(0x84); }
    void FieldD4(std::int32_t value) { write_i32(0x84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d4() const { return FieldD4(); }
    void field_d4(std::int32_t value) { FieldD4(value); }
    [[nodiscard]] std::int32_t FieldD8() const { return read_i32(0x88); }
    void FieldD8(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d8() const { return FieldD8(); }
    void field_d8(std::int32_t value) { FieldD8(value); }
    [[nodiscard]] std::int32_t FieldDC() const { return read_i32(0x8C); }
    void FieldDC(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_d_c() const { return FieldDC(); }
    void field_d_c(std::int32_t value) { FieldDC(value); }
    [[nodiscard]] std::int32_t FieldE0() const { return read_i32(0x90); }
    void FieldE0(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e0() const { return FieldE0(); }
    void field_e0(std::int32_t value) { FieldE0(value); }
    [[nodiscard]] std::int32_t FieldE4() const { return read_i32(0x94); }
    void FieldE4(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e4() const { return FieldE4(); }
    void field_e4(std::int32_t value) { FieldE4(value); }
    [[nodiscard]] std::int32_t FieldE9() const { return read_i32(0x98); }
    void FieldE9(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e9() const { return FieldE9(); }
    void field_e9(std::int32_t value) { FieldE9(value); }
    [[nodiscard]] std::int32_t FieldEC() const { return read_i32(0x9C); }
    void FieldEC(std::int32_t value) { write_i32(0x9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_e_c() const { return FieldEC(); }
    void field_e_c(std::int32_t value) { FieldEC(value); }
    [[nodiscard]] std::int32_t FieldF0() const { return read_i32(0xA0); }
    void FieldF0(std::int32_t value) { write_i32(0xA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f0() const { return FieldF0(); }
    void field_f0(std::int32_t value) { FieldF0(value); }
    [[nodiscard]] std::int32_t FieldF4() const { return read_i32(0xA4); }
    void FieldF4(std::int32_t value) { write_i32(0xA4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f4() const { return FieldF4(); }
    void field_f4(std::int32_t value) { FieldF4(value); }
    [[nodiscard]] std::int32_t FieldF8() const { return read_i32(0xA8); }
    void FieldF8(std::int32_t value) { write_i32(0xA8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f8() const { return FieldF8(); }
    void field_f8(std::int32_t value) { FieldF8(value); }
    [[nodiscard]] std::int32_t FieldFC() const { return read_i32(0xAC); }
    void FieldFC(std::int32_t value) { write_i32(0xAC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_c() const { return FieldFC(); }
    void field_f_c(std::int32_t value) { FieldFC(value); }
    [[nodiscard]] std::int32_t Field100() const { return read_i32(0xB0); }
    void Field100(std::int32_t value) { write_i32(0xB0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field100() const { return Field100(); }
    void field100(std::int32_t value) { Field100(value); }
    [[nodiscard]] std::int32_t Field104() const { return read_i32(0xB4); }
    void Field104(std::int32_t value) { write_i32(0xB4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field104() const { return Field104(); }
    void field104(std::int32_t value) { Field104(value); }
    [[nodiscard]] std::int32_t Field108() const { return read_i32(0xB8); }
    void Field108(std::int32_t value) { write_i32(0xB8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field108() const { return Field108(); }
    void field108(std::int32_t value) { Field108(value); }
    [[nodiscard]] std::int32_t Field10C() const { return read_i32(0xBC); }
    void Field10C(std::int32_t value) { write_i32(0xBC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10_c() const { return Field10C(); }
    void field10_c(std::int32_t value) { Field10C(value); }
    [[nodiscard]] std::int32_t Field110() const { return read_i32(0xC0); }
    void Field110(std::int32_t value) { write_i32(0xC0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field110() const { return Field110(); }
    void field110(std::int32_t value) { Field110(value); }
    [[nodiscard]] std::int32_t Field114() const { return read_i32(0xC4); }
    void Field114(std::int32_t value) { write_i32(0xC4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field114() const { return Field114(); }
    void field114(std::int32_t value) { Field114(value); }
    [[nodiscard]] std::int32_t Field118() const { return read_i32(0xC8); }
    void Field118(std::int32_t value) { write_i32(0xC8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field118() const { return Field118(); }
    void field118(std::int32_t value) { Field118(value); }
    [[nodiscard]] std::int32_t Field11C() const { return read_i32(0xCC); }
    void Field11C(std::int32_t value) { write_i32(0xCC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field11_c() const { return Field11C(); }
    void field11_c(std::int32_t value) { Field11C(value); }
    [[nodiscard]] std::int32_t Field120() const { return read_i32(0xD0); }
    void Field120(std::int32_t value) { write_i32(0xD0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field120() const { return Field120(); }
    void field120(std::int32_t value) { Field120(value); }
    [[nodiscard]] std::int32_t Field124() const { return read_i32(0xD4); }
    void Field124(std::int32_t value) { write_i32(0xD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field124() const { return Field124(); }
    void field124(std::int32_t value) { Field124(value); }
    [[nodiscard]] std::int32_t Field128() const { return read_i32(0xD8); }
    void Field128(std::int32_t value) { write_i32(0xD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field128() const { return Field128(); }
    void field128(std::int32_t value) { Field128(value); }
    [[nodiscard]] std::int32_t Field12C() const { return read_i32(0xDC); }
    void Field12C(std::int32_t value) { write_i32(0xDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field12_c() const { return Field12C(); }
    void field12_c(std::int32_t value) { Field12C(value); }
    [[nodiscard]] std::int32_t Field130() const { return read_i32(0xE0); }
    void Field130(std::int32_t value) { write_i32(0xE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field130() const { return Field130(); }
    void field130(std::int32_t value) { Field130(value); }
    [[nodiscard]] std::int32_t Field134() const { return read_i32(0xE4); }
    void Field134(std::int32_t value) { write_i32(0xE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field134() const { return Field134(); }
    void field134(std::int32_t value) { Field134(value); }
    [[nodiscard]] std::int32_t Field138() const { return read_i32(0xE8); }
    void Field138(std::int32_t value) { write_i32(0xE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field138() const { return Field138(); }
    void field138(std::int32_t value) { Field138(value); }
    [[nodiscard]] std::int32_t Field13C() const { return read_i32(0xEC); }
    void Field13C(std::int32_t value) { write_i32(0xEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field13_c() const { return Field13C(); }
    void field13_c(std::int32_t value) { Field13C(value); }
    [[nodiscard]] std::int32_t Field140() const { return read_i32(0xF0); }
    void Field140(std::int32_t value) { write_i32(0xF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field140() const { return Field140(); }
    void field140(std::int32_t value) { Field140(value); }
    [[nodiscard]] std::int32_t Field144() const { return read_i32(0xF4); }
    void Field144(std::int32_t value) { write_i32(0xF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field144() const { return Field144(); }
    void field144(std::int32_t value) { Field144(value); }
    [[nodiscard]] std::int32_t Field148() const { return read_i32(0xF8); }
    void Field148(std::int32_t value) { write_i32(0xF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field148() const { return Field148(); }
    void field148(std::int32_t value) { Field148(value); }
    [[nodiscard]] std::int32_t Field14C() const { return read_i32(0xFC); }
    void Field14C(std::int32_t value) { write_i32(0xFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14_c() const { return Field14C(); }
    void field14_c(std::int32_t value) { Field14C(value); }
    [[nodiscard]] std::int32_t Field150() const { return read_i32(0x100); }
    void Field150(std::int32_t value) { write_i32(0x100, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field150() const { return Field150(); }
    void field150(std::int32_t value) { Field150(value); }
    [[nodiscard]] std::int32_t Field154() const { return read_i32(0x104); }
    void Field154(std::int32_t value) { write_i32(0x104, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field154() const { return Field154(); }
    void field154(std::int32_t value) { Field154(value); }
    [[nodiscard]] std::int32_t Field158() const { return read_i32(0x108); }
    void Field158(std::int32_t value) { write_i32(0x108, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field158() const { return Field158(); }
    void field158(std::int32_t value) { Field158(value); }
    [[nodiscard]] std::int32_t Field15C() const { return read_i32(0x10C); }
    void Field15C(std::int32_t value) { write_i32(0x10C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field15_c() const { return Field15C(); }
    void field15_c(std::int32_t value) { Field15C(value); }
    [[nodiscard]] std::int32_t Field160() const { return read_i32(0x110); }
    void Field160(std::int32_t value) { write_i32(0x110, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field160() const { return Field160(); }
    void field160(std::int32_t value) { Field160(value); }
    [[nodiscard]] std::int32_t Field164() const { return read_i32(0x114); }
    void Field164(std::int32_t value) { write_i32(0x114, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field164() const { return Field164(); }
    void field164(std::int32_t value) { Field164(value); }
    [[nodiscard]] std::int32_t Field168() const { return read_i32(0x118); }
    void Field168(std::int32_t value) { write_i32(0x118, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field168() const { return Field168(); }
    void field168(std::int32_t value) { Field168(value); }
    [[nodiscard]] std::int32_t Field16C() const { return read_i32(0x11C); }
    void Field16C(std::int32_t value) { write_i32(0x11C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field16_c() const { return Field16C(); }
    void field16_c(std::int32_t value) { Field16C(value); }
    [[nodiscard]] std::int32_t Field170() const { return read_i32(0x120); }
    void Field170(std::int32_t value) { write_i32(0x120, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field170() const { return Field170(); }
    void field170(std::int32_t value) { Field170(value); }
    [[nodiscard]] std::int32_t Field174() const { return read_i32(0x124); }
    void Field174(std::int32_t value) { write_i32(0x124, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field174() const { return Field174(); }
    void field174(std::int32_t value) { Field174(value); }
    [[nodiscard]] std::int32_t Field178() const { return read_i32(0x128); }
    void Field178(std::int32_t value) { write_i32(0x128, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field178() const { return Field178(); }
    void field178(std::int32_t value) { Field178(value); }
    [[nodiscard]] Int32Array& Field17C() noexcept;
    [[nodiscard]] const Int32Array& Field17C() const noexcept;
    [[nodiscard]] Int32Array& field17_c() noexcept { return Field17C(); }
    [[nodiscard]] const Int32Array& field17_c() const noexcept { return Field17C(); }
    [[nodiscard]] std::int32_t Field18C() const { return read_i32(0x13C); }
    void Field18C(std::int32_t value) { write_i32(0x13C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18_c() const { return Field18C(); }
    void field18_c(std::int32_t value) { Field18C(value); }
    [[nodiscard]] std::int32_t Field190() const { return read_i32(0x140); }
    void Field190(std::int32_t value) { write_i32(0x140, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field190() const { return Field190(); }
    void field190(std::int32_t value) { Field190(value); }
    [[nodiscard]] std::int32_t Field194() const { return read_i32(0x144); }
    void Field194(std::int32_t value) { write_i32(0x144, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field194() const { return Field194(); }
    void field194(std::int32_t value) { Field194(value); }
    [[nodiscard]] std::int32_t Field198() const { return read_i32(0x148); }
    void Field198(std::int32_t value) { write_i32(0x148, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field198() const { return Field198(); }
    void field198(std::int32_t value) { Field198(value); }
    [[nodiscard]] Int32Array& Field19C() noexcept;
    [[nodiscard]] const Int32Array& Field19C() const noexcept;
    [[nodiscard]] Int32Array& field19_c() noexcept { return Field19C(); }
    [[nodiscard]] const Int32Array& field19_c() const noexcept { return Field19C(); }
    [[nodiscard]] Int32Array& Deaths() noexcept;
    [[nodiscard]] const Int32Array& Deaths() const noexcept;
    [[nodiscard]] Int32Array& deaths() noexcept { return Deaths(); }
    [[nodiscard]] const Int32Array& deaths() const noexcept { return Deaths(); }
    [[nodiscard]] Int32Array& Field1BC() noexcept;
    [[nodiscard]] const Int32Array& Field1BC() const noexcept;
    [[nodiscard]] Int32Array& field1_b_c() noexcept { return Field1BC(); }
    [[nodiscard]] const Int32Array& field1_b_c() const noexcept { return Field1BC(); }
    [[nodiscard]] Int32Array& Field1CC() noexcept;
    [[nodiscard]] const Int32Array& Field1CC() const noexcept;
    [[nodiscard]] Int32Array& field1_c_c() noexcept { return Field1CC(); }
    [[nodiscard]] const Int32Array& field1_c_c() const noexcept { return Field1CC(); }
    [[nodiscard]] Int32Array& SuicidesMaybe() noexcept;
    [[nodiscard]] const Int32Array& SuicidesMaybe() const noexcept;
    [[nodiscard]] Int32Array& suicides_maybe() noexcept { return SuicidesMaybe(); }
    [[nodiscard]] const Int32Array& suicides_maybe() const noexcept { return SuicidesMaybe(); }
    [[nodiscard]] std::int32_t Field1EC() const { return read_i32(0x19C); }
    void Field1EC(std::int32_t value) { write_i32(0x19C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_e_c() const { return Field1EC(); }
    void field1_e_c(std::int32_t value) { Field1EC(value); }
    [[nodiscard]] std::int32_t Field1F0() const { return read_i32(0x1A0); }
    void Field1F0(std::int32_t value) { write_i32(0x1A0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_f0() const { return Field1F0(); }
    void field1_f0(std::int32_t value) { Field1F0(value); }
    [[nodiscard]] std::int32_t Field1F4() const { return read_i32(0x1A4); }
    void Field1F4(std::int32_t value) { write_i32(0x1A4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_f4() const { return Field1F4(); }
    void field1_f4(std::int32_t value) { Field1F4(value); }
    [[nodiscard]] std::int32_t Field1F8() const { return read_i32(0x1A8); }
    void Field1F8(std::int32_t value) { write_i32(0x1A8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_f8() const { return Field1F8(); }
    void field1_f8(std::int32_t value) { Field1F8(value); }
    [[nodiscard]] Int32Array& Field1FC() noexcept;
    [[nodiscard]] const Int32Array& Field1FC() const noexcept;
    [[nodiscard]] Int32Array& field1_f_c() noexcept { return Field1FC(); }
    [[nodiscard]] const Int32Array& field1_f_c() const noexcept { return Field1FC(); }
    [[nodiscard]] Int32Array& HeadshotsMaybe() noexcept;
    [[nodiscard]] const Int32Array& HeadshotsMaybe() const noexcept;
    [[nodiscard]] Int32Array& headshots_maybe() noexcept { return HeadshotsMaybe(); }
    [[nodiscard]] const Int32Array& headshots_maybe() const noexcept { return HeadshotsMaybe(); }
    [[nodiscard]] Int32Array& Field21C() noexcept;
    [[nodiscard]] const Int32Array& Field21C() const noexcept;
    [[nodiscard]] Int32Array& field21_c() noexcept { return Field21C(); }
    [[nodiscard]] const Int32Array& field21_c() const noexcept { return Field21C(); }
    [[nodiscard]] Int32Array& DmgDealt() noexcept;
    [[nodiscard]] const Int32Array& DmgDealt() const noexcept;
    [[nodiscard]] Int32Array& dmg_dealt() noexcept { return DmgDealt(); }
    [[nodiscard]] const Int32Array& dmg_dealt() const noexcept { return DmgDealt(); }
    [[nodiscard]] Int32Array& DmgMax() noexcept;
    [[nodiscard]] const Int32Array& DmgMax() const noexcept;
    [[nodiscard]] Int32Array& dmg_max() noexcept { return DmgMax(); }
    [[nodiscard]] const Int32Array& dmg_max() const noexcept { return DmgMax(); }
    [[nodiscard]] Int32Array& BattlePoints() noexcept;
    [[nodiscard]] const Int32Array& BattlePoints() const noexcept;
    [[nodiscard]] Int32Array& battle_points() noexcept { return BattlePoints(); }
    [[nodiscard]] const Int32Array& battle_points() const noexcept { return BattlePoints(); }
    [[nodiscard]] std::int32_t Field25C() const { return read_i32(0x20C); }
    void Field25C(std::int32_t value) { write_i32(0x20C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field25_c() const { return Field25C(); }
    void field25_c(std::int32_t value) { Field25C(value); }
    [[nodiscard]] std::int32_t Field260() const { return read_i32(0x210); }
    void Field260(std::int32_t value) { write_i32(0x210, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field260() const { return Field260(); }
    void field260(std::int32_t value) { Field260(value); }
    [[nodiscard]] std::int32_t Field264() const { return read_i32(0x214); }
    void Field264(std::int32_t value) { write_i32(0x214, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field264() const { return Field264(); }
    void field264(std::int32_t value) { Field264(value); }
    [[nodiscard]] std::int32_t Field268() const { return read_i32(0x218); }
    void Field268(std::int32_t value) { write_i32(0x218, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field268() const { return Field268(); }
    void field268(std::int32_t value) { Field268(value); }
    [[nodiscard]] ByteArray& Field26C() noexcept;
    [[nodiscard]] const ByteArray& Field26C() const noexcept;
    [[nodiscard]] ByteArray& field26_c() noexcept { return Field26C(); }
    [[nodiscard]] const ByteArray& field26_c() const noexcept { return Field26C(); }
    [[nodiscard]] std::int32_t Field270() const { return read_i32(0x220); }
    void Field270(std::int32_t value) { write_i32(0x220, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field270() const { return Field270(); }
    void field270(std::int32_t value) { Field270(value); }
    [[nodiscard]] std::uint8_t Field274() const { return read_u8(0x224); }
    void Field274(std::uint8_t value) { write_u8(0x224, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field274() const { return Field274(); }
    void field274(std::uint8_t value) { Field274(value); }
    [[nodiscard]] std::uint8_t Field275() const { return read_u8(0x225); }
    void Field275(std::uint8_t value) { write_u8(0x225, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field275() const { return Field275(); }
    void field275(std::uint8_t value) { Field275(value); }
    [[nodiscard]] std::uint16_t Field276() const { return read_u16(0x226); }
    void Field276(std::uint16_t value) { write_u16(0x226, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field276() const { return Field276(); }
    void field276(std::uint16_t value) { Field276(value); }
    [[nodiscard]] ByteArray& KillStreaks() noexcept;
    [[nodiscard]] const ByteArray& KillStreaks() const noexcept;
    [[nodiscard]] ByteArray& kill_streaks() noexcept { return KillStreaks(); }
    [[nodiscard]] const ByteArray& kill_streaks() const noexcept { return KillStreaks(); }
    [[nodiscard]] UInt16Array& Field27C() noexcept;
    [[nodiscard]] const UInt16Array& Field27C() const noexcept;
    [[nodiscard]] UInt16Array& field27_c() noexcept { return Field27C(); }
    [[nodiscard]] const UInt16Array& field27_c() const noexcept { return Field27C(); }
    [[nodiscard]] UInt16Array& Field284() noexcept;
    [[nodiscard]] const UInt16Array& Field284() const noexcept;
    [[nodiscard]] UInt16Array& field284() noexcept { return Field284(); }
    [[nodiscard]] const UInt16Array& field284() const noexcept { return Field284(); }
    [[nodiscard]] UInt16Array& Field28C() noexcept;
    [[nodiscard]] const UInt16Array& Field28C() const noexcept;
    [[nodiscard]] UInt16Array& field28_c() noexcept { return Field28C(); }
    [[nodiscard]] const UInt16Array& field28_c() const noexcept { return Field28C(); }
    [[nodiscard]] std::int32_t Field294() const { return read_i32(0x244); }
    void Field294(std::int32_t value) { write_i32(0x244, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field294() const { return Field294(); }
    void field294(std::int32_t value) { Field294(value); }
    [[nodiscard]] std::int32_t Frames() const { return read_i32(0x248); }
    void Frames(std::int32_t value) { write_i32(0x248, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t frames() const { return Frames(); }
    void frames(std::int32_t value) { Frames(value); }
    [[nodiscard]] std::int32_t Field29C() const { return read_i32(0x24C); }
    void Field29C(std::int32_t value) { write_i32(0x24C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field29_c() const { return Field29C(); }
    void field29_c(std::int32_t value) { Field29C(value); }
    [[nodiscard]] std::int32_t Field2A0() const { return read_i32(0x250); }
    void Field2A0(std::int32_t value) { write_i32(0x250, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_a0() const { return Field2A0(); }
    void field2_a0(std::int32_t value) { Field2A0(value); }
    [[nodiscard]] std::int32_t Field2A4() const { return read_i32(0x254); }
    void Field2A4(std::int32_t value) { write_i32(0x254, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_a4() const { return Field2A4(); }
    void field2_a4(std::int32_t value) { Field2A4(value); }
    [[nodiscard]] std::int32_t Field2A8() const { return read_i32(0x258); }
    void Field2A8(std::int32_t value) { write_i32(0x258, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_a8() const { return Field2A8(); }
    void field2_a8(std::int32_t value) { Field2A8(value); }
    [[nodiscard]] std::int32_t Field2AC() const { return read_i32(0x25C); }
    void Field2AC(std::int32_t value) { write_i32(0x25C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_a_c() const { return Field2AC(); }
    void field2_a_c(std::int32_t value) { Field2AC(value); }
    [[nodiscard]] std::uint8_t LayerId() const { return read_u8(0x260); }
    void LayerId(std::uint8_t value) { write_u8(0x260, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t layer_id() const { return LayerId(); }
    void layer_id(std::uint8_t value) { LayerId(value); }
    [[nodiscard]] std::uint8_t Field2B1() const { return read_u8(0x261); }
    void Field2B1(std::uint8_t value) { write_u8(0x261, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field2_b1() const { return Field2B1(); }
    void field2_b1(std::uint8_t value) { Field2B1(value); }
    [[nodiscard]] std::uint16_t Field2B2() const { return read_u16(0x262); }
    void Field2B2(std::uint16_t value) { write_u16(0x262, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field2_b2() const { return Field2B2(); }
    void field2_b2(std::uint16_t value) { Field2B2(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
    static constexpr std::size_t _off3 = 0x3;
    static constexpr std::size_t _off4 = 0x4;
    static constexpr std::size_t _off5 = 0x5;
    static constexpr std::size_t _off6 = 0x6;
    static constexpr std::size_t _off7 = 0xA;
    static constexpr std::size_t _off8 = 0xB;
    static constexpr std::size_t _off9 = 0xC;
    static constexpr std::size_t _off10 = 0xE;
    static constexpr std::size_t _off11 = 0x10;
    static constexpr std::size_t _off12 = 0x12;
    static constexpr std::size_t _off13 = 0x14;
    static constexpr std::size_t _off14 = 0x18;
    static constexpr std::size_t _off15 = 0x1C;
    static constexpr std::size_t _off16 = 0x20;
    static constexpr std::size_t _off17 = 0x30;
    static constexpr std::size_t _off18 = 0x34;
    static constexpr std::size_t _off19 = 0x38;
    static constexpr std::size_t _off20 = 0x3C;
    static constexpr std::size_t _off21 = 0x40;
    static constexpr std::size_t _off22 = 0x44;
    static constexpr std::size_t _off23 = 0x48;
    static constexpr std::size_t _off24 = 0x4C;
    static constexpr std::size_t _off25 = 0x50;
    static constexpr std::size_t _off26 = 0x58;
    static constexpr std::size_t _off27 = 0x59;
    static constexpr std::size_t _off28 = 0x5A;
    static constexpr std::size_t _off29 = 0x5B;
    static constexpr std::size_t _off30 = 0x5C;
    static constexpr std::size_t _off31 = 0x6C;
    static constexpr std::size_t _off32 = 0x7C;
    static constexpr std::size_t _off33 = 0x80;
    static constexpr std::size_t _off34 = 0x84;
    static constexpr std::size_t _off35 = 0x88;
    static constexpr std::size_t _off36 = 0x8C;
    static constexpr std::size_t _off37 = 0x90;
    static constexpr std::size_t _off38 = 0x94;
    static constexpr std::size_t _off39 = 0x98;
    static constexpr std::size_t _off40 = 0x9C;
    static constexpr std::size_t _off41 = 0xA0;
    static constexpr std::size_t _off42 = 0xA4;
    static constexpr std::size_t _off43 = 0xA8;
    static constexpr std::size_t _off44 = 0xAC;
    static constexpr std::size_t _off45 = 0xB0;
    static constexpr std::size_t _off46 = 0xB4;
    static constexpr std::size_t _off47 = 0xB8;
    static constexpr std::size_t _off48 = 0xBC;
    static constexpr std::size_t _off49 = 0xC0;
    static constexpr std::size_t _off50 = 0xC4;
    static constexpr std::size_t _off51 = 0xC8;
    static constexpr std::size_t _off52 = 0xCC;
    static constexpr std::size_t _off53 = 0xD0;
    static constexpr std::size_t _off54 = 0xD4;
    static constexpr std::size_t _off55 = 0xD8;
    static constexpr std::size_t _off56 = 0xDC;
    static constexpr std::size_t _off57 = 0xE0;
    static constexpr std::size_t _off58 = 0xE4;
    static constexpr std::size_t _off59 = 0xE8;
    static constexpr std::size_t _off60 = 0xEC;
    static constexpr std::size_t _off61 = 0xF0;
    static constexpr std::size_t _off62 = 0xF4;
    static constexpr std::size_t _off63 = 0xF8;
    static constexpr std::size_t _off64 = 0xFC;
    static constexpr std::size_t _off65 = 0x100;
    static constexpr std::size_t _off66 = 0x104;
    static constexpr std::size_t _off67 = 0x108;
    static constexpr std::size_t _off68 = 0x10C;
    static constexpr std::size_t _off69 = 0x110;
    static constexpr std::size_t _off70 = 0x114;
    static constexpr std::size_t _off71 = 0x118;
    static constexpr std::size_t _off72 = 0x11C;
    static constexpr std::size_t _off73 = 0x120;
    static constexpr std::size_t _off74 = 0x124;
    static constexpr std::size_t _off75 = 0x128;
    static constexpr std::size_t _off76 = 0x12C;
    static constexpr std::size_t _off77 = 0x13C;
    static constexpr std::size_t _off78 = 0x140;
    static constexpr std::size_t _off79 = 0x144;
    static constexpr std::size_t _off80 = 0x148;
    static constexpr std::size_t _off81 = 0x14C;
    static constexpr std::size_t _off82 = 0x15C;
    static constexpr std::size_t _off83 = 0x16C;
    static constexpr std::size_t _off84 = 0x17C;
    static constexpr std::size_t _off85 = 0x18C;
    static constexpr std::size_t _off86 = 0x19C;
    static constexpr std::size_t _off87 = 0x1A0;
    static constexpr std::size_t _off88 = 0x1A4;
    static constexpr std::size_t _off89 = 0x1A8;
    static constexpr std::size_t _off90 = 0x1AC;
    static constexpr std::size_t _off91 = 0x1BC;
    static constexpr std::size_t _off92 = 0x1CC;
    static constexpr std::size_t _off93 = 0x1DC;
    static constexpr std::size_t _off94 = 0x1EC;
    static constexpr std::size_t _off95 = 0x1FC;
    static constexpr std::size_t _off96 = 0x20C;
    static constexpr std::size_t _off97 = 0x210;
    static constexpr std::size_t _off98 = 0x214;
    static constexpr std::size_t _off99 = 0x218;
    static constexpr std::size_t _off100 = 0x21C;
    static constexpr std::size_t _off101 = 0x220;
    static constexpr std::size_t _off102 = 0x224;
    static constexpr std::size_t _off103 = 0x225;
    static constexpr std::size_t _off104 = 0x226;
    static constexpr std::size_t _off105 = 0x228;
    static constexpr std::size_t _off106 = 0x22C;
    static constexpr std::size_t _off107 = 0x234;
    static constexpr std::size_t _off108 = 0x23C;
    static constexpr std::size_t _off109 = 0x244;
    static constexpr std::size_t _off110 = 0x248;
    static constexpr std::size_t _off111 = 0x24C;
    static constexpr std::size_t _off112 = 0x250;
    static constexpr std::size_t _off113 = 0x254;
    static constexpr std::size_t _off114 = 0x258;
    static constexpr std::size_t _off115 = 0x25C;
    static constexpr std::size_t _off116 = 0x260;
    static constexpr std::size_t _off117 = 0x261;
    static constexpr std::size_t _off118 = 0x262;
    std::unique_ptr<ByteArray> field6_;
    std::unique_ptr<Int32Array> sensitivity_;
    std::unique_ptr<ByteArray> invert_something_;
    std::unique_ptr<ByteArray> field34_;
    std::unique_ptr<ByteArray> hunters_;
    std::unique_ptr<ByteArray> suit_colors_;
    std::unique_ptr<ByteArray> player_names_;
    std::unique_ptr<ByteArray> bot_encounter_state_;
    std::unique_ptr<ByteArray> team_ids_;
    std::unique_ptr<ByteArray> field9_c_;
    std::unique_ptr<UInt16Array> bot_spawner_ent_ids_;
    std::unique_ptr<Int32Array> prime_time_;
    std::unique_ptr<Int32Array> field_b_c_;
    std::unique_ptr<Int32Array> field17_c_;
    std::unique_ptr<Int32Array> field19_c_;
    std::unique_ptr<Int32Array> deaths_;
    std::unique_ptr<Int32Array> field1_b_c_;
    std::unique_ptr<Int32Array> field1_c_c_;
    std::unique_ptr<Int32Array> suicides_maybe_;
    std::unique_ptr<Int32Array> field1_f_c_;
    std::unique_ptr<Int32Array> headshots_maybe_;
    std::unique_ptr<Int32Array> field21_c_;
    std::unique_ptr<Int32Array> dmg_dealt_;
    std::unique_ptr<Int32Array> dmg_max_;
    std::unique_ptr<Int32Array> battle_points_;
    std::unique_ptr<ByteArray> field26_c_;
    std::unique_ptr<ByteArray> kill_streaks_;
    std::unique_ptr<UInt16Array> field27_c_;
    std::unique_ptr<UInt16Array> field284_;
    std::unique_ptr<UInt16Array> field28_c_;
};

class RoomState : public MemoryClass {
public:
    RoomState(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    RoomState(Buffer& buffer, std::uint32_t address);
    ~RoomState() override;

    [[nodiscard]] ByteArray& Bits() noexcept;
    [[nodiscard]] const ByteArray& Bits() const noexcept;
    [[nodiscard]] ByteArray& bits() noexcept { return Bits(); }
    [[nodiscard]] const ByteArray& bits() const noexcept { return Bits(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    std::unique_ptr<ByteArray> bits_;
};

class StorySaveData : public MemoryClass {
public:
    StorySaveData(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    StorySaveData(Buffer& buffer, std::uint32_t address);
    ~StorySaveData() override;

    [[nodiscard]] std::uint16_t Weapons() const { return read_u16(0x0); }
    void Weapons(std::uint16_t value) { write_u16(0x0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t weapons() const { return Weapons(); }
    void weapons(std::uint16_t value) { Weapons(value); }
    [[nodiscard]] ByteArray& WeaponSlots() noexcept;
    [[nodiscard]] const ByteArray& WeaponSlots() const noexcept;
    [[nodiscard]] ByteArray& weapon_slots() noexcept { return WeaponSlots(); }
    [[nodiscard]] const ByteArray& weapon_slots() const noexcept { return WeaponSlots(); }
    [[nodiscard]] std::uint8_t Padding5() const { return read_u8(0x5); }
    void Padding5(std::uint8_t value) { write_u8(0x5, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding5() const { return Padding5(); }
    void padding5(std::uint8_t value) { Padding5(value); }
    [[nodiscard]] UInt16Array& Ammo() noexcept;
    [[nodiscard]] const UInt16Array& Ammo() const noexcept;
    [[nodiscard]] UInt16Array& ammo() noexcept { return Ammo(); }
    [[nodiscard]] const UInt16Array& ammo() const noexcept { return Ammo(); }
    [[nodiscard]] UInt16Array& AmmoCaps() noexcept;
    [[nodiscard]] const UInt16Array& AmmoCaps() const noexcept;
    [[nodiscard]] UInt16Array& ammo_caps() noexcept { return AmmoCaps(); }
    [[nodiscard]] const UInt16Array& ammo_caps() const noexcept { return AmmoCaps(); }
    [[nodiscard]] std::uint16_t Energy() const { return read_u16(0xE); }
    void Energy(std::uint16_t value) { write_u16(0xE, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t energy() const { return Energy(); }
    void energy(std::uint16_t value) { Energy(value); }
    [[nodiscard]] std::uint16_t EnergyCap() const { return read_u16(0x10); }
    void EnergyCap(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t energy_cap() const { return EnergyCap(); }
    void energy_cap(std::uint16_t value) { EnergyCap(value); }
    [[nodiscard]] std::uint16_t GameFlags() const { return read_u16(0x12); }
    void GameFlags(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t game_flags() const { return GameFlags(); }
    void game_flags(std::uint16_t value) { GameFlags(value); }
    [[nodiscard]] std::uint16_t Field14() const { return read_u16(0x14); }
    void Field14(std::uint16_t value) { write_u16(0x14, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field14() const { return Field14(); }
    void field14(std::uint16_t value) { Field14(value); }
    [[nodiscard]] std::uint16_t LastCheckpoint() const { return read_u16(0x16); }
    void LastCheckpoint(std::uint16_t value) { write_u16(0x16, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t last_checkpoint() const { return LastCheckpoint(); }
    void last_checkpoint(std::uint16_t value) { LastCheckpoint(value); }
    [[nodiscard]] std::uint32_t Bosses() const { return read_u32(0x18); }
    void Bosses(std::uint32_t value) { write_u32(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t bosses() const { return Bosses(); }
    void bosses(std::uint32_t value) { Bosses(value); }
    [[nodiscard]] std::uint32_t Artifacts() const { return read_u32(0x1C); }
    void Artifacts(std::uint32_t value) { write_u32(0x1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t artifacts() const { return Artifacts(); }
    void artifacts(std::uint32_t value) { Artifacts(value); }
    [[nodiscard]] std::uint32_t LostOctos() const { return read_u32(0x20); }
    void LostOctos(std::uint32_t value) { write_u32(0x20, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t lost_octos() const { return LostOctos(); }
    void lost_octos(std::uint32_t value) { LostOctos(value); }
    [[nodiscard]] std::uint8_t CurOctos() const { return read_u8(0x24); }
    void CurOctos(std::uint8_t value) { write_u8(0x24, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t cur_octos() const { return CurOctos(); }
    void cur_octos(std::uint8_t value) { CurOctos(value); }
    [[nodiscard]] std::uint8_t OwnOctos() const { return read_u8(0x25); }
    void OwnOctos(std::uint8_t value) { write_u8(0x25, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t own_octos() const { return OwnOctos(); }
    void own_octos(std::uint8_t value) { OwnOctos(value); }
    [[nodiscard]] std::uint8_t FoundOctos() const { return read_u8(0x26); }
    void FoundOctos(std::uint8_t value) { write_u8(0x26, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t found_octos() const { return FoundOctos(); }
    void found_octos(std::uint8_t value) { FoundOctos(value); }
    [[nodiscard]] ByteArray& VisitedRooms() noexcept;
    [[nodiscard]] const ByteArray& VisitedRooms() const noexcept;
    [[nodiscard]] ByteArray& visited_rooms() noexcept { return VisitedRooms(); }
    [[nodiscard]] const ByteArray& visited_rooms() const noexcept { return VisitedRooms(); }
    [[nodiscard]] Int32Array& VisitedConnectors() noexcept;
    [[nodiscard]] const Int32Array& VisitedConnectors() const noexcept;
    [[nodiscard]] Int32Array& visited_connectors() noexcept { return VisitedConnectors(); }
    [[nodiscard]] const Int32Array& visited_connectors() const noexcept { return VisitedConnectors(); }
    [[nodiscard]] StructArray<::fruityprime::memory::RoomState>& RoomState() noexcept;
    [[nodiscard]] const StructArray<::fruityprime::memory::RoomState>& RoomState() const noexcept;
    [[nodiscard]] StructArray<::fruityprime::memory::RoomState>& room_state() noexcept { return RoomState(); }
    [[nodiscard]] const StructArray<::fruityprime::memory::RoomState>& room_state() const noexcept { return RoomState(); }
    [[nodiscard]] ByteArray& FieldFCC() noexcept;
    [[nodiscard]] const ByteArray& FieldFCC() const noexcept;
    [[nodiscard]] ByteArray& field_f_c_c() noexcept { return FieldFCC(); }
    [[nodiscard]] const ByteArray& field_f_c_c() const noexcept { return FieldFCC(); }
    [[nodiscard]] std::int32_t FieldFD4() const { return read_i32(0xFD4); }
    void FieldFD4(std::int32_t value) { write_i32(0xFD4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_d4() const { return FieldFD4(); }
    void field_f_d4(std::int32_t value) { FieldFD4(value); }
    [[nodiscard]] std::int32_t FieldFD8() const { return read_i32(0xFD8); }
    void FieldFD8(std::int32_t value) { write_i32(0xFD8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_d8() const { return FieldFD8(); }
    void field_f_d8(std::int32_t value) { FieldFD8(value); }
    [[nodiscard]] std::int32_t FieldFDC() const { return read_i32(0xFDC); }
    void FieldFDC(std::int32_t value) { write_i32(0xFDC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_d_c() const { return FieldFDC(); }
    void field_f_d_c(std::int32_t value) { FieldFDC(value); }
    [[nodiscard]] std::int32_t FieldFE0() const { return read_i32(0xFE0); }
    void FieldFE0(std::int32_t value) { write_i32(0xFE0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_e0() const { return FieldFE0(); }
    void field_f_e0(std::int32_t value) { FieldFE0(value); }
    [[nodiscard]] std::int32_t FieldFE4() const { return read_i32(0xFE4); }
    void FieldFE4(std::int32_t value) { write_i32(0xFE4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_e4() const { return FieldFE4(); }
    void field_f_e4(std::int32_t value) { FieldFE4(value); }
    [[nodiscard]] std::int32_t FieldFE8() const { return read_i32(0xFE8); }
    void FieldFE8(std::int32_t value) { write_i32(0xFE8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_e8() const { return FieldFE8(); }
    void field_f_e8(std::int32_t value) { FieldFE8(value); }
    [[nodiscard]] std::int32_t FieldFEC() const { return read_i32(0xFEC); }
    void FieldFEC(std::int32_t value) { write_i32(0xFEC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_e_c() const { return FieldFEC(); }
    void field_f_e_c(std::int32_t value) { FieldFEC(value); }
    [[nodiscard]] std::int32_t FieldFF0() const { return read_i32(0xFF0); }
    void FieldFF0(std::int32_t value) { write_i32(0xFF0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_f0() const { return FieldFF0(); }
    void field_f_f0(std::int32_t value) { FieldFF0(value); }
    [[nodiscard]] std::int32_t FieldFF4() const { return read_i32(0xFF4); }
    void FieldFF4(std::int32_t value) { write_i32(0xFF4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_f4() const { return FieldFF4(); }
    void field_f_f4(std::int32_t value) { FieldFF4(value); }
    [[nodiscard]] std::int32_t FieldFF8() const { return read_i32(0xFF8); }
    void FieldFF8(std::int32_t value) { write_i32(0xFF8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_f8() const { return FieldFF8(); }
    void field_f_f8(std::int32_t value) { FieldFF8(value); }
    [[nodiscard]] std::int32_t FieldFFC() const { return read_i32(0xFFC); }
    void FieldFFC(std::int32_t value) { write_i32(0xFFC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_f_f_c() const { return FieldFFC(); }
    void field_f_f_c(std::int32_t value) { FieldFFC(value); }
    [[nodiscard]] std::int32_t Field1000() const { return read_i32(0x1000); }
    void Field1000(std::int32_t value) { write_i32(0x1000, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1000() const { return Field1000(); }
    void field1000(std::int32_t value) { Field1000(value); }
    [[nodiscard]] std::int32_t Field1004() const { return read_i32(0x1004); }
    void Field1004(std::int32_t value) { write_i32(0x1004, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1004() const { return Field1004(); }
    void field1004(std::int32_t value) { Field1004(value); }
    [[nodiscard]] std::int32_t Field1008() const { return read_i32(0x1008); }
    void Field1008(std::int32_t value) { write_i32(0x1008, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1008() const { return Field1008(); }
    void field1008(std::int32_t value) { Field1008(value); }
    [[nodiscard]] std::int32_t Field100C() const { return read_i32(0x100C); }
    void Field100C(std::int32_t value) { write_i32(0x100C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field100_c() const { return Field100C(); }
    void field100_c(std::int32_t value) { Field100C(value); }
    [[nodiscard]] std::int32_t Field1010() const { return read_i32(0x1010); }
    void Field1010(std::int32_t value) { write_i32(0x1010, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1010() const { return Field1010(); }
    void field1010(std::int32_t value) { Field1010(value); }
    [[nodiscard]] ByteArray& TriggerStateBits() noexcept;
    [[nodiscard]] const ByteArray& TriggerStateBits() const noexcept;
    [[nodiscard]] ByteArray& trigger_state_bits() noexcept { return TriggerStateBits(); }
    [[nodiscard]] const ByteArray& trigger_state_bits() const noexcept { return TriggerStateBits(); }
    [[nodiscard]] std::int32_t Field1018() const { return read_i32(0x1018); }
    void Field1018(std::int32_t value) { write_i32(0x1018, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1018() const { return Field1018(); }
    void field1018(std::int32_t value) { Field1018(value); }
    [[nodiscard]] ByteArray& Logbook() noexcept;
    [[nodiscard]] const ByteArray& Logbook() const noexcept;
    [[nodiscard]] ByteArray& logbook() noexcept { return Logbook(); }
    [[nodiscard]] const ByteArray& logbook() const noexcept { return Logbook(); }
    [[nodiscard]] std::int32_t Field105C() const { return read_i32(0x105C); }
    void Field105C(std::int32_t value) { write_i32(0x105C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field105_c() const { return Field105C(); }
    void field105_c(std::int32_t value) { Field105C(value); }
    [[nodiscard]] std::int32_t Field1060() const { return read_i32(0x1060); }
    void Field1060(std::int32_t value) { write_i32(0x1060, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1060() const { return Field1060(); }
    void field1060(std::int32_t value) { Field1060(value); }
    [[nodiscard]] std::int32_t Field1064() const { return read_i32(0x1064); }
    void Field1064(std::int32_t value) { write_i32(0x1064, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1064() const { return Field1064(); }
    void field1064(std::int32_t value) { Field1064(value); }
    [[nodiscard]] std::int32_t Field1068() const { return read_i32(0x1068); }
    void Field1068(std::int32_t value) { write_i32(0x1068, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1068() const { return Field1068(); }
    void field1068(std::int32_t value) { Field1068(value); }
    [[nodiscard]] std::int32_t Field106C() const { return read_i32(0x106C); }
    void Field106C(std::int32_t value) { write_i32(0x106C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field106_c() const { return Field106C(); }
    void field106_c(std::int32_t value) { Field106C(value); }
    [[nodiscard]] std::int32_t Field1070() const { return read_i32(0x1070); }
    void Field1070(std::int32_t value) { write_i32(0x1070, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1070() const { return Field1070(); }
    void field1070(std::int32_t value) { Field1070(value); }
    [[nodiscard]] std::int32_t Field1074() const { return read_i32(0x1074); }
    void Field1074(std::int32_t value) { write_i32(0x1074, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1074() const { return Field1074(); }
    void field1074(std::int32_t value) { Field1074(value); }
    [[nodiscard]] std::int32_t Field1078() const { return read_i32(0x1078); }
    void Field1078(std::int32_t value) { write_i32(0x1078, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1078() const { return Field1078(); }
    void field1078(std::int32_t value) { Field1078(value); }
    [[nodiscard]] std::int32_t Field107C() const { return read_i32(0x107C); }
    void Field107C(std::int32_t value) { write_i32(0x107C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field107_c() const { return Field107C(); }
    void field107_c(std::int32_t value) { Field107C(value); }
    [[nodiscard]] ByteArray& AreaHunters() noexcept;
    [[nodiscard]] const ByteArray& AreaHunters() const noexcept;
    [[nodiscard]] ByteArray& area_hunters() noexcept { return AreaHunters(); }
    [[nodiscard]] const ByteArray& area_hunters() const noexcept { return AreaHunters(); }
    [[nodiscard]] std::uint8_t Field1084() const { return read_u8(0x1084); }
    void Field1084(std::uint8_t value) { write_u8(0x1084, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t field1084() const { return Field1084(); }
    void field1084(std::uint8_t value) { Field1084(value); }
    [[nodiscard]] std::uint8_t RoomId() const { return read_u8(0x1085); }
    void RoomId(std::uint8_t value) { write_u8(0x1085, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t room_id() const { return RoomId(); }
    void room_id(std::uint8_t value) { RoomId(value); }
    [[nodiscard]] std::uint8_t SlotHunterBits() const { return read_u8(0x1086); }
    void SlotHunterBits(std::uint8_t value) { write_u8(0x1086, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t slot_hunter_bits() const { return SlotHunterBits(); }
    void slot_hunter_bits(std::uint8_t value) { SlotHunterBits(value); }
    [[nodiscard]] std::uint8_t DefeatedHunters() const { return read_u8(0x1087); }
    void DefeatedHunters(std::uint8_t value) { write_u8(0x1087, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t defeated_hunters() const { return DefeatedHunters(); }
    void defeated_hunters(std::uint8_t value) { DefeatedHunters(value); }
    [[nodiscard]] std::int32_t HunterKills() const { return read_i32(0x1088); }
    void HunterKills(std::int32_t value) { write_i32(0x1088, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t hunter_kills() const { return HunterKills(); }
    void hunter_kills(std::int32_t value) { HunterKills(value); }
    [[nodiscard]] std::int32_t DeathsFromHunter() const { return read_i32(0x108C); }
    void DeathsFromHunter(std::int32_t value) { write_i32(0x108C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t deaths_from_hunter() const { return DeathsFromHunter(); }
    void deaths_from_hunter(std::int32_t value) { DeathsFromHunter(value); }
    [[nodiscard]] std::int32_t DeathTotal() const { return read_i32(0x1090); }
    void DeathTotal(std::int32_t value) { write_i32(0x1090, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t death_total() const { return DeathTotal(); }
    void death_total(std::int32_t value) { DeathTotal(value); }
    [[nodiscard]] std::int32_t Field1094() const { return read_i32(0x1094); }
    void Field1094(std::int32_t value) { write_i32(0x1094, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1094() const { return Field1094(); }
    void field1094(std::int32_t value) { Field1094(value); }
    [[nodiscard]] std::int32_t Field1098() const { return read_i32(0x1098); }
    void Field1098(std::int32_t value) { write_i32(0x1098, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1098() const { return Field1098(); }
    void field1098(std::int32_t value) { Field1098(value); }
    [[nodiscard]] std::int32_t Field109C() const { return read_i32(0x109C); }
    void Field109C(std::int32_t value) { write_i32(0x109C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field109_c() const { return Field109C(); }
    void field109_c(std::int32_t value) { Field109C(value); }
    [[nodiscard]] std::int32_t Field10A0() const { return read_i32(0x10A0); }
    void Field10A0(std::int32_t value) { write_i32(0x10A0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10_a0() const { return Field10A0(); }
    void field10_a0(std::int32_t value) { Field10A0(value); }
    [[nodiscard]] std::int32_t ScanCount() const { return read_i32(0x10A4); }
    void ScanCount(std::int32_t value) { write_i32(0x10A4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t scan_count() const { return ScanCount(); }
    void scan_count(std::int32_t value) { ScanCount(value); }
    [[nodiscard]] std::int32_t EquipData() const { return read_i32(0x10A8); }
    void EquipData(std::int32_t value) { write_i32(0x10A8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t equip_data() const { return EquipData(); }
    void equip_data(std::int32_t value) { EquipData(value); }
    [[nodiscard]] std::uint32_t MaxScanCount() const { return read_u32(0x10AC); }
    void MaxScanCount(std::uint32_t value) { write_u32(0x10AC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t max_scan_count() const { return MaxScanCount(); }
    void max_scan_count(std::uint32_t value) { MaxScanCount(value); }
    [[nodiscard]] std::int32_t MaxEquipData() const { return read_i32(0x10B0); }
    void MaxEquipData(std::int32_t value) { write_i32(0x10B0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t max_equip_data() const { return MaxEquipData(); }
    void max_equip_data(std::int32_t value) { MaxEquipData(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x2;
    static constexpr std::size_t _off2 = 0x5;
    static constexpr std::size_t _off3 = 0x6;
    static constexpr std::size_t _off4 = 0xA;
    static constexpr std::size_t _off5 = 0xE;
    static constexpr std::size_t _off6 = 0x10;
    static constexpr std::size_t _off7 = 0x12;
    static constexpr std::size_t _off8 = 0x14;
    static constexpr std::size_t _off9 = 0x16;
    static constexpr std::size_t _off10 = 0x18;
    static constexpr std::size_t _off11 = 0x1C;
    static constexpr std::size_t _off12 = 0x20;
    static constexpr std::size_t _off13 = 0x24;
    static constexpr std::size_t _off14 = 0x25;
    static constexpr std::size_t _off15 = 0x26;
    static constexpr std::size_t _off16 = 0x27;
    static constexpr std::size_t _off17 = 0x30;
    static constexpr std::size_t _off18 = 0x54;
    static constexpr std::size_t _off19 = 0xFCC;
    static constexpr std::size_t _off20 = 0xFD4;
    static constexpr std::size_t _off21 = 0xFD8;
    static constexpr std::size_t _off22 = 0xFDC;
    static constexpr std::size_t _off23 = 0xFE0;
    static constexpr std::size_t _off24 = 0xFE4;
    static constexpr std::size_t _off25 = 0xFE8;
    static constexpr std::size_t _off26 = 0xFEC;
    static constexpr std::size_t _off27 = 0xFF0;
    static constexpr std::size_t _off28 = 0xFF4;
    static constexpr std::size_t _off29 = 0xFF8;
    static constexpr std::size_t _off30 = 0xFFC;
    static constexpr std::size_t _off31 = 0x1000;
    static constexpr std::size_t _off32 = 0x1004;
    static constexpr std::size_t _off33 = 0x1008;
    static constexpr std::size_t _off34 = 0x100C;
    static constexpr std::size_t _off35 = 0x1010;
    static constexpr std::size_t _off36 = 0x1014;
    static constexpr std::size_t _off37 = 0x1018;
    static constexpr std::size_t _off38 = 0x101C;
    static constexpr std::size_t _off39 = 0x105C;
    static constexpr std::size_t _off40 = 0x1060;
    static constexpr std::size_t _off41 = 0x1064;
    static constexpr std::size_t _off42 = 0x1068;
    static constexpr std::size_t _off43 = 0x106C;
    static constexpr std::size_t _off44 = 0x1070;
    static constexpr std::size_t _off45 = 0x1074;
    static constexpr std::size_t _off46 = 0x1078;
    static constexpr std::size_t _off47 = 0x107C;
    static constexpr std::size_t _off48 = 0x1080;
    static constexpr std::size_t _off49 = 0x1084;
    static constexpr std::size_t _off50 = 0x1085;
    static constexpr std::size_t _off51 = 0x1086;
    static constexpr std::size_t _off52 = 0x1087;
    static constexpr std::size_t _off53 = 0x1088;
    static constexpr std::size_t _off54 = 0x108C;
    static constexpr std::size_t _off55 = 0x1090;
    static constexpr std::size_t _off56 = 0x1094;
    static constexpr std::size_t _off57 = 0x1098;
    static constexpr std::size_t _off58 = 0x109C;
    static constexpr std::size_t _off59 = 0x10A0;
    static constexpr std::size_t _off60 = 0x10A4;
    static constexpr std::size_t _off61 = 0x10A8;
    static constexpr std::size_t _off62 = 0x10AC;
    static constexpr std::size_t _off63 = 0x10B0;
    std::unique_ptr<ByteArray> weapon_slots_;
    std::unique_ptr<UInt16Array> ammo_;
    std::unique_ptr<UInt16Array> ammo_caps_;
    std::unique_ptr<ByteArray> visited_rooms_;
    std::unique_ptr<Int32Array> visited_connectors_;
    std::unique_ptr<StructArray<::fruityprime::memory::RoomState>> room_state_;
    std::unique_ptr<ByteArray> field_f_c_c_;
    std::unique_ptr<ByteArray> trigger_state_bits_;
    std::unique_ptr<ByteArray> logbook_;
    std::unique_ptr<ByteArray> area_hunters_;
};

class SaveType3 : public MemoryClass {
public:
    SaveType3(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    SaveType3(Buffer& buffer, std::uint32_t address);
    ~SaveType3() override;

    [[nodiscard]] std::int32_t Field0() const { return read_i32(0x0); }
    void Field0(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field0() const { return Field0(); }
    void field0(std::int32_t value) { Field0(value); }
    [[nodiscard]] std::int32_t Field4() const { return read_i32(0x4); }
    void Field4(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4() const { return Field4(); }
    void field4(std::int32_t value) { Field4(value); }
    [[nodiscard]] std::int32_t Field8() const { return read_i32(0x8); }
    void Field8(std::int32_t value) { write_i32(0x8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field8() const { return Field8(); }
    void field8(std::int32_t value) { Field8(value); }
    [[nodiscard]] std::int32_t FieldC() const { return read_i32(0xC); }
    void FieldC(std::int32_t value) { write_i32(0xC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c() const { return FieldC(); }
    void field_c(std::int32_t value) { FieldC(value); }
    [[nodiscard]] std::int32_t Field10() const { return read_i32(0x10); }
    void Field10(std::int32_t value) { write_i32(0x10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10() const { return Field10(); }
    void field10(std::int32_t value) { Field10(value); }
    [[nodiscard]] std::int32_t Field14() const { return read_i32(0x14); }
    void Field14(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field14() const { return Field14(); }
    void field14(std::int32_t value) { Field14(value); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
    static constexpr std::size_t _off6 = 0x18;
    static constexpr std::size_t _off7 = 0x1C;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x24;
    static constexpr std::size_t _off10 = 0x28;
    static constexpr std::size_t _off11 = 0x2C;
    static constexpr std::size_t _off12 = 0x30;
    static constexpr std::size_t _off13 = 0x34;
    static constexpr std::size_t _off14 = 0x38;
    static constexpr std::size_t _off15 = 0x3C;
};

class StatsAndSettings : public MemoryClass {
public:
    StatsAndSettings(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    StatsAndSettings(Buffer& buffer, std::uint32_t address);
    ~StatsAndSettings() override;

    [[nodiscard]] std::int32_t Field0() const { return read_i32(0x0); }
    void Field0(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field0() const { return Field0(); }
    void field0(std::int32_t value) { Field0(value); }
    [[nodiscard]] std::int32_t AreaBits() const { return read_i32(0x4); }
    void AreaBits(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t area_bits() const { return AreaBits(); }
    void area_bits(std::int32_t value) { AreaBits(value); }
    [[nodiscard]] std::int32_t MultiplayerCharacters() const { return read_i32(0x8); }
    void MultiplayerCharacters(std::int32_t value) { write_i32(0x8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t multiplayer_characters() const { return MultiplayerCharacters(); }
    void multiplayer_characters(std::int32_t value) { MultiplayerCharacters(value); }
    [[nodiscard]] std::int32_t FieldC() const { return read_i32(0xC); }
    void FieldC(std::int32_t value) { write_i32(0xC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field_c() const { return FieldC(); }
    void field_c(std::int32_t value) { FieldC(value); }
    [[nodiscard]] std::int32_t Field10() const { return read_i32(0x10); }
    void Field10(std::int32_t value) { write_i32(0x10, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field10() const { return Field10(); }
    void field10(std::int32_t value) { Field10(value); }
    [[nodiscard]] std::int32_t TouchpadSensitivity() const { return read_i32(0x14); }
    void TouchpadSensitivity(std::int32_t value) { write_i32(0x14, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t touchpad_sensitivity() const { return TouchpadSensitivity(); }
    void touchpad_sensitivity(std::int32_t value) { TouchpadSensitivity(value); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::int32_t Field1C() const { return read_i32(0x1C); }
    void Field1C(std::int32_t value) { write_i32(0x1C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_c() const { return Field1C(); }
    void field1_c(std::int32_t value) { Field1C(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t Field38() const { return read_i32(0x38); }
    void Field38(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field38() const { return Field38(); }
    void field38(std::int32_t value) { Field38(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] std::int32_t Field40() const { return read_i32(0x40); }
    void Field40(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field40() const { return Field40(); }
    void field40(std::int32_t value) { Field40(value); }
    [[nodiscard]] std::int32_t Field44() const { return read_i32(0x44); }
    void Field44(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field44() const { return Field44(); }
    void field44(std::int32_t value) { Field44(value); }
    [[nodiscard]] std::int32_t Field48() const { return read_i32(0x48); }
    void Field48(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field48() const { return Field48(); }
    void field48(std::int32_t value) { Field48(value); }
    [[nodiscard]] std::int32_t Field4C() const { return read_i32(0x4C); }
    void Field4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_c() const { return Field4C(); }
    void field4_c(std::int32_t value) { Field4C(value); }
    [[nodiscard]] std::int32_t Field50() const { return read_i32(0x50); }
    void Field50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field50() const { return Field50(); }
    void field50(std::int32_t value) { Field50(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] std::int32_t Field58() const { return read_i32(0x58); }
    void Field58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field58() const { return Field58(); }
    void field58(std::int32_t value) { Field58(value); }
    [[nodiscard]] std::int32_t Field5C() const { return read_i32(0x5C); }
    void Field5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field5_c() const { return Field5C(); }
    void field5_c(std::int32_t value) { Field5C(value); }
    [[nodiscard]] std::int32_t Field60() const { return read_i32(0x60); }
    void Field60(std::int32_t value) { write_i32(0x60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field60() const { return Field60(); }
    void field60(std::int32_t value) { Field60(value); }
    [[nodiscard]] std::int32_t Field64() const { return read_i32(0x64); }
    void Field64(std::int32_t value) { write_i32(0x64, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field64() const { return Field64(); }
    void field64(std::int32_t value) { Field64(value); }
    [[nodiscard]] std::int32_t Field68() const { return read_i32(0x68); }
    void Field68(std::int32_t value) { write_i32(0x68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field68() const { return Field68(); }
    void field68(std::int32_t value) { Field68(value); }
    [[nodiscard]] std::int32_t Field6C() const { return read_i32(0x6C); }
    void Field6C(std::int32_t value) { write_i32(0x6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field6_c() const { return Field6C(); }
    void field6_c(std::int32_t value) { Field6C(value); }
    [[nodiscard]] std::int32_t Field70() const { return read_i32(0x70); }
    void Field70(std::int32_t value) { write_i32(0x70, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field70() const { return Field70(); }
    void field70(std::int32_t value) { Field70(value); }
    [[nodiscard]] std::int32_t Field74() const { return read_i32(0x74); }
    void Field74(std::int32_t value) { write_i32(0x74, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field74() const { return Field74(); }
    void field74(std::int32_t value) { Field74(value); }
    [[nodiscard]] std::int32_t Field78() const { return read_i32(0x78); }
    void Field78(std::int32_t value) { write_i32(0x78, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field78() const { return Field78(); }
    void field78(std::int32_t value) { Field78(value); }
    [[nodiscard]] std::int32_t Field7C() const { return read_i32(0x7C); }
    void Field7C(std::int32_t value) { write_i32(0x7C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field7_c() const { return Field7C(); }
    void field7_c(std::int32_t value) { Field7C(value); }
    [[nodiscard]] std::int32_t Field80() const { return read_i32(0x80); }
    void Field80(std::int32_t value) { write_i32(0x80, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field80() const { return Field80(); }
    void field80(std::int32_t value) { Field80(value); }
    [[nodiscard]] std::int32_t Field84() const { return read_i32(0x84); }
    void Field84(std::int32_t value) { write_i32(0x84, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field84() const { return Field84(); }
    void field84(std::int32_t value) { Field84(value); }
    [[nodiscard]] std::int32_t Field88() const { return read_i32(0x88); }
    void Field88(std::int32_t value) { write_i32(0x88, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field88() const { return Field88(); }
    void field88(std::int32_t value) { Field88(value); }
    [[nodiscard]] std::int32_t Field8C() const { return read_i32(0x8C); }
    void Field8C(std::int32_t value) { write_i32(0x8C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field8_c() const { return Field8C(); }
    void field8_c(std::int32_t value) { Field8C(value); }
    [[nodiscard]] std::int32_t Field90() const { return read_i32(0x90); }
    void Field90(std::int32_t value) { write_i32(0x90, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field90() const { return Field90(); }
    void field90(std::int32_t value) { Field90(value); }
    [[nodiscard]] std::int32_t Field94() const { return read_i32(0x94); }
    void Field94(std::int32_t value) { write_i32(0x94, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field94() const { return Field94(); }
    void field94(std::int32_t value) { Field94(value); }
    [[nodiscard]] std::int32_t Field98() const { return read_i32(0x98); }
    void Field98(std::int32_t value) { write_i32(0x98, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field98() const { return Field98(); }
    void field98(std::int32_t value) { Field98(value); }
    [[nodiscard]] std::int32_t Field9C() const { return read_i32(0x9C); }
    void Field9C(std::int32_t value) { write_i32(0x9C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field9_c() const { return Field9C(); }
    void field9_c(std::int32_t value) { Field9C(value); }
    [[nodiscard]] std::int32_t EnemyKillsMaybe() const { return read_i32(0xA0); }
    void EnemyKillsMaybe(std::int32_t value) { write_i32(0xA0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t enemy_kills_maybe() const { return EnemyKillsMaybe(); }
    void enemy_kills_maybe(std::int32_t value) { EnemyKillsMaybe(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
    static constexpr std::size_t _off6 = 0x18;
    static constexpr std::size_t _off7 = 0x1C;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x24;
    static constexpr std::size_t _off10 = 0x28;
    static constexpr std::size_t _off11 = 0x2C;
    static constexpr std::size_t _off12 = 0x30;
    static constexpr std::size_t _off13 = 0x34;
    static constexpr std::size_t _off14 = 0x38;
    static constexpr std::size_t _off15 = 0x3C;
    static constexpr std::size_t _off16 = 0x40;
    static constexpr std::size_t _off17 = 0x44;
    static constexpr std::size_t _off18 = 0x48;
    static constexpr std::size_t _off19 = 0x4C;
    static constexpr std::size_t _off20 = 0x50;
    static constexpr std::size_t _off21 = 0x54;
    static constexpr std::size_t _off22 = 0x58;
    static constexpr std::size_t _off23 = 0x5C;
    static constexpr std::size_t _off24 = 0x60;
    static constexpr std::size_t _off25 = 0x64;
    static constexpr std::size_t _off26 = 0x68;
    static constexpr std::size_t _off27 = 0x6C;
    static constexpr std::size_t _off28 = 0x70;
    static constexpr std::size_t _off29 = 0x74;
    static constexpr std::size_t _off30 = 0x78;
    static constexpr std::size_t _off31 = 0x7C;
    static constexpr std::size_t _off32 = 0x80;
    static constexpr std::size_t _off33 = 0x84;
    static constexpr std::size_t _off34 = 0x88;
    static constexpr std::size_t _off35 = 0x8C;
    static constexpr std::size_t _off36 = 0x90;
    static constexpr std::size_t _off37 = 0x94;
    static constexpr std::size_t _off38 = 0x98;
    static constexpr std::size_t _off39 = 0x9C;
    static constexpr std::size_t _off40 = 0xA0;
};

class LicenseInfo : public MemoryClass {
public:
    LicenseInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    LicenseInfo(Buffer& buffer, std::uint32_t address);
    ~LicenseInfo() override;

    [[nodiscard]] ByteArray& Nickname() noexcept;
    [[nodiscard]] const ByteArray& Nickname() const noexcept;
    [[nodiscard]] ByteArray& nickname() noexcept { return Nickname(); }
    [[nodiscard]] const ByteArray& nickname() const noexcept { return Nickname(); }
    [[nodiscard]] std::int32_t Field18() const { return read_i32(0x18); }
    void Field18(std::int32_t value) { write_i32(0x18, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field18() const { return Field18(); }
    void field18(std::int32_t value) { Field18(value); }
    [[nodiscard]] std::uint16_t RankPoints() const { return read_u16(0x1C); }
    void RankPoints(std::uint16_t value) { write_u16(0x1C, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t rank_points() const { return RankPoints(); }
    void rank_points(std::uint16_t value) { RankPoints(value); }
    [[nodiscard]] std::uint16_t Field1E() const { return read_u16(0x1E); }
    void Field1E(std::uint16_t value) { write_u16(0x1E, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t field1_e() const { return Field1E(); }
    void field1_e(std::uint16_t value) { Field1E(value); }
    [[nodiscard]] std::int32_t Field20() const { return read_i32(0x20); }
    void Field20(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field20() const { return Field20(); }
    void field20(std::int32_t value) { Field20(value); }
    [[nodiscard]] std::int32_t Field24() const { return read_i32(0x24); }
    void Field24(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field24() const { return Field24(); }
    void field24(std::int32_t value) { Field24(value); }
    [[nodiscard]] std::int32_t Field28() const { return read_i32(0x28); }
    void Field28(std::int32_t value) { write_i32(0x28, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field28() const { return Field28(); }
    void field28(std::int32_t value) { Field28(value); }
    [[nodiscard]] std::int32_t Field2C() const { return read_i32(0x2C); }
    void Field2C(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field2_c() const { return Field2C(); }
    void field2_c(std::int32_t value) { Field2C(value); }
    [[nodiscard]] std::int32_t Field30() const { return read_i32(0x30); }
    void Field30(std::int32_t value) { write_i32(0x30, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field30() const { return Field30(); }
    void field30(std::int32_t value) { Field30(value); }
    [[nodiscard]] std::int32_t Field34() const { return read_i32(0x34); }
    void Field34(std::int32_t value) { write_i32(0x34, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field34() const { return Field34(); }
    void field34(std::int32_t value) { Field34(value); }
    [[nodiscard]] std::int32_t HeadshotCount() const { return read_i32(0x38); }
    void HeadshotCount(std::int32_t value) { write_i32(0x38, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t headshot_count() const { return HeadshotCount(); }
    void headshot_count(std::int32_t value) { HeadshotCount(value); }
    [[nodiscard]] std::int32_t Field3C() const { return read_i32(0x3C); }
    void Field3C(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field3_c() const { return Field3C(); }
    void field3_c(std::int32_t value) { Field3C(value); }
    [[nodiscard]] std::int32_t GamplayTime1() const { return read_i32(0x40); }
    void GamplayTime1(std::int32_t value) { write_i32(0x40, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t gamplay_time1() const { return GamplayTime1(); }
    void gamplay_time1(std::int32_t value) { GamplayTime1(value); }
    [[nodiscard]] std::int32_t GamplayTime2() const { return read_i32(0x44); }
    void GamplayTime2(std::int32_t value) { write_i32(0x44, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t gamplay_time2() const { return GamplayTime2(); }
    void gamplay_time2(std::int32_t value) { GamplayTime2(value); }
    [[nodiscard]] std::int32_t GamplayTime3() const { return read_i32(0x48); }
    void GamplayTime3(std::int32_t value) { write_i32(0x48, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t gamplay_time3() const { return GamplayTime3(); }
    void gamplay_time3(std::int32_t value) { GamplayTime3(value); }
    [[nodiscard]] std::int32_t Field4C() const { return read_i32(0x4C); }
    void Field4C(std::int32_t value) { write_i32(0x4C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field4_c() const { return Field4C(); }
    void field4_c(std::int32_t value) { Field4C(value); }
    [[nodiscard]] std::int32_t Field50() const { return read_i32(0x50); }
    void Field50(std::int32_t value) { write_i32(0x50, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field50() const { return Field50(); }
    void field50(std::int32_t value) { Field50(value); }
    [[nodiscard]] std::int32_t Field54() const { return read_i32(0x54); }
    void Field54(std::int32_t value) { write_i32(0x54, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field54() const { return Field54(); }
    void field54(std::int32_t value) { Field54(value); }
    [[nodiscard]] std::int32_t Field58() const { return read_i32(0x58); }
    void Field58(std::int32_t value) { write_i32(0x58, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field58() const { return Field58(); }
    void field58(std::int32_t value) { Field58(value); }
    [[nodiscard]] std::int32_t Field5C() const { return read_i32(0x5C); }
    void Field5C(std::int32_t value) { write_i32(0x5C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field5_c() const { return Field5C(); }
    void field5_c(std::int32_t value) { Field5C(value); }
    [[nodiscard]] std::int32_t Field60() const { return read_i32(0x60); }
    void Field60(std::int32_t value) { write_i32(0x60, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field60() const { return Field60(); }
    void field60(std::int32_t value) { Field60(value); }
    [[nodiscard]] Int32Array& Field64() noexcept;
    [[nodiscard]] const Int32Array& Field64() const noexcept;
    [[nodiscard]] Int32Array& field64() noexcept { return Field64(); }
    [[nodiscard]] const Int32Array& field64() const noexcept { return Field64(); }
    [[nodiscard]] Int32Array& Field74() noexcept;
    [[nodiscard]] const Int32Array& Field74() const noexcept;
    [[nodiscard]] Int32Array& field74() noexcept { return Field74(); }
    [[nodiscard]] const Int32Array& field74() const noexcept { return Field74(); }
    [[nodiscard]] Int32Array& Field90() noexcept;
    [[nodiscard]] const Int32Array& Field90() const noexcept;
    [[nodiscard]] Int32Array& field90() noexcept { return Field90(); }
    [[nodiscard]] const Int32Array& field90() const noexcept { return Field90(); }
    [[nodiscard]] Int32Array& FieldAC() noexcept;
    [[nodiscard]] const Int32Array& FieldAC() const noexcept;
    [[nodiscard]] Int32Array& field_a_c() noexcept { return FieldAC(); }
    [[nodiscard]] const Int32Array& field_a_c() const noexcept { return FieldAC(); }
    [[nodiscard]] Int32Array& FieldD0() noexcept;
    [[nodiscard]] const Int32Array& FieldD0() const noexcept;
    [[nodiscard]] Int32Array& field_d0() noexcept { return FieldD0(); }
    [[nodiscard]] const Int32Array& field_d0() const noexcept { return FieldD0(); }
    [[nodiscard]] Int32Array& Field144() noexcept;
    [[nodiscard]] const Int32Array& Field144() const noexcept;
    [[nodiscard]] Int32Array& field144() noexcept { return Field144(); }
    [[nodiscard]] const Int32Array& field144() const noexcept { return Field144(); }
    [[nodiscard]] Int32Array& Field1B8() noexcept;
    [[nodiscard]] const Int32Array& Field1B8() const noexcept;
    [[nodiscard]] Int32Array& field1_b8() noexcept { return Field1B8(); }
    [[nodiscard]] const Int32Array& field1_b8() const noexcept { return Field1B8(); }
    [[nodiscard]] std::int32_t Field1D4() const { return read_i32(0x1D4); }
    void Field1D4(std::int32_t value) { write_i32(0x1D4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_d4() const { return Field1D4(); }
    void field1_d4(std::int32_t value) { Field1D4(value); }
    [[nodiscard]] std::int32_t Field1D8() const { return read_i32(0x1D8); }
    void Field1D8(std::int32_t value) { write_i32(0x1D8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_d8() const { return Field1D8(); }
    void field1_d8(std::int32_t value) { Field1D8(value); }
    [[nodiscard]] std::int32_t Field1DC() const { return read_i32(0x1DC); }
    void Field1DC(std::int32_t value) { write_i32(0x1DC, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_d_c() const { return Field1DC(); }
    void field1_d_c(std::int32_t value) { Field1DC(value); }
    [[nodiscard]] std::int32_t Field1E0() const { return read_i32(0x1E0); }
    void Field1E0(std::int32_t value) { write_i32(0x1E0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t field1_e0() const { return Field1E0(); }
    void field1_e0(std::int32_t value) { Field1E0(value); }
    [[nodiscard]] ByteArray& Field1E4() noexcept;
    [[nodiscard]] const ByteArray& Field1E4() const noexcept;
    [[nodiscard]] ByteArray& field1_e4() noexcept { return Field1E4(); }
    [[nodiscard]] const ByteArray& field1_e4() const noexcept { return Field1E4(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x18;
    static constexpr std::size_t _off2 = 0x1C;
    static constexpr std::size_t _off3 = 0x1E;
    static constexpr std::size_t _off4 = 0x20;
    static constexpr std::size_t _off5 = 0x24;
    static constexpr std::size_t _off6 = 0x28;
    static constexpr std::size_t _off7 = 0x2C;
    static constexpr std::size_t _off8 = 0x30;
    static constexpr std::size_t _off9 = 0x34;
    static constexpr std::size_t _off10 = 0x38;
    static constexpr std::size_t _off11 = 0x3C;
    static constexpr std::size_t _off12 = 0x40;
    static constexpr std::size_t _off13 = 0x44;
    static constexpr std::size_t _off14 = 0x48;
    static constexpr std::size_t _off15 = 0x4C;
    static constexpr std::size_t _off16 = 0x50;
    static constexpr std::size_t _off17 = 0x54;
    static constexpr std::size_t _off18 = 0x58;
    static constexpr std::size_t _off19 = 0x5C;
    static constexpr std::size_t _off20 = 0x60;
    static constexpr std::size_t _off21 = 0x64;
    static constexpr std::size_t _off22 = 0x74;
    static constexpr std::size_t _off23 = 0x90;
    static constexpr std::size_t _off24 = 0xAC;
    static constexpr std::size_t _off25 = 0xD0;
    static constexpr std::size_t _off26 = 0x144;
    static constexpr std::size_t _off27 = 0x1B8;
    static constexpr std::size_t _off28 = 0x1D4;
    static constexpr std::size_t _off29 = 0x1D8;
    static constexpr std::size_t _off30 = 0x1DC;
    static constexpr std::size_t _off31 = 0x1E0;
    static constexpr std::size_t _off32 = 0x1E4;
    std::unique_ptr<ByteArray> nickname_;
    std::unique_ptr<Int32Array> field64_;
    std::unique_ptr<Int32Array> field74_;
    std::unique_ptr<Int32Array> field90_;
    std::unique_ptr<Int32Array> field_a_c_;
    std::unique_ptr<Int32Array> field_d0_;
    std::unique_ptr<Int32Array> field144_;
    std::unique_ptr<Int32Array> field1_b8_;
    std::unique_ptr<ByteArray> field1_e4_;
};

class FriendsRivals : public MemoryClass {
public:
    FriendsRivals(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    FriendsRivals(Buffer& buffer, std::uint32_t address);
    ~FriendsRivals() override;

    [[nodiscard]] Int32Array& Fields() noexcept;
    [[nodiscard]] const Int32Array& Fields() const noexcept;
    [[nodiscard]] Int32Array& fields() noexcept { return Fields(); }
    [[nodiscard]] const Int32Array& fields() const noexcept { return Fields(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    std::unique_ptr<Int32Array> fields_;
};

class RoomDescription : public MemoryClass {
public:
    RoomDescription(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    RoomDescription(Buffer& buffer, std::uint32_t address);
    ~RoomDescription() override;

    [[nodiscard]] std::uint32_t Name() const { return read_pointer(0x0); }
    void Name(std::uint32_t value) { write_pointer(0x0, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t name() const { return Name(); }
    void name(std::uint32_t value) { Name(value); }
    [[nodiscard]] std::uint32_t Model() const { return read_pointer(0x4); }
    void Model(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t model() const { return Model(); }
    void model(std::uint32_t value) { Model(value); }
    [[nodiscard]] std::uint32_t Anim() const { return read_pointer(0x8); }
    void Anim(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t anim() const { return Anim(); }
    void anim(std::uint32_t value) { Anim(value); }
    [[nodiscard]] std::uint32_t Tex() const { return read_pointer(0xC); }
    void Tex(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t tex() const { return Tex(); }
    void tex(std::uint32_t value) { Tex(value); }
    [[nodiscard]] std::uint32_t Collision() const { return read_pointer(0x10); }
    void Collision(std::uint32_t value) { write_pointer(0x10, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t collision() const { return Collision(); }
    void collision(std::uint32_t value) { Collision(value); }
    [[nodiscard]] std::uint32_t Ent() const { return read_pointer(0x14); }
    void Ent(std::uint32_t value) { write_pointer(0x14, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ent() const { return Ent(); }
    void ent(std::uint32_t value) { Ent(value); }
    [[nodiscard]] std::uint32_t Nodedata() const { return read_pointer(0x18); }
    void Nodedata(std::uint32_t value) { write_pointer(0x18, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t nodedata() const { return Nodedata(); }
    void nodedata(std::uint32_t value) { Nodedata(value); }
    [[nodiscard]] std::uint32_t RoomNodeName() const { return read_pointer(0x1C); }
    void RoomNodeName(std::uint32_t value) { write_pointer(0x1C, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t room_node_name() const { return RoomNodeName(); }
    void room_node_name(std::uint32_t value) { RoomNodeName(value); }
    [[nodiscard]] std::int32_t BattleTimeLimit() const { return read_i32(0x20); }
    void BattleTimeLimit(std::int32_t value) { write_i32(0x20, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t battle_time_limit() const { return BattleTimeLimit(); }
    void battle_time_limit(std::int32_t value) { BattleTimeLimit(value); }
    [[nodiscard]] std::int32_t TimeLimit() const { return read_i32(0x24); }
    void TimeLimit(std::int32_t value) { write_i32(0x24, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t time_limit() const { return TimeLimit(); }
    void time_limit(std::int32_t value) { TimeLimit(value); }
    [[nodiscard]] std::uint16_t PointLimit() const { return read_u16(0x28); }
    void PointLimit(std::uint16_t value) { write_u16(0x28, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t point_limit() const { return PointLimit(); }
    void point_limit(std::uint16_t value) { PointLimit(value); }
    [[nodiscard]] std::uint16_t LayerId() const { return read_u16(0x2A); }
    void LayerId(std::uint16_t value) { write_u16(0x2A, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t layer_id() const { return LayerId(); }
    void layer_id(std::uint16_t value) { LayerId(value); }
    [[nodiscard]] std::int32_t FarClipDist() const { return read_i32(0x2C); }
    void FarClipDist(std::int32_t value) { write_i32(0x2C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t far_clip_dist() const { return FarClipDist(); }
    void far_clip_dist(std::int32_t value) { FarClipDist(value); }
    [[nodiscard]] std::uint16_t FogEnable() const { return read_u16(0x30); }
    void FogEnable(std::uint16_t value) { write_u16(0x30, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t fog_enable() const { return FogEnable(); }
    void fog_enable(std::uint16_t value) { FogEnable(value); }
    [[nodiscard]] std::uint16_t ClearFog() const { return read_u16(0x32); }
    void ClearFog(std::uint16_t value) { write_u16(0x32, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t clear_fog() const { return ClearFog(); }
    void clear_fog(std::uint16_t value) { ClearFog(value); }
    [[nodiscard]] std::uint16_t FogColor() const { return read_u16(0x34); }
    void FogColor(std::uint16_t value) { write_u16(0x34, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t fog_color() const { return FogColor(); }
    void fog_color(std::uint16_t value) { FogColor(value); }
    [[nodiscard]] std::uint16_t Padding36() const { return read_u16(0x36); }
    void Padding36(std::uint16_t value) { write_u16(0x36, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding36() const { return Padding36(); }
    void padding36(std::uint16_t value) { Padding36(value); }
    [[nodiscard]] std::uint32_t FogSlope() const { return read_u32(0x38); }
    void FogSlope(std::uint32_t value) { write_u32(0x38, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t fog_slope() const { return FogSlope(); }
    void fog_slope(std::uint32_t value) { FogSlope(value); }
    [[nodiscard]] std::int32_t FogOffset() const { return read_i32(0x3C); }
    void FogOffset(std::int32_t value) { write_i32(0x3C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t fog_offset() const { return FogOffset(); }
    void fog_offset(std::int32_t value) { FogOffset(value); }
    [[nodiscard]] formats::ColorRgb Light1Color() const { return read_color3(0x40); }
    void Light1Color(formats::ColorRgb value) { write_color3(0x40, value); }
    [[nodiscard]] formats::ColorRgb light1_color() const { return Light1Color(); }
    void light1_color(formats::ColorRgb value) { Light1Color(value); }
    [[nodiscard]] std::uint8_t Padding43() const { return read_u8(0x43); }
    void Padding43(std::uint8_t value) { write_u8(0x43, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding43() const { return Padding43(); }
    void padding43(std::uint8_t value) { Padding43(value); }
    [[nodiscard]] formats::Vector3 Light1Vec() const { return read_vec3(0x44); }
    void Light1Vec(formats::Vector3 value) { write_vec3(0x44, value); }
    [[nodiscard]] formats::Vector3 light1_vec() const { return Light1Vec(); }
    void light1_vec(formats::Vector3 value) { Light1Vec(value); }
    [[nodiscard]] formats::ColorRgb Light2Color() const { return read_color3(0x50); }
    void Light2Color(formats::ColorRgb value) { write_color3(0x50, value); }
    [[nodiscard]] formats::ColorRgb light2_color() const { return Light2Color(); }
    void light2_color(formats::ColorRgb value) { Light2Color(value); }
    [[nodiscard]] std::uint8_t Padding53() const { return read_u8(0x53); }
    void Padding53(std::uint8_t value) { write_u8(0x53, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t padding53() const { return Padding53(); }
    void padding53(std::uint8_t value) { Padding53(value); }
    [[nodiscard]] formats::Vector3 Light2Vec() const { return read_vec3(0x54); }
    void Light2Vec(formats::Vector3 value) { write_vec3(0x54, value); }
    [[nodiscard]] formats::Vector3 light2_vec() const { return Light2Vec(); }
    void light2_vec(formats::Vector3 value) { Light2Vec(value); }
    [[nodiscard]] std::uint32_t InternalName() const { return read_pointer(0x60); }
    void InternalName(std::uint32_t value) { write_pointer(0x60, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t internal_name() const { return InternalName(); }
    void internal_name(std::uint32_t value) { InternalName(value); }
    [[nodiscard]] std::uint32_t Archive() const { return read_pointer(0x64); }
    void Archive(std::uint32_t value) { write_pointer(0x64, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t archive() const { return Archive(); }
    void archive(std::uint32_t value) { Archive(value); }
    [[nodiscard]] std::int32_t KillHeight() const { return read_i32(0x68); }
    void KillHeight(std::int32_t value) { write_i32(0x68, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t kill_height() const { return KillHeight(); }
    void kill_height(std::int32_t value) { KillHeight(value); }
    [[nodiscard]] std::int32_t Size() const { return read_i32(0x6C); }
    void Size(std::int32_t value) { write_i32(0x6C, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t size() const { return Size(); }
    void size(std::int32_t value) { Size(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
    static constexpr std::size_t _off3 = 0xC;
    static constexpr std::size_t _off4 = 0x10;
    static constexpr std::size_t _off5 = 0x14;
    static constexpr std::size_t _off6 = 0x18;
    static constexpr std::size_t _off7 = 0x1C;
    static constexpr std::size_t _off8 = 0x20;
    static constexpr std::size_t _off9 = 0x24;
    static constexpr std::size_t _off10 = 0x28;
    static constexpr std::size_t _off11 = 0x2A;
    static constexpr std::size_t _off12 = 0x2C;
    static constexpr std::size_t _off13 = 0x30;
    static constexpr std::size_t _off14 = 0x32;
    static constexpr std::size_t _off15 = 0x34;
    static constexpr std::size_t _off16 = 0x36;
    static constexpr std::size_t _off17 = 0x38;
    static constexpr std::size_t _off18 = 0x3C;
    static constexpr std::size_t _off19 = 0x40;
    static constexpr std::size_t _off20 = 0x43;
    static constexpr std::size_t _off21 = 0x44;
    static constexpr std::size_t _off22 = 0x50;
    static constexpr std::size_t _off23 = 0x53;
    static constexpr std::size_t _off24 = 0x54;
    static constexpr std::size_t _off25 = 0x60;
    static constexpr std::size_t _off26 = 0x64;
    static constexpr std::size_t _off27 = 0x68;
    static constexpr std::size_t _off28 = 0x6C;
};

class EquipInfo : public MemoryClass {
public:
    EquipInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    EquipInfo(Buffer& buffer, std::uint32_t address);
    ~EquipInfo() override;

    [[nodiscard]] std::uint8_t Flags() const { return read_u8(0x0); }
    void Flags(std::uint8_t value) { write_u8(0x0, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t flags() const { return Flags(); }
    void flags(std::uint8_t value) { Flags(value); }
    [[nodiscard]] std::uint8_t Count() const { return read_u8(0x1); }
    void Count(std::uint8_t value) { write_u8(0x1, static_cast<std::uint8_t>(value)); }
    [[nodiscard]] std::uint8_t count() const { return Count(); }
    void count(std::uint8_t value) { Count(value); }
    [[nodiscard]] std::uint16_t Padding2() const { return read_u16(0x2); }
    void Padding2(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t padding2() const { return Padding2(); }
    void padding2(std::uint16_t value) { Padding2(value); }
    [[nodiscard]] std::uint32_t Beams() const { return read_pointer(0x4); }
    void Beams(std::uint32_t value) { write_pointer(0x4, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t beams() const { return Beams(); }
    void beams(std::uint32_t value) { Beams(value); }
    [[nodiscard]] std::uint32_t WeaponInfo() const { return read_pointer(0x8); }
    void WeaponInfo(std::uint32_t value) { write_pointer(0x8, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t weapon_info() const { return WeaponInfo(); }
    void weapon_info(std::uint32_t value) { WeaponInfo(value); }
    [[nodiscard]] std::uint32_t AmmoPtr() const { return read_pointer(0xC); }
    void AmmoPtr(std::uint32_t value) { write_pointer(0xC, static_cast<std::uint32_t>(value)); }
    [[nodiscard]] std::uint32_t ammo_ptr() const { return AmmoPtr(); }
    void ammo_ptr(std::uint32_t value) { AmmoPtr(value); }
    [[nodiscard]] std::uint16_t ChargeLevel() const { return read_u16(0x10); }
    void ChargeLevel(std::uint16_t value) { write_u16(0x10, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t charge_level() const { return ChargeLevel(); }
    void charge_level(std::uint16_t value) { ChargeLevel(value); }
    [[nodiscard]] std::uint16_t SmokeLevel() const { return read_u16(0x12); }
    void SmokeLevel(std::uint16_t value) { write_u16(0x12, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t smoke_level() const { return SmokeLevel(); }
    void smoke_level(std::uint16_t value) { SmokeLevel(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x1;
    static constexpr std::size_t _off2 = 0x2;
    static constexpr std::size_t _off3 = 0x4;
    static constexpr std::size_t _off4 = 0x8;
    static constexpr std::size_t _off5 = 0xC;
    static constexpr std::size_t _off6 = 0x10;
    static constexpr std::size_t _off7 = 0x12;
};

class AiButton : public MemoryClass {
public:
    AiButton(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    AiButton(Buffer& buffer, std::uint32_t address);
    ~AiButton() override;

    [[nodiscard]] std::uint16_t IsDown() const { return read_u16(0x0); }
    void IsDown(std::uint16_t value) { write_u16(0x0, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t is_down() const { return IsDown(); }
    void is_down(std::uint16_t value) { IsDown(value); }
    [[nodiscard]] std::uint16_t FramesDown() const { return read_u16(0x2); }
    void FramesDown(std::uint16_t value) { write_u16(0x2, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t frames_down() const { return FramesDown(); }
    void frames_down(std::uint16_t value) { FramesDown(value); }
    [[nodiscard]] std::uint16_t FramesUp() const { return read_u16(0x4); }
    void FramesUp(std::uint16_t value) { write_u16(0x4, static_cast<std::uint16_t>(value)); }
    [[nodiscard]] std::uint16_t frames_up() const { return FramesUp(); }
    void frames_up(std::uint16_t value) { FramesUp(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x2;
    static constexpr std::size_t _off2 = 0x4;
};

class VecFx32 : public MemoryClass {
public:
    VecFx32(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    VecFx32(Buffer& buffer, std::uint32_t address);
    ~VecFx32() override;

    [[nodiscard]] std::int32_t X() const { return read_i32(0x0); }
    void X(std::int32_t value) { write_i32(0x0, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t x() const { return X(); }
    void x(std::int32_t value) { X(value); }
    [[nodiscard]] std::int32_t Y() const { return read_i32(0x4); }
    void Y(std::int32_t value) { write_i32(0x4, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t y() const { return Y(); }
    void y(std::int32_t value) { Y(value); }
    [[nodiscard]] std::int32_t Z() const { return read_i32(0x8); }
    void Z(std::int32_t value) { write_i32(0x8, static_cast<std::int32_t>(value)); }
    [[nodiscard]] std::int32_t z() const { return Z(); }
    void z(std::int32_t value) { Z(value); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0x4;
    static constexpr std::size_t _off2 = 0x8;
};

class MtxFx43 : public MemoryClass {
public:
    MtxFx43(Buffer& buffer, std::size_t base_offset, std::uint32_t address);
    MtxFx43(Buffer& buffer, std::uint32_t address);
    ~MtxFx43() override;

    [[nodiscard]] Int32Array& M() noexcept;
    [[nodiscard]] const Int32Array& M() const noexcept;
    [[nodiscard]] Int32Array& m() noexcept { return M(); }
    [[nodiscard]] const Int32Array& m() const noexcept { return M(); }
    [[nodiscard]] ::fruityprime::memory::VecFx32& Row0() noexcept;
    [[nodiscard]] const ::fruityprime::memory::VecFx32& Row0() const noexcept;
    [[nodiscard]] ::fruityprime::memory::VecFx32& row0() noexcept { return Row0(); }
    [[nodiscard]] const ::fruityprime::memory::VecFx32& row0() const noexcept { return Row0(); }
    [[nodiscard]] ::fruityprime::memory::VecFx32& Row1() noexcept;
    [[nodiscard]] const ::fruityprime::memory::VecFx32& Row1() const noexcept;
    [[nodiscard]] ::fruityprime::memory::VecFx32& row1() noexcept { return Row1(); }
    [[nodiscard]] const ::fruityprime::memory::VecFx32& row1() const noexcept { return Row1(); }
    [[nodiscard]] ::fruityprime::memory::VecFx32& Row2() noexcept;
    [[nodiscard]] const ::fruityprime::memory::VecFx32& Row2() const noexcept;
    [[nodiscard]] ::fruityprime::memory::VecFx32& row2() noexcept { return Row2(); }
    [[nodiscard]] const ::fruityprime::memory::VecFx32& row2() const noexcept { return Row2(); }
    [[nodiscard]] ::fruityprime::memory::VecFx32& Row3() noexcept;
    [[nodiscard]] const ::fruityprime::memory::VecFx32& Row3() const noexcept;
    [[nodiscard]] ::fruityprime::memory::VecFx32& row3() noexcept { return Row3(); }
    [[nodiscard]] const ::fruityprime::memory::VecFx32& row3() const noexcept { return Row3(); }

private:
    static constexpr std::size_t _off0 = 0x0;
    static constexpr std::size_t _off1 = 0xC;
    static constexpr std::size_t _off2 = 0x18;
    static constexpr std::size_t _off3 = 0x24;
    std::unique_ptr<Int32Array> m_;
    std::unique_ptr<::fruityprime::memory::VecFx32> row0_;
    std::unique_ptr<::fruityprime::memory::VecFx32> row1_;
    std::unique_ptr<::fruityprime::memory::VecFx32> row2_;
    std::unique_ptr<::fruityprime::memory::VecFx32> row3_;
};

} // namespace fruityprime::memory

namespace MphReadNative {
using MemoryClass = ::fruityprime::memory::MemoryClass;
using MemoryLayout = ::fruityprime::memory::Layout;
using MemoryObject = ::fruityprime::memory::Object;
using MemoryField = ::fruityprime::memory::FieldInfo;
}

#include "MemoryClasses.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace fruityprime::memory {

bool Layout::add(std::string name, std::size_t offset, std::size_t size) {
    if (name.empty() || size == 0 || find(name) != nullptr) return false;
    fields_.push_back({std::move(name), offset, size});
    return true;
}

const FieldInfo* Layout::find(std::string_view name) const noexcept {
    const auto it = std::find_if(fields_.begin(), fields_.end(),
        [name](const FieldInfo& field) { return field.name == name; });
    return it == fields_.end() ? nullptr : &*it;
}

const FieldInfo& Object::require_field(std::string_view field) const {
    const FieldInfo* info = layout_->find(field);
    if (info == nullptr) throw std::out_of_range("unknown memory layout field");
    if (info->size < sizeof(std::uint32_t)
        || !buffer_->contains(base_offset_ + info->offset, sizeof(std::uint32_t)))
        throw std::out_of_range("memory layout field is outside the buffer");
    return *info;
}

std::uint32_t Object::read_u32(std::string_view field) const {
    const auto& info = require_field(field);
    return buffer_->read_u32_le(base_offset_ + info.offset);
}

void Object::write_u32(std::string_view field, std::uint32_t value) {
    const auto& info = require_field(field);
    buffer_->write_u32_le(base_offset_ + info.offset, value);
}

MemoryArrayBase::MemoryArrayBase(Buffer& buffer, std::size_t base_offset,
                                   std::size_t length, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address), length_(length) {}

MemoryArrayBase::MemoryArrayBase(Buffer& buffer, std::uint32_t address,
                                   std::size_t length)
    : MemoryClass(buffer, address), length_(length) {}

void MemoryArrayBase::require_index(std::size_t index) const {
    if (index >= length_) throw std::out_of_range("memory array index is outside the view");
}

std::size_t MemoryArrayBase::element_offset(std::size_t index,
                                              std::size_t width) const {
    require_index(index);
    if (index > (std::numeric_limits<std::size_t>::max() / width))
        throw std::out_of_range("memory array offset overflow");
    return at(index * width);
}

CEntity::CEntity(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

CEntity::CEntity(Buffer& buffer, std::uint32_t address)
    : CEntity(buffer, offset_for_address(buffer, address), address) {}

CEntity::~CEntity() = default;

CEnemyBase::CEnemyBase(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    hurt_vol_unxf_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x8C, address + 0x8C);
    hurt_vol_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0xCC, address + 0xCC);
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x118, address + 0x118);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x160, address + 0x160);
}

CEnemyBase::CEnemyBase(Buffer& buffer, std::uint32_t address)
    : CEnemyBase(buffer, offset_for_address(buffer, address), address) {}

CEnemyBase::~CEnemyBase() = default;

::fruityprime::memory::CollisionVolume& CEnemyBase::HurtVolUnxf() noexcept { return *hurt_vol_unxf_; }
const ::fruityprime::memory::CollisionVolume& CEnemyBase::HurtVolUnxf() const noexcept { return *hurt_vol_unxf_; }

::fruityprime::memory::CollisionVolume& CEnemyBase::HurtVol() noexcept { return *hurt_vol_; }
const ::fruityprime::memory::CollisionVolume& CEnemyBase::HurtVol() const noexcept { return *hurt_vol_; }

::fruityprime::memory::CModel& CEnemyBase::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CEnemyBase::Model() const noexcept { return *model_; }

::fruityprime::memory::SfxParameters& CEnemyBase::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CEnemyBase::SfxParameters() const noexcept { return *sfx_parameters_; }

CEnemy24::CEnemy24(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
    regen_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x178, address + 0x178);
    arms_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x1C8, 2, address + 0x1C8);
    legs_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x1D0, 3, address + 0x1D0);
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x1E8, address + 0x1E8);
}

CEnemy24::CEnemy24(Buffer& buffer, std::uint32_t address)
    : CEnemy24(buffer, offset_for_address(buffer, address), address) {}

CEnemy24::~CEnemy24() = default;

::fruityprime::memory::CModel& CEnemy24::Regen() noexcept { return *regen_; }
const ::fruityprime::memory::CModel& CEnemy24::Regen() const noexcept { return *regen_; }

IntPtrArray& CEnemy24::Arms() noexcept { return *arms_; }
const IntPtrArray& CEnemy24::Arms() const noexcept { return *arms_; }

IntPtrArray& CEnemy24::Legs() noexcept { return *legs_; }
const IntPtrArray& CEnemy24::Legs() const noexcept { return *legs_; }

::fruityprime::memory::CollisionVolume& CEnemy24::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CEnemy24::Volume() const noexcept { return *volume_; }

CEnemy25::CEnemy25(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
}

CEnemy25::CEnemy25(Buffer& buffer, std::uint32_t address)
    : CEnemy25(buffer, offset_for_address(buffer, address), address) {}

CEnemy25::~CEnemy25() = default;

CEnemy26::CEnemy26(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
    equip_info_ = std::make_unique<::fruityprime::memory::EquipInfo>(buffer, base_offset + 0x188, address + 0x188);
}

CEnemy26::CEnemy26(Buffer& buffer, std::uint32_t address)
    : CEnemy26(buffer, offset_for_address(buffer, address), address) {}

CEnemy26::~CEnemy26() = default;

::fruityprime::memory::EquipInfo& CEnemy26::EquipInfo() noexcept { return *equip_info_; }
const ::fruityprime::memory::EquipInfo& CEnemy26::EquipInfo() const noexcept { return *equip_info_; }

CEnemy27::CEnemy27(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
}

CEnemy27::CEnemy27(Buffer& buffer, std::uint32_t address)
    : CEnemy27(buffer, offset_for_address(buffer, address), address) {}

CEnemy27::~CEnemy27() = default;

CEnemy28::CEnemy28(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x17C, address + 0x17C);
    trocras_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x1D0, 9, address + 0x1D0);
}

CEnemy28::CEnemy28(Buffer& buffer, std::uint32_t address)
    : CEnemy28(buffer, offset_for_address(buffer, address), address) {}

CEnemy28::~CEnemy28() = default;

::fruityprime::memory::CollisionVolume& CEnemy28::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CEnemy28::Volume() const noexcept { return *volume_; }

IntPtrArray& CEnemy28::Trocras() noexcept { return *trocras_; }
const IntPtrArray& CEnemy28::Trocras() const noexcept { return *trocras_; }

Enemy29Fields::Enemy29Fields(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    vecs_ = std::make_unique<StructArray<::fruityprime::memory::VecFx32>>(buffer, offset_for_address(VecsReference()), Count2(), 12, VecsReference());
    mtxs_ = std::make_unique<StructArray<::fruityprime::memory::MtxFx43>>(buffer, offset_for_address(MtxsReference()), Count1(), 48, MtxsReference());
    ints_ = std::make_unique<Int32Array>(buffer, offset_for_address(IntsReference()), Count1(), IntsReference());
    shorts_ = std::make_unique<Int16Array>(buffer, offset_for_address(ShortsReference()), Count1(), ShortsReference());
}

Enemy29Fields::Enemy29Fields(Buffer& buffer, std::uint32_t address)
    : Enemy29Fields(buffer, offset_for_address(buffer, address), address) {}

Enemy29Fields::~Enemy29Fields() = default;

StructArray<::fruityprime::memory::VecFx32>& Enemy29Fields::Vecs() noexcept { return *vecs_; }
const StructArray<::fruityprime::memory::VecFx32>& Enemy29Fields::Vecs() const noexcept { return *vecs_; }

StructArray<::fruityprime::memory::MtxFx43>& Enemy29Fields::Mtxs() noexcept { return *mtxs_; }
const StructArray<::fruityprime::memory::MtxFx43>& Enemy29Fields::Mtxs() const noexcept { return *mtxs_; }

Int32Array& Enemy29Fields::Ints() noexcept { return *ints_; }
const Int32Array& Enemy29Fields::Ints() const noexcept { return *ints_; }

Int16Array& Enemy29Fields::Shorts() noexcept { return *shorts_; }
const Int16Array& Enemy29Fields::Shorts() const noexcept { return *shorts_; }

CEnemy29::CEnemy29(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
    mind_trick_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x178, address + 0x178);
    grapple_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x1C8, address + 0x1C8);
    fields_ = std::make_unique<::fruityprime::memory::Enemy29Fields>(buffer, offset_for_address(FieldsReference()), FieldsReference());
}

CEnemy29::CEnemy29(Buffer& buffer, std::uint32_t address)
    : CEnemy29(buffer, offset_for_address(buffer, address), address) {}

CEnemy29::~CEnemy29() = default;

::fruityprime::memory::CModel& CEnemy29::MindTrick() noexcept { return *mind_trick_; }
const ::fruityprime::memory::CModel& CEnemy29::MindTrick() const noexcept { return *mind_trick_; }

::fruityprime::memory::CModel& CEnemy29::Grapple() noexcept { return *grapple_; }
const ::fruityprime::memory::CModel& CEnemy29::Grapple() const noexcept { return *grapple_; }

::fruityprime::memory::Enemy29Fields& CEnemy29::Fields() noexcept { return *fields_; }
const ::fruityprime::memory::Enemy29Fields& CEnemy29::Fields() const noexcept { return *fields_; }

CEnemy30::CEnemy30(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEnemyBase(buffer, base_offset, address) {
}

CEnemy30::CEnemy30(Buffer& buffer, std::uint32_t address)
    : CEnemy30(buffer, offset_for_address(buffer, address), address) {}

CEnemy30::~CEnemy30() = default;

CPlatform::CPlatform(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    equip_info_ = std::make_unique<::fruityprime::memory::EquipInfoPtr>(buffer, base_offset + 0x94, address + 0x94);
    entity_collision_ = std::make_unique<::fruityprime::memory::EntityCollision>(buffer, base_offset + 0x1A8, address + 0x1A8);
    turrets_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x262, 4, address + 0x262);
    effects_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x26C, 4, address + 0x26C);
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x298, address + 0x298);
    lifetime_event_indices_ = std::make_unique<ByteArray>(buffer, base_offset + 0x2F0, 4, address + 0x2F0);
    lifetime_event_targets_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x2F4, 4, address + 0x2F4);
    lifetime_event_ids_ = std::make_unique<U32EnumArray<formats::Message>>(buffer, base_offset + 0x304, 4, address + 0x304);
    lifetime_event_param1s_ = std::make_unique<Int32Array>(buffer, base_offset + 0x314, 4, address + 0x314);
    lifetime_event_param2s_ = std::make_unique<Int32Array>(buffer, base_offset + 0x324, 4, address + 0x324);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x334, address + 0x334);
}

CPlatform::CPlatform(Buffer& buffer, std::uint32_t address)
    : CPlatform(buffer, offset_for_address(buffer, address), address) {}

CPlatform::~CPlatform() = default;

::fruityprime::memory::EquipInfoPtr& CPlatform::EquipInfo() noexcept { return *equip_info_; }
const ::fruityprime::memory::EquipInfoPtr& CPlatform::EquipInfo() const noexcept { return *equip_info_; }

::fruityprime::memory::EntityCollision& CPlatform::EntityCollision() noexcept { return *entity_collision_; }
const ::fruityprime::memory::EntityCollision& CPlatform::EntityCollision() const noexcept { return *entity_collision_; }

UInt16Array& CPlatform::Turrets() noexcept { return *turrets_; }
const UInt16Array& CPlatform::Turrets() const noexcept { return *turrets_; }

IntPtrArray& CPlatform::Effects() noexcept { return *effects_; }
const IntPtrArray& CPlatform::Effects() const noexcept { return *effects_; }

::fruityprime::memory::CModel& CPlatform::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CPlatform::Model() const noexcept { return *model_; }

ByteArray& CPlatform::LifetimeEventIndices() noexcept { return *lifetime_event_indices_; }
const ByteArray& CPlatform::LifetimeEventIndices() const noexcept { return *lifetime_event_indices_; }

IntPtrArray& CPlatform::LifetimeEventTargets() noexcept { return *lifetime_event_targets_; }
const IntPtrArray& CPlatform::LifetimeEventTargets() const noexcept { return *lifetime_event_targets_; }

U32EnumArray<formats::Message>& CPlatform::LifetimeEventIds() noexcept { return *lifetime_event_ids_; }
const U32EnumArray<formats::Message>& CPlatform::LifetimeEventIds() const noexcept { return *lifetime_event_ids_; }

Int32Array& CPlatform::LifetimeEventParam1s() noexcept { return *lifetime_event_param1s_; }
const Int32Array& CPlatform::LifetimeEventParam1s() const noexcept { return *lifetime_event_param1s_; }

Int32Array& CPlatform::LifetimeEventParam2s() noexcept { return *lifetime_event_param2s_; }
const Int32Array& CPlatform::LifetimeEventParam2s() const noexcept { return *lifetime_event_param2s_; }

::fruityprime::memory::SfxParameters& CPlatform::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CPlatform::SfxParameters() const noexcept { return *sfx_parameters_; }

CObject::CObject(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    col_structs_ = std::make_unique<StructArray<::fruityprime::memory::EntityCollision>>(buffer, base_offset + 0x88, 2, 180, address + 0x88);
    mtx_objs_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x1F0, 2, address + 0x1F0);
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x1F8, address + 0x1F8);
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x250, address + 0x250);
    sfx_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x298, address + 0x298);
}

CObject::CObject(Buffer& buffer, std::uint32_t address)
    : CObject(buffer, offset_for_address(buffer, address), address) {}

CObject::~CObject() = default;

StructArray<::fruityprime::memory::EntityCollision>& CObject::ColStructs() noexcept { return *col_structs_; }
const StructArray<::fruityprime::memory::EntityCollision>& CObject::ColStructs() const noexcept { return *col_structs_; }

IntPtrArray& CObject::MtxObjs() noexcept { return *mtx_objs_; }
const IntPtrArray& CObject::MtxObjs() const noexcept { return *mtx_objs_; }

::fruityprime::memory::CModel& CObject::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CObject::Model() const noexcept { return *model_; }

::fruityprime::memory::CollisionVolume& CObject::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CObject::Volume() const noexcept { return *volume_; }

::fruityprime::memory::SfxParameters& CObject::Sfx() noexcept { return *sfx_; }
const ::fruityprime::memory::SfxParameters& CObject::Sfx() const noexcept { return *sfx_; }

CPlayerSpawn::CPlayerSpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
}

CPlayerSpawn::CPlayerSpawn(Buffer& buffer, std::uint32_t address)
    : CPlayerSpawn(buffer, offset_for_address(buffer, address), address) {}

CPlayerSpawn::~CPlayerSpawn() = default;

CDoor::CDoor(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    door_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x5C, address + 0x5C);
    lock_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0xA4, address + 0xA4);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x100, address + 0x100);
}

CDoor::CDoor(Buffer& buffer, std::uint32_t address)
    : CDoor(buffer, offset_for_address(buffer, address), address) {}

CDoor::~CDoor() = default;

::fruityprime::memory::CModel& CDoor::DoorModel() noexcept { return *door_model_; }
const ::fruityprime::memory::CModel& CDoor::DoorModel() const noexcept { return *door_model_; }

::fruityprime::memory::CModel& CDoor::LockModel() noexcept { return *lock_model_; }
const ::fruityprime::memory::CModel& CDoor::LockModel() const noexcept { return *lock_model_; }

::fruityprime::memory::SfxParameters& CDoor::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CDoor::SfxParameters() const noexcept { return *sfx_parameters_; }

CItemSpawn::CItemSpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    base_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x64, address + 0x64);
}

CItemSpawn::CItemSpawn(Buffer& buffer, std::uint32_t address)
    : CItemSpawn(buffer, offset_for_address(buffer, address), address) {}

CItemSpawn::~CItemSpawn() = default;

::fruityprime::memory::CModel& CItemSpawn::BaseModel() noexcept { return *base_model_; }
const ::fruityprime::memory::CModel& CItemSpawn::BaseModel() const noexcept { return *base_model_; }

CItemInstance::CItemInstance(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x58, address + 0x58);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0xA0, address + 0xA0);
}

CItemInstance::CItemInstance(Buffer& buffer, std::uint32_t address)
    : CItemInstance(buffer, offset_for_address(buffer, address), address) {}

CItemInstance::~CItemInstance() = default;

::fruityprime::memory::CModel& CItemInstance::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CItemInstance::Model() const noexcept { return *model_; }

::fruityprime::memory::SfxParameters& CItemInstance::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CItemInstance::SfxParameters() const noexcept { return *sfx_parameters_; }

CEnemySpawn::CEnemySpawn(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
}

CEnemySpawn::CEnemySpawn(Buffer& buffer, std::uint32_t address)
    : CEnemySpawn(buffer, offset_for_address(buffer, address), address) {}

CEnemySpawn::~CEnemySpawn() = default;

CTriggerVolume::CTriggerVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x38, address + 0x38);
}

CTriggerVolume::CTriggerVolume(Buffer& buffer, std::uint32_t address)
    : CTriggerVolume(buffer, offset_for_address(buffer, address), address) {}

CTriggerVolume::~CTriggerVolume() = default;

::fruityprime::memory::CollisionVolume& CTriggerVolume::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CTriggerVolume::Volume() const noexcept { return *volume_; }

CAreaVolume::CAreaVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    triggered_slots_ = std::make_unique<ByteArray>(buffer, base_offset + 0x3F, 4, address + 0x3F);
    priority_slots_ = std::make_unique<ByteArray>(buffer, base_offset + 0x43, 4, address + 0x43);
    cooldown_slots_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x68, 4, address + 0x68);
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x7C, address + 0x7C);
}

CAreaVolume::CAreaVolume(Buffer& buffer, std::uint32_t address)
    : CAreaVolume(buffer, offset_for_address(buffer, address), address) {}

CAreaVolume::~CAreaVolume() = default;

ByteArray& CAreaVolume::TriggeredSlots() noexcept { return *triggered_slots_; }
const ByteArray& CAreaVolume::TriggeredSlots() const noexcept { return *triggered_slots_; }

ByteArray& CAreaVolume::PrioritySlots() noexcept { return *priority_slots_; }
const ByteArray& CAreaVolume::PrioritySlots() const noexcept { return *priority_slots_; }

UInt16Array& CAreaVolume::CooldownSlots() noexcept { return *cooldown_slots_; }
const UInt16Array& CAreaVolume::CooldownSlots() const noexcept { return *cooldown_slots_; }

::fruityprime::memory::CollisionVolume& CAreaVolume::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CAreaVolume::Volume() const noexcept { return *volume_; }

CJumpPad::CJumpPad(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    field_e4_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0xE4, address + 0xE4);
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x124, address + 0x124);
    base_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x168, address + 0x168);
    beam_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x1B0, address + 0x1B0);
}

CJumpPad::CJumpPad(Buffer& buffer, std::uint32_t address)
    : CJumpPad(buffer, offset_for_address(buffer, address), address) {}

CJumpPad::~CJumpPad() = default;

::fruityprime::memory::CollisionVolume& CJumpPad::FieldE4() noexcept { return *field_e4_; }
const ::fruityprime::memory::CollisionVolume& CJumpPad::FieldE4() const noexcept { return *field_e4_; }

::fruityprime::memory::CollisionVolume& CJumpPad::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CJumpPad::Volume() const noexcept { return *volume_; }

::fruityprime::memory::CModel& CJumpPad::BaseModel() noexcept { return *base_model_; }
const ::fruityprime::memory::CModel& CJumpPad::BaseModel() const noexcept { return *base_model_; }

::fruityprime::memory::CModel& CJumpPad::BeamModel() noexcept { return *beam_model_; }
const ::fruityprime::memory::CModel& CJumpPad::BeamModel() const noexcept { return *beam_model_; }

CPointModule::CPointModule(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
}

CPointModule::CPointModule(Buffer& buffer, std::uint32_t address)
    : CPointModule(buffer, offset_for_address(buffer, address), address) {}

CPointModule::~CPointModule() = default;

CMorphCamera::CMorphCamera(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x24, address + 0x24);
}

CMorphCamera::CMorphCamera(Buffer& buffer, std::uint32_t address)
    : CMorphCamera(buffer, offset_for_address(buffer, address), address) {}

CMorphCamera::~CMorphCamera() = default;

::fruityprime::memory::CollisionVolume& CMorphCamera::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CMorphCamera::Volume() const noexcept { return *volume_; }

COctolithFlag::COctolithFlag(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    base_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x5C, address + 0x5C);
    octo_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0xA4, address + 0xA4);
}

COctolithFlag::COctolithFlag(Buffer& buffer, std::uint32_t address)
    : COctolithFlag(buffer, offset_for_address(buffer, address), address) {}

COctolithFlag::~COctolithFlag() = default;

::fruityprime::memory::CModel& COctolithFlag::BaseModel() noexcept { return *base_model_; }
const ::fruityprime::memory::CModel& COctolithFlag::BaseModel() const noexcept { return *base_model_; }

::fruityprime::memory::CModel& COctolithFlag::OctoModel() noexcept { return *octo_model_; }
const ::fruityprime::memory::CModel& COctolithFlag::OctoModel() const noexcept { return *octo_model_; }

CFlagBase::CFlagBase(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x3C, address + 0x3C);
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x80, address + 0x80);
}

CFlagBase::CFlagBase(Buffer& buffer, std::uint32_t address)
    : CFlagBase(buffer, offset_for_address(buffer, address), address) {}

CFlagBase::~CFlagBase() = default;

::fruityprime::memory::CollisionVolume& CFlagBase::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CFlagBase::Volume() const noexcept { return *volume_; }

::fruityprime::memory::CModel& CFlagBase::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CFlagBase::Model() const noexcept { return *model_; }

CTeleporter::CTeleporter(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    tele_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x5C, address + 0x5C);
    artifact_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0xA4, address + 0xA4);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0xEC, address + 0xEC);
}

CTeleporter::CTeleporter(Buffer& buffer, std::uint32_t address)
    : CTeleporter(buffer, offset_for_address(buffer, address), address) {}

CTeleporter::~CTeleporter() = default;

::fruityprime::memory::CModel& CTeleporter::TeleModel() noexcept { return *tele_model_; }
const ::fruityprime::memory::CModel& CTeleporter::TeleModel() const noexcept { return *tele_model_; }

::fruityprime::memory::CModel& CTeleporter::ArtifactModel() noexcept { return *artifact_model_; }
const ::fruityprime::memory::CModel& CTeleporter::ArtifactModel() const noexcept { return *artifact_model_; }

::fruityprime::memory::SfxParameters& CTeleporter::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CTeleporter::SfxParameters() const noexcept { return *sfx_parameters_; }

CNodeDefense::CNodeDefense(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x44, address + 0x44);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x98, address + 0x98);
    ring_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x9C, address + 0x9C);
    node_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0xE4, address + 0xE4);
}

CNodeDefense::CNodeDefense(Buffer& buffer, std::uint32_t address)
    : CNodeDefense(buffer, offset_for_address(buffer, address), address) {}

CNodeDefense::~CNodeDefense() = default;

::fruityprime::memory::CollisionVolume& CNodeDefense::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CNodeDefense::Volume() const noexcept { return *volume_; }

::fruityprime::memory::SfxParameters& CNodeDefense::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CNodeDefense::SfxParameters() const noexcept { return *sfx_parameters_; }

::fruityprime::memory::CModel& CNodeDefense::RingModel() noexcept { return *ring_model_; }
const ::fruityprime::memory::CModel& CNodeDefense::RingModel() const noexcept { return *ring_model_; }

::fruityprime::memory::CModel& CNodeDefense::NodeModel() noexcept { return *node_model_; }
const ::fruityprime::memory::CModel& CNodeDefense::NodeModel() const noexcept { return *node_model_; }

CLightSource::CLightSource(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    volume_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x1C, address + 0x1C);
}

CLightSource::CLightSource(Buffer& buffer, std::uint32_t address)
    : CLightSource(buffer, offset_for_address(buffer, address), address) {}

CLightSource::~CLightSource() = default;

::fruityprime::memory::CollisionVolume& CLightSource::Volume() noexcept { return *volume_; }
const ::fruityprime::memory::CollisionVolume& CLightSource::Volume() const noexcept { return *volume_; }

CArtifact::CArtifact(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    artifact_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x7C, address + 0x7C);
    base_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0xC4, address + 0xC4);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x110, address + 0x110);
}

CArtifact::CArtifact(Buffer& buffer, std::uint32_t address)
    : CArtifact(buffer, offset_for_address(buffer, address), address) {}

CArtifact::~CArtifact() = default;

::fruityprime::memory::CModel& CArtifact::ArtifactModel() noexcept { return *artifact_model_; }
const ::fruityprime::memory::CModel& CArtifact::ArtifactModel() const noexcept { return *artifact_model_; }

::fruityprime::memory::CModel& CArtifact::BaseModel() noexcept { return *base_model_; }
const ::fruityprime::memory::CModel& CArtifact::BaseModel() const noexcept { return *base_model_; }

::fruityprime::memory::SfxParameters& CArtifact::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CArtifact::SfxParameters() const noexcept { return *sfx_parameters_; }

CCameraSequence::CCameraSequence(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
}

CCameraSequence::CCameraSequence(Buffer& buffer, std::uint32_t address)
    : CCameraSequence(buffer, offset_for_address(buffer, address), address) {}

CCameraSequence::~CCameraSequence() = default;

CForceField::CForceField(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x44, address + 0x44);
}

CForceField::CForceField(Buffer& buffer, std::uint32_t address)
    : CForceField(buffer, offset_for_address(buffer, address), address) {}

CForceField::~CForceField() = default;

::fruityprime::memory::CModel& CForceField::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CForceField::Model() const noexcept { return *model_; }

CBeamEffect::CBeamEffect(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x78, address + 0x78);
}

CBeamEffect::CBeamEffect(Buffer& buffer, std::uint32_t address)
    : CBeamEffect(buffer, offset_for_address(buffer, address), address) {}

CBeamEffect::~CBeamEffect() = default;

::fruityprime::memory::CModel& CBeamEffect::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CBeamEffect::Model() const noexcept { return *model_; }

CBomb::CBomb(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x68, address + 0x68);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0xB4, address + 0xB4);
}

CBomb::CBomb(Buffer& buffer, std::uint32_t address)
    : CBomb(buffer, offset_for_address(buffer, address), address) {}

CBomb::~CBomb() = default;

::fruityprime::memory::CModel& CBomb::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CBomb::Model() const noexcept { return *model_; }

::fruityprime::memory::SfxParameters& CBomb::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CBomb::SfxParameters() const noexcept { return *sfx_parameters_; }

CHalfturret::CHalfturret(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    light_info_ = std::make_unique<::fruityprime::memory::LightInfo>(buffer, base_offset + 0x48, address + 0x48);
    equip_info_ = std::make_unique<::fruityprime::memory::EquipInfoPtr>(buffer, base_offset + 0x68, address + 0x68);
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x80, address + 0x80);
}

CHalfturret::CHalfturret(Buffer& buffer, std::uint32_t address)
    : CHalfturret(buffer, offset_for_address(buffer, address), address) {}

CHalfturret::~CHalfturret() = default;

::fruityprime::memory::LightInfo& CHalfturret::LightInfo() noexcept { return *light_info_; }
const ::fruityprime::memory::LightInfo& CHalfturret::LightInfo() const noexcept { return *light_info_; }

::fruityprime::memory::EquipInfoPtr& CHalfturret::EquipInfo() noexcept { return *equip_info_; }
const ::fruityprime::memory::EquipInfoPtr& CHalfturret::EquipInfo() const noexcept { return *equip_info_; }

::fruityprime::memory::CModel& CHalfturret::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CHalfturret::Model() const noexcept { return *model_; }

CPlayer::CPlayer(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    field100_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x100, 2, address + 0x100);
    collision_ = std::make_unique<::fruityprime::memory::CollisionVolume>(buffer, base_offset + 0x108, address + 0x108);
    gun_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x15C, address + 0x15C);
    frozen_model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x1A4, address + 0x1A4);
    spine_node_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x21C, 2, address + 0x21C);
    shoot_node_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x224, 2, address + 0x224);
    biped1_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x22C, address + 0x22C);
    biped2_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x274, address + 0x274);
    alt_form_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x2C0, address + 0x2C0);
    gun_smoke_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x308, address + 0x308);
    controls_ = std::make_unique<::fruityprime::memory::PlayerControls>(buffer, base_offset + 0x364, address + 0x364);
    input_ = std::make_unique<::fruityprime::memory::PlayerInput>(buffer, base_offset + 0x464, address + 0x464);
    camera_info_ = std::make_unique<::fruityprime::memory::CameraInfo>(buffer, base_offset + 0x55C, address + 0x55C);
    light_info_ = std::make_unique<::fruityprime::memory::LightInfo>(buffer, base_offset + 0x694, address + 0x694);
    equip_info_ = std::make_unique<::fruityprime::memory::EquipInfoPtr>(buffer, base_offset + 0x850, address + 0x850);
    beam_head_ = std::make_unique<::fruityprime::memory::CBeamProjectile>(buffer, base_offset + 0x864, address + 0x864);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0xF28, address + 0xF28);
    if (AiDataPtr() != 0) {
        ai_data_ = std::make_unique<::fruityprime::memory::AiData>(
            buffer, offset_for_address(AiDataPtr()), AiDataPtr());
        a_i_context_ = std::make_unique<StructArray<::fruityprime::memory::AIContext>>(
            buffer, offset_for_address(AiDataPtr() + 0x2FC), 20,
            0xA8, AiDataPtr() + 0x2FC);
        a_i_aggro_ = std::make_unique<StructArray<::fruityprime::memory::AIAggro>>(
            buffer, offset_for_address(AiDataPtr() + 0x1064), 25,
            0x10, AiDataPtr() + 0x1064);
    }
}

CPlayer::CPlayer(Buffer& buffer, std::uint32_t address)
    : CPlayer(buffer, offset_for_address(buffer, address), address) {}

CPlayer::~CPlayer() = default;

UInt16Array& CPlayer::Field100() noexcept { return *field100_; }
const UInt16Array& CPlayer::Field100() const noexcept { return *field100_; }

::fruityprime::memory::CollisionVolume& CPlayer::Collision() noexcept { return *collision_; }
const ::fruityprime::memory::CollisionVolume& CPlayer::Collision() const noexcept { return *collision_; }

::fruityprime::memory::CModel& CPlayer::GunModel() noexcept { return *gun_model_; }
const ::fruityprime::memory::CModel& CPlayer::GunModel() const noexcept { return *gun_model_; }

::fruityprime::memory::CModel& CPlayer::FrozenModel() noexcept { return *frozen_model_; }
const ::fruityprime::memory::CModel& CPlayer::FrozenModel() const noexcept { return *frozen_model_; }

IntPtrArray& CPlayer::SpineNode() noexcept { return *spine_node_; }
const IntPtrArray& CPlayer::SpineNode() const noexcept { return *spine_node_; }

IntPtrArray& CPlayer::ShootNode() noexcept { return *shoot_node_; }
const IntPtrArray& CPlayer::ShootNode() const noexcept { return *shoot_node_; }

::fruityprime::memory::CModel& CPlayer::Biped1() noexcept { return *biped1_; }
const ::fruityprime::memory::CModel& CPlayer::Biped1() const noexcept { return *biped1_; }

::fruityprime::memory::CModel& CPlayer::Biped2() noexcept { return *biped2_; }
const ::fruityprime::memory::CModel& CPlayer::Biped2() const noexcept { return *biped2_; }

::fruityprime::memory::CModel& CPlayer::AltForm() noexcept { return *alt_form_; }
const ::fruityprime::memory::CModel& CPlayer::AltForm() const noexcept { return *alt_form_; }

::fruityprime::memory::CModel& CPlayer::GunSmoke() noexcept { return *gun_smoke_; }
const ::fruityprime::memory::CModel& CPlayer::GunSmoke() const noexcept { return *gun_smoke_; }

::fruityprime::memory::PlayerControls& CPlayer::Controls() noexcept { return *controls_; }
const ::fruityprime::memory::PlayerControls& CPlayer::Controls() const noexcept { return *controls_; }

::fruityprime::memory::PlayerInput& CPlayer::Input() noexcept { return *input_; }
const ::fruityprime::memory::PlayerInput& CPlayer::Input() const noexcept { return *input_; }

::fruityprime::memory::CameraInfo& CPlayer::CameraInfo() noexcept { return *camera_info_; }
const ::fruityprime::memory::CameraInfo& CPlayer::CameraInfo() const noexcept { return *camera_info_; }

::fruityprime::memory::LightInfo& CPlayer::LightInfo() noexcept { return *light_info_; }
const ::fruityprime::memory::LightInfo& CPlayer::LightInfo() const noexcept { return *light_info_; }

::fruityprime::memory::EquipInfoPtr& CPlayer::EquipInfo() noexcept { return *equip_info_; }
const ::fruityprime::memory::EquipInfoPtr& CPlayer::EquipInfo() const noexcept { return *equip_info_; }

::fruityprime::memory::CBeamProjectile& CPlayer::BeamHead() noexcept { return *beam_head_; }
const ::fruityprime::memory::CBeamProjectile& CPlayer::BeamHead() const noexcept { return *beam_head_; }

::fruityprime::memory::AiData* CPlayer::AiData() noexcept { return ai_data_.get(); }
const ::fruityprime::memory::AiData* CPlayer::AiData() const noexcept { return ai_data_.get(); }

::fruityprime::memory::SfxParameters& CPlayer::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CPlayer::SfxParameters() const noexcept { return *sfx_parameters_; }

StructArray<::fruityprime::memory::AIContext>* CPlayer::AIContext() noexcept { return a_i_context_.get(); }
const StructArray<::fruityprime::memory::AIContext>* CPlayer::AIContext() const noexcept { return a_i_context_.get(); }

std::uint32_t CPlayer::AggroCount() const {
    if (AiDataPtr() == 0) return 0;
    return buffer().read_u32_le(offset_for_address(AiDataPtr() + 0x1060));
}

void CPlayer::AggroCount(std::uint32_t value) {
    if (AiDataPtr() == 0) return;
    buffer().write_u32_le(offset_for_address(AiDataPtr() + 0x1060), value);
}

StructArray<::fruityprime::memory::AIAggro>* CPlayer::AIAggro() noexcept { return a_i_aggro_.get(); }
const StructArray<::fruityprime::memory::AIAggro>* CPlayer::AIAggro() const noexcept { return a_i_aggro_.get(); }

AiData::AiData(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    cur_node_type_index_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x24, 6, address + 0x24);
    field4_c_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x4C, 11, address + 0x4C);
    field7_a_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x7A, 10, address + 0x7A);
    slots_hit_total_ = std::make_unique<Int32Array>(buffer, base_offset + 0xF0, 4, address + 0xF0);
    slots_damage_total_ = std::make_unique<Int32Array>(buffer, base_offset + 0x100, 4, address + 0x100);
    ent_list_ = std::make_unique<IntPtrArray>(buffer, base_offset + 0x120, 78, address + 0x120);
    buttons_ = std::make_unique<StructArray<::fruityprime::memory::AiButton>>(buffer, base_offset + 0x258, 12, 6, address + 0x258);
    touch_btns_ = std::make_unique<StructArray<::fruityprime::memory::AiButton>>(buffer, base_offset + 0x2AA, 11, 6, address + 0x2AA);
    func_tree_ = std::make_unique<Int32Array>(buffer, base_offset + 0x2FC, 840, address + 0x2FC);
    aggro_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1064, 100, address + 0x1064);
}

AiData::AiData(Buffer& buffer, std::uint32_t address)
    : AiData(buffer, offset_for_address(buffer, address), address) {}

AiData::~AiData() = default;

UInt16Array& AiData::CurNodeTypeIndex() noexcept { return *cur_node_type_index_; }
const UInt16Array& AiData::CurNodeTypeIndex() const noexcept { return *cur_node_type_index_; }

IntPtrArray& AiData::Field4C() noexcept { return *field4_c_; }
const IntPtrArray& AiData::Field4C() const noexcept { return *field4_c_; }

UInt16Array& AiData::Field7A() noexcept { return *field7_a_; }
const UInt16Array& AiData::Field7A() const noexcept { return *field7_a_; }

Int32Array& AiData::SlotsHitTotal() noexcept { return *slots_hit_total_; }
const Int32Array& AiData::SlotsHitTotal() const noexcept { return *slots_hit_total_; }

Int32Array& AiData::SlotsDamageTotal() noexcept { return *slots_damage_total_; }
const Int32Array& AiData::SlotsDamageTotal() const noexcept { return *slots_damage_total_; }

IntPtrArray& AiData::EntList() noexcept { return *ent_list_; }
const IntPtrArray& AiData::EntList() const noexcept { return *ent_list_; }

StructArray<::fruityprime::memory::AiButton>& AiData::Buttons() noexcept { return *buttons_; }
const StructArray<::fruityprime::memory::AiButton>& AiData::Buttons() const noexcept { return *buttons_; }

StructArray<::fruityprime::memory::AiButton>& AiData::TouchBtns() noexcept { return *touch_btns_; }
const StructArray<::fruityprime::memory::AiButton>& AiData::TouchBtns() const noexcept { return *touch_btns_; }

Int32Array& AiData::FuncTree() noexcept { return *func_tree_; }
const Int32Array& AiData::FuncTree() const noexcept { return *func_tree_; }

Int32Array& AiData::Aggro() noexcept { return *aggro_; }
const Int32Array& AiData::Aggro() const noexcept { return *aggro_; }

AIContext::AIContext(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    weights_ = std::make_unique<Int32Array>(buffer, base_offset + 0x54, 21, address + 0x54);
}

AIContext::AIContext(Buffer& buffer, std::uint32_t address)
    : AIContext(buffer, offset_for_address(buffer, address), address) {}

AIContext::~AIContext() = default;

Int32Array& AIContext::Weights() noexcept { return *weights_; }
const Int32Array& AIContext::Weights() const noexcept { return *weights_; }

::fruityprime::memory::AIData1* AIContext::AIData1() noexcept {
    const auto pointer = CurData1Iter();
    if (pointer != last_data1_ptr_) {
        data1_cache_.reset();
        if (pointer != 0) data1_cache_ = std::make_unique<::fruityprime::memory::AIData1>(
            buffer(), offset_for_address(pointer), pointer);
        last_data1_ptr_ = pointer;
    }
    return data1_cache_.get();
}

const ::fruityprime::memory::AIData1* AIContext::AIData1() const noexcept {
    return const_cast<AIContext*>(this)->AIData1();
}

AIData1::AIData1(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    if (Data1Ptr() != 0 && Data1Count() > 0) {
        data1_ = std::make_unique<StructArray<::fruityprime::memory::AIData1>>(
            buffer, offset_for_address(Data1Ptr()),
            static_cast<std::size_t>(Data1Count()), 0x24, Data1Ptr());
    }
    if (Data2Ptr() != 0 && Data2Count() > 0) {
        data2_ = std::make_unique<StructArray<::fruityprime::memory::AIData2>>(
            buffer, offset_for_address(Data2Ptr()),
            static_cast<std::size_t>(Data2Count()), 0x18, Data2Ptr());
    }
}

AIData1::AIData1(Buffer& buffer, std::uint32_t address)
    : AIData1(buffer, offset_for_address(buffer, address), address) {}

AIData1::~AIData1() = default;

StructArray<::fruityprime::memory::AIData1>* AIData1::Data1() noexcept { return data1_.get(); }
const StructArray<::fruityprime::memory::AIData1>* AIData1::Data1() const noexcept { return data1_.get(); }

StructArray<::fruityprime::memory::AIData2>* AIData1::Data2() noexcept { return data2_.get(); }
const StructArray<::fruityprime::memory::AIData2>* AIData1::Data2() const noexcept { return data2_.get(); }

AIData2::AIData2(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

AIData2::AIData2(Buffer& buffer, std::uint32_t address)
    : AIData2(buffer, offset_for_address(buffer, address), address) {}

AIData2::~AIData2() = default;

AIAggro::AIAggro(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

AIAggro::AIAggro(Buffer& buffer, std::uint32_t address)
    : AIAggro(buffer, offset_for_address(buffer, address), address) {}

AIAggro::~AIAggro() = default;

std::uint8_t AIAggro::VarA2() const { return static_cast<std::uint8_t>((Bits1() & 0xF) >> 0); }

std::uint8_t AIAggro::VarA9() const { return static_cast<std::uint8_t>((Bits1() & 0xF0) >> 4); }

std::uint8_t AIAggro::VarA3() const { return static_cast<std::uint8_t>((Bits1() & 0xF00) >> 8); }

std::uint8_t AIAggro::VarA4() const { return static_cast<std::uint8_t>((Bits1() & 0xF000) >> 12); }

std::uint8_t AIAggro::VarA10() const { return static_cast<std::uint8_t>(Bits2() & 0xF); }

std::uint16_t AIAggro::VarA7() const { return static_cast<std::uint16_t>((Bits2() & 0xFFF0) >> 4); }

void AIAggro::UpdateSlots(const std::array<::fruityprime::memory::CPlayer*, 4>& players) {
    slot1_ = -1;
    slot2_ = -1;
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (players[index] != nullptr && Player1() == players[index]->address())
            slot1_ = static_cast<std::int32_t>(index);
        if (players[index] != nullptr && Player2() == players[index]->address())
            slot2_ = static_cast<std::int32_t>(index);
    }
}

CBeamProjectile::CBeamProjectile(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : CEntity(buffer, base_offset, address) {
    model_ = std::make_unique<::fruityprime::memory::CModel>(buffer, base_offset + 0x108, address + 0x108);
    sfx_parameters_ = std::make_unique<::fruityprime::memory::SfxParameters>(buffer, base_offset + 0x154, address + 0x154);
}

CBeamProjectile::CBeamProjectile(Buffer& buffer, std::uint32_t address)
    : CBeamProjectile(buffer, offset_for_address(buffer, address), address) {}

CBeamProjectile::~CBeamProjectile() = default;

::fruityprime::memory::CModel& CBeamProjectile::Model() noexcept { return *model_; }
const ::fruityprime::memory::CModel& CBeamProjectile::Model() const noexcept { return *model_; }

::fruityprime::memory::SfxParameters& CBeamProjectile::SfxParameters() noexcept { return *sfx_parameters_; }
const ::fruityprime::memory::SfxParameters& CBeamProjectile::SfxParameters() const noexcept { return *sfx_parameters_; }

CModel::CModel(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    node_animation_ = std::make_unique<::fruityprime::memory::CNodeAnimation>(buffer, base_offset + 0x10, address + 0x10);
}

CModel::CModel(Buffer& buffer, std::uint32_t address)
    : CModel(buffer, offset_for_address(buffer, address), address) {}

CModel::~CModel() = default;

::fruityprime::memory::CNodeAnimation& CModel::NodeAnimation() noexcept { return *node_animation_; }
const ::fruityprime::memory::CNodeAnimation& CModel::NodeAnimation() const noexcept { return *node_animation_; }

CNodeAnimation::CNodeAnimation(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

CNodeAnimation::CNodeAnimation(Buffer& buffer, std::uint32_t address)
    : CNodeAnimation(buffer, offset_for_address(buffer, address), address) {}

CNodeAnimation::~CNodeAnimation() = default;

EntityCollision::EntityCollision(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

EntityCollision::EntityCollision(Buffer& buffer, std::uint32_t address)
    : EntityCollision(buffer, offset_for_address(buffer, address), address) {}

EntityCollision::~EntityCollision() = default;

EquipInfoPtr::EquipInfoPtr(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

EquipInfoPtr::EquipInfoPtr(Buffer& buffer, std::uint32_t address)
    : EquipInfoPtr(buffer, offset_for_address(buffer, address), address) {}

EquipInfoPtr::~EquipInfoPtr() = default;

SfxParameters::SfxParameters(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

SfxParameters::SfxParameters(Buffer& buffer, std::uint32_t address)
    : SfxParameters(buffer, offset_for_address(buffer, address), address) {}

SfxParameters::~SfxParameters() = default;

CollisionVolume::CollisionVolume(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

CollisionVolume::CollisionVolume(Buffer& buffer, std::uint32_t address)
    : CollisionVolume(buffer, offset_for_address(buffer, address), address) {}

CollisionVolume::~CollisionVolume() = default;

Light::Light(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

Light::Light(Buffer& buffer, std::uint32_t address)
    : Light(buffer, offset_for_address(buffer, address), address) {}

Light::~Light() = default;

LightInfo::LightInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    light1_ = std::make_unique<::fruityprime::memory::Light>(buffer, base_offset + 0x0, address + 0x0);
    light2_ = std::make_unique<::fruityprime::memory::Light>(buffer, base_offset + 0x10, address + 0x10);
}

LightInfo::LightInfo(Buffer& buffer, std::uint32_t address)
    : LightInfo(buffer, offset_for_address(buffer, address), address) {}

LightInfo::~LightInfo() = default;

::fruityprime::memory::Light& LightInfo::Light1() noexcept { return *light1_; }
const ::fruityprime::memory::Light& LightInfo::Light1() const noexcept { return *light1_; }

::fruityprime::memory::Light& LightInfo::Light2() noexcept { return *light2_; }
const ::fruityprime::memory::Light& LightInfo::Light2() const noexcept { return *light2_; }

CameraInfo::CameraInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

CameraInfo::CameraInfo(Buffer& buffer, std::uint32_t address)
    : CameraInfo(buffer, offset_for_address(buffer, address), address) {}

CameraInfo::~CameraInfo() = default;

PlayerControls::PlayerControls(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

PlayerControls::PlayerControls(Buffer& buffer, std::uint32_t address)
    : PlayerControls(buffer, offset_for_address(buffer, address), address) {}

PlayerControls::~PlayerControls() = default;

PlayerInput::PlayerInput(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

PlayerInput::PlayerInput(Buffer& buffer, std::uint32_t address)
    : PlayerInput(buffer, offset_for_address(buffer, address), address) {}

PlayerInput::~PlayerInput() = default;

CameraSequence::CameraSequence(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    camera_info_ = std::make_unique<::fruityprime::memory::CameraInfo>(buffer, base_offset + 0x14, address + 0x14);
}

CameraSequence::CameraSequence(Buffer& buffer, std::uint32_t address)
    : CameraSequence(buffer, offset_for_address(buffer, address), address) {}

CameraSequence::~CameraSequence() = default;

::fruityprime::memory::CameraInfo& CameraSequence::CameraInfo() noexcept { return *camera_info_; }
const ::fruityprime::memory::CameraInfo& CameraSequence::CameraInfo() const noexcept { return *camera_info_; }

CameraSequenceKeyframe::CameraSequenceKeyframe(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    node_name_rest_ = std::make_unique<ByteArray>(buffer, base_offset + 0x58, 12, address + 0x58);
}

CameraSequenceKeyframe::CameraSequenceKeyframe(Buffer& buffer, std::uint32_t address)
    : CameraSequenceKeyframe(buffer, offset_for_address(buffer, address), address) {}

CameraSequenceKeyframe::~CameraSequenceKeyframe() = default;

ByteArray& CameraSequenceKeyframe::NodeNameRest() noexcept { return *node_name_rest_; }
const ByteArray& CameraSequenceKeyframe::NodeNameRest() const noexcept { return *node_name_rest_; }

GameState::GameState(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    field8_ = std::make_unique<ByteArray>(buffer, base_offset + 0x8, 4, address + 0x8);
    sensitivity_ = std::make_unique<Int32Array>(buffer, base_offset + 0x24, 4, address + 0x24);
    invert_something_ = std::make_unique<ByteArray>(buffer, base_offset + 0x34, 4, address + 0x34);
    field38_ = std::make_unique<ByteArray>(buffer, base_offset + 0x38, 4, address + 0x38);
    hunters_ = std::make_unique<ByteArray>(buffer, base_offset + 0x3C, 4, address + 0x3C);
    suit_colors_ = std::make_unique<ByteArray>(buffer, base_offset + 0x40, 4, address + 0x40);
    player_names_ = std::make_unique<ByteArray>(buffer, base_offset + 0x44, 4, address + 0x44);
    bot_encounter_state_ = std::make_unique<ByteArray>(buffer, base_offset + 0x48, 4, address + 0x48);
    team_ids_ = std::make_unique<ByteArray>(buffer, base_offset + 0x4C, 4, address + 0x4C);
    field_a0_ = std::make_unique<ByteArray>(buffer, base_offset + 0x50, 4, address + 0x50);
    bot_spawner_ent_ids_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x54, 4, address + 0x54);
    prime_time_ = std::make_unique<Int32Array>(buffer, base_offset + 0x60, 4, address + 0x60);
    field_c0_ = std::make_unique<Int32Array>(buffer, base_offset + 0x70, 4, address + 0x70);
    field160_ = std::make_unique<Int32Array>(buffer, base_offset + 0x110, 4, address + 0x110);
    field180_ = std::make_unique<Int32Array>(buffer, base_offset + 0x130, 4, address + 0x130);
    deaths_ = std::make_unique<Int32Array>(buffer, base_offset + 0x140, 4, address + 0x140);
    field1_a0_ = std::make_unique<Int32Array>(buffer, base_offset + 0x150, 4, address + 0x150);
    teamkills_maybe_ = std::make_unique<Int32Array>(buffer, base_offset + 0x170, 4, address + 0x170);
    suicides_maybe_ = std::make_unique<Int32Array>(buffer, base_offset + 0x180, 4, address + 0x180);
    field1_e0_ = std::make_unique<Int32Array>(buffer, base_offset + 0x190, 4, address + 0x190);
    headshots_maybe_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1A0, 4, address + 0x1A0);
    field200_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1B0, 4, address + 0x1B0);
    dmg_dealt_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1C0, 4, address + 0x1C0);
    dmg_max_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1D0, 4, address + 0x1D0);
    battle_points_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1E0, 4, address + 0x1E0);
    standings_ = std::make_unique<ByteArray>(buffer, base_offset + 0x200, 4, address + 0x200);
    kill_streaks_ = std::make_unique<ByteArray>(buffer, base_offset + 0x20C, 4, address + 0x20C);
    field260_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x210, 4, address + 0x210);
    field268_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x218, 4, address + 0x218);
    field270_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x220, 4, address + 0x220);
}

GameState::GameState(Buffer& buffer, std::uint32_t address)
    : GameState(buffer, offset_for_address(buffer, address), address) {}

GameState::~GameState() = default;

ByteArray& GameState::Field8() noexcept { return *field8_; }
const ByteArray& GameState::Field8() const noexcept { return *field8_; }

Int32Array& GameState::Sensitivity() noexcept { return *sensitivity_; }
const Int32Array& GameState::Sensitivity() const noexcept { return *sensitivity_; }

ByteArray& GameState::InvertSomething() noexcept { return *invert_something_; }
const ByteArray& GameState::InvertSomething() const noexcept { return *invert_something_; }

ByteArray& GameState::Field38() noexcept { return *field38_; }
const ByteArray& GameState::Field38() const noexcept { return *field38_; }

ByteArray& GameState::Hunters() noexcept { return *hunters_; }
const ByteArray& GameState::Hunters() const noexcept { return *hunters_; }

ByteArray& GameState::SuitColors() noexcept { return *suit_colors_; }
const ByteArray& GameState::SuitColors() const noexcept { return *suit_colors_; }

ByteArray& GameState::PlayerNames() noexcept { return *player_names_; }
const ByteArray& GameState::PlayerNames() const noexcept { return *player_names_; }

ByteArray& GameState::BotEncounterState() noexcept { return *bot_encounter_state_; }
const ByteArray& GameState::BotEncounterState() const noexcept { return *bot_encounter_state_; }

ByteArray& GameState::TeamIds() noexcept { return *team_ids_; }
const ByteArray& GameState::TeamIds() const noexcept { return *team_ids_; }

ByteArray& GameState::FieldA0() noexcept { return *field_a0_; }
const ByteArray& GameState::FieldA0() const noexcept { return *field_a0_; }

UInt16Array& GameState::BotSpawnerEntIds() noexcept { return *bot_spawner_ent_ids_; }
const UInt16Array& GameState::BotSpawnerEntIds() const noexcept { return *bot_spawner_ent_ids_; }

Int32Array& GameState::PrimeTime() noexcept { return *prime_time_; }
const Int32Array& GameState::PrimeTime() const noexcept { return *prime_time_; }

Int32Array& GameState::FieldC0() noexcept { return *field_c0_; }
const Int32Array& GameState::FieldC0() const noexcept { return *field_c0_; }

Int32Array& GameState::Field160() noexcept { return *field160_; }
const Int32Array& GameState::Field160() const noexcept { return *field160_; }

Int32Array& GameState::Field180() noexcept { return *field180_; }
const Int32Array& GameState::Field180() const noexcept { return *field180_; }

Int32Array& GameState::Deaths() noexcept { return *deaths_; }
const Int32Array& GameState::Deaths() const noexcept { return *deaths_; }

Int32Array& GameState::Field1A0() noexcept { return *field1_a0_; }
const Int32Array& GameState::Field1A0() const noexcept { return *field1_a0_; }

Int32Array& GameState::TeamkillsMaybe() noexcept { return *teamkills_maybe_; }
const Int32Array& GameState::TeamkillsMaybe() const noexcept { return *teamkills_maybe_; }

Int32Array& GameState::SuicidesMaybe() noexcept { return *suicides_maybe_; }
const Int32Array& GameState::SuicidesMaybe() const noexcept { return *suicides_maybe_; }

Int32Array& GameState::Field1E0() noexcept { return *field1_e0_; }
const Int32Array& GameState::Field1E0() const noexcept { return *field1_e0_; }

Int32Array& GameState::HeadshotsMaybe() noexcept { return *headshots_maybe_; }
const Int32Array& GameState::HeadshotsMaybe() const noexcept { return *headshots_maybe_; }

Int32Array& GameState::Field200() noexcept { return *field200_; }
const Int32Array& GameState::Field200() const noexcept { return *field200_; }

Int32Array& GameState::DmgDealt() noexcept { return *dmg_dealt_; }
const Int32Array& GameState::DmgDealt() const noexcept { return *dmg_dealt_; }

Int32Array& GameState::DmgMax() noexcept { return *dmg_max_; }
const Int32Array& GameState::DmgMax() const noexcept { return *dmg_max_; }

Int32Array& GameState::BattlePoints() noexcept { return *battle_points_; }
const Int32Array& GameState::BattlePoints() const noexcept { return *battle_points_; }

ByteArray& GameState::Standings() noexcept { return *standings_; }
const ByteArray& GameState::Standings() const noexcept { return *standings_; }

ByteArray& GameState::KillStreaks() noexcept { return *kill_streaks_; }
const ByteArray& GameState::KillStreaks() const noexcept { return *kill_streaks_; }

UInt16Array& GameState::Field260() noexcept { return *field260_; }
const UInt16Array& GameState::Field260() const noexcept { return *field260_; }

UInt16Array& GameState::Field268() noexcept { return *field268_; }
const UInt16Array& GameState::Field268() const noexcept { return *field268_; }

UInt16Array& GameState::Field270() noexcept { return *field270_; }
const UInt16Array& GameState::Field270() const noexcept { return *field270_; }

KioskGameState::KioskGameState(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    field6_ = std::make_unique<ByteArray>(buffer, base_offset + 0x6, 4, address + 0x6);
    sensitivity_ = std::make_unique<Int32Array>(buffer, base_offset + 0x20, 4, address + 0x20);
    invert_something_ = std::make_unique<ByteArray>(buffer, base_offset + 0x30, 4, address + 0x30);
    field34_ = std::make_unique<ByteArray>(buffer, base_offset + 0x34, 4, address + 0x34);
    hunters_ = std::make_unique<ByteArray>(buffer, base_offset + 0x38, 4, address + 0x38);
    suit_colors_ = std::make_unique<ByteArray>(buffer, base_offset + 0x3C, 4, address + 0x3C);
    player_names_ = std::make_unique<ByteArray>(buffer, base_offset + 0x40, 4, address + 0x40);
    bot_encounter_state_ = std::make_unique<ByteArray>(buffer, base_offset + 0x44, 4, address + 0x44);
    team_ids_ = std::make_unique<ByteArray>(buffer, base_offset + 0x48, 4, address + 0x48);
    field9_c_ = std::make_unique<ByteArray>(buffer, base_offset + 0x4C, 4, address + 0x4C);
    bot_spawner_ent_ids_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x50, 4, address + 0x50);
    prime_time_ = std::make_unique<Int32Array>(buffer, base_offset + 0x5C, 4, address + 0x5C);
    field_b_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x6C, 4, address + 0x6C);
    field17_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x12C, 4, address + 0x12C);
    field19_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x14C, 4, address + 0x14C);
    deaths_ = std::make_unique<Int32Array>(buffer, base_offset + 0x15C, 4, address + 0x15C);
    field1_b_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x16C, 4, address + 0x16C);
    field1_c_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x17C, 4, address + 0x17C);
    suicides_maybe_ = std::make_unique<Int32Array>(buffer, base_offset + 0x18C, 4, address + 0x18C);
    field1_f_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1AC, 4, address + 0x1AC);
    headshots_maybe_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1BC, 4, address + 0x1BC);
    field21_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1CC, 4, address + 0x1CC);
    dmg_dealt_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1DC, 4, address + 0x1DC);
    dmg_max_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1EC, 4, address + 0x1EC);
    battle_points_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1FC, 4, address + 0x1FC);
    field26_c_ = std::make_unique<ByteArray>(buffer, base_offset + 0x21C, 4, address + 0x21C);
    kill_streaks_ = std::make_unique<ByteArray>(buffer, base_offset + 0x228, 4, address + 0x228);
    field27_c_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x22C, 4, address + 0x22C);
    field284_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x234, 4, address + 0x234);
    field28_c_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x23C, 4, address + 0x23C);
}

KioskGameState::KioskGameState(Buffer& buffer, std::uint32_t address)
    : KioskGameState(buffer, offset_for_address(buffer, address), address) {}

KioskGameState::~KioskGameState() = default;

ByteArray& KioskGameState::Field6() noexcept { return *field6_; }
const ByteArray& KioskGameState::Field6() const noexcept { return *field6_; }

Int32Array& KioskGameState::Sensitivity() noexcept { return *sensitivity_; }
const Int32Array& KioskGameState::Sensitivity() const noexcept { return *sensitivity_; }

ByteArray& KioskGameState::InvertSomething() noexcept { return *invert_something_; }
const ByteArray& KioskGameState::InvertSomething() const noexcept { return *invert_something_; }

ByteArray& KioskGameState::Field34() noexcept { return *field34_; }
const ByteArray& KioskGameState::Field34() const noexcept { return *field34_; }

ByteArray& KioskGameState::Hunters() noexcept { return *hunters_; }
const ByteArray& KioskGameState::Hunters() const noexcept { return *hunters_; }

ByteArray& KioskGameState::SuitColors() noexcept { return *suit_colors_; }
const ByteArray& KioskGameState::SuitColors() const noexcept { return *suit_colors_; }

ByteArray& KioskGameState::PlayerNames() noexcept { return *player_names_; }
const ByteArray& KioskGameState::PlayerNames() const noexcept { return *player_names_; }

ByteArray& KioskGameState::BotEncounterState() noexcept { return *bot_encounter_state_; }
const ByteArray& KioskGameState::BotEncounterState() const noexcept { return *bot_encounter_state_; }

ByteArray& KioskGameState::TeamIds() noexcept { return *team_ids_; }
const ByteArray& KioskGameState::TeamIds() const noexcept { return *team_ids_; }

ByteArray& KioskGameState::Field9C() noexcept { return *field9_c_; }
const ByteArray& KioskGameState::Field9C() const noexcept { return *field9_c_; }

UInt16Array& KioskGameState::BotSpawnerEntIds() noexcept { return *bot_spawner_ent_ids_; }
const UInt16Array& KioskGameState::BotSpawnerEntIds() const noexcept { return *bot_spawner_ent_ids_; }

Int32Array& KioskGameState::PrimeTime() noexcept { return *prime_time_; }
const Int32Array& KioskGameState::PrimeTime() const noexcept { return *prime_time_; }

Int32Array& KioskGameState::FieldBC() noexcept { return *field_b_c_; }
const Int32Array& KioskGameState::FieldBC() const noexcept { return *field_b_c_; }

Int32Array& KioskGameState::Field17C() noexcept { return *field17_c_; }
const Int32Array& KioskGameState::Field17C() const noexcept { return *field17_c_; }

Int32Array& KioskGameState::Field19C() noexcept { return *field19_c_; }
const Int32Array& KioskGameState::Field19C() const noexcept { return *field19_c_; }

Int32Array& KioskGameState::Deaths() noexcept { return *deaths_; }
const Int32Array& KioskGameState::Deaths() const noexcept { return *deaths_; }

Int32Array& KioskGameState::Field1BC() noexcept { return *field1_b_c_; }
const Int32Array& KioskGameState::Field1BC() const noexcept { return *field1_b_c_; }

Int32Array& KioskGameState::Field1CC() noexcept { return *field1_c_c_; }
const Int32Array& KioskGameState::Field1CC() const noexcept { return *field1_c_c_; }

Int32Array& KioskGameState::SuicidesMaybe() noexcept { return *suicides_maybe_; }
const Int32Array& KioskGameState::SuicidesMaybe() const noexcept { return *suicides_maybe_; }

Int32Array& KioskGameState::Field1FC() noexcept { return *field1_f_c_; }
const Int32Array& KioskGameState::Field1FC() const noexcept { return *field1_f_c_; }

Int32Array& KioskGameState::HeadshotsMaybe() noexcept { return *headshots_maybe_; }
const Int32Array& KioskGameState::HeadshotsMaybe() const noexcept { return *headshots_maybe_; }

Int32Array& KioskGameState::Field21C() noexcept { return *field21_c_; }
const Int32Array& KioskGameState::Field21C() const noexcept { return *field21_c_; }

Int32Array& KioskGameState::DmgDealt() noexcept { return *dmg_dealt_; }
const Int32Array& KioskGameState::DmgDealt() const noexcept { return *dmg_dealt_; }

Int32Array& KioskGameState::DmgMax() noexcept { return *dmg_max_; }
const Int32Array& KioskGameState::DmgMax() const noexcept { return *dmg_max_; }

Int32Array& KioskGameState::BattlePoints() noexcept { return *battle_points_; }
const Int32Array& KioskGameState::BattlePoints() const noexcept { return *battle_points_; }

ByteArray& KioskGameState::Field26C() noexcept { return *field26_c_; }
const ByteArray& KioskGameState::Field26C() const noexcept { return *field26_c_; }

ByteArray& KioskGameState::KillStreaks() noexcept { return *kill_streaks_; }
const ByteArray& KioskGameState::KillStreaks() const noexcept { return *kill_streaks_; }

UInt16Array& KioskGameState::Field27C() noexcept { return *field27_c_; }
const UInt16Array& KioskGameState::Field27C() const noexcept { return *field27_c_; }

UInt16Array& KioskGameState::Field284() noexcept { return *field284_; }
const UInt16Array& KioskGameState::Field284() const noexcept { return *field284_; }

UInt16Array& KioskGameState::Field28C() noexcept { return *field28_c_; }
const UInt16Array& KioskGameState::Field28C() const noexcept { return *field28_c_; }

RoomState::RoomState(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    bits_ = std::make_unique<ByteArray>(buffer, base_offset + 0x0, 60, address + 0x0);
}

RoomState::RoomState(Buffer& buffer, std::uint32_t address)
    : RoomState(buffer, offset_for_address(buffer, address), address) {}

RoomState::~RoomState() = default;

ByteArray& RoomState::Bits() noexcept { return *bits_; }
const ByteArray& RoomState::Bits() const noexcept { return *bits_; }

StorySaveData::StorySaveData(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    weapon_slots_ = std::make_unique<ByteArray>(buffer, base_offset + 0x2, 3, address + 0x2);
    ammo_ = std::make_unique<UInt16Array>(buffer, base_offset + 0x6, 2, address + 0x6);
    ammo_caps_ = std::make_unique<UInt16Array>(buffer, base_offset + 0xA, 2, address + 0xA);
    visited_rooms_ = std::make_unique<ByteArray>(buffer, base_offset + 0x27, 9, address + 0x27);
    visited_connectors_ = std::make_unique<Int32Array>(buffer, base_offset + 0x30, 9, address + 0x30);
    room_state_ = std::make_unique<StructArray<::fruityprime::memory::RoomState>>(buffer, base_offset + 0x54, 66, 60, address + 0x54);
    field_f_c_c_ = std::make_unique<ByteArray>(buffer, base_offset + 0xFCC, 8, address + 0xFCC);
    trigger_state_bits_ = std::make_unique<ByteArray>(buffer, base_offset + 0x1014, 4, address + 0x1014);
    logbook_ = std::make_unique<ByteArray>(buffer, base_offset + 0x101C, 64, address + 0x101C);
    area_hunters_ = std::make_unique<ByteArray>(buffer, base_offset + 0x1080, 4, address + 0x1080);
}

StorySaveData::StorySaveData(Buffer& buffer, std::uint32_t address)
    : StorySaveData(buffer, offset_for_address(buffer, address), address) {}

StorySaveData::~StorySaveData() = default;

ByteArray& StorySaveData::WeaponSlots() noexcept { return *weapon_slots_; }
const ByteArray& StorySaveData::WeaponSlots() const noexcept { return *weapon_slots_; }

UInt16Array& StorySaveData::Ammo() noexcept { return *ammo_; }
const UInt16Array& StorySaveData::Ammo() const noexcept { return *ammo_; }

UInt16Array& StorySaveData::AmmoCaps() noexcept { return *ammo_caps_; }
const UInt16Array& StorySaveData::AmmoCaps() const noexcept { return *ammo_caps_; }

ByteArray& StorySaveData::VisitedRooms() noexcept { return *visited_rooms_; }
const ByteArray& StorySaveData::VisitedRooms() const noexcept { return *visited_rooms_; }

Int32Array& StorySaveData::VisitedConnectors() noexcept { return *visited_connectors_; }
const Int32Array& StorySaveData::VisitedConnectors() const noexcept { return *visited_connectors_; }

StructArray<::fruityprime::memory::RoomState>& StorySaveData::RoomState() noexcept { return *room_state_; }
const StructArray<::fruityprime::memory::RoomState>& StorySaveData::RoomState() const noexcept { return *room_state_; }

ByteArray& StorySaveData::FieldFCC() noexcept { return *field_f_c_c_; }
const ByteArray& StorySaveData::FieldFCC() const noexcept { return *field_f_c_c_; }

ByteArray& StorySaveData::TriggerStateBits() noexcept { return *trigger_state_bits_; }
const ByteArray& StorySaveData::TriggerStateBits() const noexcept { return *trigger_state_bits_; }

ByteArray& StorySaveData::Logbook() noexcept { return *logbook_; }
const ByteArray& StorySaveData::Logbook() const noexcept { return *logbook_; }

ByteArray& StorySaveData::AreaHunters() noexcept { return *area_hunters_; }
const ByteArray& StorySaveData::AreaHunters() const noexcept { return *area_hunters_; }

SaveType3::SaveType3(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

SaveType3::SaveType3(Buffer& buffer, std::uint32_t address)
    : SaveType3(buffer, offset_for_address(buffer, address), address) {}

SaveType3::~SaveType3() = default;

StatsAndSettings::StatsAndSettings(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

StatsAndSettings::StatsAndSettings(Buffer& buffer, std::uint32_t address)
    : StatsAndSettings(buffer, offset_for_address(buffer, address), address) {}

StatsAndSettings::~StatsAndSettings() = default;

LicenseInfo::LicenseInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    nickname_ = std::make_unique<ByteArray>(buffer, base_offset + 0x0, 24, address + 0x0);
    field64_ = std::make_unique<Int32Array>(buffer, base_offset + 0x64, 4, address + 0x64);
    field74_ = std::make_unique<Int32Array>(buffer, base_offset + 0x74, 7, address + 0x74);
    field90_ = std::make_unique<Int32Array>(buffer, base_offset + 0x90, 7, address + 0x90);
    field_a_c_ = std::make_unique<Int32Array>(buffer, base_offset + 0xAC, 9, address + 0xAC);
    field_d0_ = std::make_unique<Int32Array>(buffer, base_offset + 0xD0, 29, address + 0xD0);
    field144_ = std::make_unique<Int32Array>(buffer, base_offset + 0x144, 29, address + 0x144);
    field1_b8_ = std::make_unique<Int32Array>(buffer, base_offset + 0x1B8, 7, address + 0x1B8);
    field1_e4_ = std::make_unique<ByteArray>(buffer, base_offset + 0x1E4, 4, address + 0x1E4);
}

LicenseInfo::LicenseInfo(Buffer& buffer, std::uint32_t address)
    : LicenseInfo(buffer, offset_for_address(buffer, address), address) {}

LicenseInfo::~LicenseInfo() = default;

ByteArray& LicenseInfo::Nickname() noexcept { return *nickname_; }
const ByteArray& LicenseInfo::Nickname() const noexcept { return *nickname_; }

Int32Array& LicenseInfo::Field64() noexcept { return *field64_; }
const Int32Array& LicenseInfo::Field64() const noexcept { return *field64_; }

Int32Array& LicenseInfo::Field74() noexcept { return *field74_; }
const Int32Array& LicenseInfo::Field74() const noexcept { return *field74_; }

Int32Array& LicenseInfo::Field90() noexcept { return *field90_; }
const Int32Array& LicenseInfo::Field90() const noexcept { return *field90_; }

Int32Array& LicenseInfo::FieldAC() noexcept { return *field_a_c_; }
const Int32Array& LicenseInfo::FieldAC() const noexcept { return *field_a_c_; }

Int32Array& LicenseInfo::FieldD0() noexcept { return *field_d0_; }
const Int32Array& LicenseInfo::FieldD0() const noexcept { return *field_d0_; }

Int32Array& LicenseInfo::Field144() noexcept { return *field144_; }
const Int32Array& LicenseInfo::Field144() const noexcept { return *field144_; }

Int32Array& LicenseInfo::Field1B8() noexcept { return *field1_b8_; }
const Int32Array& LicenseInfo::Field1B8() const noexcept { return *field1_b8_; }

ByteArray& LicenseInfo::Field1E4() noexcept { return *field1_e4_; }
const ByteArray& LicenseInfo::Field1E4() const noexcept { return *field1_e4_; }

FriendsRivals::FriendsRivals(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    fields_ = std::make_unique<Int32Array>(buffer, base_offset + 0x0, 834, address + 0x0);
}

FriendsRivals::FriendsRivals(Buffer& buffer, std::uint32_t address)
    : FriendsRivals(buffer, offset_for_address(buffer, address), address) {}

FriendsRivals::~FriendsRivals() = default;

Int32Array& FriendsRivals::Fields() noexcept { return *fields_; }
const Int32Array& FriendsRivals::Fields() const noexcept { return *fields_; }

RoomDescription::RoomDescription(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

RoomDescription::RoomDescription(Buffer& buffer, std::uint32_t address)
    : RoomDescription(buffer, offset_for_address(buffer, address), address) {}

RoomDescription::~RoomDescription() = default;

EquipInfo::EquipInfo(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

EquipInfo::EquipInfo(Buffer& buffer, std::uint32_t address)
    : EquipInfo(buffer, offset_for_address(buffer, address), address) {}

EquipInfo::~EquipInfo() = default;

AiButton::AiButton(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

AiButton::AiButton(Buffer& buffer, std::uint32_t address)
    : AiButton(buffer, offset_for_address(buffer, address), address) {}

AiButton::~AiButton() = default;

VecFx32::VecFx32(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
}

VecFx32::VecFx32(Buffer& buffer, std::uint32_t address)
    : VecFx32(buffer, offset_for_address(buffer, address), address) {}

VecFx32::~VecFx32() = default;

MtxFx43::MtxFx43(Buffer& buffer, std::size_t base_offset, std::uint32_t address)
    : MemoryClass(buffer, base_offset, address) {
    m_ = std::make_unique<Int32Array>(buffer, base_offset + 0x0, 12, address + 0x0);
    row0_ = std::make_unique<::fruityprime::memory::VecFx32>(buffer, base_offset + 0x0, address + 0x0);
    row1_ = std::make_unique<::fruityprime::memory::VecFx32>(buffer, base_offset + 0xC, address + 0xC);
    row2_ = std::make_unique<::fruityprime::memory::VecFx32>(buffer, base_offset + 0x18, address + 0x18);
    row3_ = std::make_unique<::fruityprime::memory::VecFx32>(buffer, base_offset + 0x24, address + 0x24);
}

MtxFx43::MtxFx43(Buffer& buffer, std::uint32_t address)
    : MtxFx43(buffer, offset_for_address(buffer, address), address) {}

MtxFx43::~MtxFx43() = default;

Int32Array& MtxFx43::M() noexcept { return *m_; }
const Int32Array& MtxFx43::M() const noexcept { return *m_; }

::fruityprime::memory::VecFx32& MtxFx43::Row0() noexcept { return *row0_; }
const ::fruityprime::memory::VecFx32& MtxFx43::Row0() const noexcept { return *row0_; }

::fruityprime::memory::VecFx32& MtxFx43::Row1() noexcept { return *row1_; }
const ::fruityprime::memory::VecFx32& MtxFx43::Row1() const noexcept { return *row1_; }

::fruityprime::memory::VecFx32& MtxFx43::Row2() noexcept { return *row2_; }
const ::fruityprime::memory::VecFx32& MtxFx43::Row2() const noexcept { return *row2_; }

::fruityprime::memory::VecFx32& MtxFx43::Row3() noexcept { return *row3_; }
const ::fruityprime::memory::VecFx32& MtxFx43::Row3() const noexcept { return *row3_; }

} // namespace fruityprime::memory

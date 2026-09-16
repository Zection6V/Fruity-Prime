#include "MemoryClasses.hpp"

#include "Memory.hpp"
#include "MemoryArrays.hpp"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace MphRead::Memory
{
namespace
{
    [[nodiscard]] constexpr std::int32_t Add32(std::int32_t a, std::int32_t b) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) + std::bit_cast<std::uint32_t>(b));
    }

    [[nodiscard]] constexpr std::int32_t Mul32(std::int32_t a, std::int32_t b) noexcept
    {
        return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a) * std::bit_cast<std::uint32_t>(b));
    }

    [[nodiscard]] constexpr std::intptr_t AddPtr(std::intptr_t a, std::int32_t b) noexcept
    {
        using U = std::make_unsigned_t<std::intptr_t>;
        return static_cast<std::intptr_t>(static_cast<U>(a) + static_cast<U>(static_cast<std::intptr_t>(b)));
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakeChild(Memory& memory, std::int32_t address, std::int32_t offset)
    {
        return std::make_shared<T>(memory, Add32(address, offset));
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakeChild(Memory& memory, std::intptr_t address, std::int32_t offset)
    {
        return std::make_shared<T>(memory, AddPtr(address, offset));
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakePtrChild(Memory& memory, std::intptr_t address)
    {
        return std::make_shared<T>(memory, address);
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakeArray(Memory& memory, std::int32_t address, std::int32_t offset, std::int32_t count)
    {
        return std::make_shared<T>(memory, Add32(address, offset), count);
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakeArray(Memory& memory, std::intptr_t address, std::int32_t offset, std::int32_t count)
    {
        return std::make_shared<T>(memory, Detail::IntPtrAddress(AddPtr(address, offset)), count);
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> MakePtrArray(Memory& memory, std::intptr_t address, std::int32_t count)
    {
        return std::make_shared<T>(memory, Detail::IntPtrAddress(address), count);
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<StructArray<T>> MakeStructArray(Memory& memory, std::int32_t address, std::int32_t offset, std::int32_t count, std::int32_t size)
    {
        return std::make_shared<StructArray<T>>(memory, Add32(address, offset), count, size,
            [](Memory& m, std::int32_t a) { return std::make_shared<T>(m, a); });
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<StructArray<T>> MakeStructArray(Memory& memory, std::intptr_t address, std::int32_t offset, std::int32_t count, std::int32_t size)
    {
        return std::make_shared<StructArray<T>>(memory, Detail::IntPtrAddress(AddPtr(address, offset)), count, size,
            [](Memory& m, std::int32_t a) { return std::make_shared<T>(m, a); });
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<StructArray<T>> MakePtrStructArray(Memory& memory, std::intptr_t address, std::int32_t count, std::int32_t size)
    {
        return std::make_shared<StructArray<T>>(memory, Detail::IntPtrAddress(address), count, size,
            [](Memory& m, std::int32_t a) { return std::make_shared<T>(m, a); });
    }

    [[nodiscard]] std::int32_t FloatToInt32(float value) noexcept
    {
        if (std::isnan(value) || value >= 2147483648.0F || value < -2147483648.0F)
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::shared_ptr<MphRead::ManagedArray<std::uint8_t>> Bytes(std::size_t count)
    {
        return std::make_shared<MphRead::ManagedArray<std::uint8_t>>(count);
    }

    void Put16(MphRead::ManagedArray<std::uint8_t>& bytes, std::size_t at, std::uint16_t value)
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            bytes[at] = static_cast<std::uint8_t>(value);
            bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
        }
        else
        {
            bytes[at] = static_cast<std::uint8_t>(value >> 8);
            bytes[at + 1] = static_cast<std::uint8_t>(value);
        }
    }

    void Put32(MphRead::ManagedArray<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            bytes[at] = static_cast<std::uint8_t>(value);
            bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
            bytes[at + 2] = static_cast<std::uint8_t>(value >> 16);
            bytes[at + 3] = static_cast<std::uint8_t>(value >> 24);
        }
        else
        {
            bytes[at] = static_cast<std::uint8_t>(value >> 24);
            bytes[at + 1] = static_cast<std::uint8_t>(value >> 16);
            bytes[at + 2] = static_cast<std::uint8_t>(value >> 8);
            bytes[at + 3] = static_cast<std::uint8_t>(value);
        }
    }

    void CopyInt(std::int32_t value, MphRead::ManagedArray<std::uint8_t>& dest, std::int32_t index)
    {
        auto source = Bytes(4);
        Put32(*source, 0, std::bit_cast<std::uint32_t>(value));
        for (std::int32_t i = 0; i < 4; i = Add32(i, 1))
        {
            const std::int32_t destIndex = Add32(i, Mul32(index, 4));
            dest[static_cast<std::size_t>(destIndex)] = (*source)[static_cast<std::size_t>(i)];
        }
    }

    void CopyFixed(float value, MphRead::ManagedArray<std::uint8_t>& dest, std::int32_t index)
    {
        CopyInt(FloatToInt32(value * 4096.0F), dest, index);
    }

    void CheckBitConverterRange(
        const std::shared_ptr<MphRead::ManagedArray<std::uint8_t>>& buffer,
        std::int32_t startIndex, std::size_t width)
    {
        if (!buffer)
        {
            throw System::ArgumentNullException("value");
        }
        const std::size_t length = buffer->Length();
        if (startIndex < 0 || static_cast<std::size_t>(startIndex) >= length)
        {
            throw Detail::ArgumentOutOfRangeException();
        }
        const std::size_t start = static_cast<std::size_t>(startIndex);
        if (width > length - start)
        {
            throw std::invalid_argument("The array is too small for the requested value.");
        }
    }
}

MemoryClass::~MemoryClass() = default;

MemoryClass::MemoryClass(Memory& memory, std::int32_t address)
    : _memory(memory),
      _offset(Add32(address, -Memory::Offset)),
      _address(static_cast<std::intptr_t>(address))
{
}

MemoryClass::MemoryClass(Memory& memory, std::intptr_t address)
    : _memory(memory),
      _offset(Add32(Detail::IntPtrToInt32(Detail::IntPtrAddress(address)), -Memory::Offset)),
      _address(address)
{
}

bool MemoryClass::Equals(const MemoryClass* other) const noexcept
{
    return other != nullptr && Address() == other->Address();
}

std::int32_t MemoryClass::GetHashCode() const noexcept
{
    const auto raw = static_cast<std::uintptr_t>(Address());
    if constexpr (sizeof(std::intptr_t) > sizeof(std::int32_t))
    {
        const std::uint32_t bits = static_cast<std::uint32_t>(raw) ^ static_cast<std::uint32_t>(raw >> 32);
        return std::bit_cast<std::int32_t>(bits);
    }
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
}

bool operator==(const std::shared_ptr<MemoryClass>& left, const std::shared_ptr<MemoryClass>& right) noexcept
{
    if (!left) return !right;
    return left->Equals(right.get());
}

bool operator!=(const std::shared_ptr<MemoryClass>& left, const std::shared_ptr<MemoryClass>& right) noexcept
{
    return !(left == right);
}

std::int8_t MemoryClass::ReadSByte(std::int32_t offset) const
{
    return std::bit_cast<std::int8_t>(ReadByte(offset));
}

std::uint8_t MemoryClass::ReadByte(std::int32_t offset) const
{
    const auto buffer = _memory.Buffer();
    if (!buffer) throw System::NullReferenceException();
    const std::int32_t index = Add32(_offset, offset);
    if (index < 0 || static_cast<std::size_t>(index) >= buffer->Length())
    {
        throw Detail::IndexOutOfRangeException();
    }
    return (*buffer)[static_cast<std::size_t>(index)];
}

std::int16_t MemoryClass::ReadInt16(std::int32_t offset) const
{
    return std::bit_cast<std::int16_t>(ReadUInt16(offset));
}

std::uint16_t MemoryClass::ReadUInt16(std::int32_t offset) const
{
    const auto buffer = _memory.Buffer();
    const std::int32_t startIndex = Add32(_offset, offset);
    CheckBitConverterRange(buffer, startIndex, 2);
    const std::size_t start = static_cast<std::size_t>(startIndex);
    const std::uint16_t b0 = (*buffer)[start];
    const std::uint16_t b1 = (*buffer)[start + 1];
    if constexpr (std::endian::native == std::endian::little)
    {
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }
    return static_cast<std::uint16_t>((b0 << 8) | b1);
}

std::int32_t MemoryClass::ReadInt32(std::int32_t offset) const
{
    return std::bit_cast<std::int32_t>(ReadUInt32(offset));
}

std::uint32_t MemoryClass::ReadUInt32(std::int32_t offset) const
{
    const auto buffer = _memory.Buffer();
    const std::int32_t startIndex = Add32(_offset, offset);
    CheckBitConverterRange(buffer, startIndex, 4);
    const std::size_t start = static_cast<std::size_t>(startIndex);
    const std::uint32_t b0 = (*buffer)[start];
    const std::uint32_t b1 = (*buffer)[start + 1];
    const std::uint32_t b2 = (*buffer)[start + 2];
    const std::uint32_t b3 = (*buffer)[start + 3];
    if constexpr (std::endian::native == std::endian::little)
    {
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}
std::intptr_t MemoryClass::ReadPointer(std::int32_t offset) const { return static_cast<std::intptr_t>(ReadInt32(offset)); }
MphRead::ColorRgb MemoryClass::ReadColor3(std::int32_t offset) const
{
    const std::uint8_t red = ReadByte(offset);
    const std::uint8_t green = ReadByte(Add32(offset, 1));
    const std::uint8_t blue = ReadByte(Add32(offset, 2));
    return MphRead::ColorRgb(red, green, blue);
}
OpenTK::Mathematics::Vector3 MemoryClass::ReadVec3(std::int32_t offset) const
{
    const float x = ReadInt32(offset) / 4096.0F;
    const float y = ReadInt32(Add32(offset, 4)) / 4096.0F;
    const float z = ReadInt32(Add32(offset, 8)) / 4096.0F;
    return OpenTK::Mathematics::Vector3(x, y, z);
}
OpenTK::Mathematics::Vector4 MemoryClass::ReadVec4(std::int32_t offset) const
{
    const float x = ReadInt32(offset) / 4096.0F;
    const float y = ReadInt32(Add32(offset, 4)) / 4096.0F;
    const float z = ReadInt32(Add32(offset, 8)) / 4096.0F;
    const float w = ReadInt32(Add32(offset, 12)) / 4096.0F;
    return OpenTK::Mathematics::Vector4(x, y, z, w);
}
OpenTK::Mathematics::Matrix4x3 MemoryClass::ReadMtx43(std::int32_t offset) const
{
    const OpenTK::Mathematics::Vector3 row0 = ReadVec3(offset);
    const OpenTK::Mathematics::Vector3 row1 = ReadVec3(Add32(offset, 12));
    const OpenTK::Mathematics::Vector3 row2 = ReadVec3(Add32(offset, 24));
    const OpenTK::Mathematics::Vector3 row3 = ReadVec3(Add32(offset, 36));
    return OpenTK::Mathematics::Matrix4x3(row0, row1, row2, row3);
}
void MemoryClass::WriteSByte(std::int32_t offset, std::int8_t value) { WriteByte(offset, static_cast<std::uint8_t>(value)); }
void MemoryClass::WriteByte(std::int32_t offset, std::uint8_t value)
{
    auto bytes = Bytes(1); (*bytes)[0] = value; _memory.WriteMemory(AddPtr(Address(), offset), bytes, 1);
}
void MemoryClass::WriteInt16(std::int32_t offset, std::int16_t value) { WriteUInt16(offset, static_cast<std::uint16_t>(value)); }
void MemoryClass::WriteUInt16(std::int32_t offset, std::uint16_t value)
{
    auto bytes = Bytes(2); Put16(*bytes, 0, value); _memory.WriteMemory(AddPtr(Address(), offset), bytes, 2);
}
void MemoryClass::WriteInt32(std::int32_t offset, std::int32_t value) { WriteUInt32(offset, std::bit_cast<std::uint32_t>(value)); }
void MemoryClass::WriteUInt32(std::int32_t offset, std::uint32_t value)
{
    auto bytes = Bytes(4); Put32(*bytes, 0, value); _memory.WriteMemory(AddPtr(Address(), offset), bytes, 4);
}
void MemoryClass::WritePointer(std::int32_t offset, std::intptr_t value)
{
    WriteInt32(offset, Detail::IntPtrToInt32(Detail::IntPtrAddress(value)));
}
void MemoryClass::WriteColor3(std::int32_t offset, MphRead::ColorRgb value)
{
    auto bytes = Bytes(3); (*bytes)[0]=value.Red; (*bytes)[1]=value.Green; (*bytes)[2]=value.Blue; _memory.WriteMemory(AddPtr(Address(), offset), bytes, 3);
}
void MemoryClass::WriteVec3(std::int32_t offset, OpenTK::Mathematics::Vector3 value)
{
    auto bytes = Bytes(12);
    CopyFixed(value.X, *bytes, 0);
    CopyFixed(value.Y, *bytes, 1);
    CopyFixed(value.Z, *bytes, 2);
    _memory.WriteMemory(AddPtr(Address(), offset), bytes, 12);
}
void MemoryClass::WriteVec4(std::int32_t offset, OpenTK::Mathematics::Vector4 value)
{
    auto bytes = Bytes(16);
    CopyFixed(value.X, *bytes, 0);
    CopyFixed(value.Y, *bytes, 1);
    CopyFixed(value.Z, *bytes, 2);
    CopyFixed(value.W, *bytes, 3);
    _memory.WriteMemory(AddPtr(Address(), offset), bytes, 16);
}
void MemoryClass::WriteMtx43(std::int32_t offset, OpenTK::Mathematics::Matrix4x3 value)
{
    auto bytes = Bytes(48);
    CopyFixed(value.M11, *bytes, 0);
    CopyFixed(value.M12, *bytes, 1);
    CopyFixed(value.M13, *bytes, 2);
    CopyFixed(value.M21, *bytes, 3);
    CopyFixed(value.M22, *bytes, 4);
    CopyFixed(value.M23, *bytes, 5);
    CopyFixed(value.M31, *bytes, 6);
    CopyFixed(value.M32, *bytes, 7);
    CopyFixed(value.M33, *bytes, 8);
    CopyFixed(value.M41, *bytes, 9);
    CopyFixed(value.M42, *bytes, 10);
    CopyFixed(value.M43, *bytes, 11);
    _memory.WriteMemory(AddPtr(Address(), offset), bytes, 48);
}

CEntity::CEntity(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
CEntity::CEntity(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

CEnemyBase::CEnemyBase(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _HurtVolUnxf = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x8C);
    _HurtVol = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0xCC);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x118);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x160);
}

CEnemyBase::CEnemyBase(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _HurtVolUnxf = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x8C);
    _HurtVol = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0xCC);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x118);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x160);
}

CEnemy24::CEnemy24(Memory& memory, std::int32_t address) : CEnemyBase(memory, address)
{
    _Regen = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x178);
    _Arms = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1C8, 2);
    _Legs = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1D0, 3);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x1E8);
}

CEnemy24::CEnemy24(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address)
{
    _Regen = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x178);
    _Arms = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1C8, 2);
    _Legs = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1D0, 3);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x1E8);
}

CEnemy25::CEnemy25(Memory& memory, std::int32_t address) : CEnemyBase(memory, address) {}
CEnemy25::CEnemy25(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address) {}

CEnemy26::CEnemy26(Memory& memory, std::int32_t address) : CEnemyBase(memory, address)
{
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfo>(memory, address, 0x188);
}

CEnemy26::CEnemy26(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address)
{
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfo>(memory, address, 0x188);
}

CEnemy27::CEnemy27(Memory& memory, std::int32_t address) : CEnemyBase(memory, address) {}
CEnemy27::CEnemy27(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address) {}

CEnemy28::CEnemy28(Memory& memory, std::int32_t address) : CEnemyBase(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x17C);
    _Trocras = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1D0, 9);
}

CEnemy28::CEnemy28(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x17C);
    _Trocras = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1D0, 9);
}

Enemy29Fields::Enemy29Fields(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    const std::intptr_t vecsReference = VecsReference();
    const std::int32_t count2 = Count2();
    _Vecs = MakePtrStructArray<::MphRead::Memory::VecFx32>(memory, vecsReference, count2, 12);
    const std::intptr_t mtxsReference = MtxsReference();
    const std::int32_t mtxCount1 = Count1();
    _Mtxs = MakePtrStructArray<::MphRead::Memory::MtxFx43>(memory, mtxsReference, mtxCount1, 48);
    const std::intptr_t intsReference = IntsReference();
    const std::int32_t intsCount1 = Count1();
    _Ints = MakePtrArray<::MphRead::Memory::Int32Array>(memory, intsReference, intsCount1);
    const std::intptr_t shortsReference = ShortsReference();
    const std::int32_t shortsCount1 = Count1();
    _Shorts = MakePtrArray<::MphRead::Memory::Int16Array>(memory, shortsReference, shortsCount1);
}

Enemy29Fields::Enemy29Fields(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    const std::intptr_t vecsReference = VecsReference();
    const std::int32_t count2 = Count2();
    _Vecs = MakePtrStructArray<::MphRead::Memory::VecFx32>(memory, vecsReference, count2, 12);
    const std::intptr_t mtxsReference = MtxsReference();
    const std::int32_t mtxCount1 = Count1();
    _Mtxs = MakePtrStructArray<::MphRead::Memory::MtxFx43>(memory, mtxsReference, mtxCount1, 48);
    const std::intptr_t intsReference = IntsReference();
    const std::int32_t intsCount1 = Count1();
    _Ints = MakePtrArray<::MphRead::Memory::Int32Array>(memory, intsReference, intsCount1);
    const std::intptr_t shortsReference = ShortsReference();
    const std::int32_t shortsCount1 = Count1();
    _Shorts = MakePtrArray<::MphRead::Memory::Int16Array>(memory, shortsReference, shortsCount1);
}

CEnemy29::CEnemy29(Memory& memory, std::int32_t address) : CEnemyBase(memory, address)
{
    _MindTrick = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x178);
    _Grapple = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1C8);
    _Fields = MakePtrChild<::MphRead::Memory::Enemy29Fields>(memory, FieldsReference());
}

CEnemy29::CEnemy29(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address)
{
    _MindTrick = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x178);
    _Grapple = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1C8);
    _Fields = MakePtrChild<::MphRead::Memory::Enemy29Fields>(memory, FieldsReference());
}

CEnemy30::CEnemy30(Memory& memory, std::int32_t address) : CEnemyBase(memory, address) {}
CEnemy30::CEnemy30(Memory& memory, std::intptr_t address) : CEnemyBase(memory, address) {}

CPlatform::CPlatform(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x94);
    _EntityCollision = MakeChild<::MphRead::Memory::EntityCollision>(memory, address, 0x1A8);
    _Turrets = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x262, 4);
    _Effects = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x26C, 4);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x298);
    _LifetimeEventIndices = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x2F0, 4);
    _LifetimeEventTargets = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x2F4, 4);
    _LifetimeEventIds = MakeArray<::MphRead::Memory::U32EnumArray<Message>>(memory, address, 0x304, 4);
    _LifetimeEventParam1s = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x314, 4);
    _LifetimeEventParam2s = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x324, 4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x334);
}

CPlatform::CPlatform(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x94);
    _EntityCollision = MakeChild<::MphRead::Memory::EntityCollision>(memory, address, 0x1A8);
    _Turrets = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x262, 4);
    _Effects = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x26C, 4);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x298);
    _LifetimeEventIndices = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x2F0, 4);
    _LifetimeEventTargets = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x2F4, 4);
    _LifetimeEventIds = MakeArray<::MphRead::Memory::U32EnumArray<Message>>(memory, address, 0x304, 4);
    _LifetimeEventParam1s = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x314, 4);
    _LifetimeEventParam2s = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x324, 4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x334);
}

CObject::CObject(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _ColStructs = MakeStructArray<::MphRead::Memory::EntityCollision>(memory, address, 0x88, 2, 180);
    _MtxObjs = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1F0, 2);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1F8);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x250);
    _Sfx = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x298);
}

CObject::CObject(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _ColStructs = MakeStructArray<::MphRead::Memory::EntityCollision>(memory, address, 0x88, 2, 180);
    _MtxObjs = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x1F0, 2);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1F8);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x250);
    _Sfx = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x298);
}

CPlayerSpawn::CPlayerSpawn(Memory& memory, std::int32_t address) : CEntity(memory, address) {}
CPlayerSpawn::CPlayerSpawn(Memory& memory, std::intptr_t address) : CEntity(memory, address) {}

CDoor::CDoor(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _DoorModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _LockModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x100);
}

CDoor::CDoor(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _DoorModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _LockModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x100);
}

CItemSpawn::CItemSpawn(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x64);
}

CItemSpawn::CItemSpawn(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x64);
}

CItemInstance::CItemInstance(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x58);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xA0);
}

CItemInstance::CItemInstance(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x58);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xA0);
}

CEnemySpawn::CEnemySpawn(Memory& memory, std::int32_t address) : CEntity(memory, address) {}
CEnemySpawn::CEnemySpawn(Memory& memory, std::intptr_t address) : CEntity(memory, address) {}

CTriggerVolume::CTriggerVolume(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x38);
}

CTriggerVolume::CTriggerVolume(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x38);
}

CAreaVolume::CAreaVolume(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _TriggeredSlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3F, 4);
    _PrioritySlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x43, 4);
    _CooldownSlots = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x68, 4);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x7C);
}

CAreaVolume::CAreaVolume(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _TriggeredSlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3F, 4);
    _PrioritySlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x43, 4);
    _CooldownSlots = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x68, 4);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x7C);
}

CJumpPad::CJumpPad(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _FieldE4 = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0xE4);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x124);
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x168);
    _BeamModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1B0);
}

CJumpPad::CJumpPad(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _FieldE4 = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0xE4);
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x124);
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x168);
    _BeamModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1B0);
}

CPointModule::CPointModule(Memory& memory, std::int32_t address) : CEntity(memory, address) {}
CPointModule::CPointModule(Memory& memory, std::intptr_t address) : CEntity(memory, address) {}

CMorphCamera::CMorphCamera(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x24);
}

CMorphCamera::CMorphCamera(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x24);
}

COctolithFlag::COctolithFlag(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _OctoModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
}

COctolithFlag::COctolithFlag(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _OctoModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
}

CFlagBase::CFlagBase(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x3C);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x80);
}

CFlagBase::CFlagBase(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x3C);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x80);
}

CTeleporter::CTeleporter(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _TeleModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _ArtifactModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xEC);
}

CTeleporter::CTeleporter(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _TeleModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x5C);
    _ArtifactModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xA4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xEC);
}

CNodeDefense::CNodeDefense(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x44);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x98);
    _RingModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x9C);
    _NodeModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xE4);
}

CNodeDefense::CNodeDefense(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x44);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x98);
    _RingModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x9C);
    _NodeModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xE4);
}

CLightSource::CLightSource(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x1C);
}

CLightSource::CLightSource(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Volume = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x1C);
}

CArtifact::CArtifact(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _ArtifactModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x7C);
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xC4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x110);
}

CArtifact::CArtifact(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _ArtifactModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x7C);
    _BaseModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0xC4);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x110);
}

CCameraSequence::CCameraSequence(Memory& memory, std::int32_t address) : CEntity(memory, address) {}
CCameraSequence::CCameraSequence(Memory& memory, std::intptr_t address) : CEntity(memory, address) {}

CForceField::CForceField(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x44);
}

CForceField::CForceField(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x44);
}

CBeamEffect::CBeamEffect(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x78);
}

CBeamEffect::CBeamEffect(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x78);
}

CBomb::CBomb(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x68);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xB4);
}

CBomb::CBomb(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x68);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xB4);
}

CHalfturret::CHalfturret(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _LightInfo = MakeChild<::MphRead::Memory::LightInfo>(memory, address, 0x48);
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x68);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x80);
}

CHalfturret::CHalfturret(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _LightInfo = MakeChild<::MphRead::Memory::LightInfo>(memory, address, 0x48);
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x68);
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x80);
}

CPlayer::CPlayer(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Field100 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x100, 2);
    _Collision = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x108);
    _GunModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x15C);
    _FrozenModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1A4);
    _SpineNode = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x21C, 2);
    _ShootNode = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x224, 2);
    _Biped1 = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x22C);
    _Biped2 = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x274);
    _AltForm = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x2C0);
    _GunSmoke = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x308);
    _Controls = MakeChild<::MphRead::Memory::PlayerControls>(memory, address, 0x364);
    _Input = MakeChild<::MphRead::Memory::PlayerInput>(memory, address, 0x464);
    _CameraInfo = MakeChild<::MphRead::Memory::CameraInfo>(memory, address, 0x55C);
    _LightInfo = MakeChild<::MphRead::Memory::LightInfo>(memory, address, 0x694);
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x850);
    _BeamHead = MakeChild<::MphRead::Memory::CBeamProjectile>(memory, address, 0x864);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xF28);
    if (AiDataPtr() != 0)
    {
        _AiData = MakePtrChild<::MphRead::Memory::AiData>(memory, AiDataPtr());
        auto offset = AddPtr(AiDataPtr(), 0x2FC);
        _AIContext = MakePtrStructArray<::MphRead::Memory::AIContext>(memory, offset, 20, 0xA8);
        offset = AddPtr(AiDataPtr(), 0x1064);
        _AIAggro = MakePtrStructArray<::MphRead::Memory::AIAggro>(memory, offset, 25, 0x10);
    }
}

CPlayer::CPlayer(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Field100 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x100, 2);
    _Collision = MakeChild<::MphRead::Memory::CollisionVolume>(memory, address, 0x108);
    _GunModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x15C);
    _FrozenModel = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x1A4);
    _SpineNode = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x21C, 2);
    _ShootNode = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x224, 2);
    _Biped1 = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x22C);
    _Biped2 = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x274);
    _AltForm = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x2C0);
    _GunSmoke = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x308);
    _Controls = MakeChild<::MphRead::Memory::PlayerControls>(memory, address, 0x364);
    _Input = MakeChild<::MphRead::Memory::PlayerInput>(memory, address, 0x464);
    _CameraInfo = MakeChild<::MphRead::Memory::CameraInfo>(memory, address, 0x55C);
    _LightInfo = MakeChild<::MphRead::Memory::LightInfo>(memory, address, 0x694);
    _EquipInfo = MakeChild<::MphRead::Memory::EquipInfoPtr>(memory, address, 0x850);
    _BeamHead = MakeChild<::MphRead::Memory::CBeamProjectile>(memory, address, 0x864);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0xF28);
    if (AiDataPtr() != 0)
    {
        _AiData = MakePtrChild<::MphRead::Memory::AiData>(memory, AiDataPtr());
        auto offset = AddPtr(AiDataPtr(), 0x2FC);
        _AIContext = MakePtrStructArray<::MphRead::Memory::AIContext>(memory, offset, 20, 0xA8);
        offset = AddPtr(AiDataPtr(), 0x1064);
        _AIAggro = MakePtrStructArray<::MphRead::Memory::AIAggro>(memory, offset, 25, 0x10);
    }
}

std::uint32_t CPlayer::AggroCount() const
{
    const std::int32_t relative = Add32(Add32(Add32(Detail::IntPtrToInt32(Detail::IntPtrAddress(AiDataPtr())), -Memory::Offset), -_offset), 0x1060);
    return ReadUInt32(relative);
}
void CPlayer::AggroCount(std::uint32_t value)
{
    const std::int32_t relative = Add32(Add32(Add32(Detail::IntPtrToInt32(Detail::IntPtrAddress(AiDataPtr())), -Memory::Offset), -_offset), 0x1060);
    WriteUInt32(relative, value);
}

AiData::AiData(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _CurNodeTypeIndex = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x24, 6);
    _Field4C = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x4C, 11);
    _Field7A = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x7A, 10);
    _SlotsHitTotal = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xF0, 4);
    _SlotsDamageTotal = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x100, 4);
    _EntList = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x120, 78);
    _Buttons = MakeStructArray<::MphRead::Memory::AiButton>(memory, address, 0x258, 12, 6);
    _TouchBtns = MakeStructArray<::MphRead::Memory::AiButton>(memory, address, 0x2AA, 11, 6);
    _FuncTree = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x2FC, 840);
    _Aggro = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1064, 100);
}

AiData::AiData(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _CurNodeTypeIndex = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x24, 6);
    _Field4C = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x4C, 11);
    _Field7A = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x7A, 10);
    _SlotsHitTotal = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xF0, 4);
    _SlotsDamageTotal = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x100, 4);
    _EntList = MakeArray<::MphRead::Memory::IntPtrArray>(memory, address, 0x120, 78);
    _Buttons = MakeStructArray<::MphRead::Memory::AiButton>(memory, address, 0x258, 12, 6);
    _TouchBtns = MakeStructArray<::MphRead::Memory::AiButton>(memory, address, 0x2AA, 11, 6);
    _FuncTree = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x2FC, 840);
    _Aggro = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1064, 100);
}

AIContext::AIContext(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Weights = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x54, 21);
}

AIContext::AIContext(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Weights = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x54, 21);
}

std::shared_ptr<::MphRead::Memory::AIData1> AIContext::AIData1()
{
    static_assert(sizeof(std::intptr_t) >= sizeof(std::int32_t));
    if (CurData1Iter() != _lastData1Ptr)
    {
        if (CurData1Iter() == 0) _AIData1.reset();
        else _AIData1 = MakePtrChild<::MphRead::Memory::AIData1>(_memory, CurData1Iter());
        _lastData1Ptr = CurData1Iter();
    }
    return _AIData1;
}

AIData1::AIData1(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    if (Data1Ptr() != 0 && Data1Count() > 0)
    {
        const std::intptr_t data1Ptr = Data1Ptr();
        const std::int32_t data1Count = Data1Count();
        _Data1 = MakePtrStructArray<::MphRead::Memory::AIData1>(memory, data1Ptr, data1Count, 0x24);
    }
    if (Data2Ptr() != 0 && Data2Count() > 0)
    {
        const std::intptr_t data2Ptr = Data2Ptr();
        const std::int32_t data2Count = Data2Count();
        _Data2 = MakePtrStructArray<::MphRead::Memory::AIData2>(memory, data2Ptr, data2Count, 0x18);
    }
}

AIData1::AIData1(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    if (Data1Ptr() != 0 && Data1Count() > 0)
    {
        const std::intptr_t data1Ptr = Data1Ptr();
        const std::int32_t data1Count = Data1Count();
        _Data1 = MakePtrStructArray<::MphRead::Memory::AIData1>(memory, data1Ptr, data1Count, 0x24);
    }
    if (Data2Ptr() != 0 && Data2Count() > 0)
    {
        const std::intptr_t data2Ptr = Data2Ptr();
        const std::int32_t data2Count = Data2Count();
        _Data2 = MakePtrStructArray<::MphRead::Memory::AIData2>(memory, data2Ptr, data2Count, 0x18);
    }
}

AIData2::AIData2(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
AIData2::AIData2(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

AIAggro::AIAggro(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
AIAggro::AIAggro(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

void AIAggro::UpdateSlots(std::shared_ptr<MphRead::ManagedArray<std::shared_ptr<CPlayer>>> players)
{
    _slot1 = -1;
    _slot2 = -1;
    for (std::int32_t i = 0; i < 4; ++i)
    {
        const std::intptr_t player1Address = Player1();
        if (!players) throw System::NullReferenceException();
        if (static_cast<std::size_t>(i) >= players->Length()) throw Detail::IndexOutOfRangeException();
        const auto player1 = (*players)[static_cast<std::size_t>(i)];
        if (!player1) throw System::NullReferenceException();
        if (player1Address == player1->Address()) _slot1 = i;

        const std::intptr_t player2Address = Player2();
        if (!players) throw System::NullReferenceException();
        if (static_cast<std::size_t>(i) >= players->Length()) throw Detail::IndexOutOfRangeException();
        const auto player2 = (*players)[static_cast<std::size_t>(i)];
        if (!player2) throw System::NullReferenceException();
        if (player2Address == player2->Address()) _slot2 = i;
    }
}

std::uint8_t AIAggro::VarA2() const { return static_cast<std::uint8_t>(Bits1() & 0xF); }
std::uint8_t AIAggro::VarA9() const { return static_cast<std::uint8_t>((Bits1() & 0xF0) >> 4); }
std::uint8_t AIAggro::VarA3() const { return static_cast<std::uint8_t>((Bits1() & 0xF00) >> 8); }
std::uint8_t AIAggro::VarA4() const { return static_cast<std::uint8_t>((Bits1() & 0xF000) >> 12); }
std::uint8_t AIAggro::VarA10() const { return static_cast<std::uint8_t>(Bits2() & 0xF); }
std::uint16_t AIAggro::VarA7() const { return static_cast<std::uint16_t>((Bits2() & 0xFFF0) >> 4); }

CBeamProjectile::CBeamProjectile(Memory& memory, std::int32_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x108);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x154);
}

CBeamProjectile::CBeamProjectile(Memory& memory, std::intptr_t address) : CEntity(memory, address)
{
    _Model = MakeChild<::MphRead::Memory::CModel>(memory, address, 0x108);
    _SfxParameters = MakeChild<::MphRead::Memory::SfxParameters>(memory, address, 0x154);
}

CModel::CModel(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _NodeAnimation = MakeChild<::MphRead::Memory::CNodeAnimation>(memory, address, 0x10);
}

CModel::CModel(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _NodeAnimation = MakeChild<::MphRead::Memory::CNodeAnimation>(memory, address, 0x10);
}

CNodeAnimation::CNodeAnimation(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
CNodeAnimation::CNodeAnimation(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

EntityCollision::EntityCollision(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
EntityCollision::EntityCollision(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

EquipInfoPtr::EquipInfoPtr(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
EquipInfoPtr::EquipInfoPtr(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

SfxParameters::SfxParameters(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
SfxParameters::SfxParameters(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

CollisionVolume::CollisionVolume(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
CollisionVolume::CollisionVolume(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

Light::Light(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
Light::Light(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

LightInfo::LightInfo(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Light1 = MakeChild<::MphRead::Memory::Light>(memory, address, 0x0);
    _Light2 = MakeChild<::MphRead::Memory::Light>(memory, address, 0x10);
}

LightInfo::LightInfo(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Light1 = MakeChild<::MphRead::Memory::Light>(memory, address, 0x0);
    _Light2 = MakeChild<::MphRead::Memory::Light>(memory, address, 0x10);
}

CameraInfo::CameraInfo(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
CameraInfo::CameraInfo(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

PlayerControls::PlayerControls(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
PlayerControls::PlayerControls(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

PlayerInput::PlayerInput(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
PlayerInput::PlayerInput(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

CameraSequence::CameraSequence(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _CameraInfo = MakeChild<::MphRead::Memory::CameraInfo>(memory, address, 0x14);
}

CameraSequence::CameraSequence(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _CameraInfo = MakeChild<::MphRead::Memory::CameraInfo>(memory, address, 0x14);
}

CameraSequenceKeyframe::CameraSequenceKeyframe(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _NodeNameRest = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x58, 12);
}

CameraSequenceKeyframe::CameraSequenceKeyframe(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _NodeNameRest = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x58, 12);
}

GameState::GameState(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Field8 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x8, 4);
    _Sensitivity = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x24, 4);
    _InvertSomething = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x34, 4);
    _Field38 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x38, 4);
    _Hunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3C, 4);
    _SuitColors = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x40, 4);
    _PlayerNames = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x44, 4);
    _BotEncounterState = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x48, 4);
    _TeamIds = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x4C, 4);
    _FieldA0 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x50, 4);
    _BotSpawnerEntIds = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x54, 4);
    _PrimeTime = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x60, 4);
    _FieldC0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x70, 4);
    _Field160 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x110, 4);
    _Field180 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x130, 4);
    _Deaths = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x140, 4);
    _Field1A0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x150, 4);
    _TeamkillsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x170, 4);
    _SuicidesMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x180, 4);
    _Field1E0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x190, 4);
    _HeadshotsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1A0, 4);
    _Field200 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1B0, 4);
    _DmgDealt = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1C0, 4);
    _DmgMax = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1D0, 4);
    _BattlePoints = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1E0, 4);
    _Standings = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x200, 4);
    _KillStreaks = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x20C, 4);
    _Field260 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x210, 4);
    _Field268 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x218, 4);
    _Field270 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x220, 4);
}

GameState::GameState(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Field8 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x8, 4);
    _Sensitivity = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x24, 4);
    _InvertSomething = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x34, 4);
    _Field38 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x38, 4);
    _Hunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3C, 4);
    _SuitColors = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x40, 4);
    _PlayerNames = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x44, 4);
    _BotEncounterState = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x48, 4);
    _TeamIds = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x4C, 4);
    _FieldA0 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x50, 4);
    _BotSpawnerEntIds = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x54, 4);
    _PrimeTime = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x60, 4);
    _FieldC0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x70, 4);
    _Field160 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x110, 4);
    _Field180 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x130, 4);
    _Deaths = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x140, 4);
    _Field1A0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x150, 4);
    _TeamkillsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x170, 4);
    _SuicidesMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x180, 4);
    _Field1E0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x190, 4);
    _HeadshotsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1A0, 4);
    _Field200 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1B0, 4);
    _DmgDealt = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1C0, 4);
    _DmgMax = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1D0, 4);
    _BattlePoints = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1E0, 4);
    _Standings = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x200, 4);
    _KillStreaks = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x20C, 4);
    _Field260 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x210, 4);
    _Field268 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x218, 4);
    _Field270 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x220, 4);
}

KioskGameState::KioskGameState(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Field6 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x6, 4);
    _Sensitivity = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x20, 4);
    _InvertSomething = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x30, 4);
    _Field34 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x34, 4);
    _Hunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x38, 4);
    _SuitColors = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3C, 4);
    _PlayerNames = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x40, 4);
    _BotEncounterState = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x44, 4);
    _TeamIds = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x48, 4);
    _Field9C = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x4C, 4);
    _BotSpawnerEntIds = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x50, 4);
    _PrimeTime = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x5C, 4);
    _FieldBC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x6C, 4);
    _Field17C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x12C, 4);
    _Field19C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x14C, 4);
    _Deaths = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x15C, 4);
    _Field1BC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x16C, 4);
    _Field1CC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x17C, 4);
    _SuicidesMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x18C, 4);
    _Field1FC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1AC, 4);
    _HeadshotsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1BC, 4);
    _Field21C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1CC, 4);
    _DmgDealt = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1DC, 4);
    _DmgMax = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1EC, 4);
    _BattlePoints = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1FC, 4);
    _Field26C = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x21C, 4);
    _KillStreaks = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x228, 4);
    _Field27C = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x22C, 4);
    _Field284 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x234, 4);
    _Field28C = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x23C, 4);
}

KioskGameState::KioskGameState(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Field6 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x6, 4);
    _Sensitivity = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x20, 4);
    _InvertSomething = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x30, 4);
    _Field34 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x34, 4);
    _Hunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x38, 4);
    _SuitColors = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x3C, 4);
    _PlayerNames = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x40, 4);
    _BotEncounterState = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x44, 4);
    _TeamIds = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x48, 4);
    _Field9C = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x4C, 4);
    _BotSpawnerEntIds = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x50, 4);
    _PrimeTime = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x5C, 4);
    _FieldBC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x6C, 4);
    _Field17C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x12C, 4);
    _Field19C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x14C, 4);
    _Deaths = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x15C, 4);
    _Field1BC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x16C, 4);
    _Field1CC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x17C, 4);
    _SuicidesMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x18C, 4);
    _Field1FC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1AC, 4);
    _HeadshotsMaybe = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1BC, 4);
    _Field21C = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1CC, 4);
    _DmgDealt = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1DC, 4);
    _DmgMax = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1EC, 4);
    _BattlePoints = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1FC, 4);
    _Field26C = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x21C, 4);
    _KillStreaks = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x228, 4);
    _Field27C = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x22C, 4);
    _Field284 = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x234, 4);
    _Field28C = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x23C, 4);
}

RoomState::RoomState(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Bits = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x0, 60);
}

RoomState::RoomState(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Bits = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x0, 60);
}

StorySaveData::StorySaveData(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _WeaponSlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x2, 3);
    _Ammo = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x6, 2);
    _AmmoCaps = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0xA, 2);
    _VisitedRooms = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x27, 9);
    _VisitedConnectors = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x30, 9);
    _RoomState = MakeStructArray<::MphRead::Memory::RoomState>(memory, address, 0x54, 66, 60);
    _FieldFCC = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0xFCC, 8);
    _TriggerStateBits = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1014, 4);
    _Logbook = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x101C, 64);
    _AreaHunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1080, 4);
}

StorySaveData::StorySaveData(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _WeaponSlots = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x2, 3);
    _Ammo = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0x6, 2);
    _AmmoCaps = MakeArray<::MphRead::Memory::UInt16Array>(memory, address, 0xA, 2);
    _VisitedRooms = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x27, 9);
    _VisitedConnectors = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x30, 9);
    _RoomState = MakeStructArray<::MphRead::Memory::RoomState>(memory, address, 0x54, 66, 60);
    _FieldFCC = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0xFCC, 8);
    _TriggerStateBits = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1014, 4);
    _Logbook = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x101C, 64);
    _AreaHunters = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1080, 4);
}

SaveType3::SaveType3(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
SaveType3::SaveType3(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

StatsAndSettings::StatsAndSettings(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
StatsAndSettings::StatsAndSettings(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

LicenseInfo::LicenseInfo(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Nickname = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x0, 24);
    _Field64 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x64, 4);
    _Field74 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x74, 7);
    _Field90 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x90, 7);
    _FieldAC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xAC, 9);
    _FieldD0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xD0, 29);
    _Field144 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x144, 29);
    _Field1B8 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1B8, 7);
    _Field1E4 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1E4, 4);
}

LicenseInfo::LicenseInfo(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Nickname = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x0, 24);
    _Field64 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x64, 4);
    _Field74 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x74, 7);
    _Field90 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x90, 7);
    _FieldAC = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xAC, 9);
    _FieldD0 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0xD0, 29);
    _Field144 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x144, 29);
    _Field1B8 = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x1B8, 7);
    _Field1E4 = MakeArray<::MphRead::Memory::ByteArray>(memory, address, 0x1E4, 4);
}

FriendsRivals::FriendsRivals(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _Fields = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x0, 834);
}

FriendsRivals::FriendsRivals(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _Fields = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x0, 834);
}

RoomDescription::RoomDescription(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
RoomDescription::RoomDescription(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

EquipInfo::EquipInfo(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
EquipInfo::EquipInfo(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

AiButton::AiButton(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
AiButton::AiButton(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

VecFx32::VecFx32(Memory& memory, std::int32_t address) : MemoryClass(memory, address) {}
VecFx32::VecFx32(Memory& memory, std::intptr_t address) : MemoryClass(memory, address) {}

MtxFx43::MtxFx43(Memory& memory, std::int32_t address) : MemoryClass(memory, address)
{
    _M = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x0, 12);
    _Row0 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x0);
    _Row1 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0xC);
    _Row2 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x18);
    _Row3 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x24);
}

MtxFx43::MtxFx43(Memory& memory, std::intptr_t address) : MemoryClass(memory, address)
{
    _M = MakeArray<::MphRead::Memory::Int32Array>(memory, address, 0x0, 12);
    _Row0 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x0);
    _Row1 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0xC);
    _Row2 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x18);
    _Row3 = MakeChild<::MphRead::Memory::VecFx32>(memory, address, 0x24);
}

}

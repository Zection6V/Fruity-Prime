#include "MemoryArrays.hpp"

namespace MphRead::Memory
{
    SByteArray::SByteArray(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    SByteArray::SByteArray(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::int8_t SByteArray::Get(std::int32_t index)
    {
        return ReadSByte(Detail::UncheckedMultiply(index, 1));
    }

    void SByteArray::Set(std::int32_t index, std::int8_t value)
    {
        WriteSByte(Detail::UncheckedMultiply(index, 1), value);
    }

    ByteArray::ByteArray(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    ByteArray::ByteArray(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::uint8_t ByteArray::Get(std::int32_t index)
    {
        return ReadByte(Detail::UncheckedMultiply(index, 1));
    }

    void ByteArray::Set(std::int32_t index, std::uint8_t value)
    {
        WriteByte(Detail::UncheckedMultiply(index, 1), value);
    }

    Int16Array::Int16Array(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    Int16Array::Int16Array(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::int16_t Int16Array::Get(std::int32_t index)
    {
        return ReadInt16(Detail::UncheckedMultiply(index, 2));
    }

    void Int16Array::Set(std::int32_t index, std::int16_t value)
    {
        WriteInt16(Detail::UncheckedMultiply(index, 2), value);
    }

    UInt16Array::UInt16Array(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    UInt16Array::UInt16Array(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::uint16_t UInt16Array::Get(std::int32_t index)
    {
        return ReadUInt16(Detail::UncheckedMultiply(index, 2));
    }

    void UInt16Array::Set(std::int32_t index, std::uint16_t value)
    {
        WriteUInt16(Detail::UncheckedMultiply(index, 2), value);
    }

    Int32Array::Int32Array(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    Int32Array::Int32Array(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::int32_t Int32Array::Get(std::int32_t index)
    {
        return ReadInt32(Detail::UncheckedMultiply(index, 4));
    }

    void Int32Array::Set(std::int32_t index, std::int32_t value)
    {
        WriteInt32(Detail::UncheckedMultiply(index, 4), value);
    }

    UInt32Array::UInt32Array(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    UInt32Array::UInt32Array(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::uint32_t UInt32Array::Get(std::int32_t index)
    {
        return ReadUInt32(Detail::UncheckedMultiply(index, 4));
    }

    void UInt32Array::Set(std::int32_t index, std::uint32_t value)
    {
        WriteUInt32(Detail::UncheckedMultiply(index, 4), value);
    }

    IntPtrArray::IntPtrArray(Memory& memory, std::int32_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    IntPtrArray::IntPtrArray(Memory& memory, std::intptr_t address, std::int32_t length)
        : MemoryArray(memory, address, length)
    {
    }

    std::intptr_t IntPtrArray::Get(std::int32_t index)
    {
        return ReadPointer(Detail::UncheckedMultiply(index, 4));
    }

    void IntPtrArray::Set(std::int32_t index, std::intptr_t value)
    {
        WritePointer(Detail::UncheckedMultiply(index, 4), value);
    }
}

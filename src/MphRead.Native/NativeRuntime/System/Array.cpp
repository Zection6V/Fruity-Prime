#include "Array.hpp"

#include "Exceptions.hpp"

namespace MphRead::NativeRuntime
{
    ManagedBoolArray::ManagedBoolArray(std::int32_t length)
    {
        if (length < 0)
        {
            throw System::OverflowException();
        }
        _values.assign(static_cast<std::size_t>(length), 0);
    }

    std::int32_t ManagedBoolArray::Length() const noexcept
    {
        return static_cast<std::int32_t>(_values.size());
    }

    bool ManagedBoolArray::Get(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _values.size())
        {
            throw System::IndexOutOfRangeException();
        }
        return _values[static_cast<std::size_t>(index)] != 0;
    }

    void ManagedBoolArray::Set(std::int32_t index, bool value)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _values.size())
        {
            throw System::IndexOutOfRangeException();
        }
        _values[static_cast<std::size_t>(index)] = value ? 1 : 0;
    }

    std::shared_ptr<ManagedBoolArray> CreateManagedBoolArray(std::int32_t length)
    {
        return std::make_shared<ManagedBoolArray>(length);
    }

    std::int32_t ManagedBoolArrayLength(const std::shared_ptr<const ManagedBoolArray>& array)
    {
        if (!array)
        {
            throw System::NullReferenceException();
        }
        return array->Length();
    }

    bool ManagedBoolArrayGet(const std::shared_ptr<const ManagedBoolArray>& array, std::int32_t index)
    {
        if (!array)
        {
            throw System::NullReferenceException();
        }
        return array->Get(index);
    }

    void ManagedBoolArraySet(const std::shared_ptr<ManagedBoolArray>& array, std::int32_t index, bool value)
    {
        if (!array)
        {
            throw System::NullReferenceException();
        }
        array->Set(index, value);
    }
}

#pragma once

#include "MemoryClasses.hpp"

#include <any>
#include <bit>
#include <climits>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Memory
{
    static_assert(CHAR_BIT == 8);
    static_assert(sizeof(std::int8_t) == 1);
    static_assert(sizeof(std::uint8_t) == 1);
    static_assert(sizeof(std::int16_t) == 2);
    static_assert(sizeof(std::uint16_t) == 2);
    static_assert(sizeof(std::int32_t) == 4);
    static_assert(sizeof(std::uint32_t) == 4);

    namespace Detail
    {
        class IntPtrAddress
        {
        public:
            explicit constexpr IntPtrAddress(std::intptr_t value) noexcept
                : _value(value)
            {
            }

            [[nodiscard]] constexpr std::intptr_t Value() const noexcept
            {
                return _value;
            }

        private:
            std::intptr_t _value;
        };

        class IndexOutOfRangeException final : public std::out_of_range
        {
        public:
            IndexOutOfRangeException()
                : std::out_of_range("Index was outside the bounds of the array.")
            {
            }
        };

        class ArgumentOutOfRangeException final : public std::out_of_range
        {
        public:
            ArgumentOutOfRangeException()
                : std::out_of_range(
                    "Index was out of range. Must be non-negative and less than the size "
                    "of the collection. (Parameter 'index')")
            {
            }
        };

        class InvalidCastException final : public std::runtime_error
        {
        public:
            InvalidCastException()
                : std::runtime_error("Specified cast is not valid.")
            {
            }
        };

        class NullReferenceException final : public std::runtime_error
        {
        public:
            NullReferenceException()
                : std::runtime_error("Object reference not set to an instance of an object.")
            {
            }
        };

        class NotImplementedException final : public std::logic_error
        {
        public:
            explicit NotImplementedException(const char* message)
                : std::logic_error(message)
            {
            }
        };

        class OverflowException final : public std::overflow_error
        {
        public:
            OverflowException()
                : std::overflow_error("Arithmetic operation resulted in an overflow.")
            {
            }
        };

        class IEnumerator
        {
        public:
            IEnumerator() = default;
            IEnumerator(const IEnumerator&) = delete;
            IEnumerator(IEnumerator&&) = delete;
            IEnumerator& operator=(const IEnumerator&) = delete;
            IEnumerator& operator=(IEnumerator&&) = delete;
            virtual ~IEnumerator() = default;

            [[nodiscard]] virtual std::any Current() = 0;
            [[nodiscard]] virtual bool MoveNext() = 0;
            virtual void Reset() = 0;
        };

        class IEnumerable
        {
        public:
            IEnumerable() = default;
            IEnumerable(const IEnumerable&) = delete;
            IEnumerable(IEnumerable&&) = delete;
            IEnumerable& operator=(const IEnumerable&) = delete;
            IEnumerable& operator=(IEnumerable&&) = delete;
            virtual ~IEnumerable() = default;

            [[nodiscard]] virtual std::unique_ptr<IEnumerator> GetEnumerator() = 0;
        };

        [[nodiscard]] constexpr std::int32_t UncheckedAdd(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] constexpr std::int32_t UncheckedMultiply(
            std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] inline std::int32_t IntPtrToInt32(IntPtrAddress value)
        {
            const std::intptr_t raw = value.Value();
            if (raw < static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::min())
                || raw > static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::max()))
            {
                throw OverflowException();
            }
            return static_cast<std::int32_t>(raw);
        }

        template <typename T>
        [[nodiscard]] std::any Box(T value)
        {
            return std::any(std::move(value));
        }

        template <typename T>
        [[nodiscard]] std::any Box(std::shared_ptr<T> value)
        {
            if (!value)
            {
                return {};
            }
            return std::any(std::move(value));
        }
    }

    template <typename T>
    class MemoryArray : public MemoryClass,
                        public Detail::IEnumerable
    {
    public:
        class MemoryArrayEnumerator;

        MemoryArray(const MemoryArray&) = delete;
        MemoryArray(MemoryArray&&) = delete;
        MemoryArray& operator=(const MemoryArray&) = delete;
        MemoryArray& operator=(MemoryArray&&) = delete;
        ~MemoryArray() override = default;

        [[nodiscard]] std::int32_t Length() const noexcept
        {
            return _length;
        }

        [[nodiscard]] T Item(std::int32_t index)
        {
            CheckIndex(index);
            return Get(index);
        }

        void Item(std::int32_t index, T value)
        {
            CheckIndex(index);
            Set(index, value);
        }

        [[nodiscard]] std::unique_ptr<Detail::IEnumerator> GetEnumerator() override
        {
            return std::make_unique<MemoryArrayEnumerator>(this);
        }

        class MemoryArrayEnumerator : public Detail::IEnumerator
        {
        public:
            explicit MemoryArrayEnumerator(MemoryArray<T>* memoryArray)
                : _memoryArray(memoryArray)
            {
            }

            MemoryArrayEnumerator(const MemoryArrayEnumerator&) = delete;
            MemoryArrayEnumerator(MemoryArrayEnumerator&&) = delete;
            MemoryArrayEnumerator& operator=(const MemoryArrayEnumerator&) = delete;
            MemoryArrayEnumerator& operator=(MemoryArrayEnumerator&&) = delete;

        private:
            [[nodiscard]] MemoryArray<T>& Array() const
            {
                if (_memoryArray == nullptr)
                {
                    throw Detail::NullReferenceException();
                }
                return *_memoryArray;
            }

            [[nodiscard]] std::any Current() override
            {
                return Detail::Box(Array().Get(_currentIndex));
            }

            [[nodiscard]] bool MoveNext() override
            {
                _currentIndex = Detail::UncheckedAdd(_currentIndex, 1);
                return _currentIndex < Array().Length();
            }

            void Reset() override
            {
                _currentIndex = 0;
            }

            MemoryArray<T>* const _memoryArray;
            std::int32_t _currentIndex = -1;
        };

    protected:
        MemoryArray(Memory& memory, std::int32_t address, std::int32_t length)
            : MemoryClass(memory, address), _length(length)
        {
        }

        MemoryArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length)
            : MemoryClass(memory, address.Value()), _length(length)
        {
        }

        [[nodiscard]] virtual T Get(std::int32_t index) = 0;
        virtual void Set(std::int32_t index, T value) = 0;

    private:
        void CheckIndex(std::int32_t index) const
        {
            if (index < 0 || index >= _length)
            {
                throw Detail::IndexOutOfRangeException();
            }
        }

        const std::int32_t _length;
    };

    class SByteArray : public MemoryArray<std::int8_t>
    {
    public:
        SByteArray(Memory& memory, std::int32_t address, std::int32_t length);
        SByteArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::int8_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int8_t value) override;
    };

    class ByteArray : public MemoryArray<std::uint8_t>
    {
    public:
        ByteArray(Memory& memory, std::int32_t address, std::int32_t length);
        ByteArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint8_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint8_t value) override;
    };

    class Int16Array : public MemoryArray<std::int16_t>
    {
    public:
        Int16Array(Memory& memory, std::int32_t address, std::int32_t length);
        Int16Array(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::int16_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int16_t value) override;
    };

    class UInt16Array : public MemoryArray<std::uint16_t>
    {
    public:
        UInt16Array(Memory& memory, std::int32_t address, std::int32_t length);
        UInt16Array(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint16_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint16_t value) override;
    };

    class Int32Array : public MemoryArray<std::int32_t>
    {
    public:
        Int32Array(Memory& memory, std::int32_t address, std::int32_t length);
        Int32Array(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::int32_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int32_t value) override;
    };

    class UInt32Array : public MemoryArray<std::uint32_t>
    {
    public:
        UInt32Array(Memory& memory, std::int32_t address, std::int32_t length);
        UInt32Array(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint32_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint32_t value) override;
    };

    class IntPtrArray : public MemoryArray<std::intptr_t>
    {
    public:
        IntPtrArray(Memory& memory, std::int32_t address, std::int32_t length);
        IntPtrArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length);

    protected:
        [[nodiscard]] std::intptr_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::intptr_t value) override;
    };

    template <typename T>
    class U8EnumArray : public MemoryArray<T>
    {
        static_assert(std::is_enum_v<T>, "U8EnumArray<T> requires an enum type.");

    public:
        U8EnumArray(Memory& memory, std::int32_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

        U8EnumArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            const std::uint8_t value = this->ReadByte(Detail::UncheckedMultiply(index, 1));
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }

        void Set(std::int32_t index, T value) override
        {
            const std::int32_t offset = Detail::UncheckedMultiply(index, 1);
            static_cast<void>(offset);
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }
    };

    template <typename T>
    class U16EnumArray : public MemoryArray<T>
    {
        static_assert(std::is_enum_v<T>, "U16EnumArray<T> requires an enum type.");

    public:
        U16EnumArray(Memory& memory, std::int32_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

        U16EnumArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            const std::uint16_t value = this->ReadUInt16(Detail::UncheckedMultiply(index, 2));
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }

        void Set(std::int32_t index, T value) override
        {
            const std::int32_t offset = Detail::UncheckedMultiply(index, 2);
            static_cast<void>(offset);
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }
    };

    template <typename T>
    class U32EnumArray : public MemoryArray<T>
    {
        static_assert(std::is_enum_v<T>, "U32EnumArray<T> requires an enum type.");

    public:
        U32EnumArray(Memory& memory, std::int32_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

        U32EnumArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            const std::uint32_t value = this->ReadUInt32(Detail::UncheckedMultiply(index, 4));
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }

        void Set(std::int32_t index, T value) override
        {
            const std::int32_t offset = Detail::UncheckedMultiply(index, 4);
            static_cast<void>(offset);
            static_cast<void>(value);
            throw Detail::InvalidCastException();
        }
    };

    template <typename T>
    class StructArray : public MemoryArray<std::shared_ptr<T>>
    {
        static_assert(std::is_class_v<T>, "StructArray<T> requires a class type.");

    public:
        using Create = std::function<std::shared_ptr<T>(Memory&, std::int32_t)>;

        StructArray(Memory& memory, std::int32_t address, std::int32_t length,
            std::int32_t size, const Create& create)
            : MemoryArray<std::shared_ptr<T>>(memory, address, length)
        {
            for (std::int32_t i = 0; i < length; i = Detail::UncheckedAdd(i, 1))
            {
                const std::int32_t itemAddress = Detail::UncheckedAdd(
                    address, Detail::UncheckedMultiply(i, size));
                if (!create)
                {
                    throw Detail::NullReferenceException();
                }
                _items.push_back(create(memory, itemAddress));
            }
        }

        StructArray(Memory& memory, Detail::IntPtrAddress address, std::int32_t length,
            std::int32_t size, const Create& create)
            : MemoryArray<std::shared_ptr<T>>(memory, address, length)
        {
            for (std::int32_t i = 0; i < length; i = Detail::UncheckedAdd(i, 1))
            {
                const std::int32_t itemAddress = Detail::UncheckedAdd(
                    Detail::IntPtrToInt32(address), Detail::UncheckedMultiply(i, size));
                if (!create)
                {
                    throw Detail::NullReferenceException();
                }
                _items.push_back(create(memory, itemAddress));
            }
        }

    protected:
        [[nodiscard]] std::shared_ptr<T> Get(std::int32_t index) override
        {
            if (index < 0 || index >= static_cast<std::int32_t>(_items.size()))
            {
                throw Detail::ArgumentOutOfRangeException();
            }
            return _items[static_cast<std::size_t>(index)];
        }

        void Set(std::int32_t index, std::shared_ptr<T> value) override
        {
            static_cast<void>(index);
            static_cast<void>(value);
            throw Detail::NotImplementedException(
                "Writing embedded struct properties is not supported.");
        }

    private:
        std::vector<std::shared_ptr<T>> _items;
    };
}

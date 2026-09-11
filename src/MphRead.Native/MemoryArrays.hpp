#pragma once

#include "MemoryClasses.hpp"

#include <any>
#include <bit>
#include <climits>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
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

        [[nodiscard]] inline std::int32_t IntPtrToInt32(std::intptr_t value)
        {
            if (value < static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::min())
                || value > static_cast<std::intptr_t>(std::numeric_limits<std::int32_t>::max()))
            {
                throw std::overflow_error("Arithmetic operation resulted in an overflow.");
            }
            return static_cast<std::int32_t>(value);
        }

        template <typename T>
        [[nodiscard]] std::any Box(T value)
        {
            if constexpr (std::is_pointer_v<T>)
            {
                if (value == nullptr)
                {
                    return {};
                }
            }
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
                        public std::enable_shared_from_this<MemoryArray<T>>
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

        [[nodiscard]] std::shared_ptr<MemoryArrayEnumerator> GetEnumerator()
        {
            return std::make_shared<MemoryArrayEnumerator>(this->shared_from_this());
        }

        class MemoryArrayEnumerator
        {
        public:
            explicit MemoryArrayEnumerator(std::shared_ptr<MemoryArray<T>> memoryArray)
                : _memoryArray(std::move(memoryArray))
            {
            }

            MemoryArrayEnumerator(const MemoryArrayEnumerator&) = delete;
            MemoryArrayEnumerator(MemoryArrayEnumerator&&) = delete;
            MemoryArrayEnumerator& operator=(const MemoryArrayEnumerator&) = delete;
            MemoryArrayEnumerator& operator=(MemoryArrayEnumerator&&) = delete;

            [[nodiscard]] std::any Current()
            {
                return Detail::Box(_memoryArray->Get(_currentIndex));
            }

            [[nodiscard]] bool MoveNext() noexcept
            {
                _currentIndex = Detail::UncheckedAdd(_currentIndex, 1);
                return _currentIndex < _memoryArray->Length();
            }

            void Reset() noexcept
            {
                _currentIndex = 0;
            }

        private:
            const std::shared_ptr<MemoryArray<T>> _memoryArray;
            std::int32_t _currentIndex = -1;
        };

    protected:
        MemoryArray(Memory& memory, std::int32_t address, std::int32_t length)
            : MemoryClass(memory, address), _length(length)
        {
        }

        MemoryArray(Memory& memory, std::intptr_t address, std::int32_t length)
            : MemoryClass(memory, address), _length(length)
        {
        }

        [[nodiscard]] virtual T Get(std::int32_t index) = 0;
        virtual void Set(std::int32_t index, T value) = 0;

    private:
        void CheckIndex(std::int32_t index) const
        {
            if (index < 0 || index >= _length)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
        }

        const std::int32_t _length;
    };

    class SByteArray : public MemoryArray<std::int8_t>
    {
    public:
        SByteArray(Memory& memory, std::int32_t address, std::int32_t length);
        SByteArray(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::int8_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int8_t value) override;
    };

    class ByteArray : public MemoryArray<std::uint8_t>
    {
    public:
        ByteArray(Memory& memory, std::int32_t address, std::int32_t length);
        ByteArray(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint8_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint8_t value) override;
    };

    class Int16Array : public MemoryArray<std::int16_t>
    {
    public:
        Int16Array(Memory& memory, std::int32_t address, std::int32_t length);
        Int16Array(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::int16_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int16_t value) override;
    };

    class UInt16Array : public MemoryArray<std::uint16_t>
    {
    public:
        UInt16Array(Memory& memory, std::int32_t address, std::int32_t length);
        UInt16Array(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint16_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint16_t value) override;
    };

    class Int32Array : public MemoryArray<std::int32_t>
    {
    public:
        Int32Array(Memory& memory, std::int32_t address, std::int32_t length);
        Int32Array(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::int32_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::int32_t value) override;
    };

    class UInt32Array : public MemoryArray<std::uint32_t>
    {
    public:
        UInt32Array(Memory& memory, std::int32_t address, std::int32_t length);
        UInt32Array(Memory& memory, std::intptr_t address, std::int32_t length);

    protected:
        [[nodiscard]] std::uint32_t Get(std::int32_t index) override;
        void Set(std::int32_t index, std::uint32_t value) override;
    };

    class IntPtrArray : public MemoryArray<std::intptr_t>
    {
    public:
        IntPtrArray(Memory& memory, std::int32_t address, std::int32_t length);
        IntPtrArray(Memory& memory, std::intptr_t address, std::int32_t length);

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

        U8EnumArray(Memory& memory, std::intptr_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            return std::any_cast<T>(std::any{
                this->ReadByte(Detail::UncheckedMultiply(index, 1))
            });
        }

        void Set(std::int32_t index, T value) override
        {
            this->WriteByte(
                Detail::UncheckedMultiply(index, 1),
                std::any_cast<std::uint8_t>(std::any{value})
            );
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

        U16EnumArray(Memory& memory, std::intptr_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            return std::any_cast<T>(std::any{
                this->ReadUInt16(Detail::UncheckedMultiply(index, 2))
            });
        }

        void Set(std::int32_t index, T value) override
        {
            this->WriteUInt16(
                Detail::UncheckedMultiply(index, 2),
                std::any_cast<std::uint16_t>(std::any{value})
            );
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

        U32EnumArray(Memory& memory, std::intptr_t address, std::int32_t length)
            : MemoryArray<T>(memory, address, length)
        {
        }

    protected:
        [[nodiscard]] T Get(std::int32_t index) override
        {
            return std::any_cast<T>(std::any{
                this->ReadUInt32(Detail::UncheckedMultiply(index, 4))
            });
        }

        void Set(std::int32_t index, T value) override
        {
            this->WriteUInt32(
                Detail::UncheckedMultiply(index, 4),
                std::any_cast<std::uint32_t>(std::any{value})
            );
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
                _items.push_back(create(memory, Detail::UncheckedAdd(
                    address, Detail::UncheckedMultiply(i, size))));
            }
        }

        StructArray(Memory& memory, std::intptr_t address, std::int32_t length,
            std::int32_t size, const Create& create)
            : MemoryArray<std::shared_ptr<T>>(memory, address, length)
        {
            for (std::int32_t i = 0; i < length; i = Detail::UncheckedAdd(i, 1))
            {
                _items.push_back(create(memory, Detail::UncheckedAdd(
                    Detail::IntPtrToInt32(address), Detail::UncheckedMultiply(i, size))));
            }
        }

    protected:
        [[nodiscard]] std::shared_ptr<T> Get(std::int32_t index) override
        {
            return _items.at(static_cast<std::size_t>(index));
        }

        void Set(std::int32_t index, std::shared_ptr<T> value) override
        {
            static_cast<void>(index);
            static_cast<void>(value);
            throw std::logic_error("Writing embedded struct properties is not supported.");
        }

    private:
        std::vector<std::shared_ptr<T>> _items;
    };
}

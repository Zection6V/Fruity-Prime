#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace NCSFCommon
{
    struct NumberFormatInfo final
    {
        std::u16string PositiveSign;
        std::u16string NegativeSign;
        std::u16string NumberDecimalSeparator;
        std::u16string NaNSymbol;
        std::u16string PositiveInfinitySymbol;
        std::u16string NegativeInfinitySymbol;
    };

    // Native execution-context boundary corresponding to CultureInfo.CurrentCulture.NumberFormat.
    // Each thread owns its value; hosts with a managed/custom culture can replace it for that thread.
    extern thread_local NumberFormatInfo CurrentCultureNumberFormat;

    template <typename T>
    class Memory;

    template <typename T>
    class List final
    {
    private:
        struct Allocation final
        {
            std::allocator<T> Allocator;
            T* Data = nullptr;
            std::size_t Count = 0;
            std::size_t Capacity = 0;

            explicit Allocation(std::size_t capacity = 0)
                : Capacity(capacity)
            {
                if (Capacity != 0)
                {
                    Data = std::allocator_traits<std::allocator<T>>::allocate(Allocator, Capacity);
                }
            }

            Allocation(const Allocation&) = delete;
            Allocation& operator=(const Allocation&) = delete;

            ~Allocation()
            {
                for (std::size_t index = Count; index > 0; --index)
                {
                    std::allocator_traits<std::allocator<T>>::destroy(Allocator, Data + index - 1);
                }
                if (Data != nullptr)
                {
                    std::allocator_traits<std::allocator<T>>::deallocate(Allocator, Data, Capacity);
                }
            }

            template <typename U>
            void Add(U&& value)
            {
                std::allocator_traits<std::allocator<T>>::construct(
                    Allocator, Data + Count, std::forward<U>(value));
                ++Count;
            }
        };

        struct State final
        {
            std::shared_ptr<Allocation> Current;

            explicit State(std::size_t capacity = 0)
                : Current(std::make_shared<Allocation>(capacity))
            {
            }
        };

        std::shared_ptr<State> _state;

        [[nodiscard]] static std::size_t NextCapacity(
            std::size_t current, std::size_t required) noexcept
        {
            std::size_t capacity = current == 0 ? 4 : current;
            while (capacity < required)
            {
                if (capacity > std::numeric_limits<std::size_t>::max() / 2)
                {
                    return required;
                }
                capacity *= 2;
            }
            return capacity;
        }

        void Grow(std::size_t required)
        {
            const std::shared_ptr<Allocation>& current = _state->Current;
            if (required <= current->Capacity)
            {
                return;
            }

            auto next = std::make_shared<Allocation>(NextCapacity(current->Capacity, required));
            for (std::size_t index = 0; index < current->Count; ++index)
            {
                if constexpr (std::is_copy_constructible_v<T>)
                {
                    next->Add(current->Data[index]);
                }
                else
                {
                    next->Add(std::move(current->Data[index]));
                }
            }
            _state->Current = std::move(next);
        }

        template <typename>
        friend class Memory;

    public:
        List()
            : _state(std::make_shared<State>())
        {
        }

        explicit List(std::size_t capacity)
            : _state(std::make_shared<State>(capacity))
        {
        }

        [[nodiscard]] std::size_t Count() const noexcept
        {
            return _state->Current->Count;
        }

        [[nodiscard]] std::size_t Capacity() const noexcept
        {
            return _state->Current->Capacity;
        }

        std::size_t EnsureCapacity(std::size_t capacity)
        {
            Grow(capacity);
            return Capacity();
        }

        void Add(const T& value)
            requires std::is_copy_constructible_v<T>
        {
            Grow(Count() + 1);
            _state->Current->Add(value);
        }

        void Add(T&& value)
        {
            Grow(Count() + 1);
            _state->Current->Add(std::move(value));
        }

        [[nodiscard]] T& operator[](std::size_t index)
        {
            if (index >= Count())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return _state->Current->Data[index];
        }

        [[nodiscard]] const T& operator[](std::size_t index) const
        {
            if (index >= Count())
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return _state->Current->Data[index];
        }
    };

    template <typename T>
    class Memory final
    {
    private:
        using Allocation = typename List<T>::Allocation;

        std::shared_ptr<Allocation> _allocation;
        T* _data = nullptr;
        std::size_t _length = 0;

        explicit Memory(List<T>& list) noexcept
            : _allocation(list._state->Current),
              _data(_allocation->Data),
              _length(_allocation->Count)
        {
        }

        friend class Common;

    public:
        Memory() noexcept = default;

        [[nodiscard]] std::size_t Length() const noexcept
        {
            return _length;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return _length == 0;
        }

        [[nodiscard]] std::span<T> Span() const noexcept
        {
            return std::span<T>(_data, _length);
        }

        [[nodiscard]] T& operator[](std::size_t index) const
        {
            if (index >= _length)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return _data[index];
        }
    };

    template <typename T>
    class ReadOnlyMemory final
    {
    private:
        const T* _data = nullptr;
        std::size_t _length = 0;

    public:
        constexpr ReadOnlyMemory() noexcept = default;

        constexpr ReadOnlyMemory(const T* data, std::size_t length) noexcept
            : _data(data),
              _length(length)
        {
        }

        [[nodiscard]] constexpr std::size_t Length() const noexcept
        {
            return _length;
        }

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return _length == 0;
        }

        [[nodiscard]] constexpr std::span<const T> Span() const noexcept
        {
            return std::span<const T>(_data, _length);
        }

        [[nodiscard]] constexpr const T& operator[](std::size_t index) const
        {
            return Span()[index];
        }
    };

    class Regex final
    {
    private:
        class Impl;
        std::shared_ptr<const Impl> _impl;

        explicit Regex(std::u16string pattern);
        friend class Common;

    public:
        Regex(const Regex&) noexcept = default;
        Regex(Regex&&) noexcept = default;
        Regex& operator=(const Regex&) noexcept = default;
        Regex& operator=(Regex&&) noexcept = default;
        ~Regex() = default;

        [[nodiscard]] bool IsMatch(std::u16string_view input) const;
        [[nodiscard]] const std::u16string& ToString() const noexcept;
    };

    class Common final
    {
    private:
        Common() = delete;
        [[noreturn]] static void ThrowNotSupported();

    public:
        template <typename T>
        class ListMemory final
        {
        private:
            ListMemory() = delete;

            using Delegate = std::function<Memory<T>(List<T>&)>;

            struct LazyState final
            {
                std::once_flag Once;
                std::unique_ptr<Delegate> Value;
                std::exception_ptr Error;
            };

        public:
            [[nodiscard]] static const Delegate& AsMemory()
            {
                static LazyState state;
                std::call_once(state.Once, []
                {
                    try
                    {
                        state.Value = std::make_unique<Delegate>(
                            [](List<T>& list)
                            {
                                return Memory<T>(list);
                            });
                    }
                    catch (...)
                    {
                        state.Error = std::current_exception();
                    }
                });

                if (state.Error != nullptr)
                {
                    std::rethrow_exception(state.Error);
                }
                return *state.Value;
            }
        };

        template <typename T>
        [[nodiscard]] static Memory<T> AsMemory(List<T>& list)
        {
            return ListMemory<T>::AsMemory()(list);
        }

        template <typename T>
        [[nodiscard]] static std::uint8_t ToByte(T value)
        {
            static_assert(std::is_enum_v<T>, "T must be an enum type.");
            if constexpr (sizeof(T) != sizeof(std::uint8_t))
            {
                ThrowNotSupported();
            }
            else
            {
                return std::bit_cast<std::uint8_t>(value);
            }
        }

        template <typename T>
        [[nodiscard]] static T ToEnum(std::uint8_t value)
        {
            static_assert(std::is_enum_v<T>, "T must be an enum type.");
            if constexpr (sizeof(T) != sizeof(std::uint8_t))
            {
                ThrowNotSupported();
            }
            else
            {
                return std::bit_cast<T>(value);
            }
        }

        static const ReadOnlyMemory<std::uint8_t> DataBytes;

        [[nodiscard]] static std::u16string ReadNullTerminatedString(std::span<const std::uint8_t> span);
        static void WriteNullTerminatedString(std::span<std::uint8_t> span, std::u16string_view str);

        enum class SDATRecordType : std::uint8_t
        {
            Sequence,
            SequenceArchive,
            Bank,
            WaveArchive,
            Player,
            Group,
            Player2,
            Stream
        };

        [[nodiscard]] static bool VerifyHeader(
            std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected);

        [[nodiscard]] static Regex WildcardStringToRegex(std::u16string_view wildcard);

        enum class KeepType : std::uint8_t
        {
            Exclude,
            Include,
            Neither
        };

        class KeepInfo
        {
        public:
            const std::u16string Filename;
            const KeepType Keep;

            KeepInfo(std::u16string filename, KeepType keep);
            virtual ~KeepInfo() = default;

            KeepInfo& operator=(const KeepInfo&) = delete;
            KeepInfo& operator=(KeepInfo&&) = delete;

            [[nodiscard]] virtual bool Equals(const KeepInfo* other) const noexcept;
            [[nodiscard]] bool operator==(const KeepInfo& other) const noexcept;
            [[nodiscard]] bool operator!=(const KeepInfo& other) const noexcept;
            [[nodiscard]] virtual std::int32_t GetHashCode() const noexcept;
            [[nodiscard]] virtual std::u16string ToString() const;
            void Deconstruct(std::u16string& filename, KeepType& keep) const;

            friend bool operator==(
                const std::shared_ptr<KeepInfo>& left,
                const std::shared_ptr<KeepInfo>& right) noexcept
            {
                if (left.get() == right.get())
                {
                    return true;
                }
                if (!left || !right)
                {
                    return false;
                }
                return left->Equals(right.get());
            }

            friend bool operator!=(
                const std::shared_ptr<KeepInfo>& left,
                const std::shared_ptr<KeepInfo>& right) noexcept
            {
                return !(left == right);
            }

        protected:
            KeepInfo(const KeepInfo&) = default;
            KeepInfo(KeepInfo&&) noexcept = default;

            [[nodiscard]] virtual const std::type_info& EqualityContract() const noexcept;
            [[nodiscard]] virtual std::shared_ptr<KeepInfo> Clone() const;
            virtual bool PrintMembers(std::u16string& result) const;
        };

        [[nodiscard]] static KeepType IncludeFilename(
            std::u16string_view filename,
            std::u16string_view sdatNumber,
            const std::vector<std::shared_ptr<KeepInfo>>& includesAndExcludes);

        [[nodiscard]] static std::u16string SecondsToString(float seconds);
        [[nodiscard]] static std::int32_t StringToMS(std::u16string_view time);
        [[nodiscard]] static std::int32_t VLVLength(std::int32_t value) noexcept;
    };
}

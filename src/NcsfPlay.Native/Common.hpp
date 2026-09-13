#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
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
    template <typename T>
    class Memory final
    {
    private:
        struct State final
        {
            std::vector<T>* List;
            const std::size_t CapturedCapacity;
            const T* CapturedData;
            const std::size_t Length;
            std::vector<T> Captured;
            bool Detached = false;

            explicit State(std::vector<T>& list)
                : List(std::addressof(list)),
                  CapturedCapacity(list.capacity()),
                  CapturedData(GetData(list)),
                  Length(list.size()),
                  Captured(list.begin(), list.end())
            {
            }

            [[nodiscard]] static const T* GetData(const std::vector<T>& list) noexcept
            {
                if constexpr (requires { list.data(); })
                {
                    return list.data();
                }
                else
                {
                    return nullptr;
                }
            }

            [[nodiscard]] bool StillUsesCapturedStorage() const noexcept
            {
                if constexpr (requires { List->data(); })
                {
                    return List->data() == CapturedData;
                }
                else
                {
                    return List->capacity() == CapturedCapacity;
                }
            }

            void Refresh()
            {
                if (Detached)
                {
                    return;
                }

                if (!StillUsesCapturedStorage())
                {
                    const std::size_t copyLength = std::min(Length, List->size());
                    for (std::size_t index = 0; index < copyLength; ++index)
                    {
                        Captured[index] = (*List)[index];
                    }
                    Detached = true;
                    List = nullptr;
                    return;
                }

                const std::size_t copyLength = std::min(Length, List->size());
                for (std::size_t index = 0; index < copyLength; ++index)
                {
                    Captured[index] = (*List)[index];
                }
            }

            [[nodiscard]] T Read(std::size_t index)
            {
                if (index >= Length)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }

                Refresh();
                if (!Detached && index < List->size())
                {
                    return (*List)[index];
                }
                return Captured[index];
            }

            void Write(std::size_t index, const T& value)
            {
                if (index >= Length)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }

                Refresh();
                Captured[index] = value;
                if (!Detached && index < List->size())
                {
                    (*List)[index] = value;
                }
            }
        };

        std::shared_ptr<State> _state;

        explicit Memory(std::vector<T>& list)
            : _state(std::make_shared<State>(list))
        {
        }

        friend class Common;

    public:
        class Reference final
        {
        private:
            std::shared_ptr<State> _state;
            std::size_t _index;

            Reference(std::shared_ptr<State> state, std::size_t index)
                : _state(std::move(state)),
                  _index(index)
            {
            }

            friend class Memory<T>;
            friend class SpanView;

        public:
            Reference(const Reference&) noexcept = default;

            Reference& operator=(const T& value)
            {
                _state->Write(_index, value);
                return *this;
            }

            Reference& operator=(const Reference& other)
            {
                return operator=(static_cast<T>(other));
            }

            [[nodiscard]] operator T() const
            {
                return _state->Read(_index);
            }
        };

        class SpanView final
        {
        private:
            std::shared_ptr<State> _state;

            explicit SpanView(std::shared_ptr<State> state)
                : _state(std::move(state))
            {
            }

            friend class Memory<T>;

        public:
            [[nodiscard]] std::size_t size() const noexcept
            {
                return _state == nullptr ? 0 : _state->Length;
            }

            [[nodiscard]] bool empty() const noexcept
            {
                return size() == 0;
            }

            [[nodiscard]] Reference operator[](std::size_t index)
            {
                if (_state == nullptr || index >= _state->Length)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                return Reference(_state, index);
            }

            [[nodiscard]] T operator[](std::size_t index) const
            {
                if (_state == nullptr)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                return _state->Read(index);
            }
        };

        Memory() = default;

        [[nodiscard]] std::size_t Length() const noexcept
        {
            return _state == nullptr ? 0 : _state->Length;
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return Length() == 0;
        }

        [[nodiscard]] SpanView Span() const
        {
            return SpanView(_state);
        }

        [[nodiscard]] Reference operator[](std::size_t index)
        {
            if (_state == nullptr || index >= _state->Length)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return Reference(_state, index);
        }

        [[nodiscard]] T operator[](std::size_t index) const
        {
            if (_state == nullptr)
            {
                throw std::out_of_range("Index was outside the bounds of the array.");
            }
            return _state->Read(index);
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

            struct LazyState final
            {
                std::once_flag Once;
                std::unique_ptr<std::function<Memory<T>(std::vector<T>&)>> Value;
                std::exception_ptr Error;
            };

        public:
            [[nodiscard]] static const std::function<Memory<T>(std::vector<T>&)>& AsMemory()
            {
                static LazyState state;
                std::call_once(state.Once, []
                {
                    try
                    {
                        state.Value = std::make_unique<std::function<Memory<T>(std::vector<T>&)>>(
                            [](std::vector<T>& list)
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
        [[nodiscard]] static Memory<T> AsMemory(std::vector<T>& list)
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
            KeepInfo(const KeepInfo&) = default;
            KeepInfo(KeepInfo&&) noexcept = default;
            virtual ~KeepInfo() = default;

            KeepInfo& operator=(const KeepInfo&) = delete;
            KeepInfo& operator=(KeepInfo&&) = delete;

            [[nodiscard]] virtual bool Equals(const KeepInfo& other) const noexcept;
            [[nodiscard]] bool operator==(const KeepInfo& other) const noexcept;
            [[nodiscard]] bool operator!=(const KeepInfo& other) const noexcept;
            [[nodiscard]] virtual std::size_t GetHashCode() const noexcept;
            [[nodiscard]] virtual std::u16string ToString() const;
            void Deconstruct(std::u16string& filename, KeepType& keep) const;
            [[nodiscard]] std::shared_ptr<KeepInfo> WithFilename(std::u16string filename) const;
            [[nodiscard]] std::shared_ptr<KeepInfo> WithKeep(KeepType keep) const;
            [[nodiscard]] std::shared_ptr<KeepInfo> With(std::u16string filename, KeepType keep) const;

        protected:
            [[nodiscard]] virtual const std::type_info& EqualityContract() const noexcept;
            [[nodiscard]] virtual std::shared_ptr<KeepInfo> CloneWith(
                std::u16string filename, KeepType keep) const;
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

#include "Archive.hpp"

#include "../Read.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    template <typename T>
    struct ManagedRegistry final
    {
        std::mutex Mutex;
        std::unordered_map<const T*, std::shared_ptr<void>> Storage;
    };

    template <typename T>
    [[nodiscard]] ManagedRegistry<T>& Registry()
    {
        static ManagedRegistry<T>* registry = new ManagedRegistry<T>();
        return *registry;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::optional<std::string> GetDirectoryName(const std::string& path)
    {
        if (path.empty())
        {
            return std::nullopt;
        }
        const std::filesystem::path nativePath = PathFromUtf8(path);
        const std::filesystem::path parent = nativePath.parent_path();
        if (parent.empty())
        {
            return std::string();
        }
        if (parent == nativePath)
        {
            return std::nullopt;
        }
        return PathToUtf8(parent);
    }

    [[nodiscard]] std::string GetFileName(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::binary | std::ios::ate);
        if (!stream.is_open())
        {
            const int error = errno == 0 ? EIO : errno;
            throw std::system_error(error, std::generic_category());
        }
        const std::streampos end = stream.tellg();
        if (end < 0)
        {
            throw std::ios_base::failure("Could not determine file length");
        }
        if (static_cast<std::uint64_t>(end) > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::overflow_error("Array dimensions exceeded supported range.");
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (!stream)
            {
                throw std::ios_base::failure("Could not read file");
            }
        }
        return bytes;
    }

    void FileWriteAllBytes(const std::string& path, std::span<const std::uint8_t> bytes)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            const int error = errno == 0 ? EIO : errno;
            throw std::system_error(error, std::generic_category());
        }
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        stream.flush();
    }

    [[nodiscard]] std::uint32_t ReadUInt32Native(const std::uint8_t* bytes) noexcept
    {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        return value;
    }

    void WriteUInt32LittleEndian(std::ostream& stream, std::uint32_t value)
    {
        const std::array<std::uint8_t, 4> bytes = {
            static_cast<std::uint8_t>(value & 0xFFU),
            static_cast<std::uint8_t>((value >> 8) & 0xFFU),
            static_cast<std::uint8_t>((value >> 16) & 0xFFU),
            static_cast<std::uint8_t>((value >> 24) & 0xFFU)
        };
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void AppendUtf8(std::string& output, std::uint32_t codePoint)
    {
        if (codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
        {
            codePoint = 0xFFFDU;
        }
        if (codePoint <= 0x7FU)
        {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else if (codePoint <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    [[nodiscard]] std::vector<char16_t> DecodeUtf8ToUtf16(std::string_view value)
    {
        std::vector<char16_t> result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const std::uint8_t first = static_cast<std::uint8_t>(value[index]);
            std::uint32_t codePoint = 0xFFFDU;
            std::size_t length = 1;
            if (first <= 0x7FU)
            {
                codePoint = first;
            }
            else
            {
                std::uint32_t minimum = 0;
                if ((first & 0xE0U) == 0xC0U)
                {
                    codePoint = first & 0x1FU;
                    length = 2;
                    minimum = 0x80U;
                }
                else if ((first & 0xF0U) == 0xE0U)
                {
                    codePoint = first & 0x0FU;
                    length = 3;
                    minimum = 0x800U;
                }
                else if ((first & 0xF8U) == 0xF0U)
                {
                    codePoint = first & 0x07U;
                    length = 4;
                    minimum = 0x10000U;
                }
                bool valid = minimum != 0 && index + length <= value.size();
                if (valid)
                {
                    for (std::size_t offset = 1; offset < length; ++offset)
                    {
                        const std::uint8_t next = static_cast<std::uint8_t>(value[index + offset]);
                        if ((next & 0xC0U) != 0x80U)
                        {
                            valid = false;
                            break;
                        }
                        codePoint = (codePoint << 6) | (next & 0x3FU);
                    }
                }
                if (!valid || codePoint < minimum || codePoint > 0x10FFFFU
                    || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
                {
                    codePoint = 0xFFFDU;
                    length = 1;
                }
            }
            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (codePoint >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (codePoint & 0x3FFU)));
            }
            index += length;
        }
        return result;
    }

    [[nodiscard]] std::string EncodeUtf16(
        const std::shared_ptr<MphRead::ManagedArray<char16_t>>& value, bool stopAtNull)
    {
        if (!value)
        {
            throw System::ArgumentNullException("array");
        }
        std::string output;
        for (std::size_t index = 0; index < value->Length(); ++index)
        {
            const std::uint16_t first = static_cast<std::uint16_t>((*value)[index]);
            if (stopAtNull && first == 0)
            {
                break;
            }
            std::uint32_t codePoint = first;
            if (first >= 0xD800U && first <= 0xDBFFU && index + 1 < value->Length())
            {
                const std::uint16_t second = static_cast<std::uint16_t>((*value)[index + 1]);
                if (second >= 0xDC00U && second <= 0xDFFFU)
                {
                    codePoint = 0x10000U
                        + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                        + (static_cast<std::uint32_t>(second) - 0xDC00U);
                    ++index;
                }
            }
            AppendUtf8(output, codePoint);
        }
        return output;
    }

    [[nodiscard]] std::size_t Utf16Length(std::string_view value)
    {
        return DecodeUtf8ToUtf16(value).size();
    }

    [[nodiscard]] std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::uint32_t UncheckedAdd(std::uint32_t left, std::uint32_t right) noexcept
    {
        return left + right;
    }

    [[nodiscard]] std::int32_t UncheckedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }
}

namespace MphRead::Archive::ArchiveDetail
{
    template <std::size_t N>
    ByValAnsiString<N>::ByValAnsiString(const std::string& value)
    {
        auto managed = std::make_shared<std::string>(value);
        const std::size_t count = N == 0 ? 0 : std::min<std::size_t>(value.size(), N - 1);
        for (std::size_t index = 0; index < count; ++index)
        {
            _wire[index] = static_cast<std::uint8_t>(value[index]);
        }
        SetManagedValue(std::move(managed));
    }

    template <std::size_t N>
    ByValAnsiString<N>::ByValAnsiString(const ByValAnsiString& other)
        : _wire(other._wire)
    {
        if (auto value = other.TryGetManagedValue())
        {
            SetManagedValue(std::move(value));
        }
    }

    template <std::size_t N>
    ByValAnsiString<N>& ByValAnsiString<N>::operator=(const ByValAnsiString& other)
    {
        if (this != std::addressof(other))
        {
            _wire = other._wire;
            if (auto value = other.TryGetManagedValue())
            {
                SetManagedValue(std::move(value));
            }
            else
            {
                ClearManagedValue();
            }
        }
        return *this;
    }

    template <std::size_t N>
    ByValAnsiString<N>::~ByValAnsiString() noexcept
    {
        ClearManagedValueNoThrow();
    }

    template <std::size_t N>
    bool ByValAnsiString<N>::IsNull() const
    {
        return !TryGetManagedValue();
    }

    template <std::size_t N>
    std::shared_ptr<const std::string> ByValAnsiString<N>::ManagedValue() const
    {
        return TryGetManagedValue();
    }

    template <std::size_t N>
    bool ByValAnsiString<N>::Equals(const std::string& value) const
    {
        auto managed = TryGetManagedValue();
        return managed && *managed == value;
    }

    template <std::size_t N>
    const std::array<std::uint8_t, N>& ByValAnsiString<N>::WireBytes() const noexcept
    {
        return _wire;
    }

    template <std::size_t N>
    ByValAnsiString<N> ByValAnsiString<N>::FromMarshaledBytes(const std::uint8_t* bytes)
    {
        if (bytes == nullptr)
        {
            throw System::ArgumentNullException("bytes");
        }
        ByValAnsiString result;
        std::string value;
        for (std::size_t index = 0; index < N && bytes[index] != 0; ++index)
        {
            value.push_back(static_cast<char>(bytes[index]));
        }
        std::copy_n(bytes, N, result._wire.begin());
        result.SetManagedValue(std::make_shared<std::string>(std::move(value)));
        return result;
    }

    template <std::size_t N>
    void ByValAnsiString<N>::SetManagedValue(std::shared_ptr<std::string> value) const
    {
        auto& registry = Registry<ByValAnsiString<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage[this] = std::move(value);
    }

    template <std::size_t N>
    void ByValAnsiString<N>::ClearManagedValue() const
    {
        auto& registry = Registry<ByValAnsiString<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage.erase(this);
    }

    template <std::size_t N>
    void ByValAnsiString<N>::ClearManagedValueNoThrow() const noexcept
    {
        try
        {
            ClearManagedValue();
        }
        catch (...)
        {
        }
    }

    template <std::size_t N>
    std::shared_ptr<std::string> ByValAnsiString<N>::TryGetManagedValue() const
    {
        auto& registry = Registry<ByValAnsiString<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        const auto item = registry.Storage.find(this);
        if (item == registry.Storage.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<std::string>(item->second);
    }

    template <std::size_t N>
    ByValCharArray<N>::ByValCharArray(std::shared_ptr<ManagedStorage> value)
    {
        if (value)
        {
            const std::size_t count = std::min<std::size_t>(N, value->Length());
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::uint16_t ch = static_cast<std::uint16_t>((*value)[index]);
                _wire[index] = static_cast<std::uint8_t>(ch <= 0x7FU ? ch : '?');
            }
            SetManagedValue(std::move(value));
        }
    }

    template <std::size_t N>
    ByValCharArray<N>::ByValCharArray(const std::string& value)
    {
        std::vector<char16_t> chars = DecodeUtf8ToUtf16(value);
        if (chars.size() < N)
        {
            chars.resize(N, u'\0');
        }
        auto managed = std::make_shared<ManagedStorage>(chars.size());
        for (std::size_t index = 0; index < chars.size(); ++index)
        {
            (*managed)[index] = chars[index];
            if (index < N)
            {
                const std::uint16_t ch = static_cast<std::uint16_t>(chars[index]);
                _wire[index] = static_cast<std::uint8_t>(ch <= 0x7FU ? ch : '?');
            }
        }
        SetManagedValue(std::move(managed));
    }

    template <std::size_t N>
    ByValCharArray<N>::ByValCharArray(const ByValCharArray& other)
        : _wire(other.WireBytes())
    {
        if (auto value = other.TryGetManagedValue())
        {
            SetManagedValue(std::move(value));
        }
    }

    template <std::size_t N>
    ByValCharArray<N>& ByValCharArray<N>::operator=(const ByValCharArray& other)
    {
        if (this != std::addressof(other))
        {
            _wire = other.WireBytes();
            if (auto value = other.TryGetManagedValue())
            {
                SetManagedValue(std::move(value));
            }
            else
            {
                ClearManagedValue();
            }
        }
        return *this;
    }

    template <std::size_t N>
    ByValCharArray<N>::~ByValCharArray() noexcept
    {
        ClearManagedValueNoThrow();
    }

    template <std::size_t N>
    bool ByValCharArray<N>::IsNull() const
    {
        return !TryGetManagedValue();
    }

    template <std::size_t N>
    std::shared_ptr<typename ByValCharArray<N>::ManagedStorage> ByValCharArray<N>::ManagedValue() const
    {
        return TryGetManagedValue();
    }

    template <std::size_t N>
    std::size_t ByValCharArray<N>::Length() const
    {
        auto value = TryGetManagedValue();
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return value->Length();
    }

    template <std::size_t N>
    char16_t& ByValCharArray<N>::operator[](std::size_t index) const
    {
        auto value = TryGetManagedValue();
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return (*value)[index];
    }

    template <std::size_t N>
    std::string ByValCharArray<N>::MarshalString() const
    {
        return EncodeUtf16(TryGetManagedValue(), true);
    }

    template <std::size_t N>
    std::string ByValCharArray<N>::BinaryWriterBytes() const
    {
        return EncodeUtf16(TryGetManagedValue(), false);
    }

    template <std::size_t N>
    const std::array<std::uint8_t, N>& ByValCharArray<N>::WireBytes() const
    {
        if (auto value = TryGetManagedValue())
        {
            _wire.fill(0);
            const std::size_t count = std::min<std::size_t>(N, value->Length());
            for (std::size_t index = 0; index < count; ++index)
            {
                const std::uint16_t ch = static_cast<std::uint16_t>((*value)[index]);
                _wire[index] = static_cast<std::uint8_t>(ch <= 0x7FU ? ch : '?');
            }
        }
        return _wire;
    }

    template <std::size_t N>
    ByValCharArray<N> ByValCharArray<N>::FromMarshaledBytes(const std::uint8_t* bytes)
    {
        if (bytes == nullptr)
        {
            throw System::ArgumentNullException("bytes");
        }
        ByValCharArray result;
        auto managed = std::make_shared<ManagedStorage>(N);
        for (std::size_t index = 0; index < N; ++index)
        {
            result._wire[index] = bytes[index];
            (*managed)[index] = static_cast<char16_t>(bytes[index]);
        }
        result.SetManagedValue(std::move(managed));
        return result;
    }

    template <std::size_t N>
    void ByValCharArray<N>::SetManagedValue(std::shared_ptr<ManagedStorage> value) const
    {
        auto& registry = Registry<ByValCharArray<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage[this] = std::move(value);
    }

    template <std::size_t N>
    void ByValCharArray<N>::ClearManagedValue() const
    {
        auto& registry = Registry<ByValCharArray<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage.erase(this);
    }

    template <std::size_t N>
    void ByValCharArray<N>::ClearManagedValueNoThrow() const noexcept
    {
        try
        {
            ClearManagedValue();
        }
        catch (...)
        {
        }
    }

    template <std::size_t N>
    std::shared_ptr<typename ByValCharArray<N>::ManagedStorage> ByValCharArray<N>::TryGetManagedValue() const
    {
        auto& registry = Registry<ByValCharArray<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        const auto item = registry.Storage.find(this);
        if (item == registry.Storage.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<ManagedStorage>(item->second);
    }

    template <std::size_t N>
    ByValUInt32Array<N>::ByValUInt32Array(std::shared_ptr<ManagedStorage> value)
    {
        if (value)
        {
            const std::size_t count = std::min<std::size_t>(N, value->Length());
            for (std::size_t index = 0; index < count; ++index)
            {
                _wire[index] = (*value)[index];
            }
            SetManagedValue(std::move(value));
        }
    }

    template <std::size_t N>
    ByValUInt32Array<N>::ByValUInt32Array(const ByValUInt32Array& other)
        : _wire(other.WireValues())
    {
        if (auto value = other.TryGetManagedValue())
        {
            SetManagedValue(std::move(value));
        }
    }

    template <std::size_t N>
    ByValUInt32Array<N>& ByValUInt32Array<N>::operator=(const ByValUInt32Array& other)
    {
        if (this != std::addressof(other))
        {
            _wire = other.WireValues();
            if (auto value = other.TryGetManagedValue())
            {
                SetManagedValue(std::move(value));
            }
            else
            {
                ClearManagedValue();
            }
        }
        return *this;
    }

    template <std::size_t N>
    ByValUInt32Array<N>::~ByValUInt32Array() noexcept
    {
        ClearManagedValueNoThrow();
    }

    template <std::size_t N>
    bool ByValUInt32Array<N>::IsNull() const
    {
        return !TryGetManagedValue();
    }

    template <std::size_t N>
    std::shared_ptr<typename ByValUInt32Array<N>::ManagedStorage> ByValUInt32Array<N>::ManagedValue() const
    {
        return TryGetManagedValue();
    }

    template <std::size_t N>
    std::size_t ByValUInt32Array<N>::Length() const
    {
        auto value = TryGetManagedValue();
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return value->Length();
    }

    template <std::size_t N>
    std::uint32_t& ByValUInt32Array<N>::operator[](std::size_t index) const
    {
        auto value = TryGetManagedValue();
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return (*value)[index];
    }

    template <std::size_t N>
    const std::array<std::uint32_t, N>& ByValUInt32Array<N>::WireValues() const
    {
        if (auto value = TryGetManagedValue())
        {
            _wire.fill(0);
            const std::size_t count = std::min<std::size_t>(N, value->Length());
            for (std::size_t index = 0; index < count; ++index)
            {
                _wire[index] = (*value)[index];
            }
        }
        return _wire;
    }

    template <std::size_t N>
    ByValUInt32Array<N> ByValUInt32Array<N>::FromMarshaledBytes(const std::uint8_t* bytes)
    {
        if (bytes == nullptr)
        {
            throw System::ArgumentNullException("bytes");
        }
        ByValUInt32Array result;
        auto managed = std::make_shared<ManagedStorage>(N);
        for (std::size_t index = 0; index < N; ++index)
        {
            result._wire[index] = ReadUInt32Native(bytes + index * sizeof(std::uint32_t));
            (*managed)[index] = result._wire[index];
        }
        result.SetManagedValue(std::move(managed));
        return result;
    }

    template <std::size_t N>
    void ByValUInt32Array<N>::SetManagedValue(std::shared_ptr<ManagedStorage> value) const
    {
        auto& registry = Registry<ByValUInt32Array<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage[this] = std::move(value);
    }

    template <std::size_t N>
    void ByValUInt32Array<N>::ClearManagedValue() const
    {
        auto& registry = Registry<ByValUInt32Array<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        registry.Storage.erase(this);
    }

    template <std::size_t N>
    void ByValUInt32Array<N>::ClearManagedValueNoThrow() const noexcept
    {
        try
        {
            ClearManagedValue();
        }
        catch (...)
        {
        }
    }

    template <std::size_t N>
    std::shared_ptr<typename ByValUInt32Array<N>::ManagedStorage> ByValUInt32Array<N>::TryGetManagedValue() const
    {
        auto& registry = Registry<ByValUInt32Array<N>>();
        std::lock_guard<std::mutex> lock(registry.Mutex);
        const auto item = registry.Storage.find(this);
        if (item == registry.Storage.end())
        {
            return nullptr;
        }
        return std::static_pointer_cast<ManagedStorage>(item->second);
    }

    template class ByValAnsiString<8>;
    template class ByValCharArray<32>;
    template class ByValUInt32Array<4>;
    template class ByValUInt32Array<5>;
}

namespace MphRead::Archive
{
    namespace
    {
        [[nodiscard]] std::shared_ptr<ManagedArray<std::uint32_t>> ZeroUInt32Array(std::size_t length)
        {
            return std::make_shared<ManagedArray<std::uint32_t>>(length);
        }
    }

    ArchiveHeader::ArchiveHeader(std::string magicString, std::uint32_t fileCount, std::uint32_t totalSize)
        : MagicString(std::move(magicString)), FileCount(fileCount), TotalSize(totalSize),
          Padding(ZeroUInt32Array(4))
    {
    }

    ArchiveHeader::ArchiveHeader(ArchiveDetail::ByValAnsiString<8> magicString,
        std::uint32_t fileCount, std::uint32_t totalSize, ArchiveDetail::ByValUInt32Array<4> padding)
        : MagicString(std::move(magicString)), FileCount(fileCount), TotalSize(totalSize),
          Padding(std::move(padding))
    {
    }

    ArchiveHeader& ArchiveHeader::operator=(const ArchiveHeader& other)
    {
        if (this != std::addressof(other))
        {
            this->~ArchiveHeader();
            ::new (static_cast<void*>(this)) ArchiveHeader(other);
        }
        return *this;
    }

    ArchiveHeader ArchiveHeader::SwapBytes() const
    {
        return ArchiveHeader(MagicString, Archiver::SwapBytes(FileCount),
            Archiver::SwapBytes(TotalSize), ArchiveDetail::ByValUInt32Array<4>(ZeroUInt32Array(4)));
    }

    ArchiveHeader ArchiveHeader::FromMarshaledBytes(const std::array<std::uint8_t, 32>& bytes)
    {
        return ArchiveHeader(
            ArchiveDetail::ByValAnsiString<8>::FromMarshaledBytes(bytes.data()),
            ReadUInt32Native(bytes.data() + 8), ReadUInt32Native(bytes.data() + 12),
            ArchiveDetail::ByValUInt32Array<4>::FromMarshaledBytes(bytes.data() + 16));
    }

    FileHeader::FileHeader(std::shared_ptr<ManagedArray<char16_t>> filename, std::uint32_t offset,
        std::uint32_t paddedFileSize, std::uint32_t targetFileSize)
        : Filename(std::move(filename)), Offset(offset), PaddedFileSize(paddedFileSize),
          TargetFileSize(targetFileSize), Padding(ZeroUInt32Array(5))
    {
    }

    FileHeader::FileHeader(std::string filename, std::uint32_t offset,
        std::uint32_t paddedFileSize, std::uint32_t targetFileSize)
        : Filename(filename), Offset(offset), PaddedFileSize(paddedFileSize),
          TargetFileSize(targetFileSize), Padding(ZeroUInt32Array(5))
    {
    }

    FileHeader::FileHeader(ArchiveDetail::ByValCharArray<32> filename, std::uint32_t offset,
        std::uint32_t paddedFileSize, std::uint32_t targetFileSize,
        ArchiveDetail::ByValUInt32Array<5> padding)
        : Filename(std::move(filename)), Offset(offset), PaddedFileSize(paddedFileSize),
          TargetFileSize(targetFileSize), Padding(std::move(padding))
    {
    }

    FileHeader& FileHeader::operator=(const FileHeader& other)
    {
        if (this != std::addressof(other))
        {
            this->~FileHeader();
            ::new (static_cast<void*>(this)) FileHeader(other);
        }
        return *this;
    }

    FileHeader FileHeader::SwapBytes() const
    {
        return FileHeader(Filename, Archiver::SwapBytes(Offset),
            Archiver::SwapBytes(PaddedFileSize), Archiver::SwapBytes(TargetFileSize),
            ArchiveDetail::ByValUInt32Array<5>(ZeroUInt32Array(5)));
    }

    FileHeader FileHeader::FromMarshaledBytes(const std::array<std::uint8_t, 64>& bytes)
    {
        return FileHeader(
            ArchiveDetail::ByValCharArray<32>::FromMarshaledBytes(bytes.data()),
            ReadUInt32Native(bytes.data() + 32), ReadUInt32Native(bytes.data() + 36),
            ReadUInt32Native(bytes.data() + 40),
            ArchiveDetail::ByValUInt32Array<5>::FromMarshaledBytes(bytes.data() + 44));
    }

    const std::int32_t ArchiveSizes::ArchiveHeader = static_cast<std::int32_t>(sizeof(MphRead::Archive::ArchiveHeader));
    const std::int32_t ArchiveSizes::FileHeader = static_cast<std::int32_t>(sizeof(MphRead::Archive::FileHeader));

    const std::string& Archiver::MagicString()
    {
        static const std::string value = "SNDFILE";
        return value;
    }

    std::int32_t Archiver::Extract(const std::string& path, const std::optional<std::string>& destination)
    {
        std::optional<std::string> outputDirectory = destination;
        if (!outputDirectory)
        {
            outputDirectory = GetDirectoryName(path);
        }

        const std::vector<std::uint8_t> bytes = FileReadAllBytes(path);
        if (bytes.size() < static_cast<std::size_t>(ArchiveSizes::ArchiveHeader))
        {
            ThrowRead();
        }

        ArchiveHeader header = Read::ReadStruct<ArchiveHeader>(std::span<const std::uint8_t>(
            bytes.data(), static_cast<std::size_t>(ArchiveSizes::ArchiveHeader)));
        header = header.SwapBytes();
        if (!header.MagicString.Equals(MagicString())
            || header.TotalSize != static_cast<std::uint32_t>(bytes.size()))
        {
            ThrowRead();
        }

        std::uint32_t pointer = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(ArchiveSizes::ArchiveHeader)
            + static_cast<std::uint64_t>(ArchiveSizes::FileHeader) * header.FileCount);
        std::vector<FileHeader> files;
        const auto swaps = Read::DoOffsets<FileHeader>(bytes,
            static_cast<std::uint32_t>(ArchiveSizes::ArchiveHeader), ManagedInt32(header.FileCount));
        for (const FileHeader& swap : *swaps)
        {
            const FileHeader file = swap.SwapBytes();
            const std::string filename = file.Filename.MarshalString();
            if (filename.empty() || file.PaddedFileSize == 0 || file.TargetFileSize == 0
                || file.Offset > header.TotalSize || file.Offset < static_cast<std::uint32_t>(ArchiveSizes::ArchiveHeader)
                || file.PaddedFileSize > header.TotalSize || file.TargetFileSize > header.TotalSize
                || file.PaddedFileSize < file.TargetFileSize
                || NearestMultiple(file.TargetFileSize, 32) != file.PaddedFileSize
                || pointer != file.Offset)
            {
                ThrowRead();
            }
            pointer = UncheckedAdd(pointer, file.PaddedFileSize);
            files.push_back(file);
        }

        if (files.empty()
            || UncheckedAdd(files.back().Offset, files.back().PaddedFileSize) != header.TotalSize)
        {
            ThrowRead();
        }

        std::int32_t filesWritten = 0;
        for (const FileHeader& file : files)
        {
            const std::string filename = file.Filename.MarshalString();
            const std::int32_t start = ManagedInt32(file.Offset);
            const std::int32_t end = UncheckedAdd(start, ManagedInt32(file.TargetFileSize));
            if (!outputDirectory)
            {
                throw System::ArgumentNullException("first");
            }
            const std::string output = Paths::Combine(*outputDirectory, filename);
            const std::int32_t length = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(end) - static_cast<std::uint32_t>(start));
            const auto slice = ReadDetail::Slice(bytes, start, length);
            FileWriteAllBytes(output, slice);
            ++filesWritten;
        }
        return filesWritten;
    }

    std::int32_t Archiver::Extract(const std::string& path, const std::string& destination)
    {
        return Extract(path, std::optional<std::string>(destination));
    }

    void Archiver::Archive(const std::string& destinationPath,
        const std::shared_ptr<const std::vector<std::string>>& filePaths)
    {
        if (!filePaths || filePaths->empty())
        {
            ThrowWrite();
        }

        if (filePaths->size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw System::OverflowException();
        }

        std::vector<std::vector<std::uint8_t>> files;
        std::vector<FileHeader> entries;
        files.reserve(filePaths->size());
        entries.reserve(filePaths->size());

        const std::uint32_t fileCount = static_cast<std::uint32_t>(filePaths->size());
        std::uint32_t pointer = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(ArchiveSizes::ArchiveHeader)
            + static_cast<std::uint64_t>(ArchiveSizes::FileHeader) * fileCount);
        for (const std::string& filePath : *filePaths)
        {
            const std::string filename = GetFileName(filePath);
            if (Utf16Length(filename) > 32)
            {
                ThrowWrite();
            }
            std::vector<std::uint8_t> file = FileReadAllBytes(filePath);
            files.push_back(file);
            FileHeader entry(filename, pointer,
                NearestMultiple(static_cast<std::uint32_t>(file.size()), 32),
                static_cast<std::uint32_t>(file.size()));
            entries.push_back(entry);
            pointer = UncheckedAdd(pointer, entry.PaddedFileSize);
        }

        const ArchiveHeader header(MagicString(), fileCount, pointer);
        std::fstream writer(PathFromUtf8(destinationPath),
            std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!writer.is_open())
        {
            const int error = errno == 0 ? EIO : errno;
            throw std::system_error(error, std::generic_category());
        }
        writer.exceptions(std::ios::badbit | std::ios::failbit);

        std::string magic = MagicString();
        magic.push_back('\0');
        writer.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        WriteUInt32LittleEndian(writer, SwapBytes(header.FileCount));
        WriteUInt32LittleEndian(writer, SwapBytes(header.TotalSize));
        const std::array<char, 20> zeroChars{};
        writer.write(zeroChars.data(), 16);

        for (const FileHeader& entry : entries)
        {
            const std::string filenameBytes = entry.Filename.BinaryWriterBytes();
            writer.write(filenameBytes.data(), static_cast<std::streamsize>(filenameBytes.size()));
            WriteUInt32LittleEndian(writer, SwapBytes(entry.Offset));
            WriteUInt32LittleEndian(writer, SwapBytes(entry.PaddedFileSize));
            WriteUInt32LittleEndian(writer, SwapBytes(entry.TargetFileSize));
            writer.write(zeroChars.data(), 20);
        }

        const std::array<std::uint8_t, 32> zeroPadding{};
        for (std::size_t index = 0; index < files.size(); ++index)
        {
            if (!files[index].empty())
            {
                writer.write(reinterpret_cast<const char*>(files[index].data()),
                    static_cast<std::streamsize>(files[index].size()));
            }
            std::uint32_t padding = entries[index].PaddedFileSize - entries[index].TargetFileSize;
            while (padding != 0)
            {
                const std::uint32_t count = std::min<std::uint32_t>(padding, zeroPadding.size());
                writer.write(reinterpret_cast<const char*>(zeroPadding.data()), count);
                padding -= count;
            }
        }

        writer.flush();
        const std::streampos length = writer.tellp();
        assert(length != std::streampos(-1) && static_cast<std::uint64_t>(length) == pointer);
    }

    std::uint32_t Archiver::SwapBytes(std::uint32_t value) noexcept
    {
        return ((value & 0x000000FFU) << 24)
            | ((value & 0x0000FF00U) << 8)
            | ((value & 0x00FF0000U) >> 8)
            | ((value & 0xFF000000U) >> 24);
    }

    void Archiver::ThrowRead()
    {
        throw std::logic_error("Could not read archive.");
    }

    void Archiver::ThrowWrite()
    {
        throw std::logic_error("Could not write archive.");
    }

    std::uint32_t Archiver::NearestMultiple(std::uint32_t value, std::uint32_t of)
    {
        if (value <= of)
        {
            return value;
        }
        if (of == 0)
        {
            throw std::domain_error("Attempted to divide by zero.");
        }
        while (value % of != 0)
        {
            value += 1;
        }
        return value;
    }

    void Archiver::Nop() noexcept
    {
    }
}

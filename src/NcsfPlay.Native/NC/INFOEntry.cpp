#include "INFOEntry.hpp"

#include <stdexcept>
#include <utility>

namespace NCSFCommon::NC
{
    INFOEntry::INFOEntry(const INFOEntry* other)
    {
        if (other == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }
        _originalFilename = other->_originalFilename;
        _sdatNumber = other->_sdatNumber;
    }

    const std::optional<std::u16string>& INFOEntry::OriginalFilename() const noexcept
    {
        return _originalFilename;
    }

    void INFOEntry::OriginalFilename(std::optional<std::u16string> value)
    {
        _originalFilename = std::move(value);
    }

    const std::optional<std::u16string>& INFOEntry::SDATNumber() const noexcept
    {
        return _sdatNumber;
    }

    void INFOEntry::SDATNumber(std::optional<std::u16string> value)
    {
        _sdatNumber = std::move(value);
    }

    std::u16string INFOEntry::FullFilename(bool multipleSDATs) const
    {
        std::u16string result;
        if (multipleSDATs)
        {
            AppendString(result, _sdatNumber);
            result.push_back(u'/');
        }
        AppendString(result, _originalFilename);
        return result;
    }

    std::u16string INFOEntry::DebuggerDisplay() const
    {
        std::u16string result = u"Original Filename: ";
        AppendString(result, _originalFilename);
        result += u", SDAT #: ";
        AppendString(result, _sdatNumber);
        result += u", ";
        return result;
    }

    void INFOEntry::AppendString(
        std::u16string& destination,
        const std::optional<std::u16string>& value)
    {
        if (value.has_value())
        {
            destination += *value;
        }
    }
}

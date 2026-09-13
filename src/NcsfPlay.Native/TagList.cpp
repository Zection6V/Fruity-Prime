#include "TagList.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <unicode/uchar.h>
#include <unicode/ucol.h>
#include <unicode/utf16.h>
#endif

namespace
{
    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection.");
    }

    [[noreturn]] void ThrowInsertIndexOutOfRange()
    {
        throw std::out_of_range("Index must be within the bounds of the collection.");
    }

    [[noreturn]] void ThrowDuplicateKey()
    {
        throw std::invalid_argument("An item with the same key has already been added.");
    }

    [[noreturn]] void ThrowKeyNotFound()
    {
        throw std::out_of_range("The given key was not present in the collection.");
    }

    [[noreturn]] void ThrowEnumerationModified()
    {
        throw std::logic_error("Collection was modified; enumeration operation may not execute.");
    }

    [[nodiscard]] std::int32_t HashBytes(const std::uint8_t* data, std::size_t size) noexcept
    {
        std::uint32_t hash = 2166136261U;
        for (std::size_t index = 0; index < size; ++index)
        {
            hash ^= data[index];
            hash *= 16777619U;
        }
        return std::bit_cast<std::int32_t>(hash);
    }

#if defined(_WIN32)
    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left, std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }
        if (left.empty() || right.empty())
        {
            return left.empty() && right.empty();
        }
        if (left.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || right.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        const int result = CompareStringEx(
            LOCALE_NAME_INVARIANT,
            NORM_IGNORECASE | NORM_LINGUISTIC_CASING,
            reinterpret_cast<const wchar_t*>(left.data()),
            static_cast<int>(left.size()),
            reinterpret_cast<const wchar_t*>(right.data()),
            static_cast<int>(right.size()),
            nullptr,
            nullptr,
            0);
        if (result == 0)
        {
            throw std::runtime_error("Invariant culture string comparison failed.");
        }
        return result == CSTR_EQUAL;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(std::u16string_view value)
    {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        static constexpr wchar_t Empty[] = L"";
        const wchar_t* source = value.empty()
            ? Empty
            : reinterpret_cast<const wchar_t*>(value.data());
        const int sourceLength = value.empty() ? -1 : static_cast<int>(value.size());
        constexpr DWORD Flags = LCMAP_SORTKEY | NORM_IGNORECASE | NORM_LINGUISTIC_CASING;

        const int required = LCMapStringEx(
            LOCALE_NAME_INVARIANT,
            Flags,
            source,
            sourceLength,
            nullptr,
            0,
            nullptr,
            nullptr,
            0);
        if (required == 0)
        {
            throw std::runtime_error("Invariant culture sort-key generation failed.");
        }

        std::vector<wchar_t> storage((static_cast<std::size_t>(required) + sizeof(wchar_t) - 1) / sizeof(wchar_t));
        if (LCMapStringEx(
                LOCALE_NAME_INVARIANT,
                Flags,
                source,
                sourceLength,
                storage.data(),
                required,
                nullptr,
                nullptr,
                0) != required)
        {
            throw std::runtime_error("Invariant culture sort-key generation failed.");
        }
        return HashBytes(
            reinterpret_cast<const std::uint8_t*>(storage.data()),
            static_cast<std::size_t>(required));
    }
#else
    class InvariantCollator final
    {
    public:
        InvariantCollator()
        {
            UErrorCode status = U_ZERO_ERROR;
            _collator = ucol_open("", &status);
            if (U_FAILURE(status) || _collator == nullptr)
            {
                throw std::runtime_error("Unable to create the ICU invariant collator.");
            }

            ucol_setStrength(_collator, UCOL_TERTIARY);
            status = U_ZERO_ERROR;
            ucol_setAttribute(_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
            if (U_FAILURE(status))
            {
                ucol_close(_collator);
                _collator = nullptr;
                throw std::runtime_error("Unable to configure the ICU invariant collator.");
            }
        }

        InvariantCollator(const InvariantCollator&) = delete;
        InvariantCollator& operator=(const InvariantCollator&) = delete;

        ~InvariantCollator()
        {
            ucol_close(_collator);
        }

        [[nodiscard]] const UCollator* Get() const noexcept
        {
            return _collator;
        }

    private:
        UCollator* _collator = nullptr;
    };

    [[nodiscard]] const InvariantCollator& GetInvariantCollator()
    {
        static const InvariantCollator collator;
        return collator;
    }

    [[nodiscard]] std::u16string FoldInvariantCase(std::u16string_view value)
    {
        std::u16string result;
        result.reserve(value.size());

        const auto* data = reinterpret_cast<const UChar*>(value.data());
        const auto length = static_cast<std::int32_t>(value.size());
        std::int32_t index = 0;
        while (index < length)
        {
            UChar32 codePoint = 0;
            U16_NEXT(data, index, length, codePoint);
            const UChar32 folded = u_foldCase(codePoint, U_FOLD_CASE_DEFAULT);
            if (folded <= 0xFFFF)
            {
                result.push_back(static_cast<char16_t>(folded));
            }
            else
            {
                const UChar32 scalar = folded - 0x10000;
                result.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
            }
        }
        return result;
    }

    [[nodiscard]] bool InvariantCultureIgnoreCaseEquals(
        std::u16string_view left, std::u16string_view right)
    {
        if (left.data() == right.data() && left.size() == right.size())
        {
            return true;
        }
        if (left.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
            || right.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        const std::u16string foldedLeft = FoldInvariantCase(left);
        const std::u16string foldedRight = FoldInvariantCase(right);
        return ucol_strcoll(
            GetInvariantCollator().Get(),
            reinterpret_cast<const UChar*>(foldedLeft.data()),
            static_cast<std::int32_t>(foldedLeft.size()),
            reinterpret_cast<const UChar*>(foldedRight.data()),
            static_cast<std::int32_t>(foldedRight.size())) == UCOL_EQUAL;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(std::u16string_view value)
    {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        const std::u16string folded = FoldInvariantCase(value);
        const UCollator* collator = GetInvariantCollator().Get();
        const std::int32_t required = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(folded.data()),
            static_cast<std::int32_t>(folded.size()),
            nullptr,
            0);
        if (required <= 0)
        {
            throw std::runtime_error("Unable to create the ICU invariant sort key.");
        }

        std::vector<std::uint8_t> sortKey(static_cast<std::size_t>(required));
        const std::int32_t written = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(folded.data()),
            static_cast<std::int32_t>(folded.size()),
            sortKey.data(),
            required);
        if (written != required)
        {
            throw std::runtime_error("Unable to create the ICU invariant sort key.");
        }
        return HashBytes(sortKey.data(), sortKey.size());
    }
#endif
}

namespace NCSFCommon
{
    bool TagList::KeyComparer::Equals(std::u16string_view left, std::u16string_view right) const
    {
        return InvariantCultureIgnoreCaseEquals(left, right);
    }

    std::int32_t TagList::KeyComparer::GetHashCode(std::u16string_view value) const
    {
        return InvariantCultureIgnoreCaseHash(value);
    }

    TagList::ConstIterator::ConstIterator(const TagList* owner, std::size_t index) noexcept
        : _owner(owner),
          _index(index),
          _version(owner == nullptr ? 0 : owner->_version)
    {
    }

    void TagList::ConstIterator::VerifyVersion() const
    {
        if (_owner != nullptr && _version != _owner->_version)
        {
            ThrowEnumerationModified();
        }
    }

    TagList::ConstIterator::reference TagList::ConstIterator::operator*() const
    {
        VerifyVersion();
        if (_owner == nullptr || _index >= _owner->_items.size())
        {
            throw std::out_of_range("Enumeration has either not started or has already finished.");
        }
        return _owner->_items[_index];
    }

    TagList::ConstIterator::pointer TagList::ConstIterator::operator->() const
    {
        return &operator*();
    }

    TagList::ConstIterator& TagList::ConstIterator::operator++()
    {
        VerifyVersion();
        if (_owner != nullptr && _index < _owner->_items.size())
        {
            ++_index;
        }
        return *this;
    }

    TagList::ConstIterator TagList::ConstIterator::operator++(int)
    {
        ConstIterator copy = *this;
        ++(*this);
        return copy;
    }

    bool operator==(const TagList::ConstIterator& left, const TagList::ConstIterator& right)
    {
        left.VerifyVersion();
        right.VerifyVersion();
        return left._owner == right._owner && left._index == right._index;
    }

    TagList::Enumerator::Enumerator(const TagList* owner) noexcept
        : _owner(owner),
          _version(owner == nullptr ? 0 : owner->_version)
    {
    }

    void TagList::Enumerator::VerifyVersion() const
    {
        if (_owner != nullptr && _version != _owner->_version)
        {
            ThrowEnumerationModified();
        }
    }

    bool TagList::Enumerator::MoveNext()
    {
        VerifyVersion();
        if (_owner != nullptr && _nextIndex < _owner->_items.size())
        {
            _current = _owner->_items[_nextIndex];
            ++_nextIndex;
            return true;
        }

        _current = Item{};
        if (_owner != nullptr)
        {
            _nextIndex = _owner->_items.size() + 1;
        }
        return false;
    }

    TagList::Item TagList::Enumerator::Current() const
    {
        return _current;
    }

    void TagList::Enumerator::Reset()
    {
        VerifyVersion();
        _nextIndex = 0;
        _current = Item{};
    }

    std::int32_t TagList::Count() const noexcept
    {
        return static_cast<std::int32_t>(_items.size());
    }

    bool TagList::IsReadOnly() const noexcept
    {
        return false;
    }

    const TagList::KeyComparer& TagList::Comparer() const noexcept
    {
        static const KeyComparer comparer;
        return comparer;
    }

    std::size_t TagList::CheckedIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _items.size())
        {
            ThrowIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    std::size_t TagList::CheckedInsertIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) > _items.size())
        {
            ThrowInsertIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    TagList::Item TagList::operator[](std::int32_t index) const
    {
        return _items[CheckedIndex(index)];
    }

    TagList::Item TagList::operator[](std::u16string_view key) const
    {
        Item item;
        if (!TryGetValue(key, item))
        {
            ThrowKeyNotFound();
        }
        return item;
    }

    void TagList::Set(std::int32_t index, Item item)
    {
        SetItem(CheckedIndex(index), std::move(item));
    }

    void TagList::Add(Item item)
    {
        if (_items.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Collection was too large.");
        }
        InsertItem(_items.size(), std::move(item));
    }

    void TagList::Insert(std::int32_t index, Item item)
    {
        const std::size_t checkedIndex = CheckedInsertIndex(index);
        if (_items.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Collection was too large.");
        }
        InsertItem(checkedIndex, std::move(item));
    }

    bool TagList::Remove(const Item& item)
    {
        const std::int32_t index = IndexOf(item);
        if (index < 0)
        {
            return false;
        }
        RemoveItem(static_cast<std::size_t>(index));
        return true;
    }

    bool TagList::Remove(std::u16string_view key)
    {
        Item item;
        return TryGetValue(key, item) && Remove(item);
    }

    void TagList::RemoveAt(std::int32_t index)
    {
        RemoveItem(CheckedIndex(index));
    }

    void TagList::Clear() noexcept
    {
        _items.clear();
        if (_dictionaryCreated)
        {
            _dictionary.clear();
        }
        _keyCount = 0;
        IncrementVersion();
    }

    bool TagList::Contains(const Item& item) const noexcept
    {
        return IndexOf(item) >= 0;
    }

    bool TagList::Contains(std::u16string_view key) const
    {
        return _dictionaryCreated
            ? FindDictionaryIndex(key) != MissingIndex
            : FindKeyIndex(key) != MissingIndex;
    }

    bool TagList::ContainsKey(std::u16string_view key) const
    {
        return Contains(key);
    }

    bool TagList::TryGetValue(std::u16string_view key, Item& item) const
    {
        if (_dictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index == MissingIndex)
            {
                item = Item{};
                return false;
            }
            item = _dictionary[index].Value;
            return true;
        }

        const std::size_t index = FindKeyIndex(key);
        if (index == MissingIndex)
        {
            item = Item{};
            return false;
        }
        item = _items[index];
        return true;
    }

    std::int32_t TagList::IndexOf(const Item& item) const noexcept
    {
        const auto iterator = std::find(_items.begin(), _items.end(), item);
        if (iterator == _items.end())
        {
            return -1;
        }
        return static_cast<std::int32_t>(std::distance(_items.begin(), iterator));
    }

    void TagList::CopyTo(std::span<Item> array, std::int32_t arrayIndex) const
    {
        if (arrayIndex < 0)
        {
            throw std::out_of_range("Number was less than the array's lower bound in the first dimension.");
        }

        const std::size_t index = static_cast<std::size_t>(arrayIndex);
        if (index > array.size() || _items.size() > array.size() - index)
        {
            throw std::invalid_argument("Destination array was not long enough.");
        }
        std::copy(_items.begin(), _items.end(), array.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void TagList::CopyTo(std::vector<Item>& array, std::int32_t arrayIndex) const
    {
        CopyTo(std::span<Item>(array.data(), array.size()), arrayIndex);
    }

    TagList::ConstIterator TagList::begin() const noexcept
    {
        return ConstIterator(this, 0);
    }

    TagList::ConstIterator TagList::end() const noexcept
    {
        return ConstIterator(this, _items.size());
    }

    TagList::Enumerator TagList::GetEnumerator() const noexcept
    {
        return Enumerator(this);
    }

    void TagList::AddOrReplace(Item item)
    {
        std::int32_t index = -1;
        Item existingItem;
        if (TryGetValue(item.Name, existingItem))
        {
            index = IndexOf(existingItem);
            static_cast<void>(Remove(existingItem));
        }

        if (index == -1)
        {
            Add(std::move(item));
        }
        else
        {
            Insert(index, std::move(item));
        }
    }

    TagList TagList::Clone() const
    {
        TagList clone;
        for (const Item& item : *this)
        {
            clone.Add(item);
        }
        return clone;
    }

    std::u16string_view TagList::GetKeyForItem(const Item& item) const noexcept
    {
        return item.Name;
    }

    void TagList::ChangeItemKey(const Item& item, std::u16string_view newKey)
    {
        if (!ContainsItem(item))
        {
            throw std::invalid_argument("The specified item does not exist in this KeyedCollection.");
        }

        const std::u16string_view oldKey = GetKeyForItem(item);
        if (!Comparer().Equals(oldKey, newKey))
        {
            AddKey(newKey, item);
            RemoveKey(oldKey);
        }
    }

    std::size_t TagList::FindKeyIndex(std::u16string_view key) const
    {
        for (std::size_t index = 0; index < _items.size(); ++index)
        {
            if (Comparer().Equals(key, GetKeyForItem(_items[index])))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    std::size_t TagList::FindDictionaryIndex(std::u16string_view key) const
    {
        for (std::size_t index = 0; index < _dictionary.size(); ++index)
        {
            if (Comparer().Equals(key, _dictionary[index].Key))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    bool TagList::ContainsItem(const Item& item) const
    {
        if (!_dictionaryCreated)
        {
            return Contains(item);
        }

        const std::u16string_view key = GetKeyForItem(item);
        const std::size_t index = FindDictionaryIndex(key);
        return index != MissingIndex && _dictionary[index].Value == item;
    }

    void TagList::EnsureUniqueKey(std::u16string_view key) const
    {
        if (_dictionaryCreated)
        {
            if (FindDictionaryIndex(key) != MissingIndex)
            {
                ThrowDuplicateKey();
            }
            return;
        }

        if (FindKeyIndex(key) != MissingIndex)
        {
            ThrowDuplicateKey();
        }
    }

    void TagList::InsertItem(std::size_t index, Item item)
    {
        const std::u16string_view key = GetKeyForItem(item);
        AddKey(key, item);
        _items.insert(_items.begin() + static_cast<std::ptrdiff_t>(index), std::move(item));
        IncrementVersion();
    }

    void TagList::RemoveItem(std::size_t index)
    {
        const std::u16string key(GetKeyForItem(_items[index]));
        RemoveKey(key);
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(index));
        IncrementVersion();
    }

    void TagList::SetItem(std::size_t index, Item item)
    {
        const std::u16string newKey(GetKeyForItem(item));
        const std::u16string oldKey(GetKeyForItem(_items[index]));

        if (Comparer().Equals(oldKey, newKey))
        {
            if (_dictionaryCreated)
            {
                const std::size_t dictionaryIndex = FindDictionaryIndex(newKey);
                if (dictionaryIndex == MissingIndex)
                {
                    throw std::logic_error("The keyed collection dictionary is inconsistent with its items.");
                }
                _dictionary[dictionaryIndex].Value = item;
            }
        }
        else
        {
            AddKey(newKey, item);
            RemoveKey(oldKey);
        }

        _items[index] = std::move(item);
        IncrementVersion();
    }

    void TagList::AddKey(std::u16string_view key, const Item& item)
    {
        if (_dictionaryCreated)
        {
            EnsureUniqueKey(key);
            _dictionary.push_back(DictionaryEntry{std::u16string(key), item});
            return;
        }

        if (_keyCount == DictionaryCreationThreshold)
        {
            CreateDictionary();
            EnsureUniqueKey(key);
            _dictionary.push_back(DictionaryEntry{std::u16string(key), item});
            return;
        }

        EnsureUniqueKey(key);
        ++_keyCount;
    }

    void TagList::RemoveKey(std::u16string_view key)
    {
        if (_dictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index != MissingIndex)
            {
                _dictionary.erase(_dictionary.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }
        else
        {
            --_keyCount;
        }
    }

    void TagList::CreateDictionary()
    {
        std::vector<DictionaryEntry> dictionary;
        dictionary.reserve(_items.size());
        for (const Item& item : _items)
        {
            const std::u16string_view key = GetKeyForItem(item);
            for (const DictionaryEntry& existing : dictionary)
            {
                if (Comparer().Equals(existing.Key, key))
                {
                    ThrowDuplicateKey();
                }
            }
            dictionary.push_back(DictionaryEntry{std::u16string(key), item});
        }
        _dictionary = std::move(dictionary);
        _dictionaryCreated = true;
    }

    void TagList::IncrementVersion() noexcept
    {
        ++_version;
    }
}

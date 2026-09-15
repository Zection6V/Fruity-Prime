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
#include <unicode/ucol.h>
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

    [[noreturn]] void ThrowArgumentNullKey()
    {
        throw std::invalid_argument("Value cannot be null. (Parameter 'key')");
    }

    [[noreturn]] void ThrowArgumentNullObject()
    {
        throw std::invalid_argument("Value cannot be null. (Parameter 'obj')");
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

            ucol_setStrength(_collator, UCOL_SECONDARY);
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

        return ucol_strcoll(
            GetInvariantCollator().Get(),
            reinterpret_cast<const UChar*>(left.data()),
            static_cast<std::int32_t>(left.size()),
            reinterpret_cast<const UChar*>(right.data()),
            static_cast<std::int32_t>(right.size())) == UCOL_EQUAL;
    }

    [[nodiscard]] std::int32_t InvariantCultureIgnoreCaseHash(std::u16string_view value)
    {
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("String length exceeds the native comparison limit.");
        }

        const UCollator* collator = GetInvariantCollator().Get();
        const std::int32_t required = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(value.data()),
            static_cast<std::int32_t>(value.size()),
            nullptr,
            0);
        if (required <= 0)
        {
            throw std::runtime_error("Unable to create the ICU invariant sort key.");
        }

        std::vector<std::uint8_t> sortKey(static_cast<std::size_t>(required));
        const std::int32_t written = ucol_getSortKey(
            collator,
            reinterpret_cast<const UChar*>(value.data()),
            static_cast<std::int32_t>(value.size()),
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
    std::shared_ptr<const std::u16string> TagList::String::Create(std::u16string value)
    {
        return std::make_shared<const std::u16string>(std::move(value));
    }

    TagList::String::String(std::nullptr_t) noexcept
    {
    }

    TagList::String::String(std::u16string value)
        : _value(Create(std::move(value)))
    {
    }

    TagList::String::String(std::u16string_view value)
        : _value(Create(std::u16string(value)))
    {
    }

    TagList::String& TagList::String::operator=(std::nullptr_t) noexcept
    {
        _value.reset();
        return *this;
    }

    TagList::String& TagList::String::operator=(std::u16string value)
    {
        _value = Create(std::move(value));
        return *this;
    }

    TagList::String& TagList::String::operator=(std::u16string_view value)
    {
        _value = Create(std::u16string(value));
        return *this;
    }

    bool TagList::String::IsNull() const noexcept
    {
        return _value == nullptr;
    }

    std::u16string_view TagList::String::View() const noexcept
    {
        if (_value == nullptr)
        {
            return {};
        }
        return *_value;
    }

    const char16_t* TagList::String::data() const noexcept
    {
        return View().data();
    }

    std::size_t TagList::String::size() const noexcept
    {
        return View().size();
    }

    bool TagList::String::empty() const noexcept
    {
        return View().empty();
    }

    TagList::String::const_iterator TagList::String::begin() const noexcept
    {
        return View().begin();
    }

    TagList::String::const_iterator TagList::String::end() const noexcept
    {
        return View().end();
    }

    TagList::String::operator std::u16string_view() const noexcept
    {
        return View();
    }

    TagList::String::operator std::u16string() const
    {
        return std::u16string(View());
    }

    bool operator==(const TagList::String& left, const TagList::String& right) noexcept
    {
        if (left._value == right._value)
        {
            return true;
        }
        if (left._value == nullptr || right._value == nullptr)
        {
            return false;
        }
        return *left._value == *right._value;
    }

    TagList::KeyComparer::KeyComparer()
    {
#if !defined(_WIN32)
        static_cast<void>(GetInvariantCollator());
#endif
    }

    bool TagList::KeyComparer::Equals(const String& left, const String& right) const
    {
        if (left._value == right._value)
        {
            return true;
        }
        if (left._value == nullptr || right._value == nullptr)
        {
            return false;
        }
        return InvariantCultureIgnoreCaseEquals(left.View(), right.View());
    }

    std::int32_t TagList::KeyComparer::GetHashCode(const String& value) const
    {
        if (value.IsNull())
        {
            ThrowArgumentNullObject();
        }
        return InvariantCultureIgnoreCaseHash(value.View());
    }

    TagList::ConstIterator::ConstIterator(std::shared_ptr<const State> state, std::size_t index) noexcept
        : _state(std::move(state)),
          _index(index),
          _version(_state == nullptr ? 0 : _state->Version)
    {
    }

    void TagList::ConstIterator::VerifyVersion() const
    {
        if (_state != nullptr && _version != _state->Version)
        {
            ThrowEnumerationModified();
        }
    }

    TagList::ConstIterator::reference TagList::ConstIterator::operator*() const
    {
        VerifyVersion();
        if (_state == nullptr || _index >= _state->Items.size())
        {
            throw std::out_of_range("Enumeration has either not started or has already finished.");
        }
        return _state->Items[_index];
    }

    TagList::ConstIterator::pointer TagList::ConstIterator::operator->() const
    {
        return &operator*();
    }

    TagList::ConstIterator& TagList::ConstIterator::operator++()
    {
        VerifyVersion();
        if (_state != nullptr && _index < _state->Items.size())
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
        return left._state == right._state && left._index == right._index;
    }

    TagList::Enumerator::Enumerator(std::shared_ptr<const State> state) noexcept
        : _state(std::move(state)),
          _version(_state == nullptr ? 0 : _state->Version)
    {
    }

    void TagList::Enumerator::VerifyVersion() const
    {
        if (_state != nullptr && _version != _state->Version)
        {
            ThrowEnumerationModified();
        }
    }

    bool TagList::Enumerator::MoveNext()
    {
        VerifyVersion();
        if (_state != nullptr && _nextIndex < _state->Items.size())
        {
            _current = _state->Items[_nextIndex];
            ++_nextIndex;
            return true;
        }

        _current = Item{};
        if (_state != nullptr)
        {
            _nextIndex = _state->Items.size() + 1;
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

    const TagList::KeyComparer& TagList::StaticComparer()
    {
        static const KeyComparer comparer;
        return comparer;
    }

    TagList::TagList()
        : _state((static_cast<void>(StaticComparer()), std::make_shared<State>()))
    {
    }

    TagList::TagList(TagList&& other) noexcept
        : _state(other._state)
    {
    }

    TagList& TagList::operator=(TagList&& other) noexcept
    {
        _state = other._state;
        return *this;
    }

    std::int32_t TagList::Count() const noexcept
    {
        return static_cast<std::int32_t>(_state->Items.size());
    }

    bool TagList::IsReadOnly() const noexcept
    {
        return false;
    }

    const TagList::KeyComparer& TagList::Comparer() const
    {
        return StaticComparer();
    }

    std::size_t TagList::CheckedIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= _state->Items.size())
        {
            ThrowIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    std::size_t TagList::CheckedInsertIndex(std::int32_t index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) > _state->Items.size())
        {
            ThrowInsertIndexOutOfRange();
        }
        return static_cast<std::size_t>(index);
    }

    TagList::Item TagList::operator[](std::int32_t index) const
    {
        return _state->Items[CheckedIndex(index)];
    }

    TagList::Item TagList::operator[](const String& key) const
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
        const std::size_t checkedIndex = CheckedIndex(index);
        SetItem(static_cast<std::int32_t>(checkedIndex), std::move(item));
    }

    void TagList::Add(Item item)
    {
        const std::int32_t index = Count();
        InsertItem(index, std::move(item));
    }

    void TagList::Insert(std::int32_t index, Item item)
    {
        const std::size_t checkedIndex = CheckedInsertIndex(index);
        InsertItem(static_cast<std::int32_t>(checkedIndex), std::move(item));
    }

    bool TagList::Remove(const Item& item)
    {
        const std::int32_t index = IndexOf(item);
        if (index < 0)
        {
            return false;
        }
        RemoveItem(index);
        return true;
    }

    bool TagList::Remove(const String& key)
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            const std::size_t dictionaryIndex = FindDictionaryIndex(key);
            if (dictionaryIndex == MissingIndex)
            {
                return false;
            }
            const Item item = _state->Dictionary[dictionaryIndex].Value;
            return Remove(item);
        }

        for (std::size_t index = 0; index < _state->Items.size(); ++index)
        {
            if (Comparer().Equals(GetKeyForItem(_state->Items[index]), key))
            {
                RemoveItem(static_cast<std::int32_t>(index));
                return true;
            }
        }
        return false;
    }

    void TagList::RemoveAt(std::int32_t index)
    {
        const std::size_t checkedIndex = CheckedIndex(index);
        RemoveItem(static_cast<std::int32_t>(checkedIndex));
    }

    void TagList::Clear()
    {
        ClearItems();
    }

    bool TagList::Contains(const Item& item) const noexcept
    {
        return IndexOf(item) >= 0;
    }

    bool TagList::Contains(const String& key) const
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            return FindDictionaryIndex(key) != MissingIndex;
        }

        for (const Item& item : _state->Items)
        {
            if (Comparer().Equals(GetKeyForItem(item), key))
            {
                return true;
            }
        }
        return false;
    }

    bool TagList::TryGetValue(const String& key, Item& item) const
    {
        if (key.IsNull())
        {
            ThrowArgumentNullKey();
        }

        if (_state->DictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index == MissingIndex)
            {
                item = Item{};
                return false;
            }
            item = _state->Dictionary[index].Value;
            return true;
        }

        for (const Item& itemInItems : _state->Items)
        {
            const String keyInItems = GetKeyForItem(itemInItems);
            if (!keyInItems.IsNull() && Comparer().Equals(key, keyInItems))
            {
                item = itemInItems;
                return true;
            }
        }

        item = Item{};
        return false;
    }

    std::int32_t TagList::IndexOf(const Item& item) const noexcept
    {
        const auto iterator = std::find(_state->Items.begin(), _state->Items.end(), item);
        if (iterator == _state->Items.end())
        {
            return -1;
        }
        return static_cast<std::int32_t>(std::distance(_state->Items.begin(), iterator));
    }

    void TagList::CopyTo(std::span<Item> array, std::int32_t arrayIndex) const
    {
        if (arrayIndex < 0)
        {
            throw std::out_of_range("Number was less than the array's lower bound in the first dimension.");
        }

        const std::size_t index = static_cast<std::size_t>(arrayIndex);
        if (index > array.size() || _state->Items.size() > array.size() - index)
        {
            throw std::invalid_argument("Destination array was not long enough.");
        }
        std::copy(_state->Items.begin(), _state->Items.end(), array.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void TagList::CopyTo(std::vector<Item>& array, std::int32_t arrayIndex) const
    {
        CopyTo(std::span<Item>(array.data(), array.size()), arrayIndex);
    }

    TagList::ConstIterator TagList::begin() const noexcept
    {
        return ConstIterator(_state, 0);
    }

    TagList::ConstIterator TagList::end() const noexcept
    {
        return ConstIterator(_state, _state->Items.size());
    }

    TagList::Enumerator TagList::GetEnumerator() const noexcept
    {
        return Enumerator(_state);
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

    TagList::String TagList::GetKeyForItem(const Item& item) const
    {
        return item.Name;
    }

    void TagList::ChangeItemKey(const Item& item, const String& newKey)
    {
        if (!ContainsItem(item))
        {
            throw std::invalid_argument("The specified item does not exist in this KeyedCollection.");
        }

        const String oldKey = GetKeyForItem(item);
        if (!Comparer().Equals(oldKey, newKey))
        {
            if (!newKey.IsNull())
            {
                AddKey(newKey, item);
            }
            if (!oldKey.IsNull())
            {
                RemoveKey(oldKey);
            }
        }
    }

    void TagList::ClearItems()
    {
        _state->Items.clear();
        IncrementVersion();
        if (_state->DictionaryCreated)
        {
            _state->Dictionary.clear();
        }
        _state->KeyCount = 0;
    }

    void TagList::InsertItem(std::int32_t index, Item item)
    {
        const String key = GetKeyForItem(item);
        if (!key.IsNull())
        {
            AddKey(key, item);
        }

        const std::size_t itemIndex = CheckedInsertIndex(index);
        if (_state->Items.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Collection was too large.");
        }
        _state->Items.insert(
            _state->Items.begin() + static_cast<std::ptrdiff_t>(itemIndex),
            std::move(item));
        IncrementVersion();
    }

    void TagList::RemoveItem(std::int32_t index)
    {
        const std::size_t itemIndex = CheckedIndex(index);
        const String key = GetKeyForItem(_state->Items[itemIndex]);
        if (!key.IsNull())
        {
            RemoveKey(key);
        }
        _state->Items.erase(_state->Items.begin() + static_cast<std::ptrdiff_t>(itemIndex));
        IncrementVersion();
    }

    void TagList::SetItem(std::int32_t index, Item item)
    {
        const String newKey = GetKeyForItem(item);
        const std::size_t itemIndex = CheckedIndex(index);
        const String oldKey = GetKeyForItem(_state->Items[itemIndex]);

        if (Comparer().Equals(oldKey, newKey))
        {
            if (!newKey.IsNull() && _state->DictionaryCreated)
            {
                const std::size_t dictionaryIndex = FindDictionaryIndex(newKey);
                if (dictionaryIndex == MissingIndex)
                {
                    _state->Dictionary.push_back(DictionaryEntry{ newKey, item });
                }
                else
                {
                    _state->Dictionary[dictionaryIndex].Value = item;
                }
            }
        }
        else
        {
            if (!newKey.IsNull())
            {
                AddKey(newKey, item);
            }
            if (!oldKey.IsNull())
            {
                RemoveKey(oldKey);
            }
        }

        _state->Items[itemIndex] = std::move(item);
        IncrementVersion();
    }

    std::size_t TagList::FindKeyIndex(const String& key) const
    {
        for (std::size_t index = 0; index < _state->Items.size(); ++index)
        {
            if (Comparer().Equals(GetKeyForItem(_state->Items[index]), key))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    std::size_t TagList::FindDictionaryIndex(const String& key) const
    {
        for (std::size_t index = 0; index < _state->Dictionary.size(); ++index)
        {
            if (Comparer().Equals(key, _state->Dictionary[index].Key))
            {
                return index;
            }
        }
        return MissingIndex;
    }

    bool TagList::ContainsItem(const Item& item) const
    {
        if (!_state->DictionaryCreated)
        {
            return Contains(item);
        }

        const String key = GetKeyForItem(item);
        if (key.IsNull())
        {
            return Contains(item);
        }

        const std::size_t index = FindDictionaryIndex(key);
        return index != MissingIndex && _state->Dictionary[index].Value == item;
    }

    void TagList::EnsureUniqueKey(const String& key) const
    {
        if (_state->DictionaryCreated)
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

    void TagList::AddKey(const String& key, const Item& item)
    {
        if (_state->DictionaryCreated)
        {
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
            return;
        }

        if (_state->KeyCount == DictionaryCreationThreshold)
        {
            CreateDictionary();
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
            return;
        }

        EnsureUniqueKey(key);
        ++_state->KeyCount;
    }

    void TagList::RemoveKey(const String& key)
    {
        if (_state->DictionaryCreated)
        {
            const std::size_t index = FindDictionaryIndex(key);
            if (index != MissingIndex)
            {
                _state->Dictionary.erase(_state->Dictionary.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }
        else
        {
            --_state->KeyCount;
        }
    }

    void TagList::CreateDictionary()
    {
        _state->Dictionary.clear();
        _state->DictionaryCreated = true;

        for (const Item& item : _state->Items)
        {
            const String key = GetKeyForItem(item);
            if (key.IsNull())
            {
                continue;
            }
            EnsureUniqueKey(key);
            _state->Dictionary.push_back(DictionaryEntry{ key, item });
        }
    }

    void TagList::IncrementVersion() noexcept
    {
        ++_state->Version;
    }
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace NCSFCommon
{
    class TagList
    {
    public:
        struct Item final
        {
            std::u16string Name;
            std::u16string Value;

            [[nodiscard]] friend bool operator==(const Item&, const Item&) noexcept = default;
        };

        class KeyComparer final
        {
        public:
            [[nodiscard]] bool Equals(std::u16string_view left, std::u16string_view right) const;
            [[nodiscard]] std::int32_t GetHashCode(std::u16string_view value) const;

        private:
            KeyComparer() noexcept = default;
            friend class TagList;
        };

        class ConstIterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Item;
            using difference_type = std::ptrdiff_t;
            using pointer = const Item*;
            using reference = const Item&;

            ConstIterator() noexcept = default;

            [[nodiscard]] reference operator*() const;
            [[nodiscard]] pointer operator->() const;
            ConstIterator& operator++();
            ConstIterator operator++(int);

            friend bool operator==(const ConstIterator& left, const ConstIterator& right);
            friend bool operator!=(const ConstIterator& left, const ConstIterator& right)
            {
                return !(left == right);
            }

        private:
            friend class TagList;

            ConstIterator(const TagList* owner, std::size_t index) noexcept;
            void VerifyVersion() const;

            const TagList* _owner = nullptr;
            std::size_t _index = 0;
            std::uint32_t _version = 0;
        };

        class Enumerator final
        {
        public:
            Enumerator() noexcept = default;

            [[nodiscard]] bool MoveNext();
            [[nodiscard]] Item Current() const;
            void Reset();

        private:
            friend class TagList;

            explicit Enumerator(const TagList* owner) noexcept;
            void VerifyVersion() const;

            const TagList* _owner = nullptr;
            std::size_t _nextIndex = 0;
            std::uint32_t _version = 0;
            Item _current;
        };

        TagList() noexcept = default;
        virtual ~TagList() = default;

        TagList(const TagList&) = delete;
        TagList& operator=(const TagList&) = delete;
        TagList(TagList&&) noexcept = default;
        TagList& operator=(TagList&&) noexcept = default;

        [[nodiscard]] std::int32_t Count() const noexcept;
        [[nodiscard]] bool IsReadOnly() const noexcept;
        [[nodiscard]] const KeyComparer& Comparer() const noexcept;

        [[nodiscard]] Item operator[](std::int32_t index) const;
        [[nodiscard]] Item operator[](std::u16string_view key) const;
        void Set(std::int32_t index, Item item);

        void Add(Item item);
        void Insert(std::int32_t index, Item item);
        [[nodiscard]] bool Remove(const Item& item);
        [[nodiscard]] bool Remove(std::u16string_view key);
        void RemoveAt(std::int32_t index);
        void Clear() noexcept;

        [[nodiscard]] bool Contains(const Item& item) const noexcept;
        [[nodiscard]] bool Contains(std::u16string_view key) const;
        [[nodiscard]] bool ContainsKey(std::u16string_view key) const;
        [[nodiscard]] bool TryGetValue(std::u16string_view key, Item& item) const;
        [[nodiscard]] std::int32_t IndexOf(const Item& item) const noexcept;

        void CopyTo(std::span<Item> array, std::int32_t arrayIndex) const;
        void CopyTo(std::vector<Item>& array, std::int32_t arrayIndex) const;

        [[nodiscard]] ConstIterator begin() const noexcept;
        [[nodiscard]] ConstIterator end() const noexcept;
        [[nodiscard]] Enumerator GetEnumerator() const noexcept;

        void AddOrReplace(Item item);
        [[nodiscard]] TagList Clone() const;

    protected:
        [[nodiscard]] virtual std::u16string_view GetKeyForItem(const Item& item) const noexcept;
        void ChangeItemKey(const Item& item, std::u16string_view newKey);

    private:
        [[nodiscard]] std::size_t CheckedIndex(std::int32_t index) const;
        [[nodiscard]] std::size_t CheckedInsertIndex(std::int32_t index) const;
        struct DictionaryEntry final
        {
            std::u16string Key;
            Item Value;
        };

        [[nodiscard]] std::size_t FindKeyIndex(std::u16string_view key) const;
        [[nodiscard]] std::size_t FindDictionaryIndex(std::u16string_view key) const;
        [[nodiscard]] bool ContainsItem(const Item& item) const;
        void EnsureUniqueKey(std::u16string_view key) const;
        void InsertItem(std::size_t index, Item item);
        void RemoveItem(std::size_t index);
        void SetItem(std::size_t index, Item item);
        void AddKey(std::u16string_view key, const Item& item);
        void RemoveKey(std::u16string_view key);
        void CreateDictionary();
        void IncrementVersion() noexcept;

        static constexpr std::size_t MissingIndex = static_cast<std::size_t>(-1);
        static constexpr std::int32_t DictionaryCreationThreshold = 0;

        std::vector<Item> _items;
        bool _dictionaryCreated = false;
        std::vector<DictionaryEntry> _dictionary;
        std::int32_t _keyCount = 0;
        std::uint32_t _version = 0;
    };
}

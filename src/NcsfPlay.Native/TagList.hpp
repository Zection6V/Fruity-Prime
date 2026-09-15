#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace NCSFCommon
{
    class TagList
    {
        struct State;

    public:
        class KeyComparer;

        class String final
        {
        public:
            using const_iterator = std::u16string_view::const_iterator;

            String() noexcept = default;
            String(std::nullptr_t) noexcept;
            template<typename Pointer>
                requires std::same_as<Pointer, const char16_t*> || std::same_as<Pointer, char16_t*>
            String(Pointer value)
            {
                if (value != nullptr)
                {
                    _value = Create(std::u16string(value));
                }
            }
            template<std::size_t N>
            String(const char16_t (&value)[N])
                : String(std::u16string_view(value, N - 1))
            {
            }
            String(std::u16string value);
            String(std::u16string_view value);

            String(const String&) noexcept = default;
            String& operator=(const String&) noexcept = default;
            String(String&&) noexcept = default;
            String& operator=(String&&) noexcept = default;

            String& operator=(std::nullptr_t) noexcept;
            template<typename Pointer>
                requires std::same_as<Pointer, const char16_t*> || std::same_as<Pointer, char16_t*>
            String& operator=(Pointer value)
            {
                if (value == nullptr)
                {
                    _value.reset();
                }
                else
                {
                    _value = Create(std::u16string(value));
                }
                return *this;
            }
            template<std::size_t N>
            String& operator=(const char16_t (&value)[N])
            {
                return operator=(std::u16string_view(value, N - 1));
            }
            String& operator=(std::u16string value);
            String& operator=(std::u16string_view value);

            [[nodiscard]] bool IsNull() const noexcept;
            [[nodiscard]] std::u16string_view View() const noexcept;
            [[nodiscard]] const char16_t* data() const noexcept;
            [[nodiscard]] std::size_t size() const noexcept;
            [[nodiscard]] bool empty() const noexcept;
            [[nodiscard]] const_iterator begin() const noexcept;
            [[nodiscard]] const_iterator end() const noexcept;

            [[nodiscard]] operator std::u16string_view() const noexcept;
            [[nodiscard]] operator std::u16string() const;

            friend bool operator==(const String& left, const String& right) noexcept;
            [[nodiscard]] friend bool operator!=(const String& left, const String& right) noexcept
            {
                return !(left == right);
            }

        private:
            friend class KeyComparer;
            static std::shared_ptr<const std::u16string> Create(std::u16string value);

            std::shared_ptr<const std::u16string> _value;
        };

        struct Item final
        {
            String Name;
            String Value;

            [[nodiscard]] friend bool operator==(const Item&, const Item&) noexcept = default;
        };

        class KeyComparer final
        {
        public:
            [[nodiscard]] bool Equals(const String& left, const String& right) const;
            [[nodiscard]] std::int32_t GetHashCode(const String& value) const;

        private:
            KeyComparer();
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

            ConstIterator(std::shared_ptr<const State> state, std::size_t index) noexcept;
            void VerifyVersion() const;

            std::shared_ptr<const State> _state;
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

            explicit Enumerator(std::shared_ptr<const State> state) noexcept;
            void VerifyVersion() const;

            std::shared_ptr<const State> _state;
            std::size_t _nextIndex = 0;
            std::uint32_t _version = 0;
            Item _current;
        };

        TagList();
        virtual ~TagList() = default;

        TagList(const TagList&) noexcept = default;
        TagList& operator=(const TagList&) noexcept = default;
        TagList(TagList&& other) noexcept;
        TagList& operator=(TagList&& other) noexcept;

        [[nodiscard]] std::int32_t Count() const noexcept;
        [[nodiscard]] bool IsReadOnly() const noexcept;
        [[nodiscard]] const KeyComparer& Comparer() const;

        [[nodiscard]] Item operator[](std::int32_t index) const;
        [[nodiscard]] Item operator[](const String& key) const;
        void Set(std::int32_t index, Item item);

        void Add(Item item);
        void Insert(std::int32_t index, Item item);
        [[nodiscard]] bool Remove(const Item& item);
        [[nodiscard]] bool Remove(const String& key);
        void RemoveAt(std::int32_t index);
        void Clear();

        [[nodiscard]] bool Contains(const Item& item) const noexcept;
        [[nodiscard]] bool Contains(const String& key) const;
        [[nodiscard]] bool TryGetValue(const String& key, Item& item) const;
        [[nodiscard]] std::int32_t IndexOf(const Item& item) const noexcept;

        void CopyTo(std::span<Item> array, std::int32_t arrayIndex) const;
        void CopyTo(std::vector<Item>& array, std::int32_t arrayIndex) const;

        [[nodiscard]] ConstIterator begin() const noexcept;
        [[nodiscard]] ConstIterator end() const noexcept;
        [[nodiscard]] Enumerator GetEnumerator() const noexcept;

        void AddOrReplace(Item item);
        [[nodiscard]] TagList Clone() const;

    protected:
        [[nodiscard]] virtual String GetKeyForItem(const Item& item) const;
        void ChangeItemKey(const Item& item, const String& newKey);
        virtual void ClearItems();
        virtual void InsertItem(std::int32_t index, Item item);
        virtual void RemoveItem(std::int32_t index);
        virtual void SetItem(std::int32_t index, Item item);

    private:
        struct DictionaryEntry final
        {
            String Key;
            Item Value;
        };

        struct State final
        {
            std::vector<Item> Items;
            bool DictionaryCreated = false;
            std::vector<DictionaryEntry> Dictionary;
            std::int32_t KeyCount = 0;
            std::uint32_t Version = 0;
        };

        [[nodiscard]] static const KeyComparer& StaticComparer();
        [[nodiscard]] std::size_t CheckedIndex(std::int32_t index) const;
        [[nodiscard]] std::size_t CheckedInsertIndex(std::int32_t index) const;
        [[nodiscard]] std::size_t FindKeyIndex(const String& key) const;
        [[nodiscard]] std::size_t FindDictionaryIndex(const String& key) const;
        [[nodiscard]] bool ContainsItem(const Item& item) const;
        void EnsureUniqueKey(const String& key) const;
        void AddKey(const String& key, const Item& item);
        void RemoveKey(const String& key);
        void CreateDictionary();
        void IncrementVersion() noexcept;

        static constexpr std::size_t MissingIndex = static_cast<std::size_t>(-1);
        static constexpr std::int32_t DictionaryCreationThreshold = 0;

        std::shared_ptr<State> _state;
    };
}

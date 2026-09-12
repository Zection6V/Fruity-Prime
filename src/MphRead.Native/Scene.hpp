#pragma once

#include "Entities/EntityBase.hpp"
#include "Formats/Enums.hpp"
#include "Formats/Types.hpp"
#include "Messaging.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Entities
{
    class PlatformEntity;
    class ObjectEntity;
    class PlayerSpawnEntity;
    class DoorEntity;
    class ItemSpawnEntity;
    class ItemInstanceEntity;
    class EnemySpawnEntity;
    class TriggerVolumeEntity;
    class AreaVolumeEntity;
    class JumpPadEntity;
    class PointModuleEntity;
    class MorphCameraEntity;
    class OctolithFlagEntity;
    class FlagBaseEntity;
    class TeleporterEntity;
    class NodeDefenseEntity;
    class LightSourceEntity;
    class ArtifactEntity;
    class CamSeqEntity;
    class ForceFieldEntity;
    class BeamEffectEntity;
    class BombEntity;
    class EnemyInstanceEntity;
    class HalfturretEntity;
    class PlayerEntity;
    class BeamProjectileEntity;
    class FhDoorEntity;
    class FhItemSpawnEntity;
    class FhEnemySpawnEntity;
    class FhTriggerVolumeEntity;
    class FhAreaVolumeEntity;
    class FhPlatformEntity;
    class FhJumpPadEntity;
    class FhMorphCameraEntity;
}

namespace MphRead
{
    namespace SceneDetail
    {
        class InvalidOperationException final : public std::logic_error
        {
        public:
            InvalidOperationException()
                : std::logic_error("Operation is not valid due to the current state of the object.")
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

        class KeyNotFoundException final : public std::out_of_range
        {
        public:
            KeyNotFoundException()
                : std::out_of_range("The given key was not present in the dictionary.")
            {
            }
        };

        class DuplicateKeyException final : public std::invalid_argument
        {
        public:
            DuplicateKeyException()
                : std::invalid_argument("An item with the same key has already been added.")
            {
            }
        };

        class IndexOutOfRangeException final : public std::out_of_range
        {
        public:
            IndexOutOfRangeException()
                : std::out_of_range("Index was outside the bounds of the array.")
            {
            }
        };

        template <typename K, typename V>
        class ManagedDictionary final
        {
        public:
            ManagedDictionary() = default;

            void Add(const K& key, V value)
            {
                if (Find(key) != _items.end())
                {
                    throw DuplicateKeyException();
                }
                _items.emplace_back(key, std::move(value));
            }

            [[nodiscard]] bool Remove(const K& key)
            {
                auto item = Find(key);
                if (item == _items.end())
                {
                    return false;
                }
                _items.erase(item);
                return true;
            }

            [[nodiscard]] bool TryGetValue(const K& key, V& value) const
            {
                auto item = Find(key);
                if (item == _items.end())
                {
                    value = V{};
                    return false;
                }
                value = item->second;
                return true;
            }

            [[nodiscard]] V& At(const K& key)
            {
                auto item = Find(key);
                if (item == _items.end())
                {
                    throw KeyNotFoundException();
                }
                return item->second;
            }

            [[nodiscard]] const V& At(const K& key) const
            {
                auto item = Find(key);
                if (item == _items.end())
                {
                    throw KeyNotFoundException();
                }
                return item->second;
            }

            void Set(const K& key, V value)
            {
                auto item = Find(key);
                if (item == _items.end())
                {
                    _items.emplace_back(key, std::move(value));
                }
                else
                {
                    item->second = std::move(value);
                }
            }

        private:
            using Item = std::pair<K, V>;
            using Items = std::vector<Item>;

            [[nodiscard]] typename Items::iterator Find(const K& key)
            {
                for (auto item = _items.begin(); item != _items.end(); ++item)
                {
                    if (item->first == key)
                    {
                        return item;
                    }
                }
                return _items.end();
            }

            [[nodiscard]] typename Items::const_iterator Find(const K& key) const
            {
                for (auto item = _items.begin(); item != _items.end(); ++item)
                {
                    if (item->first == key)
                    {
                        return item;
                    }
                }
                return _items.end();
            }

            Items _items{};
        };

        template <typename T>
        inline constexpr unsigned char TypeTokenStorage = 0;

        template <typename T>
        [[nodiscard]] constexpr const void* TypeToken() noexcept
        {
            return &TypeTokenStorage<T>;
        }

        struct EntityTypeMapEntry final
        {
            const void* Token;
            EntityType Type;
        };

        template <typename T>
        [[nodiscard]] std::array<EntityTypeMapEntry, 34> MakeSpecializedTypeMap()
        {
            return {{
                {TypeToken<Entities::PlatformEntity>(), EntityType::Platform},
                {TypeToken<Entities::ObjectEntity>(), EntityType::Object},
                {TypeToken<Entities::PlayerSpawnEntity>(), EntityType::PlayerSpawn},
                {TypeToken<Entities::DoorEntity>(), EntityType::Door},
                {TypeToken<Entities::ItemSpawnEntity>(), EntityType::ItemSpawn},
                {TypeToken<Entities::ItemInstanceEntity>(), EntityType::ItemInstance},
                {TypeToken<Entities::EnemySpawnEntity>(), EntityType::EnemySpawn},
                {TypeToken<Entities::TriggerVolumeEntity>(), EntityType::TriggerVolume},
                {TypeToken<Entities::AreaVolumeEntity>(), EntityType::AreaVolume},
                {TypeToken<Entities::JumpPadEntity>(), EntityType::JumpPad},
                {TypeToken<Entities::PointModuleEntity>(), EntityType::PointModule},
                {TypeToken<Entities::MorphCameraEntity>(), EntityType::MorphCamera},
                {TypeToken<Entities::OctolithFlagEntity>(), EntityType::OctolithFlag},
                {TypeToken<Entities::FlagBaseEntity>(), EntityType::FlagBase},
                {TypeToken<Entities::TeleporterEntity>(), EntityType::Teleporter},
                {TypeToken<Entities::NodeDefenseEntity>(), EntityType::NodeDefense},
                {TypeToken<Entities::LightSourceEntity>(), EntityType::LightSource},
                {TypeToken<Entities::ArtifactEntity>(), EntityType::Artifact},
                {TypeToken<Entities::CamSeqEntity>(), EntityType::CameraSequence},
                {TypeToken<Entities::ForceFieldEntity>(), EntityType::ForceField},
                {TypeToken<Entities::BeamEffectEntity>(), EntityType::BeamEffect},
                {TypeToken<Entities::BombEntity>(), EntityType::Bomb},
                {TypeToken<Entities::EnemyInstanceEntity>(), EntityType::EnemyInstance},
                {TypeToken<Entities::HalfturretEntity>(), EntityType::Halfturret},
                {TypeToken<Entities::PlayerEntity>(), EntityType::Player},
                {TypeToken<Entities::BeamProjectileEntity>(), EntityType::BeamProjectile},
                {TypeToken<Entities::FhDoorEntity>(), EntityType::FhDoor},
                {TypeToken<Entities::FhItemSpawnEntity>(), EntityType::FhItemSpawn},
                {TypeToken<Entities::FhEnemySpawnEntity>(), EntityType::FhEnemySpawn},
                {TypeToken<Entities::FhTriggerVolumeEntity>(), EntityType::FhTriggerVolume},
                {TypeToken<Entities::FhAreaVolumeEntity>(), EntityType::FhAreaVolume},
                {TypeToken<Entities::FhPlatformEntity>(), EntityType::FhPlatform},
                {TypeToken<Entities::FhJumpPadEntity>(), EntityType::FhJumpPad},
                {TypeToken<Entities::FhMorphCameraEntity>(), EntityType::FhMorphCamera}
            }};
        }
    }

    template <typename T>
    class LinkedList;

    template <typename T>
    class LinkedListNode final
    {
    public:
        explicit LinkedListNode(std::shared_ptr<T> value)
            : _value(std::move(value))
        {
        }

        LinkedListNode(const LinkedListNode&) = delete;
        LinkedListNode& operator=(const LinkedListNode&) = delete;
        LinkedListNode(LinkedListNode&&) = delete;
        LinkedListNode& operator=(LinkedListNode&&) = delete;

        [[nodiscard]] const std::shared_ptr<T>& Value() const noexcept
        {
            return _value;
        }

        void Value(std::shared_ptr<T> value) noexcept
        {
            _value = std::move(value);
        }

        [[nodiscard]] const std::shared_ptr<LinkedListNode<T>>& Next() const noexcept
        {
            return _next;
        }

        [[nodiscard]] const std::shared_ptr<LinkedListNode<T>>& Previous() const noexcept
        {
            return _previous;
        }

        [[nodiscard]] LinkedList<T>* List() const noexcept
        {
            return _list;
        }

    private:
        friend class LinkedList<T>;

        std::shared_ptr<T> _value{};
        std::shared_ptr<LinkedListNode<T>> _next{};
        std::shared_ptr<LinkedListNode<T>> _previous{};
        LinkedList<T>* _list = nullptr;
    };

    template <typename T>
    class LinkedList final
    {
    public:
        using Node = LinkedListNode<T>;
        using NodePtr = std::shared_ptr<Node>;

        LinkedList() = default;
        LinkedList(const LinkedList&) = delete;
        LinkedList& operator=(const LinkedList&) = delete;
        LinkedList(LinkedList&&) = delete;
        LinkedList& operator=(LinkedList&&) = delete;

        ~LinkedList()
        {
            Clear();
        }

        [[nodiscard]] std::int32_t Count() const noexcept
        {
            return _count;
        }

        [[nodiscard]] const NodePtr& First() const noexcept
        {
            return _first;
        }

        [[nodiscard]] const NodePtr& Last() const noexcept
        {
            return _last;
        }

        [[nodiscard]] NodePtr AddFirst(std::shared_ptr<T> value)
        {
            auto node = std::make_shared<Node>(std::move(value));
            node->_list = this;
            node->_next = _first;
            if (_first)
            {
                _first->_previous = node;
            }
            else
            {
                _last = node;
            }
            _first = node;
            ++_count;
            return node;
        }

        [[nodiscard]] NodePtr AddLast(std::shared_ptr<T> value)
        {
            auto node = std::make_shared<Node>(std::move(value));
            node->_list = this;
            node->_previous = _last;
            if (_last)
            {
                _last->_next = node;
            }
            else
            {
                _first = node;
            }
            _last = node;
            ++_count;
            return node;
        }

        [[nodiscard]] NodePtr AddBefore(const NodePtr& node, std::shared_ptr<T> value)
        {
            RequireNode(node);
            auto added = std::make_shared<Node>(std::move(value));
            added->_list = this;
            added->_previous = node->_previous;
            added->_next = node;
            if (node->_previous)
            {
                node->_previous->_next = added;
            }
            else
            {
                _first = added;
            }
            node->_previous = added;
            ++_count;
            return added;
        }

        [[nodiscard]] NodePtr Find(const std::shared_ptr<T>& value) const noexcept
        {
            for (NodePtr node = _first; node; node = node->_next)
            {
                if (node->_value == value)
                {
                    return node;
                }
            }
            return nullptr;
        }

        void Remove(const NodePtr& node)
        {
            RequireNode(node);
            if (node->_previous)
            {
                node->_previous->_next = node->_next;
            }
            else
            {
                _first = node->_next;
            }
            if (node->_next)
            {
                node->_next->_previous = node->_previous;
            }
            else
            {
                _last = node->_previous;
            }
            node->_list = nullptr;
            node->_next.reset();
            node->_previous.reset();
            --_count;
        }

        void Clear() noexcept
        {
            NodePtr node = _first;
            while (node)
            {
                NodePtr next = node->_next;
                node->_list = nullptr;
                node->_next.reset();
                node->_previous.reset();
                node = std::move(next);
            }
            _first.reset();
            _last.reset();
            _count = 0;
        }

    private:
        void RequireNode(const NodePtr& node) const
        {
            if (!node)
            {
                throw std::invalid_argument("Value cannot be null. (Parameter 'node')");
            }
            if (node->_list != this)
            {
                throw SceneDetail::InvalidOperationException();
            }
        }

        NodePtr _first{};
        NodePtr _last{};
        std::int32_t _count = 0;
    };

    template <typename T>
    struct LinkedListEnumerator
    {
    public:
        LinkedListEnumerator() noexcept = default;

        explicit LinkedListEnumerator(std::shared_ptr<LinkedListNode<T>> firstNode) noexcept
            : _first(true),
              _node(std::move(firstNode)),
              _next(_node ? _node->Next() : nullptr)
        {
        }

        [[nodiscard]] std::shared_ptr<T> Current() const
        {
            if (!_node)
            {
                throw SceneDetail::InvalidOperationException();
            }
            return _node->Value();
        }

        [[nodiscard]] bool MoveNext()
        {
            if (_first)
            {
                _first = false;
                return _node != nullptr;
            }
            if (!_node)
            {
                throw System::NullReferenceException();
            }
            std::shared_ptr<LinkedListNode<T>> next = _node->Next()
                ? _node->Next()
                : _next;
            if (!next)
            {
                return false;
            }
            _node = std::move(next);
            _next = _node->Next();
            return true;
        }

    private:
        bool _first = false;
        std::shared_ptr<LinkedListNode<T>> _node{};
        std::shared_ptr<LinkedListNode<T>> _next{};
    };

    template <typename T>
    class LinkedListIterator final
    {
    public:
        LinkedListIterator() noexcept = default;

        explicit LinkedListIterator(std::shared_ptr<LinkedList<T>> list) noexcept
            : _list(std::move(list))
        {
        }

        [[nodiscard]] std::int32_t Count() const
        {
            return List().Count();
        }

        [[nodiscard]] std::shared_ptr<LinkedListNode<T>> FirstNode() const
        {
            return List().First();
        }

        [[nodiscard]] std::shared_ptr<LinkedListNode<T>> LastNode() const
        {
            return List().Last();
        }

        [[nodiscard]] LinkedListEnumerator<T> GetEnumerator() const
        {
            return LinkedListEnumerator<T>(List().First());
        }

    private:
        [[nodiscard]] LinkedList<T>& List() const
        {
            if (!_list)
            {
                throw System::NullReferenceException();
            }
            return *_list;
        }

        std::shared_ptr<LinkedList<T>> _list{};
    };

    template <typename T>
    struct LinkedListEnumeratorSpecialized
    {
    public:
        LinkedListEnumeratorSpecialized() noexcept = default;

        explicit LinkedListEnumeratorSpecialized(
            std::shared_ptr<LinkedListNode<Entities::EntityBase>> firstNode)
            : _first(true),
              _node(std::move(firstNode)),
              _next(_node ? _node->Next() : nullptr),
              _entityType(LookupEntityType())
        {
        }

        [[nodiscard]] std::shared_ptr<T> Current() const
        {
            if (!_node)
            {
                throw SceneDetail::InvalidOperationException();
            }
            const std::shared_ptr<Entities::EntityBase>& value = _node->Value();
            if (!value)
            {
                return nullptr;
            }
            std::shared_ptr<T> cast = std::dynamic_pointer_cast<T>(value);
            if (!cast)
            {
                throw SceneDetail::InvalidCastException();
            }
            return cast;
        }

        [[nodiscard]] bool MoveNext()
        {
            if (_first)
            {
                _first = false;
                return _node != nullptr;
            }
            if (!_node)
            {
                throw System::NullReferenceException();
            }
            std::shared_ptr<LinkedListNode<Entities::EntityBase>> next = _node->Next()
                ? _node->Next()
                : _next;
            if (!next)
            {
                return false;
            }
            const std::shared_ptr<Entities::EntityBase>& value = next->Value();
            if (!value)
            {
                throw System::NullReferenceException();
            }
            if (value->Type > _entityType)
            {
                return false;
            }
            _node = std::move(next);
            _next = _node->Next();
            return true;
        }

    private:
        [[nodiscard]] static EntityType LookupEntityType()
        {
            const void* token = SceneDetail::TypeToken<T>();
            for (const SceneDetail::EntityTypeMapEntry& item : _typeMap)
            {
                if (item.Token == token)
                {
                    return item.Type;
                }
            }
            throw SceneDetail::KeyNotFoundException();
        }

        inline static const std::array<SceneDetail::EntityTypeMapEntry, 34> _typeMap
            = SceneDetail::MakeSpecializedTypeMap<T>();

        bool _first = false;
        std::shared_ptr<LinkedListNode<Entities::EntityBase>> _node{};
        std::shared_ptr<LinkedListNode<Entities::EntityBase>> _next{};
        EntityType _entityType{};
    };

    template <typename T>
    class LinkedListIteratorSpecialized final
    {
    public:
        LinkedListIteratorSpecialized() noexcept = default;

        explicit LinkedListIteratorSpecialized(
            std::shared_ptr<LinkedListNode<Entities::EntityBase>> node) noexcept
            : _node(std::move(node))
        {
        }

        [[nodiscard]] LinkedListEnumeratorSpecialized<T> GetEnumerator() const
        {
            return LinkedListEnumeratorSpecialized<T>(_node);
        }

    private:
        std::shared_ptr<LinkedListNode<Entities::EntityBase>> _node{};
    };

    template <typename T>
    class ImmutableArray final
    {
    public:
        using Value = std::shared_ptr<T>;
        using Storage = std::vector<Value>;

        class ConstIterator final
        {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Value;
            using difference_type = std::ptrdiff_t;
            using reference = Value;
            using pointer = void;

            ConstIterator() noexcept = default;
            explicit ConstIterator(typename Storage::const_iterator iterator) noexcept
                : _iterator(iterator)
            {
            }

            [[nodiscard]] Value operator*() const
            {
                return *_iterator;
            }

            ConstIterator& operator++() noexcept
            {
                ++_iterator;
                return *this;
            }

            ConstIterator operator++(int) noexcept
            {
                ConstIterator copy = *this;
                ++*this;
                return copy;
            }

            [[nodiscard]] friend bool operator==(
                const ConstIterator& left, const ConstIterator& right) noexcept
            {
                return left._iterator == right._iterator;
            }

            [[nodiscard]] friend bool operator!=(
                const ConstIterator& left, const ConstIterator& right) noexcept
            {
                return !(left == right);
            }

        private:
            typename Storage::const_iterator _iterator{};
        };

        ImmutableArray() noexcept = default;

        [[nodiscard]] static ImmutableArray AsImmutableArray(
            std::shared_ptr<Storage> storage)
        {
            ImmutableArray result;
            result._storage = std::move(storage);
            return result;
        }

        [[nodiscard]] static ImmutableArray ToImmutableArray(const Storage& values)
        {
            return AsImmutableArray(std::make_shared<Storage>(values));
        }

        [[nodiscard]] bool IsDefault() const noexcept
        {
            return !_storage;
        }

        [[nodiscard]] bool IsDefaultOrEmpty() const noexcept
        {
            return !_storage || _storage->empty();
        }

        [[nodiscard]] std::int32_t Length() const
        {
            RequireStorage();
            return static_cast<std::int32_t>(_storage->size());
        }

        [[nodiscard]] Value operator[](std::int32_t index) const
        {
            RequireStorage();
            if (index < 0 || static_cast<std::size_t>(index) >= _storage->size())
            {
                throw SceneDetail::IndexOutOfRangeException();
            }
            return (*_storage)[static_cast<std::size_t>(index)];
        }

        [[nodiscard]] ConstIterator begin() const
        {
            RequireStorage();
            return ConstIterator(_storage->begin());
        }

        [[nodiscard]] ConstIterator end() const
        {
            RequireStorage();
            return ConstIterator(_storage->end());
        }

    private:
        void RequireStorage() const
        {
            if (!_storage)
            {
                throw System::NullReferenceException();
            }
        }

        std::shared_ptr<const Storage> _storage{};
    };

    class NavMapEntitySymbol
    {
    public:
        const EntityType Type;
        const std::int16_t Id;
        const std::int32_t SubType;
        const bool Locked;
        const OpenTK::Mathematics::Vector3 Position;
        const OpenTK::Mathematics::Vector3 UpVector;
        const OpenTK::Mathematics::Vector3 FacingVector;

        NavMapEntitySymbol(
            EntityType type,
            std::int16_t id,
            std::int32_t subType,
            bool locked,
            OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 upVector,
            OpenTK::Mathematics::Vector3 facingVector) noexcept;

        NavMapEntitySymbol(const NavMapEntitySymbol&) = delete;
        NavMapEntitySymbol& operator=(const NavMapEntitySymbol&) = delete;
        NavMapEntitySymbol(NavMapEntitySymbol&&) = delete;
        NavMapEntitySymbol& operator=(NavMapEntitySymbol&&) = delete;
    };

    class NavMapRoomSymbols
    {
    public:
        const std::string Name;
        const std::int32_t Id;
        const ImmutableArray<NavMapEntitySymbol> Symbols;

        NavMapRoomSymbols(
            std::string name,
            std::int32_t id,
            std::shared_ptr<std::vector<std::shared_ptr<NavMapEntitySymbol>>> symbols);

        NavMapRoomSymbols(const NavMapRoomSymbols&) = delete;
        NavMapRoomSymbols& operator=(const NavMapRoomSymbols&) = delete;
        NavMapRoomSymbols(NavMapRoomSymbols&&) = delete;
        NavMapRoomSymbols& operator=(NavMapRoomSymbols&&) = delete;
    };

    class Scene
    {
    public:
        Scene() = delete;
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;
        ~Scene() = default;

        [[nodiscard]] LinkedListIterator<Entities::EntityBase> Entities() noexcept;

        void AddEntity(std::shared_ptr<Entities::EntityBase> entity);
        void InsertEntity(std::shared_ptr<Entities::EntityBase> entity);
        void InitializeEntity(const std::shared_ptr<Entities::EntityBase>& entity);
        [[nodiscard]] bool TryGetEntity(
            std::int32_t id,
            std::shared_ptr<Entities::EntityBase>& entity) const;
        void RemoveEntity(const std::shared_ptr<Entities::EntityBase>& entity);
        void RemoveEntityFromMap(const std::shared_ptr<Entities::EntityBase>& entity);

        [[nodiscard]] std::optional<ImmutableArray<::MphRead::NavMapRoomSymbols>>
            NavMapRoomSymbols() const noexcept;
        void LoadMapSymbolEntities(std::int32_t areaId);

        [[nodiscard]] LinkedListIteratorSpecialized<Entities::PlatformEntity> GetPlatformEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::ObjectEntity> GetObjectEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::PlayerSpawnEntity> GetPlayerSpawnEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::DoorEntity> GetDoorEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::ItemSpawnEntity> GetItemSpawnEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::ItemInstanceEntity> GetItemInstanceEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::EnemySpawnEntity> GetEnemySpawnEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::TriggerVolumeEntity> GetTriggerVolumeEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::AreaVolumeEntity> GetAreaVolumeEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::JumpPadEntity> GetJumpPadEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::PointModuleEntity> GetPointModuleEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::MorphCameraEntity> GetMorphCameraEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::OctolithFlagEntity> GetOctolithFlagEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FlagBaseEntity> GetFlagBaseEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::TeleporterEntity> GetTeleporterEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::NodeDefenseEntity> GetNodeDefenseEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::LightSourceEntity> GetLightSourceEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::ArtifactEntity> GetArtifactEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::CamSeqEntity> GetCamSeqEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::ForceFieldEntity> GetForceFieldEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::BeamEffectEntity> GetBeamEffectEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::BombEntity> GetBombEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::EnemyInstanceEntity> GetEnemyInstanceEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::HalfturretEntity> GetHalfturretEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::PlayerEntity> GetPlayerEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::BeamProjectileEntity> GetBeamProjectileEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhDoorEntity> GetFhDoorEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhItemSpawnEntity> GetFhItemSpawnEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhEnemySpawnEntity> GetFhEnemySpawnEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhTriggerVolumeEntity> GetFhTriggerVolumeEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhAreaVolumeEntity> GetFhAreaVolumeEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhPlatformEntity> GetFhPlatformEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhJumpPadEntity> GetFhJumpPadEntities() const;
        [[nodiscard]] LinkedListIteratorSpecialized<Entities::FhMorphCameraEntity> GetFhMorphCameraEntities() const;

        // Declared here because Scene.hpp is the canonical partial-class owner.
        // Implementations belong to the corresponding C# partials, not Scene.cs.
        void InitEntity(const std::shared_ptr<Entities::EntityBase>& entity);
        [[nodiscard]] static MphRead::Language Language();

        MPHREAD_SCENE_MESSAGING_MEMBERS

    private:
        using EntityNode = LinkedListNode<Entities::EntityBase>;
        using EntityNodePtr = std::shared_ptr<EntityNode>;
        using EntityNodeMap = SceneDetail::ManagedDictionary<EntityType, EntityNodePtr>;

        static EntityNodeMap MakeEntityNodeMap();
        void InsertEntityByType(const std::shared_ptr<Entities::EntityBase>& entity);

        const std::shared_ptr<LinkedList<Entities::EntityBase>> _entities
            = std::make_shared<LinkedList<Entities::EntityBase>>();
        SceneDetail::ManagedDictionary<std::int32_t, std::shared_ptr<Entities::EntityBase>> _entityMap{};
        std::optional<ImmutableArray<::MphRead::NavMapRoomSymbols>> _navMapRoomSymbols{};
        EntityNodeMap _entityNodesByType = MakeEntityNodeMap();

        // Owned by Renderer.cs, but required by the accepted Messaging.cpp partial.
        std::uint64_t _frameCount = 0;
    };
}

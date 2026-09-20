#include "MapBuilder.hpp"

#include "../../Entities/TriggerVolumeEntity.hpp"
#include "../../Formats/EntityClass.hpp"
#include "../../Formats/Formats.hpp"
#include "../../Program.hpp"
#include "BuiltMap.hpp"
#include "MapDefinition.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using MphRead::ItemType;
    using MphRead::Terrain;
    using OpenTK::Mathematics::Vector2;
    using OpenTK::Mathematics::Vector3;

    [[noreturn]] void NullReference()
    {
        throw System::NullReferenceException();
    }

    [[noreturn]] void ArrayBounds()
    {
        throw std::out_of_range("Index was outside the bounds of the array.");
    }

    [[nodiscard]] float ArrayValue(const std::vector<float>* values, std::size_t index)
    {
        if (values == nullptr)
        {
            NullReference();
        }
        if (index >= values->size())
        {
            ArrayBounds();
        }
        return (*values)[index];
    }

    [[nodiscard]] float ManagedMin(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left == right && left == 0.0F)
        {
            return std::signbit(left) || std::signbit(right) ? -0.0F : 0.0F;
        }
        return left < right ? left : right;
    }

    [[nodiscard]] float ManagedMax(float left, float right) noexcept
    {
        if (std::isnan(left))
        {
            return left;
        }
        if (std::isnan(right))
        {
            return right;
        }
        if (left == right && left == 0.0F)
        {
            return std::signbit(left) && std::signbit(right) ? -0.0F : 0.0F;
        }
        return left > right ? left : right;
    }

    [[nodiscard]] float Length(Vector3 value) noexcept
    {
        return std::sqrt((value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z));
    }

    [[nodiscard]] Vector3 Divide(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
    }

    [[nodiscard]] float ManagedAbs(float value) noexcept
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value) & 0x7FFFFFFFU;
        return std::bit_cast<float>(bits);
    }

    [[nodiscard]] bool IsAsciiWhitespace(unsigned char value) noexcept
    {
        return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
    }

    [[nodiscard]] std::size_t LeadingWhitespaceBytes(std::string_view value) noexcept
    {
        if (value.empty())
        {
            return 0;
        }
        const auto b0 = static_cast<unsigned char>(value[0]);
        if (IsAsciiWhitespace(b0))
        {
            return 1;
        }
        if (value.size() >= 2)
        {
            const auto b1 = static_cast<unsigned char>(value[1]);
            if (b0 == 0xC2U && (b1 == 0x85U || b1 == 0xA0U))
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const auto b1 = static_cast<unsigned char>(value[1]);
            const auto b2 = static_cast<unsigned char>(value[2]);
            if ((b0 == 0xE1U && b1 == 0x9AU && b2 == 0x80U)
                || (b0 == 0xE2U && b1 == 0x80U
                    && ((b2 >= 0x80U && b2 <= 0x8AU)
                        || b2 == 0xA8U || b2 == 0xA9U || b2 == 0xAFU))
                || (b0 == 0xE2U && b1 == 0x81U && b2 == 0x9FU)
                || (b0 == 0xE3U && b1 == 0x80U && b2 == 0x80U))
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::size_t TrailingWhitespaceBytes(std::string_view value) noexcept
    {
        if (value.empty())
        {
            return 0;
        }
        const auto last = static_cast<unsigned char>(value.back());
        if (IsAsciiWhitespace(last))
        {
            return 1;
        }
        if (value.size() >= 2)
        {
            const auto b0 = static_cast<unsigned char>(value[value.size() - 2]);
            if (b0 == 0xC2U && (last == 0x85U || last == 0xA0U))
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const auto b0 = static_cast<unsigned char>(value[value.size() - 3]);
            const auto b1 = static_cast<unsigned char>(value[value.size() - 2]);
            if ((b0 == 0xE1U && b1 == 0x9AU && last == 0x80U)
                || (b0 == 0xE2U && b1 == 0x80U
                    && ((last >= 0x80U && last <= 0x8AU)
                        || last == 0xA8U || last == 0xA9U || last == 0xAFU))
                || (b0 == 0xE2U && b1 == 0x81U && last == 0x9FU)
                || (b0 == 0xE3U && b1 == 0x80U && last == 0x80U))
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::string_view TrimEnumWhitespace(std::string_view value) noexcept
    {
        for (;;)
        {
            const std::size_t count = LeadingWhitespaceBytes(value);
            if (count == 0)
            {
                break;
            }
            value.remove_prefix(count);
        }
        for (;;)
        {
            const std::size_t count = TrailingWhitespaceBytes(value);
            if (count == 0)
            {
                break;
            }
            value.remove_suffix(count);
        }
        return value;
    }

    [[nodiscard]] bool EqualsIgnoreCaseAscii(
        std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            unsigned char a = static_cast<unsigned char>(left[i]);
            unsigned char b = static_cast<unsigned char>(right[i]);
            if (a >= 'a' && a <= 'z')
            {
                a = static_cast<unsigned char>(a - ('a' - 'A'));
            }
            if (b >= 'a' && b <= 'z')
            {
                b = static_cast<unsigned char>(b - ('a' - 'A'));
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool TryParseSignedDecimal(
        std::string_view value,
        std::int64_t minimum,
        std::int64_t maximum,
        std::int64_t& result) noexcept
    {
        for (;;)
        {
            const std::size_t count = LeadingWhitespaceBytes(value);
            if (count == 0)
            {
                break;
            }
            value.remove_prefix(count);
        }
        if (value.empty())
        {
            return false;
        }

        bool negative = false;
        std::size_t position = 0;
        if (value[position] == '+' || value[position] == '-')
        {
            negative = value[position] == '-';
            ++position;
        }
        if (position == value.size())
        {
            return false;
        }

        const std::uint64_t positiveLimit = static_cast<std::uint64_t>(maximum);
        const std::uint64_t negativeLimit = minimum < 0
            ? static_cast<std::uint64_t>(-(minimum + 1)) + 1U
            : 0U;
        const std::uint64_t limit = negative
            ? (minimum < 0 ? negativeLimit : positiveLimit)
            : positiveLimit;

        std::uint64_t magnitude = 0;
        const std::size_t digitsStart = position;
        for (; position < value.size(); ++position)
        {
            const char ch = value[position];
            if (ch < '0' || ch > '9')
            {
                break;
            }
            const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
            if (magnitude > (limit - digit) / 10U)
            {
                return false;
            }
            magnitude = magnitude * 10U + digit;
        }
        if (position == digitsStart)
        {
            return false;
        }

        while (position < value.size()
            && IsAsciiWhitespace(static_cast<unsigned char>(value[position])))
        {
            ++position;
        }
        while (position < value.size() && value[position] == '\0')
        {
            ++position;
        }
        if (position != value.size())
        {
            return false;
        }

        if (negative)
        {
            if (minimum >= 0)
            {
                if (magnitude != 0)
                {
                    return false;
                }
                result = 0;
                return true;
            }
            if (magnitude > negativeLimit)
            {
                return false;
            }
            result = magnitude == negativeLimit
                ? minimum
                : -static_cast<std::int64_t>(magnitude);
        }
        else
        {
            if (magnitude > positiveLimit)
            {
                return false;
            }
            result = static_cast<std::int64_t>(magnitude);
        }
        return result >= minimum && result <= maximum;
    }

    [[nodiscard]] bool TryItemName(std::string_view value, ItemType& result) noexcept
    {
        struct Entry final
        {
            std::string_view Name;
            ItemType Value;
        };
        static constexpr Entry Entries[] = {
            {"None", ItemType::None},
            {"HealthMedium", ItemType::HealthMedium},
            {"HealthSmall", ItemType::HealthSmall},
            {"HealthBig", ItemType::HealthBig},
            {"DoubleDamage", ItemType::DoubleDamage},
            {"EnergyTank", ItemType::EnergyTank},
            {"VoltDriver", ItemType::VoltDriver},
            {"MissileExpansion", ItemType::MissileExpansion},
            {"Battlehammer", ItemType::Battlehammer},
            {"Imperialist", ItemType::Imperialist},
            {"Judicator", ItemType::Judicator},
            {"Magmaul", ItemType::Magmaul},
            {"ShockCoil", ItemType::ShockCoil},
            {"OmegaCannon", ItemType::OmegaCannon},
            {"UASmall", ItemType::UASmall},
            {"UABig", ItemType::UABig},
            {"MissileSmall", ItemType::MissileSmall},
            {"MissileBig", ItemType::MissileBig},
            {"Cloak", ItemType::Cloak},
            {"UAExpansion", ItemType::UAExpansion},
            {"ArtifactKey", ItemType::ArtifactKey},
            {"Deathalt", ItemType::Deathalt},
            {"AffinityWeapon", ItemType::AffinityWeapon},
            {"PickWpnMissile", ItemType::PickWpnMissile}
        };
        for (const Entry& entry : Entries)
        {
            if (EqualsIgnoreCaseAscii(value, entry.Name))
            {
                result = entry.Value;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool TryTerrainName(std::string_view value, Terrain& result) noexcept
    {
        struct Entry final
        {
            std::string_view Name;
            Terrain Value;
        };
        static constexpr Entry Entries[] = {
            {"Metal", Terrain::Metal},
            {"OrangeHolo", Terrain::OrangeHolo},
            {"GreenHolo", Terrain::GreenHolo},
            {"BlueHolo", Terrain::BlueHolo},
            {"Ice", Terrain::Ice},
            {"Snow", Terrain::Snow},
            {"Sand", Terrain::Sand},
            {"Rock", Terrain::Rock},
            {"Lava", Terrain::Lava},
            {"Acid", Terrain::Acid},
            {"Gorea", Terrain::Gorea},
            {"Unknown11", Terrain::Unknown11},
            {"All", Terrain::All}
        };
        for (const Entry& entry : Entries)
        {
            if (EqualsIgnoreCaseAscii(value, entry.Name))
            {
                result = entry.Value;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool TryParseItemType(
        const std::string* text, ItemType& result) noexcept
    {
        result = static_cast<ItemType>(0);
        if (text == nullptr)
        {
            return false;
        }
        const std::string_view rawValue = *text;
        std::int64_t numeric = 0;
        if (TryParseSignedDecimal(
                rawValue,
                std::numeric_limits<std::int32_t>::min(),
                std::numeric_limits<std::int32_t>::max(),
                numeric))
        {
            result = static_cast<ItemType>(static_cast<std::int32_t>(numeric));
            return true;
        }

        const std::string_view value = TrimEnumWhitespace(rawValue);
        if (value.empty())
        {
            return false;
        }
        std::uint32_t combined = 0;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t comma = value.find(',', start);
            const std::string_view token = TrimEnumWhitespace(
                value.substr(start, comma == std::string_view::npos
                    ? std::string_view::npos : comma - start));
            ItemType parsed{};
            if (token.empty() || !TryItemName(token, parsed))
            {
                result = static_cast<ItemType>(0);
                return false;
            }
            combined |= std::bit_cast<std::uint32_t>(
                static_cast<std::int32_t>(parsed));
            if (comma == std::string_view::npos)
            {
                result = static_cast<ItemType>(std::bit_cast<std::int32_t>(combined));
                return true;
            }
            start = comma + 1;
        }
    }

    [[nodiscard]] bool TryParseTerrain(
        const std::optional<std::string>& text, Terrain& result) noexcept
    {
        result = static_cast<Terrain>(0);
        if (!text)
        {
            return false;
        }
        const std::string_view rawValue = *text;
        std::int64_t numeric = 0;
        if (TryParseSignedDecimal(rawValue, 0, 255, numeric))
        {
            result = static_cast<Terrain>(static_cast<std::uint8_t>(numeric));
            return true;
        }

        const std::string_view value = TrimEnumWhitespace(rawValue);
        if (value.empty())
        {
            return false;
        }
        std::uint8_t combined = 0;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t comma = value.find(',', start);
            const std::string_view token = TrimEnumWhitespace(
                value.substr(start, comma == std::string_view::npos
                    ? std::string_view::npos : comma - start));
            Terrain parsed{};
            if (token.empty() || !TryTerrainName(token, parsed))
            {
                result = static_cast<Terrain>(0);
                return false;
            }
            combined = static_cast<std::uint8_t>(
                combined | static_cast<std::uint8_t>(parsed));
            if (comma == std::string_view::npos)
            {
                result = static_cast<Terrain>(combined);
                return true;
            }
            start = comma + 1;
        }
    }

    [[nodiscard]] std::string ItemTypeToString(ItemType value)
    {
        switch (value)
        {
        case ItemType::None: return "None";
        case ItemType::HealthMedium: return "HealthMedium";
        case ItemType::HealthSmall: return "HealthSmall";
        case ItemType::HealthBig: return "HealthBig";
        case ItemType::DoubleDamage: return "DoubleDamage";
        case ItemType::EnergyTank: return "EnergyTank";
        case ItemType::VoltDriver: return "VoltDriver";
        case ItemType::MissileExpansion: return "MissileExpansion";
        case ItemType::Battlehammer: return "Battlehammer";
        case ItemType::Imperialist: return "Imperialist";
        case ItemType::Judicator: return "Judicator";
        case ItemType::Magmaul: return "Magmaul";
        case ItemType::ShockCoil: return "ShockCoil";
        case ItemType::OmegaCannon: return "OmegaCannon";
        case ItemType::UASmall: return "UASmall";
        case ItemType::UABig: return "UABig";
        case ItemType::MissileSmall: return "MissileSmall";
        case ItemType::MissileBig: return "MissileBig";
        case ItemType::Cloak: return "Cloak";
        case ItemType::UAExpansion: return "UAExpansion";
        case ItemType::ArtifactKey: return "ArtifactKey";
        case ItemType::Deathalt: return "Deathalt";
        case ItemType::AffinityWeapon: return "AffinityWeapon";
        case ItemType::PickWpnMissile: return "PickWpnMissile";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] std::string JoinMultiplayerItems(
        const MphRead::Mods::MapGen::ItemTypeHashSet& values)
    {
        std::string result;
        bool first = true;
        for (const ItemType value : values)
        {
            if (!first)
            {
                result += ", ";
            }
            first = false;
            result += ItemTypeToString(value);
        }
        return result;
    }

    [[nodiscard]] std::optional<std::string> MapItemTypeText(
        MphRead::Mods::MapGen::MapItem* item)
    {
        if (item == nullptr)
        {
            NullReference();
        }
        try
        {
            return item->Type();
        }
        catch (const std::runtime_error& exception)
        {
            if (std::string_view(exception.what())
                == "Object reference not set to an instance of an object.")
            {
                return std::nullopt;
            }
            throw;
        }
    }

    [[nodiscard]] std::int16_t TruncateToInt16(std::size_t value) noexcept
    {
        const std::uint16_t low = static_cast<std::uint16_t>(
            static_cast<std::uint64_t>(value) & 0xFFFFU);
        return std::bit_cast<std::int16_t>(low);
    }

    [[nodiscard]] std::int16_t PostIncrement(std::int16_t& value) noexcept
    {
        const std::int16_t old = value;
        const std::uint16_t bits = std::bit_cast<std::uint16_t>(value);
        value = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits + 1U));
        return old;
    }

    template <typename T>
    [[nodiscard]] T* Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            NullReference();
        }
        return value.get();
    }

    [[nodiscard]] const std::shared_ptr<std::string>& RmMainString()
    {
        static const auto value = std::make_shared<std::string>("rmMain");
        return value;
    }

    [[nodiscard]] MphRead::Interop::ManagedArray<Vector3>* BuiltPoints(
        MphRead::ManagedArray<Vector3>* points) noexcept
    {
        return reinterpret_cast<MphRead::Interop::ManagedArray<Vector3>*>(points);
    }

    [[nodiscard]] MphRead::Interop::ManagedArray<Vector2>* BuiltTexcoords(
        MphRead::ManagedArray<Vector2>* texcoords) noexcept
    {
        return reinterpret_cast<MphRead::Interop::ManagedArray<Vector2>*>(texcoords);
    }
}

namespace MphRead::Mods::MapGen
{
    ItemTypeHashSet::const_iterator::const_iterator(
        const ItemTypeHashSet* owner, std::size_t index) noexcept
        : _owner(owner), _index(index)
    {
        SkipFree();
    }

    void ItemTypeHashSet::const_iterator::SkipFree() noexcept
    {
        if (_owner == nullptr)
        {
            return;
        }
        while (_index < _owner->_slots.size()
            && !_owner->_slots[_index].Occupied)
        {
            ++_index;
        }
    }

    ItemType ItemTypeHashSet::const_iterator::operator*() const noexcept
    {
        return _owner->_slots[_index].Value;
    }

    ItemTypeHashSet::const_iterator& ItemTypeHashSet::const_iterator::operator++() noexcept
    {
        ++_index;
        SkipFree();
        return *this;
    }

    void ItemTypeHashSet::const_iterator::operator++(int) noexcept
    {
        ++*this;
    }

    ItemTypeHashSet::ItemTypeHashSet(std::initializer_list<ItemType> values)
    {
        for (const ItemType value : values)
        {
            (void)Add(value);
        }
    }

    bool ItemTypeHashSet::Add(ItemType value)
    {
        if (Contains(value))
        {
            return false;
        }

        if (_freeList >= 0)
        {
            const std::int32_t index = _freeList;
            Slot& slot = _slots[static_cast<std::size_t>(index)];
            _freeList = slot.NextFree;
            slot.Value = value;
            slot.Occupied = true;
            slot.NextFree = -1;
        }
        else
        {
            _slots.push_back(Slot{value, true, -1});
        }
        ++_count;
        return true;
    }

    bool ItemTypeHashSet::Remove(ItemType value) noexcept
    {
        for (std::size_t i = 0; i < _slots.size(); ++i)
        {
            Slot& slot = _slots[i];
            if (slot.Occupied && slot.Value == value)
            {
                slot.Occupied = false;
                slot.NextFree = _freeList;
                _freeList = static_cast<std::int32_t>(i);
                --_count;
                return true;
            }
        }
        return false;
    }

    bool ItemTypeHashSet::Contains(ItemType value) const noexcept
    {
        for (const Slot& slot : _slots)
        {
            if (slot.Occupied && slot.Value == value)
            {
                return true;
            }
        }
        return false;
    }

    void ItemTypeHashSet::Clear() noexcept
    {
        _slots.clear();
        _freeList = -1;
        _count = 0;
    }

    std::int32_t ItemTypeHashSet::Count() const noexcept
    {
        return _count;
    }

    ItemTypeHashSet::const_iterator ItemTypeHashSet::begin() const noexcept
    {
        return const_iterator(this, 0);
    }

    ItemTypeHashSet::const_iterator ItemTypeHashSet::end() const noexcept
    {
        return const_iterator(this, _slots.size());
    }

    ItemTypeHashSet MapBuilder::MultiplayerItems{
        ItemType::HealthSmall,
        ItemType::HealthMedium,
        ItemType::HealthBig,
        ItemType::UASmall,
        ItemType::UABig,
        ItemType::MissileSmall,
        ItemType::MissileBig,
        ItemType::DoubleDamage,
        ItemType::Cloak,
        ItemType::Deathalt,
        ItemType::VoltDriver,
        ItemType::Battlehammer,
        ItemType::Imperialist,
        ItemType::Judicator,
        ItemType::Magmaul,
        ItemType::ShockCoil,
        ItemType::OmegaCannon,
        ItemType::AffinityWeapon
    };

    const std::array<float, 6> MapBuilder::_faceShades{
        1.0F, 0.55F, 0.82F, 0.82F, 0.74F, 0.74F
    };

    std::shared_ptr<BuiltMap> MapBuilder::Build(MapDefinition* def)
    {
        auto map = std::make_shared<BuiltMap>(def);

        if (def == nullptr)
        {
            NullReference();
        }
        MapDefinition::BrushList* brushes = def->Brushes();
        if (brushes == nullptr)
        {
            NullReference();
        }
        for (const std::shared_ptr<MapBrush>& brush : *brushes)
        {
            AddBrush(map.get(), def, brush.get());
        }
        AddEntities(map.get(), def);
        return map;
    }

    void MapBuilder::AddBrush(BuiltMap* map, MapDefinition* def, MapBrush* brush)
    {
        if (brush == nullptr)
        {
            NullReference();
        }

        const float x0Left = ArrayValue(brush->Min(), 0);
        const float x0Right = ArrayValue(brush->Max(), 0);
        const float x0 = ManagedMin(x0Left, x0Right);
        const float y0Left = ArrayValue(brush->Min(), 1);
        const float y0Right = ArrayValue(brush->Max(), 1);
        const float y0 = ManagedMin(y0Left, y0Right);
        const float z0Left = ArrayValue(brush->Min(), 2);
        const float z0Right = ArrayValue(brush->Max(), 2);
        const float z0 = ManagedMin(z0Left, z0Right);
        const float x1Left = ArrayValue(brush->Min(), 0);
        const float x1Right = ArrayValue(brush->Max(), 0);
        const float x1 = ManagedMax(x1Left, x1Right);
        const float y1Left = ArrayValue(brush->Min(), 1);
        const float y1Right = ArrayValue(brush->Max(), 1);
        const float y1 = ManagedMax(y1Left, y1Right);
        const float z1Left = ArrayValue(brush->Min(), 2);
        const float z1Right = ArrayValue(brush->Max(), 2);
        const float z1 = ManagedMax(z1Left, z1Right);

        const Vector3 normals[6] = {
            Vector3(0.0F, 1.0F, 0.0F),
            Vector3(0.0F, -1.0F, 0.0F),
            Vector3(1.0F, 0.0F, 0.0F),
            Vector3(-1.0F, 0.0F, 0.0F),
            Vector3(0.0F, 0.0F, 1.0F),
            Vector3(0.0F, 0.0F, -1.0F)
        };
        const Vector3 sidePoints[6][4] = {
            {
                Vector3(x0, y1, z1), Vector3(x1, y1, z1),
                Vector3(x1, y1, z0), Vector3(x0, y1, z0)
            },
            {
                Vector3(x0, y0, z0), Vector3(x1, y0, z0),
                Vector3(x1, y0, z1), Vector3(x0, y0, z1)
            },
            {
                Vector3(x1, y0, z1), Vector3(x1, y0, z0),
                Vector3(x1, y1, z0), Vector3(x1, y1, z1)
            },
            {
                Vector3(x0, y0, z0), Vector3(x0, y0, z1),
                Vector3(x0, y1, z1), Vector3(x0, y1, z0)
            },
            {
                Vector3(x0, y0, z1), Vector3(x1, y0, z1),
                Vector3(x1, y1, z1), Vector3(x0, y1, z1)
            },
            {
                Vector3(x1, y0, z0), Vector3(x0, y0, z0),
                Vector3(x0, y1, z0), Vector3(x1, y1, z0)
            }
        };

        float texScale = 16.0F;
        if (brush->Material() >= 0)
        {
            const std::int32_t materialForCount = brush->Material();
            if (def == nullptr)
            {
                NullReference();
            }
            MapDefinition::MaterialList* materialsForCount = def->Materials();
            if (materialsForCount == nullptr)
            {
                NullReference();
            }
            if (static_cast<std::int64_t>(materialForCount)
                < static_cast<std::int64_t>(materialsForCount->size()))
            {
                MapDefinition::MaterialList* materialsForIndex = def->Materials();
                const std::int32_t materialForIndex = brush->Material();
                if (materialsForIndex == nullptr)
                {
                    NullReference();
                }
                MapMaterial* material = Require(
                    materialsForIndex->at(static_cast<std::size_t>(materialForIndex)));
                texScale = material->TexScale();
            }
        }

        const Vector3 origin(x0, y0, z0);
        Terrain terrain = Terrain::Metal;
        Terrain parsed = Terrain::Metal;
        if (brush->Terrain().has_value())
        {
            const std::optional<std::string> terrainText = brush->Terrain();
            if (TryParseTerrain(terrainText, parsed))
            {
                terrain = parsed;
            }
        }

        for (std::size_t i = 0; i < 6; ++i)
        {
            auto* points = new MphRead::ManagedArray<Vector3>(4);
            auto* texcoords = new MphRead::ManagedArray<Vector2>(4);
            for (std::size_t j = 0; j < 4; ++j)
            {
                (*points)[j] = sidePoints[i][j];
                (*texcoords)[j] = Project(sidePoints[i][j], normals[i], origin, texScale);
            }

            const std::int32_t faceMaterial = brush->Material();
            const float faceShade = _faceShades[i] * brush->Shade();
            auto* face = new BuiltFace(
                BuiltPoints(points),
                BuiltTexcoords(texcoords),
                normals[i],
                faceMaterial,
                faceShade);
            face->Damaging(brush->Damaging());
            face->Terrain(terrain);

            if (map == nullptr)
            {
                NullReference();
            }
            map->Faces().push_back(face);
            if (brush->Solid())
            {
                map->Solid().push_back(face);
            }
        }
    }

    Vector2 MapBuilder::Project(
        Vector3 point, Vector3 normal, Vector3 origin, float texScale) noexcept
    {
        const float ax = ManagedAbs(normal.X);
        const float ay = ManagedAbs(normal.Y);
        const float az = ManagedAbs(normal.Z);
        if (ay > ax && ay >= az)
        {
            return Vector2(
                (point.X - origin.X) * texScale,
                (point.Z - origin.Z) * texScale);
        }
        if (ax >= az)
        {
            return Vector2(
                (point.Z - origin.Z) * texScale,
                (origin.Y - point.Y) * texScale);
        }
        return Vector2(
            (point.X - origin.X) * texScale,
            (origin.Y - point.Y) * texScale);
    }

    void MapBuilder::AddEntities(BuiltMap* map, MapDefinition* def)
    {
        if (map == nullptr)
        {
            NullReference();
        }
        std::int16_t id = TruncateToInt16(map->Entities().size());

        if (def == nullptr)
        {
            NullReference();
        }
        MapDefinition::SpawnList* spawns = def->Spawns();
        if (spawns == nullptr)
        {
            NullReference();
        }
        for (const std::shared_ptr<MapSpawn>& spawnValue : *spawns)
        {
            MapSpawn* spawn = Require(spawnValue);
            constexpr float DegreesToRadians = 0.017453292519943295769F;
            const float yaw = spawn->Yaw() * DegreesToRadians;

            auto* entity = new Editor::PlayerSpawnEntityEditor();
            entity->Id = PostIncrement(id);
            entity->LayerMask = 0xFFFFU;
            entity->Position = ToVector(spawn->Position());
            entity->Up = Vector3(0.0F, 1.0F, 0.0F);
            entity->Facing = Vector3(std::sin(yaw), 0.0F, std::cos(yaw)).Normalized();
            entity->NodeName = RmMainString();
            entity->Active = true;
            entity->Availability = 0;
            entity->TeamIndex = -1;
            map->Entities().push_back(entity);
        }

        MapDefinition::JumpPadList* jumpPads = def->JumpPads();
        if (jumpPads == nullptr)
        {
            NullReference();
        }
        for (const std::shared_ptr<MapJumpPad>& padValue : *jumpPads)
        {
            MapJumpPad* pad = Require(padValue);
            const auto [beam, speed] = SolveJumpPad(pad);

            auto* entity = new Editor::JumpPadEntityEditor();
            entity->Id = PostIncrement(id);
            entity->LayerMask = 0xFFFFU;
            entity->Position = ToVector(pad->Position());
            entity->Up = Vector3(0.0F, 1.0F, 0.0F);
            entity->Facing = Vector3(0.0F, 0.0F, 1.0F);
            entity->NodeName = RmMainString();
            entity->ParentId = -1;
            entity->Volume = MakeBox(pad->Size());
            entity->BeamVector = beam;
            entity->Speed = speed;
            entity->ControlLockTime = pad->ControlLockTime();
            entity->CooldownTime = pad->CooldownTime();
            entity->Active = true;
            entity->ModelId = pad->ModelId();
            entity->BeamType = 0;
            entity->TriggerFlags = Entities::TriggerFlags::PlayerBiped
                | Entities::TriggerFlags::PlayerAlt
                | Entities::TriggerFlags::IncludeBots;
            map->Entities().push_back(entity);
        }

        MapDefinition::ItemList* items = def->Items();
        if (items == nullptr)
        {
            NullReference();
        }
        for (const std::shared_ptr<MapItem>& itemValue : *items)
        {
            MapItem* item = Require(itemValue);
            const std::optional<std::string> typeText = MapItemTypeText(item);
            ItemType itemType{};
            if (!TryParseItemType(
                    typeText ? std::addressof(*typeText) : nullptr, itemType))
            {
                const std::optional<std::string> messageType = MapItemTypeText(item);
                throw MphRead::ProgramException(
                    "Unknown item type " + messageType.value_or(std::string()) + ".");
            }
            if (!MultiplayerItems.Contains(itemType))
            {
                const std::optional<std::string> messageType = MapItemTypeText(item);
                throw MphRead::ProgramException(
                    messageType.value_or(std::string())
                    + " does not belong in a multiplayer map. It is one of the story's permanent upgrades -- "
                    "an energy tank, a missile or UA expansion, an artifact -- which raise a hunter's capacity "
                    "for the rest of the game rather than topping it up for the rest of the match. Use one of: "
                    + JoinMultiplayerItems(MultiplayerItems) + ".");
            }

            auto* entity = new Editor::ItemSpawnEntityEditor();
            entity->Id = PostIncrement(id);
            entity->LayerMask = 0xFFFFU;
            entity->Position = ToVector(item->Position());
            entity->Up = Vector3(0.0F, 1.0F, 0.0F);
            entity->Facing = Vector3(0.0F, 0.0F, 1.0F);
            entity->NodeName = RmMainString();
            entity->ParentId = -1;
            entity->ItemType = itemType;
            entity->Enabled = true;
            entity->HasBase = item->HasBase();
            entity->AlwaysActive = true;
            entity->MaxSpawnCount = 0;
            entity->SpawnInterval = item->SpawnInterval();
            entity->SpawnDelay = 0;
            entity->NotifyEntityId = -1;
            entity->CollectedMessage = Message::None;
            map->Entities().push_back(entity);
        }

        if (map->Entities().empty())
        {
            throw MphRead::ProgramException("A map needs at least one entity.");
        }
    }

    std::pair<Vector3, float> MapBuilder::SolveJumpPad(MapJumpPad* pad)
    {
        if (pad == nullptr)
        {
            NullReference();
        }

        if (pad->Vector() != nullptr)
        {
            const Vector3 beam = ToVector(pad->Vector()).Normalized();
            const float speed = pad->Speed();
            return std::pair<Vector3, float>(beam, speed);
        }
        if (pad->Target() == nullptr)
        {
            throw MphRead::ProgramException(
                "A jump pad needs either a target or a vector and speed.");
        }

        const Vector3 from = ToVector(pad->Position());
        const Vector3 to = ToVector(pad->Target());
        const Vector3 delta(
            to.X - from.X,
            to.Y - from.Y,
            to.Z - from.Z);
        const float horizontal = Length(Vector3(delta.X, 0.0F, delta.Z));
        constexpr float Gravity = 77.0F / 4096.0F;
        const float rise = ManagedMax(delta.Y, 0.0F)
            + ManagedMax(2.0F, horizontal * 0.22F);
        const float up = std::sqrt(2.0F * Gravity * rise);
        const float fall = std::sqrt(
            2.0F * Gravity * ManagedMax(rise - delta.Y, 0.01F));
        const float frames = (up + fall) / Gravity;
        const Vector3 velocity(
            delta.X / frames,
            up,
            delta.Z / frames);
        const float speed = Length(velocity);
        return std::pair<Vector3, float>(Divide(velocity, speed), speed);
    }

    CollisionVolume MapBuilder::MakeBox(const std::vector<float>* size)
    {
        CollisionVolume volume;
        volume.Type = VolumeType::Box;
        volume.BoxVector1 = Vector3(1.0F, 0.0F, 0.0F);
        volume.BoxVector2 = Vector3(0.0F, 1.0F, 0.0F);
        volume.BoxVector3 = Vector3(0.0F, 0.0F, 1.0F);
        const float positionX = ArrayValue(size, 0);
        const float positionZ = ArrayValue(size, 2);
        volume.BoxPosition = Vector3(-positionX / 2.0F, 0.0F, -positionZ / 2.0F);
        volume.BoxDot1 = ArrayValue(size, 0);
        volume.BoxDot2 = ArrayValue(size, 1);
        volume.BoxDot3 = ArrayValue(size, 2);
        return volume;
    }

    Vector3 MapBuilder::ToVector(const std::vector<float>* values)
    {
        const float x = ArrayValue(values, 0);
        const float y = ArrayValue(values, 1);
        const float z = ArrayValue(values, 2);
        return Vector3(x, y, z);
    }
}

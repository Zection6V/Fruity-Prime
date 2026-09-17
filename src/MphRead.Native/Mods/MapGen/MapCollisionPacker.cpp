#include "MapCollisionPacker.hpp"

#include "../../Formats/Collision.hpp"
#include "../../Program.hpp"
#include "../../Utility/Repack.hpp"
#include "../../Utility/RepackCollision.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Utility::CollisionDataEditor;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    [[nodiscard]] constexpr std::int32_t WrapAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapMul(
        std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t ConvertToInt32Net9(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }
        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(std::trunc(wide));
    }

    [[nodiscard]] std::int32_t ListCount(std::size_t count)
    {
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("List count exceeds Int32.MaxValue.");
        }
        return static_cast<std::int32_t>(count);
    }

    [[nodiscard]] Vector3 ComponentMin(Vector3 a, Vector3 b) noexcept
    {
        a.X = a.X < b.X ? a.X : b.X;
        a.Y = a.Y < b.Y ? a.Y : b.Y;
        a.Z = a.Z < b.Z ? a.Z : b.Z;
        return a;
    }

    [[nodiscard]] Vector3 ComponentMax(Vector3 a, Vector3 b) noexcept
    {
        a.X = a.X > b.X ? a.X : b.X;
        a.Y = a.Y > b.Y ? a.Y : b.Y;
        a.Z = a.Z > b.Z ? a.Z : b.Z;
        return a;
    }

    struct Vector3Equal final
    {
        [[nodiscard]] bool operator()(Vector3 left, Vector3 right) const noexcept
        {
            return left.X == right.X
                && left.Y == right.Y
                && left.Z == right.Z;
        }
    };

    struct Vector4Equal final
    {
        [[nodiscard]] bool operator()(Vector4 left, Vector4 right) const noexcept
        {
            return left.X == right.X
                && left.Y == right.Y
                && left.Z == right.Z
                && left.W == right.W;
        }
    };

    [[nodiscard]] std::size_t HashFloat(float value) noexcept
    {
        return std::hash<float>{}(value);
    }

    [[nodiscard]] std::size_t CombineHash(std::size_t hash, std::size_t value) noexcept
    {
        return hash ^ (value + static_cast<std::size_t>(0x9e3779b9U)
            + (hash << 6U) + (hash >> 2U));
    }

    struct Vector3Hash final
    {
        [[nodiscard]] std::size_t operator()(Vector3 value) const noexcept
        {
            std::size_t hash = HashFloat(value.X);
            hash = CombineHash(hash, HashFloat(value.Y));
            return CombineHash(hash, HashFloat(value.Z));
        }
    };

    struct Vector4Hash final
    {
        [[nodiscard]] std::size_t operator()(Vector4 value) const noexcept
        {
            std::size_t hash = HashFloat(value.X);
            hash = CombineHash(hash, HashFloat(value.Y));
            hash = CombineHash(hash, HashFloat(value.Z));
            return CombineHash(hash, HashFloat(value.W));
        }
    };

    template <typename TKey, typename THash, typename TEqual>
    class ManagedDictionary final
    {
    public:
        [[nodiscard]] bool TryGetValue(const TKey& key, std::uint16_t& value) const
        {
            const auto bucket = _buckets.find(THash{}(key));
            if (bucket != _buckets.end())
            {
                for (const auto& entry : bucket->second)
                {
                    if (TEqual{}(entry.first, key))
                    {
                        value = entry.second;
                        return true;
                    }
                }
            }
            value = 0;
            return false;
        }

        void Add(const TKey& key, std::uint16_t value)
        {
            auto& bucket = _buckets[THash{}(key)];
            for (const auto& entry : bucket)
            {
                if (TEqual{}(entry.first, key))
                {
                    throw std::invalid_argument(
                        "An item with the same key has already been added.");
                }
            }
            bucket.emplace_back(key, value);
        }

    private:
        std::unordered_map<std::size_t,
            std::vector<std::pair<TKey, std::uint16_t>>> _buckets{};
    };

    struct Face final
    {
        std::uint16_t PlaneIndex = 0;
        std::shared_ptr<CollisionDataEditor> Editor;
        std::uint16_t Count = 0;
        std::uint16_t Start = 0;
    };

    struct Entry final
    {
        std::uint16_t Count = 0;
        std::uint16_t Start = 0;
    };

    [[nodiscard]] std::shared_ptr<CollisionDataEditor> RequireEditor(
        const std::shared_ptr<CollisionDataEditor>& editor)
    {
        if (!editor)
        {
            throw System::NullReferenceException();
        }
        return editor;
    }

    [[nodiscard]] std::string FormatInt(std::int32_t value)
    {
        return std::to_string(value);
    }
}

namespace MphRead::Mods::MapGen
{
    std::vector<std::uint8_t> MapCollisionPacker::Pack(
        const std::shared_ptr<const std::vector<
            std::shared_ptr<MphRead::Utility::CollisionDataEditor>>>& data)
    {
        using MphRead::Formats::Collision::CollisionHeader;
        using MphRead::Utility::BinaryWriter;

        if (!data)
        {
            throw System::NullReferenceException();
        }
        if (data->empty())
        {
            throw MphRead::ProgramException("A map needs at least one solid face.");
        }

        std::vector<Vector3> points;
        ManagedDictionary<Vector3, Vector3Hash, Vector3Equal> pointIds;
        std::vector<Vector4> planes;
        ManagedDictionary<Vector4, Vector4Hash, Vector4Equal> planeIds;
        std::vector<std::uint16_t> pointIndices;
        std::vector<Face> faces;
        Vector3 min(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        Vector3 max(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest());

        for (const std::shared_ptr<CollisionDataEditor>& editorValue : *data)
        {
            const std::shared_ptr<CollisionDataEditor> editor = RequireEditor(editorValue);
            const std::int32_t editorPointCount = ListCount(editor->Points->size());
            if (editorPointCount < 3 || editorPointCount > 10)
            {
                throw MphRead::ProgramException(
                    "A collision face has " + FormatInt(editorPointCount)
                    + " points; the format allows 3 to 10.");
            }

            std::uint16_t planeIndex = 0;
            if (!planeIds.TryGetValue(editor->Plane, planeIndex))
            {
                planeIndex = static_cast<std::uint16_t>(planes.size());
                planes.push_back(editor->Plane);
                planeIds.Add(editor->Plane, planeIndex);
            }

            const std::int32_t start = ListCount(pointIndices.size());
            for (const Vector3 point : *editor->Points)
            {
                std::uint16_t pointIndex = 0;
                if (!pointIds.TryGetValue(point, pointIndex))
                {
                    pointIndex = static_cast<std::uint16_t>(points.size());
                    if (points.size() > std::numeric_limits<std::uint16_t>::max())
                    {
                        throw MphRead::ProgramException(
                            "The map has more than 65535 distinct collision points, which the format cannot index. "
                            "Convert at a larger scale or with less geometry.");
                    }
                    points.push_back(point);
                    pointIds.Add(point, pointIndex);
                    min = ComponentMin(min, point);
                    max = ComponentMax(max, point);
                }
                pointIndices.push_back(pointIndex);
            }
            pointIndices.push_back(pointIndices.at(static_cast<std::size_t>(start)));
            faces.push_back(Face{
                planeIndex,
                editor,
                static_cast<std::uint16_t>(editor->Points->size()),
                static_cast<std::uint16_t>(start)
            });
        }

        const auto partCount = [](float maximum, float minimum)
        {
            const float quotient = (maximum - minimum) / CellSize;
            const std::int32_t floored = ConvertToInt32Net9(std::floor(quotient));
            return std::max<std::int32_t>(1, WrapAdd(floored, 1));
        };
        const std::int32_t partsX = partCount(max.X, min.X);
        const std::int32_t partsY = partCount(max.Y, min.Y);
        const std::int32_t partsZ = partCount(max.Z, min.Z);
        const std::int32_t cellCount = WrapMul(WrapMul(partsX, partsY), partsZ);
        if (cellCount < 0)
        {
            throw System::OverflowException();
        }
        std::vector<std::optional<std::vector<std::uint16_t>>> cells(
            static_cast<std::size_t>(cellCount));

        const std::int32_t dataCount = ListCount(data->size());
        for (std::int32_t i = 0; i < dataCount; i++)
        {
            const std::shared_ptr<CollisionDataEditor> editor = RequireEditor(
                data->at(static_cast<std::size_t>(i)));
            Vector3 faceMin(
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
            Vector3 faceMax(
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest());
            for (const Vector3 point : *editor->Points)
            {
                faceMin = ComponentMin(faceMin, point);
                faceMax = ComponentMax(faceMax, point);
            }

            const std::int32_t x0 = CellIndex(faceMin.X, min.X, partsX);
            const std::int32_t x1 = CellIndex(faceMax.X, min.X, partsX);
            const std::int32_t y0 = CellIndex(faceMin.Y, min.Y, partsY);
            const std::int32_t y1 = CellIndex(faceMax.Y, min.Y, partsY);
            const std::int32_t z0 = CellIndex(faceMin.Z, min.Z, partsZ);
            const std::int32_t z1 = CellIndex(faceMax.Z, min.Z, partsZ);
            for (std::int32_t y = y0; y <= y1; y = WrapAdd(y, 1))
            {
                for (std::int32_t z = z0; z <= z1; z = WrapAdd(z, 1))
                {
                    for (std::int32_t x = x0; x <= x1; x = WrapAdd(x, 1))
                    {
                        const std::int32_t index = WrapAdd(
                            WrapAdd(
                                WrapMul(WrapMul(y, partsX), partsZ),
                                WrapMul(z, partsX)),
                            x);
                        if (index < 0 || index >= cellCount)
                        {
                            throw std::out_of_range("Index was outside the bounds of the array.");
                        }
                        auto& cell = cells.at(static_cast<std::size_t>(index));
                        if (!cell)
                        {
                            cell.emplace();
                        }
                        cell->push_back(static_cast<std::uint16_t>(i));
                    }
                }
            }
        }

        std::int32_t references = 0;
        for (const std::optional<std::vector<std::uint16_t>>& cell : cells)
        {
            references = WrapAdd(
                references,
                cell ? ListCount(cell->size()) : 0);
        }

        std::vector<std::uint16_t> dataIndices;
        std::vector<Entry> entries;
        for (const std::optional<std::vector<std::uint16_t>>& cell : cells)
        {
            const std::int32_t start = ListCount(dataIndices.size());
            if (start > std::numeric_limits<std::uint16_t>::max())
            {
                throw MphRead::ProgramException(
                    "The collision grid needs " + FormatInt(references) + " face references "
                    "(" + FormatInt(partsX) + "x" + FormatInt(partsY) + "x"
                    + FormatInt(partsZ) + " cells over " + FormatInt(dataCount) + " faces), "
                    "and the format indexes them with 16 bits. "
                    "Convert at a larger scale, or with fewer solid surfaces.");
            }
            if (cell)
            {
                dataIndices.insert(dataIndices.end(), cell->begin(), cell->end());
            }
            entries.push_back(Entry{
                static_cast<std::uint16_t>(cell ? cell->size() : 0U),
                static_cast<std::uint16_t>(start)
            });
        }

        static_assert(sizeof(CollisionHeader) == 84);
        BinaryWriter writer;
        writer.Position(sizeof(CollisionHeader));

        const std::int32_t pointOffset = ListCount(writer.Position());
        for (const Vector3 point : points)
        {
            writer.WriteVector3(point);
        }
        const std::int32_t planeOffset = ListCount(writer.Position());
        for (const Vector4 plane : planes)
        {
            writer.WriteVector4(plane);
        }
        const std::int32_t pointIndexOffset = ListCount(writer.Position());
        for (const std::uint16_t index : pointIndices)
        {
            writer.Write(index);
        }
        Align(writer);

        const std::int32_t dataOffset = ListCount(writer.Position());
        for (const Face& face : faces)
        {
            writer.Write(static_cast<std::uint32_t>(0));
            writer.Write(face.PlaneIndex);
            writer.Write(static_cast<std::uint16_t>(face.Editor->Flags));
            writer.Write(face.Editor->LayerMask);
            writer.Write(static_cast<std::uint16_t>(0));
            writer.Write(face.Count);
            writer.Write(face.Start);
        }

        const std::int32_t dataIndexOffset = ListCount(writer.Position());
        for (const std::uint16_t index : dataIndices)
        {
            writer.Write(index);
        }
        Align(writer);

        const std::int32_t entryOffset = ListCount(writer.Position());
        for (const Entry& entry : entries)
        {
            writer.Write(entry.Count);
            writer.Write(entry.Start);
        }
        const std::int32_t portalOffset = ListCount(writer.Position());

        writer.Position(0);
        writer.WriteString("wc01", 4);
        writer.Write(ListCount(points.size()));
        writer.Write(pointOffset);
        writer.Write(ListCount(planes.size()));
        writer.Write(planeOffset);
        writer.Write(ListCount(pointIndices.size()));
        writer.Write(pointIndexOffset);
        writer.Write(ListCount(faces.size()));
        writer.Write(dataOffset);
        writer.Write(ListCount(dataIndices.size()));
        writer.Write(dataIndexOffset);
        writer.Write(partsX);
        writer.Write(partsY);
        writer.Write(partsZ);
        writer.WriteVector3(min);
        writer.Write(ListCount(entries.size()));
        writer.Write(entryOffset);
        writer.Write(static_cast<std::int32_t>(0));
        writer.Write(portalOffset);
        return writer.ToArray();
    }

    std::int32_t MapCollisionPacker::CellIndex(
        float value, float origin, std::int32_t parts) noexcept
    {
        const std::int32_t index = ConvertToInt32Net9((value - origin) / CellSize);
        return std::clamp(index, 0, parts - 1);
    }

    void MapCollisionPacker::Align(MphRead::Utility::BinaryWriter& writer)
    {
        while (writer.Position() % 4U != 0U)
        {
            writer.Write(static_cast<std::uint8_t>(0));
        }
    }
}

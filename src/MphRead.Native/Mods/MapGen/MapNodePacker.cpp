#include "MapNodePacker.hpp"

#include "../../Formats/NodeData.hpp"
#include "../../Formats/Types.hpp"
#include "BuiltMap.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace System
{
    class InvalidOperationException final : public std::runtime_error
    {
    public:
        InvalidOperationException()
            : std::runtime_error("Sequence contains no elements")
        {
        }
    };
}

namespace MphRead::Mods::MapGen
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        constexpr float FineSpacing = 2.0F;
        constexpr float Spacing = 6.0F;
        constexpr float Headroom = 1.9F;
        constexpr float StepUp = 1.2F;
        constexpr float StepDown = 8.0F;
        constexpr float WalkNormal = 0.7F;
        constexpr std::int32_t MaxNeighbours = 16;
        constexpr float Reach = 1.6F;
        constexpr std::int32_t MaxNodes = 700;

        struct Node final
        {
            Vector3 Position{};
            std::int32_t Cell = 0;
            std::vector<std::int32_t> Neighbours{};
        };

        struct BucketGrid final
        {
            std::vector<std::optional<std::vector<std::int32_t>>> Buckets{};
            std::int32_t Columns = 0;
            std::int32_t Rows = 0;
        };

        using Route = std::pair<std::uint16_t, std::uint16_t>;
        using RoutesForNode = std::vector<Route>;

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

        [[nodiscard]] Vector3 ComponentMin(Vector3 left, Vector3 right) noexcept
        {
            return Vector3(
                left.X < right.X ? left.X : right.X,
                left.Y < right.Y ? left.Y : right.Y,
                left.Z < right.Z ? left.Z : right.Z);
        }

        [[nodiscard]] Vector3 ComponentMax(Vector3 left, Vector3 right) noexcept
        {
            return Vector3(
                left.X > right.X ? left.X : right.X,
                left.Y > right.Y ? left.Y : right.Y,
                left.Z > right.Z ? left.Z : right.Z);
        }

        [[nodiscard]] Vector3 Add(Vector3 left, Vector3 right) noexcept
        {
            return Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
        }

        [[nodiscard]] Vector3 Subtract(Vector3 left, Vector3 right) noexcept
        {
            return Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
        }

        [[nodiscard]] Vector3 Multiply(Vector3 value, float scalar) noexcept
        {
            return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
        }

        [[nodiscard]] float Length(Vector3 value) noexcept
        {
            return std::sqrt((value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z));
        }

        [[nodiscard]] std::int32_t UncheckedInt32(float value) noexcept
        {
            if (std::isnan(value)
                || value < static_cast<float>(std::numeric_limits<std::int32_t>::min())
                || value >= 2147483648.0F)
            {
                return std::numeric_limits<std::int32_t>::min();
            }
            return static_cast<std::int32_t>(value);
        }

        [[nodiscard]] std::int32_t UncheckedAdd(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
        }

        [[nodiscard]] std::int32_t UncheckedMultiply(std::int32_t left, std::int32_t right) noexcept
        {
            return std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
        }

        [[nodiscard]] float RoundToEven(float value) noexcept
        {
            if (!std::isfinite(value))
            {
                return value;
            }
            const float floor = std::floor(value);
            const float fraction = value - floor;
            if (fraction < 0.5F)
            {
                return floor;
            }
            if (fraction > 0.5F)
            {
                return floor + 1.0F;
            }
            const float half = floor * 0.5F;
            return half == std::floor(half) ? floor : floor + 1.0F;
        }

        [[nodiscard]] bool ManagedFloatLess(float left, float right) noexcept
        {
            const bool leftNan = std::isnan(left);
            const bool rightNan = std::isnan(right);
            if (leftNan)
            {
                return !rightNan;
            }
            if (rightNan)
            {
                return false;
            }
            return left < right;
        }

        [[nodiscard]] const MphRead::ManagedArray<Vector3>* Points(const BuiltFace* face)
        {
            if (face == nullptr)
            {
                throw System::NullReferenceException();
            }
            const auto* points = face->Points();
            if (points == nullptr)
            {
                throw System::NullReferenceException();
            }
            return reinterpret_cast<const MphRead::ManagedArray<Vector3>*>(points);
        }

        [[nodiscard]] Vector3 Normal(const BuiltFace* face)
        {
            if (face == nullptr)
            {
                throw System::NullReferenceException();
            }
            return face->Normal();
        }

        [[nodiscard]] bool ContainsValue(
            const std::vector<std::int32_t>& values, std::int32_t value) noexcept
        {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        void Bounds(
            const std::vector<BuiltFace*>* solid,
            Vector3& min,
            Vector3& max)
        {
            if (solid == nullptr)
            {
                throw System::NullReferenceException();
            }
            min = Vector3(
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());
            max = Vector3(
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest());
            for (const BuiltFace* face : *solid)
            {
                const auto* points = Points(face);
                for (std::size_t i = 0; i < points->Length(); i++)
                {
                    const Vector3 point = (*points)[i];
                    min = ComponentMin(min, point);
                    max = ComponentMax(max, point);
                }
            }
        }

        [[nodiscard]] bool Contains(
            const BuiltFace* face,
            float x,
            float z,
            float& y)
        {
            y = 0.0F;
            const auto* points = Points(face);
            bool inside = false;
            if (points->Length() > 0)
            {
                std::size_t j = points->Length() - 1;
                for (std::size_t i = 0; i < points->Length(); j = i++)
                {
                    const Vector3 pi = (*points)[i];
                    const Vector3 pj = (*points)[j];
                    if ((pi.Z > z) != (pj.Z > z)
                        && x < ((pj.X - pi.X) * (z - pi.Z) / (pj.Z - pi.Z)) + pi.X)
                    {
                        inside = !inside;
                    }
                }
            }
            const Vector3 normal = Normal(face);
            if (!inside || std::fabs(normal.Y) < 0.0001F)
            {
                return false;
            }
            const float d = Vector3::Dot(normal, (*points)[0]);
            y = (d - (normal.X * x) - (normal.Z * z)) / normal.Y;
            return true;
        }

        [[nodiscard]] BucketGrid Buckets(
            const std::vector<BuiltFace*>* solid,
            Vector3 min,
            float spacing)
        {
            static_cast<void>(min);
            Vector3 low{};
            Vector3 high{};
            Bounds(solid, low, high);
            const std::int32_t columnsRaw = UncheckedAdd(
                UncheckedInt32(std::ceil((high.X - low.X) / spacing)), 1);
            const std::int32_t rowsRaw = UncheckedAdd(
                UncheckedInt32(std::ceil((high.Z - low.Z) / spacing)), 1);
            const std::int32_t columns = std::max<std::int32_t>(1, columnsRaw);
            const std::int32_t rows = std::max<std::int32_t>(1, rowsRaw);

            BucketGrid result{};
            result.Columns = columns;
            result.Rows = rows;
            const std::size_t bucketCount = static_cast<std::size_t>(columns)
                * static_cast<std::size_t>(rows);
            result.Buckets.resize(bucketCount);

            if (solid == nullptr)
            {
                throw System::NullReferenceException();
            }
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(solid->size()); i++)
            {
                const BuiltFace* face = (*solid)[static_cast<std::size_t>(i)];
                const auto* points = Points(face);
                if (points->Length() == 0)
                {
                    throw System::InvalidOperationException();
                }
                float minX = (*points)[0].X;
                float maxX = (*points)[0].X;
                float minZ = (*points)[0].Z;
                float maxZ = (*points)[0].Z;
                for (std::size_t p = 1; p < points->Length(); p++)
                {
                    const Vector3 point = (*points)[p];
                    minX = ManagedMin(minX, point.X);
                    maxX = ManagedMax(maxX, point.X);
                    minZ = ManagedMin(minZ, point.Z);
                    maxZ = ManagedMax(maxZ, point.Z);
                }
                const std::int32_t c0 = std::clamp(
                    UncheckedInt32(std::floor((minX - low.X) / spacing)), 0, columns - 1);
                const std::int32_t c1 = std::clamp(
                    UncheckedInt32(std::floor((maxX - low.X) / spacing)), 0, columns - 1);
                const std::int32_t r0 = std::clamp(
                    UncheckedInt32(std::floor((minZ - low.Z) / spacing)), 0, rows - 1);
                const std::int32_t r1 = std::clamp(
                    UncheckedInt32(std::floor((maxZ - low.Z) / spacing)), 0, rows - 1);
                for (std::int32_t r = r0; r <= r1; r++)
                {
                    for (std::int32_t c = c0; c <= c1; c++)
                    {
                        const std::size_t index = static_cast<std::size_t>(r)
                            * static_cast<std::size_t>(columns)
                            + static_cast<std::size_t>(c);
                        if (!result.Buckets[index].has_value())
                        {
                            result.Buckets[index].emplace();
                        }
                        result.Buckets[index]->push_back(i);
                    }
                }
            }
            return result;
        }

        [[nodiscard]] bool Blocked(
            const std::vector<BuiltFace*>* solid,
            const std::vector<std::int32_t>& bucket,
            float x,
            float z,
            float y)
        {
            if (solid == nullptr)
            {
                throw System::NullReferenceException();
            }
            for (const std::int32_t index : bucket)
            {
                const BuiltFace* face = (*solid)[static_cast<std::size_t>(index)];
                if (Normal(face).Y > -0.3F)
                {
                    continue;
                }
                float ceiling = 0.0F;
                if (Contains(face, x, z, ceiling)
                    && ceiling > y + 0.2F
                    && ceiling < y + Headroom)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::vector<Node> Sample(
            const std::vector<BuiltFace*>* solid,
            float spacing)
        {
            Vector3 min{};
            Vector3 max{};
            Bounds(solid, min, max);
            BucketGrid grid = Buckets(solid, min, spacing);
            std::vector<Node> nodes{};
            std::vector<float> heights{};
            for (std::int32_t row = 0; row < grid.Rows; row++)
            {
                for (std::int32_t column = 0; column < grid.Columns; column++)
                {
                    const float x = min.X + (static_cast<float>(column) + 0.5F) * spacing;
                    const float z = min.Z + (static_cast<float>(row) + 0.5F) * spacing;
                    heights.clear();
                    const std::size_t bucketIndex = static_cast<std::size_t>(row)
                        * static_cast<std::size_t>(grid.Columns)
                        + static_cast<std::size_t>(column);
                    const auto& bucketValue = grid.Buckets[bucketIndex];
                    if (!bucketValue.has_value())
                    {
                        continue;
                    }
                    const std::vector<std::int32_t>* bucket = &*bucketValue;
                    for (const std::int32_t index : *bucket)
                    {
                        const BuiltFace* face = (*solid)[static_cast<std::size_t>(index)];
                        float y = 0.0F;
                        if (Normal(face).Y < WalkNormal || !Contains(face, x, z, y))
                        {
                            continue;
                        }
                        heights.push_back(y);
                    }
                    std::sort(heights.begin(), heights.end(), ManagedFloatLess);
                    float last = std::numeric_limits<float>::lowest();
                    for (const float y : heights)
                    {
                        if (y - last < Headroom)
                        {
                            last = y;
                            continue;
                        }
                        last = y;
                        if (Blocked(solid, *bucket, x, z, y))
                        {
                            continue;
                        }
                        Node node{};
                        node.Position = Vector3(x, y + 0.5F, z);
                        node.Cell = UncheckedAdd(UncheckedMultiply(row, grid.Columns), column);
                        nodes.push_back(std::move(node));
                    }
                }
            }
            return nodes;
        }

        [[nodiscard]] std::vector<std::int32_t> Cells(
            Vector3 from,
            Vector3 to,
            Vector3 min,
            float spacing,
            std::int32_t columns,
            std::int32_t rows)
        {
            const Vector3 delta = Subtract(to, from);
            const std::int32_t steps = UncheckedAdd(1, UncheckedInt32(Length(delta) / spacing));
            std::vector<std::int32_t> result{};
            for (std::int32_t i = 0; i <= steps; i++)
            {
                const Vector3 point = Add(from, Multiply(delta,
                    static_cast<float>(i) / static_cast<float>(steps)));
                const std::int32_t column = UncheckedInt32(std::floor((point.X - min.X) / spacing));
                const std::int32_t row = UncheckedInt32(std::floor((point.Z - min.Z) / spacing));
                if (column >= 0 && row >= 0 && column < columns && row < rows)
                {
                    result.push_back(UncheckedAdd(UncheckedMultiply(row, columns), column));
                }
            }
            return result;
        }

        [[nodiscard]] bool Extent(
            const BuiltFace* face,
            Vector3 point,
            float low,
            float high)
        {
            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();
            const auto* points = Points(face);
            for (std::size_t i = 0; i < points->Length(); i++)
            {
                const Vector3 corner = (*points)[i];
                minX = ManagedMin(minX, corner.X);
                maxX = ManagedMax(maxX, corner.X);
                minY = ManagedMin(minY, corner.Y);
                maxY = ManagedMax(maxY, corner.Y);
                minZ = ManagedMin(minZ, corner.Z);
                maxZ = ManagedMax(maxZ, corner.Z);
            }
            return point.X >= minX - 0.05F && point.X <= maxX + 0.05F
                && point.Z >= minZ - 0.05F && point.Z <= maxZ + 0.05F
                && maxY > low && minY < high;
        }

        [[nodiscard]] bool Wall(
            const std::vector<BuiltFace*>* solid,
            const BucketGrid& grid,
            Vector3 min,
            float spacing,
            Vector3 from,
            Vector3 to)
        {
            const float low = ManagedMin(from.Y, to.Y) + 0.3F;
            const float high = ManagedMax(from.Y, to.Y) + Headroom;
            std::unordered_set<std::int32_t> seen{};
            for (const std::int32_t cell : Cells(from, to, min, spacing, grid.Columns, grid.Rows))
            {
                const auto& bucketValue = grid.Buckets[static_cast<std::size_t>(cell)];
                if (!bucketValue.has_value())
                {
                    continue;
                }
                const std::vector<std::int32_t>* bucket = &*bucketValue;
                for (const std::int32_t index : *bucket)
                {
                    if (!seen.insert(index).second)
                    {
                        continue;
                    }
                    const BuiltFace* face = (*solid)[static_cast<std::size_t>(index)];
                    const Vector3 normal = Normal(face);
                    if (std::fabs(normal.Y) > 0.5F)
                    {
                        continue;
                    }
                    const auto* points = Points(face);
                    const float d = Vector3::Dot(normal, (*points)[0]);
                    const float start = Vector3::Dot(normal, from) - d;
                    const float end = Vector3::Dot(normal, to) - d;
                    if (((start > 0.0F) == (end > 0.0F)) || std::fabs(start - end) < 0.0001F)
                    {
                        continue;
                    }
                    const Vector3 crossing = Add(from, Multiply(Subtract(to, from), start / (start - end)));
                    if (Extent(face, crossing, low, high))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        void Connect(
            const std::vector<BuiltFace*>* solid,
            std::vector<Node>& nodes,
            float spacing)
        {
            Vector3 min{};
            Vector3 max{};
            Bounds(solid, min, max);
            BucketGrid grid = Buckets(solid, min, spacing);
            std::unordered_map<std::int32_t, std::vector<std::int32_t>> byCell{};
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                byCell[nodes[static_cast<std::size_t>(i)].Cell].push_back(i);
            }
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                Node& node = nodes[static_cast<std::size_t>(i)];
                const std::int32_t column = node.Cell % grid.Columns;
                const std::int32_t row = node.Cell / grid.Columns;
                for (std::int32_t dr = -1; dr <= 1; dr++)
                {
                    for (std::int32_t dc = -1; dc <= 1; dc++)
                    {
                        if (dr == 0 && dc == 0)
                        {
                            continue;
                        }
                        const std::int32_t nc = column + dc;
                        const std::int32_t nr = row + dr;
                        if (nc < 0 || nr < 0 || nc >= grid.Columns || nr >= grid.Rows)
                        {
                            continue;
                        }
                        const std::int32_t key = UncheckedAdd(UncheckedMultiply(nr, grid.Columns), nc);
                        const auto found = byCell.find(key);
                        if (found == byCell.end())
                        {
                            continue;
                        }
                        for (const std::int32_t j : found->second)
                        {
                            if (j == i
                                || node.Neighbours.size() >= static_cast<std::size_t>(MaxNeighbours)
                                || ContainsValue(node.Neighbours, j))
                            {
                                continue;
                            }
                            const float rise = nodes[static_cast<std::size_t>(j)].Position.Y - node.Position.Y;
                            if (rise > StepUp || rise < -StepDown)
                            {
                                continue;
                            }
                            if (Wall(solid, grid, min, spacing,
                                node.Position, nodes[static_cast<std::size_t>(j)].Position))
                            {
                                continue;
                            }
                            node.Neighbours.push_back(j);
                            Node& other = nodes[static_cast<std::size_t>(j)];
                            if (other.Neighbours.size() < static_cast<std::size_t>(MaxNeighbours)
                                && !ContainsValue(other.Neighbours, i))
                            {
                                other.Neighbours.push_back(i);
                            }
                        }
                    }
                }
            }
        }

        [[nodiscard]] std::int32_t Largest(const std::vector<Node>& nodes)
        {
            std::vector<bool> seen(nodes.size(), false);
            std::int32_t best = 0;
            std::deque<std::int32_t> queue{};
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                if (seen[static_cast<std::size_t>(i)])
                {
                    continue;
                }
                std::int32_t size = 0;
                seen[static_cast<std::size_t>(i)] = true;
                queue.clear();
                queue.push_back(i);
                while (!queue.empty())
                {
                    const std::int32_t current = queue.front();
                    queue.pop_front();
                    size++;
                    for (const std::int32_t neighbour : nodes[static_cast<std::size_t>(current)].Neighbours)
                    {
                        if (!seen[static_cast<std::size_t>(neighbour)])
                        {
                            seen[static_cast<std::size_t>(neighbour)] = true;
                            queue.push_back(neighbour);
                        }
                    }
                }
                best = std::max(best, size);
            }
            return best;
        }

        [[nodiscard]] std::vector<Node> Decimate(
            const std::vector<Node>& fine,
            std::int32_t radius)
        {
            radius = std::max<std::int32_t>(1, radius);
            std::vector<std::int32_t> owner(fine.size(), -1);
            std::vector<std::int32_t> seeds{};
            std::deque<std::pair<std::int32_t, std::int32_t>> queue{};
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(fine.size()); i++)
            {
                if (owner[static_cast<std::size_t>(i)] != -1)
                {
                    continue;
                }
                const std::int32_t index = static_cast<std::int32_t>(seeds.size());
                seeds.push_back(i);
                owner[static_cast<std::size_t>(i)] = index;
                queue.clear();
                queue.emplace_back(i, 0);
                while (!queue.empty())
                {
                    const auto [current, depth] = queue.front();
                    queue.pop_front();
                    if (depth == radius)
                    {
                        continue;
                    }
                    for (const std::int32_t neighbour : fine[static_cast<std::size_t>(current)].Neighbours)
                    {
                        if (owner[static_cast<std::size_t>(neighbour)] == -1)
                        {
                            owner[static_cast<std::size_t>(neighbour)] = index;
                            queue.emplace_back(neighbour, depth + 1);
                        }
                    }
                }
            }
            std::vector<Node> nodes{};
            nodes.reserve(seeds.size());
            for (const std::int32_t seed : seeds)
            {
                Node node{};
                node.Position = fine[static_cast<std::size_t>(seed)].Position;
                node.Cell = fine[static_cast<std::size_t>(seed)].Cell;
                nodes.push_back(std::move(node));
            }
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(fine.size()); i++)
            {
                for (const std::int32_t j : fine[static_cast<std::size_t>(i)].Neighbours)
                {
                    const std::int32_t a = owner[static_cast<std::size_t>(i)];
                    const std::int32_t b = owner[static_cast<std::size_t>(j)];
                    if (a == b || a < 0 || b < 0)
                    {
                        continue;
                    }
                    Node& nodeA = nodes[static_cast<std::size_t>(a)];
                    if (nodeA.Neighbours.size() < static_cast<std::size_t>(MaxNeighbours)
                        && !ContainsValue(nodeA.Neighbours, b))
                    {
                        nodeA.Neighbours.push_back(b);
                    }
                    Node& nodeB = nodes[static_cast<std::size_t>(b)];
                    if (nodeB.Neighbours.size() < static_cast<std::size_t>(MaxNeighbours)
                        && !ContainsValue(nodeB.Neighbours, a))
                    {
                        nodeB.Neighbours.push_back(a);
                    }
                }
            }
            return nodes;
        }

        [[nodiscard]] std::vector<RoutesForNode> Routes(const std::vector<Node>& nodes)
        {
            const std::int32_t count = static_cast<std::int32_t>(nodes.size());
            std::vector<std::uint16_t> hop(
                static_cast<std::size_t>(count) * static_cast<std::size_t>(count), 0);
            std::vector<std::int32_t> distance(nodes.size(), 0);
            std::deque<std::int32_t> queue{};
            for (std::int32_t destination = 0; destination < count; destination++)
            {
                std::fill(distance.begin(), distance.end(), -1);
                distance[static_cast<std::size_t>(destination)] = 0;
                queue.clear();
                queue.push_back(destination);
                while (!queue.empty())
                {
                    const std::int32_t current = queue.front();
                    queue.pop_front();
                    for (const std::int32_t neighbour : nodes[static_cast<std::size_t>(current)].Neighbours)
                    {
                        if (distance[static_cast<std::size_t>(neighbour)] < 0)
                        {
                            distance[static_cast<std::size_t>(neighbour)]
                                = distance[static_cast<std::size_t>(current)] + 1;
                            queue.push_back(neighbour);
                        }
                    }
                }
                for (std::int32_t source = 0; source < count; source++)
                {
                    std::uint16_t answer = static_cast<std::uint16_t>(source);
                    if (source != destination && distance[static_cast<std::size_t>(source)] > 0)
                    {
                        for (const std::int32_t neighbour : nodes[static_cast<std::size_t>(source)].Neighbours)
                        {
                            if (distance[static_cast<std::size_t>(neighbour)]
                                == distance[static_cast<std::size_t>(source)] - 1)
                            {
                                answer = static_cast<std::uint16_t>(neighbour);
                                break;
                            }
                        }
                    }
                    hop[static_cast<std::size_t>(source) * static_cast<std::size_t>(count)
                        + static_cast<std::size_t>(destination)] = answer;
                }
            }
            std::vector<RoutesForNode> results(nodes.size());
            for (std::int32_t source = 0; source < count; source++)
            {
                RoutesForNode runs{};
                std::int32_t start = 0;
                while (start < count)
                {
                    const std::uint16_t value = hop[
                        static_cast<std::size_t>(source) * static_cast<std::size_t>(count)
                        + static_cast<std::size_t>(start)];
                    std::int32_t end = start + 1;
                    while (end < count
                        && hop[static_cast<std::size_t>(source) * static_cast<std::size_t>(count)
                            + static_cast<std::size_t>(end)] == value
                        && end - start < std::numeric_limits<std::uint16_t>::max())
                    {
                        end++;
                    }
                    runs.emplace_back(static_cast<std::uint16_t>(end - start), value);
                    start = end;
                }
                results[static_cast<std::size_t>(source)] = std::move(runs);
            }
            return results;
        }

        void WriteU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
            bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
        }

        void WriteU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
            bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
            bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
            bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
        }

        void WriteI32(std::vector<std::uint8_t>& bytes, std::int32_t value)
        {
            WriteU32(bytes, std::bit_cast<std::uint32_t>(value));
        }

        [[nodiscard]] std::vector<std::uint8_t> Write(
            const std::vector<Node>& nodes,
            float spacing)
        {
            static_cast<void>(spacing);
            const std::vector<RoutesForNode> routes = Routes(nodes);
            constexpr std::int32_t indexOffset = 14;
            constexpr std::int32_t dataOffset = 16;
            constexpr std::int32_t listOffset = 24;
            constexpr std::int32_t nodeOffset = 32;
            const std::int32_t valuesOffset = UncheckedAdd(
                nodeOffset,
                UncheckedMultiply(static_cast<std::int32_t>(nodes.size()), 36));
            std::vector<std::int32_t> routeOffset(nodes.size(), 0);
            std::int32_t cursor = valuesOffset;
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                routeOffset[static_cast<std::size_t>(i)] = cursor;
                cursor = UncheckedAdd(cursor,
                    UncheckedMultiply(static_cast<std::int32_t>(routes[static_cast<std::size_t>(i)].size()), 4));
            }
            std::vector<std::uint8_t> bytes{};
            if (cursor > 0)
            {
                bytes.reserve(static_cast<std::size_t>(cursor));
            }
            WriteU16(bytes, 6);
            WriteU16(bytes, 1);
            WriteU32(bytes, static_cast<std::uint32_t>(indexOffset));
            WriteU32(bytes, static_cast<std::uint32_t>(dataOffset));
            WriteU16(bytes, 0);
            WriteU16(bytes, 0);
            WriteU32(bytes, static_cast<std::uint32_t>(listOffset));
            WriteU16(bytes, 1);
            WriteU16(bytes, 0x5C);
            WriteU32(bytes, static_cast<std::uint32_t>(nodeOffset));
            WriteU16(bytes, static_cast<std::uint16_t>(nodes.size()));
            WriteU16(bytes, 0x5C);
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                const Node& node = nodes[static_cast<std::size_t>(i)];
                WriteU16(bytes, static_cast<std::uint16_t>(MphRead::Formats::NodeType::Navigation));
                WriteU16(bytes, static_cast<std::uint16_t>(i));
                WriteU16(bytes, 0);
                WriteU16(bytes, 0);
                WriteI32(bytes, MphRead::Fixed::ToInt(node.Position.X));
                WriteI32(bytes, MphRead::Fixed::ToInt(node.Position.Y));
                WriteI32(bytes, MphRead::Fixed::ToInt(node.Position.Z));
                WriteI32(bytes, MphRead::Fixed::ToInt(Reach));
                WriteU32(bytes, static_cast<std::uint32_t>(routeOffset[static_cast<std::size_t>(i)]));
                WriteU32(bytes, static_cast<std::uint32_t>(routeOffset[static_cast<std::size_t>(i)]));
                WriteU32(bytes, 0);
            }
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(nodes.size()); i++)
            {
                for (const auto& [run, hop] : routes[static_cast<std::size_t>(i)])
                {
                    WriteU16(bytes, run);
                    WriteU16(bytes, hop);
                }
            }
            return bytes;
        }

        [[nodiscard]] std::int32_t DegreeSum(const std::vector<Node>& nodes)
        {
            std::int32_t total = 0;
            for (const Node& node : nodes)
            {
                const std::int32_t value = static_cast<std::int32_t>(node.Neighbours.size());
                if (value > 0 && total > std::numeric_limits<std::int32_t>::max() - value)
                {
                    throw System::OverflowException();
                }
                total += value;
            }
            return total;
        }

        [[nodiscard]] std::int32_t MaxDegree(const std::vector<Node>& nodes)
        {
            if (nodes.empty())
            {
                throw System::InvalidOperationException();
            }
            std::int32_t result = static_cast<std::int32_t>(nodes.front().Neighbours.size());
            for (std::size_t i = 1; i < nodes.size(); i++)
            {
                result = std::max(result, static_cast<std::int32_t>(nodes[i].Neighbours.size()));
            }
            return result;
        }

        [[nodiscard]] std::int32_t IsolatedCount(const std::vector<Node>& nodes) noexcept
        {
            std::int32_t count = 0;
            for (const Node& node : nodes)
            {
                if (node.Neighbours.empty())
                {
                    count++;
                }
            }
            return count;
        }
    }

    std::tuple<std::vector<std::uint8_t>, std::int32_t, std::int32_t> MapNodePacker::Pack(
        const std::vector<BuiltFace*>* solid)
    {
        std::vector<Node> fine = Sample(solid, FineSpacing);
        Connect(solid, fine, FineSpacing);
        if (std::getenv("FP_NODEDEBUG") != nullptr)
        {
            const std::int32_t edges = DegreeSum(fine) / 2;
            std::printf("  [nodes] fine %d nodes, %d edges, largest component %d\n",
                static_cast<std::int32_t>(fine.size()), edges, Largest(fine));
        }
        float spacing = Spacing;
        std::vector<Node> nodes{};
        while (true)
        {
            nodes = Decimate(fine, UncheckedInt32(RoundToEven(spacing / FineSpacing)));
            if (nodes.size() <= static_cast<std::size_t>(MaxNodes) || spacing > 64.0F)
            {
                break;
            }
            spacing *= 1.4F;
        }
        const std::int32_t edges = DegreeSum(nodes) / 2;
        if (std::getenv("FP_NODEDEBUG") != nullptr)
        {
            std::printf(
                "  [nodes] coarse %d nodes, %d edges, largest component %d, degree max %d, isolated %d\n",
                static_cast<std::int32_t>(nodes.size()),
                edges,
                Largest(nodes),
                MaxDegree(nodes),
                IsolatedCount(nodes));
        }
        return std::make_tuple(
            Write(nodes, spacing),
            static_cast<std::int32_t>(nodes.size()),
            edges);
    }
}

#include "Collision.hpp"

#include "../Metadata/Metadata.hpp"

// Exact prerequisite counterparts. These are deliberately not replaced by
// Collision-specific compatibility code.
// #include "../Paths.hpp"
// #include "../Read.hpp"
// #include "../Scene.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead::Formats::Collision
{
    namespace
    {
        template <typename T>
        T& ReassignReadonlyValue(T* self, const T& other) noexcept
        {
            if (self != &other)
            {
                std::destroy_at(self);
                std::construct_at(self, other);
            }
            return *self;
        }

        template <std::size_t N>
        std::string MarshalString(const std::array<char, N>& value)
        {
            std::size_t length = 0;
            while (length < N && value[length] != '\0')
            {
                ++length;
            }
            return std::string(value.data(), length);
        }

        template <typename T>
        const T& At(
            const std::shared_ptr<const std::vector<T>>& values,
            std::size_t index)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            return values->at(index);
        }

        template <typename T>
        T& At(
            const std::shared_ptr<std::vector<T>>& values,
            std::size_t index)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            return values->at(index);
        }

        template <typename T>
        std::size_t Count(
            const std::shared_ptr<const std::vector<T>>& values)
        {
            if (!values)
            {
                throw System::NullReferenceException();
            }
            return values->size();
        }

        bool HasFlag(CollisionFlags value, CollisionFlags flag) noexcept
        {
            return (
                static_cast<std::uint16_t>(value)
                & static_cast<std::uint16_t>(flag)
            ) != 0;
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>>
        ConvertPoints(
            const std::shared_ptr<const std::vector<MphRead::Vector3Fx>>& values)
        {
            if (!values)
            {
                throw System::ArgumentNullException("source");
            }

            auto result
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();
            result->reserve(values->size());

            for (const MphRead::Vector3Fx& value : *values)
            {
                result->push_back(value.ToFloatVector());
            }

            return result;
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector4>>
        ConvertPlanes(
            const std::shared_ptr<const std::vector<MphRead::Vector4Fx>>& values)
        {
            if (!values)
            {
                throw System::ArgumentNullException("source");
            }

            auto result
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector4>>();
            result->reserve(values->size());

            for (const MphRead::Vector4Fx& value : *values)
            {
                result->push_back(value.ToFloatVector());
            }

            return result;
        }

        OpenTK::Mathematics::Vector3 Average(
            const std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>>& points)
        {
            if (!points)
            {
                throw System::ArgumentNullException("source");
            }

            float x = 0.0F;
            float y = 0.0F;
            float z = 0.0F;

            for (const OpenTK::Mathematics::Vector3& point : *points)
            {
                x += point.X;
                y += point.Y;
                z += point.Z;
            }

            const float count = static_cast<float>(points->size());

            return OpenTK::Mathematics::Vector3(
                x / count,
                y / count,
                z / count);
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>>
        MakeMphPortalPoints(const RawCollisionPortal& raw)
        {
            auto points
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();

            points->reserve(4);
            points->push_back(raw.Point1.ToFloatVector());
            points->push_back(raw.Point2.ToFloatVector());
            points->push_back(raw.Point3.ToFloatVector());
            points->push_back(raw.Point4.ToFloatVector());

            return points;
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector4>>
        MakeMphPortalPlanes(const RawCollisionPortal& raw)
        {
            auto planes
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector4>>();

            planes->reserve(4);
            planes->push_back(raw.Plane1.ToFloatVector());
            planes->push_back(raw.Plane2.ToFloatVector());
            planes->push_back(raw.Plane3.ToFloatVector());
            planes->push_back(raw.Plane4.ToFloatVector());

            return planes;
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>>
        MakeFhPortalPoints(
            const FhCollisionPortal& raw,
            const std::shared_ptr<const std::vector<FhCollisionVector>>& rawVectors,
            const std::shared_ptr<const std::vector<MphRead::Vector3Fx>>& rawPoints)
        {
            auto points
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>();
            points->reserve(raw.VectorCount);

            for (std::int32_t i = 0; i < raw.VectorCount; ++i)
            {
                const FhCollisionVector& vector = At(
                    rawVectors,
                    static_cast<std::size_t>(raw.VectorStartIndex) + i);

                points->push_back(
                    At(rawPoints, vector.Point2Index).ToFloatVector());
            }

            return points;
        }

        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector4>>
        MakeFhPortalPlanes(
            const FhCollisionPortal& raw,
            const std::shared_ptr<const std::vector<FhCollisionVector>>& rawVectors,
            const std::shared_ptr<const std::vector<MphRead::Vector4Fx>>& rawPlanes)
        {
            auto planes
                = std::make_shared<std::vector<OpenTK::Mathematics::Vector4>>();
            planes->reserve(raw.VectorCount);

            for (std::int32_t i = 0; i < raw.VectorCount; ++i)
            {
                const FhCollisionVector& vector = At(
                    rawVectors,
                    static_cast<std::size_t>(raw.VectorStartIndex) + i);

                planes->push_back(
                    At(rawPlanes, vector.PlaneIndex).ToFloatVector());
            }

            return planes;
        }

        std::int32_t WrapMultiply(
            std::int32_t left,
            std::int32_t right) noexcept
        {
            const std::uint32_t value
                = static_cast<std::uint32_t>(left)
                * static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        std::int32_t WrapAdd(
            std::int32_t left,
            std::int32_t right) noexcept
        {
            const std::uint32_t value
                = static_cast<std::uint32_t>(left)
                + static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        std::int32_t WrapSubtract(
            std::int32_t left,
            std::int32_t right) noexcept
        {
            const std::uint32_t value
                = static_cast<std::uint32_t>(left)
                - static_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(value);
        }

        std::int32_t ManagedDivide(
            std::int32_t numerator,
            std::int32_t denominator)
        {
            if (denominator == 0)
            {
                // The common Native runtime must translate this to the
                // managed DivideByZeroException category.
                throw std::domain_error("Attempted to divide by zero.");
            }

            if (numerator == std::numeric_limits<std::int32_t>::min()
                && denominator == -1)
            {
                throw System::OverflowException();
            }

            return numerator / denominator;
        }

        MphRead::Scene& RequireScene(MphRead::Scene* scene)
        {
            if (scene == nullptr)
            {
                throw System::NullReferenceException();
            }
            return *scene;
        }

        struct CollisionCaches
        {
            std::unordered_map<std::string, std::shared_ptr<CollisionInfo>> Mph;
            std::unordered_map<std::string, std::shared_ptr<CollisionInfo>> Fh;
        };

        CollisionCaches& Caches()
        {
            static CollisionCaches caches;
            return caches;
        }
    }

    CollisionHeader& CollisionHeader::operator=(
        const CollisionHeader& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    CollisionData& CollisionData::operator=(
        const CollisionData& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    CollisionEntry& CollisionEntry::operator=(
        const CollisionEntry& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    RawCollisionPortal& RawCollisionPortal::operator=(
        const RawCollisionPortal& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionPortal& FhCollisionPortal::operator=(
        const FhCollisionPortal& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionHeader& FhCollisionHeader::operator=(
        const FhCollisionHeader& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionData& FhCollisionData::operator=(
        const FhCollisionData& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionVector& FhCollisionVector::operator=(
        const FhCollisionVector& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionEntry& FhCollisionEntry::operator=(
        const FhCollisionEntry& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    FhCollisionTreeNode& FhCollisionTreeNode::operator=(
        const FhCollisionTreeNode& other) noexcept
    {
        return ReassignReadonlyValue(this, other);
    }

    EntityCollision::EntityCollision(
        std::shared_ptr<CollisionInstance> collision,
        std::shared_ptr<MphRead::EntityBase> entity)
        : Entity(std::move(entity)),
          Collision(std::move(collision)),
          DrawPoints(
              std::make_shared<std::vector<OpenTK::Mathematics::Vector3>>())
    {
    }

    std::int32_t CollisionData::Slipperiness() const noexcept
    {
        return (
            static_cast<std::uint16_t>(Flags) & 0x0018U
        ) >> 3;
    }

    MphRead::Terrain CollisionData::Terrain() const noexcept
    {
        return static_cast<MphRead::Terrain>(
            (
                static_cast<std::uint16_t>(Flags)
                & 0x01E0U
            ) >> 5);
    }

    bool CollisionData::IgnorePlayers() const noexcept
    {
        return HasFlag(Flags, CollisionFlags::IgnorePlayers);
    }

    bool CollisionData::IgnoreBeams() const noexcept
    {
        return HasFlag(Flags, CollisionFlags::IgnoreBeams);
    }

    std::int32_t CollisionData::Axis() const noexcept
    {
        return LayerMask & 3;
    }

    Portal::Portal(RawCollisionPortal raw)
        : Name(MarshalString(raw.Name)),
          NodeName1(MarshalString(raw.NodeName1)),
          NodeName2(MarshalString(raw.NodeName2)),
          LayerMask(raw.LayerMask),
          IsForceField(Name.starts_with("pmag")),
          Points(MakeMphPortalPoints(raw)),
          Planes(MakeMphPortalPlanes(raw)),
          Plane(raw.Plane.ToFloatVector()),
          Position(Average(Points)),
          Flags(raw.Flags),
          Unknown00(raw.UnusedDE),
          Unknown01(raw.UnusedDF)
    {
        assert(raw.PointCount == 4);
    }

    Portal::Portal(
        FhCollisionPortal raw,
        std::shared_ptr<const std::vector<FhCollisionVector>> rawVectors,
        std::shared_ptr<const std::vector<MphRead::Vector3Fx>> rawPoints,
        std::shared_ptr<const std::vector<MphRead::Vector4Fx>> rawPlanes)
        : Name(MarshalString(raw.Name)),
          NodeName1(MarshalString(raw.NodeName1)),
          NodeName2(MarshalString(raw.NodeName2)),
          LayerMask(4),
          IsForceField(false),
          Points(MakeFhPortalPoints(raw, rawVectors, rawPoints)),
          Planes(MakeFhPortalPlanes(raw, rawVectors, rawPlanes)),
          Plane(raw.Plane.ToFloatVector()),
          Position(Average(Points)),
          Flags(0),
          Unknown00(raw.Field5C),
          Unknown01(raw.Field5D)
    {
    }

    Portal::Portal(
        std::string nodeName1,
        std::string nodeName2,
        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>> points,
        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector4>> planes,
        OpenTK::Mathematics::Vector4 plane)
        : Name(
              "port_"
              + nodeName1
              + "_"
              + nodeName2),
          NodeName1(std::move(nodeName1)),
          NodeName2(std::move(nodeName2)),
          LayerMask(4),
          IsForceField(false),
          Points(std::move(points)),
          Planes(std::move(planes)),
          Plane(plane),
          Position(Average(Points)),
          Flags(1),
          Unknown00(0),
          Unknown01(0)
    {
    }

    CollisionInstance::CollisionInstance(
        std::string name,
        std::shared_ptr<CollisionInfo> info,
        bool isEntity)
        : Name(std::move(name)),
          Info(std::move(info)),
          IsEntity(isEntity)
    {
    }

    CollisionInfo::CollisionInfo(
        std::shared_ptr<const std::vector<MphRead::Vector3Fx>> points,
        std::shared_ptr<const std::vector<MphRead::Vector4Fx>> planes,
        std::shared_ptr<const std::vector<std::shared_ptr<Portal>>> portals,
        bool firstHunt)
        : FirstHunt(firstHunt),
          Points(ConvertPoints(points)),
          Planes(ConvertPlanes(planes)),
          Portals(std::move(portals))
    {
    }

    MphCollisionInfo::MphCollisionInfo(
        CollisionHeader header,
        std::shared_ptr<const std::vector<MphRead::Vector3Fx>> points,
        std::shared_ptr<const std::vector<MphRead::Vector4Fx>> planes,
        std::shared_ptr<const std::vector<std::uint16_t>> ptIdxs,
        std::shared_ptr<const std::vector<CollisionData>> data,
        std::shared_ptr<const std::vector<std::uint16_t>> dataIdxs,
        std::shared_ptr<const std::vector<CollisionEntry>> entries,
        std::shared_ptr<const std::vector<std::shared_ptr<Portal>>> portals)
        : CollisionInfo(
              std::move(points),
              std::move(planes),
              std::move(portals),
              false),
          Header(header),
          PointIndices(std::move(ptIdxs)),
          Data(std::move(data)),
          DataIndices(std::move(dataIdxs)),
          Entries(std::move(entries)),
          MinPosition(header.MinPosition.ToFloatVector())
    {
    }

    const std::array<OpenTK::Mathematics::Vector4, 12>&
    MphCollisionInfo::Colors()
    {
        static const std::array<OpenTK::Mathematics::Vector4, 12> colors
        {
            OpenTK::Mathematics::Vector4(0.69F, 0.69F, 0.69F, 1.0F),
            OpenTK::Mathematics::Vector4(1.0F, 0.612F, 0.153F, 1.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 1.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.858F, 1.0F),
            OpenTK::Mathematics::Vector4(0.141F, 1.0F, 1.0F, 1.0F),
            OpenTK::Mathematics::Vector4(1.0F, 1.0F, 1.0F, 1.0F),
            OpenTK::Mathematics::Vector4(0.964F, 1.0F, 0.058F, 1.0F),
            OpenTK::Mathematics::Vector4(0.505F, 0.364F, 0.211F, 1.0F),
            OpenTK::Mathematics::Vector4(0.984F, 0.701F, 0.576F, 1.0F),
            OpenTK::Mathematics::Vector4(0.988F, 0.463F, 0.824F, 1.0F),
            OpenTK::Mathematics::Vector4(0.615F, 0.0F, 0.909F, 1.0F),
            OpenTK::Mathematics::Vector4(0.85F, 0.85F, 0.85F, 1.0F)
        };

        return colors;
    }

    void MphCollisionInfo::GetDrawInfo(
        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>> points,
        OpenTK::Mathematics::Vector3 translation,
        MphRead::EntityType entityType,
        MphRead::Scene* scenePtr)
    {
        MphRead::Scene& scene = RequireScene(scenePtr);

        const std::int32_t polygonId = scene.GetNextPolygonId();

        if (!Data)
        {
            throw System::NullReferenceException();
        }

        for (std::size_t i = 0; i < Data->size(); ++i)
        {
            const CollisionData& data = (*Data)[i];

            if (scene.ColTerDisplay != MphRead::Terrain::All
                && scene.ColTerDisplay != data.Terrain())
            {
                continue;
            }

            if ((scene.ColTypeDisplay == CollisionType::Player
                    && data.IgnorePlayers())
                || (scene.ColTypeDisplay == CollisionType::Beam
                    && data.IgnoreBeams())
                || (scene.ColTypeDisplay == CollisionType::Both
                    && (data.IgnorePlayers() || data.IgnoreBeams())))
            {
                continue;
            }

            OpenTK::Mathematics::Vector4 color{};

            if (scene.ColDisplayColor == CollisionColor::Entity)
            {
                if (entityType == MphRead::EntityType::Platform)
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.109F, 0.768F, 0.850F, 1.0F);
                }
                else if (entityType == MphRead::EntityType::Object)
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.952F, 0.105F, 0.635F, 1.0F);
                }
                else
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.952F, 0.694F, 0.105F, 1.0F);
                }
            }
            else if (scene.ColDisplayColor == CollisionColor::Terrain)
            {
                color = Colors().at(
                    static_cast<std::size_t>(data.Terrain()));
            }
            else if (scene.ColDisplayColor == CollisionColor::Type)
            {
                if (data.IgnoreBeams())
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.956F, 0.933F, 0.203F, 1.0F);
                }
                else if (data.IgnorePlayers())
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.250F, 0.807F, 0.250F, 1.0F);
                }
                else
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.807F, 0.250F, 0.776F, 1.0F);
                }
            }
            else
            {
                color = OpenTK::Mathematics::Vector4(
                    1.0F, 0.0F, 0.0F, 1.0F);
            }

            color.W = scene.ColDisplayAlpha;

            assert(
                data.PointIndexCount >= 3
                && data.PointIndexCount <= 10);

            auto verts = std::make_shared<
                MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(
                    data.PointIndexCount);

            for (std::int32_t j = 0;
                j < data.PointIndexCount;
                ++j)
            {
                const std::uint16_t pointIndex = At(
                    PointIndices,
                    static_cast<std::size_t>(data.PointStartIndex) + j);

                (*verts)[j]
                    = At(points, pointIndex) + translation;
            }

            scene.AddRenderItem(
                MphRead::CullingMode::Back,
                polygonId,
                color,
                MphRead::RenderItemType::Ngon,
                verts,
                data.PointIndexCount);
        }
    }

    OpenTK::Mathematics::Vector3i
    MphCollisionInfo::PartIndexFromEntry(
        std::int32_t index) const
    {
        const std::int32_t x = Header.PartsX;
        const std::int32_t xz
            = WrapMultiply(x, Header.PartsZ);

        const std::int32_t yInc
            = ManagedDivide(index, xz);

        const std::int32_t afterY
            = WrapSubtract(
                index,
                WrapMultiply(yInc, xz));

        const std::int32_t zInc
            = ManagedDivide(afterY, x);

        const std::int32_t xInc
            = WrapSubtract(
                afterY,
                WrapMultiply(zInc, x));

        return OpenTK::Mathematics::Vector3i(
            xInc,
            yInc,
            zInc);
    }

    std::int32_t MphCollisionInfo::EntryIndexFromPoint(
        OpenTK::Mathematics::Vector3 point) const
    {
        if (point.X < MinPosition.X
            || point.Y < MinPosition.Y
            || point.Z < MinPosition.Z)
        {
            return -1;
        }

        const std::int32_t xInc
            = static_cast<std::int32_t>(
                (point.X - MinPosition.X) / 4.0F);

        const std::int32_t yInc
            = static_cast<std::int32_t>(
                (point.Y - MinPosition.Y) / 4.0F);

        const std::int32_t zInc
            = static_cast<std::int32_t>(
                (point.Z - MinPosition.Z) / 4.0F);

        if (xInc >= Header.PartsX
            || yInc >= Header.PartsY
            || zInc >= Header.PartsZ)
        {
            return -1;
        }

        return WrapAdd(
            WrapAdd(
                WrapMultiply(
                    yInc,
                    WrapMultiply(Header.PartsX, Header.PartsZ)),
                WrapMultiply(zInc, Header.PartsX)),
            xInc);
    }

    void MphCollisionInfo::GetPartition(
        OpenTK::Mathematics::Vector3 point,
        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>> points,
        MphRead::EntityType entityType,
        MphRead::Scene* scenePtr) const
    {
        const std::int32_t entryIndex
            = EntryIndexFromPoint(point);

        MphRead::Scene& scene = RequireScene(scenePtr);
        std::int32_t polygonId = scene.GetNextPolygonId();

        const std::size_t entryCount = Count(Entries);

        // Deliberately ">" rather than ">=" to reproduce the C# behavior.
        if (entryIndex < 0
            || entryIndex > static_cast<std::int32_t>(entryCount))
        {
            return;
        }

        const CollisionEntry& entry
            = Entries->at(static_cast<std::size_t>(entryIndex));

        for (std::int32_t i = 0;
            i < entry.DataCount;
            ++i)
        {
            const std::uint16_t dataIndex = At(
                DataIndices,
                static_cast<std::size_t>(entry.DataStartIndex) + i);

            const CollisionData& data
                = At(Data, dataIndex);

            if (scene.ColTerDisplay != MphRead::Terrain::All
                && scene.ColTerDisplay != data.Terrain())
            {
                continue;
            }

            if ((scene.ColTypeDisplay == CollisionType::Player
                    && data.IgnorePlayers())
                || (scene.ColTypeDisplay == CollisionType::Beam
                    && data.IgnoreBeams())
                || (scene.ColTypeDisplay == CollisionType::Both
                    && (data.IgnorePlayers() || data.IgnoreBeams())))
            {
                continue;
            }

            OpenTK::Mathematics::Vector4 color{};

            if (scene.ColDisplayColor == CollisionColor::Entity)
            {
                if (entityType == MphRead::EntityType::Platform)
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.109F, 0.768F, 0.850F, 1.0F);
                }
                else if (entityType == MphRead::EntityType::Object)
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.952F, 0.105F, 0.635F, 1.0F);
                }
                else
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.952F, 0.694F, 0.105F, 1.0F);
                }
            }
            else if (scene.ColDisplayColor == CollisionColor::Terrain)
            {
                color = Colors().at(
                    static_cast<std::size_t>(data.Terrain()));
            }
            else if (scene.ColDisplayColor == CollisionColor::Type)
            {
                if (data.IgnoreBeams())
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.956F, 0.933F, 0.203F, 1.0F);
                }
                else if (data.IgnorePlayers())
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.250F, 0.807F, 0.250F, 1.0F);
                }
                else
                {
                    color = OpenTK::Mathematics::Vector4(
                        0.807F, 0.250F, 0.776F, 1.0F);
                }
            }
            else
            {
                color = OpenTK::Mathematics::Vector4(
                    1.0F, 0.0F, 0.0F, 1.0F);
            }

            color.W = scene.ColDisplayAlpha;

            assert(
                data.PointIndexCount >= 3
                && data.PointIndexCount <= 10);

            auto verts = std::make_shared<
                MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(
                    data.PointIndexCount);

            for (std::int32_t j = 0;
                j < data.PointIndexCount;
                ++j)
            {
                const std::uint16_t pointIndex = At(
                    PointIndices,
                    static_cast<std::size_t>(data.PointStartIndex) + j);

                (*verts)[j] = At(points, pointIndex);
            }

            scene.AddRenderItem(
                MphRead::CullingMode::Back,
                polygonId,
                color,
                MphRead::RenderItemType::Ngon,
                verts,
                data.PointIndexCount);
        }

        auto bverts = std::make_shared<
            MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(8);

        OpenTK::Mathematics::Vector3 point0 = MinPosition;

        const OpenTK::Mathematics::Vector3i partInc
            = PartIndexFromEntry(entryIndex);

        point0.X += static_cast<float>(partInc.X * 4);
        point0.Y += static_cast<float>(partInc.Y * 4);
        point0.Z += static_cast<float>(partInc.Z * 4);

        const OpenTK::Mathematics::Vector3 sideX(
            4.0F, 0.0F, 0.0F);
        const OpenTK::Mathematics::Vector3 sideY(
            0.0F, 4.0F, 0.0F);
        const OpenTK::Mathematics::Vector3 sideZ(
            0.0F, 0.0F, 4.0F);

        (*bverts)[0] = point0;
        (*bverts)[1] = point0 + sideZ;
        (*bverts)[2] = point0 + sideX;
        (*bverts)[3] = point0 + sideX + sideZ;
        (*bverts)[4] = point0 + sideY;
        (*bverts)[5] = point0 + sideY + sideZ;
        (*bverts)[6] = point0 + sideX + sideY;
        (*bverts)[7] = point0 + sideX + sideY + sideZ;

        polygonId = scene.GetNextPolygonId();

        const OpenTK::Mathematics::Vector4 bcolor(
            1.0F, 0.3F, 1.0F, 0.5F);

        scene.AddRenderItem(
            MphRead::CullingMode::Front,
            polygonId,
            bcolor,
            MphRead::RenderItemType::Box,
            bverts,
            8);
    }

    FhCollisionInfo::FhCollisionInfo(
        FhCollisionHeader header,
        std::shared_ptr<const std::vector<MphRead::Vector3Fx>> points,
        std::shared_ptr<const std::vector<MphRead::Vector4Fx>> planes,
        std::shared_ptr<const std::vector<FhCollisionData>> data,
        std::shared_ptr<const std::vector<FhCollisionVector>> vectors,
        std::shared_ptr<const std::vector<std::uint16_t>> dataIndices,
        std::shared_ptr<const std::vector<std::shared_ptr<Portal>>> portals,
        std::shared_ptr<const std::vector<FhCollisionEntry>> entries,
        std::shared_ptr<const std::vector<std::int32_t>> treeNodeIndices,
        std::shared_ptr<const std::vector<FhCollisionTreeNode>> treeNodes)
        : CollisionInfo(
              std::move(points),
              std::move(planes),
              std::move(portals),
              true),
          Header(header),
          Data(std::move(data)),
          Vectors(std::move(vectors)),
          DataIndices(std::move(dataIndices)),
          Entries(std::move(entries)),
          TreeNodeIndices(std::move(treeNodeIndices)),
          TreeNodes(std::move(treeNodes))
    {
    }

    void FhCollisionInfo::GetDrawInfo(
        std::shared_ptr<const std::vector<OpenTK::Mathematics::Vector3>> points,
        OpenTK::Mathematics::Vector3 translation,
        MphRead::EntityType entityType,
        MphRead::Scene* scenePtr)
    {
        static_cast<void>(entityType);

        MphRead::Scene& scene = RequireScene(scenePtr);

        OpenTK::Mathematics::Vector4 color(
            OpenTK::Mathematics::Vector3(1.0F, 0.0F, 0.0F),
            0.5F);

        color.W = scene.ColDisplayAlpha;

        const std::int32_t polygonId
            = scene.GetNextPolygonId();

        const std::size_t portalCount = Count(Portals);
        const std::size_t dataCount = Count(Data);

        for (std::size_t i = portalCount;
            i < dataCount;
            ++i)
        {
            const FhCollisionData& data = (*Data)[i];

            assert(
                data.VectorCount >= 3
                && data.VectorCount <= 8);

            auto verts = std::make_shared<
                MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(
                    data.VectorCount);

            for (std::int32_t j = 0;
                j < data.VectorCount;
                ++j)
            {
                const FhCollisionVector& vector = At(
                    Vectors,
                    static_cast<std::size_t>(data.VectorStartIndex) + j);

                (*verts)[j]
                    = At(points, vector.Point2Index) + translation;
            }

            scene.AddRenderItem(
                MphRead::CullingMode::Back,
                polygonId,
                color,
                MphRead::RenderItemType::Ngon,
                verts,
                data.VectorCount);
        }
    }

    void FhCollisionInfo::GetPartition(
        std::shared_ptr<std::vector<OpenTK::Mathematics::Vector3>> points,
        MphRead::Scene* scenePtr) const
    {
        MphRead::Scene& scene = RequireScene(scenePtr);

        const std::int32_t entryIndex
            = static_cast<std::int32_t>(scene.ShowVolumes);

        if (entryIndex <= 0)
        {
            return;
        }

        OpenTK::Mathematics::Vector4 color(
            OpenTK::Mathematics::Vector3(1.0F, 0.0F, 0.0F),
            0.5F);

        color.W = scene.ColDisplayAlpha;

        std::int32_t polygonId
            = scene.GetNextPolygonId();

        // Deliberately no upper-bound guard: C# indexes directly here.
        const FhCollisionEntry& entry
            = At(Entries, static_cast<std::size_t>(entryIndex));

        for (std::int32_t i = 0;
            i < entry.DataCount;
            ++i)
        {
            const std::int32_t dataIndex = At(
                DataIndices,
                static_cast<std::size_t>(entry.DataStartIndex) + i);

            const FhCollisionData& data
                = At(Data, static_cast<std::size_t>(dataIndex));

            assert(
                data.VectorCount >= 3
                && data.VectorCount <= 8);

            auto verts = std::make_shared<
                MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(
                    data.VectorCount);

            for (std::int32_t j = 0;
                j < data.VectorCount;
                ++j)
            {
                const FhCollisionVector& vector = At(
                    Vectors,
                    static_cast<std::size_t>(data.VectorStartIndex) + j);

                (*verts)[j] = At(points, vector.Point2Index);
            }

            scene.AddRenderItem(
                MphRead::CullingMode::Back,
                polygonId,
                color,
                MphRead::RenderItemType::Ngon,
                verts,
                data.VectorCount);
        }

        auto bverts = std::make_shared<
            MphRead::ManagedArray<OpenTK::Mathematics::Vector3>>(8);

        const OpenTK::Mathematics::Vector3 minPoint
            = entry.MinBounds.ToFloatVector();

        const OpenTK::Mathematics::Vector3 maxPoint
            = entry.MaxBounds.ToFloatVector();

        const OpenTK::Mathematics::Vector3 sideX(
            maxPoint.X - minPoint.X,
            0.0F,
            0.0F);

        const OpenTK::Mathematics::Vector3 sideY(
            0.0F,
            maxPoint.Y - minPoint.Y,
            0.0F);

        const OpenTK::Mathematics::Vector3 sideZ(
            0.0F,
            0.0F,
            maxPoint.Z - minPoint.Z);

        (*bverts)[0] = minPoint;
        (*bverts)[1] = minPoint + sideZ;
        (*bverts)[2] = minPoint + sideX;
        (*bverts)[3] = minPoint + sideX + sideZ;
        (*bverts)[4] = minPoint + sideY;
        (*bverts)[5] = minPoint + sideY + sideZ;
        (*bverts)[6] = minPoint + sideX + sideY;
        (*bverts)[7] = minPoint + sideX + sideY + sideZ;

        polygonId = scene.GetNextPolygonId();

        const OpenTK::Mathematics::Vector4 bcolor(
            1.0F, 0.3F, 1.0F, 0.5F);

        scene.AddRenderItem(
            MphRead::CullingMode::Front,
            polygonId,
            bcolor,
            MphRead::RenderItemType::Box,
            bverts,
            8);
    }

    std::shared_ptr<CollisionInstance> Collision::GetCollision(
        const MphRead::ModelMetadata* meta,
        bool extra)
    {
        if (meta == nullptr)
        {
            throw System::NullReferenceException();
        }

        std::optional<std::string> path
            = extra
            ? meta->ExtraCollisionPath
            : meta->CollisionPath;

        assert(path.has_value());

        std::string name = meta->Name;

        if (name == "AlimbicCapsule" && extra)
        {
            name = "AlmbCapsuleShld";
        }

        return GetCollision(
            std::move(path),
            std::move(name),
            meta->FirstHunt,
            -1,
            true);
    }

    std::shared_ptr<CollisionInstance> Collision::GetCollision(
        const MphRead::RoomMetadata* meta,
        std::int32_t roomLayerMask)
    {
        if (meta == nullptr)
        {
            throw System::NullReferenceException();
        }

        if (roomLayerMask == 0
            && meta->NodeLayer > 0)
        {
            const std::uint32_t bit
                = 1U
                << (
                    static_cast<std::uint32_t>(meta->NodeLayer)
                    & 31U
                );

            roomLayerMask = static_cast<std::int32_t>(
                (bit & 0xFFU) << 6);
        }

        return GetCollision(
            std::optional<std::string>(meta->CollisionPath),
            meta->Name,
            meta->FirstHunt || meta->Hybrid,
            roomLayerMask,
            false);
    }

    std::shared_ptr<CollisionInstance> Collision::GetCollision(
        std::optional<std::string> path,
        std::string name,
        bool firstHunt,
        std::int32_t roomLayerMask,
        bool isEntity)
    {
        CollisionCaches& caches = Caches();

        auto& cache = firstHunt
            ? caches.Fh
            : caches.Mph;

        if (roomLayerMask == -1)
        {
            if (!path)
            {
                throw System::ArgumentNullException("key");
            }

            const auto existing = cache.find(*path);
            if (existing != cache.end())
            {
                return std::make_shared<CollisionInstance>(
                    std::move(name),
                    existing->second,
                    isEntity);
            }
        }

        if (!path)
        {
            throw System::ArgumentNullException("path");
        }

        // Exact prerequisite contracts:
        //
        // Paths::FileSystem
        // Paths::FhFileSystem
        // Paths::Combine(root, relativePath)
        // System.IO File.ReadAllBytes equivalent
        //
        // The actual call must remain behaviorally identical to:
        //
        // File.ReadAllBytes(Paths.Combine(
        //     firstHunt ? Paths.FhFileSystem : Paths.FileSystem,
        //     path))
        //
        // The following names intentionally assume those exact prerequisite
        // Native counterparts.

        const std::string fullPath = Paths::Combine(
            firstHunt
                ? Paths::FhFileSystem
                : Paths::FileSystem,
            *path);

        const std::vector<std::uint8_t> storage
            = System::IO::File::ReadAllBytes(fullPath);

        const std::span<const std::uint8_t> bytes(
            storage.data(),
            storage.size());

        const CollisionHeader header
            = Read::ReadStruct<CollisionHeader>(bytes);

        std::shared_ptr<CollisionInfo> info;

        if (MarshalString(header.Type) == "wc01")
        {
            info = ReadMphCollision(
                header,
                bytes,
                roomLayerMask);
        }
        else
        {
            info = ReadFhCollision(bytes);
        }

        if (roomLayerMask == -1)
        {
            const auto [iterator, inserted]
                = cache.emplace(*path, info);

            if (!inserted)
            {
                // Dictionary.Add would fail rather than overwrite.
                throw std::runtime_error(
                    "An item with the same key has already been added.");
            }
        }

        return std::make_shared<CollisionInstance>(
            std::move(name),
            std::move(info),
            isEntity);
    }

    std::shared_ptr<MphCollisionInfo> Collision::ReadMphCollision(
        CollisionHeader header,
        std::span<const std::uint8_t> bytes,
        std::int32_t roomLayerMask)
    {
        const auto points
            = Read::DoOffsets<MphRead::Vector3Fx>(
                bytes,
                header.PointOffset,
                header.PointCount);

        const auto planes
            = Read::DoOffsets<MphRead::Vector4Fx>(
                bytes,
                header.PlaneOffset,
                header.PlaneCount);

        const auto pointIdxs
            = Read::DoOffsets<std::uint16_t>(
                bytes,
                header.PointIndexOffset,
                header.PointIndexCount);

        const auto data
            = Read::DoOffsets<CollisionData>(
                bytes,
                header.DataOffset,
                header.DataCount);

        const auto dataIdxs
            = Read::DoOffsets<std::uint16_t>(
                bytes,
                header.DataIndexOffset,
                header.DataIndexCount);

        const auto entries
            = Read::DoOffsets<CollisionEntry>(
                bytes,
                header.EntryOffset,
                header.EntryCount);

        auto portals
            = std::make_shared<std::vector<std::shared_ptr<Portal>>>();

        const auto rawPortals
            = Read::DoOffsets<RawCollisionPortal>(
                bytes,
                header.PortalOffset,
                header.PortalCount);

        for (const RawCollisionPortal& portal : *rawPortals)
        {
            if ((portal.LayerMask & 4) != 0
                || roomLayerMask == -1
                || (portal.LayerMask & roomLayerMask) != 0)
            {
                portals->push_back(
                    std::make_shared<Portal>(portal));
            }
        }

        auto finalData
            = std::make_shared<std::vector<CollisionData>>();

        auto finalIndices
            = std::make_shared<std::vector<std::uint16_t>>();

        auto finalEntries
            = std::make_shared<std::vector<CollisionEntry>>();

        if (roomLayerMask == -1)
        {
            finalData->insert(
                finalData->end(),
                data->begin(),
                data->end());

            finalIndices->insert(
                finalIndices->end(),
                dataIdxs->begin(),
                dataIdxs->end());

            finalEntries->insert(
                finalEntries->end(),
                entries->begin(),
                entries->end());
        }
        else
        {
            std::unordered_map<std::uint16_t, std::uint16_t> indexMap;

            for (const CollisionEntry& entry : *entries)
            {
                if (entry.DataCount > 0)
                {
                    std::uint16_t newCount = 0;

                    const std::uint16_t newStartIndex
                        = static_cast<std::uint16_t>(
                            finalIndices->size());

                    for (std::int32_t i = 0;
                        i < entry.DataCount;
                        ++i)
                    {
                        const std::uint16_t oldIndex = dataIdxs->at(
                            static_cast<std::size_t>(
                                entry.DataStartIndex) + i);

                        const auto mapped
                            = indexMap.find(oldIndex);

                        if (mapped != indexMap.end())
                        {
                            finalIndices->push_back(
                                mapped->second);

                            newCount = static_cast<std::uint16_t>(
                                newCount + 1);
                        }
                        else
                        {
                            const CollisionData& item
                                = data->at(oldIndex);

                            if ((item.LayerMask & 4) != 0
                                || (item.LayerMask & roomLayerMask) != 0)
                            {
                                const std::uint16_t newIndex
                                    = static_cast<std::uint16_t>(
                                        finalData->size());

                                finalIndices->push_back(newIndex);
                                finalData->push_back(item);

                                newCount
                                    = static_cast<std::uint16_t>(
                                        newCount + 1);

                                indexMap.emplace(
                                    oldIndex,
                                    newIndex);
                            }
                        }
                    }

                    finalEntries->emplace_back(
                        newCount,
                        newStartIndex);
                }
                else
                {
                    finalEntries->push_back(entry);
                }
            }
        }

        return std::make_shared<MphCollisionInfo>(
            header,
            points,
            planes,
            pointIdxs,
            finalData,
            finalIndices,
            finalEntries,
            portals);
    }

    std::shared_ptr<FhCollisionInfo> Collision::ReadFhCollision(
        std::span<const std::uint8_t> bytes)
    {
        const FhCollisionHeader header
            = Read::ReadStruct<FhCollisionHeader>(bytes);

        const auto data
            = Read::DoOffsets<FhCollisionData>(
                bytes,
                header.DataOffset,
                header.DataCount);

        const auto vectors
            = Read::DoOffsets<FhCollisionVector>(
                bytes,
                header.VectorOffset,
                header.VectorCount);

        const auto dataIndices
            = Read::DoOffsets<std::uint16_t>(
                bytes,
                header.DataIndexOffset,
                header.DataIndexCount);

        const auto points
            = Read::DoOffsets<MphRead::Vector3Fx>(
                bytes,
                header.PointOffset,
                header.PointCount);

        const auto planes
            = Read::DoOffsets<MphRead::Vector4Fx>(
                bytes,
                header.PlaneOffset,
                header.PlaneCount);

        const auto entries
            = Read::DoOffsets<FhCollisionEntry>(
                bytes,
                header.EntryOffset,
                header.EntryCount);

        const auto treeNodeIndices
            = Read::DoOffsets<std::int32_t>(
                bytes,
                header.TreeNodeIndexOffset,
                header.TreeNodeIndexCount);

        const auto treeNodes
            = Read::DoOffsets<FhCollisionTreeNode>(
                bytes,
                header.TreeNodeOffset,
                header.TreeNodeCount);

        auto portals
            = std::make_shared<std::vector<std::shared_ptr<Portal>>>();

        const auto rawPortals
            = Read::DoOffsets<FhCollisionPortal>(
                bytes,
                header.PortalOffset,
                header.PortalCount);

        for (const FhCollisionPortal& portal : *rawPortals)
        {
            portals->push_back(
                std::make_shared<Portal>(
                    portal,
                    vectors,
                    points,
                    planes));
        }

        return std::make_shared<FhCollisionInfo>(
            header,
            points,
            planes,
            data,
            vectors,
            dataIndices,
            portals,
            entries,
            treeNodeIndices,
            treeNodes);
    }
}

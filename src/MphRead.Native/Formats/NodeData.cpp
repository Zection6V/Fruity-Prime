#include "NodeData.hpp"

#include "Formats.hpp"
#include "../Program.hpp"
#include "../Read.hpp"

#include <algorithm>
#include <bit>
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>

namespace MphRead::NativeRuntime
{
    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path);
    void DirectoryEnumerateFiles(
        const std::string& path,
        const std::function<void(const std::string&)>& visitor);
    [[nodiscard]] std::string PathGetFileName(const std::string& path);
    [[nodiscard]] bool StringEndsWithCurrentCulture(
        const std::string& value, const std::string& suffix);
    [[noreturn]] void ThrowListIndexOutOfRange();
    void DebugAssert(bool condition);
}

namespace
{
    using MphRead::Formats::FhNodeData;
    using MphRead::Formats::NodeData;
    using MphRead::Formats::NodeData3;
    using MphRead::Formats::NodeDataHeader;
    using MphRead::Formats::NodeDataStruct1;
    using MphRead::Formats::NodeDataStruct2;
    using MphRead::Formats::NodeDataStruct3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;

    template <typename T>
    [[nodiscard]] const T& Require(const std::shared_ptr<const T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] Matrix4 CreateTranslation(Vector3 position) noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(position, 1.0F));
    }

    [[nodiscard]] constexpr float DistanceSquared(Vector3 left, Vector3 right) noexcept
    {
        const float x = left.X - right.X;
        const float y = left.Y - right.Y;
        const float z = left.Z - right.Z;
        return x * x + y * y + z * z;
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::int64_t value) noexcept
    {
        return ManagedInt32(static_cast<std::uint32_t>(value));
    }

    template <typename T>
    [[nodiscard]] const T& ManagedListAt(
        const std::vector<T>& values, std::size_t index)
    {
        if (index >= values.size())
        {
            MphRead::NativeRuntime::ThrowListIndexOutOfRange();
        }
        return values[index];
    }
}

namespace MphRead::Formats
{
    const std::vector<Vector4> NodeData3::_nodeDataColors = {
        Vector4(1.0F, 0.0F, 0.0F, 1.0F),
        Vector4(0.0F, 1.0F, 0.0F, 1.0F),
        Vector4(0.0F, 0.0F, 1.0F, 1.0F),
        Vector4(0.0F, 1.0F, 1.0F, 1.0F),
        Vector4(1.0F, 0.0F, 1.0F, 1.0F),
        Vector4(1.0F, 1.0F, 0.0F, 1.0F)
    };

    NodeData::NodeData(
        NodeDataHeader header,
        std::shared_ptr<const std::vector<std::uint16_t>> setIndices,
        std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> data)
        : NodeData(BuildInit(
            header,
            std::move(setIndices),
            std::move(data)))
    {
    }

    NodeData::NodeData(Init init)
        : Header(init.Header),
          SetIndices(std::move(init.SetIndices)),
          Data(std::move(init.Data)),
          SetSelector(std::move(init.SetSelector))
    {
    }

    NodeData::Init NodeData::BuildInit(
        NodeDataHeader header,
        std::shared_ptr<const std::vector<std::uint16_t>> setIndices,
        std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> data)
    {
        auto setSelector = MphRead::NativeRuntime::CreateManagedBoolArray(16);
        return Init{
            header,
            std::move(setIndices),
            std::move(data),
            std::move(setSelector)};
    }

    bool NodeData::Simple() const
    {
        const auto& data = Require(Data);
        return data.size() == 1 && Require(data[0]).size() == 1;
    }

    NodeData3::NodeData3(
        NodeDataStruct3 raw,
        std::int32_t index1,
        std::int32_t index2,
        std::shared_ptr<const std::vector<std::uint16_t>> values)
        : NodeType(static_cast<MphRead::Formats::NodeType>(raw.NodeType)),
          Id(raw.Id),
          Field4(raw.Field4),
          Count2(raw.Count2),
          Position(raw.Position.ToFloatVector()),
          MaxDistance(MphRead::Fixed::ToFloat(raw.MaxDistance)),
          Index1(index1),
          Index2(index2),
          Values(std::move(values)),
          Transform(CreateTranslation(Position)),
          Color(ManagedListAt(_nodeDataColors, raw.NodeType))
    {
    }

    NodeData3::NodeData3(MphRead::Vector3Fx position)
        : NodeType(MphRead::Formats::NodeType::Navigation),
          Id(0),
          Field4(0),
          Count2(0),
          Position(position.ToFloatVector()),
          MaxDistance(0.0F),
          Index1(0),
          Index2(0),
          Values(std::make_shared<const std::vector<std::uint16_t>>()),
          Transform(CreateTranslation(Position)),
          Color(ManagedListAt(_nodeDataColors, 0))
    {
    }

    void ReadNodeData::TestAll()
    {
        const std::string directory = MphRead::Paths::Combine(
            MphRead::Paths::FileSystem(), "levels\\nodeData");
        MphRead::NativeRuntime::DirectoryEnumerateFiles(
            directory,
            [](const std::string& path)
            {
                if (!MphRead::NativeRuntime::StringEndsWithCurrentCulture(
                        path, "levels\\nodeData\\unit2_Land_Node.bin"))
                {
                    static_cast<void>(ReadNodeData::ReadData(
                        MphRead::Paths::Combine(
                            "levels\\nodeData",
                            MphRead::NativeRuntime::PathGetFileName(path)),
                        false));
                }
            });
        Nop();
    }

    std::shared_ptr<NodeData> ReadNodeData::ReadData(
        const std::string& path, bool firstHunt)
    {
        const std::vector<std::uint8_t> storage = MphRead::NativeRuntime::FileReadAllBytes(MphRead::Paths::Combine(
            firstHunt ? MphRead::Paths::FhFileSystem() : MphRead::Paths::FileSystem(),
            path));
        const std::span<const std::uint8_t> bytes(storage);

        const std::uint16_t version = MphRead::Read::SpanReadUshort(bytes, 0);
        if (firstHunt)
        {
            MphRead::NativeRuntime::DebugAssert(version == 0);
        }
        if (version == 0)
        {
            return ReadFhNodeData(bytes);
        }
        if (version != 6)
        {
            throw MphRead::ProgramException(
                "Unexpected node data version " + std::to_string(version) + ".");
        }

        const NodeDataHeader header = MphRead::Read::ReadStruct<NodeDataHeader>(bytes);
        const std::int32_t setIndexCount = header.IndexCount;
        MphRead::NativeRuntime::DebugAssert(
            setIndexCount == 0 || setIndexCount == 1);
        MphRead::NativeRuntime::DebugAssert(
            header.DataOffset == header.IndexOffset + 2U);

        std::unordered_set<std::uint32_t> types;
        std::uint32_t min = std::numeric_limits<std::uint32_t>::max();

        const auto setIndices = MphRead::Read::DoOffsets<std::uint16_t>(
            bytes, header.IndexOffset, setIndexCount);

        auto data = std::make_shared<std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<NodeDataStruct3>>>>>>();

        const auto str1s = MphRead::Read::DoOffsets<NodeDataStruct1>(
            bytes, header.DataOffset, static_cast<std::int32_t>(header.DataCount));
        for (const NodeDataStruct1& str1 : Require(str1s))
        {
            auto sub = std::make_shared<std::vector<
                std::shared_ptr<const std::vector<NodeDataStruct3>>>>();
            const auto str2s = MphRead::Read::DoOffsets<NodeDataStruct2>(
                bytes, str1.Offset2, static_cast<std::int32_t>(str1.Count));
            for (const NodeDataStruct2& str2 : Require(str2s))
            {
                const auto str3s = MphRead::Read::DoOffsets<NodeDataStruct3>(
                    bytes, str2.Offset3, static_cast<std::int32_t>(str2.Count));
                for (const NodeDataStruct3& str3 : Require(str3s))
                {
                    min = std::min(min, str3.Offset1);
                    types.insert(str3.NodeType);
                }
                sub->push_back(str3s);
            }
            data->push_back(sub);
        }

        const std::int64_t valueBytes
            = static_cast<std::int64_t>(storage.size()) - static_cast<std::int64_t>(min);
        const std::int32_t valueCount = ManagedInt32(valueBytes / 2);
        const auto values = MphRead::Read::DoOffsets<std::uint16_t>(bytes, min, valueCount);

        auto cast = std::make_shared<std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>> >();

        for (const auto& sub : *data)
        {
            auto newSub = std::make_shared<std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>();
            for (const auto& str3s : Require(sub))
            {
                auto cast3s = std::make_shared<std::vector<std::shared_ptr<NodeData3>>>();
                for (const NodeDataStruct3& str3 : Require(str3s))
                {
                    const std::int32_t index1 = ManagedInt32((str3.Offset1 - min) / 2U);
                    const std::int32_t index2 = ManagedInt32((str3.Offset2 - min) / 2U);
                    cast3s->push_back(std::make_shared<NodeData3>(
                        str3, index1, index2, values));
                }
                newSub->push_back(cast3s);
            }
            cast->push_back(newSub);
        }

        return std::make_shared<NodeData>(header, setIndices, cast);
    }

    std::shared_ptr<NodeData> ReadNodeData::ReadFhNodeData(
        std::span<const std::uint8_t> bytes)
    {
        const std::uint16_t count = MphRead::Read::SpanReadUshort(bytes, 2);
        const auto headers = MphRead::Read::DoOffsets<FhNodeData>(
            bytes, 4U, static_cast<std::uint32_t>(count));

        auto cast3s = std::make_shared<std::vector<std::shared_ptr<NodeData3>>>();
        cast3s->reserve(Require(headers).size());
        for (const FhNodeData& header : Require(headers))
        {
            cast3s->push_back(std::make_shared<NodeData3>(header.Position));
        }

        auto sub = std::make_shared<std::vector<
            std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>();
        sub->push_back(cast3s);

        auto data = std::make_shared<std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>>();
        data->push_back(sub);

        auto setIndices = std::make_shared<std::vector<std::uint16_t>>();
        setIndices->push_back(0);

        return std::make_shared<NodeData>(NodeDataHeader{}, setIndices, data);
    }

    std::shared_ptr<NodeData3> ReadNodeData::FindClosestNode(
        const std::shared_ptr<NodeData>& nodeData,
        Vector3 position,
        bool useMaxDist)
    {
        NodeData& dataObject = Require(nodeData);
        const auto& outer = Require(dataObject.Data);
#ifndef NDEBUG
        const bool hasFirstList
            = !outer.empty() && !Require(outer[0]).empty();
        MphRead::NativeRuntime::DebugAssert(hasFirstList);
#endif

        const auto& middle = Require(ManagedListAt(outer, 0));
        const auto& list = Require(ManagedListAt(middle, 0));

        std::shared_ptr<NodeData3> result{};
        float minDist = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            const std::shared_ptr<NodeData3>& data = list[i];
            NodeData3& item = Require(data);
            const float dist = DistanceSquared(position, item.Position);
            if (dist < minDist)
            {
                result = data;
                minDist = dist;
            }
        }
        if (result && useMaxDist
            && minDist > result->MaxDistance * result->MaxDistance)
        {
            result.reset();
        }
        return result;
    }

    void ReadNodeData::Nop() noexcept
    {
    }
}

#pragma once

#include "Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace MphRead::NativeRuntime
{
    class ManagedBoolArray;

    [[nodiscard]] std::shared_ptr<ManagedBoolArray> CreateManagedBoolArray(
        std::int32_t length);
    [[nodiscard]] std::int32_t ManagedBoolArrayLength(
        const std::shared_ptr<const ManagedBoolArray>& array);
    [[nodiscard]] bool ManagedBoolArrayGet(
        const std::shared_ptr<const ManagedBoolArray>& array, std::int32_t index);
    void ManagedBoolArraySet(
        const std::shared_ptr<ManagedBoolArray>& array,
        std::int32_t index,
        bool value);
}

namespace MphRead::Formats
{
    class NodeData;
    class NodeData3;

    enum class NodeType : std::uint16_t
    {
        Navigation = 0,
        Special = 1,
        Aerial = 2,
        Vantage = 3,
        AltForm = 4,
        Hazard = 5
    };

#pragma pack(push, 2)
    struct NodeDataHeader
    {
        const std::uint16_t Version = 0;
        const std::uint16_t DataCount = 0;
        const std::uint32_t IndexOffset = 0;
        const std::uint32_t DataOffset = 0;
        const std::uint16_t IndexCount = 0;

        NodeDataHeader() noexcept = default;
        NodeDataHeader(const NodeDataHeader&) noexcept = default;
        NodeDataHeader& operator=(const NodeDataHeader& other) noexcept
        {
            if (this != &other)
            {
                this->~NodeDataHeader();
                ::new (static_cast<void*>(this)) NodeDataHeader(other);
            }
            return *this;
        }
    };
#pragma pack(pop)

    struct NodeDataStruct1
    {
        const std::uint32_t Offset2 = 0;
        const std::uint16_t Count = 0;
        const std::uint16_t Padding6 = 0;

        NodeDataStruct1() noexcept = default;
        NodeDataStruct1(const NodeDataStruct1&) noexcept = default;
        NodeDataStruct1& operator=(const NodeDataStruct1& other) noexcept
        {
            if (this != &other)
            {
                this->~NodeDataStruct1();
                ::new (static_cast<void*>(this)) NodeDataStruct1(other);
            }
            return *this;
        }
    };

    struct NodeDataStruct2
    {
        const std::uint32_t Offset3 = 0;
        const std::uint16_t Count = 0;
        const std::uint16_t Padding6 = 0;

        NodeDataStruct2() noexcept = default;
        NodeDataStruct2(const NodeDataStruct2&) noexcept = default;
        NodeDataStruct2& operator=(const NodeDataStruct2& other) noexcept
        {
            if (this != &other)
            {
                this->~NodeDataStruct2();
                ::new (static_cast<void*>(this)) NodeDataStruct2(other);
            }
            return *this;
        }
    };

    struct NodeDataStruct3
    {
        const std::uint16_t NodeType = 0;
        const std::uint16_t Id = 0;
        const std::uint16_t Field4 = 0;
        const std::uint16_t Count2 = 0;
        const MphRead::Vector3Fx Position{};
        const std::int32_t MaxDistance = 0;
        const std::uint32_t Offset1 = 0;
        const std::uint32_t Offset2 = 0;
        const std::uint32_t Offset3 = 0;

        NodeDataStruct3() noexcept = default;
        NodeDataStruct3(const NodeDataStruct3&) noexcept = default;
        NodeDataStruct3& operator=(const NodeDataStruct3& other) noexcept
        {
            if (this != &other)
            {
                this->~NodeDataStruct3();
                ::new (static_cast<void*>(this)) NodeDataStruct3(other);
            }
            return *this;
        }
    };

    struct FhNodeData
    {
        const std::uint16_t Field0 = 0;
        const std::uint16_t Field2 = 0;
        const MphRead::Vector3Fx Position{};
        const std::uint32_t Offset1 = 0;
        const std::uint32_t Offset2 = 0;

        FhNodeData() noexcept = default;
        FhNodeData(const FhNodeData&) noexcept = default;
        FhNodeData& operator=(const FhNodeData& other) noexcept
        {
            if (this != &other)
            {
                this->~FhNodeData();
                ::new (static_cast<void*>(this)) FhNodeData(other);
            }
            return *this;
        }
    };

    class NodeData
    {
    public:
        const NodeDataHeader Header;
        const std::shared_ptr<const std::vector<std::uint16_t>> SetIndices;
        const std::shared_ptr<const std::vector<
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> Data;
        const std::shared_ptr<MphRead::NativeRuntime::ManagedBoolArray> SetSelector;

        NodeData(
            NodeDataHeader header,
            std::shared_ptr<const std::vector<std::uint16_t>> setIndices,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<
                    std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> data);

        NodeData(const NodeData&) = delete;
        NodeData& operator=(const NodeData&) = delete;
        NodeData(NodeData&&) = delete;
        NodeData& operator=(NodeData&&) = delete;

        [[nodiscard]] bool Simple() const;

    private:
        struct Init
        {
            NodeDataHeader Header;
            std::shared_ptr<const std::vector<std::uint16_t>> SetIndices;
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<
                    std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> Data;
            std::shared_ptr<MphRead::NativeRuntime::ManagedBoolArray> SetSelector;
        };

        explicit NodeData(Init init);
        [[nodiscard]] static Init BuildInit(
            NodeDataHeader header,
            std::shared_ptr<const std::vector<std::uint16_t>> setIndices,
            std::shared_ptr<const std::vector<
                std::shared_ptr<const std::vector<
                    std::shared_ptr<const std::vector<std::shared_ptr<NodeData3>>>>>>> data);
    };

    class NodeData3
    {
    public:
        const MphRead::Formats::NodeType NodeType;
        const std::uint16_t Id;
        const std::uint32_t Field4;
        const std::int32_t Count2;
        const OpenTK::Mathematics::Vector3 Position;
        const float MaxDistance;
        const std::int32_t Index1;
        const std::int32_t Index2;
        const std::shared_ptr<const std::vector<std::uint16_t>> Values;

        const OpenTK::Mathematics::Matrix4 Transform;
        const OpenTK::Mathematics::Vector4 Color;

        NodeData3(
            NodeDataStruct3 raw,
            std::int32_t index1,
            std::int32_t index2,
            std::shared_ptr<const std::vector<std::uint16_t>> values);
        explicit NodeData3(MphRead::Vector3Fx position);

        NodeData3(const NodeData3&) = delete;
        NodeData3& operator=(const NodeData3&) = delete;
        NodeData3(NodeData3&&) = delete;
        NodeData3& operator=(NodeData3&&) = delete;

    private:
        static const std::vector<OpenTK::Mathematics::Vector4> _nodeDataColors;
    };

    class ReadNodeData final
    {
    public:
        ReadNodeData() = delete;

        static void TestAll();
        [[nodiscard]] static std::shared_ptr<NodeData> ReadData(
            const std::string& path, bool firstHunt);
        [[nodiscard]] static std::shared_ptr<NodeData3> FindClosestNode(
            const std::shared_ptr<NodeData>& nodeData,
            OpenTK::Mathematics::Vector3 position,
            bool useMaxDist = false);

    private:
        [[nodiscard]] static std::shared_ptr<NodeData> ReadFhNodeData(
            std::span<const std::uint8_t> bytes);
        static void Nop() noexcept;
    };

    static_assert(std::is_same_v<std::underlying_type_t<NodeType>, std::uint16_t>);
    static_assert(sizeof(NodeType) == sizeof(std::uint16_t));
    static_assert(static_cast<std::uint16_t>(NodeType::Navigation) == 0);
    static_assert(static_cast<std::uint16_t>(NodeType::Special) == 1);
    static_assert(static_cast<std::uint16_t>(NodeType::Aerial) == 2);
    static_assert(static_cast<std::uint16_t>(NodeType::Vantage) == 3);
    static_assert(static_cast<std::uint16_t>(NodeType::AltForm) == 4);
    static_assert(static_cast<std::uint16_t>(NodeType::Hazard) == 5);

    static_assert(std::is_standard_layout_v<NodeDataHeader>);
    static_assert(sizeof(NodeDataHeader) == 14);
    static_assert(alignof(NodeDataHeader) == 2);
    static_assert(offsetof(NodeDataHeader, Version) == 0);
    static_assert(offsetof(NodeDataHeader, DataCount) == 2);
    static_assert(offsetof(NodeDataHeader, IndexOffset) == 4);
    static_assert(offsetof(NodeDataHeader, DataOffset) == 8);
    static_assert(offsetof(NodeDataHeader, IndexCount) == 12);

    static_assert(std::is_standard_layout_v<NodeDataStruct1>);
    static_assert(sizeof(NodeDataStruct1) == 8);
    static_assert(alignof(NodeDataStruct1) == 4);
    static_assert(offsetof(NodeDataStruct1, Offset2) == 0);
    static_assert(offsetof(NodeDataStruct1, Count) == 4);
    static_assert(offsetof(NodeDataStruct1, Padding6) == 6);

    static_assert(std::is_standard_layout_v<NodeDataStruct2>);
    static_assert(sizeof(NodeDataStruct2) == 8);
    static_assert(alignof(NodeDataStruct2) == 4);
    static_assert(offsetof(NodeDataStruct2, Offset3) == 0);
    static_assert(offsetof(NodeDataStruct2, Count) == 4);
    static_assert(offsetof(NodeDataStruct2, Padding6) == 6);

    static_assert(std::is_standard_layout_v<NodeDataStruct3>);
    static_assert(sizeof(NodeDataStruct3) == 36);
    static_assert(alignof(NodeDataStruct3) == 4);
    static_assert(offsetof(NodeDataStruct3, NodeType) == 0);
    static_assert(offsetof(NodeDataStruct3, Id) == 2);
    static_assert(offsetof(NodeDataStruct3, Field4) == 4);
    static_assert(offsetof(NodeDataStruct3, Count2) == 6);
    static_assert(offsetof(NodeDataStruct3, Position) == 8);
    static_assert(offsetof(NodeDataStruct3, MaxDistance) == 20);
    static_assert(offsetof(NodeDataStruct3, Offset1) == 24);
    static_assert(offsetof(NodeDataStruct3, Offset2) == 28);
    static_assert(offsetof(NodeDataStruct3, Offset3) == 32);

    static_assert(std::is_standard_layout_v<FhNodeData>);
    static_assert(sizeof(FhNodeData) == 24);
    static_assert(alignof(FhNodeData) == 4);
    static_assert(offsetof(FhNodeData, Field0) == 0);
    static_assert(offsetof(FhNodeData, Field2) == 2);
    static_assert(offsetof(FhNodeData, Position) == 4);
    static_assert(offsetof(FhNodeData, Offset1) == 16);
    static_assert(offsetof(FhNodeData, Offset2) == 20);
}

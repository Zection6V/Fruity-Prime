#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace OpenTK::Mathematics
{
    struct Vector4
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };
}

namespace MphRead::Formats::Culling
{
    // C# string is a nullable, immutable reference type. This adapter keeps
    // reference identity across copies while retaining string-style value
    // equality and allowing the public field itself to be reassigned.
    class NullableString
    {
    public:
        constexpr NullableString() noexcept = default;
        constexpr NullableString(std::nullptr_t) noexcept
        {
        }

        NullableString(const char* value)
            : _value(value == nullptr ? nullptr : std::make_shared<const std::string>(value))
        {
        }

        NullableString(std::string value)
            : _value(std::make_shared<const std::string>(std::move(value)))
        {
        }

        explicit NullableString(std::shared_ptr<const std::string> value) noexcept
            : _value(std::move(value))
        {
        }

        NullableString(const NullableString&) noexcept = default;
        NullableString& operator=(const NullableString&) noexcept = default;

        // C# references have no destructive move operation. Keep native moves
        // copy-like so moving a NodeRef does not change the source RoomName.
        NullableString(NullableString&& other) noexcept
            : _value(other._value)
        {
        }

        NullableString& operator=(NullableString&& other) noexcept
        {
            if (this != &other)
            {
                _value = other._value;
            }
            return *this;
        }

        [[nodiscard]] bool HasValue() const noexcept
        {
            return static_cast<bool>(_value);
        }

        explicit operator bool() const noexcept
        {
            return HasValue();
        }

        [[nodiscard]] const std::string* Get() const noexcept
        {
            return _value.get();
        }

        [[nodiscard]] const std::string& operator*() const noexcept
        {
            return *_value;
        }

        [[nodiscard]] const std::string* operator->() const noexcept
        {
            return _value.get();
        }

        friend bool operator==(const NullableString& lhs, const NullableString& rhs) noexcept
        {
            if (lhs._value == rhs._value)
            {
                return true;
            }
            if (!lhs._value || !rhs._value)
            {
                return false;
            }
            return *lhs._value == *rhs._value;
        }

        friend bool operator!=(const NullableString& lhs, const NullableString& rhs) noexcept
        {
            return !(lhs == rhs);
        }

    private:
        std::shared_ptr<const std::string> _value{};
    };

    struct NodeRef
    {
        NullableString RoomName{};
        std::int32_t PartIndex = 0;
        std::int32_t NodeIndex = 0;
        std::int32_t ModelIndex = 0;

        static const NodeRef None;

        NodeRef() = default;
        constexpr NodeRef(std::nullptr_t, std::int32_t partIndex,
            std::int32_t nodeIndex, std::int32_t modelIndex) noexcept
            : RoomName(nullptr),
              PartIndex(partIndex),
              NodeIndex(nodeIndex),
              ModelIndex(modelIndex)
        {
        }
        NodeRef(NullableString roomName, std::int32_t partIndex,
            std::int32_t nodeIndex, std::int32_t modelIndex) noexcept;

        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;
    };

    [[nodiscard]] bool operator==(const NodeRef& lhs, const NodeRef& rhs);
    [[nodiscard]] bool operator!=(const NodeRef& lhs, const NodeRef& rhs);

    struct FrustumPlane
    {
        std::int32_t XIndex1 = 0;
        std::int32_t XIndex2 = 0;
        std::int32_t YIndex1 = 0;
        std::int32_t YIndex2 = 0;
        std::int32_t ZIndex1 = 0;
        std::int32_t ZIndex2 = 0;
        OpenTK::Mathematics::Vector4 Plane{};
    };

    class RoomPartVisInfo
    {
    public:
        MphRead::Formats::Culling::NodeRef NodeRef{};
        float ViewMinX = 0.0F;
        float ViewMaxX = 0.0F;
        float ViewMinY = 0.0F;
        float ViewMaxY = 0.0F;
        std::shared_ptr<RoomPartVisInfo> Next{};

        RoomPartVisInfo() = default;
        RoomPartVisInfo(const RoomPartVisInfo&) = delete;
        RoomPartVisInfo& operator=(const RoomPartVisInfo&) = delete;
        RoomPartVisInfo(RoomPartVisInfo&&) = delete;
        RoomPartVisInfo& operator=(RoomPartVisInfo&&) = delete;
    };

    class FrustumInfo
    {
    public:
        std::int32_t Index = 0;
        std::int32_t Count = 0;
        const std::shared_ptr<std::array<FrustumPlane, 10>> Planes;

        FrustumInfo();
        FrustumInfo(const FrustumInfo&) = delete;
        FrustumInfo& operator=(const FrustumInfo&) = delete;
        FrustumInfo(FrustumInfo&&) = delete;
        FrustumInfo& operator=(FrustumInfo&&) = delete;
    };

    class RoomFrustumItem
    {
    public:
        MphRead::Formats::Culling::NodeRef NodeRef{};
        const std::shared_ptr<FrustumInfo> Info;
        std::shared_ptr<RoomFrustumItem> Next{};

        RoomFrustumItem();
        RoomFrustumItem(const RoomFrustumItem&) = delete;
        RoomFrustumItem& operator=(const RoomFrustumItem&) = delete;
        RoomFrustumItem(RoomFrustumItem&&) = delete;
        RoomFrustumItem& operator=(RoomFrustumItem&&) = delete;
    };
}

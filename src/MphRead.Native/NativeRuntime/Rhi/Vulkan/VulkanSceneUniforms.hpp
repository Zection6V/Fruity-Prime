#pragma once

#include "../SceneShaderAbi.hpp"
#include "../../FrameTelemetry.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <cstring>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // The production std140 packing adapter. Descriptions are generated from
    // logical values and independently checked against compiled SPIR-V.
    class VulkanSceneUniforms
    {
    public:
        struct BlockDesc final
        {
            std::string_view semantic;
            std::uint32_t group, binding, size;
            bool small = false;
        };
        struct MemberDesc final
        {
            std::string_view name;
            std::uint32_t block;
            SceneShaderAbi::ValueType type;
            std::uint32_t offset, size, count;
        };
        struct Block final
        {
            std::uint32_t Group, Binding;
            std::vector<std::byte> Data;
            std::uint64_t Generation = 1;
            bool Small = false;
        };
        VulkanSceneUniforms(std::span<const BlockDesc> blocks, std::span<const MemberDesc> members)
        {
            for (const auto& desc : blocks)
            {
                if (desc.group >= SceneShaderAbi::GroupCount || !desc.size || desc.size % 16)
                    throw std::invalid_argument("Invalid Vulkan scene uniform block.");
                for (const auto& previous : Blocks)
                    if (previous.Group == desc.group && previous.Binding == desc.binding)
                        throw std::invalid_argument("Duplicate Vulkan scene uniform block.");
                if (desc.small && desc.size > 128) throw std::invalid_argument("Small constant budget exceeded.");
                Blocks.push_back({desc.group, desc.binding, std::vector<std::byte>(desc.size), 1, desc.small});
                if (desc.group == static_cast<std::uint32_t>(SceneShaderAbi::Group::Material) && !desc.small)
                    _materialBlock = Blocks.size() - 1;
            }
            for (const auto& desc : members)
            {
                if (desc.name.empty() || desc.block >= Blocks.size())
                    throw std::invalid_argument("Invalid Vulkan scene uniform member.");
                const auto width = ValueSize(desc.type);
                const auto size = desc.count ? std::uint64_t(desc.count) * ((width + 15) / 16 * 16) : width;
                const auto capacity = Blocks[desc.block].Data.size();
                const auto alignment = desc.count || width >= 12 ? 16U : 4U;
                if (desc.size != size || desc.offset % alignment || desc.offset > capacity || desc.size > capacity - desc.offset)
                    throw std::invalid_argument("Vulkan scene uniform member exceeds its ABI.");
                for (const auto& [name, previous] : Members)
                    if (previous.block == desc.block && desc.offset < previous.offset + previous.size
                        && previous.offset < desc.offset + desc.size)
                        throw std::invalid_argument("Overlapping Vulkan scene uniform members.");
                if (!Members.emplace(desc.name, desc).second)
                    throw std::invalid_argument("Duplicate Vulkan scene uniform member.");
                if (desc.name == "cel_bands") _celBands = std::pair{desc.block, desc.offset};
                if (desc.name == "toon_table") _toonTable = std::pair{desc.block, desc.offset};
                const auto slot = SceneShaderAbi::ConstantIndexOf(desc.name);
                if (slot < _denseMembers.size()) _denseMembers[slot] = desc;
            }
        }
        [[nodiscard]] const MemberDesc* Find(std::string_view name) const
        {
            const auto found = Members.find(name);
            return found == Members.end() ? nullptr : &found->second;
        }
        void Write(std::string_view name, const void* data, std::size_t size, SceneShaderAbi::ValueType type)
        {
            FrameTelemetry::Count(FrameTelemetry::Counter::UniformNames);
            WriteMember(Find(name), data, size, type);
        }
        void Write(std::size_t slot, const void* data, std::size_t size, SceneShaderAbi::ValueType type)
        {
            const FrameTelemetry::Scope measured(FrameTelemetry::Phase::Uniform);
            FrameTelemetry::Count(FrameTelemetry::Counter::UniformSlots);
            const auto& member = _denseMembers.at(slot);
            WriteMember(member ? &*member : nullptr, data, size, type);
        }
    private:
        void WriteMember(const MemberDesc* member, const void* data, std::size_t size, SceneShaderAbi::ValueType type)
        {
            if (!member) return; // OpenGL's inactive uniform semantics
            const auto intBool = type == SceneShaderAbi::ValueType::Int && member->type == SceneShaderAbi::ValueType::Bool;
            if (member->count || size != member->size || (type != member->type && !intBool) || !data)
                throw std::invalid_argument("Vulkan scene constant write does not match the shader ABI.");
            if (intBool)
            {
                std::int32_t value; std::memcpy(&value, data, sizeof(value));
                if (value != 0 && value != 1) throw std::invalid_argument("Invalid Vulkan scene boolean.");
            }
            auto& block = BlockAt(member->block);
            auto* destination = block.Data.data() + member->offset;
            if (!SameBytes(destination, data, size))
            {
                CopyBytes(destination, data, size); ++block.Generation;
                if (!_owner && _celBands == std::pair{member->block, member->offset}) ++_globalMaterialGeneration;
            }
        }
    public:
        void WriteArray(std::string_view name, const float* data, std::size_t elementFloats, std::size_t count)
        {
            FrameTelemetry::Count(FrameTelemetry::Counter::UniformNames);
            WriteArrayMember(Find(name), data, elementFloats, count);
        }
        void WriteArray(std::size_t slot, const float* data, std::size_t elementFloats, std::size_t count)
        {
            const FrameTelemetry::Scope measured(FrameTelemetry::Phase::Uniform);
            FrameTelemetry::Count(FrameTelemetry::Counter::UniformSlots);
            const auto& member = _denseMembers.at(slot);
            WriteArrayMember(member ? &*member : nullptr, data, elementFloats, count);
        }
    private:
        void WriteArrayMember(const MemberDesc* member, const float* data, std::size_t elementFloats, std::size_t count)
        {
            if (!member) return;
            if (!member->count || elementFloats != ValueSize(member->type) / sizeof(float) || (count && !data))
                throw std::invalid_argument("Vulkan scene array write does not match the shader ABI.");
            const auto stride = member->size / member->count;
            count = std::min<std::size_t>(count, member->count);
            auto& block = BlockAt(member->block);
            bool changed = false;
            for (std::size_t i = 0; i < count; ++i)
            {
                auto* destination = block.Data.data() + member->offset + i * stride;
                const auto* source = data + i * elementFloats;
                const auto size = ValueSize(member->type);
                if (!SameBytes(destination, source, size))
                { CopyBytes(destination, source, size); changed = true; }
            }
            if (changed)
            {
                ++block.Generation;
                if (!_owner && _toonTable == std::pair{member->block, member->offset}) ++_globalMaterialGeneration;
            }
        }
    public:
        void SelectMaterialOwner(std::uint64_t identity)
        {
            _owner = nullptr; _ownerIdentity = 0;
            if (!identity || !_materialBlock || identity >= OwnerLimit) return;
            const auto pageIndex = static_cast<std::size_t>(identity / OwnersPerPage);
            if (_owners.size() <= pageIndex) _owners.resize(pageIndex + 1);
            if (!_owners[pageIndex]) _owners[pageIndex] = std::make_unique<OwnerPage>();
            auto& owner = (*_owners[pageIndex])[identity % OwnersPerPage];
            if (owner.Material.Data.empty()) owner.Material = Blocks[*_materialBlock];
            if (owner.GlobalGeneration != _globalMaterialGeneration)
            {
                // Item writes stay in the owner's block; only these constants
                // inherit the program-wide material state.
                for (const auto name : {"toon_table", "cel_bands"})
                    if (const auto* member = Find(name))
                    {
                        const auto* source = Blocks[member->block].Data.data() + member->offset;
                        auto* destination = owner.Material.Data.data() + member->offset;
                        if (std::memcmp(destination, source, member->size) != 0)
                        { std::memcpy(destination, source, member->size); ++owner.Material.Generation; }
                    }
                owner.GlobalGeneration = _globalMaterialGeneration;
            }
            _owner = &owner; _ownerIdentity = identity;
        }
        [[nodiscard]] std::uint64_t MaterialOwnerIdentity() const noexcept { return _ownerIdentity; }
        [[nodiscard]] Block& BlockAt(std::size_t index)
        { return _owner && _materialBlock == index ? _owner->Material : Blocks.at(index); }
        [[nodiscard]] const Block& BlockAt(std::size_t index) const
        { return _owner && _materialBlock == index ? _owner->Material : Blocks.at(index); }
        [[nodiscard]] const Block* MaterialBlock() const
        { return _materialBlock ? &BlockAt(*_materialBlock) : nullptr; }
        inline static constexpr std::uint64_t OwnerLimit = 1U << 20;
        std::vector<Block> Blocks;
    private:
        struct Owner final { Block Material{}; std::uint64_t GlobalGeneration = 0; };
        inline static constexpr std::size_t OwnersPerPage = 64;
        using OwnerPage = std::array<Owner, OwnersPerPage>;
        std::vector<std::unique_ptr<OwnerPage>> _owners;
        std::optional<std::size_t> _materialBlock;
        Owner* _owner = nullptr;
        std::uint64_t _ownerIdentity = 0, _globalMaterialGeneration = 1;
        // The two members every material owner inherits from the program (block, offset).
        std::optional<std::pair<std::uint32_t, std::uint32_t>> _celBands, _toonTable;
        // Constant sizes, so the compiler compares and copies in registers
        // instead of calling memcmp/memcpy for every constant a draw sets.
        static bool SameBytes(const std::byte* destination, const void* source, std::size_t size) noexcept
        {
            switch (size)
            {
            case 4: return std::memcmp(destination, source, 4) == 0;
            case 12: return std::memcmp(destination, source, 12) == 0;
            case 16: return std::memcmp(destination, source, 16) == 0;
            case 64: return std::memcmp(destination, source, 64) == 0;
            default: return std::memcmp(destination, source, size) == 0;
            }
        }
        static void CopyBytes(std::byte* destination, const void* source, std::size_t size) noexcept
        {
            switch (size)
            {
            case 4: std::memcpy(destination, source, 4); break;
            case 12: std::memcpy(destination, source, 12); break;
            case 16: std::memcpy(destination, source, 16); break;
            case 64: std::memcpy(destination, source, 64); break;
            default: std::memcpy(destination, source, size); break;
            }
        }
        static std::uint32_t ValueSize(SceneShaderAbi::ValueType type)
        {
            using SceneShaderAbi::ValueType;
            switch (type)
            {
            case ValueType::Bool: case ValueType::Int: case ValueType::Float: return 4;
            case ValueType::Vec3: return 12;
            case ValueType::Vec4: return 16;
            case ValueType::Mat4: return 64;
            }
            throw std::invalid_argument("Unknown Vulkan scene value type.");
        }
        std::unordered_map<std::string_view, MemberDesc> Members;
        std::array<std::optional<MemberDesc>, SceneShaderAbi::Constants.size()> _denseMembers{};
    };
}

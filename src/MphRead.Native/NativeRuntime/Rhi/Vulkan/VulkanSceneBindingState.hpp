#pragma once
#include "../SceneShaderAbi.hpp"
#include "../SceneShaders.hpp"
#include <array>
#include <cstdint>
#include <utility>
#include <memory>
#include <stdexcept>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // One logical program's state in a native recording generation. Direct
    // arrays retain unchanged groups across program switches. Native handles
    // never survive a recording reset or a replaced program identity.
    template<class Slice, class Descriptor, class SampledTexture>
    struct VulkanSceneBindingState final
    {
        std::uint64_t Identity = 0, Recording = 0, MaterialOwner = 0;
        std::array<std::pair<Slice, std::uint64_t>, SceneShaderAbi::Bindings.size()> Blocks{};
        std::array<Descriptor, SceneShaderAbi::GroupCount> Sets{};
        std::array<SampledTexture, 4> Textures{};
        std::array<std::uint64_t, SceneShaderAbi::GroupCount> UniformVersions{};

        bool Prepare(std::uint64_t identity, std::uint64_t recording)
        {
            if (Identity == identity && Recording == recording) return false;
            *this = {}; Identity = identity; Recording = recording;
            return true;
        }
        bool SelectMaterialOwner(std::uint64_t identity) noexcept
        {
            if (MaterialOwner == identity) return false;
            MaterialOwner = identity; return true;
        }
        bool SelectTexture(std::uint32_t unit, std::uint32_t group, const SampledTexture& texture)
        {
            auto& previous = Textures.at(unit);
            if (previous == texture) return false;
            // Validate the group before changing the stored texture identity.
            Sets.at(group) = {}; previous = texture;
            return true;
        }
    };

    // Bounded direct lookup for non-adjacent uses of an unchanged texture group
    // (e.g. HUD icons alternating atlas textures). One CPU allocation at stream
    // construction; collisions replace only CPU lookup data, never a GPU set.
    // Uniform versions describe uploaded slices, not uniform bytes or hashes.
    template<class Descriptor, class SampledTexture>
    class VulkanSceneDescriptorState final
    {
        static constexpr std::size_t TextureSlots = 512;
        static constexpr std::size_t ProgramCount = static_cast<std::size_t>(SceneProgram::Backdrop) + 1;
        struct Entry final
        {
            std::uint64_t Texture = 0, Program = 0, Recording = 0, Epoch = 0, Uniform = 0;
            std::array<SampledTexture, 4> Textures{};
            Descriptor Set{};
        };
        using Entries = std::array<Entry, ProgramCount * SceneShaderAbi::GroupCount * TextureSlots>;
        std::unique_ptr<Entries> _entries = std::make_unique<Entries>();
        Entry& At(SceneProgram program, std::uint32_t group, std::uint64_t texture)
        {
            if (group >= SceneShaderAbi::GroupCount) throw std::out_of_range("Invalid scene descriptor group.");
            return _entries->at((static_cast<std::size_t>(program) * SceneShaderAbi::GroupCount + group)
                * TextureSlots + texture % TextureSlots);
        }
    public:
        Descriptor Find(SceneProgram program, std::uint32_t group, std::uint64_t texture,
            std::uint64_t identity, std::uint64_t recording, std::uint64_t epoch,
            std::uint64_t uniform, const std::array<SampledTexture, 4>& textures)
        {
            const auto& entry = At(program, group, texture);
            return entry.Texture == texture && entry.Program == identity && entry.Recording == recording
                && entry.Epoch == epoch && entry.Uniform == uniform && entry.Textures == textures ? entry.Set : Descriptor{};
        }
        void Store(SceneProgram program, std::uint32_t group, std::uint64_t texture,
            std::uint64_t identity, std::uint64_t recording, std::uint64_t epoch,
            std::uint64_t uniform, const std::array<SampledTexture, 4>& textures, Descriptor set)
        { At(program, group, texture) = {texture, identity, recording, epoch, uniform, textures, set}; }
    };
}

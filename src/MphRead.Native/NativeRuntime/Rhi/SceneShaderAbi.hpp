#pragma once

#include "Bindings.hpp"
#include "ShaderConstants.hpp"
#include "VertexSemantics.hpp"

#include <array>
#include <cstdint>
#include <string_view>

// Logical scene inputs. A group is an update/lifetime boundary, not a Vulkan
// descriptor set or a byte packing convention. API adapters choose physical
// indices and pack ShaderConstants into their own representation.
namespace MphRead::NativeRuntime::Rhi::SceneShaderAbi
{
    enum class Group : std::uint8_t { Frame, Material, Draw, Post, Count };
    inline constexpr std::uint32_t GroupCount = static_cast<std::uint32_t>(Group::Count);

    struct Binding final
    {
        Group group;
        std::uint32_t binding;
        BindingType type;
        std::string_view semantic;

        bool operator==(const Binding&) const = default;
    };

    inline constexpr Binding Frame{Group::Frame, 0, BindingType::UniformBuffer, "frame"};
    inline constexpr Binding Light{Group::Frame, 1, BindingType::UniformBuffer, "light"};
    inline constexpr Binding Fog{Group::Frame, 2, BindingType::UniformBuffer, "fog"};
    inline constexpr Binding Material{Group::Material, 0, BindingType::UniformBuffer, "material"};
    inline constexpr Binding MaterialTexture{Group::Material, 1, BindingType::SampledTexture, "material-texture"};
    inline constexpr Binding MaterialSampler{Group::Material, 2, BindingType::Sampler, "material-sampler"};
    inline constexpr Binding Draw{Group::Draw, 0, BindingType::UniformBuffer, "draw"};
    inline constexpr Binding Cel{Group::Post, 0, BindingType::UniformBuffer, "cel"};
    inline constexpr Binding Hud{Group::Post, 1, BindingType::UniformBuffer, "hud"};
    inline constexpr Binding Disruption{Group::Post, 2, BindingType::UniformBuffer, "disruption"};
    inline constexpr Binding DisruptionTables{Group::Post, 3, BindingType::UniformBuffer, "disruption-tables"};
    inline constexpr std::array Bindings{Frame, Light, Fog, Material, MaterialTexture,
        MaterialSampler, Draw, Cel, Hud, Disruption, DisruptionTables};

    [[nodiscard]] constexpr bool IsValid() noexcept
    {
        for (std::size_t i = 0; i < Bindings.size(); ++i)
        {
            if (Bindings[i].group >= Group::Count || Bindings[i].semantic.empty()) return false;
            for (std::size_t j = 0; j < i; ++j)
                if ((Bindings[i].group == Bindings[j].group && Bindings[i].binding == Bindings[j].binding)
                    || Bindings[i].semantic == Bindings[j].semantic) return false;
        }
        return true;
    }
    static_assert(IsValid());
}

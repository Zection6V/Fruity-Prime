#include "FruityVulkanSceneShaders.hpp"
#include "../NativeRuntime/Rhi/CommandList.hpp"
#include "../NativeRuntime/Rhi/Vulkan/VulkanSceneBindingState.hpp"
#include <iostream>
#include <cstdlib>
#include <new>

namespace { std::size_t allocations = 0; }
void* operator new(std::size_t size)
{
    if (void* result = std::malloc(size ? size : 1)) { ++allocations; return result; }
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace
{
    using namespace MphRead::NativeRuntime::Rhi;
    using Store = Vulkan::VulkanSceneUniforms;
    using SceneShaderAbi::ValueType;
    void Expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void BindingVersions()
    {
        struct Slice { std::uint64_t Buffer = 0, Offset = 0; };
        struct Texture
        {
            std::uint64_t Identity = 0, Generation = 0, Sampler = 0, View = 0, Layout = 0;
            bool operator==(const Texture&) const = default;
        };
        using State = Vulkan::VulkanSceneBindingState<Slice, std::uint64_t, Texture>;
        std::array<State, 2> states{};
        auto& main = states[0];
        const auto material = static_cast<std::uint32_t>(SceneShaderAbi::Group::Material);
        Expect(main.Prepare(100, 1), "Initial recording was not admitted.");
        main.Blocks[0] = {{7, 64}, 9}; main.Sets[material] = 11;
        Expect(states[1].Prepare(101, 1) && !main.Prepare(100, 1)
            && main.Sets[material] == 11 && main.Blocks[0].first.Offset == 64,
            "Switching programs discarded an unchanged group/slice.");
        Expect(main.SelectTexture(0, material, {20, 1, 30, 40, 5}) && !main.Sets[material], "Changed texture retained its group.");
        main.Sets[material] = 12;
        Expect(!main.SelectTexture(0, material, {20, 1, 30, 40, 5}) && main.Sets[material] == 12, "Unchanged texture dirtied descriptors.");
        Expect(main.SelectTexture(0, material, {20, 2, 30, 41, 5}) && !main.Sets[material], "Replaced native view/generation kept descriptors.");
        Expect(main.SelectMaterialOwner(55) && !main.SelectMaterialOwner(55) && main.SelectMaterialOwner(56), "Material owner changes lost identity.");
        Expect(main.Prepare(100, 2) && !main.Sets[material] && !main.Blocks[0].first.Buffer && !main.MaterialOwner,
            "Recording reset retained a descriptor, upload or owner.");
        main.Sets[material] = 13;
        Expect(main.Prepare(102, 2) && !main.Sets[material], "Replacement program retained native state.");

        Vulkan::VulkanSceneDescriptorState<std::uint64_t, Texture> cache;
        std::array<Texture, 4> textures{{{20, 1, 30, 40, 5}}};
        cache.Store(SceneProgram::Main, material, 20, 102, 2, 1, 7, textures, 300);
        const auto find = [&](std::uint64_t program, std::uint64_t recording, std::uint64_t epoch, std::uint64_t uniform) {
            return cache.Find(SceneProgram::Main, material, 20, program, recording, epoch, uniform, textures);
        };
        Expect(find(102, 2, 1, 7) == 300 && !find(103, 2, 1, 7) && !find(102, 3, 1, 7)
            && !find(102, 2, 2, 7) && !find(102, 2, 1, 8), "Descriptor lifetime/upload version validation failed.");
        textures[0].Sampler = 31; Expect(!find(102, 2, 1, 7), "Changed sampler retained descriptor state.");
        textures[0].Sampler = 30; textures[0].Layout = 6; Expect(!find(102, 2, 1, 7), "Changed layout retained descriptor state.");
        textures[0].Layout = 5;
        cache.Store(SceneProgram::Main, material, 20 + 512, 102, 2, 1, 7, textures, 301);
        Expect(!find(102, 2, 1, 7), "Direct-table collision returned another texture's descriptor.");
        const auto before = allocations;
        for (unsigned i = 0; i < 10000; ++i)
        {
            const auto texture = 20 + i % 2;
            textures[0].Identity = texture;
            main.SelectTexture(0, material, textures[0]);
            cache.Store(SceneProgram::Main, material, texture, 102, 2, 1, 7, textures, 300 + i % 2);
            Expect(cache.Find(SceneProgram::Main, material, texture, 102, 2, 1, 7, textures) == 300 + i % 2,
                "Alternating unchanged atlas bindings lost descriptor reuse.");
        }
        Expect(allocations == before, "Steady binding/descriptor state allocated CPU heap storage.");
    }
    template<class F> void Reject(F action)
    {
        bool rejected = false;
        try { action(); } catch (const std::invalid_argument&) { rejected = true; }
        Expect(rejected, "Malformed packing request was accepted.");
    }
    std::size_t FloatCount(ValueType type)
    {
        switch (type) { case ValueType::Vec3: return 3; case ValueType::Vec4: return 4; case ValueType::Mat4: return 16; default: return 1; }
    }
    template<class Blocks, class Members> void CheckProgram(const Blocks& blocks, const Members& members)
    {
        Store store(blocks, members);
        for (const auto& member : members)
        {
            const auto before = store.Blocks;
            const auto elements = member.count ? member.count + 3 : 1;
            const auto width = FloatCount(member.type);
            std::vector<float> values(elements * width);
            for (std::size_t i = 0; i < values.size(); ++i) values[i] = float(i + 1);
            const std::int32_t integer = member.type == ValueType::Bool ? 1 : 7;
            const auto write = [&] {
                if (member.count) store.WriteArray(member.name, values.data(), width, elements);
                else if (member.type == ValueType::Bool || member.type == ValueType::Int)
                    store.Write(member.name, &integer, 4, ValueType::Int);
                else store.Write(member.name, values.data(), width * sizeof(float), member.type);
            };
            write();
            for (std::size_t i = 0; i < store.Blocks.size(); ++i)
            {
                Expect(store.Blocks[i].Generation == before[i].Generation + (i == member.block ? 1 : 0), "Packing dirtied another block.");
                if (i != member.block) Expect(store.Blocks[i].Data == before[i].Data, "Packing changed another block's bytes.");
            }
            const auto& actual = store.Blocks[member.block].Data;
            const auto stride = member.count ? member.size / member.count : member.size;
            const auto count = member.count ? member.count : 1;
            for (unsigned i = 0; i < count; ++i)
            {
                const auto* expected = member.type == ValueType::Bool || member.type == ValueType::Int
                    ? reinterpret_cast<const std::byte*>(&integer) : reinterpret_cast<const std::byte*>(values.data() + i * width);
                Expect(std::memcmp(actual.data() + member.offset + i * stride, expected, width * sizeof(float)) == 0,
                    "Generated offset/array stride/matrix order changed values.");
                for (std::size_t j = width * sizeof(float); j < stride; ++j)
                    Expect(actual[member.offset + i * stride + j] == std::byte{0}, "Array packing wrote std140 padding.");
            }
            for (std::size_t i = 0; i < actual.size(); ++i)
                if (i < member.offset || i >= member.offset + member.size)
                    Expect(actual[i] == before[member.block].Data[i], "Packing overwrote a neighboring member.");
            const auto generation = store.Blocks[member.block].Generation;
            write(); Expect(store.Blocks[member.block].Generation == generation, "Unchanged values invalidated upload reuse.");
            const auto slot = SceneShaderAbi::ConstantIndexOf(member.name);
            Expect(slot < SceneShaderAbi::Constants.size(), "Generated semantic has no dense slot.");
            const auto beforeAllocations = allocations;
            for (unsigned repeat = 0; repeat < 100; ++repeat)
            {
                if (member.count) store.WriteArray(slot, values.data(), width, elements);
                else if (member.type == ValueType::Bool || member.type == ValueType::Int)
                    store.Write(slot, &integer, 4, ValueType::Int);
                else store.Write(slot, values.data(), width * sizeof(float), member.type);
            }
            Expect(store.Blocks[member.block].Generation == generation && allocations == beforeAllocations,
                "Dense constant writes changed values or allocated steady storage.");
            if (member.count)
            {
                Reject([&] { store.WriteArray(member.name, values.data(), width + 1, elements); });
                Reject([&] { store.Write(member.name, values.data(), member.size, member.type); });
            }
            else Reject([&] { store.Write(member.name, values.data(), member.size + 1, member.type); });
        }
        store.Write("not-active-on-this-program", nullptr, 0, ValueType::Float);
        for (std::size_t slot = 0; slot < SceneShaderAbi::Constants.size(); ++slot)
            if (!store.Find(SceneShaderAbi::Constants[slot].name)) store.Write(slot, nullptr, 0, ValueType::Float);
    }
    void MaterialOwners()
    {
        Store store(Vulkan::Generated::main_blocks, Vulkan::Generated::main_uniforms);
        const float red[3]{1, 0, 0}, green[3]{0, 1, 0};
        const auto material = store.Find("diffuse")->block;
        store.SelectMaterialOwner(1);
        store.Write("diffuse", red, sizeof(red), ValueType::Vec3);
        const auto redGeneration = store.BlockAt(material).Generation;
        const auto* redStorage = store.BlockAt(material).Data.data();
        store.SelectMaterialOwner(70); // a second page must not invalidate an owner
        store.Write("diffuse", green, sizeof(green), ValueType::Vec3);
        store.SelectMaterialOwner(1);
        Expect(store.BlockAt(material).Data.data() == redStorage, "Owner storage moved on another owner's admission.");
        store.Write("diffuse", red, sizeof(red), ValueType::Vec3);
        Expect(store.BlockAt(material).Generation == redGeneration, "Selecting an unchanged owner dirtied its material.");
        Expect(std::memcmp(store.BlockAt(material).Data.data() + store.Find("diffuse")->offset,
            red, sizeof(red)) == 0, "An owner's material inherited another owner's values.");
        store.SelectMaterialOwner(0);
        const std::int32_t bands = 4;
        store.Write("cel_bands", &bands, sizeof(bands), ValueType::Int);
        store.SelectMaterialOwner(1);
        Expect(store.BlockAt(material).Generation == redGeneration + 1, "Program-wide cel changes did not dirty an owner.");
        const auto propagated = store.BlockAt(material).Generation;
        store.SelectMaterialOwner(0); store.SelectMaterialOwner(1);
        Expect(store.BlockAt(material).Generation == propagated, "Unchanged global constants repeatedly dirtied an owner.");
        store.Write("diffuse", green, sizeof(green), ValueType::Vec3);
        Expect(store.BlockAt(material).Generation == propagated + 1, "Owner mutation did not advance its generation.");
        store.SelectMaterialOwner(Store::OwnerLimit);
        Expect(store.MaterialOwnerIdentity() == 0 && &store.BlockAt(material) == &store.Blocks[material],
            "Out-of-budget identities did not preserve the ordinary uniform path.");
    }
    void InvalidMetadataAndWrites()
    {
        Store store(Vulkan::Generated::main_blocks, Vulkan::Generated::main_uniforms);
        const auto* alpha = store.Find("mat_alpha");
        const auto* test = store.Find("alpha_test");
        Expect(alpha && test && alpha->block == test->block && alpha->offset == offsetof(CommandList::SmallDrawConstants, materialAlpha)
            && test->offset == offsetof(CommandList::SmallDrawConstants, alphaTest)
            && store.Blocks[alpha->block].Small && store.Blocks[alpha->block].Data.size() == sizeof(CommandList::SmallDrawConstants)
            && sizeof(CommandList::SmallDrawConstants) <= 128, "Portable small draw ABI drift.");
        Expect(!store.Blocks[store.Find("mtx_stack")->block].Small, "Large matrix stack became inline constants.");
        float value = 0.5F;
        Reject([&] { store.Write("use_light", &value, 4, ValueType::Float); });
        std::int32_t invalidBoolean = 7;
        Reject([&] { store.Write("use_light", &invalidBoolean, 4, ValueType::Int); });
        Reject([&] { store.Write("use_light", nullptr, 4, ValueType::Int); });
        auto members = Vulkan::Generated::main_uniforms;
        members[0].offset = UINT32_MAX;
        Reject([&] { Store bad(Vulkan::Generated::main_blocks, members); });
        members = Vulkan::Generated::main_uniforms; members[0].block = UINT32_MAX;
        Reject([&] { Store bad(Vulkan::Generated::main_blocks, members); });
        members = Vulkan::Generated::main_uniforms; members[0].size += 4;
        Reject([&] { Store bad(Vulkan::Generated::main_blocks, members); });
        members = Vulkan::Generated::main_uniforms; members[1].name = members[0].name;
        Reject([&] { Store bad(Vulkan::Generated::main_blocks, members); });
        auto blocks = Vulkan::Generated::main_blocks; blocks[0].size -= 1;
        Reject([&] { Store bad(blocks, Vulkan::Generated::main_uniforms); });
        blocks = Vulkan::Generated::main_blocks; blocks[1] = blocks[0];
        Reject([&] { Store bad(blocks, Vulkan::Generated::main_uniforms); });
    }
}
int main()
{
    try
    {
        CheckProgram(Vulkan::Generated::main_blocks, Vulkan::Generated::main_uniforms);
        CheckProgram(Vulkan::Generated::composite_blocks, Vulkan::Generated::composite_uniforms);
        CheckProgram(Vulkan::Generated::cel_blocks, Vulkan::Generated::cel_uniforms);
        CheckProgram(Vulkan::Generated::shift_blocks, Vulkan::Generated::shift_uniforms);
        CheckProgram(Vulkan::Generated::backdrop_blocks, Vulkan::Generated::backdrop_uniforms);
        InvalidMetadataAndWrites();
        MaterialOwners();
        BindingVersions();
        std::cout << "Production Vulkan uniform packing: all five programs, 55 constants, per-block generations PASS\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

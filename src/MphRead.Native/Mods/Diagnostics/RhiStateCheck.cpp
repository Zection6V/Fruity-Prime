#include "RhiStateCheck.hpp"
#include "../../NativeRuntime/Rhi/GraphicsDevice.hpp"
#include "../../NativeRuntime/Rhi/CommandList.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace MphRead::Mods::Diagnostics
{
    void CheckRhiResourceStates(NativeRuntime::Rhi::GraphicsDevice& device)
    {
        using namespace NativeRuntime::Rhi;
        unsigned rejected = 0;
        const auto reject = [&](auto action) {
            bool caught = false; const auto before = device.Statistics();
            try { action(); } catch (const std::invalid_argument&) { caught = true; }
            if (!caught || device.DrainErrors()) throw std::runtime_error("Invalid resource state reached the native API.");
            const auto after = device.Statistics();
            if (before.Submitted != after.Submitted || before.HostWaits != after.HostWaits
                || before.DeviceWideWaits != after.DeviceWideWaits || before.LiveObjects() != after.LiveObjects())
                throw std::runtime_error("Invalid state changed submissions, waits or native ownership.");
            ++rejected;
        };
        constexpr auto unknown = static_cast<ResourceState>(1U << 31);
        TextureDesc desc{}; desc.width = desc.height = 2; desc.format = TextureFormat::RGBA8Unorm;
        desc.usage = TextureUsage::Sampled | TextureUsage::TransferSrc | TextureUsage::TransferDst;
        for (const auto state : {unknown, ResourceState::CopyDst | ResourceState::ShaderRead,
            ResourceState::VertexBuffer, ResourceState::ConstantBuffer})
        { auto invalid = desc; invalid.initialState = state; reject([&] { (void)device.CreateTexture(invalid); }); }
        for (const auto state : {unknown, ResourceState::CopyDst | ResourceState::CopySrc,
            ResourceState::ColorAttachment, ResourceState::DepthStencilRead, ResourceState::Present})
            reject([&] { (void)device.CreateBuffer({16, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, state}); });
        for (const auto state : {ResourceState::VertexBuffer, ResourceState::IndexBuffer, ResourceState::ConstantBuffer,
            ResourceState::ShaderRead, ResourceState::ShaderWrite, ResourceState::CopyDst})
            reject([&] { (void)device.CreateBuffer({16, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, state}); });
        for (const auto state : {ResourceState::ColorAttachment, ResourceState::DepthStencilRead,
            ResourceState::DepthStencilWrite, ResourceState::ShaderWrite, ResourceState::Present})
        { auto invalid = desc; invalid.initialState = state; reject([&] { (void)device.CreateTexture(invalid); }); }
        for (const auto state : {ResourceState::CopySrc, ResourceState::CopyDst})
        { auto invalid = desc; invalid.usage = TextureUsage::Sampled; invalid.initialState = state;
            reject([&] { (void)device.CreateTexture(invalid); }); }
        for (const auto format : {TextureFormat::RGBA8Unorm, TextureFormat::D24UnormS8Uint})
        { auto invalid = desc; invalid.format = format;
            invalid.usage = format == TextureFormat::RGBA8Unorm ? TextureUsage::DepthStencilAttachment : TextureUsage::ColorAttachment;
            reject([&] { (void)device.CreateTexture(invalid); }); }
        reject([&] { auto invalid = desc; invalid.usage = static_cast<TextureUsage>(1U << 31); (void)device.CreateTexture(invalid); });
        reject([&] { (void)device.CreateBuffer({16, static_cast<BufferUsage>(1U << 31)}); });

        auto source = device.CreateBuffer({16, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu, ResourceState::Common});
        auto output = device.CreateBuffer({16, BufferUsage::TransferDst, MemoryUsage::GpuToCpu});
        auto image = device.CreateTexture(desc);
        std::array<std::byte, 16> payload{}, actual{};
        for (unsigned i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::byte>(17 * i + 3);
        device.WriteBuffer(*source, 0, payload); // Host writes preserve Common.
        auto commands = device.CreateCommandList(); commands->Begin();
        BufferTextureCopy region{}; region.width = region.height = 2;
        reject([&] { commands->CopyBuffer(*source, 0, *output, 0, 16); });
        reject([&] { commands->CopyBufferToTexture(*source, *image, region); });
        reject([&] { commands->CopyTextureToBuffer(*image, *output, region); });
        reject([&] { commands->Transition(*source, ResourceState::Undefined, ResourceState::CopySrc); });
        for (const auto state : {ResourceState::VertexBuffer, ResourceState::IndexBuffer, ResourceState::ConstantBuffer,
            ResourceState::ShaderRead, ResourceState::ShaderWrite, ResourceState::CopyDst})
            reject([&] { commands->Transition(*source, ResourceState::Common, state); });
        for (const auto state : {unknown, ResourceState::Common, ResourceState::ColorAttachment,
            ResourceState::CopyDst | ResourceState::ShaderRead})
            reject([&] { commands->Transition(*source, ResourceState::Common, state); });
        commands->Transition(*source, ResourceState::Common, ResourceState::CopySrc);
        commands->Transition(*output, ResourceState::Undefined, ResourceState::CopyDst);
        for (const auto state : {unknown, ResourceState::Undefined, ResourceState::VertexBuffer,
            ResourceState::ShaderRead | ResourceState::ShaderWrite})
            reject([&] { commands->Transition(*image, ResourceState::Undefined, state); });
        for (const auto state : {ResourceState::ColorAttachment, ResourceState::DepthStencilRead,
            ResourceState::DepthStencilWrite, ResourceState::ShaderWrite, ResourceState::Present})
            reject([&] { commands->Transition(*image, ResourceState::Undefined, state); });
        commands->Transition(*image, ResourceState::Undefined, ResourceState::CopyDst);
        reject([&] { commands->Transition(*image, ResourceState::Undefined, ResourceState::CopySrc); });
        commands->CopyBufferToTexture(*source, *image, region);
        reject([&] { commands->CopyTextureToBuffer(*image, *output, region); });
        commands->Transition(*image, ResourceState::CopyDst, ResourceState::ShaderRead | ResourceState::CopySrc);
        commands->Transition(*image, ResourceState::ShaderRead | ResourceState::CopySrc, ResourceState::CopySrc);
        commands->CopyTextureToBuffer(*image, *output, region); commands->End();
        device.ReadBuffer(*output, 0, actual);
        if (actual != payload) throw std::runtime_error("Rejected state transitions damaged subsequent GPU copies.");

        auto other = device.CreateCommandList(); other->Begin();
        other->Transition(*source, ResourceState::CopySrc, ResourceState::Common); other->End();
        commands->Begin(); reject([&] { commands->Transition(*source, ResourceState::CopySrc, ResourceState::Common); });
        commands->Transition(*source, ResourceState::Common, ResourceState::CopySrc); commands->End();
        auto initializedDesc = desc; initializedDesc.initialState = ResourceState::ShaderRead;
        auto initialized = device.CreateTexture(initializedDesc);
        commands->Begin(); commands->Transition(*initialized, ResourceState::ShaderRead, ResourceState::CopyDst); commands->End();
        auto sampledDesc = desc; sampledDesc.usage = TextureUsage::Sampled;
        auto sampled = device.CreateTexture(sampledDesc);
        commands->Begin();
        for (const auto state : {ResourceState::CopySrc, ResourceState::CopyDst})
            reject([&] { commands->Transition(*sampled, ResourceState::Undefined, state); });
        commands->Transition(*sampled, ResourceState::Undefined, ResourceState::ShaderRead); commands->End();

        auto storageDesc = desc; storageDesc.usage = TextureUsage::Storage | TextureUsage::TransferSrc | TextureUsage::TransferDst;
        storageDesc.initialState = ResourceState::ShaderRead;
        auto storage = device.CreateTexture(storageDesc);
        commands->Begin(); commands->Transition(*storage, ResourceState::ShaderRead, ResourceState::CopyDst);
        commands->CopyBufferToTexture(*source, *storage, region);
        commands->Transition(*storage, ResourceState::CopyDst, ResourceState::ShaderWrite);
        commands->Transition(*storage, ResourceState::ShaderWrite, ResourceState::ShaderRead);
        commands->Transition(*storage, ResourceState::ShaderRead, ResourceState::CopySrc);
        commands->CopyTextureToBuffer(*storage, *output, region); commands->End();
        device.ReadBuffer(*output, 0, actual);
        if (actual != payload) throw std::runtime_error("Storage texture state transitions changed transferred pixels.");

        device.ResizeTexture(*image, 2, 2); // Same extent retains its state.
        commands->Begin(); commands->Transition(*image, ResourceState::CopySrc, ResourceState::Common); commands->End();
        device.ResizeTexture(*image, 3, 2); // New storage starts Undefined.
        commands->Begin(); reject([&] { commands->Transition(*image, ResourceState::Common, ResourceState::CopyDst); });
        commands->Transition(*image, ResourceState::Undefined, ResourceState::CopyDst); commands->End();
        std::array<std::byte, 24> pixels{}; device.WriteTexture(*image, {3, 2, TextureFormat::RGBA8Unorm, pixels.data()});
        commands->Begin(); commands->Transition(*image, ResourceState::ShaderRead, ResourceState::Common); commands->End();
        device.WriteTexture(*image, {3, 2, TextureFormat::RGBA8Unorm, pixels.data()}); // Preserve established Common.
        commands->Begin(); commands->Transition(*image, ResourceState::Common, ResourceState::ShaderRead); commands->End();

        auto gpu = device.CreateBuffer({16, BufferUsage::TransferDst | BufferUsage::TransferSrc, MemoryUsage::GpuOnly});
        device.WriteBuffer(*gpu, 0, payload);
        commands->Begin(); commands->Transition(*gpu, ResourceState::CopyDst, ResourceState::CopySrc);
        commands->CopyBuffer(*gpu, 0, *output, 0, 16); commands->End(); device.ReadBuffer(*output, 0, actual);
        if (actual != payload || device.DrainErrors()) throw std::runtime_error("GPU upload state or pixels differ.");
        std::cout << "[resource states] PASS; rejected=" << rejected
            << "; initial/tracked state; invalid bits/type/write/usage/format; copies preserved; resize/upload/storage states\n";
    }
}

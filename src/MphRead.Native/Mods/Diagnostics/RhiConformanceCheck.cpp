#include "RhiConformanceCheck.hpp"
#include "../../NativeRuntime/Rhi/OpenGL/OpenGlDiagnostics.hpp"
#include "../../NativeRuntime/Rhi/OpenGL/OpenGlDevice.hpp"
#include "../Branding.hpp"
#include "../../Renderer.hpp"
#include "../../NativeRuntime/Rhi/BackendSession.hpp"
#include "../../Testing/RhiConformanceShaderSource.hpp"
#if defined(FRUITY_HAS_VULKAN)
#include "FruityRhiConformanceShaders.hpp"
#endif
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    namespace
    {
        namespace Rhi = NativeRuntime::Rhi;
        void Expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
        template <class T> auto Bytes(const T& values) { return std::as_bytes(std::span(values)); }
        Rhi::ShaderDesc Shader(Rhi::GraphicsBackend backend, Rhi::ShaderStage stage)
        {
            Rhi::ShaderDesc desc{};
            desc.stage = stage;
            if (backend == Rhi::GraphicsBackend::OpenGl)
            {
                desc.format = Rhi::ShaderCodeFormat::GlslSource;
                const auto source = stage == Rhi::ShaderStage::Vertex ? Rhi::TestingShaderAssets::Vertex : Rhi::TestingShaderAssets::Fragment;
                desc.code.resize(source.size());
                std::memcpy(desc.code.data(), source.data(), source.size());
            }
            else
            {
#if defined(FRUITY_HAS_VULKAN)
                const auto words = stage == Rhi::ShaderStage::Vertex
                    ? std::span<const std::uint32_t>(Rhi::TestingShaderAssets::vert) : std::span<const std::uint32_t>(Rhi::TestingShaderAssets::frag);
                const auto bytes = std::as_bytes(words);
                desc.code.assign(bytes.begin(), bytes.end());
#else
                throw std::runtime_error("Vulkan shader fixture unavailable in this build.");
#endif
            }
            return desc;
        }
        void ExerciseUnframedLifetime(Rhi::GraphicsDevice& device)
        {
            using namespace Rhi;
            const auto baseline = device.Statistics();
            for (unsigned cycle = 0; cycle < 16; ++cycle)
            {
                auto commands = device.CreateCommandList();
                auto source = device.CreateBuffer({16, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu});
                auto destination = device.CreateBuffer({16, BufferUsage::TransferDst, MemoryUsage::GpuToCpu});
                const std::array<unsigned, 4> payload{cycle, 0xAABBCCDD, cycle * 37, 0x10203040};
                device.WriteBuffer(*source, 0, Bytes(payload));
                commands->Begin();
                commands->Transition(*source, ResourceState::Undefined, ResourceState::CopySrc);
                commands->Transition(*destination, ResourceState::Undefined, ResourceState::CopyDst);
                commands->CopyBuffer(*source, 0, *destination, 0, 16); commands->End();
                std::array<unsigned, 4> result{};
                device.ReadBuffer(*destination, 0, std::as_writable_bytes(std::span(result)));
                Expect(result == payload, "Unframed transfer/readback contents differ.");
                const auto beforeRelease = device.Statistics();
                source.reset(); destination.reset(); commands.reset();
                const auto afterRelease = device.Statistics();
                Expect(afterRelease.DeviceWideWaits == beforeRelease.DeviceWideWaits
                    && afterRelease.HostWaits == beforeRelease.HostWaits,
                    "Ordinary unframed resource release waited for the GPU.");
            }
            const auto submitted = device.Statistics();
            Expect(submitted.Submitted > baseline.Submitted && submitted.CompletedFrame == baseline.CompletedFrame,
                "Unframed work must advance submission serials independently of frames.");
            device.WaitIdle();
            const auto completed = device.Statistics();
            Expect(completed.Completed == completed.Submitted && completed.Retired == 0 && completed.LiveObjects() == 0,
                "Explicit idle must complete unframed work and release its native resources.");
        }
        struct HeldResources final
        {
            std::vector<std::shared_ptr<void>> Leases;
            Rhi::CommandList* Commands = nullptr;
            Rhi::GraphicsPipeline* Pipeline = nullptr;
            Rhi::BindingLayout* Layout = nullptr;
            Rhi::BindingSet* Set = nullptr;
            Rhi::Texture* Texture = nullptr;
            template <typename T> void Keep(std::unique_ptr<T> object) { Leases.emplace_back(std::move(object)); }
        };

        void Exercise(Rhi::GraphicsDevice& device, HeldResources* held = nullptr)
        {
            using namespace Rhi;
            auto commands = device.CreateCommandList();
            // Byte-range GPU copy with distinct source/destination offsets.
            BufferDesc uploadDesc{64, BufferUsage::TransferSrc, MemoryUsage::CpuToGpu};
            BufferDesc readbackDesc{64, BufferUsage::TransferDst, MemoryUsage::GpuToCpu};
            auto upload = device.CreateBuffer(uploadDesc), readback = device.CreateBuffer(readbackDesc);
            std::array<std::byte, 32> payload{};
            for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::byte>(i * 19 + 7);
            device.WriteBuffer(*upload, 5, payload);
            commands->Begin();
            commands->Transition(*upload, ResourceState::Undefined, ResourceState::CopySrc);
            commands->Transition(*readback, ResourceState::Undefined, ResourceState::CopyDst);
            commands->CopyBuffer(*upload, 5, *readback, 9, payload.size()); commands->End();
            std::array<std::byte, 32> copied{}; device.ReadBuffer(*readback, 9, copied);
            Expect(copied == payload, "GPU buffer copy/readback contents differ.");
            bool rejected = false;
            commands->Begin();
            try { commands->CopyBuffer(*upload, 63, *readback, 0, 2); } catch (const std::out_of_range&) { rejected = true; }
            Expect(rejected, "Out-of-range GPU buffer copy was accepted.");
            commands->End();

            // Pitched image transfer through GPU buffers, preserving padding.
            TextureDesc imageDesc{};
            imageDesc.width = 2; imageDesc.height = 2; imageDesc.format = Rhi::TextureFormat::RGBA8Unorm;
            imageDesc.usage = TextureUsage::Sampled | TextureUsage::TransferSrc | TextureUsage::TransferDst;
            auto image = device.CreateTexture(imageDesc);
            std::array<std::byte, 32> pixels{};
            for (std::size_t i = 0; i < 8; ++i) { pixels[4+i] = static_cast<std::byte>(i*13+1); pixels[16+i] = static_cast<std::byte>(i*17+9); }
            device.WriteBuffer(*upload, 0, pixels);
            commands->Begin(); commands->Transition(*image, ResourceState::Undefined, ResourceState::CopyDst);
            BufferTextureCopy region{}; region.bufferOffset = 4; region.bytesPerRow = 12; region.width = 2; region.height = 2;
            commands->CopyBufferToTexture(*upload, *image, region);
            commands->Transition(*image, ResourceState::CopyDst, ResourceState::CopySrc);
            commands->CopyTextureToBuffer(*image, *readback, region); commands->End();
            std::array<std::byte, 32> imageBytes{}; device.ReadBuffer(*readback, 0, imageBytes);
            Expect(std::memcmp(imageBytes.data()+4, pixels.data()+4, 8) == 0 && std::memcmp(imageBytes.data()+16, pixels.data()+16, 8) == 0,
                "Pitched GPU texture transfer contents differ.");

            auto vertex = device.CreateShader(Shader(device.GetBackend(), ShaderStage::Vertex));
            auto fragment = device.CreateShader(Shader(device.GetBackend(), ShaderStage::Fragment));
            const std::array<float, 6> positions{-1, -1, 3, -1, -1, 3};
            const std::array<std::uint32_t, 4> indices{99, 0, 1, 2};
            BufferDesc verticesDesc{sizeof(positions), BufferUsage::Vertex | BufferUsage::TransferDst, MemoryUsage::CpuToGpu};
            BufferDesc indicesDesc{sizeof(indices), BufferUsage::Index | BufferUsage::TransferDst, MemoryUsage::CpuToGpu};
            BufferDesc uniformDesc{16, BufferUsage::Uniform | BufferUsage::TransferDst, MemoryUsage::CpuToGpu};
            auto vertices = device.CreateBuffer(verticesDesc), index = device.CreateBuffer(indicesDesc);
            auto frame = device.CreateBuffer(uniformDesc), draw = device.CreateBuffer(uniformDesc);
            device.WriteBuffer(*vertices, 0, Bytes(positions)); device.WriteBuffer(*index, 0, Bytes(indices));
            const std::array<float, 4> tint{1, 1, 1, 1}, offset{0, 0, 0, 0};
            device.WriteBuffer(*frame, 0, Bytes(tint)); device.WriteBuffer(*draw, 0, Bytes(offset));
            TextureDesc sampleDesc = imageDesc; sampleDesc.height = 1;
            auto sampled = device.CreateTexture(sampleDesc);
            const std::array<unsigned char, 8> redBlue{255, 0, 0, 255, 0, 0, 255, 255};
            device.WriteTexture(*sampled, {2, 1, Rhi::TextureFormat::RGBA8Unorm, redBlue.data()});
            auto sampleView = device.CreateTextureView(*sampled, {});
            SamplerDesc nearestDesc{}; nearestDesc.minFilter = Filter::Nearest; nearestDesc.magFilter = Filter::Nearest;
            auto nearest = device.CreateSampler(nearestDesc), linear = device.CreateSampler({});
            BindingLayoutDesc frameLayout{{{7, BindingType::UniformBuffer, ShaderStage::Fragment, 1}}};
            // Declaration order is independent of the backend's physical
            // binding order; GL emission flattens by logical binding number.
            BindingLayoutDesc materialLayout{{{10, BindingType::Sampler, ShaderStage::Fragment, 1},
                {4, BindingType::SampledTexture, ShaderStage::Fragment, 1},
                {9, BindingType::Sampler, ShaderStage::Fragment, 1}, {3, BindingType::SampledTexture, ShaderStage::Fragment, 1}}};
            BindingLayoutDesc drawLayout{{{4, BindingType::UniformBuffer, ShaderStage::Vertex, 1}}};
            auto frameGroup = device.CreateBindingLayout(frameLayout), materialGroup = device.CreateBindingLayout(materialLayout), drawGroup = device.CreateBindingLayout(drawLayout);
            auto frameSet = device.CreateBindingSet({frameGroup.get(), {{7, BufferBinding{frame.get(), 0, 16}}}});
            auto drawSet = device.CreateBindingSet({drawGroup.get(), {{4, BufferBinding{draw.get(), 0, 16}}}});
            auto nearestSet = device.CreateBindingSet({materialGroup.get(), {{3, TextureBinding{sampleView.get()}},
                {4, TextureBinding{sampleView.get()}}, {9, SamplerBinding{nearest.get()}}, {10, SamplerBinding{linear.get()}}}});
            auto linearSet = device.CreateBindingSet({materialGroup.get(), {{3, TextureBinding{sampleView.get()}},
                {4, TextureBinding{sampleView.get()}}, {9, SamplerBinding{linear.get()}}, {10, SamplerBinding{nearest.get()}}}});
            GraphicsPipelineDesc pipelineDesc{};
            pipelineDesc.vertexShader = vertex.get(); pipelineDesc.fragmentShader = fragment.get();
            pipelineDesc.pipelineLayout.groups = {frameLayout, materialLayout, drawLayout, {}};
            pipelineDesc.vertexBuffers = {{0, 8, VertexInputRate::Vertex}};
            pipelineDesc.vertexAttributes = {{0, 0, VertexFormat::Float2, 0}};
            pipelineDesc.colorFormats = {Rhi::TextureFormat::RGBA8Unorm}; pipelineDesc.blendAttachments = {{}};
            pipelineDesc.rasterizer.cullMode = CullMode::None;
            auto pipeline = device.CreateGraphicsPipeline(pipelineDesc);
            // Pipeline creation borrows shader inputs. A completed executable
            // must survive the public shader wrappers on either backend.
            vertex.reset(); fragment.reset();
            TextureDesc targetDesc{}; targetDesc.width = 16; targetDesc.height = 16; targetDesc.format = Rhi::TextureFormat::RGBA8Unorm;
            targetDesc.usage = TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
            auto target = device.CreateTexture(targetDesc); auto targetView = device.CreateTextureView(*target, {});
            RenderingColorAttachment color{targetView.get(), LoadOp::Clear, StoreOp::Store, {0, 0, 0, 1}};
            RenderingInfo info{}; info.width = 16; info.height = 16; info.colorAttachments = std::span(&color, 1);
            (void)device.BeginFrame();
            auto render = [&](const BindingSet& material, bool indexed) {
                commands->Begin(); commands->BeginRendering(info); commands->SetPipeline(*pipeline);
                commands->SetViewport({0, 0, 16, 16}); commands->SetVertexBuffer(0, *vertices);
                commands->SetBindingSet(0, *frameSet); commands->SetBindingSet(1, material); commands->SetBindingSet(2, *drawSet);
                if (indexed) { commands->SetIndexBuffer(*index, IndexType::UInt32); commands->DrawIndexed(3, 1, 1); }
                else commands->Draw(3);
                commands->EndRendering(); commands->End();
                std::array<unsigned char, 8> pixel{};
                commands->ReadColor(info, 4, 8, 1, 1, Rhi::TextureFormat::RGBA8Unorm, pixel.data());
                commands->ReadColor(info, 12, 8, 1, 1, Rhi::TextureFormat::RGBA8Unorm, pixel.data() + 4);
                return pixel;
            };
            const auto nearestPixel = render(*nearestSet, false), linearPixel = render(*linearSet, true);
            Expect(nearestPixel[0] < 3 && nearestPixel[2] > 252 && nearestPixel[3] == 255, "Nonindexed draw or nearest sampler contents differ.");
            Expect(linearPixel[0] >= 126 && linearPixel[0] <= 129 && linearPixel[2] >= 126 && linearPixel[2] <= 129 && linearPixel[3] == 255,
                "Indexed draw or independent linear sampler contents differ.");
            Expect(nearestPixel[4] >= 126 && nearestPixel[4] <= 129 && nearestPixel[6] >= 126 && nearestPixel[6] <= 129 && nearestPixel[7] == 255
                && linearPixel[4] < 3 && linearPixel[6] > 252 && linearPixel[7] == 255,
                "The same image did not support two simultaneous sampler states.");
            rejected = false;
            commands->Begin(); commands->SetPipeline(*pipeline);
            try { commands->SetBindingSet(4, *frameSet); } catch (const std::invalid_argument&) { rejected = true; }
            Expect(rejected, "Out-of-range binding group was accepted.");
            commands->End();
            device.EndFrame(); device.WaitIdle();
            Expect(device.DrainErrors() == 0, "RHI conformance raised native graphics errors.");
            if (held)
            {
                held->Commands = commands.get(); held->Pipeline = pipeline.get();
                held->Layout = frameGroup.get(); held->Set = frameSet.get(); held->Texture = target.get();
                held->Keep(std::move(commands)); held->Keep(std::move(pipeline));
                for (auto* value : {&upload, &readback, &vertices, &index, &frame, &draw}) held->Keep(std::move(*value));
                for (auto* value : {&image, &sampled, &target}) held->Keep(std::move(*value));
                for (auto* value : {&sampleView, &targetView}) held->Keep(std::move(*value));
                for (auto* value : {&nearest, &linear}) held->Keep(std::move(*value));
                for (auto* value : {&frameGroup, &materialGroup, &drawGroup}) held->Keep(std::move(*value));
                for (auto* value : {&frameSet, &drawSet, &nearestSet, &linearSet}) held->Keep(std::move(*value));
                held->Keep(device.CreateShader(Shader(device.GetBackend(), ShaderStage::Vertex)));
                held->Keep(device.CreateShader(Shader(device.GetBackend(), ShaderStage::Fragment)));
                targetDesc.format = Rhi::TextureFormat::D24UnormS8Uint;
                targetDesc.usage = TextureUsage::DepthStencilAttachment;
                held->Keep(device.CreateTexture(targetDesc));
            }
        }

        void ExerciseOpenGlSessionLifetime(const Rhi::BackendProvider& provider)
        {
            using namespace Rhi;
            for (unsigned cycle = 0; cycle < 8; ++cycle)
            {
                auto outgoing = provider.CreateSession({});
                HeldResources held;
                auto& oldDevice = outgoing->Device();
                Exercise(oldDevice, &held);
                bool rejected = false;
                auto duplicate = provider.CreateSession({});
                try { (void)duplicate->Device(); } catch (const std::logic_error&) { rejected = true; }
                Expect(rejected, "Two session devices claimed the same OpenGL context.");
                auto released = OpenGL::NativeReleaseCheck(oldDevice);
                const auto oldHandle = held.Texture->Handle();
                const auto textureDesc = held.Texture->Desc();
                // Exercise explicit owner teardown and the window's defensive
                // close path while its session wrapper is still alive.
                if (cycle % 2) OpenGL::ReleaseContextDevice();
                else { outgoing->Shutdown(); outgoing->Shutdown(); }
                released();
                Expect(!held.Texture->Handle(), "Detached texture still exposed its old native name.");
                rejected = false;
                try { held.Commands->Begin(); } catch (const std::logic_error&) { rejected = true; }
                Expect(rejected, "Command recording survived its ended OpenGL session.");

                auto incoming = provider.CreateSession({});
                auto& device = incoming->Device();
                outgoing->Shutdown(); outgoing->Shutdown();
                auto commands = device.CreateCommandList(); commands->Begin();
                rejected = false;
                try { commands->SetPipeline(*held.Pipeline); } catch (const std::invalid_argument&) { rejected = true; }
                Expect(rejected, "Old session pipeline was accepted by a new device.");
                BindingLayoutDesc frameLayout{{{7, BindingType::UniformBuffer, ShaderStage::Fragment, 1}}};
                GraphicsPipelineDesc state{}; state.pipelineLayout.groups = {frameLayout};
                auto pipeline = device.CreateGraphicsPipeline(state); commands->SetPipeline(*pipeline);
                rejected = false;
                try { commands->SetBindingSet(0, *held.Set); } catch (const std::invalid_argument&) { rejected = true; }
                Expect(rejected, "Old session binding set was accepted by a new device.");
                auto guard = device.CreateBuffer({16, BufferUsage::Uniform, MemoryUsage::GpuToCpu});
                rejected = false;
                try { (void)device.CreateBindingSet({held.Layout, {{7, BufferBinding{guard.get(), 0, 16}}}}); }
                catch (const std::invalid_argument&) { rejected = true; }
                Expect(rejected, "Old session binding layout was accepted by a new device.");
                rejected = false;
                try { (void)device.CreateTextureView(*held.Texture, {}); } catch (const std::invalid_argument&) { rejected = true; }
                Expect(rejected, "Old session texture view was accepted by a new device.");
                rejected = false;
                try { device.ResizeTexture(*held.Texture, 16, 16); } catch (const std::invalid_argument&) { rejected = true; }
                Expect(rejected, "Old session texture storage was resized by a new device.");
                commands->End();
                auto guardTexture = device.CreateTexture(textureDesc, oldHandle);
                const std::array<unsigned, 4> payload{cycle, 7, 23, 0xAABBCCDD};
                device.WriteBuffer(*guard, 0, Bytes(payload));
                const auto before = device.Statistics();
                held.Leases.clear();
                const auto after = device.Statistics();
                Expect(after.LiveObjects() == before.LiveObjects() && after.Retired == before.Retired,
                    "Late old-session destruction altered incoming resources.");
                std::array<unsigned, 4> result{};
                device.ReadBuffer(*guard, 0, std::as_writable_bytes(std::span(result)));
                Expect(result == payload && device.FindTexture(oldHandle) == guardTexture.get() && !device.DrainErrors(),
                    "Late old-session destruction deleted a reused incoming native name.");
                guard.reset(); guardTexture.reset(); commands.reset(); pipeline.reset();
                Exercise(device);
                device.WaitIdle();
                Expect(!device.Statistics().LiveObjects() && !device.Statistics().Retired,
                    "Incoming session did not release its resources.");
                incoming->Shutdown();
            }
            std::cout << "[rhi conformance] OpenGL session ownership PASS; 8 shutdown/recreate cycles; native release; late wrappers inert; stale bindings rejected\n";
        }
    }
    int RunRhiConformanceCheck()
    {
        try
        {
            RendererPlatform::WindowSettings settings{};
            settings.ClientSize = {64, 64}; settings.StartVisible = false;
            settings.Title = std::string(Branding::Name) + " RHI conformance";
            settings.Profile = RendererPlatform::WindowSettings::ContextProfile::Compatability;
            settings.ApiMajor = 4; settings.ApiMinor = 5;
            auto window = RendererPlatform::CreateWindow(settings);
            for (const auto backend : {Rhi::GraphicsBackend::OpenGl, Rhi::GraphicsBackend::Vulkan})
            {
                const auto* provider = Rhi::FindBackendProvider(backend);
                if (!provider) continue;
                auto session = provider->CreateSession({true});
                auto& device = session->Device();
                if (backend == Rhi::GraphicsBackend::Vulkan) Expect(session->ValidationEnabled(), "Conformance requires Vulkan validation.");
                ExerciseUnframedLifetime(device);
                Exercise(device);
                device.TrimCaches();
                device.WaitIdle();
                const auto live = device.Statistics();
                Expect(live.LiveObjects() == 0 && live.Retired == 0,
                    "RHI conformance did not release its resources.");
                session->Shutdown();
                Expect(session->ValidationErrors() == 0, "RHI conformance shutdown validation failed.");
                std::cout << "[rhi conformance] " << (backend == Rhi::GraphicsBackend::OpenGl ? "OpenGL" : "Vulkan")
                    << " PASS; unframed submission lifetime; GPU buffers/copies/pitched texture transfers; four-group layout; UBO/image/sampler; Draw/DrawIndexed; sampler pixels; release=0\n";
                if (backend == Rhi::GraphicsBackend::OpenGl) ExerciseOpenGlSessionLifetime(*provider);
            }
            return 0;
        }
        catch (const std::exception& error) { std::cerr << "[rhi conformance] FAIL: " << error.what() << '\n'; return 1; }
    }
}

#include "OpenGlDevice.hpp"

#include "../../OpenTK/GL.hpp"
#include "../../../Mods/Render/GlNames.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    namespace
    {
        namespace GL = ::OpenTK::Graphics::OpenGL::GL;

        constexpr std::int32_t FramebufferBinding = 0x8CA6; // GL_FRAMEBUFFER_BINDING
        // The enum shares its name with the function that takes it.
        using GlRenderbufferStorage = enum ::OpenTK::Graphics::OpenGL::GL::RenderbufferStorage;

        [[noreturn]] void NotYet(const char* what)
        {
            throw std::logic_error(std::string("OpenGL RHI: ") + what
                + " is not implemented by the OpenGL backend.");
        }

        [[nodiscard]] bool IsDepthFormat(TextureFormat format) noexcept
        {
            return format == TextureFormat::D16Unorm || format == TextureFormat::D24UnormS8Uint
                || format == TextureFormat::D32Float || format == TextureFormat::D32FloatS8Uint;
        }

        [[nodiscard]] bool Has(TextureUsage usage, TextureUsage bit) noexcept
        {
            return (static_cast<std::uint32_t>(usage) & static_cast<std::uint32_t>(bit)) != 0;
        }

        // A texture format as TexImage2D spells it.
        struct GlTextureFormat final
        {
            GL::PixelInternalFormat Internal;
            GL::PixelFormat Format;
            GL::PixelType Type;
        };

        [[nodiscard]] GlTextureFormat ToGl(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::RGBA8Unorm:
                return {GL::PixelInternalFormat::Rgba, GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte};
            case TextureFormat::RGB8Unorm:
                return {GL::PixelInternalFormat::Rgb, GL::PixelFormat::Rgb, GL::PixelType::UnsignedByte};
            case TextureFormat::D24UnormS8Uint:
                return {GL::PixelInternalFormat::Depth24Stencil8, GL::PixelFormat::DepthStencil,
                    GL::PixelType::UnsignedInt248};
            case TextureFormat::D32FloatS8Uint:
                // GL_DEPTH32F_STENCIL8 / GL_FLOAT_32_UNSIGNED_INT_24_8_REV
                return {static_cast<GL::PixelInternalFormat>(0x8CAD), GL::PixelFormat::DepthStencil,
                    static_cast<GL::PixelType>(0x8DAD)};
            default:
                throw std::invalid_argument("OpenGL RHI: unsupported texture format");
            }
        }

        [[nodiscard]] GlRenderbufferStorage ToGlRenderbuffer(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::D24UnormS8Uint: return GlRenderbufferStorage::Depth24Stencil8;
            case TextureFormat::D32FloatS8Uint: return static_cast<GlRenderbufferStorage>(0x8CAD);
            default: throw std::invalid_argument("OpenGL RHI: unsupported renderbuffer format");
            }
        }

        [[nodiscard]] std::int32_t ToGl(Filter filter, bool minification) noexcept
        {
            if (minification)
            {
                return static_cast<std::int32_t>(filter == Filter::Linear
                    ? GL::TextureMinFilter::Linear : GL::TextureMinFilter::Nearest);
            }
            return static_cast<std::int32_t>(filter == Filter::Linear
                ? GL::TextureMagFilter::Linear : GL::TextureMagFilter::Nearest);
        }

        [[nodiscard]] std::int32_t ToGl(SamplerAddressMode mode) noexcept
        {
            switch (mode)
            {
            case SamplerAddressMode::ClampToEdge:
            case SamplerAddressMode::ClampToBorder:
                return static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge);
            case SamplerAddressMode::MirroredRepeat:
                return static_cast<std::int32_t>(GL::TextureWrapMode::MirroredRepeat);
            case SamplerAddressMode::Repeat:
            default:
                return static_cast<std::int32_t>(GL::TextureWrapMode::Repeat);
            }
        }

        void DrainErrors()
        {
            for (std::int32_t i = 0; i < 64 && static_cast<std::int32_t>(GL::GetError()) != 0; ++i)
            {
            }
        }

        class OpenGlGraphicsDevice;
        class OpenGlCommandList;

        class OpenGlTexture final : public Texture
        {
        public:
            OpenGlTexture(OpenGlGraphicsDevice& device, const TextureDesc& desc,
                std::int32_t name, bool renderbuffer)
                : _device(&device), _desc(desc), _name(name), _renderbuffer(renderbuffer)
            {
            }

            ~OpenGlTexture() override;

            [[nodiscard]] const TextureDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] TextureHandle Handle() const noexcept override
            {
                return _renderbuffer ? TextureHandle{} : TextureHandle{_name};
            }

            [[nodiscard]] std::int32_t Name() const noexcept { return _name; }
            [[nodiscard]] bool IsRenderbuffer() const noexcept { return _renderbuffer; }
            [[nodiscard]] bool HasStorage() const noexcept { return _hasStorage; }
            void SetExtent(std::uint32_t width, std::uint32_t height) noexcept
            {
                _desc.width = width;
                _desc.height = height;
                _hasStorage = true;
            }
            void Detach() noexcept { _device = nullptr; }

        private:
            OpenGlGraphicsDevice* _device;
            TextureDesc _desc;
            std::int32_t _name;
            bool _renderbuffer;
            bool _hasStorage = false;
        };

        class OpenGlTextureView final : public TextureView
        {
        public:
            OpenGlTextureView(Texture& texture, const TextureViewDesc& desc)
                : _texture(texture), _desc(desc)
            {
            }

            [[nodiscard]] const TextureViewDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] const Texture& TextureResource() const noexcept override { return _texture; }

        private:
            Texture& _texture;
            TextureViewDesc _desc;
        };

        class OpenGlSampler final : public Sampler
        {
        public:
            explicit OpenGlSampler(const SamplerDesc& desc) : _desc(desc) {}
            [[nodiscard]] const SamplerDesc& Desc() const noexcept override { return _desc; }

        private:
            SamplerDesc _desc;
        };

        [[nodiscard]] const OpenGlTexture& Native(const Texture& texture)
        {
            return static_cast<const OpenGlTexture&>(texture);
        }

        [[nodiscard]] OpenGlTexture& Native(Texture& texture)
        {
            return static_cast<OpenGlTexture&>(texture);
        }

        // Attach the rendering info's targets to the framebuffer bound for drawing.
        void AttachTargets(const RenderingInfo& info)
        {
            if (!info.colorAttachments.empty() && info.colorAttachments[0].view != nullptr)
            {
                const OpenGlTexture& color = Native(info.colorAttachments[0].view->TextureResource());
                GL::FramebufferTexture2D(GL::FramebufferTarget::Framebuffer,
                    GL::FramebufferAttachment::ColorAttachment0, GL::TextureTarget::Texture2D,
                    color.Name(), 0);
            }
            if (info.depthStencilAttachment != nullptr && info.depthStencilAttachment->view != nullptr)
            {
                const OpenGlTexture& depth = Native(info.depthStencilAttachment->view->TextureResource());
                if (depth.IsRenderbuffer())
                {
                    GL::FramebufferRenderbuffer(GL::FramebufferTarget::Framebuffer,
                        GL::FramebufferAttachment::DepthStencilAttachment,
                        GL::RenderbufferTarget::Renderbuffer, depth.Name());
                }
                else
                {
                    GL::FramebufferTexture2D(GL::FramebufferTarget::Framebuffer,
                        GL::FramebufferAttachment::DepthStencilAttachment, GL::TextureTarget::Texture2D,
                        depth.Name(), 0);
                }
            }
        }

        struct FramebufferKey final
        {
            const Texture* Color = nullptr;
            const Texture* Depth = nullptr;
            bool operator==(const FramebufferKey&) const = default;
        };

        struct FramebufferKeyHash final
        {
            std::size_t operator()(const FramebufferKey& key) const noexcept
            {
                return std::hash<const void*>()(key.Color) * 31U ^ std::hash<const void*>()(key.Depth);
            }
        };

        [[nodiscard]] FramebufferKey KeyOf(const RenderingInfo& info) noexcept
        {
            FramebufferKey key{};
            if (!info.colorAttachments.empty() && info.colorAttachments[0].view != nullptr)
            {
                key.Color = &info.colorAttachments[0].view->TextureResource();
            }
            if (info.depthStencilAttachment != nullptr && info.depthStencilAttachment->view != nullptr)
            {
                key.Depth = &info.depthStencilAttachment->view->TextureResource();
            }
            return key;
        }

        class OpenGlGraphicsDevice final : public GraphicsDevice
        {
        public:
            OpenGlGraphicsDevice()
            {
                _capabilities.backend = GraphicsBackend::OpenGl;
                _capabilities.maxColorAttachments = 1;
                _capabilities.supportsWireframe = true;
            }

            ~OpenGlGraphicsDevice() override;

            [[nodiscard]] GraphicsBackend GetBackend() const noexcept override { return GraphicsBackend::OpenGl; }
            [[nodiscard]] const Capabilities& GetCapabilities() const noexcept override { return _capabilities; }

            [[nodiscard]] std::unique_ptr<Buffer> CreateBuffer(const BufferDesc&) override
            {
                NotYet("CreateBuffer (mesh buffers are owned by OpenGlGeometry)");
            }

            [[nodiscard]] std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) override
            {
                const bool renderbuffer = IsDepthFormat(desc.format)
                    && !Has(desc.usage, TextureUsage::Sampled);
                const std::int32_t name = renderbuffer ? GL::GenRenderbuffer()
                    : ::MphRead::Mods::Render::GlNames::NextTexture();
                return Make(desc, name, renderbuffer);
            }

            [[nodiscard]] std::unique_ptr<Texture> CreateTexture(
                const TextureDesc& desc, TextureHandle handle) override
            {
                if (!handle || IsDepthFormat(desc.format))
                {
                    throw std::invalid_argument("OpenGL RHI: a chosen handle must be a nonzero colour texture");
                }
                if (_byHandle.contains(handle.value))
                {
                    throw std::invalid_argument("OpenGL RHI: texture handle already live");
                }
                return Make(desc, handle.value, false);
            }

            [[nodiscard]] Texture* FindTexture(TextureHandle handle) noexcept override
            {
                const auto found = _byHandle.find(handle.value);
                return found == _byHandle.end() ? nullptr : found->second;
            }

            Texture& RetainTexture(std::unique_ptr<Texture> texture) override
            {
                Texture& kept = *texture;
                _retained.push_back(std::move(texture));
                return kept;
            }

            [[nodiscard]] std::unique_ptr<TextureView> CreateTextureView(
                Texture& texture, const TextureViewDesc& desc) override
            {
                return std::make_unique<OpenGlTextureView>(texture, desc);
            }

            [[nodiscard]] std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) override
            {
                return std::make_unique<OpenGlSampler>(desc);
            }

            [[nodiscard]] std::unique_ptr<Shader> CreateShader(const ShaderDesc&) override
            {
                NotYet("CreateShader");
            }

            [[nodiscard]] std::unique_ptr<BindingLayout> CreateBindingLayout(const BindingLayoutDesc&) override
            {
                NotYet("CreateBindingLayout");
            }

            [[nodiscard]] std::unique_ptr<BindingSet> CreateBindingSet(const BindingSetDesc&) override
            {
                NotYet("CreateBindingSet");
            }

            [[nodiscard]] std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
                const GraphicsPipelineDesc&) override
            {
                NotYet("CreateGraphicsPipeline");
            }

            [[nodiscard]] std::unique_ptr<CommandList> CreateCommandList() override;

            void WriteTexture(Texture& texture, const TextureWrite& write) override
            {
                OpenGlTexture& gl = Native(texture);
                const GlTextureFormat format = ToGl(write.format);
                GL::BindTexture(GL::TextureTarget::Texture2D, gl.Name());
                GL::TexImage2D(GL::TextureTarget::Texture2D, 0, format.Internal,
                    static_cast<std::int32_t>(write.width), static_cast<std::int32_t>(write.height), 0,
                    format.Format, format.Type, write.data);
                GL::BindTexture(GL::TextureTarget::Texture2D, 0);
                gl.SetExtent(write.width, write.height);
            }

            void ResizeTexture(Texture& texture, std::uint32_t width, std::uint32_t height) override
            {
                OpenGlTexture& gl = Native(texture);
                AllocateStorage(gl, width, height);
            }

            [[nodiscard]] bool CanRender(const RenderingInfo& info) override
            {
                bool complete = false;
                WithScratchFramebuffer(info, [&]
                {
                    complete = GL::CheckFramebufferStatus(GL::FramebufferTarget::Framebuffer)
                        == GL::FramebufferErrorCode::FramebufferComplete;
                });
                return complete;
            }

            [[nodiscard]] std::uint32_t DepthBits(const RenderingInfo& info) override
            {
                std::uint32_t bits = 0;
                WithScratchFramebuffer(info, [&]
                {
                    try
                    {
                        DrainErrors();
                        std::int32_t answer = 0;
                        GL::GetFramebufferAttachmentParameter(GL::FramebufferTarget::Framebuffer,
                            GL::FramebufferAttachment::DepthAttachment,
                            GL::FramebufferParameterName::FramebufferAttachmentDepthSize, answer);
                        if (static_cast<std::int32_t>(GL::GetError()) == 0 && answer >= 8 && answer <= 32)
                        {
                            bits = static_cast<std::uint32_t>(answer);
                        }
                    }
                    catch (...)
                    {
                        bits = 0;
                    }
                });
                return bits;
            }

            // Called by a texture as it is destroyed.
            void Forget(OpenGlTexture& texture) noexcept;

            void Register(OpenGlCommandList& list) { _lists.insert(&list); }
            void Unregister(OpenGlCommandList& list) noexcept { _lists.erase(&list); }

            // Storage for a render target: TexImage2D with no data, or a
            // renderbuffer's storage, at this extent.
            void AllocateStorage(OpenGlTexture& gl, std::uint32_t width, std::uint32_t height)
            {
                if (gl.IsRenderbuffer())
                {
                    GL::BindRenderbuffer(GL::RenderbufferTarget::Renderbuffer, gl.Name());
                    GL::RenderbufferStorage(GL::RenderbufferTarget::Renderbuffer,
                        ToGlRenderbuffer(gl.Desc().format),
                        static_cast<std::int32_t>(width), static_cast<std::int32_t>(height));
                    GL::BindRenderbuffer(GL::RenderbufferTarget::Renderbuffer, 0);
                }
                else
                {
                    const GlTextureFormat format = ToGl(gl.Desc().format);
                    GL::BindTexture(GL::TextureTarget::Texture2D, gl.Name());
                    GL::TexImage2D(GL::TextureTarget::Texture2D, 0, format.Internal,
                        static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 0,
                        format.Format, format.Type, nullptr);
                    GL::BindTexture(GL::TextureTarget::Texture2D, 0);
                }
                gl.SetExtent(width, height);
            }

        private:
            [[nodiscard]] std::unique_ptr<Texture> Make(
                const TextureDesc& desc, std::int32_t name, bool renderbuffer)
            {
                auto texture = std::make_unique<OpenGlTexture>(*this, desc, name, renderbuffer);
                if (!renderbuffer)
                {
                    _byHandle[name] = texture.get();
                }
                _live.insert(texture.get());
                // A render target has storage from the start; a sampled
                // texture gets its storage from its first WriteTexture.
                if (Has(desc.usage, TextureUsage::ColorAttachment)
                    || Has(desc.usage, TextureUsage::DepthStencilAttachment))
                {
                    AllocateStorage(*texture, desc.width, desc.height);
                }
                return texture;
            }

            template <typename F>
            void WithScratchFramebuffer(const RenderingInfo& info, F&& query)
            {
                const std::int32_t previous = GL::GetInteger(FramebufferBinding);
                const std::int32_t framebuffer = GL::GenFramebuffer();
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, framebuffer);
                AttachTargets(info);
                query();
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, previous);
                GL::DeleteFramebuffer(framebuffer);
            }

            Capabilities _capabilities{};
            std::unordered_map<std::int32_t, OpenGlTexture*> _byHandle{};
            std::unordered_set<OpenGlTexture*> _live{};
            std::vector<std::unique_ptr<Texture>> _retained{};
            std::unordered_set<OpenGlCommandList*> _lists{};
        };

        class OpenGlCommandList final : public CommandList
        {
        public:
            explicit OpenGlCommandList(OpenGlGraphicsDevice& device) : _device(&device)
            {
                device.Register(*this);
            }

            ~OpenGlCommandList() override
            {
                for (const auto& [key, framebuffer] : _framebuffers)
                {
                    (void)key;
                    GL::DeleteFramebuffer(framebuffer);
                }
                if (_device != nullptr)
                {
                    _device->Unregister(*this);
                }
            }

            void Begin() override {}
            void End() override {}

            void BeginRendering(const RenderingInfo& info) override
            {
                if (info.swapchain)
                {
                    GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, 0);
                    _current = {};
                }
                else
                {
                    const FramebufferKey key = KeyOf(info);
                    auto found = _framebuffers.find(key);
                    if (found == _framebuffers.end())
                    {
                        const std::int32_t framebuffer = GL::GenFramebuffer();
                        GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, framebuffer);
                        AttachTargets(info);
                        found = _framebuffers.emplace(key, framebuffer).first;
                    }
                    else
                    {
                        GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, found->second);
                    }
                    _current = key;
                }
                ClearFor(info);
            }

            void EndRendering() override {}

            void SetPipeline(const GraphicsPipeline&) override { NotYet("SetPipeline"); }

            void SetViewport(const Viewport& viewport) override
            {
                GL::Viewport(static_cast<std::int32_t>(viewport.x), static_cast<std::int32_t>(viewport.y),
                    static_cast<std::int32_t>(viewport.width), static_cast<std::int32_t>(viewport.height));
            }

            void SetScissor(const Scissor& scissor) override
            {
                GL::Scissor(scissor.x, scissor.y, static_cast<std::int32_t>(scissor.width),
                    static_cast<std::int32_t>(scissor.height));
            }

            void SetVertexBuffer(std::uint32_t, const Buffer&, std::uint64_t) override { NotYet("SetVertexBuffer"); }
            void SetIndexBuffer(const Buffer&, IndexType, std::uint64_t) override { NotYet("SetIndexBuffer"); }
            void SetBindingSet(std::uint32_t, const BindingSet&) override { NotYet("SetBindingSet"); }
            void SetStencilReference(std::uint32_t) override { NotYet("SetStencilReference"); }
            void Draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) override { NotYet("Draw"); }
            void DrawIndexed(std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::uint32_t) override
            {
                NotYet("DrawIndexed");
            }
            void CopyBuffer(const Buffer&, std::uint64_t, Buffer&, std::uint64_t, std::uint64_t) override
            {
                NotYet("CopyBuffer");
            }
            void CopyBufferToTexture(const Buffer&, Texture&, const BufferTextureCopy&) override
            {
                NotYet("CopyBufferToTexture");
            }
            void CopyTextureToBuffer(const Texture&, Buffer&, const BufferTextureCopy&) override
            {
                NotYet("CopyTextureToBuffer");
            }
            // OpenGL tracks resource state itself.
            void Transition(Buffer&, ResourceState, ResourceState) override {}
            void Transition(Texture&, ResourceState, ResourceState) override {}

            void BindSampledTexture(std::uint32_t slot, const Texture* texture, const Sampler* sampler) override
            {
                if (slot != 0)
                {
                    GL::ActiveTexture(static_cast<GL::TextureUnit>(
                        static_cast<std::int32_t>(GL::TextureUnit::Texture0) + static_cast<std::int32_t>(slot)));
                }
                GL::BindTexture(GL::TextureTarget::Texture2D, texture != nullptr ? Native(*texture).Name() : 0);
                if (texture != nullptr)
                {
                    if (sampler == nullptr)
                    {
                        throw std::invalid_argument("OpenGL RHI: a bound texture needs a sampler");
                    }
                    const SamplerDesc& desc = sampler->Desc();
                    GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMinFilter,
                        ToGl(desc.minFilter, true));
                    GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMagFilter,
                        ToGl(desc.magFilter, false));
                    GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapS,
                        ToGl(desc.addressU));
                    GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapT,
                        ToGl(desc.addressV));
                }
                if (slot != 0)
                {
                    GL::ActiveTexture(GL::TextureUnit::Texture0);
                }
            }

            void ReadColor(const RenderingInfo& info, std::uint32_t x, std::uint32_t y,
                std::uint32_t width, std::uint32_t height, TextureFormat format, void* destination) override
            {
                const GlTextureFormat gl = ToGl(format);
                if (info.swapchain)
                {
                    GL::BindFramebuffer(GL::FramebufferTarget::ReadFramebuffer, 0);
                    GL::ReadBuffer(GL::ReadBufferMode::Back);
                }
                else
                {
                    GL::BindFramebuffer(GL::FramebufferTarget::ReadFramebuffer, FramebufferFor(info));
                    GL::ReadBuffer(GL::ReadBufferMode::ColorAttachment0);
                }
                GL::PixelStore(GL::PixelStoreParameter::PackAlignment, 1);
                GL::ReadPixels(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                    static_cast<std::int32_t>(width), static_cast<std::int32_t>(height),
                    gl.Format, gl.Type, destination);
                if (!info.swapchain)
                {
                    GL::BindFramebuffer(GL::FramebufferTarget::ReadFramebuffer, 0);
                }
            }

            void CopyColorAttachmentToTexture(Texture& destination, std::uint32_t width, std::uint32_t height) override
            {
                GL::BindTexture(GL::TextureTarget::Texture2D, Native(destination).Name());
                GL::CopyTexSubImage2D(GL::TextureTarget::Texture2D, 0, 0, 0, 0, 0,
                    static_cast<std::int32_t>(width), static_cast<std::int32_t>(height));
            }

            // A texture is going away: drop every framebuffer built on it.
            void Forget(const Texture& texture) noexcept
            {
                for (auto it = _framebuffers.begin(); it != _framebuffers.end();)
                {
                    if (it->first.Color == &texture || it->first.Depth == &texture)
                    {
                        try
                        {
                            GL::DeleteFramebuffer(it->second);
                        }
                        catch (...)
                        {
                        }
                        if (_current == it->first)
                        {
                            _current = {};
                        }
                        it = _framebuffers.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            void Detach() noexcept { _device = nullptr; }

        private:
            // The framebuffer for these attachments, built if need be without
            // disturbing whatever is bound for drawing.
            [[nodiscard]] std::int32_t FramebufferFor(const RenderingInfo& info)
            {
                const FramebufferKey key = KeyOf(info);
                const auto found = _framebuffers.find(key);
                if (found != _framebuffers.end())
                {
                    return found->second;
                }
                const std::int32_t previous = GL::GetInteger(FramebufferBinding);
                const std::int32_t framebuffer = GL::GenFramebuffer();
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, framebuffer);
                AttachTargets(info);
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, previous);
                _framebuffers.emplace(key, framebuffer);
                return framebuffer;
            }

            void ClearFor(const RenderingInfo& info)
            {
                std::int32_t mask = 0;
                const bool clearColor = !info.colorAttachments.empty()
                    && info.colorAttachments[0].loadOp == LoadOp::Clear;
                const bool clearDepth = info.depthStencilAttachment != nullptr
                    && info.depthStencilAttachment->depthLoadOp == LoadOp::Clear;
                const bool clearStencil = info.depthStencilAttachment != nullptr
                    && info.depthStencilAttachment->stencilLoadOp == LoadOp::Clear;
                if (clearColor)
                {
                    const ClearColor& value = info.colorAttachments[0].clearValue;
                    GL::ClearColor(value.red, value.green, value.blue, value.alpha);
                    mask |= static_cast<std::int32_t>(GL::ClearBufferMask::ColorBufferBit);
                }
                if (clearDepth)
                {
                    mask |= static_cast<std::int32_t>(GL::ClearBufferMask::DepthBufferBit);
                }
                if (clearStencil)
                {
                    GL::ClearStencil(static_cast<std::int32_t>(info.depthStencilAttachment->clearStencil));
                    mask |= static_cast<std::int32_t>(GL::ClearBufferMask::StencilBufferBit);
                }
                if (mask != 0)
                {
                    GL::Clear(static_cast<GL::ClearBufferMask>(mask));
                }
            }

            OpenGlGraphicsDevice* _device;
            std::unordered_map<FramebufferKey, std::int32_t, FramebufferKeyHash> _framebuffers{};
            FramebufferKey _current{};
        };

        OpenGlTexture::~OpenGlTexture()
        {
            if (_device != nullptr)
            {
                _device->Forget(*this);
            }
            try
            {
                if (_renderbuffer)
                {
                    GL::DeleteRenderbuffer(_name);
                }
                else
                {
                    GL::DeleteTexture(_name);
                }
            }
            catch (...)
            {
                // Released while the context is current; nothing to report to.
            }
        }

        void OpenGlGraphicsDevice::Forget(OpenGlTexture& texture) noexcept
        {
            for (OpenGlCommandList* list : _lists)
            {
                list->Forget(texture);
            }
            _live.erase(&texture);
            const auto found = _byHandle.find(texture.Name());
            if (found != _byHandle.end() && found->second == &texture)
            {
                _byHandle.erase(found);
            }
        }

        OpenGlGraphicsDevice::~OpenGlGraphicsDevice()
        {
            for (OpenGlTexture* texture : _live)
            {
                texture->Detach();
            }
            for (OpenGlCommandList* list : _lists)
            {
                list->Detach();
            }
        }

        std::unique_ptr<CommandList> OpenGlGraphicsDevice::CreateCommandList()
        {
            return std::make_unique<OpenGlCommandList>(*this);
        }

        std::unique_ptr<OpenGlGraphicsDevice>& Instance()
        {
            static std::unique_ptr<OpenGlGraphicsDevice> device;
            return device;
        }
    }

    GraphicsDevice& ContextDevice()
    {
        auto& device = Instance();
        if (!device)
        {
            device = std::make_unique<OpenGlGraphicsDevice>();
        }
        return *device;
    }

    void ResetContextDevice() noexcept
    {
        // The textures are gone with their context; their destructors would
        // delete names the new context may already have reused.
        Instance().release();
    }
}

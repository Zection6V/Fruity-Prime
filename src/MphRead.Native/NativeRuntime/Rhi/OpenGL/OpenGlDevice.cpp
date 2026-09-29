#include "OpenGlDevice.hpp"

#include "../../OpenTK/GL.hpp"
#include "../../../Mods/Render/GlNames.hpp"
#include "../../System/Runtime.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
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
        // These two enums share their names with the functions that take them,
        // so the type is read off the function's own parameter. (An
        // elaborated `enum X` works on GCC and Clang, and MSVC reads it as
        // redeclaring the scoped enum as an unscoped one.)
        template <std::size_t N, typename F>
        struct ParameterOf;
        template <std::size_t N, typename R, typename... A>
        struct ParameterOf<N, R (*)(A...)>
        {
            using type = std::tuple_element_t<N, std::tuple<A...>>;
        };
        using GlRenderbufferStorage = ParameterOf<1, decltype(&GL::RenderbufferStorage)>::type;
        using GlStencilOp = ParameterOf<0, decltype(&GL::StencilOp)>::type;

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

        // A GL object waiting in the retirement queue.
        struct GlObject final
        {
            enum class Kind : std::uint8_t { Texture, Renderbuffer, Framebuffer, Buffer, Shader, Program };
            Kind What = Kind::Texture;
            std::int32_t Name = 0;
        };

        void DestroyNative(const GlObject& object) noexcept
        {
            try
            {
                switch (object.What)
                {
                case GlObject::Kind::Texture: GL::DeleteTexture(object.Name); break;
                case GlObject::Kind::Renderbuffer: GL::DeleteRenderbuffer(object.Name); break;
                case GlObject::Kind::Framebuffer: GL::DeleteFramebuffer(object.Name); break;
                case GlObject::Kind::Buffer: GL::DeleteBuffer(object.Name); break;
                case GlObject::Kind::Shader: GL::DeleteShader(object.Name); break;
                case GlObject::Kind::Program: GL::DeleteProgram(object.Name); break;
                }
            }
            catch (...)
            {
            }
        }

        // GL's own numbers for the RHI's pipeline enums. The wrapper's enums
        // carry only the values upstream used, so these are cast from the
        // registry values directly.
        [[nodiscard]] std::int32_t ToGl(CompareOp op) noexcept
        {
            return 0x0200 + static_cast<std::int32_t>(op); // GL_NEVER..GL_ALWAYS are in RHI order
        }

        [[nodiscard]] std::int32_t ToGl(StencilOp op) noexcept
        {
            switch (op)
            {
            case StencilOp::Zero: return 0;
            case StencilOp::Replace: return 0x1E01;
            case StencilOp::IncrementClamp: return 0x1E02;
            case StencilOp::DecrementClamp: return 0x1E03;
            case StencilOp::Invert: return 0x150A;
            case StencilOp::IncrementWrap: return 0x8507;
            case StencilOp::DecrementWrap: return 0x8508;
            case StencilOp::Keep:
            default: return 0x1E00;
            }
        }

        [[nodiscard]] std::int32_t ToGl(BlendFactor factor) noexcept
        {
            switch (factor)
            {
            case BlendFactor::Zero: return 0;
            case BlendFactor::One: return 1;
            case BlendFactor::SrcColor: return 0x0300;
            case BlendFactor::OneMinusSrcColor: return 0x0301;
            case BlendFactor::SrcAlpha: return 0x0302;
            case BlendFactor::OneMinusSrcAlpha: return 0x0303;
            case BlendFactor::DstAlpha: return 0x0304;
            case BlendFactor::OneMinusDstAlpha: return 0x0305;
            case BlendFactor::DstColor: return 0x0306;
            case BlendFactor::OneMinusDstColor: return 0x0307;
            case BlendFactor::ConstantColor: return 0x8001;
            case BlendFactor::OneMinusConstantColor: return 0x8002;
            case BlendFactor::ConstantAlpha: return 0x8003;
            case BlendFactor::OneMinusConstantAlpha: return 0x8004;
            }
            return 1;
        }

        constexpr std::int32_t CapDepthTest = 0x0B71;
        constexpr std::int32_t CapStencilTest = 0x0B90;
        constexpr std::int32_t CapBlend = 0x0BE2;
        constexpr std::int32_t CapCullFace = 0x0B44;
        constexpr std::int32_t CapPolygonOffsetFill = 0x8037;
        constexpr std::int32_t CurrentProgram = 0x8B8D; // GL_CURRENT_PROGRAM

        void SetCap(std::int32_t cap, bool on)
        {
            if (on)
            {
                GL::Enable(static_cast<GL::EnableCap>(cap));
            }
            else
            {
                GL::Disable(static_cast<GL::EnableCap>(cap));
            }
        }

        class OpenGlGraphicsPipeline final : public GraphicsPipeline
        {
        public:
            OpenGlGraphicsPipeline(const GraphicsPipelineDesc& desc, std::int32_t program)
                : _desc(desc), _program(program)
            {
            }
            [[nodiscard]] const GraphicsPipelineDesc& Desc() const noexcept override { return _desc; }
            // 0: the pipeline leaves the current program alone.
            [[nodiscard]] std::int32_t Program() const noexcept { return _program; }

        private:
            GraphicsPipelineDesc _desc;
            std::int32_t _program;
        };

        class OpenGlShader final : public Shader
        {
        public:
            OpenGlShader(OpenGlGraphicsDevice& device, ShaderStage stage, std::int32_t name)
                : _device(&device), _name(name)
            {
                _desc.stage = stage;
            }
            ~OpenGlShader() override;

            [[nodiscard]] const ShaderDesc& Desc() const noexcept override { return _desc; }
            [[nodiscard]] std::int32_t Name() const noexcept { return _name; }
            void Detach() noexcept { _device = nullptr; }

        private:
            OpenGlGraphicsDevice* _device;
            ShaderDesc _desc{};
            std::int32_t _name;
        };

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
                // The name may be waiting to be deleted from its last life; it is
                // this texture now, so it must not be.
                _retired.Cancel([&handle](const GlObject& object)
                {
                    return object.What == GlObject::Kind::Texture && object.Name == handle.value;
                });
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

            // Shaders come from GLSL source here (CreateGlslShader): the
            // renderer's programs are GLSL, and SPIR-V is the Vulkan backend's.
            [[nodiscard]] std::unique_ptr<Shader> CreateShader(const ShaderDesc&) override
            {
                NotYet("CreateShader from bytecode (OpenGL takes GLSL through CreateGlslShader)");
            }

            [[nodiscard]] std::unique_ptr<Shader> CreateGlsl(ShaderStage stage, const std::string& source)
            {
                const std::int32_t name = GL::CreateShader(stage == ShaderStage::Vertex
                    ? GL::ShaderType::VertexShader : GL::ShaderType::FragmentShader);
                GL::ShaderSource(name, source);
                GL::CompileShader(name);
                std::int32_t status = 0;
                GL::GetShader(name, GL::ShaderParameter::CompileStatus, status);
                if (::MphRead::NativeRuntime::DebuggerAttached() && !GL::GetShaderInfoLog(name).empty())
                {
                    ::MphRead::NativeRuntime::DebuggerBreak();
                }
                if (status == 0)
                {
                    const std::string log = GL::GetShaderInfoLog(name);
                    GL::DeleteShader(name);
                    throw std::runtime_error(log);
                }
                auto shader = std::make_unique<OpenGlShader>(*this, stage, name);
                _shaders.insert(shader.get());
                return shader;
            }

            [[nodiscard]] std::int32_t Program(const Shader& vertex, const Shader& fragment)
            {
                const auto key = std::make_pair(&vertex, &fragment);
                const auto found = _programs.find(key);
                if (found != _programs.end())
                {
                    return found->second;
                }
                const std::int32_t program = GL::CreateProgram();
                GL::AttachShader(program, static_cast<const OpenGlShader&>(vertex).Name());
                GL::AttachShader(program, static_cast<const OpenGlShader&>(fragment).Name());
                GL::LinkProgram(program);
                _programs.emplace(key, program);
                return program;
            }

            // A shader is going away: so is every program linked from it.
            void Forget(const OpenGlShader& shader) noexcept
            {
                for (auto it = _programs.begin(); it != _programs.end();)
                {
                    if (it->first.first == &shader || it->first.second == &shader)
                    {
                        Retire(GlObject{GlObject::Kind::Program, it->second});
                        it = _programs.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                _shaders.erase(const_cast<OpenGlShader*>(&shader));
            }

            FrameContext BeginFrame() override
            {
                if (_frameOpen)
                {
                    EndFrame();
                }
                ++_frame;
                const std::size_t slot = static_cast<std::size_t>(_frame % FramesInFlight);
                if (_fences[slot] != nullptr)
                {
                    if (!GL::ClientWaitSync(_fences[slot], 1'000'000'000ULL))
                    {
                        GL::Finish();
                    }
                    GL::DeleteSync(_fences[slot]);
                    _fences[slot] = nullptr;
                    _completed = std::max(_completed, _fenceFrames[slot]);
                }
                _frameOpen = true;
                PollFences();
                _retired.Collect(_completed, DestroyNative);
                return FrameContext{_frame, static_cast<std::uint32_t>(_frame % FramesInFlight)};
            }

            // The frame's GPU work ends at a fence. A slot still holding the
            // fence of the frame FramesInFlight back is waited on at BeginFrame: that
            // is the in-flight limit, and GL's own throttling means it has
            // almost always signalled by now.
            void EndFrame() override
            {
                if (!_frameOpen)
                {
                    return;
                }
                _frameOpen = false;
                const std::size_t slot = static_cast<std::size_t>(_frame % FramesInFlight);
                _fences[slot] = GL::FenceSync();
                _fenceFrames[slot] = _frame;
                if (_fences[slot] == nullptr)
                {
                    // Establish actual completion when sync is unavailable.
                    GL::Finish();
                    _completed = _frame;
                }
            }

            void WaitIdle() override
            {
                GL::Finish();
                for (std::size_t i = 0; i < _fences.size(); ++i)
                {
                    if (_fences[i] != nullptr)
                    {
                        GL::DeleteSync(_fences[i]);
                        _fences[i] = nullptr;
                    }
                }
                _completed = _frame;
                _retired.CollectAll(DestroyNative);
            }

            [[nodiscard]] GpuResourceStatistics Statistics() const override;

            void Retire(const GlObject& object)
            {
                if (object.What == GlObject::Kind::Buffer)
                {
                    _buffers.erase(object.Name);
                }
                _retired.Retire(object, _frame);
            }

            std::int32_t CreateGeometryBuffer()
            {
                const auto name = GL::GenBuffer();
                if (name != 0) _buffers.insert(name);
                return name;
            }

            [[nodiscard]] std::string AdapterDescription() override
            {
                return "vendor=" + GL::GetString(GL::StringName::Vendor)
                    + "\nrenderer=" + GL::GetString(GL::StringName::Renderer)
                    + "\nversion=" + GL::GetString(GL::StringName::Version)
                    + "\nshading language=" + GL::GetString(GL::StringName::ShadingLanguageVersion);
            }

            [[nodiscard]] std::int32_t DrainErrors() override
            {
                std::int32_t first = 0;
                for (std::int32_t i = 0; i < 64; ++i)
                {
                    const auto code = static_cast<std::int32_t>(GL::GetError());
                    if (code == 0)
                    {
                        break;
                    }
                    if (first == 0)
                    {
                        first = code;
                    }
                }
                return first;
            }

            [[nodiscard]] std::unique_ptr<BindingLayout> CreateBindingLayout(const BindingLayoutDesc&) override
            {
                NotYet("CreateBindingLayout");
            }

            [[nodiscard]] std::unique_ptr<BindingSet> CreateBindingSet(const BindingSetDesc&) override
            {
                NotYet("CreateBindingSet");
            }

            // The shaders, binding layout and vertex layout in the desc are
            // for backends that bake them in; OpenGL takes the program and the
            // arrays from what is bound, and applies the fixed state here.
            [[nodiscard]] std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
                const GraphicsPipelineDesc& desc) override
            {
                const std::int32_t program = desc.vertexShader != nullptr && desc.fragmentShader != nullptr
                    ? Program(*desc.vertexShader, *desc.fragmentShader) : 0;
                return std::make_unique<OpenGlGraphicsPipeline>(desc, program);
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
            std::unordered_set<OpenGlShader*> _shaders{};
            std::unordered_set<std::int32_t> _buffers{};
            std::map<std::pair<const Shader*, const Shader*>, std::int32_t> _programs{};

            void PollFences()
            {
                for (std::size_t i = 0; i < _fences.size(); ++i)
                {
                    if (_fences[i] != nullptr && GL::ClientWaitSync(_fences[i], 0))
                    {
                        GL::DeleteSync(_fences[i]);
                        _fences[i] = nullptr;
                        _completed = std::max(_completed, _fenceFrames[i]);
                    }
                }
            }

            RetirementQueue<GlObject> _retired{};
            std::uint64_t _frame = 0;
            std::uint64_t _completed = 0;
            bool _frameOpen = false;
            std::array<void*, FramesInFlight> _fences{};
            std::array<std::uint64_t, FramesInFlight> _fenceFrames{};
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
                    RetireFramebuffer(framebuffer);
                }
                if (_device != nullptr)
                {
                    _device->Unregister(*this);
                }
            }

            [[nodiscard]] std::size_t FramebufferCount() const noexcept { return _framebuffers.size(); }

            // The compatibility context's defaults the renderer set once as a
            // scene loaded: fixed-function texturing on (the launcher overlay
            // and the photograph still draw with it) and a depth test.
            void Begin() override
            {
                GL::Enable(GL::EnableCap::DepthTest);
                GL::Enable(GL::EnableCap::Texture2D);
                GL::DepthFunc(GL::DepthFunction::Lequal);
            }
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
                // Code outside the RHI may have changed GL state since the last
                // pipeline was applied; the next SetPipeline applies in full.
                _applied = nullptr;
                ClearFor(info);
            }

            void EndRendering() override {}

            void SetPipeline(const GraphicsPipeline& pipeline) override
            {
                if (&pipeline == _applied)
                {
                    return;
                }
                _applied = &pipeline;
                const GraphicsPipelineDesc& desc = pipeline.Desc();
                const std::int32_t program = static_cast<const OpenGlGraphicsPipeline&>(pipeline).Program();
                if (program != 0)
                {
                    GL::UseProgram(program);
                }

                const RasterizerStateDesc& raster = desc.rasterizer;
                SetCap(CapCullFace, raster.cullMode != CullMode::None);
                if (raster.cullMode != CullMode::None)
                {
                    GL::CullFace(raster.cullMode == CullMode::Front ? GL::TriangleFace::Front : GL::TriangleFace::Back);
                }
                GL::PolygonMode(GL::TriangleFace::FrontAndBack,
                    raster.fillMode == FillMode::Wireframe ? GL::PolygonMode::Line : GL::PolygonMode::Fill);
                GL::LineWidth(raster.lineWidth);
                SetCap(CapPolygonOffsetFill, raster.depthBiasEnable);
                GL::PolygonOffset(raster.depthBiasSlope, raster.depthBiasConstant);

                const DepthStencilStateDesc& ds = desc.depthStencil;
                SetCap(CapDepthTest, ds.depthTestEnable);
                GL::DepthFunc(static_cast<GL::DepthFunction>(ToGl(ds.depthCompareOp)));
                GL::DepthMask(ds.depthWriteEnable);
                SetCap(CapStencilTest, ds.stencilTestEnable);
                GL::StencilMask(ds.stencilWriteMask);
                GL::StencilOp(static_cast<GlStencilOp>(ToGl(ds.front.failOp)),
                    static_cast<GlStencilOp>(ToGl(ds.front.depthFailOp)),
                    static_cast<GlStencilOp>(ToGl(ds.front.passOp)));
                ApplyStencilFunc();

                const BlendAttachmentDesc blend = desc.blendAttachments.empty()
                    ? BlendAttachmentDesc{} : desc.blendAttachments[0];
                SetCap(CapBlend, blend.blendEnable);
                GL::BlendFunc(static_cast<GL::BlendingFactor>(ToGl(blend.srcColorFactor)),
                    static_cast<GL::BlendingFactor>(ToGl(blend.dstColorFactor)));
                const auto mask = static_cast<std::uint8_t>(blend.writeMask);
                GL::ColorMask((mask & 1U) != 0, (mask & 2U) != 0, (mask & 4U) != 0, (mask & 8U) != 0);

                ApplyAlphaTest(desc.alphaTest);
            }

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
            void SetStencilReference(std::uint32_t reference) override
            {
                _stencilReference = reference;
                ApplyStencilFunc();
            }
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
                const auto current = _framebuffers.find(_current);
                if (current != _framebuffers.end())
                {
                    GL::BindFramebuffer(GL::FramebufferTarget::ReadFramebuffer, current->second);
                }
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
                        RetireFramebuffer(it->second);
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
            void RetireFramebuffer(std::int32_t framebuffer) noexcept
            {
                if (_device != nullptr)
                {
                    _device->Retire(GlObject{GlObject::Kind::Framebuffer, framebuffer});
                }
                else
                {
                    DestroyNative(GlObject{GlObject::Kind::Framebuffer, framebuffer});
                }
            }

            void ApplyStencilFunc()
            {
                if (_applied == nullptr)
                {
                    return;
                }
                const DepthStencilStateDesc& ds = _applied->Desc().depthStencil;
                GL::StencilFunc(static_cast<GL::StencilFunction>(ToGl(ds.front.compareOp)),
                    static_cast<std::int32_t>(_stencilReference), ds.stencilReadMask);
            }

            // The alpha test is a discard in the fragment shader, driven by the
            // program's alpha_test uniform; a program without one has no test.
            void ApplyAlphaTest(AlphaTestMode mode)
            {
#if defined(__ANDROID__)
                // The ES wrapper already emulates glAlphaFunc as this same
                // uniform and writes it before every draw from its own state,
                // so the state is what has to be set.
                if (mode == AlphaTestMode::Disabled)
                {
                    GL::Disable(GL::EnableCap::AlphaTest);
                }
                else
                {
                    GL::Enable(GL::EnableCap::AlphaTest);
                    GL::AlphaFunc(mode == AlphaTestMode::EqualOne
                        ? GL::AlphaFunction::Equal : GL::AlphaFunction::Less, 1.0F);
                }
                return;
#endif
                const std::int32_t program = GL::GetInteger(CurrentProgram);
                if (program == 0)
                {
                    return;
                }
                auto found = _alphaTestLocations.find(program);
                if (found == _alphaTestLocations.end())
                {
                    found = _alphaTestLocations.emplace(program,
                        GL::GetUniformLocation(program, "alpha_test")).first;
                }
                if (found->second != -1)
                {
                    GL::Uniform1(found->second, static_cast<std::int32_t>(mode));
                }
            }

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

            // The load ops, within the render area. A clear clears the whole
            // attachment (or area) whatever the last pipeline's write masks
            // were, so the masks a clear needs are set first; the next
            // SetPipeline applies its own in full, since BeginRendering forgot
            // the last one.
            void ClearFor(const RenderingInfo& info)
            {
                constexpr std::int32_t CapScissorTest = 0x0C11;
                if (info.renderArea.width > 0 && info.renderArea.height > 0)
                {
                    SetCap(CapScissorTest, true);
                    GL::Scissor(info.renderArea.x, info.renderArea.y,
                        static_cast<std::int32_t>(info.renderArea.width),
                        static_cast<std::int32_t>(info.renderArea.height));
                }
                else
                {
                    SetCap(CapScissorTest, false);
                }
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
                    GL::ColorMask(true, true, true, true);
                    mask |= static_cast<std::int32_t>(GL::ClearBufferMask::ColorBufferBit);
                }
                if (clearDepth)
                {
                    // clearDepth is 1.0 everywhere the renderer clears; glClearDepth
                    // is never changed from it.
                    GL::DepthMask(true);
                    mask |= static_cast<std::int32_t>(GL::ClearBufferMask::DepthBufferBit);
                }
                if (clearStencil)
                {
                    GL::ClearStencil(static_cast<std::int32_t>(info.depthStencilAttachment->clearStencil));
                    GL::StencilMask(0xFF);
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
            const GraphicsPipeline* _applied = nullptr;
            std::uint32_t _stencilReference = 0;
            std::unordered_map<std::int32_t, std::int32_t> _alphaTestLocations{};
        };

        OpenGlTexture::~OpenGlTexture()
        {
            const GlObject object{_renderbuffer ? GlObject::Kind::Renderbuffer : GlObject::Kind::Texture, _name};
            if (_device != nullptr)
            {
                _device->Forget(*this);
                _device->Retire(object);
            }
            else
            {
                DestroyNative(object);
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
            for (OpenGlShader* shader : _shaders)
            {
                shader->Detach();
            }
            _retired.CollectAll(DestroyNative);
        }

        GpuResourceStatistics OpenGlGraphicsDevice::Statistics() const
        {
            GpuResourceStatistics statistics{};
            statistics.Textures = static_cast<std::uint32_t>(_live.size());
            statistics.Buffers = static_cast<std::uint32_t>(_buffers.size());
            for (const auto* texture : _live)
            {
                if (texture->IsRenderbuffer())
                {
                    ++statistics.Renderbuffers;
                    --statistics.Textures;
                }
            }
            statistics.Shaders = static_cast<std::uint32_t>(_shaders.size());
            statistics.Programs = static_cast<std::uint32_t>(_programs.size());
            for (const OpenGlCommandList* list : _lists)
            {
                statistics.Framebuffers += static_cast<std::uint32_t>(list->FramebufferCount());
            }
            statistics.Retired = static_cast<std::uint32_t>(_retired.Size());
            statistics.CompletedFrame = _completed;
            return statistics;
        }

        OpenGlShader::~OpenGlShader()
        {
            const GlObject object{GlObject::Kind::Shader, _name};
            if (_device != nullptr)
            {
                _device->Forget(*this);
                _device->Retire(object);
            }
            else
            {
                DestroyNative(object);
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

    std::unique_ptr<Shader> CreateGlslShader(GraphicsDevice& device, ShaderStage stage, const std::string& source)
    {
        return static_cast<OpenGlGraphicsDevice&>(device).CreateGlsl(stage, source);
    }

    std::int32_t ProgramFor(GraphicsDevice& device, const Shader& vertex, const Shader& fragment)
    {
        return static_cast<OpenGlGraphicsDevice&>(device).Program(vertex, fragment);
    }

    void RetireBuffer(std::int32_t buffer) noexcept
    {
        if (buffer == 0)
        {
            return;
        }
        if (auto& device = Instance(); device)
        {
            device->Retire(GlObject{GlObject::Kind::Buffer, buffer});
        }
        else
        {
            DestroyNative(GlObject{GlObject::Kind::Buffer, buffer});
        }
    }

    std::int32_t CreateGeometryBuffer()
    {
        return static_cast<OpenGlGraphicsDevice&>(ContextDevice()).CreateGeometryBuffer();
    }

    void ResetContextDevice() noexcept
    {
        // The textures are gone with their context; their destructors would
        // delete names the new context may already have reused.
        Instance().release();
    }

    void FinishContextDevice()
    {
        if (auto& device = Instance(); device)
        {
            device->WaitIdle();
        }
    }
}

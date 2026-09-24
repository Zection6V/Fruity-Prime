#if !ANDROID
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;
using OpenTK.Windowing.GraphicsLibraryFramework;
using Veldrid;
using Veldrid.SPIRV;
using VPixelFormat = Veldrid.PixelFormat;
using GLPixelFormat = OpenTK.Graphics.OpenGL.PixelFormat;
using GLTexture = OpenTK.Graphics.OpenGL.TextureTarget;
using GLFramebufferAttachment = OpenTK.Graphics.OpenGL.FramebufferAttachment;
using GLErrorCode = OpenTK.Graphics.OpenGL.ErrorCode;
using GLStencilOp = OpenTK.Graphics.OpenGL.StencilOp;

namespace MphRead.Mods.Render
{
    internal static class VulkanGl
    {
        private const int FloatsPerVertex = 16;
        private const uint VertexStride = FloatsPerVertex * sizeof(float);
        private const uint UboSize = 4272;
        private const int ProjectionOffset = 0;
        private const int ViewOffset = 64;
        private const int ViewInvOffset = 128;
        private const int TexOffset = 192;
        private const int StackOffset = 256;
        private const int ImmColorOffset = 2304;
        private const int OverrideOffset = 2320;
        private const int FadeOffset = 2336;
        private const int Params0Offset = 2352;
        private const int Params1Offset = 2368;
        private const int Params2Offset = 2384;
        private const int Light1VectorOffset = 2400;
        private const int Light2VectorOffset = 2416;
        private const int Light1ColorOffset = 2432;
        private const int Light2ColorOffset = 2448;
        private const int DiffuseOffset = 2464;
        private const int AmbientOffset = 2480;
        private const int SpecularOffset = 2496;
        private const int EmissionOffset = 2512;
        private const int FogColorOffset = 2528;
        private const int PaletteOverrideOffset = 2544;
        private const int FlatColorOffset = 2560;
        private const int Scene0Offset = 2576;
        private const int Scene1Offset = 2592;
        private const int Scene2Offset = 2608;
        private const int ToonTableOffset = 2624;
        private const int Rtt0Offset = 3136;
        private const int Cel0Offset = 3152;
        private const int Cel1Offset = 3168;
        private const int Shift0Offset = 3184;
        private const int ShiftTableOffset = 3200;
        private const int WhiteTableOffset = 3456;
        private const int ImmNormalOffset = 4224;
        private const int ImmTex0Offset = 4240;
        private const int ImmTex1Offset = 4256;

        private enum ProgramKind { Screen, Scene, Rtt, Shift, Cel, Backdrop }

        private sealed class Batch
        {
            public readonly List<float> Vertices = new(4096);
            public readonly List<uint> Triangles = new(4096);
            public readonly List<uint> Lines = new(1024);
            public int VertexCount;
            public void Clear()
            {
                Vertices.Clear();
                Triangles.Clear();
                Lines.Clear();
                VertexCount = 0;
            }
        }

        private sealed class DisplayList
        {
            public float[] Vertices = Array.Empty<float>();
            public uint[] Triangles = Array.Empty<uint>();
            public uint[] Lines = Array.Empty<uint>();
            public bool SetsColor;
            public Vector4 FinalColor;
            public bool SetsNormal;
            public Vector3 FinalNormal;
            public bool SetsTex0;
            public Vector3 FinalTex0;
            public bool SetsTex1;
            public Vector2 FinalTex1;
        }

        private sealed class ShaderInfo
        {
            public ShaderType Type;
            public string Source = "";
            public bool Compiled;
            public string InfoLog = "";
        }

        private sealed class ProgramInfo
        {
            public ProgramKind Kind = ProgramKind.Screen;
            public bool Linked;
            public string InfoLog = "";
            public readonly Dictionary<int, ShaderInfo> Shaders = new();
            public readonly Dictionary<string, object> Values = new(StringComparer.Ordinal);
        }

        private sealed class TextureInfo : IDisposable
        {
            public Veldrid.Texture? Texture;
            public TextureView? View;
            public uint Width;
            public uint Height;
            public bool Depth;
            public bool MinLinear;
            public bool MagLinear;
            // OpenGL RGB internal formats have no alpha component. Sampling
            // them returns alpha 1 even if the compatibility storage has to be
            // RGBA on Vulkan. Keep that semantic separate from the physical
            // Veldrid format so render targets cannot leak fragment alpha into
            // the later fullscreen composite.
            public bool ForceOpaqueAlpha;
            // CPU-uploaded textures keep the caller's row/UV convention.
            // A Vulkan framebuffer, however, has a top-left UV origin while
            // Fruity's fullscreen quads carry OpenGL framebuffer coordinates
            // (top is v=1). RTT/shift sampling flips only these textures.
            public bool FlipVWhenSampledAsOpenGl;
            public SamplerAddressMode AddressU = SamplerAddressMode.Wrap;
            public SamplerAddressMode AddressV = SamplerAddressMode.Wrap;
            public int Version;

            public void Dispose()
            {
                View?.Dispose();
                Texture?.Dispose();
                View = null;
                Texture = null;
            }
        }

        private sealed class FramebufferInfo : IDisposable
        {
            public int ColorTexture;
            public int DepthTexture;
            public int DepthRenderbuffer;
            public Veldrid.Framebuffer? Framebuffer;
            public void Dispose()
            {
                Framebuffer?.Dispose();
                Framebuffer = null;
            }
        }

        private sealed class RenderbufferInfo : IDisposable
        {
            public Veldrid.Texture? Texture;
            public uint Width;
            public uint Height;
            public void Dispose()
            {
                Texture?.Dispose();
                Texture = null;
            }
        }

        private sealed class UniformSlot : IDisposable
        {
            public DeviceBuffer Buffer { get; }
            public ResourceSet? Set;
            public string SetKey = "";

            public UniformSlot(ResourceFactory factory)
            {
                Buffer = factory.CreateBuffer(new BufferDescription(
                    UboSize, BufferUsage.UniformBuffer | BufferUsage.Dynamic));
            }

            public void InvalidateSet()
            {
                Set?.Dispose();
                Set = null;
                SetKey = "";
            }

            public void Dispose()
            {
                InvalidateSet();
                Buffer.Dispose();
            }
        }

        private sealed class ClearUniformSlot : IDisposable
        {
            public DeviceBuffer Buffer { get; }
            public ResourceSet Set { get; }

            public ClearUniformSlot(ResourceFactory factory, ResourceLayout layout)
            {
                Buffer = factory.CreateBuffer(new BufferDescription(
                    16, BufferUsage.UniformBuffer | BufferUsage.Dynamic));
                Set = factory.CreateResourceSet(new ResourceSetDescription(layout, Buffer));
            }

            public void Dispose()
            {
                Set.Dispose();
                Buffer.Dispose();
            }
        }

        private sealed class GeometrySlot : IDisposable
        {
            public DeviceBuffer? VertexBuffer { get; private set; }
            public DeviceBuffer? IndexBuffer { get; private set; }
            private uint _vertexBytes;
            private uint _indexBytes;

            public void Ensure(ResourceFactory factory, uint vertexBytes, uint indexBytes)
            {
                if (VertexBuffer == null || vertexBytes > _vertexBytes)
                {
                    VertexBuffer?.Dispose();
                    _vertexBytes = Math.Max(vertexBytes, Math.Max(_vertexBytes * 2, 65536u));
                    VertexBuffer = factory.CreateBuffer(new BufferDescription(
                        _vertexBytes, BufferUsage.VertexBuffer | BufferUsage.Dynamic));
                }
                if (IndexBuffer == null || indexBytes > _indexBytes)
                {
                    IndexBuffer?.Dispose();
                    _indexBytes = Math.Max(indexBytes, Math.Max(_indexBytes * 2, 32768u));
                    IndexBuffer = factory.CreateBuffer(new BufferDescription(
                        _indexBytes, BufferUsage.IndexBuffer | BufferUsage.Dynamic));
                }
            }

            public void Dispose()
            {
                VertexBuffer?.Dispose();
                IndexBuffer?.Dispose();
                VertexBuffer = null;
                IndexBuffer = null;
                _vertexBytes = 0;
                _indexBytes = 0;
            }
        }

        private static GraphicsDevice? _gd;
        private static ResourceFactory? _factory;
        private static CommandList? _commands;
        private static Fence? _frameFence;
        private static bool _commandsOpen;
        private static bool _frameInFlight;
        private static ResourceLayout? _layout;
        private static ResourceLayout? _clearLayout;
        private static Shader[]? _sceneShaders;
        private static Shader[]? _screenShaders;
        private static Shader[]? _rttShaders;
        private static Shader[]? _shiftShaders;
        private static Shader[]? _celShaders;
        private static Shader[]? _backdropShaders;
        private static Shader[]? _clearShaders;
        private static readonly Dictionary<string, Pipeline> _pipelines = new();
        private static readonly Dictionary<string, Sampler> _samplers = new();
        private static readonly List<UniformSlot> _uniformSlots = new();
        private static int _uniformSlotIndex;
        private static readonly List<ClearUniformSlot> _clearUniformSlots = new();
        private static int _clearUniformSlotIndex;
        private static readonly List<GeometrySlot> _geometrySlots = new();
        private static int _geometrySlotIndex;

        private static readonly Dictionary<int, TextureInfo> _textures = new();
        private static readonly Dictionary<int, FramebufferInfo> _framebuffers = new();
        private static readonly Dictionary<int, RenderbufferInfo> _renderbuffers = new();
        private static readonly Dictionary<int, DisplayList> _lists = new();
        private static readonly Dictionary<int, ShaderInfo> _shaders = new();
        private static readonly Dictionary<int, ProgramInfo> _programs = new();
        private static readonly Dictionary<int, (int Program, string Name)> _locations = new();

        private static readonly Batch _batch = new();
        private static int _nextTexture = 1;
        private static int _nextFramebuffer = 1;
        private static int _nextRenderbuffer = 1;
        private static int _nextList = 1;
        private static int _nextShader = 1;
        private static int _nextProgram = 1;
        private static int _nextLocation = 1;
        private static bool _recording;
        private static int _recordList;
        private static PrimitiveType _primitive;
        private static int _primitiveStart;
        private static Vector4 _preListColor;
        private static Vector3 _preListNormal;
        private static Vector3 _preListTex0;
        private static Vector2 _preListTex1;
        private static bool _listColorSet;
        private static bool _listNormalSet;
        private static bool _listTex0Set;
        private static bool _listTex1Set;

        private static Vector4 _currentColor = Vector4.One;
        private static Vector3 _currentNormal = Vector3.UnitZ;
        private static Vector3 _currentTex0;
        private static Vector2 _currentTex1;
        private static bool _vertexColorSet;
        private static int _activeTextureUnit;
        private static readonly int[] _boundTextures = new int[2];
        private static readonly bool[] _texture2DEnabled = new bool[2];
        private static readonly TextureEnvMode[] _textureEnvModes =
        {
            TextureEnvMode.Modulate,
            TextureEnvMode.Modulate
        };

        private static int _currentProgram;
        private static int _drawFramebuffer;
        private static int _readFramebuffer;
        private static int _boundRenderbuffer;

        private static bool _depthTest;
        private static bool _depthWrite = true;
        private static DepthFunction _depthFunction = DepthFunction.Less;
        private static bool _blend;
        private static BlendingFactor _blendSrc = BlendingFactor.SrcAlpha;
        private static BlendingFactor _blendDst = BlendingFactor.OneMinusSrcAlpha;
        private static bool _cull;
        private static TriangleFace _cullFace = TriangleFace.Back;
        private static bool _scissor;
        private static bool _alphaTest;
        private static AlphaFunction _alphaFunction = AlphaFunction.Always;
        private static float _alphaReference;
        private static bool _polygonOffsetFill;
        private static float _polygonOffsetFactor;
        private static float _polygonOffsetUnits;
        private static bool _stencilTest;
        private static StencilFunction _stencilFunction = StencilFunction.Always;
        private static int _stencilReference;
        private static int _stencilReadMask = 0xFF;
        private static int _stencilWriteMask = 0xFF;
        private static GLStencilOp _stencilFail = GLStencilOp.Keep;
        private static GLStencilOp _stencilDepthFail = GLStencilOp.Keep;
        private static GLStencilOp _stencilPass = GLStencilOp.Keep;
        private static OpenTK.Graphics.OpenGL.PolygonMode _polygonMode = OpenTK.Graphics.OpenGL.PolygonMode.Fill;
        private static bool _maskR = true, _maskG = true, _maskB = true, _maskA = true;
        private static Color4 _clearColor = new(0, 0, 0, 1);
        private static int _clearStencil;
        private static int _viewX, _viewY, _viewW = 1, _viewH = 1;
        private static int _scissorX, _scissorY, _scissorW = 1, _scissorH = 1;

        private static TextureInfo? _white;

        public static bool Initialized => _gd != null;

        [DllImport("kernel32", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandle(string? moduleName);

        public static unsafe void Initialize(RenderWindow window)
        {
            if (_gd != null)
            {
                return;
            }

            // A Vulkan device represents a fresh OpenGL-compatible context.
            // Do not carry emulated state across a live backend/window switch.
            ResetCompatibilityState();

            Vector2i size = window.FramebufferSize;
            SwapchainSource source;
            if (OperatingSystem.IsWindows())
            {
                source = SwapchainSource.CreateWin32(GLFW.GetWin32Window(window.WindowPtr), GetModuleHandle(null));
            }
            else if (OperatingSystem.IsLinux() && IsWayland())
            {
                source = SwapchainSource.CreateWayland(GLFW.GetWaylandDisplay(), GLFW.GetWaylandWindow(window.WindowPtr));
            }
            else if (OperatingSystem.IsLinux())
            {
                UIntPtr xwindow = GLFW.GetX11Window(window.WindowPtr);
                source = SwapchainSource.CreateXlib(GLFW.GetX11Display(),
                    new IntPtr(unchecked((long)xwindow.ToUInt64())));
            }
            else if (OperatingSystem.IsMacOS())
            {
                source = SwapchainSource.CreateNSView(GLFW.GetCocoaView(window.WindowPtr));
            }
            else
            {
                throw new PlatformNotSupportedException("Vulkan renderer has no window surface implementation for this platform.");
            }

            var options = new GraphicsDeviceOptions(false, VPixelFormat.D24_UNorm_S8_UInt, true)
            {
                PreferDepthRangeZeroToOne = false,
                PreferStandardClipSpaceYDirection = true
            };
            var swapchain = new SwapchainDescription(source,
                (uint)Math.Max(size.X, 1), (uint)Math.Max(size.Y, 1),
                VPixelFormat.D24_UNorm_S8_UInt, true);
            _gd = GraphicsDevice.CreateVulkan(options, swapchain);
            _factory = _gd.ResourceFactory;
            _commands = _factory.CreateCommandList();
            _frameFence = _factory.CreateFence(false);

            _layout = _factory.CreateResourceLayout(new ResourceLayoutDescription(
                new ResourceLayoutElementDescription("CompatUniforms", ResourceKind.UniformBuffer,
                    ShaderStages.Vertex | ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Tex0", ResourceKind.TextureReadOnly, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Samp0", ResourceKind.Sampler, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Tex1", ResourceKind.TextureReadOnly, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Samp1", ResourceKind.Sampler, ShaderStages.Fragment)));

            _clearLayout = _factory.CreateResourceLayout(new ResourceLayoutDescription(
                new ResourceLayoutElementDescription("ClearUniforms", ResourceKind.UniformBuffer,
                    ShaderStages.Fragment)));

            _sceneShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.SceneVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.SceneFragment), "main"));
            _screenShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.ScreenFragment), "main"));
            _rttShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.RttFragment), "main"));
            _shiftShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.ShiftFragment), "main"));
            _celShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.CelFragment), "main"));
            _backdropShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.BackdropFragment), "main"));
            _clearShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ClearVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.ClearFragment), "main"));

            _white = new TextureInfo();
            AllocateTexture(_white, 1, 1, depth: false, forceOpaqueAlpha: false);
            byte[] white = { 255, 255, 255, 255 };
            _gd.UpdateTexture(_white.Texture!, white, 0, 0, 0, 1, 1, 1, 0, 0);
            DebugLog.Line("render", $"Vulkan device: {_gd.DeviceName} ({_gd.VendorName}), API {_gd.ApiVersion}");
        }

        private static void ResetCompatibilityState()
        {
            _nextTexture = 1;
            _nextFramebuffer = 1;
            _nextRenderbuffer = 1;
            _nextList = 1;
            _nextShader = 1;
            _nextProgram = 1;
            _nextLocation = 1;
            _recording = false;
            _recordList = 0;
            _primitiveStart = 0;
            _preListColor = Vector4.One;
            _preListNormal = Vector3.UnitZ;
            _preListTex0 = Vector3.Zero;
            _preListTex1 = Vector2.Zero;
            _listColorSet = false;
            _listNormalSet = false;
            _listTex0Set = false;
            _listTex1Set = false;

            _currentColor = Vector4.One;
            _currentNormal = Vector3.UnitZ;
            _currentTex0 = Vector3.Zero;
            _currentTex1 = Vector2.Zero;
            _vertexColorSet = false;
            _activeTextureUnit = 0;
            Array.Clear(_boundTextures, 0, _boundTextures.Length);
            Array.Clear(_texture2DEnabled, 0, _texture2DEnabled.Length);
            for (int i = 0; i < _textureEnvModes.Length; i++)
            {
                _textureEnvModes[i] = TextureEnvMode.Modulate;
            }

            _currentProgram = 0;
            _drawFramebuffer = 0;
            _readFramebuffer = 0;
            _boundRenderbuffer = 0;

            _depthTest = false;
            _depthWrite = true;
            _depthFunction = DepthFunction.Less;
            _blend = false;
            _blendSrc = BlendingFactor.One;
            _blendDst = BlendingFactor.Zero;
            _cull = false;
            _cullFace = TriangleFace.Back;
            _scissor = false;
            _alphaTest = false;
            _alphaFunction = AlphaFunction.Always;
            _alphaReference = 0f;
            _polygonOffsetFill = false;
            _polygonOffsetFactor = 0f;
            _polygonOffsetUnits = 0f;
            _stencilTest = false;
            _stencilFunction = StencilFunction.Always;
            _stencilReference = 0;
            _stencilReadMask = 0xFF;
            _stencilWriteMask = 0xFF;
            _stencilFail = GLStencilOp.Keep;
            _stencilDepthFail = GLStencilOp.Keep;
            _stencilPass = GLStencilOp.Keep;
            _polygonMode = OpenTK.Graphics.OpenGL.PolygonMode.Fill;
            _maskR = _maskG = _maskB = _maskA = true;
            _clearColor = new Color4(0f, 0f, 0f, 0f);
            _clearStencil = 0;
            _viewX = _viewY = 0;
            _viewW = _viewH = 1;
            _scissorX = _scissorY = 0;
            _scissorW = _scissorH = 1;
        }

        private static bool IsWayland()
        {
            return OperatingSystem.IsLinux()
                && GLFW.GetPlatform() == OpenTK.Windowing.GraphicsLibraryFramework.Platform.Wayland;
        }

        public static void Shutdown()
        {
            if (_gd == null)
            {
                return;
            }
            if (_commandsOpen)
            {
                _commands!.End();
                _gd.SubmitCommands(_commands);
                _commandsOpen = false;
            }
            _gd.WaitForIdle();
            foreach (Pipeline pipeline in _pipelines.Values) pipeline.Dispose();
            foreach (UniformSlot slot in _uniformSlots) slot.Dispose();
            foreach (ClearUniformSlot slot in _clearUniformSlots) slot.Dispose();
            foreach (Sampler sampler in _samplers.Values) sampler.Dispose();
            foreach (TextureInfo texture in _textures.Values) texture.Dispose();
            foreach (FramebufferInfo framebuffer in _framebuffers.Values) framebuffer.Dispose();
            foreach (RenderbufferInfo renderbuffer in _renderbuffers.Values) renderbuffer.Dispose();
            _white?.Dispose();
            if (_sceneShaders != null) foreach (Shader shader in _sceneShaders) shader.Dispose();
            if (_screenShaders != null) foreach (Shader shader in _screenShaders) shader.Dispose();
            if (_rttShaders != null) foreach (Shader shader in _rttShaders) shader.Dispose();
            if (_shiftShaders != null) foreach (Shader shader in _shiftShaders) shader.Dispose();
            if (_celShaders != null) foreach (Shader shader in _celShaders) shader.Dispose();
            if (_backdropShaders != null) foreach (Shader shader in _backdropShaders) shader.Dispose();
            if (_clearShaders != null) foreach (Shader shader in _clearShaders) shader.Dispose();
            foreach (GeometrySlot slot in _geometrySlots) slot.Dispose();
            _clearLayout?.Dispose();
            _layout?.Dispose();
            _commands?.Dispose();
            _frameFence?.Dispose();
            _gd.Dispose();

            _pipelines.Clear();
            _uniformSlots.Clear();
            _uniformSlotIndex = 0;
            _clearUniformSlots.Clear();
            _clearUniformSlotIndex = 0;
            _geometrySlots.Clear();
            _geometrySlotIndex = 0;
            _samplers.Clear();
            _textures.Clear();
            _framebuffers.Clear();
            _renderbuffers.Clear();
            _lists.Clear();
            _shaders.Clear();
            _programs.Clear();
            _locations.Clear();
            _batch.Clear();
            _gd = null;
            _factory = null;
            _commands = null;
            _frameFence = null;
            _frameInFlight = false;
            _layout = null;
            _clearLayout = null;
            _white = null;
        }

        public static void Present()
        {
            if (_gd == null)
            {
                return;
            }
            if (_commandsOpen)
            {
                _commands!.End();
                _gd.SubmitCommands(_commands, _frameFence!);
                _commandsOpen = false;
                _frameInFlight = true;
            }
            _gd.SwapBuffers();
        }

        public static void SetVSync(bool enabled)
        {
            if (_gd != null)
            {
                _gd.SyncToVerticalBlank = enabled;
            }
        }

        public static void Resize(int width, int height)
        {
            if (_gd == null || width <= 0 || height <= 0)
            {
                return;
            }
            SynchronizeResourceMutation();
            if (_gd.SwapchainFramebuffer.Width != (uint)width || _gd.SwapchainFramebuffer.Height != (uint)height)
            {
                _gd.ResizeMainWindow((uint)width, (uint)height);
                DisposePipelineCache();
            }
        }

        // Veldrid's Vulkan resources are ref-counted, but once the last
        // reference is released their Vk objects are destroyed immediately.
        // Any cached framebuffer, descriptor set, image view, image, sampler,
        // or pipeline must therefore outlive both submitted work and commands
        // recorded but not submitted yet.
        private static void SynchronizeResourceMutation()
        {
            if (_gd == null)
            {
                return;
            }
            if (_commandsOpen)
            {
                // EnsureFrame never begins a new list until the previous
                // fenced frame has completed, so an open list is the only
                // outstanding work here. Submit that list with the reusable
                // frame fence and wait only for this submission before
                // destroying resources it referenced.
                _commands!.End();
                _gd.SubmitCommands(_commands, _frameFence!);
                _commandsOpen = false;
                _frameInFlight = true;
                _gd.WaitForFence(_frameFence!);
                _frameFence!.Reset();
                _frameInFlight = false;
                _uniformSlotIndex = 0;
                _clearUniformSlotIndex = 0;
                _geometrySlotIndex = 0;
                return;
            }
            if (_frameInFlight)
            {
                // Present submitted this frame with our reusable fence.
                // Waiting the fence is sufficient and avoids a device-wide
                // idle when no newer command list has been recorded.
                _gd.WaitForFence(_frameFence!);
                _frameFence!.Reset();
                _frameInFlight = false;
                _uniformSlotIndex = 0;
                _clearUniformSlotIndex = 0;
                _geometrySlotIndex = 0;
            }
        }

        private static void EnsureFrame()
        {
            if (_commandsOpen)
            {
                return;
            }
            if (_frameInFlight)
            {
                _gd!.WaitForFence(_frameFence!);
                _frameFence!.Reset();
                _frameInFlight = false;
                _uniformSlotIndex = 0;
                _clearUniformSlotIndex = 0;
                _geometrySlotIndex = 0;
            }
            _commands!.Begin();
            _commandsOpen = true;
        }

        private static Veldrid.Framebuffer CurrentFramebuffer(int name)
        {
            if (name == 0)
            {
                return _gd!.SwapchainFramebuffer;
            }
            if (!_framebuffers.TryGetValue(name, out FramebufferInfo? info))
            {
                throw new ProgramException($"Unknown Vulkan framebuffer {name}.");
            }
            if (info.Framebuffer == null)
            {
                TextureInfo color = GetTexture(info.ColorTexture);
                Veldrid.Texture? depth = null;
                if (info.DepthTexture != 0)
                {
                    depth = GetTexture(info.DepthTexture).Texture;
                }
                else if (info.DepthRenderbuffer != 0
                    && _renderbuffers.TryGetValue(info.DepthRenderbuffer, out RenderbufferInfo? rb))
                {
                    depth = rb.Texture;
                }
                if (color.Texture == null)
                {
                    throw new ProgramException($"Framebuffer {name} has no color attachment.");
                }
                info.Framebuffer = _factory!.CreateFramebuffer(new FramebufferDescription(depth, color.Texture));
            }
            return info.Framebuffer;
        }

        private static void BindCurrentFramebuffer()
        {
            EnsureFrame();
            _commands!.SetFramebuffer(CurrentFramebuffer(_drawFramebuffer));
        }

        private static TextureInfo GetTexture(int name)
        {
            if (name == 0)
            {
                return _white!;
            }
            if (!_textures.TryGetValue(name, out TextureInfo? info))
            {
                info = new TextureInfo();
                _textures[name] = info;
                if (name >= _nextTexture) _nextTexture = name + 1;
            }
            return info;
        }

        private static void AllocateTexture(TextureInfo info, int width, int height,
            bool depth, bool forceOpaqueAlpha)
        {
            InvalidateSets();
            info.Dispose();
            info.Width = (uint)Math.Max(width, 1);
            info.Height = (uint)Math.Max(height, 1);
            info.Depth = depth;
            info.ForceOpaqueAlpha = !depth && forceOpaqueAlpha;
            TextureUsage usage = depth
                ? TextureUsage.DepthStencil | TextureUsage.Sampled
                : TextureUsage.Sampled | TextureUsage.RenderTarget;
            VPixelFormat format = depth ? VPixelFormat.D24_UNorm_S8_UInt : VPixelFormat.R8_G8_B8_A8_UNorm;
            if (depth && !_gd!.GetPixelFormatSupport(
                format, TextureType.Texture2D, usage))
            {
                // Sampling a D24S8 attachment is optional in Vulkan. The
                // renderer already treats an incomplete depth-texture FBO as
                // "cel bands still work, outline disabled", so expose the
                // unsupported attachment through CheckFramebufferStatus rather
                // than turning a graphics option into a device-startup crash.
                info.Version++;
                InvalidateFramebuffers();
                return;
            }
            info.Texture = _factory!.CreateTexture(TextureDescription.Texture2D(
                info.Width, info.Height, 1, 1, format, usage));
            info.View = _factory.CreateTextureView(info.Texture);
            info.Version++;
            InvalidateFramebuffers();
        }

        private static void InvalidateFramebuffers()
        {
            SynchronizeResourceMutation();
            foreach (FramebufferInfo fb in _framebuffers.Values)
            {
                fb.Dispose();
            }
            DisposePipelineCache();
        }

        private static void InvalidateSets()
        {
            SynchronizeResourceMutation();
            foreach (UniformSlot slot in _uniformSlots)
            {
                slot.InvalidateSet();
            }
        }

        private static void ClearPipelineCache()
        {
            SynchronizeResourceMutation();
            DisposePipelineCache();
        }

        private static void DisposePipelineCache()
        {
            foreach (Pipeline pipeline in _pipelines.Values) pipeline.Dispose();
            _pipelines.Clear();
        }

        public static void Begin(PrimitiveType mode)
        {
            if (!_recording)
            {
                _batch.Clear();
                _vertexColorSet = false;
            }
            _primitive = mode;
            _primitiveStart = _batch.VertexCount;
        }

        public static void End()
        {
            int count = _batch.VertexCount - _primitiveStart;
            EmitIndices(_primitive, _primitiveStart, count);
            if (!_recording)
            {
                DrawData(_batch.Vertices.ToArray(), _batch.Triangles.ToArray(), _batch.Lines.ToArray());
                _batch.Clear();
            }
        }

        public static void Vertex2(float x, float y) => Vertex3(x, y, 0f);

        public static void Vertex3(float x, float y, float z)
        {
            List<float> v = _batch.Vertices;
            v.Add(x); v.Add(y); v.Add(z);
            v.Add(_currentColor.X); v.Add(_currentColor.Y); v.Add(_currentColor.Z); v.Add(_currentColor.W);
            v.Add(_currentNormal.X); v.Add(_currentNormal.Y); v.Add(_currentNormal.Z);
            v.Add(_currentTex0.X); v.Add(_currentTex0.Y); v.Add(_currentTex0.Z);
            v.Add(_currentTex1.X); v.Add(_currentTex1.Y);
            // OpenGL display lists record attribute-setting commands, not the
            // current attributes that happened to exist while GL_COMPILE ran.
            // Keep a compact per-vertex mask so attributes not set locally in
            // the list can come from glCallList-time current state. Immediate
            // mode captures all four current attributes at the Vertex call.
            int attributeMask = !_recording ? 0xF
                : (_vertexColorSet ? 0x1 : 0)
                | (_listNormalSet ? 0x2 : 0)
                | (_listTex0Set ? 0x4 : 0)
                | (_listTex1Set ? 0x8 : 0);
            v.Add(attributeMask);
            _batch.VertexCount++;
        }

        public static void Vertex3(Vector3 value) => Vertex3(value.X, value.Y, value.Z);

        public static void Color3(float r, float g, float b)
        {
            _currentColor = new Vector4(r, g, b, 1f);
            _vertexColorSet = true;
            if (_recording) _listColorSet = true;
        }

        public static void Color3(Vector3 value) => Color3(value.X, value.Y, value.Z);

        public static void Color4(float r, float g, float b, float a)
        {
            _currentColor = new Vector4(r, g, b, a);
            _vertexColorSet = true;
            if (_recording) _listColorSet = true;
        }

        public static void Normal3(float x, float y, float z)
        {
            _currentNormal = new Vector3(x, y, z);
            if (_recording) _listNormalSet = true;
        }

        public static void TexCoord2(float s, float t)
        {
            _currentTex0 = new Vector3(s, t, 0f);
            if (_recording) _listTex0Set = true;
        }

        public static void TexCoord3(float s, float t, float r)
        {
            _currentTex0 = new Vector3(s, t, r);
            if (_recording) _listTex0Set = true;
        }

        public static void TexCoord3(Vector3 value)
        {
            _currentTex0 = value;
            if (_recording) _listTex0Set = true;
        }

        public static void MultiTexCoord2(TextureUnit unit, float s, float t)
        {
            int index = Math.Clamp((int)unit - (int)TextureUnit.Texture0, 0, 1);
            if (index == 0)
            {
                _currentTex0 = new Vector3(s, t, 0f);
                if (_recording) _listTex0Set = true;
            }
            else
            {
                _currentTex1 = new Vector2(s, t);
                if (_recording) _listTex1Set = true;
            }
        }

        private static void EmitIndices(PrimitiveType mode, int b, int n)
        {
            List<uint> t = _batch.Triangles;
            List<uint> l = _batch.Lines;
            switch (mode)
            {
                case PrimitiveType.Triangles:
                    for (int i = 0; i + 2 < n; i += 3) { t.Add((uint)(b+i)); t.Add((uint)(b+i+1)); t.Add((uint)(b+i+2)); }
                    break;
                case PrimitiveType.Quads:
                    for (int i = 0; i + 3 < n; i += 4)
                    {
                        t.Add((uint)(b+i)); t.Add((uint)(b+i+1)); t.Add((uint)(b+i+2));
                        t.Add((uint)(b+i)); t.Add((uint)(b+i+2)); t.Add((uint)(b+i+3));
                    }
                    break;
                case PrimitiveType.TriangleStrip:
                    for (int i = 0; i + 2 < n; i++)
                    {
                        if ((i & 1) == 0) { t.Add((uint)(b+i)); t.Add((uint)(b+i+1)); t.Add((uint)(b+i+2)); }
                        else { t.Add((uint)(b+i+1)); t.Add((uint)(b+i)); t.Add((uint)(b+i+2)); }
                    }
                    break;
                case PrimitiveType.QuadStrip:
                    for (int i = 0; i + 3 < n; i += 2)
                    {
                        t.Add((uint)(b+i)); t.Add((uint)(b+i+1)); t.Add((uint)(b+i+3));
                        t.Add((uint)(b+i)); t.Add((uint)(b+i+3)); t.Add((uint)(b+i+2));
                    }
                    break;
                case PrimitiveType.TriangleFan:
                    for (int i = 1; i + 1 < n; i++) { t.Add((uint)b); t.Add((uint)(b+i)); t.Add((uint)(b+i+1)); }
                    break;
                case PrimitiveType.LineLoop:
                    for (int i = 0; i < n; i++) { l.Add((uint)(b+i)); l.Add((uint)(b+(i+1)%n)); }
                    break;
                case PrimitiveType.Lines:
                    for (int i = 0; i + 1 < n; i += 2) { l.Add((uint)(b+i)); l.Add((uint)(b+i+1)); }
                    break;
                case PrimitiveType.LineStrip:
                    for (int i = 0; i + 1 < n; i++) { l.Add((uint)(b+i)); l.Add((uint)(b+i+1)); }
                    break;
                default:
                    throw new ProgramException($"No Vulkan translation for primitive type {mode}.");
            }
        }

        public static int GenLists(int range)
        {
            int first = _nextList;
            _nextList += Math.Max(range, 1);
            return first;
        }

        public static void NewList(int list, ListMode mode)
        {
            _batch.Clear();
            _preListColor = _currentColor;
            _preListNormal = _currentNormal;
            _preListTex0 = _currentTex0;
            _preListTex1 = _currentTex1;
            _listColorSet = false;
            _listNormalSet = false;
            _listTex0Set = false;
            _listTex1Set = false;
            _recording = true;
            _recordList = list;
            _vertexColorSet = false;
        }

        public static void EndList()
        {
            _recording = false;
            _lists[_recordList] = new DisplayList
            {
                Vertices = _batch.Vertices.ToArray(),
                Triangles = _batch.Triangles.ToArray(),
                Lines = _batch.Lines.ToArray(),
                SetsColor = _listColorSet,
                FinalColor = _currentColor,
                SetsNormal = _listNormalSet,
                FinalNormal = _currentNormal,
                SetsTex0 = _listTex0Set,
                FinalTex0 = _currentTex0,
                SetsTex1 = _listTex1Set,
                FinalTex1 = _currentTex1
            };
            // GL_COMPILE records commands without executing them. Restore the
            // current attributes that existed before NewList; they will change
            // only when the list is actually called.
            _currentColor = _preListColor;
            _currentNormal = _preListNormal;
            _currentTex0 = _preListTex0;
            _currentTex1 = _preListTex1;
            _vertexColorSet = false;
            _batch.Clear();
        }

        public static void CallList(int list)
        {
            if (_lists.TryGetValue(list, out DisplayList? data))
            {
                DrawData(data.Vertices, data.Triangles, data.Lines);
                // Executing a display list updates OpenGL current attributes
                // for commands contained in the list.
                if (data.SetsColor) _currentColor = data.FinalColor;
                if (data.SetsNormal) _currentNormal = data.FinalNormal;
                if (data.SetsTex0) _currentTex0 = data.FinalTex0;
                if (data.SetsTex1) _currentTex1 = data.FinalTex1;
            }
        }

        public static void DeleteLists(int list, int range)
        {
            for (int i = 0; i < range; i++) _lists.Remove(list + i);
        }

        private static GeometrySlot AcquireGeometrySlot(uint vertexBytes, uint indexBytes)
        {
            if (_geometrySlotIndex == _geometrySlots.Count)
            {
                _geometrySlots.Add(new GeometrySlot());
            }
            GeometrySlot slot = _geometrySlots[_geometrySlotIndex++];
            // A slot is used once per open command list and is not reused until
            // the frame fence has completed. Repeatedly uploading unrelated
            // draws into offset zero of one vertex/index pair makes correctness
            // depend on transfer-to-vertex barriers emitted by the abstraction
            // layer and has produced torn geometry on real Vulkan drivers.
            slot.Ensure(_factory!, vertexBytes, indexBytes);
            return slot;
        }

        private static void DrawData(float[] vertices, uint[] triangles, uint[] lines)
        {
            if (_gd == null || vertices.Length == 0 || triangles.Length + lines.Length == 0)
            {
                return;
            }
            uint vertexBytes = (uint)(vertices.Length * sizeof(float));
            uint indexBytes = (uint)((triangles.Length + lines.Length) * sizeof(uint));
            GeometrySlot geometry = AcquireGeometrySlot(vertexBytes, indexBytes);
            uint[] indices = new uint[triangles.Length + lines.Length];
            Array.Copy(triangles, indices, triangles.Length);
            Array.Copy(lines, 0, indices, triangles.Length, lines.Length);

            BindCurrentFramebuffer();
            _commands!.UpdateBuffer(geometry.VertexBuffer!, 0, vertices);
            _commands.UpdateBuffer(geometry.IndexBuffer!, 0, indices);
            Veldrid.Framebuffer fb = CurrentFramebuffer(_drawFramebuffer);
            _commands!.SetVertexBuffer(0, geometry.VertexBuffer);
            _commands.SetIndexBuffer(geometry.IndexBuffer!, IndexFormat.UInt32);
            if (_scissor)
            {
                int scissorWidth = Math.Max(_scissorW, 1);
                int scissorHeight = Math.Max(_scissorH, 1);
                int scissorY = (int)fb.Height - _scissorY - scissorHeight;
                _commands.SetScissorRect(0, (uint)Math.Max(_scissorX, 0), (uint)Math.Max(scissorY, 0),
                    (uint)scissorWidth, (uint)scissorHeight);
            }
            float viewportWidth = _viewW > 0 ? _viewW : fb.Width;
            float viewportHeight = _viewH > 0 ? _viewH : fb.Height;
            float viewportY = fb.Height - _viewY - viewportHeight;
            _commands.SetViewport(0, new Veldrid.Viewport(_viewX, viewportY,
                viewportWidth, viewportHeight, 0, 1));

            byte[] ubo = BuildUniforms();
            UniformSlot uniformSlot = AcquireUniformSlot();
            // Veldrid 4.9 CommandList.UpdateBuffer emits a transfer -> vertex-
            // input barrier for every buffer, even uniform buffers. That does
            // not synchronize shader uniform reads. Each draw therefore owns
            // a persistently mapped UBO slot, updated directly before submit;
            // slots are not reused until the frame fence has completed.
            _gd.UpdateBuffer(uniformSlot.Buffer, 0, ubo);
            ResourceSet set = GetResourceSet(uniformSlot);
            if (triangles.Length > 0)
            {
                _commands.SetPipeline(GetPipeline(PrimitiveTopology.TriangleList, fb));
                _commands.SetGraphicsResourceSet(0, set);
                _commands.DrawIndexed((uint)triangles.Length, 1, 0, 0, 0);
            }
            if (lines.Length > 0)
            {
                _commands.SetPipeline(GetPipeline(PrimitiveTopology.LineList, fb));
                _commands.SetGraphicsResourceSet(0, set);
                _commands.DrawIndexed((uint)lines.Length, 1, (uint)triangles.Length, 0, 0);
            }
        }

        private static Pipeline GetClearPipeline(bool clearColor, bool clearDepth, bool clearStencil,
            Veldrid.Framebuffer fb)
        {
            string key = $"clear:{clearColor}:{clearDepth}:{clearStencil}:{_scissor}:"
                + $"{_maskR}{_maskG}{_maskB}{_maskA}:{_clearStencil & 0xFF}:"
                + $"{_stencilWriteMask & 0xFF}:{fb.OutputDescription.GetHashCode()}";
            if (_pipelines.TryGetValue(key, out Pipeline? pipeline))
            {
                return pipeline;
            }

            ColorWriteMask writeMask = 0;
            if (clearColor)
            {
                if (_maskR) writeMask |= ColorWriteMask.Red;
                if (_maskG) writeMask |= ColorWriteMask.Green;
                if (_maskB) writeMask |= ColorWriteMask.Blue;
                if (_maskA) writeMask |= ColorWriteMask.Alpha;
            }
            BlendAttachmentDescription attachment = BlendAttachmentDescription.Disabled;
            attachment.ColorWriteMask = writeMask;
            var blendState = new BlendStateDescription(RgbaFloat.White, attachment);

            var depthState = new DepthStencilStateDescription(
                depthTestEnabled: clearDepth,
                depthWriteEnabled: clearDepth,
                comparisonKind: ComparisonKind.Always);
            if (clearStencil)
            {
                var behavior = new StencilBehaviorDescription(
                    Veldrid.StencilOperation.Keep,
                    Veldrid.StencilOperation.Replace,
                    Veldrid.StencilOperation.Keep,
                    ComparisonKind.Always);
                depthState.StencilTestEnabled = true;
                depthState.StencilFront = behavior;
                depthState.StencilBack = behavior;
                depthState.StencilReadMask = 0xFF;
                depthState.StencilWriteMask = (byte)(_stencilWriteMask & 0xFF);
                depthState.StencilReference = (uint)(_clearStencil & 0xFF);
            }

            var raster = new RasterizerStateDescription(
                FaceCullMode.None, PolygonFillMode.Solid,
                FrontFace.CounterClockwise, true, _scissor);
            var description = new GraphicsPipelineDescription(
                blendState,
                depthState,
                raster,
                PrimitiveTopology.TriangleList,
                new ShaderSetDescription(Array.Empty<VertexLayoutDescription>(), _clearShaders!),
                new[] { _clearLayout! },
                fb.OutputDescription);
            pipeline = _factory!.CreateGraphicsPipeline(description);
            _pipelines[key] = pipeline;
            return pipeline;
        }

        private static Pipeline GetPipeline(PrimitiveTopology topology, Veldrid.Framebuffer fb)
        {
            ProgramInfo program = CurrentProgramInfo();
            string key = $"{program.Kind}:{topology}:{_depthTest}:{_depthWrite}:{_depthFunction}:"
                + $"{_stencilTest}:{_stencilFunction}:{_stencilReference}:{_stencilReadMask}:{_stencilWriteMask}:"
                + $"{_stencilFail}:{_stencilDepthFail}:{_stencilPass}:{_blend}:{_blendSrc}:{_blendDst}:"
                + $"{_cull}:{_cullFace}:{_polygonMode}:{_scissor}:{_maskR}{_maskG}{_maskB}{_maskA}:"
                + $"{fb.OutputDescription.GetHashCode()}";
            if (_pipelines.TryGetValue(key, out Pipeline? pipeline))
            {
                return pipeline;
            }

            ColorWriteMask writeMask = 0;
            if (_maskR) writeMask |= ColorWriteMask.Red;
            if (_maskG) writeMask |= ColorWriteMask.Green;
            if (_maskB) writeMask |= ColorWriteMask.Blue;
            if (_maskA) writeMask |= ColorWriteMask.Alpha;

            BlendAttachmentDescription attachment;
            if (_blend)
            {
                attachment = new BlendAttachmentDescription(true,
                    MapBlend(_blendSrc), MapBlend(_blendDst), BlendFunction.Add,
                    MapBlend(_blendSrc), MapBlend(_blendDst), BlendFunction.Add)
                {
                    ColorWriteMask = writeMask
                };
            }
            else
            {
                attachment = BlendAttachmentDescription.Disabled;
                attachment.ColorWriteMask = writeMask;
            }

            var blendState = new BlendStateDescription(RgbaFloat.White, attachment);
            var depthState = new DepthStencilStateDescription(_depthTest, _depthWrite, MapComparison(_depthFunction));
            if (_stencilTest)
            {
                var behavior = new StencilBehaviorDescription(
                    MapStencilOperation(_stencilFail),
                    MapStencilOperation(_stencilPass),
                    MapStencilOperation(_stencilDepthFail),
                    MapComparison(_stencilFunction));
                depthState.StencilTestEnabled = true;
                depthState.StencilFront = behavior;
                depthState.StencilBack = behavior;
                depthState.StencilReadMask = (byte)(_stencilReadMask & 0xFF);
                depthState.StencilWriteMask = (byte)(_stencilWriteMask & 0xFF);
                depthState.StencilReference = (uint)(_stencilReference & 0xFF);
            }
            FaceCullMode cull = !_cull ? FaceCullMode.None
                : _cullFace == TriangleFace.Front ? FaceCullMode.Front : FaceCullMode.Back;
            PolygonFillMode fill = _polygonMode == OpenTK.Graphics.OpenGL.PolygonMode.Line && _gd!.Features.FillModeWireframe
                ? PolygonFillMode.Wireframe : PolygonFillMode.Solid;
            var raster = new RasterizerStateDescription(cull, fill, FrontFace.CounterClockwise, true, _scissor);
            var vertexLayout = new VertexLayoutDescription(VertexStride,
                new VertexElementDescription("a_position", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float3) { Offset = 0 },
                new VertexElementDescription("a_color", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float4) { Offset = 12 },
                new VertexElementDescription("a_normal", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float3) { Offset = 28 },
                new VertexElementDescription("a_tex0", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float3) { Offset = 40 },
                new VertexElementDescription("a_tex1", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float2) { Offset = 52 },
                new VertexElementDescription("a_attr_mask", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float1) { Offset = 60 });
            Shader[] shaders = program.Kind switch
            {
                ProgramKind.Scene => _sceneShaders!,
                ProgramKind.Rtt => _rttShaders!,
                ProgramKind.Shift => _shiftShaders!,
                ProgramKind.Cel => _celShaders!,
                ProgramKind.Backdrop => _backdropShaders!,
                _ => _screenShaders!
            };
            var pd = new GraphicsPipelineDescription(
                blendState,
                depthState,
                raster,
                topology,
                new ShaderSetDescription(new[] { vertexLayout }, shaders),
                new[] { _layout! },
                fb.OutputDescription);
            pipeline = _factory!.CreateGraphicsPipeline(pd);
            _pipelines[key] = pipeline;
            return pipeline;
        }

        private static BlendFactor MapBlend(BlendingFactor value)
        {
            return value switch
            {
                BlendingFactor.Zero => BlendFactor.Zero,
                BlendingFactor.One => BlendFactor.One,
                BlendingFactor.SrcAlpha => BlendFactor.SourceAlpha,
                BlendingFactor.OneMinusSrcAlpha => BlendFactor.InverseSourceAlpha,
                BlendingFactor.DstAlpha => BlendFactor.DestinationAlpha,
                BlendingFactor.OneMinusDstAlpha => BlendFactor.InverseDestinationAlpha,
                BlendingFactor.SrcColor => BlendFactor.SourceColor,
                BlendingFactor.OneMinusSrcColor => BlendFactor.InverseSourceColor,
                BlendingFactor.DstColor => BlendFactor.DestinationColor,
                BlendingFactor.OneMinusDstColor => BlendFactor.InverseDestinationColor,
                _ => BlendFactor.SourceAlpha
            };
        }

        private static ComparisonKind MapComparison(DepthFunction value)
        {
            return value switch
            {
                DepthFunction.Never => ComparisonKind.Never,
                DepthFunction.Less => ComparisonKind.Less,
                DepthFunction.Equal => ComparisonKind.Equal,
                DepthFunction.Lequal => ComparisonKind.LessEqual,
                DepthFunction.Greater => ComparisonKind.Greater,
                DepthFunction.Notequal => ComparisonKind.NotEqual,
                DepthFunction.Gequal => ComparisonKind.GreaterEqual,
                DepthFunction.Always => ComparisonKind.Always,
                _ => ComparisonKind.LessEqual
            };
        }

        private static ComparisonKind MapComparison(StencilFunction value)
        {
            return value switch
            {
                StencilFunction.Never => ComparisonKind.Never,
                StencilFunction.Less => ComparisonKind.Less,
                StencilFunction.Equal => ComparisonKind.Equal,
                StencilFunction.Lequal => ComparisonKind.LessEqual,
                StencilFunction.Greater => ComparisonKind.Greater,
                StencilFunction.Notequal => ComparisonKind.NotEqual,
                StencilFunction.Gequal => ComparisonKind.GreaterEqual,
                StencilFunction.Always => ComparisonKind.Always,
                _ => ComparisonKind.Always
            };
        }

        private static Veldrid.StencilOperation MapStencilOperation(GLStencilOp value)
        {
            return value switch
            {
                GLStencilOp.Zero => Veldrid.StencilOperation.Zero,
                GLStencilOp.Replace => Veldrid.StencilOperation.Replace,
                GLStencilOp.Incr => Veldrid.StencilOperation.IncrementAndClamp,
                GLStencilOp.Decr => Veldrid.StencilOperation.DecrementAndClamp,
                GLStencilOp.Invert => Veldrid.StencilOperation.Invert,
                GLStencilOp.IncrWrap => Veldrid.StencilOperation.IncrementAndWrap,
                GLStencilOp.DecrWrap => Veldrid.StencilOperation.DecrementAndWrap,
                _ => Veldrid.StencilOperation.Keep
            };
        }

        private static ClearUniformSlot AcquireClearUniformSlot()
        {
            if (_clearUniformSlotIndex == _clearUniformSlots.Count)
            {
                _clearUniformSlots.Add(new ClearUniformSlot(_factory!, _clearLayout!));
            }
            return _clearUniformSlots[_clearUniformSlotIndex++];
        }

        private static UniformSlot AcquireUniformSlot()
        {
            if (_uniformSlotIndex == _uniformSlots.Count)
            {
                _uniformSlots.Add(new UniformSlot(_factory!));
            }
            return _uniformSlots[_uniformSlotIndex++];
        }

        private static ResourceSet GetResourceSet(UniformSlot slot)
        {
            TextureInfo t0 = _boundTextures[0] == 0 ? _white! : GetTexture(_boundTextures[0]);
            TextureInfo t1 = _boundTextures[1] == 0 ? _white! : GetTexture(_boundTextures[1]);
            if (t0.View == null) t0 = _white!;
            if (t1.View == null) t1 = _white!;
            string key = $"{_boundTextures[0]}:{t0.Version}:{t0.MinLinear}:{t0.MagLinear}:{t0.AddressU}:{t0.AddressV}"
                + $"|{_boundTextures[1]}:{t1.Version}:{t1.MinLinear}:{t1.MagLinear}:{t1.AddressU}:{t1.AddressV}";
            if (slot.Set != null && slot.SetKey == key)
            {
                return slot.Set;
            }

            slot.InvalidateSet();
            Sampler s0 = GetSampler(t0);
            Sampler s1 = GetSampler(t1);
            slot.Set = _factory!.CreateResourceSet(new ResourceSetDescription(
                _layout!, slot.Buffer, t0.View!, s0, t1.View!, s1));
            slot.SetKey = key;
            return slot.Set;
        }

        private static Sampler GetSampler(TextureInfo texture)
        {
            string key = $"{texture.MinLinear}:{texture.MagLinear}:{texture.AddressU}:{texture.AddressV}";
            if (_samplers.TryGetValue(key, out Sampler? sampler))
            {
                return sampler;
            }

            // Fruity's OpenGL path uses the non-mipmapped GL_NEAREST and
            // GL_LINEAR filters. Do not inherit Veldrid's stock Linear/Point
            // sampler descriptions here: they enable mip-level selection up to
            // uint.MaxValue and also collapse minification and magnification
            // into one choice. This compatibility image has exactly level 0,
            // so make the OpenGL state explicit and keep LOD fixed there.
            SamplerFilter filter = texture.MinLinear
                ? (texture.MagLinear
                    ? SamplerFilter.MinLinear_MagLinear_MipPoint
                    : SamplerFilter.MinLinear_MagPoint_MipPoint)
                : (texture.MagLinear
                    ? SamplerFilter.MinPoint_MagLinear_MipPoint
                    : SamplerFilter.MinPoint_MagPoint_MipPoint);
            var description = new SamplerDescription(
                texture.AddressU,
                texture.AddressV,
                SamplerAddressMode.Wrap,
                filter,
                comparisonKind: null,
                maximumAnisotropy: 0,
                minimumLod: 0,
                maximumLod: 0,
                lodBias: 0,
                borderColor: SamplerBorderColor.TransparentBlack);
            sampler = _factory!.CreateSampler(description);
            _samplers[key] = sampler;
            return sampler;
        }

        private static bool BoundTextureNeedsOpenGlVFlip(int unit)
        {
            int name = _boundTextures[unit];
            return name != 0
                && _textures.TryGetValue(name, out TextureInfo? texture)
                && texture.FlipVWhenSampledAsOpenGl;
        }

        private static bool BoundTextureForcesOpaqueAlpha(int unit)
        {
            int name = _boundTextures[unit];
            return name != 0
                && _textures.TryGetValue(name, out TextureInfo? texture)
                && texture.ForceOpaqueAlpha;
        }

        private static ProgramInfo CurrentProgramInfo()
        {
            if (_currentProgram != 0 && _programs.TryGetValue(_currentProgram, out ProgramInfo? info))
            {
                return info;
            }
            return _fixedProgram;
        }

        private static readonly ProgramInfo _fixedProgram = new() { Kind = ProgramKind.Screen };

        private static byte[] BuildUniforms()
        {
            var data = new byte[UboSize];
            ProgramInfo p = CurrentProgramInfo();
            WriteMatrix(data, ProjectionOffset, GetMatrix(p, "proj_mtx", identity: true));
            WriteMatrix(data, ViewOffset, GetMatrix(p, "view_mtx", identity: true));
            WriteMatrix(data, ViewInvOffset, GetMatrix(p, "view_inv_mtx", identity: true));
            WriteMatrix(data, TexOffset, GetMatrix(p, "tex_mtx", identity: true));

            if (p.Values.TryGetValue("mtx_stack", out object? stackObj) && stackObj is float[] stack)
            {
                int bytes = Math.Min(stack.Length * sizeof(float), 32 * 64);
                System.Buffer.BlockCopy(stack, 0, data, StackOffset, bytes);
                for (int m = stack.Length / 16; m < 32; m++) WriteIdentity(data, StackOffset + m * 64);
            }
            else
            {
                for (int m = 0; m < 32; m++) WriteIdentity(data, StackOffset + m * 64);
            }

            WriteVector4(data, ImmColorOffset, _currentColor);
            WriteVector4(data, OverrideOffset, GetVector4(p, "override_color", Vector4.One));
            WriteVector4(data, FadeOffset, GetVector4(p, "fade_color", Vector4.Zero));

            float matAlpha = GetFloat(p, "mat_alpha", 1f);
            float useTexture;
            if (p.Kind == ProgramKind.Scene)
            {
                useTexture = GetFloat(p, "use_texture", _boundTextures[0] != 0 ? 1f : 0f);
            }
            else if (_currentProgram == 0)
            {
                // In the compatibility pipeline, binding a texture and enabling
                // GL_TEXTURE_2D are distinct pieces of state. Shader programs do
                // not use this fixed-function enable, matching desktop OpenGL.
                useTexture = _texture2DEnabled[0] && _boundTextures[0] != 0 ? 1f : 0f;
            }
            else
            {
                useTexture = _boundTextures[0] != 0 ? 1f : 0f;
            }
            float showColors = GetFloat(p, "show_colors", 1f);
            float useOverride = GetFloat(p, "use_override", 0f);
            WriteVector4(data, Params0Offset, new Vector4(matAlpha, useTexture, showColors, useOverride));

            int alphaMode = !_alphaTest ? 0 : _alphaFunction switch
            {
                AlphaFunction.Never => 1,
                AlphaFunction.Less => 2,
                AlphaFunction.Equal => 3,
                AlphaFunction.Lequal => 4,
                AlphaFunction.Greater => 5,
                AlphaFunction.Notequal => 6,
                AlphaFunction.Gequal => 7,
                AlphaFunction.Always => 8,
                _ => 8
            };
            float flipTex0 = BoundTextureNeedsOpenGlVFlip(0) ? 1f : 0f;
            float flipTex1 = BoundTextureNeedsOpenGlVFlip(1) ? 1f : 0f;
            float fixedReplace = _currentProgram == 0
                && _textureEnvModes[0] == TextureEnvMode.Replace ? 1f : 0f;
            WriteVector4(data, Params1Offset,
                new Vector4(alphaMode, flipTex0, flipTex1, fixedReplace));
            float forceOpaqueTex0 = BoundTextureForcesOpaqueAlpha(0) ? 1f : 0f;
            float forceOpaqueTex1 = BoundTextureForcesOpaqueAlpha(1) ? 1f : 0f;
            WriteVector4(data, Params2Offset, new Vector4(
                _polygonOffsetFactor, _polygonOffsetUnits,
                _polygonOffsetFill ? 1f : 0f, forceOpaqueTex0));

            WriteVector4(data, Light1VectorOffset, new Vector4(GetVector3(p, "light1vec", Vector3.Zero), 0f));
            WriteVector4(data, Light2VectorOffset, new Vector4(GetVector3(p, "light2vec", Vector3.Zero), 0f));
            WriteVector4(data, Light1ColorOffset, new Vector4(GetVector3(p, "light1col", Vector3.Zero), 0f));
            WriteVector4(data, Light2ColorOffset, new Vector4(GetVector3(p, "light2col", Vector3.Zero), 0f));
            WriteVector4(data, DiffuseOffset, new Vector4(GetVector3(p, "diffuse", Vector3.Zero), 0f));
            WriteVector4(data, AmbientOffset, new Vector4(GetVector3(p, "ambient", Vector3.Zero), 0f));
            WriteVector4(data, SpecularOffset, new Vector4(GetVector3(p, "specular", Vector3.Zero), 0f));
            WriteVector4(data, EmissionOffset, new Vector4(GetVector3(p, "emission", Vector3.Zero), 0f));
            WriteVector4(data, FogColorOffset, GetVector4(p, "fog_color", Vector4.Zero));
            WriteVector4(data, PaletteOverrideOffset, GetVector4(p, "pal_override_color", Vector4.Zero));
            WriteVector4(data, FlatColorOffset, new Vector4(GetVector3(p, "flat_color", Vector3.Zero), 0f));

            WriteVector4(data, Scene0Offset, new Vector4(
                GetFloat(p, "use_light", 0f),
                GetFloat(p, "fog_enable", 0f),
                GetFloat(p, "fog_min", 0f),
                GetFloat(p, "fog_max", 1f)));
            WriteVector4(data, Scene1Offset, new Vector4(
                GetFloat(p, "texgen_mode", 0f),
                GetFloat(p, "mat_mode", 0f),
                GetFloat(p, "use_pal_override", 0f),
                GetFloat(p, "cel_bands", 0f)));
            WriteVector4(data, Scene2Offset, new Vector4(
                GetFloat(p, "use_flat", 0f),
                GetFloat(p, "strength", 0f),
                forceOpaqueTex1, _alphaReference));
            WritePackedVec3Array(data, ToonTableOffset, GetFloatArray(p, "toon_table"), 32);

            WriteVector4(data, Rtt0Offset, new Vector4(
                GetFloat(p, "alpha", 1f),
                GetFloat(p, "use_mask", 0f),
                GetFloat(p, "view_width", 1f),
                GetFloat(p, "view_height", 1f)));
            WriteVector4(data, Cel0Offset, new Vector4(
                GetFloat(p, "texel_w", 1f),
                GetFloat(p, "texel_h", 1f),
                GetFloat(p, "outline", 0f),
                GetFloat(p, "near_plane", 0f)));
            WriteVector4(data, Cel1Offset, new Vector4(
                GetFloat(p, "far_plane", 1f),
                GetFloat(p, "depth_quantum", 0f),
                GetFloat(p, "probe", 0f),
                0f));
            WriteVector4(data, Shift0Offset, new Vector4(
                GetFloat(p, "shift_idx", 0f),
                GetFloat(p, "shift_fac", 0f),
                GetFloat(p, "lerp_fac", 0f),
                GetFloat(p, "white_fac", 0f)));
            WritePackedFloatArray(data, ShiftTableOffset, GetFloatArray(p, "shift_table"), 64);
            WritePackedFloatArray(data, WhiteTableOffset, GetFloatArray(p, "white_table"), 192);
            WriteVector4(data, ImmNormalOffset, new Vector4(_currentNormal, 0f));
            WriteVector4(data, ImmTex0Offset, new Vector4(_currentTex0, 0f));
            WriteVector4(data, ImmTex1Offset, new Vector4(_currentTex1.X, _currentTex1.Y, 0f, 0f));
            return data;
        }

        private static float[] GetMatrix(ProgramInfo p, string name, bool identity)
        {
            if (p.Values.TryGetValue(name, out object? value) && value is float[] data && data.Length >= 16)
            {
                return data;
            }
            return identity ? IdentityFloats() : new float[16];
        }

        private static Vector4 GetVector4(ProgramInfo p, string name, Vector4 fallback)
        {
            if (p.Values.TryGetValue(name, out object? value))
            {
                if (value is Vector4 v4) return v4;
                if (value is Vector3 v3) return new Vector4(v3, 1f);
            }
            return fallback;
        }

        private static Vector3 GetVector3(ProgramInfo p, string name, Vector3 fallback)
        {
            if (p.Values.TryGetValue(name, out object? value))
            {
                if (value is Vector3 v3) return v3;
                if (value is Vector4 v4) return v4.Xyz;
            }
            return fallback;
        }

        private static float[] GetFloatArray(ProgramInfo p, string name)
        {
            return p.Values.TryGetValue(name, out object? value) && value is float[] array
                ? array : Array.Empty<float>();
        }

        private static float GetFloat(ProgramInfo p, string name, float fallback)
        {
            if (p.Values.TryGetValue(name, out object? value))
            {
                if (value is float f) return f;
                if (value is int i) return i;
            }
            return fallback;
        }

        private static readonly float[] _identity =
        {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };

        private static float[] IdentityFloats() => (float[])_identity.Clone();
        private static void WriteIdentity(byte[] data, int offset) => System.Buffer.BlockCopy(_identity, 0, data, offset, 64);
        private static void WriteMatrix(byte[] data, int offset, float[] matrix) => System.Buffer.BlockCopy(matrix, 0, data, offset, 64);
        private static void WriteVector4(byte[] data, int offset, Vector4 value)
        {
            float[] f = { value.X, value.Y, value.Z, value.W };
            System.Buffer.BlockCopy(f, 0, data, offset, 16);
        }

        private static void WritePackedVec3Array(byte[] data, int offset, float[] values, int count)
        {
            for (int i = 0; i < count; i++)
            {
                int source = i * 3;
                WriteVector4(data, offset + i * 16, new Vector4(
                    source < values.Length ? values[source] : 0f,
                    source + 1 < values.Length ? values[source + 1] : 0f,
                    source + 2 < values.Length ? values[source + 2] : 0f,
                    0f));
            }
        }

        private static void WritePackedFloatArray(byte[] data, int offset, float[] values, int count)
        {
            for (int i = 0; i < count; i += 4)
            {
                WriteVector4(data, offset + i * 4, new Vector4(
                    i < values.Length ? values[i] : 0f,
                    i + 1 < values.Length ? values[i + 1] : 0f,
                    i + 2 < values.Length ? values[i + 2] : 0f,
                    i + 3 < values.Length ? values[i + 3] : 0f));
            }
        }

        public static int GenTexture()
        {
            int name = _nextTexture++;
            _textures[name] = new TextureInfo();
            return name;
        }

        public static void DeleteTexture(int name)
        {
            if (name == 0)
            {
                return;
            }
            for (int unit = 0; unit < _boundTextures.Length; unit++)
            {
                if (_boundTextures[unit] == name)
                {
                    _boundTextures[unit] = 0;
                }
            }
            if (_textures.Remove(name, out TextureInfo? info))
            {
                InvalidateSets();
                info.Dispose();
                InvalidateFramebuffers();
            }
        }

        public static void ActiveTexture(TextureUnit unit)
        {
            _activeTextureUnit = Math.Clamp((int)unit - (int)TextureUnit.Texture0, 0, 1);
        }

        public static void BindTexture(GLTexture target, int name)
        {
            _boundTextures[_activeTextureUnit] = name;
            if (name != 0) GetTexture(name);
        }

        public static void TexParameter(GLTexture target, TextureParameterName pname, int param)
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0) return;
            TextureInfo info = GetTexture(name);
            if (pname == TextureParameterName.TextureMinFilter)
            {
                bool linear = param == (int)TextureMinFilter.Linear
                    || param == (int)TextureMinFilter.LinearMipmapNearest
                    || param == (int)TextureMinFilter.LinearMipmapLinear;
                info.MinLinear = linear;
                return;
            }
            if (pname == TextureParameterName.TextureMagFilter)
            {
                info.MagLinear = param == (int)TextureMagFilter.Linear;
                return;
            }
            if (pname == TextureParameterName.TextureWrapS || pname == TextureParameterName.TextureWrapT)
            {
                SamplerAddressMode address = param == (int)TextureWrapMode.MirroredRepeat
                    ? SamplerAddressMode.Mirror
                    : param == (int)TextureWrapMode.ClampToEdge || param == (int)TextureWrapMode.Clamp
                        ? SamplerAddressMode.Clamp
                        : SamplerAddressMode.Wrap;
                if (pname == TextureParameterName.TextureWrapS)
                {
                    if (address == info.AddressU) return;
                    info.AddressU = address;
                }
                else
                {
                    if (address == info.AddressV) return;
                    info.AddressV = address;
                }
                // Address modes are also part of the resource-set
                // cache key; keep old sets alive and select/create the matching
                // sampler lazily on the next draw.
            }
        }

        public static void TexImage2D(GLTexture target, int level, PixelInternalFormat internalFormat,
            int width, int height, int border, GLPixelFormat format, PixelType type, IntPtr pixels)
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0) return;
            bool depth = internalFormat == PixelInternalFormat.Depth24Stencil8
                || format == GLPixelFormat.DepthStencil;
            TextureInfo info = GetTexture(name);
            AllocateTexture(info, width, height, depth,
                internalFormat == PixelInternalFormat.Rgb);
            if (pixels != IntPtr.Zero && !depth)
            {
                int srcBpp = format == GLPixelFormat.Rgb ? 3 : 4;
                byte[] src = new byte[Math.Max(width, 0) * Math.Max(height, 0) * srcBpp];
                Marshal.Copy(pixels, src, 0, src.Length);
                UploadBytes(info, src, width, height, format);
            }
        }

        public static void TexImage2D<T>(GLTexture target, int level, PixelInternalFormat internalFormat,
            int width, int height, int border, GLPixelFormat format, PixelType type, T[] pixels) where T : struct
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0) return;
            bool depth = internalFormat == PixelInternalFormat.Depth24Stencil8
                || format == GLPixelFormat.DepthStencil;
            TextureInfo info = GetTexture(name);
            AllocateTexture(info, width, height, depth,
                internalFormat == PixelInternalFormat.Rgb);
            if (!depth && pixels.Length > 0)
            {
                UploadBytes(info, BytesOf(pixels), width, height, format);
            }
        }

        public static void TexSubImage2D<T>(GLTexture target, int level, int xoffset, int yoffset,
            int width, int height, GLPixelFormat format, PixelType type, T[] pixels) where T : struct
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0) return;
            TextureInfo info = GetTexture(name);
            if (info.Texture == null) return;
            byte[] data = ConvertPixels(BytesOf(pixels), width, height, format);
            _gd!.UpdateTexture(info.Texture, data, (uint)xoffset, (uint)yoffset, 0,
                (uint)width, (uint)height, 1, 0, 0);
        }

        public static void TexSubImage2D(GLTexture target, int level, int xoffset, int yoffset,
            int width, int height, GLPixelFormat format, PixelType type, IntPtr pixels)
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0 || pixels == IntPtr.Zero) return;
            TextureInfo info = GetTexture(name);
            if (info.Texture == null) return;
            int bytesPerPixel = format == GLPixelFormat.Rgb ? 3 : 4;
            byte[] source = new byte[Math.Max(width, 0) * Math.Max(height, 0) * bytesPerPixel];
            Marshal.Copy(pixels, source, 0, source.Length);
            byte[] data = ConvertPixels(source, width, height, format);
            _gd!.UpdateTexture(info.Texture, data, (uint)xoffset, (uint)yoffset, 0,
                (uint)width, (uint)height, 1, 0, 0);
        }

        private static byte[] BytesOf<T>(T[] values) where T : struct
        {
            int size = Marshal.SizeOf<T>() * values.Length;
            byte[] bytes = new byte[size];
            GCHandle handle = GCHandle.Alloc(values, GCHandleType.Pinned);
            try { Marshal.Copy(handle.AddrOfPinnedObject(), bytes, 0, size); }
            finally { handle.Free(); }
            return bytes;
        }

        private static byte[] ConvertPixels(byte[] source, int width, int height, GLPixelFormat format)
        {
            if (format != GLPixelFormat.Rgb) return source;
            int count = Math.Max(width, 0) * Math.Max(height, 0);
            byte[] rgba = new byte[count * 4];
            for (int i = 0; i < count; i++)
            {
                rgba[i*4] = source[i*3];
                rgba[i*4+1] = source[i*3+1];
                rgba[i*4+2] = source[i*3+2];
                rgba[i*4+3] = 255;
            }
            return rgba;
        }

        private static void UploadBytes(TextureInfo info, byte[] source, int width, int height, GLPixelFormat format)
        {
            byte[] data = ConvertPixels(source, width, height, format);
            _gd!.UpdateTexture(info.Texture!, data, 0, 0, 0, (uint)width, (uint)height, 1, 0, 0);
        }

        public static void CopyTexSubImage2D(GLTexture target, int level, int xoffset, int yoffset,
            int x, int y, int width, int height)
        {
            int name = _boundTextures[_activeTextureUnit];
            if (name == 0 || _gd == null) return;
            TextureInfo dst = GetTexture(name);
            if (dst.Texture == null) return;
            Veldrid.Texture src = CurrentFramebuffer(_readFramebuffer).ColorTargets[0].Target;
            int sourceY = y;
            if (_gd.IsUvOriginTopLeft)
            {
                sourceY = Math.Max((int)src.Height - y - height, 0);
            }
            EnsureFrame();
            _commands!.CopyTexture(src, (uint)x, (uint)sourceY, 0, 0, 0,
                dst.Texture, (uint)xoffset, (uint)yoffset, 0, 0, 0,
                (uint)width, (uint)height, 1, 1);
            dst.FlipVWhenSampledAsOpenGl = _gd.IsUvOriginTopLeft;
        }

        public static void ReadPixels<T>(int x, int y, int width, int height,
            GLPixelFormat format, PixelType type, T[] pixels) where T : struct
        {
            if (_gd == null || _factory == null || _commands == null
                || width <= 0 || height <= 0 || pixels.Length == 0)
            {
                Array.Clear(pixels, 0, pixels.Length);
                return;
            }
            if (type != PixelType.UnsignedByte
                || (format != GLPixelFormat.Rgb && format != GLPixelFormat.Rgba))
            {
                throw new ProgramException($"Vulkan readback does not support {format}/{type}.");
            }

            Veldrid.Texture source = CurrentFramebuffer(_readFramebuffer).ColorTargets[0].Target;
            bool forceOpaqueReadAlpha = _readFramebuffer != 0
                && _framebuffers.TryGetValue(_readFramebuffer, out FramebufferInfo? readFb)
                && readFb.ColorTexture != 0
                && _textures.TryGetValue(readFb.ColorTexture, out TextureInfo? readTexture)
                && readTexture.ForceOpaqueAlpha;
            int copyX = Math.Clamp(x, 0, Math.Max((int)source.Width - width, 0));
            int copyY = Math.Clamp(y, 0, Math.Max((int)source.Height - height, 0));
            if (_gd.IsUvOriginTopLeft)
            {
                copyY = Math.Max((int)source.Height - copyY - height, 0);
            }

            using Veldrid.Texture staging = _factory.CreateTexture(TextureDescription.Texture2D(
                (uint)width, (uint)height, 1, 1, source.Format, TextureUsage.Staging));

            // Keep the readback copy in the same ordered command stream as the
            // draws it reads. A synchronous CPU readback still has to wait for
            // that copy, but it does not require a device-wide idle: submit the
            // current list with the reusable frame fence, wait only that
            // submission, then map the staging texture.
            EnsureFrame();
            _commands!.CopyTexture(source, (uint)copyX, (uint)copyY, 0, 0, 0,
                staging, 0, 0, 0, 0, 0, (uint)width, (uint)height, 1, 1);
            _commands.End();
            _gd.SubmitCommands(_commands, _frameFence!);
            _commandsOpen = false;
            _frameInFlight = true;
            _gd.WaitForFence(_frameFence!);
            _frameFence!.Reset();
            _frameInFlight = false;
            _uniformSlotIndex = 0;
            _clearUniformSlotIndex = 0;
            _geometrySlotIndex = 0;

            int outputBpp = format == GLPixelFormat.Rgb ? 3 : 4;
            byte[] output = new byte[width * height * outputBpp];
            MappedResource mapped = _gd.Map(staging, MapMode.Read);
            try
            {
                byte[] row = new byte[width * 4];
                bool bgra = source.Format.ToString().StartsWith("B8_G8_R8_A8",
                    StringComparison.Ordinal);
                for (int rowIndex = 0; rowIndex < height; rowIndex++)
                {
                    // OpenGL ReadPixels returns the bottom row first. Vulkan
                    // staging textures on top-left-origin devices map the top
                    // row first, so reverse only the mapped row order after
                    // converting the requested source rectangle above.
                    int sourceRow = _gd.IsUvOriginTopLeft ? height - 1 - rowIndex : rowIndex;
                    IntPtr rowPtr = IntPtr.Add(mapped.Data, checked((int)(sourceRow * mapped.RowPitch)));
                    Marshal.Copy(rowPtr, row, 0, row.Length);
                    int dest = rowIndex * width * outputBpp;
                    for (int col = 0; col < width; col++)
                    {
                        int src = col * 4;
                        byte r = bgra ? row[src + 2] : row[src];
                        byte g = row[src + 1];
                        byte b = bgra ? row[src] : row[src + 2];
                        output[dest++] = r;
                        output[dest++] = g;
                        output[dest++] = b;
                        if (outputBpp == 4)
                        {
                            output[dest++] = forceOpaqueReadAlpha ? (byte)255 : row[src + 3];
                        }
                    }
                }
            }
            finally
            {
                _gd.Unmap(staging);
            }

            int targetBytes = Marshal.SizeOf<T>() * pixels.Length;
            int copyBytes = Math.Min(targetBytes, output.Length);
            GCHandle handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            try
            {
                Marshal.Copy(output, 0, handle.AddrOfPinnedObject(), copyBytes);
            }
            finally
            {
                handle.Free();
            }
        }

        public static int GenFramebuffer()
        {
            int id = _nextFramebuffer++;
            _framebuffers[id] = new FramebufferInfo();
            return id;
        }

        public static void BindFramebuffer(FramebufferTarget target, int framebuffer)
        {
            if (target == FramebufferTarget.ReadFramebuffer) _readFramebuffer = framebuffer;
            else if (target == FramebufferTarget.DrawFramebuffer) _drawFramebuffer = framebuffer;
            else { _drawFramebuffer = framebuffer; _readFramebuffer = framebuffer; }
        }

        public static void FramebufferTexture2D(FramebufferTarget target, GLFramebufferAttachment attachment,
            GLTexture textarget, int texture, int level)
        {
            int id = target == FramebufferTarget.ReadFramebuffer ? _readFramebuffer : _drawFramebuffer;
            if (id == 0 || !_framebuffers.TryGetValue(id, out FramebufferInfo? fb)) return;
            SynchronizeResourceMutation();
            fb.Dispose();
            if (attachment == GLFramebufferAttachment.ColorAttachment0)
            {
                fb.ColorTexture = texture;
                if (texture != 0)
                {
                    GetTexture(texture).FlipVWhenSampledAsOpenGl = _gd!.IsUvOriginTopLeft;
                }
            }
            else if (attachment == GLFramebufferAttachment.DepthStencilAttachment
                || attachment == GLFramebufferAttachment.DepthAttachment)
            {
                // OpenGL has one object binding per attachment point. Attaching
                // a texture replaces any renderbuffer previously bound there;
                // otherwise detaching this texture later would incorrectly
                // resurrect a stale renderbuffer.
                fb.DepthTexture = texture;
                fb.DepthRenderbuffer = 0;
            }
            DisposePipelineCache();
        }

        public static int GenRenderbuffer()
        {
            int id = _nextRenderbuffer++;
            _renderbuffers[id] = new RenderbufferInfo();
            return id;
        }

        public static void BindRenderbuffer(RenderbufferTarget target, int renderbuffer) => _boundRenderbuffer = renderbuffer;

        public static void RenderbufferStorage(RenderbufferTarget target,
            OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat, int width, int height)
        {
            if (_boundRenderbuffer == 0 || !_renderbuffers.TryGetValue(_boundRenderbuffer, out RenderbufferInfo? rb)) return;
            SynchronizeResourceMutation();
            rb.Dispose();
            rb.Width = (uint)Math.Max(width, 1);
            rb.Height = (uint)Math.Max(height, 1);
            rb.Texture = _factory!.CreateTexture(TextureDescription.Texture2D(rb.Width, rb.Height, 1, 1,
                VPixelFormat.D24_UNorm_S8_UInt, TextureUsage.DepthStencil));
            InvalidateFramebuffers();
        }

        public static void FramebufferRenderbuffer(FramebufferTarget target, GLFramebufferAttachment attachment,
            RenderbufferTarget renderbufferTarget, int renderbuffer)
        {
            int id = target == FramebufferTarget.ReadFramebuffer ? _readFramebuffer : _drawFramebuffer;
            if (id == 0 || !_framebuffers.TryGetValue(id, out FramebufferInfo? fb)) return;
            SynchronizeResourceMutation();
            fb.Dispose();
            fb.DepthRenderbuffer = renderbuffer;
            fb.DepthTexture = 0;
            DisposePipelineCache();
        }

        public static FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target)
        {
            int id = target == FramebufferTarget.ReadFramebuffer ? _readFramebuffer : _drawFramebuffer;
            if (id == 0)
            {
                return FramebufferErrorCode.FramebufferComplete;
            }
            if (!_framebuffers.TryGetValue(id, out FramebufferInfo? fb))
            {
                return FramebufferErrorCode.FramebufferIncompleteMissingAttachment;
            }

            bool attached = false;
            if (fb.ColorTexture != 0)
            {
                attached = true;
                if (!_textures.TryGetValue(fb.ColorTexture, out TextureInfo? color)
                    || color.Texture == null)
                {
                    return FramebufferErrorCode.FramebufferIncompleteAttachment;
                }
            }
            if (fb.DepthTexture != 0)
            {
                attached = true;
                if (!_textures.TryGetValue(fb.DepthTexture, out TextureInfo? depth)
                    || depth.Texture == null)
                {
                    return FramebufferErrorCode.FramebufferIncompleteAttachment;
                }
            }
            if (fb.DepthRenderbuffer != 0)
            {
                attached = true;
                if (!_renderbuffers.TryGetValue(fb.DepthRenderbuffer, out RenderbufferInfo? depth)
                    || depth.Texture == null)
                {
                    return FramebufferErrorCode.FramebufferIncompleteAttachment;
                }
            }
            return attached
                ? FramebufferErrorCode.FramebufferComplete
                : FramebufferErrorCode.FramebufferIncompleteMissingAttachment;
        }

        public static void DeleteFramebuffer(int framebuffer)
        {
            if (framebuffer == 0)
            {
                return;
            }
            if (_framebuffers.Remove(framebuffer, out FramebufferInfo? fb))
            {
                SynchronizeResourceMutation();
                if (_drawFramebuffer == framebuffer) _drawFramebuffer = 0;
                if (_readFramebuffer == framebuffer) _readFramebuffer = 0;
                fb.Dispose();
                DisposePipelineCache();
            }
        }

        public static void DeleteRenderbuffer(int renderbuffer)
        {
            if (renderbuffer == 0)
            {
                return;
            }
            if (_renderbuffers.Remove(renderbuffer, out RenderbufferInfo? rb))
            {
                SynchronizeResourceMutation();
                if (_boundRenderbuffer == renderbuffer) _boundRenderbuffer = 0;
                rb.Dispose();
                InvalidateFramebuffers();
            }
        }

        public static void GetFramebufferAttachmentParameter(FramebufferTarget target,
            GLFramebufferAttachment attachment, FramebufferParameterName pname, out int result)
        {
            const int GlTextureObject = 0x1702;
            const int GlRenderbufferObject = 0x8D41;
            result = 0;

            int id = target == FramebufferTarget.ReadFramebuffer ? _readFramebuffer : _drawFramebuffer;
            if (pname == FramebufferParameterName.FramebufferAttachmentDepthSize)
            {
                if (id == 0)
                {
                    result = 24;
                    return;
                }
                if (_framebuffers.TryGetValue(id, out FramebufferInfo? depthInfo)
                    && (depthInfo.DepthTexture != 0 || depthInfo.DepthRenderbuffer != 0))
                {
                    result = 24;
                }
                return;
            }

            if (id == 0 || !_framebuffers.TryGetValue(id, out FramebufferInfo? fb))
            {
                return;
            }

            int objectName = 0;
            int objectType = 0;
            if (attachment == GLFramebufferAttachment.ColorAttachment0)
            {
                objectName = fb.ColorTexture;
                if (objectName != 0) objectType = GlTextureObject;
            }
            else if (attachment == GLFramebufferAttachment.DepthAttachment
                || attachment == GLFramebufferAttachment.StencilAttachment
                || attachment == GLFramebufferAttachment.DepthStencilAttachment)
            {
                if (fb.DepthTexture != 0)
                {
                    objectName = fb.DepthTexture;
                    objectType = GlTextureObject;
                }
                else if (fb.DepthRenderbuffer != 0)
                {
                    objectName = fb.DepthRenderbuffer;
                    objectType = GlRenderbufferObject;
                }
            }

            if (pname == FramebufferParameterName.FramebufferAttachmentObjectName)
            {
                result = objectName;
            }
            else if (pname == FramebufferParameterName.FramebufferAttachmentObjectType)
            {
                result = objectType;
            }
        }

        public static int CreateShader(ShaderType type)
        {
            int id = _nextShader++;
            _shaders[id] = new ShaderInfo { Type = type };
            return id;
        }

        public static void ShaderSource(int shader, string source)
        {
            if (_shaders.TryGetValue(shader, out ShaderInfo? info))
            {
                info.Source = source;
                info.Compiled = false;
                info.InfoLog = "";
            }
        }

        public static void CompileShader(int shader)
        {
            if (!_shaders.TryGetValue(shader, out ShaderInfo? info))
            {
                return;
            }

            bool known = info.Type switch
            {
                ShaderType.VertexShader =>
                    String.Equals(info.Source, Shaders.VertexShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.RttVertexShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.BackdropVertexShader, StringComparison.Ordinal),
                ShaderType.FragmentShader =>
                    String.Equals(info.Source, Shaders.FragmentShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.RttFragmentShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.ShiftFragmentShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.CelFragmentShader, StringComparison.Ordinal)
                    || String.Equals(info.Source, Shaders.BackdropFragmentShader, StringComparison.Ordinal),
                _ => false
            };

            info.Compiled = known;
            info.InfoLog = known ? ""
                : "The Vulkan compatibility backend has no translated equivalent for this shader source.";
        }

        public static void GetShader(int shader, ShaderParameter pname, out int value)
        {
            value = _shaders.TryGetValue(shader, out ShaderInfo? info) && info.Compiled ? 1 : 0;
        }

        public static string GetShaderInfoLog(int shader) =>
            _shaders.TryGetValue(shader, out ShaderInfo? info) ? info.InfoLog : "Unknown shader object.";

        public static void DeleteShader(int shader) => _shaders.Remove(shader);

        public static int CreateProgram()
        {
            int id = _nextProgram++;
            _programs[id] = new ProgramInfo();
            return id;
        }

        public static void AttachShader(int program, int shader)
        {
            if (_programs.TryGetValue(program, out ProgramInfo? p)
                && _shaders.TryGetValue(shader, out ShaderInfo? info))
            {
                p.Shaders[shader] = info;
            }
        }

        public static void DetachShader(int program, int shader)
        {
            if (_programs.TryGetValue(program, out ProgramInfo? p)) p.Shaders.Remove(shader);
        }

        public static void LinkProgram(int program)
        {
            if (!_programs.TryGetValue(program, out ProgramInfo? p))
            {
                return;
            }

            p.Linked = false;
            p.InfoLog = "";
            ShaderInfo? vertex = null;
            ShaderInfo? fragment = null;
            foreach (ShaderInfo shader in p.Shaders.Values)
            {
                if (!shader.Compiled)
                {
                    p.InfoLog = "An attached shader did not compile for the Vulkan backend.";
                    return;
                }
                if (shader.Type == ShaderType.VertexShader) vertex = shader;
                else if (shader.Type == ShaderType.FragmentShader) fragment = shader;
            }
            if (vertex == null || fragment == null)
            {
                p.InfoLog = "A Vulkan program requires one translated vertex shader and one translated fragment shader.";
                return;
            }

            if (String.Equals(vertex.Source, Shaders.VertexShader, StringComparison.Ordinal)
                && String.Equals(fragment.Source, Shaders.FragmentShader, StringComparison.Ordinal))
            {
                p.Kind = ProgramKind.Scene;
            }
            else if (String.Equals(vertex.Source, Shaders.RttVertexShader, StringComparison.Ordinal)
                && String.Equals(fragment.Source, Shaders.RttFragmentShader, StringComparison.Ordinal))
            {
                p.Kind = ProgramKind.Rtt;
            }
            else if (String.Equals(vertex.Source, Shaders.RttVertexShader, StringComparison.Ordinal)
                && String.Equals(fragment.Source, Shaders.ShiftFragmentShader, StringComparison.Ordinal))
            {
                p.Kind = ProgramKind.Shift;
            }
            else if (String.Equals(vertex.Source, Shaders.RttVertexShader, StringComparison.Ordinal)
                && String.Equals(fragment.Source, Shaders.CelFragmentShader, StringComparison.Ordinal))
            {
                p.Kind = ProgramKind.Cel;
            }
            else if (String.Equals(vertex.Source, Shaders.BackdropVertexShader, StringComparison.Ordinal)
                && String.Equals(fragment.Source, Shaders.BackdropFragmentShader, StringComparison.Ordinal))
            {
                p.Kind = ProgramKind.Backdrop;
            }
            else
            {
                p.InfoLog = "The attached shader pair has no Vulkan compatibility pipeline.";
                return;
            }

            p.Linked = true;
        }

        public static void GetProgram(int program, GetProgramParameterName pname, out int value)
        {
            value = _programs.TryGetValue(program, out ProgramInfo? info) && info.Linked ? 1 : 0;
        }

        public static string GetProgramInfoLog(int program) =>
            _programs.TryGetValue(program, out ProgramInfo? info) ? info.InfoLog : "Unknown program object.";
        public static void DeleteProgram(int program) => _programs.Remove(program);
        public static void UseProgram(int program) => _currentProgram = program;

        public static int GetUniformLocation(int program, string name)
        {
            if (!_programs.ContainsKey(program)) return -1;
            foreach (KeyValuePair<int, (int Program, string Name)> kv in _locations)
            {
                if (kv.Value.Program == program && kv.Value.Name == name) return kv.Key;
            }
            int id = _nextLocation++;
            _locations[id] = (program, name);
            return id;
        }

        private static void SetUniform(int location, object value)
        {
            if (location < 0 || !_locations.TryGetValue(location, out var loc)
                || !_programs.TryGetValue(loc.Program, out ProgramInfo? p)) return;
            p.Values[loc.Name] = value;
        }

        public static void Uniform1(int location, int value) => SetUniform(location, value);
        public static void Uniform1(int location, float value) => SetUniform(location, value);
        public static void Uniform1(int location, int count, float[] value) => SetUniform(location, (float[])value.Clone());
        public static void Uniform3(int location, Vector3 value) => SetUniform(location, value);
        public static void Uniform3(int location, int count, float[] value) => SetUniform(location, (float[])value.Clone());
        public static void Uniform4(int location, Vector4 value) => SetUniform(location, value);
        public static void Uniform4(int location, ref Vector4 value) => SetUniform(location, value);
        public static void Uniform4(int location, float x, float y, float z, float w) => SetUniform(location, new Vector4(x,y,z,w));
        public static void Uniform4(int location, int x, int y, int z, int w) => SetUniform(location, new Vector4(x,y,z,w));

        public static void UniformMatrix4(int location, bool transpose, ref Matrix4 matrix)
        {
            SetUniform(location, MatrixFloats(matrix));
        }

        public static void UniformMatrix4(int location, int count, bool transpose, float[] value)
        {
            SetUniform(location, (float[])value.Clone());
        }

        private static float[] MatrixFloats(Matrix4 m) => new[]
        {
            m.Row0.X,m.Row0.Y,m.Row0.Z,m.Row0.W,
            m.Row1.X,m.Row1.Y,m.Row1.Z,m.Row1.W,
            m.Row2.X,m.Row2.Y,m.Row2.Z,m.Row2.W,
            m.Row3.X,m.Row3.Y,m.Row3.Z,m.Row3.W
        };

        public static void Enable(EnableCap cap)
        {
            switch (cap)
            {
                case EnableCap.DepthTest: _depthTest = true; break;
                case EnableCap.Blend: _blend = true; break;
                case EnableCap.CullFace: _cull = true; break;
                case EnableCap.ScissorTest: _scissor = true; break;
                case EnableCap.AlphaTest: _alphaTest = true; break;
                case EnableCap.StencilTest: _stencilTest = true; break;
                case EnableCap.PolygonOffsetFill: _polygonOffsetFill = true; break;
                case EnableCap.Texture2D: _texture2DEnabled[_activeTextureUnit] = true; break;
            }
        }

        public static void Disable(EnableCap cap)
        {
            switch (cap)
            {
                case EnableCap.DepthTest: _depthTest = false; break;
                case EnableCap.Blend: _blend = false; break;
                case EnableCap.CullFace: _cull = false; break;
                case EnableCap.ScissorTest: _scissor = false; break;
                case EnableCap.AlphaTest: _alphaTest = false; break;
                case EnableCap.StencilTest: _stencilTest = false; break;
                case EnableCap.PolygonOffsetFill: _polygonOffsetFill = false; break;
                case EnableCap.Texture2D: _texture2DEnabled[_activeTextureUnit] = false; break;
            }
        }

        public static void AlphaFunc(AlphaFunction func, float reference)
        {
            _alphaFunction = func;
            _alphaReference = Math.Clamp(reference, 0f, 1f);
        }
        public static void PolygonMode(TriangleFace face, OpenTK.Graphics.OpenGL.PolygonMode mode)
        {
            _polygonMode = mode;
        }
        public static void LineWidth(float width) { }
        public static void DebugMessageCallback(DebugProc callback, IntPtr userParam) { }

        public static void Clear(ClearBufferMask mask)
        {
            if (_gd == null) return;
            BindCurrentFramebuffer();
            Veldrid.Framebuffer fb = CurrentFramebuffer(_drawFramebuffer);

            bool clearColor = (mask & ClearBufferMask.ColorBufferBit) != 0
                && (_maskR || _maskG || _maskB || _maskA);
            bool clearDepth = (mask & ClearBufferMask.DepthBufferBit) != 0
                && _depthWrite && fb.DepthTarget != null;
            bool clearStencil = (mask & ClearBufferMask.StencilBufferBit) != 0
                && (_stencilWriteMask & 0xFF) != 0 && fb.DepthTarget != null;

            bool fullColorMask = _maskR && _maskG && _maskB && _maskA;
            bool fullStencilMask = (_stencilWriteMask & 0xFF) == 0xFF;

            // Keep the native clear path for the common full-frame cases.
            // glClear obeys color/depth/stencil write masks and the scissor
            // test, so anything more selective is drawn below instead.
            if (!_scissor && clearColor && fullColorMask)
            {
                _commands!.ClearColorTarget(0,
                    new RgbaFloat(_clearColor.R, _clearColor.G, _clearColor.B, _clearColor.A));
                clearColor = false;
            }
            if (!_scissor && clearDepth && clearStencil && fullStencilMask)
            {
                _commands!.ClearDepthStencil(1f, (byte)_clearStencil);
                clearDepth = false;
                clearStencil = false;
            }
            if (!clearColor && !clearDepth && !clearStencil)
            {
                return;
            }

            // Veldrid 4.9 only exposes whole-attachment clears. OpenGL allows
            // aspect-only and scissored clears; the transparent-face pass needs
            // depth-only, and the launcher/results hunter preview needs a
            // scissored color+depth clear. A tiny fullscreen triangle preserves
            // those semantics without reaching into Veldrid's Vulkan internals.
            _commands!.SetViewport(0, new Veldrid.Viewport(0, 0, fb.Width, fb.Height, 0, 1));
            if (_scissor)
            {
                int width = Math.Max(_scissorW, 1);
                int height = Math.Max(_scissorH, 1);
                int y = (int)fb.Height - _scissorY - height;
                _commands.SetScissorRect(0, (uint)Math.Max(_scissorX, 0), (uint)Math.Max(y, 0),
                    (uint)width, (uint)height);
            }
            else
            {
                _commands.SetScissorRect(0, 0, 0, fb.Width, fb.Height);
            }
            float[] color = { _clearColor.R, _clearColor.G, _clearColor.B, _clearColor.A };
            ClearUniformSlot clearSlot = AcquireClearUniformSlot();
            _gd.UpdateBuffer(clearSlot.Buffer, 0, color);
            _commands.SetPipeline(GetClearPipeline(clearColor, clearDepth, clearStencil, fb));
            _commands.SetGraphicsResourceSet(0, clearSlot.Set);
            _commands.Draw(3);
        }

        public static void ClearColor(Color4 color) => _clearColor = color;
        public static void ClearColor(float r, float g, float b, float a) => _clearColor = new Color4(r,g,b,a);
        public static void ClearStencil(int value) => _clearStencil = value;
        public static void ColorMask(bool r, bool g, bool b, bool a)
        {
            _maskR=r; _maskG=g; _maskB=b; _maskA=a;
        }
        public static void DepthMask(bool value) { _depthWrite = value; }
        public static void DepthFunc(DepthFunction func) { _depthFunction = func; }
        public static void CullFace(TriangleFace face) { _cullFace = face; }
        public static void BlendFunc(BlendingFactor src, BlendingFactor dst)
        {
            _blendSrc=src; _blendDst=dst;
        }
        public static void StencilFunc(StencilFunction func, int reference, int mask)
        {
            _stencilFunction = func;
            _stencilReference = reference;
            _stencilReadMask = mask;
        }
        public static void StencilOp(GLStencilOp fail, GLStencilOp zfail, GLStencilOp zpass)
        {
            _stencilFail = fail;
            _stencilDepthFail = zfail;
            _stencilPass = zpass;
        }
        public static void StencilMask(int mask) { _stencilWriteMask = mask; }
        public static void PolygonOffset(float factor, float units)
        {
            _polygonOffsetFactor = factor;
            _polygonOffsetUnits = units;
        }

        public static void Viewport(int x, int y, int width, int height)
        {
            _viewX=x; _viewY=y; _viewW=Math.Max(width,1); _viewH=Math.Max(height,1);
        }

        public static void Scissor(int x, int y, int width, int height)
        {
            _scissorX=x; _scissorY=y; _scissorW=Math.Max(width,1); _scissorH=Math.Max(height,1);
        }

        public static void PixelStore(PixelStoreParameter pname, int param) { }
        public static void DrawBuffer(DrawBufferMode mode) { }
        public static void ReadBuffer(ReadBufferMode mode) { }
        public static GLErrorCode GetError() => GLErrorCode.NoError;
        public static string GetString(StringName name)
        {
            if (_gd == null) return "Vulkan";
            return name switch
            {
                StringName.Vendor => _gd.VendorName,
                StringName.Renderer => _gd.DeviceName,
                StringName.Version => $"Vulkan {_gd.ApiVersion}",
                StringName.ShadingLanguageVersion => "SPIR-V / GLSL 450",
                _ => "Vulkan"
            };
        }
        public static int GetInteger(GetPName pname) => 0;

        public static void MatrixMode(MatrixMode mode) { }
        public static void PushMatrix() { }
        public static void PopMatrix() { }
        public static void LoadIdentity() { }
        public static void TexEnv(TextureEnvTarget target, TextureEnvParameter pname, int param)
        {
            if (target == TextureEnvTarget.TextureEnv
                && pname == TextureEnvParameter.TextureEnvMode)
            {
                _textureEnvModes[_activeTextureUnit] = (TextureEnvMode)param;
            }
        }
    }
}
#endif

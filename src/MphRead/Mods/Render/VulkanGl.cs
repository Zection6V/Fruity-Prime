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

namespace MphRead.Mods.Render
{
    internal static class VulkanGl
    {
        private const int FloatsPerVertex = 16;
        private const uint VertexStride = FloatsPerVertex * sizeof(float);
        private const uint UboSize = 2384;
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

        private enum ProgramKind { Screen, Scene }

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
        }

        private sealed class ShaderInfo
        {
            public ShaderType Type;
            public string Source = "";
        }

        private sealed class ProgramInfo
        {
            public ProgramKind Kind = ProgramKind.Screen;
            public readonly List<int> Shaders = new();
            public readonly Dictionary<string, object> Values = new(StringComparer.Ordinal);
        }

        private sealed class TextureInfo : IDisposable
        {
            public Veldrid.Texture? Texture;
            public TextureView? View;
            public uint Width;
            public uint Height;
            public bool Depth;
            public bool Linear;
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

        private static GraphicsDevice? _gd;
        private static ResourceFactory? _factory;
        private static CommandList? _commands;
        private static bool _commandsOpen;
        private static ResourceLayout? _layout;
        private static DeviceBuffer? _ubo;
        private static DeviceBuffer? _vertexBuffer;
        private static DeviceBuffer? _indexBuffer;
        private static uint _vertexBufferBytes;
        private static uint _indexBufferBytes;
        private static Shader[]? _sceneShaders;
        private static Shader[]? _screenShaders;
        private static readonly Dictionary<string, Pipeline> _pipelines = new();
        private static readonly Dictionary<string, ResourceSet> _sets = new();

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

        private static Vector4 _currentColor = Vector4.One;
        private static Vector3 _currentNormal = Vector3.UnitZ;
        private static Vector3 _currentTex0;
        private static Vector2 _currentTex1;
        private static bool _vertexColorSet;
        private static int _activeTextureUnit;
        private static readonly int[] _boundTextures = new int[2];

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
            _ubo = _factory.CreateBuffer(new BufferDescription(UboSize,
                BufferUsage.UniformBuffer | BufferUsage.Dynamic));

            _layout = _factory.CreateResourceLayout(new ResourceLayoutDescription(
                new ResourceLayoutElementDescription("CompatUniforms", ResourceKind.UniformBuffer,
                    ShaderStages.Vertex | ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Tex0", ResourceKind.TextureReadOnly, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Samp0", ResourceKind.Sampler, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Tex1", ResourceKind.TextureReadOnly, ShaderStages.Fragment),
                new ResourceLayoutElementDescription("Samp1", ResourceKind.Sampler, ShaderStages.Fragment)));

            _sceneShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.SceneVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.SceneFragment), "main"));
            _screenShaders = _factory.CreateFromSpirv(
                new ShaderDescription(ShaderStages.Vertex, Encoding.UTF8.GetBytes(VulkanShaders.ScreenVertex), "main"),
                new ShaderDescription(ShaderStages.Fragment, Encoding.UTF8.GetBytes(VulkanShaders.ScreenFragment), "main"));

            _white = new TextureInfo();
            AllocateTexture(_white, 1, 1, depth: false);
            byte[] white = { 255, 255, 255, 255 };
            _gd.UpdateTexture(_white.Texture!, white, 0, 0, 0, 1, 1, 1, 0, 0);
            DebugLog.Line("render", $"Vulkan device: {_gd.DeviceName} ({_gd.VendorName}), API {_gd.ApiVersion}");
        }

        private static bool IsWayland()
        {
            if (!OperatingSystem.IsLinux())
            {
                return false;
            }
            if (Environment.GetEnvironmentVariable("OPENTK_4_USE_WAYLAND") == "0")
            {
                return false;
            }
            return String.Equals(Environment.GetEnvironmentVariable("XDG_SESSION_TYPE"),
                "wayland", StringComparison.OrdinalIgnoreCase);
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
            foreach (ResourceSet set in _sets.Values) set.Dispose();
            foreach (TextureInfo texture in _textures.Values) texture.Dispose();
            foreach (FramebufferInfo framebuffer in _framebuffers.Values) framebuffer.Dispose();
            foreach (RenderbufferInfo renderbuffer in _renderbuffers.Values) renderbuffer.Dispose();
            _white?.Dispose();
            if (_sceneShaders != null) foreach (Shader shader in _sceneShaders) shader.Dispose();
            if (_screenShaders != null) foreach (Shader shader in _screenShaders) shader.Dispose();
            _vertexBuffer?.Dispose();
            _indexBuffer?.Dispose();
            _ubo?.Dispose();
            _layout?.Dispose();
            _commands?.Dispose();
            _gd.Dispose();

            _pipelines.Clear();
            _sets.Clear();
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
            _layout = null;
            _ubo = null;
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
                _gd.SubmitCommands(_commands);
                _commandsOpen = false;
            }
            _gd.SwapBuffers();
            _gd.WaitForIdle();
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
            if (_commandsOpen)
            {
                _commands!.End();
                _gd.SubmitCommands(_commands);
                _commandsOpen = false;
                _gd.WaitForIdle();
            }
            if (_gd.SwapchainFramebuffer.Width != (uint)width || _gd.SwapchainFramebuffer.Height != (uint)height)
            {
                _gd.ResizeMainWindow((uint)width, (uint)height);
                ClearPipelineCache();
            }
        }

        private static void EnsureFrame()
        {
            if (_commandsOpen)
            {
                return;
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

        private static void AllocateTexture(TextureInfo info, int width, int height, bool depth)
        {
            InvalidateSets();
            info.Dispose();
            info.Width = (uint)Math.Max(width, 1);
            info.Height = (uint)Math.Max(height, 1);
            info.Depth = depth;
            TextureUsage usage = depth
                ? TextureUsage.DepthStencil | TextureUsage.Sampled
                : TextureUsage.Sampled | TextureUsage.RenderTarget;
            VPixelFormat format = depth ? VPixelFormat.D24_UNorm_S8_UInt : VPixelFormat.R8_G8_B8_A8_UNorm;
            info.Texture = _factory!.CreateTexture(TextureDescription.Texture2D(
                info.Width, info.Height, 1, 1, format, usage));
            info.View = _factory.CreateTextureView(info.Texture);
            info.Version++;
            InvalidateFramebuffers();
        }

        private static void InvalidateFramebuffers()
        {
            if (_gd != null)
            {
                _gd.WaitForIdle();
            }
            foreach (FramebufferInfo fb in _framebuffers.Values)
            {
                fb.Dispose();
            }
            ClearPipelineCache();
        }

        private static void InvalidateSets()
        {
            foreach (ResourceSet set in _sets.Values) set.Dispose();
            _sets.Clear();
        }

        private static void ClearPipelineCache()
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
            v.Add(_vertexColorSet ? 1f : 0f);
            _batch.VertexCount++;
        }

        public static void Vertex3(Vector3 value) => Vertex3(value.X, value.Y, value.Z);

        public static void Color3(float r, float g, float b)
        {
            _currentColor = new Vector4(r, g, b, 1f);
            _vertexColorSet = true;
        }

        public static void Color3(Vector3 value) => Color3(value.X, value.Y, value.Z);

        public static void Color4(float r, float g, float b, float a)
        {
            _currentColor = new Vector4(r, g, b, a);
            _vertexColorSet = true;
        }

        public static void Normal3(float x, float y, float z) => _currentNormal = new Vector3(x, y, z);
        public static void TexCoord2(float s, float t) => _currentTex0 = new Vector3(s, t, 0f);
        public static void TexCoord3(float s, float t, float r) => _currentTex0 = new Vector3(s, t, r);
        public static void TexCoord3(Vector3 value) => _currentTex0 = value;

        public static void MultiTexCoord2(TextureUnit unit, float s, float t)
        {
            int index = Math.Clamp((int)unit - (int)TextureUnit.Texture0, 0, 1);
            if (index == 0) _currentTex0 = new Vector3(s, t, 0f);
            else _currentTex1 = new Vector2(s, t);
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
                Lines = _batch.Lines.ToArray()
            };
            _batch.Clear();
        }

        public static void CallList(int list)
        {
            if (_lists.TryGetValue(list, out DisplayList? data))
            {
                DrawData(data.Vertices, data.Triangles, data.Lines);
            }
        }

        public static void DeleteLists(int list, int range)
        {
            for (int i = 0; i < range; i++) _lists.Remove(list + i);
        }

        private static void EnsureBuffers(uint vertexBytes, uint indexBytes)
        {
            if (_vertexBuffer == null || vertexBytes > _vertexBufferBytes)
            {
                _gd!.WaitForIdle();
                _vertexBuffer?.Dispose();
                _vertexBufferBytes = Math.Max(vertexBytes, Math.Max(_vertexBufferBytes * 2, 65536u));
                _vertexBuffer = _factory!.CreateBuffer(new BufferDescription(_vertexBufferBytes,
                    BufferUsage.VertexBuffer | BufferUsage.Dynamic));
            }
            if (_indexBuffer == null || indexBytes > _indexBufferBytes)
            {
                _gd!.WaitForIdle();
                _indexBuffer?.Dispose();
                _indexBufferBytes = Math.Max(indexBytes, Math.Max(_indexBufferBytes * 2, 32768u));
                _indexBuffer = _factory!.CreateBuffer(new BufferDescription(_indexBufferBytes,
                    BufferUsage.IndexBuffer | BufferUsage.Dynamic));
            }
        }

        private static void DrawData(float[] vertices, uint[] triangles, uint[] lines)
        {
            if (_gd == null || vertices.Length == 0 || triangles.Length + lines.Length == 0)
            {
                return;
            }
            uint vertexBytes = (uint)(vertices.Length * sizeof(float));
            uint indexBytes = (uint)((triangles.Length + lines.Length) * sizeof(uint));
            EnsureBuffers(vertexBytes, indexBytes);
            uint[] indices = new uint[triangles.Length + lines.Length];
            Array.Copy(triangles, indices, triangles.Length);
            Array.Copy(lines, 0, indices, triangles.Length, lines.Length);

            BindCurrentFramebuffer();
            _commands!.UpdateBuffer(_vertexBuffer!, 0, vertices);
            _commands.UpdateBuffer(_indexBuffer!, 0, indices);
            Veldrid.Framebuffer fb = CurrentFramebuffer(_drawFramebuffer);
            _commands!.SetVertexBuffer(0, _vertexBuffer);
            _commands.SetIndexBuffer(_indexBuffer!, IndexFormat.UInt32);
            if (_scissor)
            {
                _commands.SetScissorRect(0, (uint)Math.Max(_scissorX, 0), (uint)Math.Max(_scissorY, 0),
                    (uint)Math.Max(_scissorW, 1), (uint)Math.Max(_scissorH, 1));
            }
            _commands.SetViewport(0, new Veldrid.Viewport(_viewX, _viewY,
                _viewW > 0 ? _viewW : fb.Width, _viewH > 0 ? _viewH : fb.Height, 0, 1));

            byte[] ubo = BuildUniforms();
            _commands.UpdateBuffer(_ubo!, 0, ubo);
            ResourceSet set = GetResourceSet();
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

        private static Pipeline GetPipeline(PrimitiveTopology topology, Veldrid.Framebuffer fb)
        {
            ProgramInfo program = CurrentProgramInfo();
            string key = $"{program.Kind}:{topology}:{_depthTest}:{_depthWrite}:{_depthFunction}:{_blend}:{_blendSrc}:{_blendDst}:{_cull}:{_cullFace}:{_polygonMode}:{_scissor}:{_maskR}{_maskG}{_maskB}{_maskA}:{fb.OutputDescription.GetHashCode()}";
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
                new VertexElementDescription("a_color_set", VertexElementSemantic.TextureCoordinate, VertexElementFormat.Float1) { Offset = 60 });
            Shader[] shaders = program.Kind == ProgramKind.Scene ? _sceneShaders! : _screenShaders!;
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

        private static ResourceSet GetResourceSet()
        {
            TextureInfo t0 = _boundTextures[0] == 0 ? _white! : GetTexture(_boundTextures[0]);
            TextureInfo t1 = _boundTextures[1] == 0 ? _white! : GetTexture(_boundTextures[1]);
            if (t0.View == null) t0 = _white!;
            if (t1.View == null) t1 = _white!;
            string key = $"{_boundTextures[0]}:{t0.Version}:{t0.Linear}|{_boundTextures[1]}:{t1.Version}:{t1.Linear}";
            if (_sets.TryGetValue(key, out ResourceSet? set))
            {
                return set;
            }
            Sampler s0 = t0.Linear ? _gd!.LinearSampler : _gd!.PointSampler;
            Sampler s1 = t1.Linear ? _gd!.LinearSampler : _gd!.PointSampler;
            set = _factory!.CreateResourceSet(new ResourceSetDescription(
                _layout!, _ubo!, t0.View!, s0, t1.View!, s1));
            _sets[key] = set;
            return set;
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
            float useTexture = p.Kind == ProgramKind.Scene
                ? GetFloat(p, "use_texture", _boundTextures[0] != 0 ? 1f : 0f)
                : (_boundTextures[0] != 0 ? 1f : 0f);
            float showColors = GetFloat(p, "show_colors", 1f);
            float useOverride = GetFloat(p, "use_override", 0f);
            WriteVector4(data, Params0Offset, new Vector4(matAlpha, useTexture, showColors, useOverride));
            int alphaMode = !_alphaTest ? 0
                : _alphaFunction == AlphaFunction.Equal ? 1
                : _alphaFunction == AlphaFunction.Less ? 2 : 0;
            WriteVector4(data, Params1Offset, new Vector4(alphaMode, 0, 0, 0));
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

        public static int GenTexture()
        {
            int name = _nextTexture++;
            _textures[name] = new TextureInfo();
            return name;
        }

        public static void DeleteTexture(int name)
        {
            if (_textures.Remove(name, out TextureInfo? info))
            {
                _gd?.WaitForIdle();
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
            if (pname == TextureParameterName.TextureMinFilter || pname == TextureParameterName.TextureMagFilter)
            {
                bool linear = param == (int)TextureMinFilter.Linear || param == (int)TextureMagFilter.Linear;
                if (linear != info.Linear)
                {
                    info.Linear = linear;
                    InvalidateSets();
                }
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
            AllocateTexture(info, width, height, depth);
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
            AllocateTexture(info, width, height, depth);
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
            EnsureFrame();
            _commands!.CopyTexture(src, (uint)x, (uint)y, 0, 0, 0,
                dst.Texture, (uint)xoffset, (uint)yoffset, 0, 0, 0,
                (uint)width, (uint)height, 1, 1);
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
            int copyX = Math.Clamp(x, 0, Math.Max((int)source.Width - width, 0));
            int copyY = Math.Clamp(y, 0, Math.Max((int)source.Height - height, 0));
            if (_gd.IsUvOriginTopLeft)
            {
                copyY = Math.Max((int)source.Height - copyY - height, 0);
            }

            if (_commandsOpen)
            {
                _commands.End();
                _gd.SubmitCommands(_commands);
                _commandsOpen = false;
            }
            _gd.WaitForIdle();

            using Veldrid.Texture staging = _factory.CreateTexture(TextureDescription.Texture2D(
                (uint)width, (uint)height, 1, 1, source.Format, TextureUsage.Staging));
            _commands.Begin();
            _commands.CopyTexture(source, (uint)copyX, (uint)copyY, 0, 0, 0,
                staging, 0, 0, 0, 0, 0, (uint)width, (uint)height, 1, 1);
            _commands.End();
            _gd.SubmitCommands(_commands);
            _gd.WaitForIdle();

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
                    IntPtr rowPtr = IntPtr.Add(mapped.Data, checked((int)(rowIndex * mapped.RowPitch)));
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
                            output[dest++] = row[src + 3];
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
            fb.Dispose();
            if (attachment == GLFramebufferAttachment.ColorAttachment0) fb.ColorTexture = texture;
            else if (attachment == GLFramebufferAttachment.DepthStencilAttachment
                || attachment == GLFramebufferAttachment.DepthAttachment) fb.DepthTexture = texture;
            ClearPipelineCache();
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
            _gd!.WaitForIdle();
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
            fb.Dispose();
            fb.DepthRenderbuffer = renderbuffer;
            fb.DepthTexture = 0;
            ClearPipelineCache();
        }

        public static FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target)
            => FramebufferErrorCode.FramebufferComplete;

        public static void DeleteFramebuffer(int framebuffer)
        {
            if (_framebuffers.Remove(framebuffer, out FramebufferInfo? fb))
            {
                _gd?.WaitForIdle();
                fb.Dispose();
                ClearPipelineCache();
            }
        }

        public static void DeleteRenderbuffer(int renderbuffer)
        {
            if (_renderbuffers.Remove(renderbuffer, out RenderbufferInfo? rb))
            {
                _gd?.WaitForIdle();
                rb.Dispose();
                InvalidateFramebuffers();
            }
        }

        public static void GetFramebufferAttachmentParameter(FramebufferTarget target,
            GLFramebufferAttachment attachment, FramebufferParameterName pname, out int result)
        {
            result = 24;
        }

        public static int CreateShader(ShaderType type)
        {
            int id = _nextShader++;
            _shaders[id] = new ShaderInfo { Type = type };
            return id;
        }

        public static void ShaderSource(int shader, string source)
        {
            if (_shaders.TryGetValue(shader, out ShaderInfo? info)) info.Source = source;
        }
        public static void CompileShader(int shader) { }
        public static void GetShader(int shader, ShaderParameter pname, out int value) => value = 1;
        public static string GetShaderInfoLog(int shader) => "";
        public static void DeleteShader(int shader) => _shaders.Remove(shader);

        public static int CreateProgram()
        {
            int id = _nextProgram++;
            _programs[id] = new ProgramInfo();
            return id;
        }

        public static void AttachShader(int program, int shader)
        {
            if (_programs.TryGetValue(program, out ProgramInfo? p)) p.Shaders.Add(shader);
        }

        public static void DetachShader(int program, int shader)
        {
            if (_programs.TryGetValue(program, out ProgramInfo? p)) p.Shaders.Remove(shader);
        }

        public static void LinkProgram(int program)
        {
            if (!_programs.TryGetValue(program, out ProgramInfo? p)) return;
            foreach (int shader in p.Shaders)
            {
                if (_shaders.TryGetValue(shader, out ShaderInfo? s) && s.Type == ShaderType.VertexShader
                    && String.Equals(s.Source, Shaders.VertexShader, StringComparison.Ordinal))
                {
                    p.Kind = ProgramKind.Scene;
                    return;
                }
            }
            p.Kind = ProgramKind.Screen;
        }

        public static void GetProgram(int program, GetProgramParameterName pname, out int value) => value = 1;
        public static string GetProgramInfoLog(int program) => "";
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
                case EnableCap.Blend: _blend = true; ClearPipelineCache(); break;
                case EnableCap.CullFace: _cull = true; ClearPipelineCache(); break;
                case EnableCap.ScissorTest: _scissor = true; ClearPipelineCache(); break;
                case EnableCap.AlphaTest: _alphaTest = true; break;
            }
        }

        public static void Disable(EnableCap cap)
        {
            switch (cap)
            {
                case EnableCap.DepthTest: _depthTest = false; break;
                case EnableCap.Blend: _blend = false; ClearPipelineCache(); break;
                case EnableCap.CullFace: _cull = false; ClearPipelineCache(); break;
                case EnableCap.ScissorTest: _scissor = false; ClearPipelineCache(); break;
                case EnableCap.AlphaTest: _alphaTest = false; break;
            }
        }

        public static void AlphaFunc(AlphaFunction func, float reference) => _alphaFunction = func;
        public static void PolygonMode(TriangleFace face, OpenTK.Graphics.OpenGL.PolygonMode mode)
        {
            if (_polygonMode != mode) { _polygonMode = mode; ClearPipelineCache(); }
        }
        public static void LineWidth(float width) { }
        public static void DebugMessageCallback(DebugProc callback, IntPtr userParam) { }

        public static void Clear(ClearBufferMask mask)
        {
            if (_gd == null) return;
            BindCurrentFramebuffer();
            if ((mask & ClearBufferMask.ColorBufferBit) != 0)
            {
                _commands!.ClearColorTarget(0, new RgbaFloat(_clearColor.R, _clearColor.G, _clearColor.B, _clearColor.A));
            }
            if ((mask & (ClearBufferMask.DepthBufferBit | ClearBufferMask.StencilBufferBit)) != 0
                && CurrentFramebuffer(_drawFramebuffer).DepthTarget != null)
            {
                _commands!.ClearDepthStencil(1f, (byte)_clearStencil);
            }
        }

        public static void ClearColor(Color4 color) => _clearColor = color;
        public static void ClearColor(float r, float g, float b, float a) => _clearColor = new Color4(r,g,b,a);
        public static void ClearStencil(int value) => _clearStencil = value;
        public static void ColorMask(bool r, bool g, bool b, bool a)
        {
            _maskR=r; _maskG=g; _maskB=b; _maskA=a; ClearPipelineCache();
        }
        public static void DepthMask(bool value) { _depthWrite = value; ClearPipelineCache(); }
        public static void DepthFunc(DepthFunction func) { _depthFunction = func; ClearPipelineCache(); }
        public static void CullFace(TriangleFace face) { _cullFace = face; ClearPipelineCache(); }
        public static void BlendFunc(BlendingFactor src, BlendingFactor dst)
        {
            _blendSrc=src; _blendDst=dst; ClearPipelineCache();
        }
        public static void StencilFunc(StencilFunction func, int reference, int mask) { }
        public static void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,
            OpenTK.Graphics.OpenGL.StencilOp zfail, OpenTK.Graphics.OpenGL.StencilOp zpass) { }
        public static void StencilMask(int mask) { }
        public static void PolygonOffset(float factor, float units) { }

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
        public static void TexEnv(TextureEnvTarget target, TextureEnvParameter pname, int param) { }
    }
}
#endif

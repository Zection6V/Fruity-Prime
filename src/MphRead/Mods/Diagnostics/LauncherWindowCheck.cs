#if MPHREAD_SHELL
using System;
using MphRead.Mods.Launcher.Gui;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace MphRead.Mods.Diagnostics
{
    /// <summary>Exercise the real desktop launcher without cartridge data.</summary>
    internal static class LauncherWindowCheck
    {
        private static bool _active;
        private static bool _passed;
        private static bool _requireVulkan;
        private static int _frames;

        public static int Run(bool requireVulkan = false)
        {
            _active = true;
            _passed = false;
            _requireVulkan = requireVulkan;
            _frames = 0;
            bool geometry = WindowGeometry.Enabled;
            WindowGeometry.Enabled = false;
            try
            {
                bool ran = GuiLauncher.TryRun();
                Console.WriteLine(ran && _passed ? "Launcher window check passed." : "Launcher window check failed.");
                return ran && _passed ? 0 : 1;
            }
            finally
            {
                _active = false;
                _requireVulkan = false;
                WindowGeometry.Enabled = geometry;
            }
        }

        // Called before buffer swap, after the same UI path used on first launch.
        internal static void AfterDraw(RenderWindow window)
        {
            if (!_active || (++_frames != 20 && _frames != 40)) return;
            try
            {
                if (_frames == 20)
                {
                    if (_requireVulkan && !Render.RendererBackend.UseVulkan)
                        throw new InvalidOperationException("Vulkan was requested but the window did not use Vulkan.");
                    string api = Render.RendererBackend.UseVulkan ? "Vulkan" : "GL";
                    Console.WriteLine($"[windowcheck] {GL.GetString(StringName.Renderer)}; {api} {GL.GetString(StringName.Version)}");
                    Link(Shaders.VertexShader, Shaders.FragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.RttFragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.CelFragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.ShiftFragmentShader);
                }
                int width = window.FramebufferSize.X, height = window.FramebufferSize.Y;
                if (!Render.UiOverlay.HasFrame || width <= 0 || height <= 0)
                    throw new InvalidOperationException("Launcher did not upload a frame.");
                byte[] pixels = new byte[checked(width * height * 4)];
                GL.ReadBuffer(ReadBufferMode.Back);
                GL.ReadPixels(0, 0, width, height, PixelFormat.Rgba, PixelType.UnsignedByte, pixels);
                int lit = 0;
                for (int i = 0; i < pixels.Length; i += 4)
                    if (pixels[i] > 16 || pixels[i + 1] > 16 || pixels[i + 2] > 16) lit++;
                if (lit < width * height / 100)
                    throw new InvalidOperationException("Launcher frame is black.");
                var error = GL.GetError();
                if (error != ErrorCode.NoError) throw new InvalidOperationException($"OpenGL error: {error}");
                Console.WriteLine($"[windowcheck] rendered {width}x{height}, {lit} lit pixels");
                if (_frames == 20 && _requireVulkan)
                {
                    CheckVulkanClearSemantics();
                }
                if (_frames == 20) window.ClientSize = new Vector2i(1100, 740);
                else { _passed = true; window.Close(); }
            }
            catch (Exception ex)
            {
                // Never throw through GLFW's native callbacks.
                Console.Error.WriteLine($"[windowcheck] {ex}");
                _active = false;
                window.Close();
            }
        }

        private static void CheckVulkanClearSemantics()
        {
            const int size = 16;
            int texture = GL.GenTexture();
            int framebuffer = GL.GenFramebuffer();
            int depth = GL.GenRenderbuffer();
            try
            {
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, texture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    size, size, 0, PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter,
                    (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter,
                    (int)TextureMagFilter.Nearest);
                GL.BindTexture(TextureTarget.Texture2D, 0);

                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,
                    FramebufferAttachment.ColorAttachment0, TextureTarget.Texture2D, texture, 0);
                GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer, depth);
                GL.RenderbufferStorage(RenderbufferTarget.Renderbuffer,
                    RenderbufferStorage.Depth24Stencil8, size, size);
                GL.FramebufferRenderbuffer(FramebufferTarget.Framebuffer,
                    FramebufferAttachment.DepthStencilAttachment,
                    RenderbufferTarget.Renderbuffer, depth);
                GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer, 0);
                if (GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer)
                    != FramebufferErrorCode.FramebufferComplete)
                {
                    throw new InvalidOperationException("Vulkan clear regression framebuffer is incomplete.");
                }

                GL.Viewport(0, 0, size, size);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.AlphaTest);
                GL.Disable(EnableCap.CullFace);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.ScissorTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(true);
                GL.StencilMask(0xFF);
                GL.ClearStencil(0);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit
                    | ClearBufferMask.StencilBufferBit);

                // OpenGL coordinates are bottom-left based. An asymmetric box
                // catches both a lost scissor and a missing Y conversion.
                GL.Enable(EnableCap.ScissorTest);
                GL.Scissor(2, 3, 5, 4);
                GL.ClearColor(1f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Disable(EnableCap.ScissorTest);

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, 3, 4, 255, 0, 0)
                    || !PixelIs(pixels, size, 3, 10, 0, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan scissored clear did not match OpenGL bottom-left coordinates.");
                }

                // Write stencil=7 everywhere, clear depth only, then prove the
                // stencil survived by drawing green through an Equal test.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(true);
                GL.StencilMask(0xFF);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.ClearStencil(0);
                GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit
                    | ClearBufferMask.StencilBufferBit);

                GL.UseProgram(0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Enable(EnableCap.StencilTest);
                GL.StencilFunc(StencilFunction.Always, 7, 0xFF);
                GL.StencilOp(StencilOp.Keep, StencilOp.Keep, StencilOp.Replace);
                GL.ColorMask(false, false, false, false);
                GL.DepthMask(false);
                GL.Color4(1f, 1f, 1f, 1f);
                DrawTestQuad();

                GL.DepthMask(true);
                GL.Clear(ClearBufferMask.DepthBufferBit);

                GL.ColorMask(true, true, true, true);
                GL.DepthMask(false);
                GL.StencilMask(0);
                GL.StencilFunc(StencilFunction.Equal, 7, 0xFF);
                GL.StencilOp(StencilOp.Keep, StencilOp.Keep, StencilOp.Keep);
                GL.Color4(0f, 1f, 0f, 1f);
                DrawTestQuad();

                Array.Clear(pixels, 0, pixels.Length);
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, 8, 8, 0, 255, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan depth-only clear destroyed the stencil attachment.");
                }
                Console.WriteLine("[windowcheck] Vulkan scissor/depth-stencil clear semantics passed.");
            }
            finally
            {
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, 0);
                GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer, 0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.ScissorTest);
                GL.Disable(EnableCap.StencilTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(true);
                GL.StencilMask(0xFF);
                GL.DeleteRenderbuffer(depth);
                GL.DeleteFramebuffer(framebuffer);
                GL.DeleteTexture(texture);
            }
        }

        private static bool PixelIs(byte[] pixels, int width, int x, int y,
            byte red, byte green, byte blue)
        {
            int offset = (y * width + x) * 4;
            return Math.Abs(pixels[offset] - red) <= 2
                && Math.Abs(pixels[offset + 1] - green) <= 2
                && Math.Abs(pixels[offset + 2] - blue) <= 2;
        }

        private static void DrawTestQuad()
        {
            GL.Begin(PrimitiveType.Quads);
            GL.Vertex3(-1f, -1f, 0f);
            GL.Vertex3(1f, -1f, 0f);
            GL.Vertex3(1f, 1f, 0f);
            GL.Vertex3(-1f, 1f, 0f);
            GL.End();
        }

        private static void Link(string vertex, string fragment)
        {
            int program = GL.CreateProgram();
            try
            {
                foreach (var (type, source) in new[] {
                    (ShaderType.VertexShader, vertex), (ShaderType.FragmentShader, fragment) })
                {
                    int shader = GL.CreateShader(type);
                    try
                    {
                        GL.ShaderSource(shader, source);
                        GL.CompileShader(shader);
                        GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
                        if (compiled == 0) throw new InvalidOperationException(GL.GetShaderInfoLog(shader));
                        GL.AttachShader(program, shader);
                    }
                    finally { GL.DeleteShader(shader); }
                }
                GL.LinkProgram(program);
                GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
                if (linked == 0) throw new InvalidOperationException(GL.GetProgramInfoLog(program));
            }
            finally { GL.DeleteProgram(program); }
        }
    }
}
#endif

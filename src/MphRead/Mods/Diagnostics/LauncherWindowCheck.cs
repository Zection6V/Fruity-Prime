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
            _frames++;
            bool checkpoint = _frames == 20 || _frames == 40
                || (_requireVulkan && (_frames == 30 || _frames == 50 || _frames == 60));
            if (!_active || !checkpoint) return;
            try
            {
                if (_frames == 20)
                {
                    if (_requireVulkan && !Render.RendererBackend.IsActive(Render.RendererBackendKind.Vulkan))
                        throw new InvalidOperationException("Vulkan was requested but the window did not use Vulkan.");
                    string api = Render.RendererBackend.IsActive(Render.RendererBackendKind.Vulkan) ? "Vulkan" : "GL";
                    Console.WriteLine($"[windowcheck] {GL.GetString(StringName.Renderer)}; {api} {GL.GetString(StringName.Version)}");
                    Link(Shaders.VertexShader, Shaders.FragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.RttFragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.CelFragmentShader);
                    Link(Shaders.RttVertexShader, Shaders.ShiftFragmentShader);
                    Link(Shaders.BackdropVertexShader, Shaders.BackdropFragmentShader);
                    if (_requireVulkan)
                    {
                        CheckVulkanRejectsUnknownShader();
                    }
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

                // Vulkan's cached descriptors, image views, framebuffers and
                // pipelines are rebuilt around a resize. Cycle several real
                // window sizes so CI catches resource lifetime regressions
                // instead of proving only the first swapchain recreation.
                if (_requireVulkan)
                {
                    Vector2i? next = _frames switch
                    {
                        20 => new Vector2i(1100, 740),
                        30 => new Vector2i(1240, 780),
                        40 => new Vector2i(1060, 720),
                        50 => new Vector2i(1280, 768),
                        _ => null
                    };
                    if (next.HasValue)
                    {
                        Console.WriteLine($"[windowcheck] Vulkan resize stress -> {next.Value.X}x{next.Value.Y}");
                        window.ClientSize = next.Value;
                    }
                    else
                    {
                        _passed = true;
                        window.Close();
                    }
                }
                else if (_frames == 20)
                {
                    window.ClientSize = new Vector2i(1100, 740);
                }
                else
                {
                    _passed = true;
                    window.Close();
                }
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

                // OpenGL has a single binding per framebuffer attachment point.
                // Replacing a depth-stencil renderbuffer with a texture, then
                // detaching that texture, must leave the attachment empty; the
                // old renderbuffer must not silently come back.
                int replacementDepth = GL.GenTexture();
                try
                {
                    GL.ActiveTexture(TextureUnit.Texture0);
                    GL.BindTexture(TextureTarget.Texture2D, replacementDepth);
                    GL.TexImage2D(TextureTarget.Texture2D, 0,
                        PixelInternalFormat.Depth24Stencil8, size, size, 0,
                        PixelFormat.DepthStencil, PixelType.UnsignedInt248, IntPtr.Zero);
                    GL.BindTexture(TextureTarget.Texture2D, 0);

                    GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                    GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,
                        FramebufferAttachment.DepthStencilAttachment,
                        TextureTarget.Texture2D, replacementDepth, 0);
                    GL.GetFramebufferAttachmentParameter(FramebufferTarget.Framebuffer,
                        FramebufferAttachment.DepthStencilAttachment,
                        FramebufferParameterName.FramebufferAttachmentObjectName,
                        out int attachedDepth);
                    if (attachedDepth != replacementDepth)
                    {
                        throw new InvalidOperationException(
                            "Vulkan depth texture did not replace the renderbuffer attachment.");
                    }

                    GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,
                        FramebufferAttachment.DepthStencilAttachment,
                        TextureTarget.Texture2D, 0, 0);
                    GL.GetFramebufferAttachmentParameter(FramebufferTarget.Framebuffer,
                        FramebufferAttachment.DepthStencilAttachment,
                        FramebufferParameterName.FramebufferAttachmentObjectName,
                        out attachedDepth);
                    if (attachedDepth != 0)
                    {
                        throw new InvalidOperationException(
                            "Vulkan resurrected a stale depth renderbuffer after texture detach.");
                    }
                    Console.WriteLine(
                        "[windowcheck] Vulkan OpenGL-compatible depth attachment replacement passed.");
                }
                finally
                {
                    GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                    GL.FramebufferRenderbuffer(FramebufferTarget.Framebuffer,
                        FramebufferAttachment.DepthStencilAttachment,
                        RenderbufferTarget.Renderbuffer, depth);
                    GL.ActiveTexture(TextureUnit.Texture0);
                    GL.BindTexture(TextureTarget.Texture2D, 0);
                    GL.DeleteTexture(replacementDepth);
                }

                // The renderer inherits OpenGL's default CCW front-face rule.
                // Veldrid maps the enum directly to Vulkan, while Vulkan's
                // viewport Y handling can change winding if the backend is not
                // configured consistently. Prove the actual pipeline rather
                // than relying on that interaction being obvious.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.Disable(EnableCap.ScissorTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.AlphaTest);
                GL.Enable(EnableCap.CullFace);
                GL.CullFace(TriangleFace.Back);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(true);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);

                GL.Color4(1f, 0f, 0f, 1f);
                GL.Begin(PrimitiveType.Triangles);
                GL.Vertex3(-0.75f, -0.75f, 0f);
                GL.Vertex3(0.75f, -0.75f, 0f);
                GL.Vertex3(0f, 0.75f, 0f);
                GL.End();

                Array.Clear(pixels, 0, pixels.Length);
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, 8, 8, 255, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan culled the OpenGL CCW front face.");
                }

                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(0f, 1f, 0f, 1f);
                GL.Begin(PrimitiveType.Triangles);
                GL.Vertex3(-0.75f, -0.75f, 0f);
                GL.Vertex3(0f, 0.75f, 0f);
                GL.Vertex3(0.75f, -0.75f, 0f);
                GL.End();

                Array.Clear(pixels, 0, pixels.Length);
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, 8, 8, 0, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan failed to cull the OpenGL CW back face.");
                }
                GL.Disable(EnableCap.CullFace);
                Console.WriteLine("[windowcheck] Vulkan OpenGL-compatible face winding passed.");

                // Scene matrices in Fruity are OpenGL matrices. A vertex at
                // clip z=-0.5 is valid there, but raw Vulkan would reject it
                // because Vulkan's clip range begins at zero. The Vulkan scene
                // vertex shader must remap [-w,+w] to [0,+w].
                int sceneProgram = CreateSceneProgram();
                try
                {
                    GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                    GL.ClearColor(0f, 0f, 0f, 1f);
                    GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
                    GL.UseProgram(sceneProgram);
                    GL.Disable(EnableCap.DepthTest);
                    GL.Disable(EnableCap.CullFace);
                    GL.Disable(EnableCap.StencilTest);
                    GL.Disable(EnableCap.Blend);
                    GL.Color4(0f, 0f, 1f, 1f);
                    GL.TexCoord3(0f, 0f, 0f);
                    GL.Begin(PrimitiveType.Triangles);
                    GL.Vertex3(-0.75f, -0.75f, -0.5f);
                    GL.Vertex3(0.75f, -0.75f, -0.5f);
                    GL.Vertex3(0f, 0.75f, -0.5f);
                    GL.End();

                    Array.Clear(pixels, 0, pixels.Length);
                    GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                    GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                        PixelType.UnsignedByte, pixels);
                    if (!PixelIs(pixels, size, 8, 8, 0, 0, 255))
                    {
                        throw new InvalidOperationException(
                            "Vulkan did not preserve the OpenGL clip-space depth range.");
                    }
                    Console.WriteLine("[windowcheck] Vulkan OpenGL clip-depth mapping passed.");
                }
                finally
                {
                    GL.UseProgram(0);
                    GL.DeleteProgram(sceneProgram);
                }

                CheckVulkanUniformIsolation(framebuffer, size);
                CheckVulkanSceneTextureUniformIsolation(framebuffer, size);
                CheckVulkanFixedFunctionTextureState(framebuffer, size);
                CheckVulkanCopyTexSubImageOrientation(framebuffer, size);
                CheckVulkanFramebufferSamplingOrientation(texture, framebuffer, size);
                CheckVulkanRttMaskOrientation(texture, framebuffer, size);
            }
            finally
            {
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, 0);
                GL.BindRenderbuffer(RenderbufferTarget.Renderbuffer, 0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.ScissorTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(true);
                GL.StencilMask(0xFF);
                GL.DeleteRenderbuffer(depth);
                GL.DeleteFramebuffer(framebuffer);
                GL.DeleteTexture(texture);
            }
        }

        private static void CheckVulkanUniformIsolation(int framebuffer, int size)
        {
            int program = CreateSceneProgram();
            try
            {
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.Viewport(0, 0, size, size);
                GL.UseProgram(program);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.Texture2D);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.Disable(EnableCap.AlphaTest);
                GL.Disable(EnableCap.ScissorTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(false);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);

                int useOverride = GL.GetUniformLocation(program, "use_override");
                int overrideColor = GL.GetUniformLocation(program, "override_color");
                int useTexture = GL.GetUniformLocation(program, "use_texture");
                int useLight = GL.GetUniformLocation(program, "use_light");
                int showColors = GL.GetUniformLocation(program, "show_colors");
                GL.Uniform1(useOverride, 1);
                GL.Uniform1(useTexture, 0);
                GL.Uniform1(useLight, 0);
                GL.Uniform1(showColors, 0);

                // These two draws are deliberately recorded before one
                // readback/submit. A shared mutable Vulkan UBO would let the
                // second color leak into the first draw.
                GL.Uniform4(overrideColor, 1f, 0f, 0f, 1f);
                DrawTestQuad(-1f, -0.05f);
                GL.Uniform4(overrideColor, 0f, 1f, 0f, 1f);
                DrawTestQuad(0.05f, 1f);

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 4, size / 2, 255, 0, 0)
                    || !PixelIs(pixels, size, size * 3 / 4, size / 2, 0, 255, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan draw uniforms were overwritten by a later draw in the same frame.");
                }
                Console.WriteLine("[windowcheck] Vulkan per-draw uniform isolation passed.");
            }
            finally
            {
                GL.UseProgram(0);
                GL.Enable(EnableCap.Texture2D);
                GL.DepthMask(true);
                GL.DeleteProgram(program);
            }
        }

        private static void CheckVulkanSceneTextureUniformIsolation(
            int framebuffer, int size)
        {
            int texture = GL.GenTexture();
            int program = CreateSceneProgram();
            try
            {
                byte[] blue = { 0, 0, 255, 255 };
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, texture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    1, 1, 0, PixelFormat.Rgba, PixelType.UnsignedByte, blue);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureWrapS, (int)TextureWrapMode.ClampToEdge);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureWrapT, (int)TextureWrapMode.ClampToEdge);

                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.Viewport(0, 0, size, size);
                GL.UseProgram(program);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.Disable(EnableCap.AlphaTest);
                GL.Disable(EnableCap.ScissorTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(false);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);

                GL.Uniform1(GL.GetUniformLocation(program, "use_texture"), 1);
                GL.Uniform1(GL.GetUniformLocation(program, "use_light"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "show_colors"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "use_override"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "use_pal_override"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "mat_mode"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "mat_alpha"), 1f);
                GL.Uniform1(GL.GetUniformLocation(program, "cel_bands"), 0);
                GL.Uniform1(GL.GetUniformLocation(program, "texgen_mode"), 0);
                Matrix4 identity = Matrix4.Identity;
                GL.UniformMatrix4(GL.GetUniformLocation(program, "tex_mtx"),
                    false, ref identity);

                int useFlat = GL.GetUniformLocation(program, "use_flat");
                int flatColor = GL.GetUniformLocation(program, "flat_color");

                GL.Uniform1(useFlat, 0);
                DrawTexturedTestQuad(-1f, -0.05f);

                GL.Uniform1(useFlat, 1);
                GL.Uniform3(flatColor, new Vector3(1f, 1f, 0f));
                DrawTexturedTestQuad(0.05f, 1f);

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 4, size / 2, 0, 0, 255)
                    || !PixelIs(pixels, size, size * 3 / 4, size / 2, 255, 255, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan Scene texture/flat-color state leaked between draws.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan Scene texture/flat-color uniform isolation passed.");
            }
            finally
            {
                GL.UseProgram(0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Enable(EnableCap.Texture2D);
                GL.DepthMask(true);
                GL.DeleteProgram(program);
                GL.DeleteTexture(texture);
            }
        }

        private static void CheckVulkanFixedFunctionTextureState(
            int framebuffer, int size)
        {
            int sample = GL.GenTexture();
            try
            {
                byte[] red = { 255, 0, 0, 255 };
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, sample);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    1, 1, 0, PixelFormat.Rgba, PixelType.UnsignedByte, red);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);

                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.Viewport(0, 0, size, size);
                GL.UseProgram(0);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.Disable(EnableCap.AlphaTest);
                GL.Disable(EnableCap.ScissorTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(false);

                GL.Enable(EnableCap.Texture2D);
                GL.TexEnv(TextureEnvTarget.TextureEnv, TextureEnvParameter.TextureEnvMode,
                    (int)TextureEnvMode.Replace);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(0f, 1f, 0f, 1f);
                DrawTestQuad();

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 2, size / 2, 255, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan fixed-function TextureEnv Replace did not replace vertex color.");
                }

                // Keep the texture bound. In compatibility OpenGL, disabling
                // GL_TEXTURE_2D is independent from the texture binding.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
                GL.Disable(EnableCap.Texture2D);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(0f, 1f, 0f, 1f);
                DrawTestQuad();

                Array.Clear(pixels, 0, pixels.Length);
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, framebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 2, size / 2, 0, 255, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan ignored GL_TEXTURE_2D disable with a texture still bound.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan fixed-function texture enable/TexEnv semantics passed.");
            }
            finally
            {
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.TexEnv(TextureEnvTarget.TextureEnv, TextureEnvParameter.TextureEnvMode,
                    (int)TextureEnvMode.Modulate);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Enable(EnableCap.Texture2D);
                GL.DepthMask(true);
                GL.DeleteTexture(sample);
            }
        }

        private static void CheckVulkanCopyTexSubImageOrientation(
            int sourceFramebuffer, int size)
        {
            int copiedTexture = GL.GenTexture();
            int program = 0;
            try
            {
                // OpenGL framebuffer coordinates are bottom-left. Make the
                // lower half red and upper half green so y=0 has an
                // unambiguous expected result after a framebuffer-to-texture
                // copy on Vulkan's top-left-origin images.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, sourceFramebuffer);
                GL.Viewport(0, 0, size, size);
                GL.UseProgram(0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.Disable(EnableCap.AlphaTest);
                GL.ColorMask(true, true, true, true);
                GL.DepthMask(false);

                GL.Enable(EnableCap.ScissorTest);
                GL.Scissor(0, 0, size, size / 2);
                GL.ClearColor(1f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Scissor(0, size / 2, size, size / 2);
                GL.ClearColor(0f, 1f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Disable(EnableCap.ScissorTest);

                GL.BindTexture(TextureTarget.Texture2D, copiedTexture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    size, size / 2, 0, PixelFormat.Rgba,
                    PixelType.UnsignedByte, IntPtr.Zero);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);

                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, sourceFramebuffer);
                GL.CopyTexSubImage2D(TextureTarget.Texture2D, 0,
                    0, 0, 0, 0, size, size / 2);

                // Draw the copied half back over the same FBO. A correct
                // OpenGL-coordinate copy is uniformly red. The old Vulkan
                // implementation copied the top half instead and produced
                // green here.
                program = CreateProgram(Shaders.RttVertexShader, Shaders.RttFragmentShader);
                GL.UseProgram(program);
                GL.Uniform1(GL.GetUniformLocation(program, "alpha"), 1f);
                GL.Uniform1(GL.GetUniformLocation(program, "use_mask"), 0);
                GL.Uniform4(GL.GetUniformLocation(program, "fade_color"), Vector4.Zero);
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, sourceFramebuffer);
                GL.Viewport(0, 0, size, size);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(1f, 1f, 1f, 1f);
                GL.Begin(PrimitiveType.TriangleStrip);
                GL.TexCoord3(1f, 1f, 0f); GL.Vertex3(1f, 1f, 0f);
                GL.TexCoord3(0f, 1f, 0f); GL.Vertex3(-1f, 1f, 0f);
                GL.TexCoord3(1f, 0f, 0f); GL.Vertex3(1f, -1f, 0f);
                GL.TexCoord3(0f, 0f, 0f); GL.Vertex3(-1f, -1f, 0f);
                GL.End();

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, sourceFramebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 2, size / 2, 255, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan CopyTexSubImage2D did not use OpenGL bottom-left source coordinates.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan OpenGL-compatible CopyTexSubImage2D orientation passed.");
            }
            finally
            {
                GL.UseProgram(0);
                GL.Disable(EnableCap.ScissorTest);
                GL.DepthMask(true);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                if (program != 0) GL.DeleteProgram(program);
                GL.DeleteTexture(copiedTexture);
            }
        }

        private static void CheckVulkanFramebufferSamplingOrientation(
            int sourceTexture, int sourceFramebuffer, int size)
        {
            int targetTexture = GL.GenTexture();
            int targetFramebuffer = GL.GenFramebuffer();
            int program = 0;
            try
            {
                // Build an asymmetric source in OpenGL framebuffer coordinates:
                // red on top, black on bottom.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, sourceFramebuffer);
                GL.Viewport(0, 0, size, size);
                GL.UseProgram(0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.ColorMask(true, true, true, true);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Enable(EnableCap.ScissorTest);
                GL.Scissor(0, size / 2, size, size / 2);
                GL.ClearColor(1f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Disable(EnableCap.ScissorTest);

                GL.BindTexture(TextureTarget.Texture2D, targetTexture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    size, size, 0, PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter,
                    (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter,
                    (int)TextureMagFilter.Nearest);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, targetFramebuffer);
                GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,
                    FramebufferAttachment.ColorAttachment0, TextureTarget.Texture2D,
                    targetTexture, 0);
                if (GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer)
                    != FramebufferErrorCode.FramebufferComplete)
                {
                    throw new InvalidOperationException(
                        "Vulkan framebuffer-orientation regression target is incomplete.");
                }

                program = CreateProgram(Shaders.RttVertexShader, Shaders.RttFragmentShader);
                GL.UseProgram(program);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, sourceTexture);
                GL.Viewport(0, 0, size, size);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(1f, 1f, 1f, 1f);
                GL.Begin(PrimitiveType.TriangleStrip);
                GL.TexCoord3(1f, 1f, 0f); GL.Vertex3(1f, 1f, 0f);
                GL.TexCoord3(0f, 1f, 0f); GL.Vertex3(-1f, 1f, 0f);
                GL.TexCoord3(1f, 0f, 0f); GL.Vertex3(1f, -1f, 0f);
                GL.TexCoord3(0f, 0f, 0f); GL.Vertex3(-1f, -1f, 0f);
                GL.End();

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, targetFramebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 2, size * 3 / 4, 255, 0, 0)
                    || !PixelIs(pixels, size, size / 2, size / 4, 0, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan framebuffer texture sampling was vertically inverted.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan OpenGL-compatible framebuffer texture orientation passed.");
            }
            finally
            {
                GL.UseProgram(0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.Disable(EnableCap.ScissorTest);
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, 0);
                if (program != 0) GL.DeleteProgram(program);
                GL.DeleteFramebuffer(targetFramebuffer);
                GL.DeleteTexture(targetTexture);
            }
        }

        private static void CheckVulkanRttMaskOrientation(
            int sourceTexture, int sourceFramebuffer, int size)
        {
            int maskTexture = GL.GenTexture();
            int targetTexture = GL.GenTexture();
            int targetFramebuffer = GL.GenFramebuffer();
            int program = 0;
            try
            {
                // The source is solid red. The mask is a CPU-uploaded image
                // whose first half of rows has alpha and second half does not.
                // With OpenGL's bottom-left gl_FragCoord plus the RTT shader's
                // 1-y mask lookup, those first texture rows cover the TOP half
                // of the screen.
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, sourceFramebuffer);
                GL.Viewport(0, 0, size, size);
                GL.Disable(EnableCap.ScissorTest);
                GL.ColorMask(true, true, true, true);
                GL.ClearColor(1f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);

                byte[] mask = new byte[size * size * 4];
                for (int y = 0; y < size / 2; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        int at = (y * size + x) * 4;
                        mask[at] = 255;
                        mask[at + 1] = 255;
                        mask[at + 2] = 255;
                        mask[at + 3] = 255;
                    }
                }
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, maskTexture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    size, size, 0, PixelFormat.Rgba, PixelType.UnsignedByte, mask);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter,
                    (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter,
                    (int)TextureMagFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapS,
                    (int)TextureWrapMode.ClampToEdge);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapT,
                    (int)TextureWrapMode.ClampToEdge);
                GL.BindTexture(TextureTarget.Texture2D, 0);

                GL.BindTexture(TextureTarget.Texture2D, targetTexture);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba,
                    size, size, 0, PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter,
                    (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter,
                    (int)TextureMagFilter.Nearest);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, targetFramebuffer);
                GL.FramebufferTexture2D(FramebufferTarget.Framebuffer,
                    FramebufferAttachment.ColorAttachment0, TextureTarget.Texture2D,
                    targetTexture, 0);
                if (GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer)
                    != FramebufferErrorCode.FramebufferComplete)
                {
                    throw new InvalidOperationException(
                        "Vulkan RTT-mask regression target is incomplete.");
                }

                program = CreateProgram(Shaders.RttVertexShader, Shaders.RttFragmentShader);
                GL.UseProgram(program);
                GL.Uniform1(GL.GetUniformLocation(program, "alpha"), 1f);
                GL.Uniform1(GL.GetUniformLocation(program, "use_mask"), 1);
                GL.Uniform1(GL.GetUniformLocation(program, "view_width"), (float)size);
                GL.Uniform1(GL.GetUniformLocation(program, "view_height"), (float)size);
                GL.Uniform4(GL.GetUniformLocation(program, "fade_color"), Vector4.Zero);

                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, sourceTexture);
                GL.ActiveTexture(TextureUnit.Texture1);
                GL.BindTexture(TextureTarget.Texture2D, maskTexture);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.Viewport(0, 0, size, size);
                GL.Disable(EnableCap.DepthTest);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.CullFace);
                GL.Enable(EnableCap.Blend);
                GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);
                GL.ClearColor(0f, 0f, 0f, 1f);
                GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Color4(1f, 1f, 1f, 1f);
                GL.Begin(PrimitiveType.TriangleStrip);
                GL.TexCoord3(1f, 1f, 0f); GL.Vertex3(1f, 1f, 0f);
                GL.TexCoord3(0f, 1f, 0f); GL.Vertex3(-1f, 1f, 0f);
                GL.TexCoord3(1f, 0f, 0f); GL.Vertex3(1f, -1f, 0f);
                GL.TexCoord3(0f, 0f, 0f); GL.Vertex3(-1f, -1f, 0f);
                GL.End();

                byte[] pixels = new byte[size * size * 4];
                GL.BindFramebuffer(FramebufferTarget.ReadFramebuffer, targetFramebuffer);
                GL.ReadPixels(0, 0, size, size, PixelFormat.Rgba,
                    PixelType.UnsignedByte, pixels);
                if (!PixelIs(pixels, size, size / 2, size * 3 / 4, 0, 0, 0)
                    || !PixelIs(pixels, size, size / 2, size / 4, 255, 0, 0))
                {
                    throw new InvalidOperationException(
                        "Vulkan RTT mask did not preserve OpenGL gl_FragCoord/Y orientation.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan OpenGL-compatible RTT mask orientation passed.");
            }
            finally
            {
                GL.UseProgram(0);
                GL.Disable(EnableCap.Blend);
                GL.ActiveTexture(TextureUnit.Texture1);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.ActiveTexture(TextureUnit.Texture0);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, 0);
                if (program != 0) GL.DeleteProgram(program);
                GL.DeleteFramebuffer(targetFramebuffer);
                GL.DeleteTexture(targetTexture);
                GL.DeleteTexture(maskTexture);
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

        private static void DrawTestQuad(float left, float right)
        {
            GL.Begin(PrimitiveType.Quads);
            GL.Vertex3(left, -1f, 0f);
            GL.Vertex3(right, -1f, 0f);
            GL.Vertex3(right, 1f, 0f);
            GL.Vertex3(left, 1f, 0f);
            GL.End();
        }

        private static void DrawTexturedTestQuad(float left, float right)
        {
            GL.Begin(PrimitiveType.Quads);
            GL.TexCoord3(0f, 0f, 0f);
            GL.Vertex3(left, -1f, 0f);
            GL.TexCoord3(1f, 0f, 0f);
            GL.Vertex3(right, -1f, 0f);
            GL.TexCoord3(1f, 1f, 0f);
            GL.Vertex3(right, 1f, 0f);
            GL.TexCoord3(0f, 1f, 0f);
            GL.Vertex3(left, 1f, 0f);
            GL.End();
        }

        private static int CreateSceneProgram()
            => CreateProgram(Shaders.VertexShader, Shaders.FragmentShader);

        private static int CreateProgram(string vertex, string fragment)
        {
            int program = GL.CreateProgram();
            try
            {
                foreach (var (type, source) in new[] {
                    (ShaderType.VertexShader, vertex),
                    (ShaderType.FragmentShader, fragment) })
                {
                    int shader = GL.CreateShader(type);
                    try
                    {
                        GL.ShaderSource(shader, source);
                        GL.CompileShader(shader);
                        GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
                        if (compiled == 0)
                        {
                            throw new InvalidOperationException(GL.GetShaderInfoLog(shader));
                        }
                        GL.AttachShader(program, shader);
                    }
                    finally
                    {
                        GL.DeleteShader(shader);
                    }
                }
                GL.LinkProgram(program);
                GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
                if (linked == 0)
                {
                    throw new InvalidOperationException(GL.GetProgramInfoLog(program));
                }
                return program;
            }
            catch
            {
                GL.DeleteProgram(program);
                throw;
            }
        }

        private static void CheckVulkanRejectsUnknownShader()
        {
            int shader = GL.CreateShader(ShaderType.FragmentShader);
            try
            {
                GL.ShaderSource(shader,
                    "#version 120\nvoid main() { gl_FragColor = vec4(0.25, 0.5, 0.75, 1.0); }");
                GL.CompileShader(shader);
                GL.GetShader(shader, ShaderParameter.CompileStatus, out int compiled);
                if (compiled != 0)
                {
                    throw new InvalidOperationException(
                        "Vulkan accepted a shader source with no translated backend equivalent.");
                }
                string log = GL.GetShaderInfoLog(shader);
                if (String.IsNullOrWhiteSpace(log))
                {
                    throw new InvalidOperationException(
                        "Vulkan rejected an unknown shader without an actionable info log.");
                }
                Console.WriteLine(
                    "[windowcheck] Vulkan rejects untranslated shader sources.");
            }
            finally
            {
                GL.DeleteShader(shader);
            }
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

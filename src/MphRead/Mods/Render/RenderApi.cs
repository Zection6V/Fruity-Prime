#if !ANDROID
using System;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// API-neutral renderer facade used by gameplay, HUD, launcher and capture.
    ///
    /// Backend selection happens once, before the window is created. From then
    /// on this type delegates to one IRenderBackend; it contains no Vulkan,
    /// OpenGL, Direct3D or Metal policy of its own.
    /// </summary>
    internal static class RenderApi
    {
        private static IRenderBackend Backend => RendererBackend.ActiveImplementation;

        public static void Initialize(RenderWindow window) => Backend.Initialize(window);
        public static void Shutdown() => Backend.Shutdown();
        public static void Present() => Backend.Present();
        public static void SetVSync(bool enabled) => Backend.SetVSync(enabled);
        public static void Resize(int width, int height) => Backend.Resize(width, height);

        public static void Begin(PrimitiveType mode) => Backend.Begin(mode);
        public static void End() => Backend.End();
        public static void Vertex2(float x,float y) => Backend.Vertex2(x, y);
        public static void Vertex3(float x,float y,float z) => Backend.Vertex3(x, y, z);
        public static void Vertex3(Vector3 v) => Backend.Vertex3(v);
        public static void Color3(float r,float g,float b) => Backend.Color3(r, g, b);
        public static void Color3(Vector3 v) => Backend.Color3(v);
        public static void Color4(float r,float g,float b,float a) => Backend.Color4(r, g, b, a);
        public static void Normal3(float x,float y,float z) => Backend.Normal3(x, y, z);
        public static void TexCoord2(float s,float t) => Backend.TexCoord2(s, t);
        public static void TexCoord3(float s,float t,float r) => Backend.TexCoord3(s, t, r);
        public static void TexCoord3(Vector3 v) => Backend.TexCoord3(v);
        public static void MultiTexCoord2(TextureUnit unit,float s,float t) => Backend.MultiTexCoord2(unit, s, t);
        public static int GenLists(int range) => Backend.GenLists(range);
        public static void NewList(int list,ListMode mode) => Backend.NewList(list, mode);
        public static void EndList() => Backend.EndList();
        public static void CallList(int list) => Backend.CallList(list);
        public static void DeleteLists(int list,int range) => Backend.DeleteLists(list, range);
        public static int GenTexture() => Backend.GenTexture();
        public static void DeleteTexture(int name) => Backend.DeleteTexture(name);
        public static void ActiveTexture(TextureUnit unit) => Backend.ActiveTexture(unit);
        public static void BindTexture(TextureTarget target,int name) => Backend.BindTexture(target, name);
        public static void TexParameter(TextureTarget target,TextureParameterName pname,int param) => Backend.TexParameter(target, pname, param);
        public static void TexImage2D(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,IntPtr pixels) => Backend.TexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
        public static void TexImage2D<T>(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,T[] pixels) where T:struct => Backend.TexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
        public static void TexSubImage2D<T>(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => Backend.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
        public static void TexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,IntPtr pixels) => Backend.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
        public static void CopyTexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int x,int y,int width,int height) => Backend.CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
        public static void ReadPixels<T>(int x,int y,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => Backend.ReadPixels(x, y, width, height, format, type, pixels);
        public static int CreateShader(ShaderType type) => Backend.CreateShader(type);
        public static void ShaderSource(int shader,string source) => Backend.ShaderSource(shader, source);
        public static void CompileShader(int shader) => Backend.CompileShader(shader);
        public static void GetShader(int shader,ShaderParameter pname,out int value) => Backend.GetShader(shader, pname, out value);
        public static string GetShaderInfoLog(int shader) => Backend.GetShaderInfoLog(shader);
        public static void DeleteShader(int shader) => Backend.DeleteShader(shader);
        public static int CreateProgram() => Backend.CreateProgram();
        public static void AttachShader(int program,int shader) => Backend.AttachShader(program, shader);
        public static void DetachShader(int program,int shader) => Backend.DetachShader(program, shader);
        public static void LinkProgram(int program) => Backend.LinkProgram(program);
        public static void GetProgram(int program,GetProgramParameterName pname,out int value) => Backend.GetProgram(program, pname, out value);
        public static string GetProgramInfoLog(int program) => Backend.GetProgramInfoLog(program);
        public static void DeleteProgram(int program) => Backend.DeleteProgram(program);
        public static void UseProgram(int program) => Backend.UseProgram(program);
        public static int GetUniformLocation(int program,string name) => Backend.GetUniformLocation(program, name);
        public static void Uniform1(int location,int value) => Backend.Uniform1(location, value);
        public static void Uniform1(int location,float value) => Backend.Uniform1(location, value);
        public static void Uniform1(int location,int count,float[] value) => Backend.Uniform1(location, count, value);
        public static void Uniform3(int location,Vector3 value) => Backend.Uniform3(location, value);
        public static void Uniform3(int location,int count,float[] value) => Backend.Uniform3(location, count, value);
        public static void Uniform4(int location,Vector4 value) => Backend.Uniform4(location, value);
        public static void Uniform4(int location,ref Vector4 value) => Backend.Uniform4(location, ref value);
        public static void Uniform4(int location,float x,float y,float z,float w) => Backend.Uniform4(location, x, y, z, w);
        public static void Uniform4(int location,int x,int y,int z,int w) => Backend.Uniform4(location, x, y, z, w);
        public static void UniformMatrix4(int location,bool transpose,ref Matrix4 matrix) => Backend.UniformMatrix4(location, transpose, ref matrix);
        public static void UniformMatrix4(int location,int count,bool transpose,float[] value) => Backend.UniformMatrix4(location, count, transpose, value);
        public static void Enable(EnableCap cap) => Backend.Enable(cap);
        public static void Disable(EnableCap cap) => Backend.Disable(cap);
        public static void AlphaFunc(AlphaFunction func,float reference) => Backend.AlphaFunc(func, reference);
        public static void PolygonMode(TriangleFace face,OpenTK.Graphics.OpenGL.PolygonMode mode) => Backend.PolygonMode(face, mode);
        public static void LineWidth(float width) => Backend.LineWidth(width);
        public static void DebugMessageCallback(DebugProc callback,IntPtr userParam) => Backend.DebugMessageCallback(callback, userParam);
        public static void Clear(ClearBufferMask mask) => Backend.Clear(mask);
        public static void ClearColor(Color4 color) => Backend.ClearColor(color);
        public static void ClearColor(float r,float g,float b,float a) => Backend.ClearColor(r, g, b, a);
        public static void ClearStencil(int value) => Backend.ClearStencil(value);
        public static void ColorMask(bool r,bool g,bool b,bool a) => Backend.ColorMask(r, g, b, a);
        public static void DepthMask(bool value) => Backend.DepthMask(value);
        public static void DepthFunc(DepthFunction func) => Backend.DepthFunc(func);
        public static void CullFace(TriangleFace face) => Backend.CullFace(face);
        public static void BlendFunc(BlendingFactor src,BlendingFactor dst) => Backend.BlendFunc(src, dst);
        public static void StencilFunc(StencilFunction func,int reference,int mask) => Backend.StencilFunc(func, reference, mask);
        public static void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,OpenTK.Graphics.OpenGL.StencilOp zfail,OpenTK.Graphics.OpenGL.StencilOp zpass) => Backend.StencilOp(fail, zfail, zpass);
        public static void StencilMask(int mask) => Backend.StencilMask(mask);
        public static void PolygonOffset(float factor,float units) => Backend.PolygonOffset(factor, units);
        public static void Viewport(int x,int y,int width,int height) => Backend.Viewport(x, y, width, height);
        public static void Scissor(int x,int y,int width,int height) => Backend.Scissor(x, y, width, height);
        public static void PixelStore(PixelStoreParameter pname,int param) => Backend.PixelStore(pname, param);
        public static void DrawBuffer(DrawBufferMode mode) => Backend.DrawBuffer(mode);
        public static void ReadBuffer(ReadBufferMode mode) => Backend.ReadBuffer(mode);
        public static ErrorCode GetError() => Backend.GetError();
        public static string GetString(StringName name) => Backend.GetString(name);
        public static int GetInteger(GetPName pname) => Backend.GetInteger(pname);
        public static int GenFramebuffer() => Backend.GenFramebuffer();
        public static void BindFramebuffer(FramebufferTarget target,int framebuffer) => Backend.BindFramebuffer(target, framebuffer);
        public static void FramebufferTexture2D(FramebufferTarget target,FramebufferAttachment attachment,TextureTarget textarget,int texture,int level) => Backend.FramebufferTexture2D(target, attachment, textarget, texture, level);
        public static int GenRenderbuffer() => Backend.GenRenderbuffer();
        public static void BindRenderbuffer(RenderbufferTarget target,int renderbuffer) => Backend.BindRenderbuffer(target, renderbuffer);
        public static void RenderbufferStorage(RenderbufferTarget target,OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat,int width,int height) => Backend.RenderbufferStorage(target, internalFormat, width, height);
        public static void FramebufferRenderbuffer(FramebufferTarget target,FramebufferAttachment attachment,RenderbufferTarget renderbufferTarget,int renderbuffer) => Backend.FramebufferRenderbuffer(target, attachment, renderbufferTarget, renderbuffer);
        public static void GetFramebufferAttachmentParameter(FramebufferTarget target,FramebufferAttachment attachment,FramebufferParameterName pname,out int result) => Backend.GetFramebufferAttachmentParameter(target, attachment, pname, out result);
        public static void DeleteFramebuffer(int framebuffer) => Backend.DeleteFramebuffer(framebuffer);
        public static void DeleteRenderbuffer(int renderbuffer) => Backend.DeleteRenderbuffer(renderbuffer);
        public static FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target) => Backend.CheckFramebufferStatus(target);
        public static void MatrixMode(MatrixMode mode) => Backend.MatrixMode(mode);
        public static void PushMatrix() => Backend.PushMatrix();
        public static void PopMatrix() => Backend.PopMatrix();
        public static void LoadIdentity() => Backend.LoadIdentity();
        public static void TexEnv(TextureEnvTarget target,TextureEnvParameter pname,int param) => Backend.TexEnv(target, pname, param);
    }
}
#endif

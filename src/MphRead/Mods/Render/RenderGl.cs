#if !ANDROID
using System;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace MphRead.Mods.Render
{
    internal static class RenderGl
    {
        private static bool Vk => RendererBackend.UseVulkan;

        public static void Initialize(RenderWindow window)
        {
            if (Vk) VulkanGl.Initialize(window);
        }

        public static void Shutdown()
        {
            if (Vk) VulkanGl.Shutdown();
        }

        public static void Present()
        {
            if (Vk) VulkanGl.Present();
        }

        public static void SetVSync(bool enabled)
        {
            if (Vk) VulkanGl.SetVSync(enabled);
        }

        public static void Resize(int width, int height)
        {
            if (Vk) VulkanGl.Resize(width, height);
        }

        public static void Begin(PrimitiveType mode) { if (Vk) VulkanGl.Begin(mode); else OpenTK.Graphics.OpenGL.GL.Begin(mode); }
        public static void End() { if (Vk) VulkanGl.End(); else OpenTK.Graphics.OpenGL.GL.End(); }
        public static void Vertex3(float x,float y,float z) { if (Vk) VulkanGl.Vertex3(x,y,z); else OpenTK.Graphics.OpenGL.GL.Vertex3(x,y,z); }
        public static void Vertex3(Vector3 v) { if (Vk) VulkanGl.Vertex3(v); else OpenTK.Graphics.OpenGL.GL.Vertex3(v); }
        public static void Color3(float r,float g,float b) { if (Vk) VulkanGl.Color3(r,g,b); else OpenTK.Graphics.OpenGL.GL.Color3(r,g,b); }
        public static void Color3(Vector3 v) { if (Vk) VulkanGl.Color3(v); else OpenTK.Graphics.OpenGL.GL.Color3(v); }
        public static void Color4(float r,float g,float b,float a) { if (Vk) VulkanGl.Color4(r,g,b,a); else OpenTK.Graphics.OpenGL.GL.Color4(r,g,b,a); }
        public static void Normal3(float x,float y,float z) { if (Vk) VulkanGl.Normal3(x,y,z); else OpenTK.Graphics.OpenGL.GL.Normal3(x,y,z); }
        public static void TexCoord2(float s,float t) { if (Vk) VulkanGl.TexCoord2(s,t); else OpenTK.Graphics.OpenGL.GL.TexCoord2(s,t); }
        public static void TexCoord3(float s,float t,float r) { if (Vk) VulkanGl.TexCoord3(s,t,r); else OpenTK.Graphics.OpenGL.GL.TexCoord3(s,t,r); }
        public static void TexCoord3(Vector3 v) { if (Vk) VulkanGl.TexCoord3(v); else OpenTK.Graphics.OpenGL.GL.TexCoord3(v); }
        public static void MultiTexCoord2(TextureUnit unit,float s,float t) { if (Vk) VulkanGl.MultiTexCoord2(unit,s,t); else OpenTK.Graphics.OpenGL.GL.MultiTexCoord2(unit,s,t); }

        public static int GenLists(int range) => Vk ? VulkanGl.GenLists(range) : OpenTK.Graphics.OpenGL.GL.GenLists(range);
        public static void NewList(int list,ListMode mode) { if (Vk) VulkanGl.NewList(list,mode); else OpenTK.Graphics.OpenGL.GL.NewList(list,mode); }
        public static void EndList() { if (Vk) VulkanGl.EndList(); else OpenTK.Graphics.OpenGL.GL.EndList(); }
        public static void CallList(int list) { if (Vk) VulkanGl.CallList(list); else OpenTK.Graphics.OpenGL.GL.CallList(list); }
        public static void DeleteLists(int list,int range) { if (Vk) VulkanGl.DeleteLists(list,range); else OpenTK.Graphics.OpenGL.GL.DeleteLists(list,range); }

        public static int GenTexture() => Vk ? VulkanGl.GenTexture() : OpenTK.Graphics.OpenGL.GL.GenTexture();
        public static void DeleteTexture(int name) { if (Vk) VulkanGl.DeleteTexture(name); else OpenTK.Graphics.OpenGL.GL.DeleteTexture(name); }
        public static void ActiveTexture(TextureUnit unit) { if (Vk) VulkanGl.ActiveTexture(unit); else OpenTK.Graphics.OpenGL.GL.ActiveTexture(unit); }
        public static void BindTexture(TextureTarget target,int name) { if (Vk) VulkanGl.BindTexture(target,name); else OpenTK.Graphics.OpenGL.GL.BindTexture(target,name); }
        public static void TexParameter(TextureTarget target,TextureParameterName pname,int param) { if (Vk) VulkanGl.TexParameter(target,pname,param); else OpenTK.Graphics.OpenGL.GL.TexParameter(target,pname,param); }
        public static void TexImage2D(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,IntPtr pixels)
        { if (Vk) VulkanGl.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels); else OpenTK.Graphics.OpenGL.GL.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels); }
        public static void TexImage2D<T>(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,T[] pixels) where T:struct
        { if (Vk) VulkanGl.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels); else OpenTK.Graphics.OpenGL.GL.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels); }
        public static void TexSubImage2D<T>(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct
        { if (Vk) VulkanGl.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels); else OpenTK.Graphics.OpenGL.GL.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels); }
        public static void CopyTexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int x,int y,int width,int height)
        { if (Vk) VulkanGl.CopyTexSubImage2D(target,level,xoffset,yoffset,x,y,width,height); else OpenTK.Graphics.OpenGL.GL.CopyTexSubImage2D(target,level,xoffset,yoffset,x,y,width,height); }
        public static void ReadPixels<T>(int x,int y,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct
        { if (Vk) VulkanGl.ReadPixels(x,y,width,height,format,type,pixels); else OpenTK.Graphics.OpenGL.GL.ReadPixels(x,y,width,height,format,type,pixels); }

        public static int CreateShader(ShaderType type) => Vk ? VulkanGl.CreateShader(type) : OpenTK.Graphics.OpenGL.GL.CreateShader(type);
        public static void ShaderSource(int shader,string source) { if (Vk) VulkanGl.ShaderSource(shader,source); else OpenTK.Graphics.OpenGL.GL.ShaderSource(shader,source); }
        public static void CompileShader(int shader) { if (Vk) VulkanGl.CompileShader(shader); else OpenTK.Graphics.OpenGL.GL.CompileShader(shader); }
        public static void GetShader(int shader,ShaderParameter pname,out int value) { if (Vk) VulkanGl.GetShader(shader,pname,out value); else OpenTK.Graphics.OpenGL.GL.GetShader(shader,pname,out value); }
        public static string GetShaderInfoLog(int shader) => Vk ? VulkanGl.GetShaderInfoLog(shader) : OpenTK.Graphics.OpenGL.GL.GetShaderInfoLog(shader);
        public static void DeleteShader(int shader) { if (Vk) VulkanGl.DeleteShader(shader); else OpenTK.Graphics.OpenGL.GL.DeleteShader(shader); }

        public static int CreateProgram() => Vk ? VulkanGl.CreateProgram() : OpenTK.Graphics.OpenGL.GL.CreateProgram();
        public static void AttachShader(int program,int shader) { if (Vk) VulkanGl.AttachShader(program,shader); else OpenTK.Graphics.OpenGL.GL.AttachShader(program,shader); }
        public static void DetachShader(int program,int shader) { if (Vk) VulkanGl.DetachShader(program,shader); else OpenTK.Graphics.OpenGL.GL.DetachShader(program,shader); }
        public static void LinkProgram(int program) { if (Vk) VulkanGl.LinkProgram(program); else OpenTK.Graphics.OpenGL.GL.LinkProgram(program); }
        public static void GetProgram(int program,GetProgramParameterName pname,out int value) { if (Vk) VulkanGl.GetProgram(program,pname,out value); else OpenTK.Graphics.OpenGL.GL.GetProgram(program,pname,out value); }
        public static string GetProgramInfoLog(int program) => Vk ? VulkanGl.GetProgramInfoLog(program) : OpenTK.Graphics.OpenGL.GL.GetProgramInfoLog(program);
        public static void DeleteProgram(int program) { if (Vk) VulkanGl.DeleteProgram(program); else OpenTK.Graphics.OpenGL.GL.DeleteProgram(program); }
        public static void UseProgram(int program) { if (Vk) VulkanGl.UseProgram(program); else OpenTK.Graphics.OpenGL.GL.UseProgram(program); }
        public static int GetUniformLocation(int program,string name) => Vk ? VulkanGl.GetUniformLocation(program,name) : OpenTK.Graphics.OpenGL.GL.GetUniformLocation(program,name);

        public static void Uniform1(int location,int value) { if (Vk) VulkanGl.Uniform1(location,value); else OpenTK.Graphics.OpenGL.GL.Uniform1(location,value); }
        public static void Uniform1(int location,float value) { if (Vk) VulkanGl.Uniform1(location,value); else OpenTK.Graphics.OpenGL.GL.Uniform1(location,value); }
        public static void Uniform1(int location,int count,float[] value) { if (Vk) VulkanGl.Uniform1(location,count,value); else OpenTK.Graphics.OpenGL.GL.Uniform1(location,count,value); }
        public static void Uniform3(int location,Vector3 value) { if (Vk) VulkanGl.Uniform3(location,value); else OpenTK.Graphics.OpenGL.GL.Uniform3(location,value); }
        public static void Uniform3(int location,int count,float[] value) { if (Vk) VulkanGl.Uniform3(location,count,value); else OpenTK.Graphics.OpenGL.GL.Uniform3(location,count,value); }
        public static void Uniform4(int location,Vector4 value) { if (Vk) VulkanGl.Uniform4(location,value); else OpenTK.Graphics.OpenGL.GL.Uniform4(location,value); }
        public static void Uniform4(int location,ref Vector4 value) { if (Vk) VulkanGl.Uniform4(location,ref value); else OpenTK.Graphics.OpenGL.GL.Uniform4(location,ref value); }
        public static void Uniform4(int location,float x,float y,float z,float w) { if (Vk) VulkanGl.Uniform4(location,x,y,z,w); else OpenTK.Graphics.OpenGL.GL.Uniform4(location,x,y,z,w); }
        public static void Uniform4(int location,int x,int y,int z,int w) { if (Vk) VulkanGl.Uniform4(location,x,y,z,w); else OpenTK.Graphics.OpenGL.GL.Uniform4(location,x,y,z,w); }
        public static void UniformMatrix4(int location,bool transpose,ref Matrix4 matrix) { if (Vk) VulkanGl.UniformMatrix4(location,transpose,ref matrix); else OpenTK.Graphics.OpenGL.GL.UniformMatrix4(location,transpose,ref matrix); }
        public static void UniformMatrix4(int location,int count,bool transpose,float[] value) { if (Vk) VulkanGl.UniformMatrix4(location,count,transpose,value); else OpenTK.Graphics.OpenGL.GL.UniformMatrix4(location,count,transpose,value); }

        public static void Enable(EnableCap cap) { if (Vk) VulkanGl.Enable(cap); else OpenTK.Graphics.OpenGL.GL.Enable(cap); }
        public static void Disable(EnableCap cap) { if (Vk) VulkanGl.Disable(cap); else OpenTK.Graphics.OpenGL.GL.Disable(cap); }
        public static void AlphaFunc(AlphaFunction func,float reference) { if (Vk) VulkanGl.AlphaFunc(func,reference); else OpenTK.Graphics.OpenGL.GL.AlphaFunc(func,reference); }
        public static void PolygonMode(TriangleFace face,OpenTK.Graphics.OpenGL.PolygonMode mode) { if (Vk) VulkanGl.PolygonMode(face,mode); else OpenTK.Graphics.OpenGL.GL.PolygonMode(face,mode); }
        public static void LineWidth(float width) { if (Vk) VulkanGl.LineWidth(width); else OpenTK.Graphics.OpenGL.GL.LineWidth(width); }
        public static void DebugMessageCallback(DebugProc callback,IntPtr userParam) { if (Vk) VulkanGl.DebugMessageCallback(callback,userParam); else OpenTK.Graphics.OpenGL.GL.DebugMessageCallback(callback,userParam); }
        public static void Clear(ClearBufferMask mask) { if (Vk) VulkanGl.Clear(mask); else OpenTK.Graphics.OpenGL.GL.Clear(mask); }
        public static void ClearColor(Color4 color) { if (Vk) VulkanGl.ClearColor(color); else OpenTK.Graphics.OpenGL.GL.ClearColor(color); }
        public static void ClearColor(float r,float g,float b,float a) { if (Vk) VulkanGl.ClearColor(r,g,b,a); else OpenTK.Graphics.OpenGL.GL.ClearColor(r,g,b,a); }
        public static void ClearStencil(int value) { if (Vk) VulkanGl.ClearStencil(value); else OpenTK.Graphics.OpenGL.GL.ClearStencil(value); }
        public static void ColorMask(bool r,bool g,bool b,bool a) { if (Vk) VulkanGl.ColorMask(r,g,b,a); else OpenTK.Graphics.OpenGL.GL.ColorMask(r,g,b,a); }
        public static void DepthMask(bool value) { if (Vk) VulkanGl.DepthMask(value); else OpenTK.Graphics.OpenGL.GL.DepthMask(value); }
        public static void DepthFunc(DepthFunction func) { if (Vk) VulkanGl.DepthFunc(func); else OpenTK.Graphics.OpenGL.GL.DepthFunc(func); }
        public static void CullFace(TriangleFace face) { if (Vk) VulkanGl.CullFace(face); else OpenTK.Graphics.OpenGL.GL.CullFace(face); }
        public static void BlendFunc(BlendingFactor src,BlendingFactor dst) { if (Vk) VulkanGl.BlendFunc(src,dst); else OpenTK.Graphics.OpenGL.GL.BlendFunc(src,dst); }
        public static void StencilFunc(StencilFunction func,int reference,int mask) { if (Vk) VulkanGl.StencilFunc(func,reference,mask); else OpenTK.Graphics.OpenGL.GL.StencilFunc(func,reference,mask); }
        public static void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,OpenTK.Graphics.OpenGL.StencilOp zfail,OpenTK.Graphics.OpenGL.StencilOp zpass) { if (Vk) VulkanGl.StencilOp(fail,zfail,zpass); else OpenTK.Graphics.OpenGL.GL.StencilOp(fail,zfail,zpass); }
        public static void StencilMask(int mask) { if (Vk) VulkanGl.StencilMask(mask); else OpenTK.Graphics.OpenGL.GL.StencilMask(mask); }
        public static void PolygonOffset(float factor,float units) { if (Vk) VulkanGl.PolygonOffset(factor,units); else OpenTK.Graphics.OpenGL.GL.PolygonOffset(factor,units); }
        public static void Viewport(int x,int y,int width,int height) { if (Vk) VulkanGl.Viewport(x,y,width,height); else OpenTK.Graphics.OpenGL.GL.Viewport(x,y,width,height); }
        public static void Scissor(int x,int y,int width,int height) { if (Vk) VulkanGl.Scissor(x,y,width,height); else OpenTK.Graphics.OpenGL.GL.Scissor(x,y,width,height); }
        public static void PixelStore(PixelStoreParameter pname,int param) { if (Vk) VulkanGl.PixelStore(pname,param); else OpenTK.Graphics.OpenGL.GL.PixelStore(pname,param); }
        public static void ReadBuffer(ReadBufferMode mode) { if (Vk) VulkanGl.ReadBuffer(mode); else OpenTK.Graphics.OpenGL.GL.ReadBuffer(mode); }
        public static ErrorCode GetError() => Vk ? VulkanGl.GetError() : OpenTK.Graphics.OpenGL.GL.GetError();
        public static string GetString(StringName name) => Vk ? VulkanGl.GetString(name) : OpenTK.Graphics.OpenGL.GL.GetString(name);
        public static int GetInteger(GetPName pname) => Vk ? VulkanGl.GetInteger(pname) : OpenTK.Graphics.OpenGL.GL.GetInteger(pname);

        public static int GenFramebuffer() => Vk ? VulkanGl.GenFramebuffer() : OpenTK.Graphics.OpenGL.GL.GenFramebuffer();
        public static void BindFramebuffer(FramebufferTarget target,int framebuffer) { if (Vk) VulkanGl.BindFramebuffer(target,framebuffer); else OpenTK.Graphics.OpenGL.GL.BindFramebuffer(target,framebuffer); }
        public static void FramebufferTexture2D(FramebufferTarget target,FramebufferAttachment attachment,TextureTarget textarget,int texture,int level)
        { if (Vk) VulkanGl.FramebufferTexture2D(target,attachment,textarget,texture,level); else OpenTK.Graphics.OpenGL.GL.FramebufferTexture2D(target,attachment,textarget,texture,level); }
        public static int GenRenderbuffer() => Vk ? VulkanGl.GenRenderbuffer() : OpenTK.Graphics.OpenGL.GL.GenRenderbuffer();
        public static void BindRenderbuffer(RenderbufferTarget target,int renderbuffer) { if (Vk) VulkanGl.BindRenderbuffer(target,renderbuffer); else OpenTK.Graphics.OpenGL.GL.BindRenderbuffer(target,renderbuffer); }
        public static void RenderbufferStorage(RenderbufferTarget target,OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat,int width,int height)
        { if (Vk) VulkanGl.RenderbufferStorage(target,internalFormat,width,height); else OpenTK.Graphics.OpenGL.GL.RenderbufferStorage(target,internalFormat,width,height); }
        public static void FramebufferRenderbuffer(FramebufferTarget target,FramebufferAttachment attachment,RenderbufferTarget renderbufferTarget,int renderbuffer)
        { if (Vk) VulkanGl.FramebufferRenderbuffer(target,attachment,renderbufferTarget,renderbuffer); else OpenTK.Graphics.OpenGL.GL.FramebufferRenderbuffer(target,attachment,renderbufferTarget,renderbuffer); }
        public static void GetFramebufferAttachmentParameter(FramebufferTarget target,FramebufferAttachment attachment,FramebufferParameterName pname,out int result)
        { if (Vk) VulkanGl.GetFramebufferAttachmentParameter(target,attachment,pname,out result); else OpenTK.Graphics.OpenGL.GL.GetFramebufferAttachmentParameter(target,attachment,pname,out result); }
        public static void DeleteFramebuffer(int framebuffer) { if (Vk) VulkanGl.DeleteFramebuffer(framebuffer); else OpenTK.Graphics.OpenGL.GL.DeleteFramebuffer(framebuffer); }
        public static void DeleteRenderbuffer(int renderbuffer) { if (Vk) VulkanGl.DeleteRenderbuffer(renderbuffer); else OpenTK.Graphics.OpenGL.GL.DeleteRenderbuffer(renderbuffer); }
        public static FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target) => Vk ? VulkanGl.CheckFramebufferStatus(target) : OpenTK.Graphics.OpenGL.GL.CheckFramebufferStatus(target);

        public static void MatrixMode(MatrixMode mode) { if (Vk) VulkanGl.MatrixMode(mode); else OpenTK.Graphics.OpenGL.GL.MatrixMode(mode); }
        public static void PushMatrix() { if (Vk) VulkanGl.PushMatrix(); else OpenTK.Graphics.OpenGL.GL.PushMatrix(); }
        public static void PopMatrix() { if (Vk) VulkanGl.PopMatrix(); else OpenTK.Graphics.OpenGL.GL.PopMatrix(); }
        public static void LoadIdentity() { if (Vk) VulkanGl.LoadIdentity(); else OpenTK.Graphics.OpenGL.GL.LoadIdentity(); }
        public static void TexEnv(TextureEnvTarget target,TextureEnvParameter pname,int param) { if (Vk) VulkanGl.TexEnv(target,pname,param); else OpenTK.Graphics.OpenGL.GL.TexEnv(target,pname,param); }
    }
}
#endif

#if !ANDROID
using System;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// OpenGL implementation of the renderer contract.
    /// No selection policy lives here: this class only translates commands.
    /// </summary>
    internal sealed class OpenGlRenderBackend : IRenderBackend
    {
        private RenderWindow? _window;

        public RendererBackendKind Kind => RendererBackendKind.OpenGL;
        public string DisplayName => "OpenGL";
        public RenderWindowApi WindowApi => RenderWindowApi.OpenGL;
        public bool OwnsPresentation => false;

        public bool IsSupportedOnCurrentPlatform() => !OperatingSystem.IsAndroid();
        public bool IsRuntimeAvailable() => IsSupportedOnCurrentPlatform();

        public void Initialize(RenderWindow window) => _window = window;
        public void Shutdown() => _window = null;
        public void Present() { }
        public void SetVSync(bool enabled)
        {
            if (_window != null)
            {
                _window.VSync = enabled ? VSyncMode.On : VSyncMode.Off;
            }
        }
        public void Resize(int width, int height) { }

        public void Begin(PrimitiveType mode) => OpenTK.Graphics.OpenGL.GL.Begin(mode);
        public void End() => OpenTK.Graphics.OpenGL.GL.End();
        public void Vertex2(float x,float y) => OpenTK.Graphics.OpenGL.GL.Vertex2(x,y);
        public void Vertex3(float x,float y,float z) => OpenTK.Graphics.OpenGL.GL.Vertex3(x,y,z);
        public void Vertex3(Vector3 v) => OpenTK.Graphics.OpenGL.GL.Vertex3(v);
        public void Color3(float r,float g,float b) => OpenTK.Graphics.OpenGL.GL.Color3(r,g,b);
        public void Color3(Vector3 v) => OpenTK.Graphics.OpenGL.GL.Color3(v);
        public void Color4(float r,float g,float b,float a) => OpenTK.Graphics.OpenGL.GL.Color4(r,g,b,a);
        public void Normal3(float x,float y,float z) => OpenTK.Graphics.OpenGL.GL.Normal3(x,y,z);
        public void TexCoord2(float s,float t) => OpenTK.Graphics.OpenGL.GL.TexCoord2(s,t);
        public void TexCoord3(float s,float t,float r) => OpenTK.Graphics.OpenGL.GL.TexCoord3(s,t,r);
        public void TexCoord3(Vector3 v) => OpenTK.Graphics.OpenGL.GL.TexCoord3(v);
        public void MultiTexCoord2(TextureUnit unit,float s,float t) => OpenTK.Graphics.OpenGL.GL.MultiTexCoord2(unit,s,t);
        public int GenLists(int range) => OpenTK.Graphics.OpenGL.GL.GenLists(range);
        public void NewList(int list,ListMode mode) => OpenTK.Graphics.OpenGL.GL.NewList(list,mode);
        public void EndList() => OpenTK.Graphics.OpenGL.GL.EndList();
        public void CallList(int list) => OpenTK.Graphics.OpenGL.GL.CallList(list);
        public void DeleteLists(int list,int range) => OpenTK.Graphics.OpenGL.GL.DeleteLists(list,range);
        public int GenTexture() => OpenTK.Graphics.OpenGL.GL.GenTexture();
        public void DeleteTexture(int name) => OpenTK.Graphics.OpenGL.GL.DeleteTexture(name);
        public void ActiveTexture(TextureUnit unit) => OpenTK.Graphics.OpenGL.GL.ActiveTexture(unit);
        public void BindTexture(TextureTarget target,int name) => OpenTK.Graphics.OpenGL.GL.BindTexture(target,name);
        public void TexParameter(TextureTarget target,TextureParameterName pname,int param) => OpenTK.Graphics.OpenGL.GL.TexParameter(target,pname,param);
        public void TexImage2D(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,IntPtr pixels) => OpenTK.Graphics.OpenGL.GL.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels);
        public void TexImage2D<T>(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,T[] pixels) where T:struct => OpenTK.Graphics.OpenGL.GL.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels);
        public void TexSubImage2D<T>(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => OpenTK.Graphics.OpenGL.GL.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels);
        public void TexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,IntPtr pixels) => OpenTK.Graphics.OpenGL.GL.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels);
        public void CopyTexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int x,int y,int width,int height) => OpenTK.Graphics.OpenGL.GL.CopyTexSubImage2D(target,level,xoffset,yoffset,x,y,width,height);
        public void ReadPixels<T>(int x,int y,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => OpenTK.Graphics.OpenGL.GL.ReadPixels(x,y,width,height,format,type,pixels);
        public int CreateShader(ShaderType type) => OpenTK.Graphics.OpenGL.GL.CreateShader(type);
        public void ShaderSource(int shader,string source) => OpenTK.Graphics.OpenGL.GL.ShaderSource(shader,source);
        public void CompileShader(int shader) => OpenTK.Graphics.OpenGL.GL.CompileShader(shader);
        public void GetShader(int shader,ShaderParameter pname,out int value) => OpenTK.Graphics.OpenGL.GL.GetShader(shader,pname,out value);
        public string GetShaderInfoLog(int shader) => OpenTK.Graphics.OpenGL.GL.GetShaderInfoLog(shader);
        public void DeleteShader(int shader) => OpenTK.Graphics.OpenGL.GL.DeleteShader(shader);
        public int CreateProgram() => OpenTK.Graphics.OpenGL.GL.CreateProgram();
        public void AttachShader(int program,int shader) => OpenTK.Graphics.OpenGL.GL.AttachShader(program,shader);
        public void DetachShader(int program,int shader) => OpenTK.Graphics.OpenGL.GL.DetachShader(program,shader);
        public void LinkProgram(int program) => OpenTK.Graphics.OpenGL.GL.LinkProgram(program);
        public void GetProgram(int program,GetProgramParameterName pname,out int value) => OpenTK.Graphics.OpenGL.GL.GetProgram(program,pname,out value);
        public string GetProgramInfoLog(int program) => OpenTK.Graphics.OpenGL.GL.GetProgramInfoLog(program);
        public void DeleteProgram(int program) => OpenTK.Graphics.OpenGL.GL.DeleteProgram(program);
        public void UseProgram(int program) => OpenTK.Graphics.OpenGL.GL.UseProgram(program);
        public int GetUniformLocation(int program,string name) => OpenTK.Graphics.OpenGL.GL.GetUniformLocation(program,name);
        public void Uniform1(int location,int value) => OpenTK.Graphics.OpenGL.GL.Uniform1(location,value);
        public void Uniform1(int location,float value) => OpenTK.Graphics.OpenGL.GL.Uniform1(location,value);
        public void Uniform1(int location,int count,float[] value) => OpenTK.Graphics.OpenGL.GL.Uniform1(location,count,value);
        public void Uniform3(int location,Vector3 value) => OpenTK.Graphics.OpenGL.GL.Uniform3(location,value);
        public void Uniform3(int location,int count,float[] value) => OpenTK.Graphics.OpenGL.GL.Uniform3(location,count,value);
        public void Uniform4(int location,Vector4 value) => OpenTK.Graphics.OpenGL.GL.Uniform4(location,value);
        public void Uniform4(int location,ref Vector4 value) => OpenTK.Graphics.OpenGL.GL.Uniform4(location,ref value);
        public void Uniform4(int location,float x,float y,float z,float w) => OpenTK.Graphics.OpenGL.GL.Uniform4(location,x,y,z,w);
        public void Uniform4(int location,int x,int y,int z,int w) => OpenTK.Graphics.OpenGL.GL.Uniform4(location,x,y,z,w);
        public void UniformMatrix4(int location,bool transpose,ref Matrix4 matrix) => OpenTK.Graphics.OpenGL.GL.UniformMatrix4(location,transpose,ref matrix);
        public void UniformMatrix4(int location,int count,bool transpose,float[] value) => OpenTK.Graphics.OpenGL.GL.UniformMatrix4(location,count,transpose,value);
        public void Enable(EnableCap cap) => OpenTK.Graphics.OpenGL.GL.Enable(cap);
        public void Disable(EnableCap cap) => OpenTK.Graphics.OpenGL.GL.Disable(cap);
        public void AlphaFunc(AlphaFunction func,float reference) => OpenTK.Graphics.OpenGL.GL.AlphaFunc(func,reference);
        public void PolygonMode(TriangleFace face,OpenTK.Graphics.OpenGL.PolygonMode mode) => OpenTK.Graphics.OpenGL.GL.PolygonMode(face,mode);
        public void LineWidth(float width) => OpenTK.Graphics.OpenGL.GL.LineWidth(width);
        public void DebugMessageCallback(DebugProc callback,IntPtr userParam) => OpenTK.Graphics.OpenGL.GL.DebugMessageCallback(callback,userParam);
        public void Clear(ClearBufferMask mask) => OpenTK.Graphics.OpenGL.GL.Clear(mask);
        public void ClearColor(Color4 color) => OpenTK.Graphics.OpenGL.GL.ClearColor(color);
        public void ClearColor(float r,float g,float b,float a) => OpenTK.Graphics.OpenGL.GL.ClearColor(r,g,b,a);
        public void ClearStencil(int value) => OpenTK.Graphics.OpenGL.GL.ClearStencil(value);
        public void ColorMask(bool r,bool g,bool b,bool a) => OpenTK.Graphics.OpenGL.GL.ColorMask(r,g,b,a);
        public void DepthMask(bool value) => OpenTK.Graphics.OpenGL.GL.DepthMask(value);
        public void DepthFunc(DepthFunction func) => OpenTK.Graphics.OpenGL.GL.DepthFunc(func);
        public void CullFace(TriangleFace face) => OpenTK.Graphics.OpenGL.GL.CullFace(face);
        public void BlendFunc(BlendingFactor src,BlendingFactor dst) => OpenTK.Graphics.OpenGL.GL.BlendFunc(src,dst);
        public void StencilFunc(StencilFunction func,int reference,int mask) => OpenTK.Graphics.OpenGL.GL.StencilFunc(func,reference,mask);
        public void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,OpenTK.Graphics.OpenGL.StencilOp zfail,OpenTK.Graphics.OpenGL.StencilOp zpass) => OpenTK.Graphics.OpenGL.GL.StencilOp(fail,zfail,zpass);
        public void StencilMask(int mask) => OpenTK.Graphics.OpenGL.GL.StencilMask(mask);
        public void PolygonOffset(float factor,float units) => OpenTK.Graphics.OpenGL.GL.PolygonOffset(factor,units);
        public void Viewport(int x,int y,int width,int height) => OpenTK.Graphics.OpenGL.GL.Viewport(x,y,width,height);
        public void Scissor(int x,int y,int width,int height) => OpenTK.Graphics.OpenGL.GL.Scissor(x,y,width,height);
        public void PixelStore(PixelStoreParameter pname,int param) => OpenTK.Graphics.OpenGL.GL.PixelStore(pname,param);
        public void DrawBuffer(DrawBufferMode mode) => OpenTK.Graphics.OpenGL.GL.DrawBuffer(mode);
        public void ReadBuffer(ReadBufferMode mode) => OpenTK.Graphics.OpenGL.GL.ReadBuffer(mode);
        public ErrorCode GetError() => OpenTK.Graphics.OpenGL.GL.GetError();
        public string GetString(StringName name) => OpenTK.Graphics.OpenGL.GL.GetString(name);
        public int GetInteger(GetPName pname) => OpenTK.Graphics.OpenGL.GL.GetInteger(pname);
        public int GenFramebuffer() => OpenTK.Graphics.OpenGL.GL.GenFramebuffer();
        public void BindFramebuffer(FramebufferTarget target,int framebuffer) => OpenTK.Graphics.OpenGL.GL.BindFramebuffer(target,framebuffer);
        public void FramebufferTexture2D(FramebufferTarget target,FramebufferAttachment attachment,TextureTarget textarget,int texture,int level) => OpenTK.Graphics.OpenGL.GL.FramebufferTexture2D(target,attachment,textarget,texture,level);
        public int GenRenderbuffer() => OpenTK.Graphics.OpenGL.GL.GenRenderbuffer();
        public void BindRenderbuffer(RenderbufferTarget target,int renderbuffer) => OpenTK.Graphics.OpenGL.GL.BindRenderbuffer(target,renderbuffer);
        public void RenderbufferStorage(RenderbufferTarget target,OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat,int width,int height) => OpenTK.Graphics.OpenGL.GL.RenderbufferStorage(target,internalFormat,width,height);
        public void FramebufferRenderbuffer(FramebufferTarget target,FramebufferAttachment attachment,RenderbufferTarget renderbufferTarget,int renderbuffer) => OpenTK.Graphics.OpenGL.GL.FramebufferRenderbuffer(target,attachment,renderbufferTarget,renderbuffer);
        public void GetFramebufferAttachmentParameter(FramebufferTarget target,FramebufferAttachment attachment,FramebufferParameterName pname,out int result) => OpenTK.Graphics.OpenGL.GL.GetFramebufferAttachmentParameter(target,attachment,pname,out result);
        public void DeleteFramebuffer(int framebuffer) => OpenTK.Graphics.OpenGL.GL.DeleteFramebuffer(framebuffer);
        public void DeleteRenderbuffer(int renderbuffer) => OpenTK.Graphics.OpenGL.GL.DeleteRenderbuffer(renderbuffer);
        public FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target) => OpenTK.Graphics.OpenGL.GL.CheckFramebufferStatus(target);
        public void MatrixMode(MatrixMode mode) => OpenTK.Graphics.OpenGL.GL.MatrixMode(mode);
        public void PushMatrix() => OpenTK.Graphics.OpenGL.GL.PushMatrix();
        public void PopMatrix() => OpenTK.Graphics.OpenGL.GL.PopMatrix();
        public void LoadIdentity() => OpenTK.Graphics.OpenGL.GL.LoadIdentity();
        public void TexEnv(TextureEnvTarget target,TextureEnvParameter pname,int param) => OpenTK.Graphics.OpenGL.GL.TexEnv(target,pname,param);
    }
}
#endif

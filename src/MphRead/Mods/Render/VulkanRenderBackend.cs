#if !ANDROID
using System;
using System.Runtime.InteropServices;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// Vulkan implementation of the renderer contract.
    /// Platform loader work belongs here rather than in window/configuration code.
    /// </summary>
    internal sealed class VulkanRenderBackend : IRenderBackend
    {
        private static bool _loaderPrepared;

        public RendererBackendKind Kind => RendererBackendKind.Vulkan;
        public string DisplayName => "Vulkan";
        public RenderWindowApi WindowApi => RenderWindowApi.NoApi;
        public bool OwnsPresentation => true;

        public bool IsSupportedOnCurrentPlatform()
        {
            if (OperatingSystem.IsAndroid())
            {
                return false;
            }
            // Veldrid.SPIRV 1.0.14 does not ship an arm64 macOS runtime shader
            // compiler. Once shaders are precompiled this architecture gate can
            // be removed without touching selection/window code.
            if (OperatingSystem.IsMacOS()
                && RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
            {
                return false;
            }
            return OperatingSystem.IsWindows() || OperatingSystem.IsLinux() || OperatingSystem.IsMacOS();
        }

        public bool IsRuntimeAvailable()
        {
            if (!IsSupportedOnCurrentPlatform())
            {
                return false;
            }
            PrepareLoader();
            return Veldrid.GraphicsDevice.IsBackendSupported(Veldrid.GraphicsBackend.Vulkan);
        }

        private static void PrepareLoader()
        {
            if (_loaderPrepared || !OperatingSystem.IsLinux())
            {
                return;
            }
            _loaderPrepared = true;
            try
            {
                NativeLibrary.SetDllImportResolver(
                    typeof(Vulkan.VulkanNative).Assembly,
                    (libraryName, _, _) =>
                    {
                        if (libraryName == "libdl"
                            && NativeLibrary.TryLoad("libdl.so.2", out IntPtr handle))
                        {
                            return handle;
                        }
                        return IntPtr.Zero;
                    });
            }
            catch (InvalidOperationException)
            {
                // Another component already owns this assembly's resolver.
            }
        }

        public void Initialize(RenderWindow window) => VulkanGl.Initialize(window);
        public void Shutdown() => VulkanGl.Shutdown();
        public void Present() => VulkanGl.Present();
        public void SetVSync(bool enabled) => VulkanGl.SetVSync(enabled);
        public void Resize(int width, int height) => VulkanGl.Resize(width, height);

        public void Begin(PrimitiveType mode) => VulkanGl.Begin(mode);
        public void End() => VulkanGl.End();
        public void Vertex2(float x,float y) => VulkanGl.Vertex2(x,y);
        public void Vertex3(float x,float y,float z) => VulkanGl.Vertex3(x,y,z);
        public void Vertex3(Vector3 v) => VulkanGl.Vertex3(v);
        public void Color3(float r,float g,float b) => VulkanGl.Color3(r,g,b);
        public void Color3(Vector3 v) => VulkanGl.Color3(v);
        public void Color4(float r,float g,float b,float a) => VulkanGl.Color4(r,g,b,a);
        public void Normal3(float x,float y,float z) => VulkanGl.Normal3(x,y,z);
        public void TexCoord2(float s,float t) => VulkanGl.TexCoord2(s,t);
        public void TexCoord3(float s,float t,float r) => VulkanGl.TexCoord3(s,t,r);
        public void TexCoord3(Vector3 v) => VulkanGl.TexCoord3(v);
        public void MultiTexCoord2(TextureUnit unit,float s,float t) => VulkanGl.MultiTexCoord2(unit,s,t);
        public int GenLists(int range) => VulkanGl.GenLists(range);
        public void NewList(int list,ListMode mode) => VulkanGl.NewList(list,mode);
        public void EndList() => VulkanGl.EndList();
        public void CallList(int list) => VulkanGl.CallList(list);
        public void DeleteLists(int list,int range) => VulkanGl.DeleteLists(list,range);
        public int GenTexture() => VulkanGl.GenTexture();
        public void DeleteTexture(int name) => VulkanGl.DeleteTexture(name);
        public void ActiveTexture(TextureUnit unit) => VulkanGl.ActiveTexture(unit);
        public void BindTexture(TextureTarget target,int name) => VulkanGl.BindTexture(target,name);
        public void TexParameter(TextureTarget target,TextureParameterName pname,int param) => VulkanGl.TexParameter(target,pname,param);
        public void TexImage2D(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,IntPtr pixels) => VulkanGl.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels);
        public void TexImage2D<T>(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,T[] pixels) where T:struct => VulkanGl.TexImage2D(target,level,internalFormat,width,height,border,format,type,pixels);
        public void TexSubImage2D<T>(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => VulkanGl.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels);
        public void TexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,IntPtr pixels) => VulkanGl.TexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels);
        public void CopyTexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int x,int y,int width,int height) => VulkanGl.CopyTexSubImage2D(target,level,xoffset,yoffset,x,y,width,height);
        public void ReadPixels<T>(int x,int y,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct => VulkanGl.ReadPixels(x,y,width,height,format,type,pixels);
        public int CreateShader(ShaderType type) => VulkanGl.CreateShader(type);
        public void ShaderSource(int shader,string source) => VulkanGl.ShaderSource(shader,source);
        public void CompileShader(int shader) => VulkanGl.CompileShader(shader);
        public void GetShader(int shader,ShaderParameter pname,out int value) => VulkanGl.GetShader(shader,pname,out value);
        public string GetShaderInfoLog(int shader) => VulkanGl.GetShaderInfoLog(shader);
        public void DeleteShader(int shader) => VulkanGl.DeleteShader(shader);
        public int CreateProgram() => VulkanGl.CreateProgram();
        public void AttachShader(int program,int shader) => VulkanGl.AttachShader(program,shader);
        public void DetachShader(int program,int shader) => VulkanGl.DetachShader(program,shader);
        public void LinkProgram(int program) => VulkanGl.LinkProgram(program);
        public void GetProgram(int program,GetProgramParameterName pname,out int value) => VulkanGl.GetProgram(program,pname,out value);
        public string GetProgramInfoLog(int program) => VulkanGl.GetProgramInfoLog(program);
        public void DeleteProgram(int program) => VulkanGl.DeleteProgram(program);
        public void UseProgram(int program) => VulkanGl.UseProgram(program);
        public int GetUniformLocation(int program,string name) => VulkanGl.GetUniformLocation(program,name);
        public void Uniform1(int location,int value) => VulkanGl.Uniform1(location,value);
        public void Uniform1(int location,float value) => VulkanGl.Uniform1(location,value);
        public void Uniform1(int location,int count,float[] value) => VulkanGl.Uniform1(location,count,value);
        public void Uniform3(int location,Vector3 value) => VulkanGl.Uniform3(location,value);
        public void Uniform3(int location,int count,float[] value) => VulkanGl.Uniform3(location,count,value);
        public void Uniform4(int location,Vector4 value) => VulkanGl.Uniform4(location,value);
        public void Uniform4(int location,ref Vector4 value) => VulkanGl.Uniform4(location,ref value);
        public void Uniform4(int location,float x,float y,float z,float w) => VulkanGl.Uniform4(location,x,y,z,w);
        public void Uniform4(int location,int x,int y,int z,int w) => VulkanGl.Uniform4(location,x,y,z,w);
        public void UniformMatrix4(int location,bool transpose,ref Matrix4 matrix) => VulkanGl.UniformMatrix4(location,transpose,ref matrix);
        public void UniformMatrix4(int location,int count,bool transpose,float[] value) => VulkanGl.UniformMatrix4(location,count,transpose,value);
        public void Enable(EnableCap cap) => VulkanGl.Enable(cap);
        public void Disable(EnableCap cap) => VulkanGl.Disable(cap);
        public void AlphaFunc(AlphaFunction func,float reference) => VulkanGl.AlphaFunc(func,reference);
        public void PolygonMode(TriangleFace face,OpenTK.Graphics.OpenGL.PolygonMode mode) => VulkanGl.PolygonMode(face,mode);
        public void LineWidth(float width) => VulkanGl.LineWidth(width);
        public void DebugMessageCallback(DebugProc callback,IntPtr userParam) => VulkanGl.DebugMessageCallback(callback,userParam);
        public void Clear(ClearBufferMask mask) => VulkanGl.Clear(mask);
        public void ClearColor(Color4 color) => VulkanGl.ClearColor(color);
        public void ClearColor(float r,float g,float b,float a) => VulkanGl.ClearColor(r,g,b,a);
        public void ClearStencil(int value) => VulkanGl.ClearStencil(value);
        public void ColorMask(bool r,bool g,bool b,bool a) => VulkanGl.ColorMask(r,g,b,a);
        public void DepthMask(bool value) => VulkanGl.DepthMask(value);
        public void DepthFunc(DepthFunction func) => VulkanGl.DepthFunc(func);
        public void CullFace(TriangleFace face) => VulkanGl.CullFace(face);
        public void BlendFunc(BlendingFactor src,BlendingFactor dst) => VulkanGl.BlendFunc(src,dst);
        public void StencilFunc(StencilFunction func,int reference,int mask) => VulkanGl.StencilFunc(func,reference,mask);
        public void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,OpenTK.Graphics.OpenGL.StencilOp zfail,OpenTK.Graphics.OpenGL.StencilOp zpass) => VulkanGl.StencilOp(fail,zfail,zpass);
        public void StencilMask(int mask) => VulkanGl.StencilMask(mask);
        public void PolygonOffset(float factor,float units) => VulkanGl.PolygonOffset(factor,units);
        public void Viewport(int x,int y,int width,int height) => VulkanGl.Viewport(x,y,width,height);
        public void Scissor(int x,int y,int width,int height) => VulkanGl.Scissor(x,y,width,height);
        public void PixelStore(PixelStoreParameter pname,int param) => VulkanGl.PixelStore(pname,param);
        public void DrawBuffer(DrawBufferMode mode) => VulkanGl.DrawBuffer(mode);
        public void ReadBuffer(ReadBufferMode mode) => VulkanGl.ReadBuffer(mode);
        public ErrorCode GetError() => VulkanGl.GetError();
        public string GetString(StringName name) => VulkanGl.GetString(name);
        public int GetInteger(GetPName pname) => VulkanGl.GetInteger(pname);
        public int GenFramebuffer() => VulkanGl.GenFramebuffer();
        public void BindFramebuffer(FramebufferTarget target,int framebuffer) => VulkanGl.BindFramebuffer(target,framebuffer);
        public void FramebufferTexture2D(FramebufferTarget target,FramebufferAttachment attachment,TextureTarget textarget,int texture,int level) => VulkanGl.FramebufferTexture2D(target,attachment,textarget,texture,level);
        public int GenRenderbuffer() => VulkanGl.GenRenderbuffer();
        public void BindRenderbuffer(RenderbufferTarget target,int renderbuffer) => VulkanGl.BindRenderbuffer(target,renderbuffer);
        public void RenderbufferStorage(RenderbufferTarget target,OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat,int width,int height) => VulkanGl.RenderbufferStorage(target,internalFormat,width,height);
        public void FramebufferRenderbuffer(FramebufferTarget target,FramebufferAttachment attachment,RenderbufferTarget renderbufferTarget,int renderbuffer) => VulkanGl.FramebufferRenderbuffer(target,attachment,renderbufferTarget,renderbuffer);
        public void GetFramebufferAttachmentParameter(FramebufferTarget target,FramebufferAttachment attachment,FramebufferParameterName pname,out int result) => VulkanGl.GetFramebufferAttachmentParameter(target,attachment,pname,out result);
        public void DeleteFramebuffer(int framebuffer) => VulkanGl.DeleteFramebuffer(framebuffer);
        public void DeleteRenderbuffer(int renderbuffer) => VulkanGl.DeleteRenderbuffer(renderbuffer);
        public FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target) => VulkanGl.CheckFramebufferStatus(target);
        public void MatrixMode(MatrixMode mode) => VulkanGl.MatrixMode(mode);
        public void PushMatrix() => VulkanGl.PushMatrix();
        public void PopMatrix() => VulkanGl.PopMatrix();
        public void LoadIdentity() => VulkanGl.LoadIdentity();
        public void TexEnv(TextureEnvTarget target,TextureEnvParameter pname,int param) => VulkanGl.TexEnv(target,pname,param);
    }
}
#endif

#if !ANDROID
using System;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// Complete command contract consumed by the current renderer.
    ///
    /// The renderer above this interface does not choose a graphics API.
    /// OpenGL, Vulkan, and future Direct3D 12 / Metal implementations translate
    /// this contract independently. Keeping the translation behind one backend
    /// object prevents API-specific branches from spreading through scene,
    /// HUD, capture, launcher, and diagnostics code.
    /// </summary>
    internal interface IRenderBackend
    {
        RendererBackendKind Kind { get; }
        string DisplayName { get; }
        RenderWindowApi WindowApi { get; }
        bool OwnsPresentation { get; }

        bool IsSupportedOnCurrentPlatform();
        bool IsRuntimeAvailable();

        void Initialize(RenderWindow window);
        void Shutdown();
        void Present();
        void SetVSync(bool enabled);
        void Resize(int width, int height);

        void Begin(PrimitiveType mode);
        void End();
        void Vertex2(float x,float y);
        void Vertex3(float x,float y,float z);
        void Vertex3(Vector3 v);
        void Color3(float r,float g,float b);
        void Color3(Vector3 v);
        void Color4(float r,float g,float b,float a);
        void Normal3(float x,float y,float z);
        void TexCoord2(float s,float t);
        void TexCoord3(float s,float t,float r);
        void TexCoord3(Vector3 v);
        void MultiTexCoord2(TextureUnit unit,float s,float t);
        int GenLists(int range);
        void NewList(int list,ListMode mode);
        void EndList();
        void CallList(int list);
        void DeleteLists(int list,int range);
        int GenTexture();
        void DeleteTexture(int name);
        void ActiveTexture(TextureUnit unit);
        void BindTexture(TextureTarget target,int name);
        void TexParameter(TextureTarget target,TextureParameterName pname,int param);
        void TexImage2D(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,IntPtr pixels);
        void TexImage2D<T>(TextureTarget target,int level,PixelInternalFormat internalFormat,int width,int height,int border,PixelFormat format,PixelType type,T[] pixels) where T:struct;
        void TexSubImage2D<T>(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct;
        void TexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int width,int height,PixelFormat format,PixelType type,IntPtr pixels);
        void CopyTexSubImage2D(TextureTarget target,int level,int xoffset,int yoffset,int x,int y,int width,int height);
        void ReadPixels<T>(int x,int y,int width,int height,PixelFormat format,PixelType type,T[] pixels) where T:struct;
        int CreateShader(ShaderType type);
        void ShaderSource(int shader,string source);
        void CompileShader(int shader);
        void GetShader(int shader,ShaderParameter pname,out int value);
        string GetShaderInfoLog(int shader);
        void DeleteShader(int shader);
        int CreateProgram();
        void AttachShader(int program,int shader);
        void DetachShader(int program,int shader);
        void LinkProgram(int program);
        void GetProgram(int program,GetProgramParameterName pname,out int value);
        string GetProgramInfoLog(int program);
        void DeleteProgram(int program);
        void UseProgram(int program);
        int GetUniformLocation(int program,string name);
        void Uniform1(int location,int value);
        void Uniform1(int location,float value);
        void Uniform1(int location,int count,float[] value);
        void Uniform3(int location,Vector3 value);
        void Uniform3(int location,int count,float[] value);
        void Uniform4(int location,Vector4 value);
        void Uniform4(int location,ref Vector4 value);
        void Uniform4(int location,float x,float y,float z,float w);
        void Uniform4(int location,int x,int y,int z,int w);
        void UniformMatrix4(int location,bool transpose,ref Matrix4 matrix);
        void UniformMatrix4(int location,int count,bool transpose,float[] value);
        void Enable(EnableCap cap);
        void Disable(EnableCap cap);
        void AlphaFunc(AlphaFunction func,float reference);
        void PolygonMode(TriangleFace face,OpenTK.Graphics.OpenGL.PolygonMode mode);
        void LineWidth(float width);
        void DebugMessageCallback(DebugProc callback,IntPtr userParam);
        void Clear(ClearBufferMask mask);
        void ClearColor(Color4 color);
        void ClearColor(float r,float g,float b,float a);
        void ClearStencil(int value);
        void ColorMask(bool r,bool g,bool b,bool a);
        void DepthMask(bool value);
        void DepthFunc(DepthFunction func);
        void CullFace(TriangleFace face);
        void BlendFunc(BlendingFactor src,BlendingFactor dst);
        void StencilFunc(StencilFunction func,int reference,int mask);
        void StencilOp(OpenTK.Graphics.OpenGL.StencilOp fail,OpenTK.Graphics.OpenGL.StencilOp zfail,OpenTK.Graphics.OpenGL.StencilOp zpass);
        void StencilMask(int mask);
        void PolygonOffset(float factor,float units);
        void Viewport(int x,int y,int width,int height);
        void Scissor(int x,int y,int width,int height);
        void PixelStore(PixelStoreParameter pname,int param);
        void DrawBuffer(DrawBufferMode mode);
        void ReadBuffer(ReadBufferMode mode);
        ErrorCode GetError();
        string GetString(StringName name);
        int GetInteger(GetPName pname);
        int GenFramebuffer();
        void BindFramebuffer(FramebufferTarget target,int framebuffer);
        void FramebufferTexture2D(FramebufferTarget target,FramebufferAttachment attachment,TextureTarget textarget,int texture,int level);
        int GenRenderbuffer();
        void BindRenderbuffer(RenderbufferTarget target,int renderbuffer);
        void RenderbufferStorage(RenderbufferTarget target,OpenTK.Graphics.OpenGL.RenderbufferStorage internalFormat,int width,int height);
        void FramebufferRenderbuffer(FramebufferTarget target,FramebufferAttachment attachment,RenderbufferTarget renderbufferTarget,int renderbuffer);
        void GetFramebufferAttachmentParameter(FramebufferTarget target,FramebufferAttachment attachment,FramebufferParameterName pname,out int result);
        void DeleteFramebuffer(int framebuffer);
        void DeleteRenderbuffer(int renderbuffer);
        FramebufferErrorCode CheckFramebufferStatus(FramebufferTarget target);
        void MatrixMode(MatrixMode mode);
        void PushMatrix();
        void PopMatrix();
        void LoadIdentity();
        void TexEnv(TextureEnvTarget target,TextureEnvParameter pname,int param);
    }
}
#endif

using System;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// Creates the native desktop window from the already-selected renderer's
    /// window requirements. It deliberately knows nothing about Vulkan,
    /// Direct3D, Metal, loaders, devices, shaders, or command translation.
    /// </summary>
    internal static class DesktopRenderWindow
    {
        public static void PreserveWorkingDirectory()
        {
            // GLFW otherwise changes a bundled Mac app to Contents/Resources,
            // separating launcher validation/settings from the extraction child.
            if (OperatingSystem.IsMacOS())
                GLFW.InitHint(InitHintBool.CocoaChdirResources, false);
        }

        public static NativeWindowSettings Settings(bool background = false)
        {
            GLFWProvider.SetErrorCallback((code, description) =>
                Console.Error.WriteLine($"[window] GLFW {code}: {description}"));
            PreserveWorkingDirectory();
            if (background && OperatingSystem.IsMacOS())
                GLFW.InitHint(InitHintBool.CocoaMenubar, false);

            if (!RendererBackend.WindowCreated)
            {
                RendererBackend.Configure(GameState.LoadSettings().Renderer);
            }

#if ANDROID
            RenderWindowApi windowApi = RenderWindowApi.OpenGL;
#else
            RendererBackend.LockForWindow();
            RenderWindowApi windowApi = RendererBackend.WindowApi;
#endif

            bool noApi = windowApi == RenderWindowApi.NoApi;
            return new NativeWindowSettings
            {
                ClientSize = new Vector2i(1280, 768),
                Title = Branding.Name,
                API = noApi ? ContextAPI.NoAPI : ContextAPI.OpenGL,
                AutoLoadBindings = !noApi,
                // Legacy immediate mode/GLSL 1.20 need the 2.1 context on macOS;
                // Apple's 3.2+ contexts are core-only.
                Profile = OperatingSystem.IsMacOS() ? ContextProfile.Any : ContextProfile.Compatability,
                Flags = ContextFlags.Default,
                APIVersion = OperatingSystem.IsMacOS() ? new Version(2, 1) : new Version(3, 2),
                StartVisible = false
            };
        }
    }
}

using System;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;

namespace MphRead.Mods.Render
{
    internal static class DesktopGlContext
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
            // Install before NativeWindowSettings initializes GLFW/monitors.
            // OpenTK's default handler throws across native frames and aborts
            // on macOS; NativeWindow checks a failed CreateWindow itself and
            // throws safely after returning to managed code.
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
            RendererBackendKind backend = RendererBackendKind.OpenGL;
#else
            bool vulkanAvailable = Veldrid.GraphicsDevice.IsBackendSupported(Veldrid.GraphicsBackend.Vulkan);
            RendererBackendKind backend = RendererBackend.LockForWindow(vulkanAvailable);
#endif
            return new NativeWindowSettings
            {
                ClientSize = new Vector2i(1280, 768),
                Title = Branding.Name,
                API = backend == RendererBackendKind.Vulkan ? ContextAPI.NoAPI : ContextAPI.OpenGL,
                AutoLoadBindings = backend != RendererBackendKind.Vulkan,
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

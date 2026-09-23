using System;

namespace MphRead.Mods.Render
{
    public enum RendererBackendKind
    {
        OpenGL,
        Vulkan
    }

    public static class RendererBackend
    {
        public static readonly string[] Names = { "OpenGL", "Vulkan" };

        public static RendererBackendKind Requested { get; private set; } = RendererBackendKind.OpenGL;
        public static RendererBackendKind Active { get; private set; } = RendererBackendKind.OpenGL;
        public static bool WindowCreated { get; private set; }

        public static bool UseVulkan => WindowCreated && Active == RendererBackendKind.Vulkan;

        public static void Configure(string? name)
        {
            if (OperatingSystem.IsAndroid())
            {
                Requested = RendererBackendKind.OpenGL;
                return;
            }
            Requested = String.Equals(name, "Vulkan", StringComparison.OrdinalIgnoreCase)
                ? RendererBackendKind.Vulkan
                : RendererBackendKind.OpenGL;
        }

        public static RendererBackendKind LockForWindow(bool vulkanAvailable)
        {
            if (WindowCreated)
            {
                return Active;
            }
            Active = Requested == RendererBackendKind.Vulkan && vulkanAvailable
                ? RendererBackendKind.Vulkan
                : RendererBackendKind.OpenGL;
            WindowCreated = true;
            if (Requested == RendererBackendKind.Vulkan && Active != RendererBackendKind.Vulkan)
            {
                DebugLog.Line("render", "Vulkan was requested but no Vulkan loader/device is available; using OpenGL");
            }
            return Active;
        }

        public static string SaveName => Requested == RendererBackendKind.Vulkan ? "Vulkan" : "OpenGL";
        public static int RequestedIndex => Requested == RendererBackendKind.Vulkan ? 1 : 0;
        public static bool RestartRequired => WindowCreated && Requested != Active;
    }
}

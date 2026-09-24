#if !ANDROID
using System;
using System.Collections.Generic;
using System.Linq;

namespace MphRead.Mods.Render
{
    [Flags]
    internal enum RenderPlatform
    {
        None = 0,
        Windows = 1,
        Linux = 2,
        MacOS = 4
    }

    internal sealed class RenderBackendRegistration
    {
        public RendererBackendKind Kind { get; }
        public string DisplayName { get; }
        public RenderPlatform Platforms { get; }
        public RenderWindowApi WindowApi { get; }
        public IRenderBackend? Implementation { get; }

        public bool Implemented => Implementation != null;

        public RenderBackendRegistration(RendererBackendKind kind, string displayName,
            RenderPlatform platforms, RenderWindowApi windowApi, IRenderBackend? implementation)
        {
            Kind = kind;
            DisplayName = displayName;
            Platforms = platforms;
            WindowApi = windowApi;
            Implementation = implementation;
        }

        public bool SupportsCurrentPlatform()
        {
            RenderPlatform current = RenderBackendRegistry.CurrentPlatform;
            return current != RenderPlatform.None && (Platforms & current) != 0;
        }
    }

    /// <summary>
    /// The only place that knows which renderer implementations exist and
    /// which operating systems they belong to.
    ///
    /// Adding Direct3D 12 or Metal is intentionally a registration task:
    /// implement IRenderBackend, replace the null registration below, and the
    /// settings/selection/window code does not change.
    /// </summary>
    internal static class RenderBackendRegistry
    {
        private static readonly OpenGlRenderBackend _openGl = new();
        private static readonly VulkanRenderBackend _vulkan = new();

        private static readonly RenderBackendRegistration[] _all =
        {
            new(RendererBackendKind.OpenGL, "OpenGL",
                RenderPlatform.Windows | RenderPlatform.Linux | RenderPlatform.MacOS,
                RenderWindowApi.OpenGL, _openGl),
            new(RendererBackendKind.Vulkan, "Vulkan",
                RenderPlatform.Windows | RenderPlatform.Linux | RenderPlatform.MacOS,
                RenderWindowApi.NoApi, _vulkan),

            // Reserved registrations document the intended platform matrix
            // without exposing unfinished choices to players.
            new(RendererBackendKind.Direct3D12, "Direct3D 12",
                RenderPlatform.Windows, RenderWindowApi.NoApi, null),
            new(RendererBackendKind.Metal, "Metal",
                RenderPlatform.MacOS, RenderWindowApi.NoApi, null)
        };

        public static RenderPlatform CurrentPlatform =>
            OperatingSystem.IsWindows() ? RenderPlatform.Windows
            : OperatingSystem.IsLinux() ? RenderPlatform.Linux
            : OperatingSystem.IsMacOS() ? RenderPlatform.MacOS
            : RenderPlatform.None;

        public static IReadOnlyList<RenderBackendRegistration> All => _all;

        public static RenderBackendRegistration OpenGl =>
            _all.First(entry => entry.Kind == RendererBackendKind.OpenGL);

        public static IEnumerable<RenderBackendRegistration> Selectable =>
            _all.Where(entry => entry.Implemented
                && entry.SupportsCurrentPlatform()
                && entry.Implementation!.IsSupportedOnCurrentPlatform());

        public static string[] SelectableNames =>
            Selectable.Select(entry => entry.DisplayName).ToArray();

        public static RenderBackendRegistration? Find(RendererBackendKind kind) =>
            _all.FirstOrDefault(entry => entry.Kind == kind);

        public static RenderBackendRegistration? Find(string? displayName) =>
            _all.FirstOrDefault(entry =>
                String.Equals(entry.DisplayName, displayName, StringComparison.OrdinalIgnoreCase));

        public static bool CanActivate(RenderBackendRegistration registration) =>
            registration.Implemented
            && registration.SupportsCurrentPlatform()
            && registration.Implementation!.IsSupportedOnCurrentPlatform()
            && registration.Implementation.IsRuntimeAvailable();
    }
}
#endif

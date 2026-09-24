using System;
#if !ANDROID
using System.Linq;
#endif

namespace MphRead.Mods.Render
{
    public enum RendererBackendKind
    {
        OpenGL,
        Vulkan,
        Direct3D12,
        Metal
    }

    internal enum RenderWindowApi
    {
        OpenGL,
        NoApi
    }

    /// <summary>
    /// Stores renderer selection state. It does not know how any graphics API
    /// works; implementation/platform/runtime policy is delegated to
    /// RenderBackendRegistry and IRenderBackend implementations.
    /// </summary>
    public static class RendererBackend
    {
        public static RendererBackendKind Requested { get; private set; } = RendererBackendKind.OpenGL;
        public static RendererBackendKind Active { get; private set; } = RendererBackendKind.OpenGL;
        public static bool WindowCreated { get; private set; }
        private static RendererBackendKind? _forced;

#if !ANDROID
        private static IRenderBackend? _activeImplementation;
        internal static IRenderBackend ActiveImplementation =>
            _activeImplementation
            ?? throw new InvalidOperationException("Renderer backend has not been locked for a window.");
#endif

        public static string[] Names
        {
            get
            {
#if ANDROID
                return new[] { "OpenGL" };
#else
                return RenderBackendRegistry.SelectableNames;
#endif
            }
        }

        public static bool IsActive(RendererBackendKind kind) =>
            WindowCreated && Active == kind;

        public static bool OwnsPresentation
        {
            get
            {
#if ANDROID
                return false;
#else
                return WindowCreated && ActiveImplementation.OwnsPresentation;
#endif
            }
        }

        public static void Configure(string? name)
        {
            if (_forced.HasValue)
            {
                Requested = _forced.Value;
                return;
            }
#if ANDROID
            Requested = RendererBackendKind.OpenGL;
#else
            Requested = RenderBackendRegistry.Find(name)?.Kind ?? RendererBackendKind.OpenGL;
#endif
        }

        internal static void ForceForProcess(RendererBackendKind backend)
        {
            if (WindowCreated && Active != backend)
            {
                throw new InvalidOperationException("The renderer is already locked for this process.");
            }
            _forced = backend;
            Requested = backend;
        }

#if !ANDROID
        public static RendererBackendKind LockForWindow()
        {
            if (WindowCreated)
            {
                return Active;
            }

            RenderBackendRegistration requested = RenderBackendRegistry.Find(Requested)
                ?? throw new InvalidOperationException(
                    $"Unknown graphics renderer '{Requested}'.");

            if (!RenderBackendRegistry.CanActivate(requested)
                || requested.Implementation == null)
            {
                throw new PlatformNotSupportedException(
                    $"The selected graphics renderer '{requested.DisplayName}' "
                    + "is not available on this platform or runtime.");
            }

            Active = requested.Kind;
            _activeImplementation = requested.Implementation;
            WindowCreated = true;
            DebugLog.Line("render", $"using renderer {requested.DisplayName}");
            return Active;
        }

        internal static RenderWindowApi WindowApi
        {
            get
            {
                if (!WindowCreated)
                {
                    throw new InvalidOperationException("Renderer backend has not been locked.");
                }
                return RenderBackendRegistry.Find(Active)?.WindowApi ?? RenderWindowApi.OpenGL;
            }
        }
#endif

        public static string SaveName
        {
            get
            {
#if ANDROID
                return "OpenGL";
#else
                return RenderBackendRegistry.Find(Requested)?.DisplayName ?? "OpenGL";
#endif
            }
        }

        public static int RequestedIndex
        {
            get
            {
                string[] names = Names;
                string name = SaveName;
                int index = Array.FindIndex(names,
                    value => String.Equals(value, name, StringComparison.OrdinalIgnoreCase));
                return index >= 0 ? index : 0;
            }
        }

        public static bool RestartRequired => WindowCreated && Requested != Active;
    }
}

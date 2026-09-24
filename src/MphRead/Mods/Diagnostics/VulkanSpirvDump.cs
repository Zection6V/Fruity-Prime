#if MPHREAD_SHELL
using System;
using Veldrid;
using Veldrid.SPIRV;
using MphRead.Mods.Render;

namespace MphRead.Mods.Diagnostics
{
    /// <summary>
    /// Temporary migration aid: compile the Vulkan GLSL once on a platform
    /// that ships Veldrid.SPIRV's native compiler, so the resulting SPIR-V can
    /// be committed and runtime shader compilation can be removed.
    /// </summary>
    internal static class VulkanSpirvDump
    {
        public static int Run()
        {
            var shaders = new (string Name, string Source, ShaderStages Stage)[]
            {
                ("SceneVertex", VulkanShaders.SceneVertex, ShaderStages.Vertex),
                ("SceneFragment", VulkanShaders.SceneFragment, ShaderStages.Fragment),
                ("ScreenVertex", VulkanShaders.ScreenVertex, ShaderStages.Vertex),
                ("ScreenFragment", VulkanShaders.ScreenFragment, ShaderStages.Fragment),
                ("RttFragment", VulkanShaders.RttFragment, ShaderStages.Fragment),
                ("BackdropFragment", VulkanShaders.BackdropFragment, ShaderStages.Fragment),
                ("ShiftFragment", VulkanShaders.ShiftFragment, ShaderStages.Fragment),
                ("CelFragment", VulkanShaders.CelFragment, ShaderStages.Fragment),
                ("ClearVertex", VulkanShaders.ClearVertex, ShaderStages.Vertex),
                ("ClearFragment", VulkanShaders.ClearFragment, ShaderStages.Fragment)
            };

            foreach (var shader in shaders)
            {
                SpirvCompilationResult compiled = SpirvCompilation.CompileGlslToSpirv(
                    shader.Source, shader.Name, shader.Stage, GlslCompileOptions.Default);
                Console.WriteLine($"FRUITY_SPV:{shader.Name}:{Convert.ToBase64String(compiled.SpirvBytes)}");
            }
            return 0;
        }
    }
}
#endif

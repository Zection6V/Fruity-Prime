#include "PreSetupDiagnostics.hpp"

#include "CommandLine.hpp"
#include "LaunchPath.hpp"

#include "../../NativeRuntime/Rhi/SceneBackend.hpp"
#include "../../NativeRuntime/Rhi/Vulkan/VulkanContext.hpp"
#include "../../NativeRuntime/Rhi/Vulkan/VulkanSwapchain.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Utility/Console.hpp"
#include "../Diagnostics/BackdropParityCheck.hpp"
#include "../Diagnostics/CompatibilityCheck.hpp"
#include "../Diagnostics/PresentConformanceCheck.hpp"
#include "../Diagnostics/RhiConformanceCheck.hpp"
#if defined(MPHREAD_SHELL)
#include "../Diagnostics/GlfwPathCheck.hpp"
#include "../Diagnostics/LauncherWindowCheck.hpp"
#include "../Diagnostics/ThumbnailWindowCheck.hpp"
#endif

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>

namespace
{
    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }
}

namespace MphRead::Mods::Entry
{
    bool TryRunPreSetupDiagnostics(const std::vector<std::string>& args)
    {
        if (const std::optional<std::string> contract = ValueAfter(args, "rhicontract"); contract.has_value())
        {
            try
            {
                // Relative to where the command was typed: startup has moved the
                // working directory to the installation by now.
                const std::string path = FullPathCombine(ConsoleSetup::LaunchDirectory(), *contract);
                std::ofstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary | std::ios::trunc);
                file << ::MphRead::NativeRuntime::Rhi::SceneBackendContract();
                SetExitCode(file.good() ? 0 : 1);
            }
            catch (...)
            {
                SetExitCode(1);
            }
            return true;
        }
        if (HasFlag(args, "vulkancheck"))
        {
            SetExitCode(::MphRead::NativeRuntime::Rhi::Vulkan::RunFoundationCheck());
            return true;
        }
        if (const auto directory = ValueAfter(args, "backdropparity"); directory.has_value())
        {
            SetExitCode(Diagnostics::RunBackdropParityCheck(
                FullPathCombine(ConsoleSetup::LaunchDirectory(), *directory), HasFlag(args, "backdropobserve")));
            return true;
        }
        if (HasFlag(args, "presentconformance"))
        {
            SetExitCode(Diagnostics::RunPresentConformanceCheck());
            return true;
        }
        if (HasFlag(args, "rhiconformance"))
        {
            SetExitCode(Diagnostics::RunRhiConformanceCheck());
            return true;
        }
        if (HasFlag(args, "reflexcheck"))
        {
            SetExitCode(::MphRead::NativeRuntime::Rhi::Vulkan::RunPresentationCheck(false, true));
            return true;
        }
        if (HasFlag(args, "vulkanpresentcheck"))
        {
            SetExitCode(::MphRead::NativeRuntime::Rhi::Vulkan::RunPresentationCheck());
            return true;
        }
        if (HasFlag(args, "vulkanresourcecheck"))
        {
            SetExitCode(::MphRead::NativeRuntime::Rhi::Vulkan::RunResourceCheck());
            return true;
        }
        if (HasFlag(args, "vulkanpresentfallbackcheck"))
        {
            SetExitCode(::MphRead::NativeRuntime::Rhi::Vulkan::RunPresentationCheck(true));
            return true;
        }
#if defined(MPHREAD_SHELL)
        if (HasFlag(args, "glfwpathcheck"))
        {
            SetExitCode(Diagnostics::GlfwPathCheck::Run());
            return true;
        }
        if (HasFlag(args, "thumbnailwindowcheck"))
        {
            SetExitCode(Diagnostics::ThumbnailWindowCheck::Run(HasFlag(args, "legacyglcheck")));
            return true;
        }
        if (HasFlag(args, "windowcheck"))
        {
            SetExitCode(Diagnostics::LauncherWindowCheck::Run());
            return true;
        }
#endif
        if (HasFlag(args, "smoketest"))
        {
            SetExitCode(Diagnostics::CompatibilityCheck::Run());
            return true;
        }
        return false;
    }
}

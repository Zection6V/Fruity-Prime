#include "../Mods/Network/Testing/Maps/MapAuditCleanup.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct CleanupState final
    {
        std::vector<std::string> Operations{};
        std::vector<std::string> Failures{};
    };

    using MphRead::Mods::Network::MapAuditCleanupStep;
    using MphRead::Mods::Network::RunMapAuditCleanup;

    void SceneStop(CleanupState& state)
    {
        state.Operations.emplace_back("scene-stop");
        throw std::runtime_error("scene stop failed");
    }

    void GpuRelease(CleanupState& state)
    {
        state.Operations.emplace_back("gpu-release");
        throw std::runtime_error("gpu release failed");
    }

    void SceneDestroy(CleanupState& state)
    {
        state.Operations.emplace_back("scene-destroy");
    }

    void ReportFailure(CleanupState& state, std::string_view operation, std::exception_ptr)
    {
        state.Failures.emplace_back(operation);
        if (operation == "scene-stop")
        {
            throw std::runtime_error("diagnostic sink failed");
        }
    }

    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void CheckAllStagesRunAndFirstFailureSurvives()
    {
        CleanupState state{};
        const std::array steps{
            MapAuditCleanupStep<CleanupState>{"scene-stop", SceneStop},
            MapAuditCleanupStep<CleanupState>{"gpu-release", GpuRelease},
            MapAuditCleanupStep<CleanupState>{"scene-destroy", SceneDestroy}
        };

        const std::exception_ptr failure = RunMapAuditCleanup(
            steps, state, ReportFailure);

        Expect(state.Operations == std::vector<std::string>{
                "scene-stop", "gpu-release", "scene-destroy"},
            "A failing cleanup stage prevented a later stage from running.");
        Expect(state.Failures == std::vector<std::string>{"scene-stop", "gpu-release"},
            "Cleanup failures were not reported by operation.");
        Expect(static_cast<bool>(failure), "Cleanup did not return its first failure.");
        try
        {
            std::rethrow_exception(failure);
        }
        catch (const std::runtime_error& error)
        {
            Expect(std::string_view(error.what()) == "scene stop failed",
                "Cleanup did not preserve the first exception.");
        }
    }

    void CheckSuccessfulStagesReturnNoFailure()
    {
        CleanupState state{};
        const std::array steps{
            MapAuditCleanupStep<CleanupState>{"scene-destroy", SceneDestroy}
        };
        const std::exception_ptr failure = RunMapAuditCleanup(
            steps, state, MphRead::Mods::Network::MapAuditCleanupFailureHandler<CleanupState>{});
        Expect(!failure, "Successful cleanup returned an exception.");
        Expect(state.Operations == std::vector<std::string>{"scene-destroy"},
            "Successful cleanup did not run its stage.");
    }
}

int main()
{
    try
    {
        CheckAllStagesRunAndFirstFailureSurvives();
        CheckSuccessfulStagesReturnNoFailure();
        std::cout << "MapAuditCleanup PASS; later stages run after failures; first failure preserved\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MapAuditCleanup FAIL: " << error.what() << '\n';
        return 1;
    }
}

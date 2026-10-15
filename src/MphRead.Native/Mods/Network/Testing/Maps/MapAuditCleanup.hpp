#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <string_view>

namespace MphRead::Mods::Network
{
    template <typename TContext>
    struct MapAuditCleanupStep final
    {
        std::string_view Operation;
        void (*Action)(TContext& context);
    };

    template <typename TContext>
    using MapAuditCleanupFailureHandler = void (*)(
        TContext& context, std::string_view operation, std::exception_ptr failure);

    // Runs every cleanup stage, reports each failure, and returns the first
    // exception so the caller can rethrow it after releasing outer resources.
    template <typename TContext, std::size_t StepCount>
    [[nodiscard]] std::exception_ptr RunMapAuditCleanup(
        const std::array<MapAuditCleanupStep<TContext>, StepCount>& steps,
        TContext& context,
        MapAuditCleanupFailureHandler<TContext> failureHandler) noexcept
    {
        std::exception_ptr firstFailure{};
        for (const MapAuditCleanupStep<TContext>& step : steps)
        {
            if (step.Action == nullptr)
            {
                continue;
            }
            try
            {
                step.Action(context);
            }
            catch (...)
            {
                const std::exception_ptr failure = std::current_exception();
                if (!firstFailure)
                {
                    firstFailure = failure;
                }
                if (failureHandler != nullptr)
                {
                    try
                    {
                        failureHandler(context, step.Operation, failure);
                    }
                    catch (...)
                    {
                        // Reporting must not prevent later cleanup stages.
                    }
                }
            }
        }
        return firstFailure;
    }
}

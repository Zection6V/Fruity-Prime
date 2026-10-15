#include "RenderOverrides.hpp"

#include "CommandLine.hpp"

#include "../../Features.hpp"
#include "../../NativeRuntime/Rhi/SceneBackend.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../Diagnostics/FramePerformance.hpp"
#include "../Render/Crosshair.hpp"
#include "../Render/FrameTiming.hpp"
#include "../Render/Radar.hpp"
#include "../RenderOptions.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;

namespace MphRead::Mods::Entry
{
    void ApplyRenderOverrides(const std::vector<std::string>& args)
    {
        using MphRead::Features;
        using MphRead::Mods::RenderOptions;
        using MphRead::Mods::Render::Crosshair;
        using MphRead::Mods::Render::FrameTiming;

        // -rhi vulkan: the scene draws through the Vulkan backend, offscreen,
        // into targets the captures read back. -vkvalidation adds the layers.
        const std::optional<std::string> rhi = ValueAfter(args, "rhi");
        ::MphRead::NativeRuntime::Rhi::SceneBackendRequest backend{};
        if (rhi.has_value() && ::MphRead::NativeRuntime::Rhi::ParseSceneBackendRequest(*rhi, backend))
        {
            ::MphRead::NativeRuntime::Rhi::RequestSceneBackend(backend, true);
            std::cout << "[render] scene backend requested: "
                << ::MphRead::NativeRuntime::Rhi::SceneBackendRequestName(backend) << std::endl;
        }
        if (HasFlag(args, "vkvalidation")) ::MphRead::NativeRuntime::Rhi::SetSceneValidation(true);

        const std::optional<std::string> cel = ValueAfter(args, "cel");
        if (cel.has_value() && !StartsWithHyphen(cel))
        {
            RenderOptions::CelShading(
                RenderOptions::ParseOnOff(*cel, RenderOptions::CelShading()));
        }
        else if (HasFlag(args, "cel"))
        {
            RenderOptions::CelShading(true);
        }

        const std::optional<std::string> performance = ValueAfter(args, "performance");
        if (performance.has_value() && !StartsWithHyphen(performance))
        {
            RenderOptions::PerformanceMode(
                RenderOptions::ParseOnOff(*performance, RenderOptions::PerformanceMode()));
        }
        else if (HasFlag(args, "performance"))
        {
            RenderOptions::PerformanceMode(true);
        }

        const std::optional<std::string> fog = ValueAfter(args, "fog");
        if (fog.has_value() && !StartsWithHyphen(fog))
        {
            RenderOptions::Fog(RenderOptions::ParseOnOff(*fog, RenderOptions::Fog()));
        }

        const std::optional<std::string> fov = ValueAfter(args, "fov");
        if (fov.has_value() && !StartsWithHyphen(fov))
        {
            RenderOptions::FieldOfView(RenderOptions::ParseFov(
                std::string_view(*fov), RenderOptions::FieldOfView()));
        }

        const std::optional<std::string> fps = ValueAfter(args, "fps");
        if (fps.has_value() && !StartsWithHyphen(fps))
        {
            RenderOptions::ShowFps(RenderOptions::ParseOnOff(*fps, RenderOptions::ShowFps()));
        }
        else if (HasFlag(args, "fps"))
        {
            RenderOptions::ShowFps(true);
        }

        const std::optional<std::string> fpsCap = ValueAfter(args, "fpscap");
        const auto measure = ValueAfter(args, "fpsmeasure");
        if (measure && !StartsWithHyphen(measure))
        {
            std::optional<int> measurementCap;
            if (fpsCap && !StartsWithHyphen(fpsCap))
                measurementCap = FrameTiming::ParseCap(*fpsCap, FrameTiming::FrameRateCap());
            MphRead::Mods::Diagnostics::FramePerformance::Configure(*measure, HasFlag(args, "gpuprofile"), measurementCap);
        }
        if (fpsCap.has_value() && !StartsWithHyphen(fpsCap))
        {
            const std::int32_t cap = FrameTiming::ParseCap(*fpsCap, FrameTiming::FrameRateCap());
            if (cap == FrameTiming::DisplayRate)
            {
                FrameTiming::ForceVSync(true);
            }
            FrameTiming::SetFrameRateCap(cap == FrameTiming::DisplayRate ? FrameTiming::Unlimited : cap);
        }
        const std::optional<std::string> vsync = ValueAfter(args, "vsync");
        if (vsync.has_value() && !StartsWithHyphen(vsync))
        {
            FrameTiming::ForceVSync(RenderOptions::ParseOnOff(*vsync, FrameTiming::VSync()));
        }

        const std::optional<std::string> bands = ValueAfter(args, "celbands");
        std::int32_t parsedBands = 0;
        if (bands.has_value() && Int32TryParseCurrentCulture(*bands, parsedBands))
        {
            RenderOptions::CelBands(parsedBands);
        }

        const std::optional<std::string> edge = ValueAfter(args, "celedge");
        if (edge.has_value())
        {
            std::string trimmed = *edge;
            while (!trimmed.empty() && trimmed.back() == '%')
            {
                trimmed.pop_back();
            }
            std::int32_t edgePercent = 0;
            if (Int32TryParseCurrentCulture(trimmed, edgePercent))
            {
                RenderOptions::CelEdge(edgePercent / 100.0f);
            }
        }

        const std::optional<std::string> proHud = ValueAfter(args, "prohud");
        if (proHud.has_value() && !StartsWithHyphen(proHud))
        {
            Features::ProHud(RenderOptions::ParseOnOff(*proHud, Features::ProHud()));
        }
        else if (HasFlag(args, "prohud"))
        {
            Features::ProHud(true);
        }

        const std::optional<std::string> weaponStyle = ValueAfter(args, "weaponstyle");
        if (weaponStyle.has_value() && !StartsWithHyphen(weaponStyle))
        {
            if (StringEqualsOrdinalIgnoreCase(*weaponStyle, "static")
                || StringEqualsOrdinalIgnoreCase(*weaponStyle, "quake"))
            {
                Features::ProHudFixedWeapon(true);
                Features::FixedWeapon(true);
            }
            else if (StringEqualsOrdinalIgnoreCase(*weaponStyle, "dynamic")
                || StringEqualsOrdinalIgnoreCase(*weaponStyle, "metroid"))
            {
                Features::ProHudFixedWeapon(false);
                Features::FixedWeapon(false);
            }
        }

        const std::optional<std::string> crosshair = ValueAfter(args, "crosshair");
        if (crosshair.has_value() && !StartsWithHyphen(crosshair))
        {
            Crosshair::Style = Crosshair::ParseStyle(*crosshair, Crosshair::Style);
        }

        const std::optional<std::string> crosshairSize = ValueAfter(args, "crosshairsize");
        if (crosshairSize.has_value() && !StartsWithHyphen(crosshairSize))
        {
            Crosshair::Size = Crosshair::ParseSize(*crosshairSize, Crosshair::Size);
        }

        const std::optional<std::string> radar = ValueAfter(args, "radar");
        if (radar.has_value() && !StartsWithHyphen(radar))
        {
            MphRead::Mods::Render::Radar::Enabled = RenderOptions::ParseOnOff(
                std::string_view(*radar), MphRead::Mods::Render::Radar::Enabled);
        }
        else if (HasFlag(args, "radar"))
        {
            MphRead::Mods::Render::Radar::Enabled = true;
        }

        const std::optional<std::string> radarBackground = ValueAfter(args, "radarbackground");
        if (radarBackground.has_value() && !StartsWithHyphen(radarBackground))
        {
            MphRead::Mods::Render::Radar::ShowBackground = RenderOptions::ParseOnOff(
                std::string_view(*radarBackground), MphRead::Mods::Render::Radar::ShowBackground);
        }

        const std::optional<std::string> radarOutlines = ValueAfter(args, "radaroutlines");
        if (radarOutlines.has_value() && !StartsWithHyphen(radarOutlines))
        {
            MphRead::Mods::Render::Radar::ShowOutlines = RenderOptions::ParseOnOff(
                std::string_view(*radarOutlines), MphRead::Mods::Render::Radar::ShowOutlines);
        }
    }
}

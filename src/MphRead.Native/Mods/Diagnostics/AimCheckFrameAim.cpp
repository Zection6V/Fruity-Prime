#include "AimCheck.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Features.hpp"
#include "../../Scene.hpp"
#include "../Input/FrameAim.hpp"
#include "../InputSettings.hpp"
#include "../Render/AnimationClock.hpp"
#include "../Render/FrameTiming.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;
        constexpr float Pi = 3.14159265358979323846F;

        float AngleDegrees(Vector3 a, Vector3 b)
        {
            return std::atan2(OpenTK::Mathematics::Length(Vector3::Cross(a, b)), Vector3::Dot(a, b)) * 180.0F / Pi;
        }

        // The direction a view matrix looks along (OpenTK row-vector LookAt).
        Vector3 Forward(const Matrix4& view)
        {
            return Vector3(-view.M13, -view.M23, -view.M33).Normalized();
        }

        // The largest angle between two views' axes: heading, pitch and roll.
        float ViewDegrees(const Matrix4& a, const Matrix4& b)
        {
            return std::max({AngleDegrees(Vector3(a.M11, a.M21, a.M31), Vector3(b.M11, b.M21, b.M31)),
                AngleDegrees(Vector3(a.M12, a.M22, a.M32), Vector3(b.M12, b.M22, b.M32)),
                AngleDegrees(Vector3(a.M13, a.M23, a.M33), Vector3(b.M13, b.M23, b.M33))});
        }

        double YawDegrees(Vector3 v)
        {
            return std::atan2(v.X, v.Z) * 180.0 / Pi;
        }

        bool Same(Vector3 a, Vector3 b)
        {
            return std::memcmp(&a, &b, sizeof(a)) == 0;
        }

        struct Series
        {
            double Sum = 0, Max = 0;
            std::size_t Count = 0;
            void Add(double v) { Sum += v; Max = std::max(Max, v); ++Count; }
            [[nodiscard]] double Mean() const { return Count ? Sum / static_cast<double>(Count) : 0; }
        };
    }

    // The display-rate mouse look (Mods/Input/FrameAim), against the
    // simulation's own aim.
    int AimCheck::CheckFrameAim(Entities::PlayerEntity& player)
    {
        int checks = 0;
        const auto check = [&](bool ok, const char* name)
        {
            if (!ok) throw std::runtime_error(name);
            ++checks; std::cout << "AIM PASS " << name << '\n';
        };
        using Keyboard = OpenTK::Windowing::GraphicsLibraryFramework::KeyboardState;
        using Mouse = OpenTK::Windowing::GraphicsLibraryFramework::MouseState;
        Keyboard keyboard;
        Mouse mouse;
        const bool fixedBefore = Features::FixedCrosshair();
        Features::FixedCrosshair(true);
        // A simulation step as the scene runs one: the step clock first.
        const auto step = [&](const Mouse& state)
        {
            Render::AnimationClock::Simulation().Step();
            Entities::PlayerEntity::ProcessInput(keyboard, state, false);
            player.ProcessBiped();
        };
        const auto reset = [&](float yaw, float pitch, bool zoom)
        {
            player.Controls().ClearAll(); player.ModForgetInputDeltas();
            player.Controls().SetNativeAim(false); player.Controls().NativeControl() = {};
            player.Controls().SetMouseAim(true); player.Controls().SetKeyboardAim(false);
            player._aimFrame = {}; player._gunVec1 = player._facingVector = Vector3(0, 0, 1);
            player._gunVec2 = Vector3(1, 0, 0); player._upVector = Vector3(0, 1, 0);
            player._aimY = 0; player._equipInfo->Zoomed = zoom;
            player._flags1 &= ~Entities::PlayerFlags1::NoAimInput;
            player._frozenTimer = 0; player._camSwitchTimer = 0xFFFF;
            player._cameraType = Entities::CameraType::First;
            player._modFrameAim = {};
            player.UpdateAimY(pitch); player.UpdateAimX(yaw);
            mouse = {}; keyboard = {};
            step(mouse);
        };

        // 1. Turning in a frame's pieces is turning in a step's whole: the
        // same mouse path ends at the same aim, and a step after the frames
        // adds nothing the frames had already applied.
        std::mt19937 rng(20261016U);
        std::uniform_real_distribution<float> pixels(-60.0F, 60.0F), yaws(-180.0F, 180.0F), pitches(-80.0F, 80.0F);
        Series pieces, clampedPieces, view, viewVsStep, nearPole, gun, stepAdds;
        int trials = 0, clamped = 0;
        bool engaged = true;
        for (int i = 0; i < 1000; ++i)
        {
            const bool zoom = i % 5 == 4;
            // Near straight up and down too, where the camera's up is least vertical.
            const float startYaw = yaws(rng);
            const float startPitch = i % 7 == 0 ? 83.0F : i % 7 == 1 ? -82.0F : i % 7 == 2 ? 78.0F : pitches(rng);
            std::vector<std::pair<float, float>> path;
            const int frames = 1 + i % 4;
            float x = 0, y = 0;
            for (int f = 0; f < frames; ++f) { x += pixels(rng); y += pixels(rng); path.emplace_back(x, y); }

            // The step alone, consuming the whole path at once.
            reset(startYaw, startPitch, zoom);
            mouse.X = path.back().first; mouse.Y = path.back().second;
            step(mouse);
            const Vector3 whole = player._gunVec1;

            // Frames along the path, then the step.
            reset(startYaw, startPitch, zoom);
            bool atLimit = std::fabs(player._aimY) >= 84.99F;
            Matrix4 drawn = player.CameraInfo()->ViewMatrix;
            for (const auto& [px, py] : path)
            {
                mouse.X = px; mouse.Y = py;
                const Vector3 before = player._gunVec1;
                player.ModApplyFrameAim(mouse);
                engaged = engaged && !Same(before, player._gunVec1);
                atLimit = atLimit || std::fabs(player._aimY) >= 84.99F;
                // What the frame shows: the camera the last step built, turned.
                drawn = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                view.Add(AngleDegrees(Forward(drawn), player._facingVector));
            }
            const Vector3 framed = player._gunVec1;
            const Vector3 gunAt = player._gunDrawPos, muzzle = player._muzzlePos;
            step(mouse);
            stepAdds.Add(AngleDegrees(framed, player._gunVec1));
            // The last frame's picture against the camera the step then builds
            // for the same aim: no heading, pitch or roll of difference.
            const float againstStep = ViewDegrees(drawn, player.CameraInfo()->ViewMatrix);
            viewVsStep.Add(againstStep);
            if (std::fabs(player._aimY) > 70.0F) nearPole.Add(againstStep);
            gun.Add(std::max<double>(OpenTK::Mathematics::Length(gunAt - player._gunDrawPos),
                OpenTK::Mathematics::Length(muzzle - player._muzzlePos)) * 100.0);
            // At the pitch limit the clamp drops motion in each piece it cuts,
            // as it does in each 60 Hz step: pieces and whole differ there.
            if (atLimit) { clampedPieces.Add(AngleDegrees(whole, player._gunVec1)); ++clamped; }
            else pieces.Add(AngleDegrees(whole, player._gunVec1));
            ++trials;
        }
        std::cout << std::fixed << std::setprecision(6) << "FRAMEAIM pieces trials=" << trials
            << " pieces_vs_whole_deg mean=" << pieces.Mean() << " max=" << pieces.Max
            << " at_pitch_limit=" << clamped << " (mean=" << clampedPieces.Mean() << " max=" << clampedPieces.Max << ")"
            << " step_after_frames_deg max=" << stepAdds.Max
            << " view_vs_aim_deg max=" << view.Max << " view_vs_step_deg max=" << viewVsStep.Max
            << " (above 70 degrees: " << nearPole.Count << " trials, max=" << nearPole.Max << ")"
            << " gun_vs_step_cm max=" << gun.Max << '\n';
        check(engaged, "frame aim turns the aim for mouse, exact, fixed crosshair, first person");
        check(stepAdds.Max < 0.0001, "a step after the frames fires along the aim the frames reached");
        check(pieces.Max < 0.01, "frames and steps end at the aim the steps alone reach");
        check(view.Max < 0.01, "the drawn view looks along the frame's aim");
        check(viewVsStep.Max < 0.01, "the drawn view is the step's camera for that aim, roll included, near the poles too");
        check(gun.Max < 0.5, "gun and muzzle are where the step puts them");

        // 1b. Strafing leans the camera across its heading (_viewTiltAngleH).
        // A frame that turns the heading must lean the way the step's camera
        // for that heading does, or turning while strafing rolls the picture
        // back and forth.
        {
            Series lean;
            for (int i = 0; i < 300; ++i)
            {
                reset(yaws(rng), pitches(rng) * 0.5F, false);
                player._viewTiltAngleH = (i % 2 == 0 ? 1.0F : -1.0F) * (5.0F + static_cast<float>(i % 30));
                player.UpdateCamera();
                mouse.X = pixels(rng) * 2.0F; mouse.Y = pixels(rng) * 0.5F;
                player.ModApplyFrameAim(mouse);
                const Matrix4 drawn = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                // The camera a step builds for the frame's aim, with the same lean.
                player.UpdateCamera();
                lean.Add(ViewDegrees(drawn, player.CameraInfo()->ViewMatrix));
            }
            player._viewTiltAngleH = 0;
            std::cout << std::setprecision(6) << "FRAMEAIM strafe lean trials=" << lean.Count
                << " view_vs_step_deg max=" << lean.Max << '\n';
            check(lean.Max < 0.01, "a strafing lean turns with the frame's heading");
        }

        // 2. Off, or where the camera is not the aim, frames change nothing.
        for (const bool off : {true, false})
        {
            reset(0, 0, false);
            if (off) Input::FrameAim::Enabled(false); else Features::FixedCrosshair(false);
            const Vector3 before = player._gunVec1;
            Mouse moved = mouse; moved.X += 50;
            player.ModApplyFrameAim(moved);
            const bool untouched = Same(before, player._gunVec1) && !player.ModFrameFacingShift().has_value();
            Input::FrameAim::Enabled(true); Features::FixedCrosshair(true);
            check(untouched, off ? "-frameaim off leaves the aim to the step" : "DS drifting crosshair leaves the aim to the step");
        }

        // 3. A turn at constant speed, read once a frame, drawn faster than the
        // simulation: how old is the turn in the picture, how even is it, and
        // how far is what is seen from where the next shot goes.
        const float degreesPerPixel = InputSettings::MouseSensitivity() / 4.0F;
        const double degreesPerSecond = 180.0;
        for (const int fps : {144, 240, 360})
        {
            double seenWithout = 0;
            for (const bool frameAim : {false, true})
            {
                reset(0, 0, false);
                Render::FrameTiming::Reset();
                Series age, seen;
                std::vector<double> increments, pendingSeen;
                double previous = 0;
                for (int frame = 1; frame <= fps * 3; ++frame)
                {
                    const double t = static_cast<double>(frame) / fps;
                    mouse.X = static_cast<float>(-degreesPerSecond * t / degreesPerPixel);
                    if (frameAim) player.ModApplyFrameAim(mouse);
                    const auto steps = Render::FrameTiming::Advance(1.0 / fps);
                    for (int s = 0; s < steps; ++s) step(mouse);
                    if (steps > 0)
                    {
                        const double shot = YawDegrees(player._gunVec1);
                        for (const double shown : pendingSeen) seen.Add(std::fabs(std::remainder(shot - shown, 360.0)));
                        pendingSeen.clear();
                    }
                    const Matrix4 shown = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                    const double yaw = YawDegrees(Forward(shown));
                    const double truth = std::remainder(degreesPerSecond * t, 360.0);
                    age.Add(std::fabs(std::remainder(truth - yaw, 360.0)) / degreesPerSecond * 1000.0);
                    if (frame > fps / 2) increments.push_back(std::remainder(yaw - previous, 360.0));
                    previous = yaw;
                    pendingSeen.push_back(yaw);
                }
                double mean = 0;
                for (const double d : increments) mean += d;
                mean /= static_cast<double>(increments.size());
                double variance = 0;
                for (const double d : increments) variance += (d - mean) * (d - mean);
                const double unevenness = std::sqrt(variance / static_cast<double>(increments.size())) / std::fabs(mean);
                std::cout << std::setprecision(3) << "FRAMEAIM schedule fps=" << fps << " frame_aim=" << (frameAim ? "on " : "off")
                    << " picture_age_ms mean=" << age.Mean() << " max=" << age.Max
                    << " per_frame_turn_cv=" << unevenness
                    << " seen_vs_next_shot_deg mean=" << seen.Mean() << " max=" << seen.Max << '\n';
                // A shot still leaves on a step; what frame aim can do is show
                // the turn the frame it is read and evenly, and so be nearer.
                if (!frameAim) seenWithout = seen.Mean();
                else check(age.Max < 0.01 && unevenness < 0.001 && seen.Mean() < seenWithout,
                    "frame aim: turn shown the frame it is read, evenly, and nearer the next shot");
            }
        }
        Render::FrameTiming::Reset();
        Features::FixedCrosshair(fixedBefore);
        reset(0, 0, false);
        return checks;
    }
}

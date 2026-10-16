#include "AimCheck.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Features.hpp"
#include "../../Formats/Types.hpp"
#include "../../Scene.hpp"
#include "../../Utility/Rng.hpp"
#include "../Input/AimNumericPolicy.hpp"
#include "../Input/FrameAim.hpp"
#include "../../Entities/CamSeq/CameraSequence.hpp"
#include "../InputSettings.hpp"
#include "../Render/AnimationClock.hpp"
#include "../Render/FrameTiming.hpp"
#include "../Render/ScenePresentation.hpp"

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
        // The room may still own its intro camera. These checks exercise a
        // first-person player and both HUD styles, independently of settings.
        struct FirstPersonScope
        {
            std::shared_ptr<Formats::CameraSequence> Sequence = Formats::CameraSequence::Current()
                ? Formats::CameraSequence::Current()->shared_from_this() : nullptr;
            bool ProHud = Features::ProHud();
            FirstPersonScope() { Formats::CameraSequence::Current(nullptr); Features::ProHud(false); }
            ~FirstPersonScope() { Formats::CameraSequence::Current(Sequence.get()); Features::ProHud(ProHud); }
        } firstPerson;
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

        // 4. The DS-style crosshair is presented, not turned: a picture shows
        // the aim the next step turns to and the view on its way to the view
        // that step makes, and the step finds exactly the state it left.
        {
            Features::FixedCrosshair(false);
            struct Aim { Vector3 Gun, Facing, Position, Side, Up, Draw, Vec, Muzzle; float Y; };
            const auto aim = [&]
            {
                return Aim{player._gunVec1, player._facingVector, player._aimPosition, player._gunVec2,
                    player._upVector, player._gunDrawPos, player._aimVec, player._muzzlePos, player._aimY};
            };
            const auto same = [](const Aim& a, const Aim& b) { return std::memcmp(&a, &b, sizeof(Aim)) == 0; };
            const auto atPhase = [](double phase)
            {
                Render::FrameTiming::Reset();
                (void)Render::FrameTiming::Advance(phase * Render::FrameTiming::StepSeconds);
            };
            const float envelope = std::acos(Input::AimEnvelopeCos) * 180.0F / Pi;
            Series gunVsStep, arrival, atStart, outside, viewAlong;
            bool presented = true, restored = true;
            for (int i = 0; i < 1000; ++i)
            {
                // A view behind its aim, as the ease leaves it after a turn.
                reset(yaws(rng), pitches(rng) * 0.6F, i % 5 == 4);
                mouse.X = pixels(rng) * 3.0F; mouse.Y = pixels(rng);
                step(mouse);
                const Aim left = aim();
                // At the step's own instant, with the mouse where the step
                // read it, the picture is the step's view.
                atPhase(0.0);
                presented = player.ModPresentFrameAim(mouse) && presented;
                atStart.Add(AngleDegrees(player._facingVector, left.Facing));
                player.ModRestoreFrameAim();
                Mouse moved = mouse; moved.X += pixels(rng); moved.Y += pixels(rng);
                Vector3 shownGun{}, shownFacing{};
                for (const double phase : {0.0, 0.37, 0.9999})
                {
                    atPhase(phase);
                    presented = player.ModPresentFrameAim(moved) && presented;
                    shownGun = player._gunVec1; shownFacing = player._facingVector;
                    const Matrix4 drawn = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                    viewAlong.Add(AngleDegrees(Forward(drawn), shownFacing));
                    outside.Add(std::max(0.0F, AngleDegrees(shownGun, shownFacing) - envelope));
                    player.ModRestoreFrameAim();
                    restored = restored && same(left, aim());
                }
                step(moved);
                gunVsStep.Add(AngleDegrees(shownGun, player._gunVec1));
                arrival.Add(AngleDegrees(shownFacing, player._facingVector));
            }
            std::cout << std::setprecision(6) << "FRAMEAIM ds trials=" << gunVsStep.Count
                << " crosshair_vs_next_step_deg max=" << gunVsStep.Max
                << " view_at_step_end_vs_step_deg max=" << arrival.Max
                << " view_at_step_start_vs_step_deg max=" << atStart.Max
                << " view_outside_envelope_deg max=" << outside.Max
                << " drawn_view_vs_shown_deg max=" << viewAlong.Max << '\n';
            check(presented, "DS crosshair is presented for mouse, exact, first person");
            check(restored, "a DS picture leaves the step exactly the aim it left");
            check(gunVsStep.Max < 0.0001, "the DS crosshair shown is the aim the next step turns to");
            check(arrival.Max < 0.01, "the DS view reaches the step's own view as the step runs");
            check(atStart.Max < 0.01 && outside.Max < 0.01,
                "the DS view starts from the step's and stays inside the envelope of the aim shown");
            check(viewAlong.Max < 0.01, "the drawn DS view looks along the view shown");

            // The same mouse, with and without pictures between the steps:
            // the simulation's aim is the same to the bit, step after step.
            std::vector<std::pair<float, float>> path;
            for (int k = 0; k < 240; ++k) path.emplace_back(pixels(rng), pixels(rng) * 0.3F);
            std::vector<Aim> alone, pictured;
            for (const bool pictures : {false, true})
            {
                reset(30, 10, false);
                std::vector<Aim>& out = pictures ? pictured : alone;
                for (const auto& [dx, dy] : path)
                {
                    Mouse next = mouse; next.X += dx; next.Y += dy;
                    if (pictures)
                    {
                        for (const double phase : {0.25, 0.5, 0.75})
                        {
                            Mouse part = mouse;
                            part.X += dx * static_cast<float>(phase); part.Y += dy * static_cast<float>(phase);
                            atPhase(phase);
                            (void)player.ModPresentFrameAim(part);
                            player.ModRestoreFrameAim();
                        }
                    }
                    mouse = next;
                    step(mouse);
                    out.push_back(aim());
                }
            }
            bool lockstep = alone.size() == pictured.size();
            for (std::size_t k = 0; lockstep && k < alone.size(); ++k) lockstep = same(alone[k], pictured[k]);
            check(lockstep, "DS pictures between steps leave every step's aim bit-identical");

            // A turn at constant speed drawn at 240 Hz: the crosshair is the
            // mouse's the frame it is read, and the view moves every frame.
            // 60 degrees a second keeps the view inside the 15 degree envelope;
            // 180 runs it against the envelope, which the step clamps.
            for (const double speed : {60.0, 180.0})
            for (const bool frameAim : {false, true})
            {
                reset(0, 0, false);
                Render::FrameTiming::Reset();
                Input::FrameAim::Enabled(frameAim);
                constexpr int fps = 240;
                Series age;
                std::vector<double> increments;
                double previous = 0;
                for (int frame = 1; frame <= fps * 3; ++frame)
                {
                    const double t = static_cast<double>(frame) / fps;
                    mouse.X = static_cast<float>(-speed * t / degreesPerPixel);
                    const auto steps = Render::FrameTiming::Advance(1.0 / fps);
                    for (int s = 0; s < steps; ++s) step(mouse);
                    (void)player.ModPresentFrameAim(mouse);
                    const Matrix4 shown = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                    const double truth = std::remainder(speed * t, 360.0);
                    age.Add(std::fabs(std::remainder(truth - YawDegrees(player._gunVec1), 360.0)) / speed * 1000.0);
                    const double yaw = YawDegrees(Forward(shown));
                    if (frame > fps / 2) increments.push_back(std::remainder(yaw - previous, 360.0));
                    previous = yaw;
                    player.ModRestoreFrameAim();
                }
                double mean = 0;
                for (const double d : increments) mean += d;
                mean /= static_cast<double>(increments.size());
                double variance = 0;
                for (const double d : increments) variance += (d - mean) * (d - mean);
                const double unevenness = std::sqrt(variance / static_cast<double>(increments.size())) / std::fabs(mean);
                std::size_t still = 0;
                for (const double d : increments) still += std::fabs(d) < 1e-6 ? 1U : 0U;
                std::cout << std::setprecision(3) << "FRAMEAIM ds schedule fps=" << fps << " deg_per_s=" << speed
                    << " frame_aim=" << (frameAim ? "on " : "off") << " crosshair_age_ms mean=" << age.Mean()
                    << " max=" << age.Max << " view_per_frame_turn_cv=" << unevenness
                    << " frames_view_still=" << still << "/" << increments.size() << '\n';
                if (frameAim)
                {
                    check(age.Max < 0.01 && still == 0 && unevenness < 0.01,
                        "DS crosshair shown the frame it is read, view turning evenly every frame");
                }
            }
            Input::FrameAim::Enabled(true);
        }

        // 5. A watched player: the step built their camera, then the snapshot
        // moved and turned them. A picture shows the camera a step would build
        // where the playout has them now, with their gun where it sits in it.
        {
            Series eyeError, viewError, gunError;
            bool restored = true;
            std::uniform_real_distribution<float> along(-0.6F, 0.6F);
            for (int i = 0; i < 300; ++i)
            {
                reset(yaws(rng), pitches(rng) * 0.6F, false);
                player.UpdateCamera();
                player.UpdateAimVecs();
                const Vector3 from = player.Position;
                player.ModNoteCameraSource();
                // The snapshot's move and turn, then the playout's travel on to the picture.
                const Vector3 moved(along(rng), along(rng) * 0.3F, along(rng));
                const Vector3 travel(along(rng) * 0.5F, 0, along(rng) * 0.5F);
                player.Position = from + moved;
                Vector3 facing = player._facingVector;
                facing = Vector3(facing.X + along(rng) * 0.3F, facing.Y + along(rng) * 0.2F, facing.Z + along(rng) * 0.3F);
                player.ModSetFacing(facing);
                const Matrix4 stepView = player.CameraInfo()->ViewMatrix;
                const Vector3 cameraBefore = player.CameraInfo()->Position;
                const Vector3 gunInView = Matrix::Vec3MultMtx4(player._gunDrawPos, stepView);
                if (!player.ModPresentWatchedView(travel)) { restored = false; continue; }
                const Matrix4 shown = player.CameraInfo()->ViewMatrix;
                const Vector3 shownEye = player.CameraInfo()->Position;
                gunError.Add(OpenTK::Mathematics::Length(Matrix::Vec3MultMtx4(player._gunDrawPos, shown) - gunInView) * 100.0);
                player.ModRestoreFrameAim();
                restored = restored && Same(cameraBefore, player.CameraInfo()->Position)
                    && std::memcmp(&stepView, &player.CameraInfo()->ViewMatrix, sizeof(Matrix4)) == 0;
                // The camera a step builds there.
                player.Position = from + moved + travel;
                player.UpdateCamera();
                eyeError.Add(OpenTK::Mathematics::Length(shownEye - player.CameraInfo()->Position) * 100.0);
                viewError.Add(ViewDegrees(shown, player.CameraInfo()->ViewMatrix));
            }
            std::cout << std::setprecision(6) << "FRAMEAIM watched trials=" << viewError.Count
                << " eye_vs_step_cm max=" << eyeError.Max << " view_vs_step_deg max=" << viewError.Max
                << " gun_in_view_cm max=" << gunError.Max << '\n';
            check(restored, "a watched picture leaves the step's camera as it was");
            check(eyeError.Max < 0.01 && viewError.Max < 0.01,
                "a watched picture is the camera a step builds where the player is shown");
            check(gunError.Max < 0.01, "a watched player's gun stays where it sits in their view");
        }

        // 6. The local eye: a step moved it with the body, and a picture part
        // of the way to the next step carries it on along that motion -- at a
        // steady walk, exactly where the next step puts it -- with the gun
        // riding along, and the step's camera back afterwards.
        {
            const auto atPhase = [](double phase)
            {
                Render::FrameTiming::Reset();
                (void)Render::FrameTiming::Advance(phase * Render::FrameTiming::StepSeconds);
            };
            Series eyeError, viewError, gunError, bodyError;
            bool restored = true, carried = true;
            std::uniform_real_distribution<float> walk(-0.4F, 0.4F), phases(0.05F, 0.95F);
            for (const bool fixed : {true, false})
            {
                Features::FixedCrosshair(fixed);
                for (int i = 0; i < 200; ++i)
                {
                    reset(yaws(rng), pitches(rng) * 0.6F, false);
                    const Vector3 start = player.Position;
                    const Vector3 velocity(walk(rng), walk(rng) * 0.2F, walk(rng));
                    // Two steps of a walk, as the scene records them.
                    player.SetPrevPosition(start);
                    player.Position = start;
                    player.UpdateCamera();
                    Render::ScenePresentation::EndStep(*player._scene);
                    player.Position = start + velocity;
                    player.UpdateCamera();
                    player.UpdateAimVecs();
                    const Matrix4 stepView = player.CameraInfo()->ViewMatrix;
                    const Vector3 stepEye = player.CameraInfo()->Position;
                    const Vector3 gunInView = Matrix::Vec3MultMtx4(player._gunDrawPos, stepView);
                    const float phase = phases(rng);
                    atPhase(phase);
                    // The world ran this step: what a scene step tells the presentation.
                    Render::ScenePresentation::EndStep(*player._scene);
                    if (!player.ModPresentOwnView(&mouse)) { carried = false; continue; }
                    const Matrix4 shown = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                    const Vector3 shownEye = player.CameraInfo()->Position;
                    // The body's drawn items, carried by the same step's motion.
                    const auto body = player.ModPresentationCarry();
                    const Vector3 bodyShift = body ? Vector3(body->M41, body->M42, body->M43) : Vector3{};
                    bodyError.Add(OpenTK::Mathematics::Length(bodyShift
                        - OpenTK::Mathematics::Multiply(velocity, phase)) * 100.0);
                    gunError.Add(OpenTK::Mathematics::Length(Matrix::Vec3MultMtx4(player._gunDrawPos, shown) - gunInView) * 100.0);
                    player.ModRestoreFrameAim();
                    restored = restored && Same(stepEye, player.CameraInfo()->Position)
                        && Same(start + velocity, player.Position)
                        && std::memcmp(&stepView, &player.CameraInfo()->ViewMatrix, sizeof(Matrix4)) == 0;
                    // The camera a step builds at that point of a steady walk.
                    player.Position = start + velocity + OpenTK::Mathematics::Multiply(velocity, phase);
                    player.UpdateCamera();
                    eyeError.Add(OpenTK::Mathematics::Length(shownEye - player.CameraInfo()->Position) * 100.0);
                    // The DS-style picture also eases the view, which the bare step does not.
                    if (fixed) viewError.Add(ViewDegrees(shown, player.CameraInfo()->ViewMatrix));
                }
            }
            Features::FixedCrosshair(true);
            Render::FrameTiming::Reset();
            std::cout << std::setprecision(6) << "FRAMEAIM own eye trials=" << eyeError.Count
                << " eye_vs_steady_walk_cm max=" << eyeError.Max << " view_vs_step_deg max=" << viewError.Max
                << " gun_in_view_cm max=" << gunError.Max << " body_vs_steady_walk_cm max=" << bodyError.Max << '\n';
            check(carried, "the local eye is carried between steps");
            check(bodyError.Max < 0.01, "the local body is carried with it");
            check(restored, "a local picture leaves the step's camera as it was");
            check(eyeError.Max < 0.01 && viewError.Max < 0.01,
                "on a steady walk the carried eye is where the step puts it then");
            check(gunError.Max < 0.01, "the local gun stays where it sits in the view");

            // A view turned by anything but this frame's mouse -- a pad, keys,
            // an alt-form camera swinging round the ball -- turns on between
            // steps along the step's own turn: the target is carried too.
            Series targetError;
            for (int i = 0; i < 200; ++i)
            {
                reset(yaws(rng), pitches(rng) * 0.5F, false);
                player.UpdateCamera();
                const Vector3 targetBefore = player.CameraInfo()->Target;
                Render::ScenePresentation::EndStep(*player._scene);
                player.UpdateAimX(walk(rng) * 10.0F);
                player.UpdateAimY(walk(rng) * 5.0F);
                player.UpdateCamera();
                player.UpdateAimVecs();
                Render::ScenePresentation::EndStep(*player._scene);
                const Vector3 targetNow = player.CameraInfo()->Target;
                const float phase = phases(rng);
                atPhase(phase);
                if (!player.ModPresentOwnView(nullptr)) { carried = false; continue; }
                const Vector3 expected = targetNow + OpenTK::Mathematics::Multiply(targetNow - targetBefore, phase);
                targetError.Add(OpenTK::Mathematics::Length(player.CameraInfo()->Target - expected) * 100.0);
                player.ModRestoreFrameAim();
                restored = restored && Same(targetNow, player.CameraInfo()->Target);
            }
            std::cout << std::setprecision(6) << "FRAMEAIM own turn trials=" << targetError.Count
                << " target_vs_step_turn_cm max=" << targetError.Max << '\n';
            check(carried && restored && targetError.Max < 0.01,
                "a view no mouse turns this frame turns on along the step's turn");
        }

        // Idle sway is a cosine segment, not mouse motion. A picture samples
        // that same segment without choosing the next random target.
        {
            const bool noSway = Features::NoIdleSway();
            const bool delayed = Features::DelayedIdleSway();
            Features::NoIdleSway(false);
            Features::DelayedIdleSway(false);
            Series endpoint, gunError;
            bool restored = true, moving = true, takeover = true;
            for (const bool fixed : {true, false})
            {
                Features::FixedCrosshair(fixed);
                for (const float segment : {.2F, .7F, .9999F})
                {
                    reset(0, 0, false);
                    player._timeSinceInput = static_cast<std::uint16_t>(player.ModIdleSwayStartTime() + 5);
                    player._field410 = player._field428 = Vector3(0, 0, 1);
                    player._field41C = Vector3(.04F, .03F, 1);
                    player._field40C = segment;
                    player._facingVector = player.ModIdleSwayFacing(segment);
                    player.ModRebuildAimFromFacing();
                    player.UpdateCamera();
                    Render::ScenePresentation::EndStep(*player._scene);
                    Render::ScenePresentation::EndStep(*player._scene);
                    const auto facing = player._facingVector;
                    const auto gun = player._gunVec1;
                    const auto muzzle = player._muzzlePos;
                    const auto camera = player.CameraInfo()->ViewMatrix;
                    const auto rng2 = Rng::Rng2();
                    const auto gunInView = Matrix::Vec3MultMtx4(player._gunDrawPos, camera);
                    for (const double phase : {0.0, .25, .5, .75, 1.0})
                    {
                        Render::ScenePresentation::ForcePhase(phase);
                        moving = player.ModPresentOwnView(&mouse) && moving;
                        const auto shown = Input::FrameAim::View(&player, *player.CameraInfo(), player.CameraInfo()->ViewMatrix);
                        gunError.Add(OpenTK::Mathematics::Length(
                            Matrix::Vec3MultMtx4(player._gunDrawPos, shown) - gunInView));
                        if (phase == 0) moving = moving && ViewDegrees(shown, camera) < .0001;
                        if (phase == .5 && segment < .99F) moving = moving && ViewDegrees(shown, camera) > .00001;
                        player.ModRestoreFrameAim();
                        restored = restored && Same(facing, player._facingVector) && Same(gun, player._gunVec1)
                            && Same(muzzle, player._muzzlePos) && player._field40C == segment && Rng::Rng2() == rng2
                            && std::memcmp(&camera, &player.CameraInfo()->ViewMatrix, sizeof(camera)) == 0;
                        if (phase == 1)
                        {
                            player.ModUpdateIdleSway();
                            player.UpdateCamera();
                            endpoint.Add(ViewDegrees(shown, player.CameraInfo()->ViewMatrix));
                            player._field40C = segment;
                            player._field410 = player._field428 = Vector3(0, 0, 1);
                            player._field41C = Vector3(.04F, .03F, 1);
                            player._facingVector = facing;
                            Rng::SetRng2(rng2);
                        }
                    }
                    Mouse fresh = mouse; fresh.X += 3;
                    takeover = takeover && !player.ModIdleSwayPresentable(&fresh);
                    player._equipInfo->Zoomed = true;
                    takeover = takeover && !player.ModIdleSwayPresentable(&mouse);
                    player._equipInfo->Zoomed = false;
                }
            }
            Render::ScenePresentation::ForcePhase(std::nullopt);
            Features::NoIdleSway(noSway);
            Features::DelayedIdleSway(delayed);
            std::cout << "FRAMEAIM idle sway endpoint_deg max=" << endpoint.Max
                << " gun_in_view max=" << gunError.Max << '\n';
            check(moving, "idle sway moves between steps in both HUD styles");
            check(restored, "idle sway pictures preserve camera aim muzzle phase and RNG");
            check(endpoint.Max < .001, "idle sway arrives at the next step including random-target rollover");
            check(gunError.Max < .0001, "idle sway carries the gun with the view");
            check(takeover, "fresh mouse motion and zoom take priority over idle sway");
        }

        Render::FrameTiming::Reset();
        Features::FixedCrosshair(fixedBefore);
        reset(0, 0, false);
        return checks;
    }
}

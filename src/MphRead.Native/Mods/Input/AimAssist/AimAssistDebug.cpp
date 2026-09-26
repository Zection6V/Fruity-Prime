#include "AimAssistDebug.hpp"

#include "../AimInputSourceTracker.hpp"
#include "../GamepadManager.hpp"
#include "../../Chat/ChatFont.hpp"
#include "../../../Hud/HudInfo.hpp"
#include "../../../Scene.hpp"
#include "../../../NativeRuntime/System/Number.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

namespace MphRead::Mods::Input::AimAssist
{
    namespace
    {
        [[nodiscard]] std::string N(float value, std::string_view format)
        {
            return ::MphRead::NativeRuntime::ToString(value, format);
        }

        [[nodiscard]] const char* B(bool value) noexcept
        {
            return value ? "True" : "False";
        }
    }

    void AimAssistDebug::Draw(Scene& scene)
    {
        using Chat::ChatFont;
        if (!Enabled)
        {
            return;
        }
        if (_scene != &scene)
        {
            _scene = &scene;
            _font = std::make_shared<Hud::HudObjectInstance>(ChatFont::Cell, ChatFont::Cell);
            _font->Enabled = true;
            _font->SetPaletteData(std::make_shared<const std::vector<ColorRgba>>(
                std::initializer_list<ColorRgba>{ColorRgba(), ColorRgba(255, 255, 255, 255)}), scene);
            const std::span<std::uint8_t> pixels = ChatFont::Pixels();
            _font->SetCharacterData(std::make_shared<const std::vector<std::uint8_t>>(pixels.begin(), pixels.end()), scene);
        }
        const std::int64_t now = ::MphRead::NativeRuntime::EnvironmentTickCount64();
        if (now - _textAt > 150)
        {
            _textAt = now;
            const GamepadSnapshot snapshot = GamepadManager::Snapshot();
            _lines = {
                std::string("Aim: ") + ToString(AimInputSourceTracker::Current()) + " pad: " + snapshot.DeviceId.value_or(""),
                "Target " + std::to_string(Result.TargetSlot) + " score " + N(Result.Score, "0.00") + " distance "
                    + N(Target.Distance, "0.0") + " body " + N(Target.BodyError.Length(), "0.00") + " head "
                    + N(Target.HeadError.Length(), "0.00"),
                std::string("LOS body ") + B(Target.BodyVisible) + " head " + B(Target.HeadVisible) + " point "
                    + ToString(Result.PointType) + " blend " + N(Result.HeadBlend, "0.00"),
                "Friction " + N(Result.Friction, "0.00") + " rotation " + N(Result.RotationStrength, "0.00")
                    + " velocity " + N(Velocity.X, "0.0") + "," + N(Velocity.Y, "0.0"),
                "Raw " + N(Raw.X, "0.00") + "," + N(Raw.Y, "0.00") + " final " + N(Result.X, "0.00") + ","
                    + N(Result.Y, "0.00") + " correction " + N(Result.X - Raw.X, "0.00") + "," + N(Result.Y - Raw.Y, "0.00")};
        }
        scene.DrawHudFlatBox(2, 2, 254, 37, OpenTK::Mathematics::Vector4(0, 0, 0, .8F));
        float y = 4;
        for (const std::string& line : _lines)
        {
            float x = 4;
            for (const char ch : line)
            {
                const std::int32_t index = ChatFont::Index(static_cast<char16_t>(static_cast<unsigned char>(ch)));
                if (index < 0)
                {
                    continue;
                }
                _font->PositionX = x / 256;
                _font->PositionY = y / 192;
                _font->Alpha = 1;
                _font->SetData(index, ColorRgba(220, 255, 220, 255), scene);
                scene.DrawHudObject(_font, 1, .45F);
                x += static_cast<float>(ChatFont::Widths()[static_cast<std::size_t>(index)]) * .45F;
                if (x > 250)
                {
                    break;
                }
            }
            y += 6;
        }
    }
}

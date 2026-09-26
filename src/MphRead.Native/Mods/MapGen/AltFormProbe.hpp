#pragma once

#include "../../Renderer.hpp"
#include "../../Formats/Enums.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    class PlayerEntity;
    class PlayerControls;
    class Keybind;
}

namespace MphRead::Mods::MapGen
{
    // Stands one hunter on one spot, jumps, morphs, and reports where the
    // floor stopped working. Runs the real engine with nobody watching.
    // Usage: -altprobe "DUST2" -at 1.17053,-2.73169,-41.3657 [-hunter Weavel]
    class AltFormProbe final : public MphRead::RendererPlatform::WindowEvents
    {
    private:
        std::shared_ptr<MphRead::RendererPlatform::Window> _window;

        const std::string _room;
        const OpenTK::Mathematics::Vector3 _start;
        const Hunter _hunter;

        [[nodiscard]] static std::int32_t MaxDelay();
        static constexpr std::int32_t Settle = 40;
        static constexpr std::int32_t Observe = 150;

        // The probe drives this slot, not slot 0, whose controls are refilled
        // from the keyboard every frame.
        static constexpr std::int32_t Slot = 1;

        static inline std::optional<std::int32_t> _traceDelay{};

        std::int32_t _delay = 0;
        std::int32_t _frame = 0;
        std::int32_t _resetFrames = 0;
        bool _armed = false;
        float _rest = 0;
        float _lowest = 0;
        float _highest = 0;
        float _final = 0;
        std::vector<std::tuple<std::int32_t, float, float, float, bool>> _trials;

        // How far below the starting height counts as through the floor.
        static constexpr float Through = 1.2F;

        std::unique_ptr<MphRead::Scene> _scene;
        std::vector<bool> _wasDown;

        [[nodiscard]] static MphRead::RendererPlatform::WindowSettings WindowSettings();

        AltFormProbe(std::string room, OpenTK::Mathematics::Vector3 start, Hunter hunter);

        [[nodiscard]] OpenTK::Mathematics::Vector2i ClientSize() const;
        void Close();
        void SwapBuffers();
        void Run();
        void Step();
        void Place(Entities::PlayerEntity& player);
        static void Press(Entities::PlayerEntity& player, Entities::Keybind& bind, bool down);
        void Finish(Entities::PlayerEntity& player, Entities::PlayerControls& c);
        [[nodiscard]] std::int32_t Report();

    protected:
        void OnLoad() override;
        void OnRenderFrame(const MphRead::RendererPlatform::FrameEventArgs& args) override;

    public:
        AltFormProbe(const AltFormProbe&) = delete;
        AltFormProbe& operator=(const AltFormProbe&) = delete;
        ~AltFormProbe() override;

        // Print every frame of one trial instead of a table of all of them.
        [[nodiscard]] static std::optional<std::int32_t> TraceDelay() noexcept { return _traceDelay; }
        static void TraceDelay(std::optional<std::int32_t> value) noexcept { _traceDelay = value; }

        [[nodiscard]] MphRead::Scene& Scene() noexcept;

        [[nodiscard]] static std::int32_t Run(const std::string& room, OpenTK::Mathematics::Vector3 start, Hunter hunter);
    };
}

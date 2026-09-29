#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class RenderWindow;
    class Scene;
}

namespace MphRead::Mods::Render
{
    class LauncherHunter final
    {
    public:
        LauncherHunter() = delete;

        [[nodiscard]] static bool Wanted() noexcept;
        static void Wanted(bool value) noexcept;
        [[nodiscard]] static ::MphRead::Hunter Hunter() noexcept;
        static void Hunter(::MphRead::Hunter value) noexcept;
        [[nodiscard]] static std::int32_t Suit() noexcept;
        static void Suit(std::int32_t value) noexcept;
        [[nodiscard]] static float Left() noexcept;
        static void Left(float value) noexcept;
        [[nodiscard]] static float Top() noexcept;
        static void Top(float value) noexcept;
        [[nodiscard]] static float Right() noexcept;
        static void Right(float value) noexcept;
        [[nodiscard]] static float Bottom() noexcept;
        static void Bottom(float value) noexcept;
        [[nodiscard]] static bool Drawn() noexcept;
        static void Reset();
        // Delete/destroy the launcher side scene while its owning desktop GL
        // context is still alive.
        static void ReleaseGl() noexcept;
        // A match's scene let go of its GL objects: rebuild the side scene.
        static void NoteGlUnloaded() noexcept;
        static void Draw(::MphRead::RenderWindow& window, std::int32_t width, std::int32_t height);

    private:
        static bool _wanted;
        static ::MphRead::Hunter _hunter;
        static std::int32_t _suit;
        static float _left;
        static float _top;
        static float _right;
        static float _bottom;
        static bool _drawn;
        static bool _failed;
        static bool _said;
        static bool _glStale;
        static std::shared_ptr<::MphRead::Scene> _scene;
    };
}

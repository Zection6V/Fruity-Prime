#pragma once

namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::Mods::Testing
{
    class GoreaBattle final
    {
    public:
        static void Run();
        [[nodiscard]] static bool Begin(MphRead::RenderWindow& window);

        GoreaBattle() = delete;
        GoreaBattle(const GoreaBattle&) = delete;
        GoreaBattle& operator=(const GoreaBattle&) = delete;
    };
}

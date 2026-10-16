#pragma once
#include <cstdint>
namespace MphRead::Entities { class PlayerEntity; }
namespace MphRead::Mods::Network
{
    class FormTransitionTrace final
    {
    public:
        static bool Enabled() noexcept { return _enabled; }
        static void Enabled(bool value) noexcept { _enabled = value; }
        static void RecordDraw(const Entities::PlayerEntity& player);
    private:
        static bool _enabled;
    };

    // Diagnostic observation around the original acceptance rules.
    class FormRequestTrace final
    {
    public:
        explicit FormRequestTrace(const Entities::PlayerEntity& player);
        ~FormRequestTrace();
    private:
        const Entities::PlayerEntity* _player = nullptr;
        std::uint8_t _before = 0;
    };
}

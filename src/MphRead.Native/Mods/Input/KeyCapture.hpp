#pragma once
namespace MphRead::Mods::Input {
class KeyCapture final {
public:
 static bool AnyListening() noexcept { return _listening; }
 static void Listening(bool value) noexcept { _listening = value; }
private:
 inline static bool _listening = false;
};
}

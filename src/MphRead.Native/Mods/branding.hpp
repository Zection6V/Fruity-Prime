#pragma once

#include <string>

namespace fruityprime::branding {

inline constexpr const char* Name = "Fruity Prime";
inline constexpr const char* FileName = "FruityPrime";
inline constexpr const char* Upstream = "MphRead";
inline constexpr const char* Repository = "liveteklol/Fruity-Prime";

[[nodiscard]] std::string name_and_version();

} // namespace fruityprime::branding

namespace fruityprime::mods {

// Exact static type boundary of Mods/Branding.cs. The lowercase namespace
// functions above remain compatibility entry points for existing native code.
class Branding final {
public:
    inline static constexpr const char* Name = branding::Name;
    inline static constexpr const char* FileName = branding::FileName;
    inline static constexpr const char* Upstream = branding::Upstream;
    inline static constexpr const char* Repository = branding::Repository;

    [[nodiscard]] static std::string Executable();
    [[nodiscard]] static std::string NameAndVersion();
};

} // namespace fruityprime::mods

namespace MphReadNative::Mods {
using Branding = ::fruityprime::mods::Branding;
}

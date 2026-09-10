#pragma once

#include <string>
#include <string_view>

namespace MphRead
{
    namespace Mods
    {
        class Branding final
        {
        public:
            inline static constexpr std::string_view Name = "Fruity Prime";
            inline static constexpr std::string_view FileName = "FruityPrime";
            inline static constexpr std::string_view Upstream = "MphRead";
            inline static constexpr std::string_view Repository = "liveteklol/Fruity-Prime";

            [[nodiscard]] static std::string Executable();
            [[nodiscard]] static std::string NameAndVersion();

            Branding() = delete;
            Branding(const Branding&) = delete;
            Branding& operator=(const Branding&) = delete;
        };
    }
}

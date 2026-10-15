#include "Formats/Enums.hpp"
#include "Formats/Formats.hpp"
#include "Mods/Entry/OptionValues.hpp"
#include "Mods/Network/Discovery/NetMaster.hpp"
#include "Mods/Network/Protocol/Packets/PacketTypes.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

// The typed values ModEntry reads off the command line, pinned as they parse
// today (refactoring instructions 2026-10-15, section 6.1 "required
// additions"): absent, invalid, negative, separators, non-ASCII, rounding and
// the thousands rule each command was ported with. The current-culture
// reader is only checked where every culture agrees.
using namespace MphRead::Mods::Entry;
using MphRead::BeamType;
using MphRead::GameMode;
using MphRead::Hunter;

namespace
{
    using Args = std::vector<std::string>;
    using Text = std::optional<std::string>;

    bool Check(bool condition, const char* id, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << id << ": " << message << '\n';
        }
        return condition;
    }

    bool Invariant()
    {
        bool passed = true;
        double d = 5;
        passed &= Check(!TryParseDoubleInvariant(std::nullopt, d) && d == 0,
            "V01", "absent: false, and the value is set to 0");
        float f = 5;
        passed &= Check(!TryParseFloatInvariant(std::nullopt, f) && f == 0,
            "V01b", "absent float: false, and the value is set to 0");
        passed &= Check(TryParseDoubleInvariant(Text("2.5"), d) && d == 2.5, "V02", "a decimal point");
        passed &= Check(TryParseDoubleInvariant(Text("-3.25"), d) && d == -3.25, "V03", "a negative number");
        passed &= Check(TryParseDoubleInvariant(Text("1,000.5"), d) && d == 1000.5,
            "V04", "thousands separators are allowed by default");
        passed &= Check(!TryParseDoubleInvariant(Text("1,000.5"), d, false),
            "V04b", "and refused when the command asked for no thousands");
        passed &= Check(!TryParseFloatInvariant(Text("1,5"), f, false),
            "V04c", "a comma is never a decimal separator in the invariant culture");
        passed &= Check(TryParseDoubleInvariant(Text("1e3"), d) && d == 1000, "V05", "an exponent");
        passed &= Check(TryParseDoubleInvariant(Text(" 7 "), d) && d == 7, "V06", "surrounding white space");
        passed &= Check(!TryParseDoubleInvariant(Text("abc"), d), "V07", "not a number");
        passed &= Check(!TryParseDoubleInvariant(Text(""), d), "V07b", "an empty value is not a number");
        passed &= Check(!TryParseDoubleInvariant(Text("\xEF\xBC\x91\xEF\xBC\x92"), d),
            "V08", "full-width digits are not digits");
        passed &= Check(TryParseFloatInvariant(Text("0.1"), f) && f == 0.1f, "V09", "a float rounds to nearest");
        passed &= Check(TryParseFloatInvariant(Text("16777217"), f) && f == 16777216.0f,
            "V09b", "a float rounds past 2^24");
        passed &= Check(TryParseDoubleCurrent("15", d) && d == 15, "V10", "current culture: an integer");
        passed &= Check(!TryParseDoubleCurrent("x", d), "V10b", "current culture: not a number");
        return passed;
    }

    bool Enums()
    {
        bool passed = true;
        GameMode mode = GameMode::Battle;
        passed &= Check(!TryParseGameMode(std::nullopt, mode), "V11", "absent mode");
        // Enum.TryParse semantics, as ported: a name that parses as nothing
        // still writes the enum's default. So "-mode bogus" is mode None,
        // not the command's own default of Battle -- unlike an absent -mode.
        passed &= Check(!TryParseGameMode(Text("nope"), mode) && mode == GameMode::None,
            "V11b", "an unknown mode overwrites the value with None");
        mode = GameMode::Battle;
        passed &= Check(TryParseGameMode(Text("capture"), mode) && mode == GameMode::Capture,
            "V11c", "mode names ignore case");
        Hunter hunter = Hunter::Samus;
        passed &= Check(TryParseHunter(Text("noxus"), hunter) && hunter == Hunter::Noxus,
            "V12", "hunter names ignore case");
        BeamType beam = BeamType::PowerBeam;
        passed &= Check(TryParseBeam(Text("SHOCKCOIL"), beam) && beam == BeamType::ShockCoil,
            "V13", "beam names ignore case");
        beam = BeamType::ShockCoil;
        passed &= Check(!TryParseBeam(Text("bogus"), beam) && beam == BeamType::PowerBeam,
            "V13b", "an unknown beam overwrites the value with PowerBeam (0), as -dpstest -weapon does");
        return passed;
    }

    bool Readers()
    {
        using MphRead::Mods::Network::NetConfig;
        using MphRead::Mods::Network::NetMasterConfig;
        bool passed = true;
        passed &= Check(ParsePort(Args{}) == NetConfig::DefaultPort, "V14", "no -port: the game port");
        passed &= Check(ParsePort(Args{"-port", "1234"}) == 1234, "V14b", "-port N");
        passed &= Check(ParsePort(Args{"-port", "x"}) == NetConfig::DefaultPort, "V14c", "a bad -port: the default");
        passed &= Check(ParsePort(Args{"-port", "-5"}) == -5, "V14d", "no range check on -port");
        passed &= Check(ParsePort(Args{"-masterport", "1"}) == NetConfig::DefaultPort,
            "V14e", "-masterport is not -port");
        passed &= Check(ParseMasterPort(Args{}) == NetMasterConfig::DefaultPort, "V15", "no -masterport: the directory port");
        passed &= Check(ParseMasterPort(Args{"-masterport", "999"}) == 999, "V15b", "-masterport N");
        passed &= Check(ParseName(Args{"-name", "Bob"}) == "Bob", "V16", "-name");
        passed &= Check(!ParseName(Args{}).empty() && ParseName(Args{}) == MachineName(),
            "V16b", "no -name: the machine's name");
        passed &= Check(ParseHunter(Args{}) == Hunter::Samus, "V17", "no -hunter: Samus");
        passed &= Check(ParseHunter(Args{"-hunter", "bogus"}) == Hunter::Samus, "V17b", "a bad -hunter: Samus");
        passed &= Check(ParseHunter(Args{"--HUNTER", "Sylux"}) == Hunter::Sylux, "V17c", "-hunter H");
        passed &= Check(ParseRecolor(Args{}) == 0 && ParseRecolor(Args{"-recolor", "3"}) == 3
            && ParseRecolor(Args{"-recolor", "x"}) == 0, "V18", "-recolor, else 0");
        return passed;
    }
}

int main()
{
    bool passed = true;
    passed &= Invariant();
    passed &= Enums();
    passed &= Readers();
    if (!passed)
    {
        return 1;
    }
    std::cout << "ModEntry option values: all cases pass\n";
    return 0;
}

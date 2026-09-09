#include "Mods/credits.hpp"

#include "Mods/branding.hpp"

#include <string>

namespace fruityprime::credits {
namespace {

constexpr std::array<Entry, 12> Entries{{
    {"NoneGiven",
     "MphRead: the model viewer, scene renderer, format parsers and "
     "gameplay recreation this is built on",
     "https://github.com/NoneGiven/MphRead"},
    {"dsgraph",
     "the original MPH model viewer, on which all other projects are built",
     ""},
    {"Chemical", "documentation of the model format",
     "https://gitlab.com/ch-mcl/metroid-prime-hunters-file-document"},
    {"McKay42",
     "COLLADA export method (mph-model-viewer) and ARC file format "
     "information (mph-arc-extractor)",
     "https://github.com/McKay42"},
    {"Barubary", "LZ10 compression routines (dsdecmp)",
     "https://github.com/Barubary/dsdecmp"},
    {"loveemu", "SWAV conversion function (swav2wav)",
     "https://github.com/loveemu/loveemu-lab"},
    {"Gericom", "ActImagine VX movie file format information, via an ffmpeg "
     "patch", ""},
    {"CharlesVanEeckhout", "further understanding of VX video decoding",
     "https://github.com/CharlesVanEeckhout/actimagine"},
    {"CyberBotX", "NCSF converter and player for Nintendo DS sequenced music",
     "https://github.com/CyberBotX/NCSF"},
    {"hackyourlife",
     "mph-viewer, developed in parallel; the transparency rendering was "
     "derived from its source",
     "https://github.com/hackyourlife/mph-viewer"},
    {"OpenTK", "the OpenGL bindings the renderer uses",
     "https://github.com/opentk/opentk"},
    {"OpenAL Soft and SoundFlow", "audio",
     "https://github.com/LSXPrime/SoundFlow"}
}};

} // namespace

const std::array<Entry, 12>& entries() noexcept {
    return Entries;
}

std::string summary() {
    return std::string(branding::Name) + " is " + std::string(Author)
        + "'s fork of " + std::string(branding::Upstream)
        + " by NoneGiven.";
}

std::string names() {
    std::string result;
    for (const Entry& entry : Entries) {
        if (entry.who == "NoneGiven") {
            continue;
        }
        if (!result.empty()) {
            result += " · ";
        }
        result += entry.who;
    }
    return result;
}

std::string compact() {
    return "A fork of " + std::string(branding::Upstream)
        + " by NoneGiven\n" + names();
}

void print(std::ostream& output) {
    output << '\n'
           << "  " << branding::name_and_version() << '\n'
           << "  " << summary() << "\n\n"
           << "  " << Author << '\n'
           << "      " << ForkWork << '\n'
           << "      support this project: " << SupportUrl << "\n\n"
           << "  A significant portion of this project's code is based on the\n"
           << "  file format information or source code of these projects:\n\n";
    for (const Entry& entry : Entries) {
        output << "  " << entry.who << '\n'
               << "      " << entry.what << '\n';
        if (!entry.where.empty()) {
            output << "      " << entry.where << '\n';
        }
    }
    output << "\n  Metroid Prime Hunters is Nintendo's. No game data is included\n"
           << "  with this program: it is unpacked from your own cartridge dump.\n\n";
}

} // namespace fruityprime::credits

namespace fruityprime::mods {

const std::array<Credits::Entry, 12>& Credits::Entries() noexcept {
    return credits::entries();
}

std::string Credits::Summary() { return credits::summary(); }
std::string Credits::Names() { return credits::names(); }
std::string Credits::Compact() { return credits::compact(); }
void Credits::Print(std::ostream& output) { credits::print(output); }

} // namespace fruityprime::mods

#include "Credits.hpp"
#include "Branding.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    void WriteLine(std::string_view value = {})
    {
        std::cout.write(value.data(), static_cast<std::streamsize>(value.size()));
        std::cout.put('\n');
    }
}

namespace MphRead
{
    namespace Mods
    {
        Credits::Entry::Entry(std::string who, std::string what, std::string where)
            : _who(std::move(who)), _what(std::move(what)), _where(std::move(where))
        {
        }

        const std::string& Credits::Entry::Who() const noexcept
        {
            return _who;
        }

        const std::string& Credits::Entry::What() const noexcept
        {
            return _what;
        }

        const std::string& Credits::Entry::Where() const noexcept
        {
            return _where;
        }

        std::string Credits::Summary()
        {
            std::string result;
            result.reserve(
                Branding::Name.size() + Author.size() + Branding::Upstream.size() + 34U);
            result.append(Branding::Name);
            result.append(" is ");
            result.append(Author);
            result.append("'s fork of ");
            result.append(Branding::Upstream);
            result.append(" by NoneGiven.");
            return result;
        }

        std::string Credits::Compact()
        {
            std::string result;
            result.reserve(Branding::Upstream.size() + 31U);
            result.append("A fork of ");
            result.append(Branding::Upstream);
            result.append(" by NoneGiven\n");
            result.append(Names());
            return result;
        }

        std::string Credits::Names()
        {
            std::string result;
            bool first = true;
            for (const Entry& entry : Entries())
            {
                if (entry.Who() == "NoneGiven")
                {
                    continue;
                }

                if (!first)
                {
                    result.append(" · ");
                }
                result.append(entry.Who());
                first = false;
            }
            return result;
        }

        const std::vector<Credits::Entry>& Credits::Entries()
        {
            static const std::vector<Entry> entries
            {
                Entry(
                    "NoneGiven",
                    "MphRead: the model viewer, scene renderer, "
                    "format parsers and gameplay recreation this is built on",
                    "https://github.com/NoneGiven/MphRead"),
                Entry(
                    "dsgraph",
                    "the original MPH model viewer, on which all "
                    "other projects are built",
                    ""),
                Entry(
                    "Chemical",
                    "documentation of the model format",
                    "https://gitlab.com/ch-mcl/metroid-prime-hunters-file-document"),
                Entry(
                    "McKay42",
                    "COLLADA export method (mph-model-viewer) and "
                    "ARC file format information (mph-arc-extractor)",
                    "https://github.com/McKay42"),
                Entry(
                    "Barubary",
                    "LZ10 compression routines (dsdecmp)",
                    "https://github.com/Barubary/dsdecmp"),
                Entry(
                    "loveemu",
                    "SWAV conversion function (swav2wav)",
                    "https://github.com/loveemu/loveemu-lab"),
                Entry(
                    "Gericom",
                    "ActImagine VX movie file format information, "
                    "via an ffmpeg patch",
                    ""),
                Entry(
                    "CharlesVanEeckhout",
                    "further understanding of VX video "
                    "decoding",
                    "https://github.com/CharlesVanEeckhout/actimagine"),
                Entry(
                    "CyberBotX",
                    "NCSF converter and player for Nintendo DS "
                    "sequenced music",
                    "https://github.com/CyberBotX/NCSF"),
                Entry(
                    "hackyourlife",
                    "mph-viewer, developed in parallel; the "
                    "transparency rendering was derived from its source",
                    "https://github.com/hackyourlife/mph-viewer"),
                Entry(
                    "OpenTK",
                    "the OpenGL bindings the renderer uses",
                    "https://github.com/opentk/opentk"),
                Entry(
                    "OpenAL Soft and SoundFlow",
                    "audio",
                    "https://github.com/LSXPrime/SoundFlow")
            };
            return entries;
        }

        void Credits::Print()
        {
            WriteLine();
            WriteLine(std::string("  ") + Branding::NameAndVersion());
            WriteLine(std::string("  ") + Summary());
            WriteLine();
            WriteLine(std::string("  ") + std::string(Author));
            WriteLine(std::string("      ") + std::string(ForkWork));
            WriteLine(std::string("      support this project: ") + std::string(SupportUrl));
            WriteLine();
            WriteLine("  A significant portion of this project's code is based on the");
            WriteLine("  file format information or source code of these projects:");
            WriteLine();
            for (const Entry& entry : Entries())
            {
                WriteLine(std::string("  ") + entry.Who());
                WriteLine(std::string("      ") + entry.What());
                if (!entry.Where().empty())
                {
                    WriteLine(std::string("      ") + entry.Where());
                }
            }
            WriteLine();
            WriteLine("  Metroid Prime Hunters is Nintendo's. No game data is included");
            WriteLine("  with this program: it is unpacked from your own cartridge dump.");
            WriteLine();
        }
    }
}

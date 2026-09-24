#pragma once

#include <optional>
#include <string>

namespace MphRead::Mods::Launcher
{
    // The exact seven retail Metroid Prime Hunters dumps this build will
    // extract from, checked by the ROM's own MD5 rather than its name or its
    // header's game code -- a renamed file or a hand-patched one reads both of
    // those back unchanged. See the "ROM not retail" memory: a non-retail dump
    // can pass a header check and then crash deep inside a hunter model's
    // palette, which is what this catches before extraction rather than
    // mid-match.
    class RomWhitelist final
    {
    public:
        RomWhitelist() = delete;
        ~RomWhitelist() = delete;
        RomWhitelist(const RomWhitelist&) = delete;
        RomWhitelist& operator=(const RomWhitelist&) = delete;
        RomWhitelist(RomWhitelist&&) = delete;
        RomWhitelist& operator=(RomWhitelist&&) = delete;

        // The file's MD5 as lowercase hex, or nothing when it could not be read.
        [[nodiscard]] static std::optional<std::string> Hash(const std::string& path);

        // True and the dump's label ("USA 1.1 (AMHE1)") when the file's MD5 is
        // one of the seven; false and nothing for anything else, including a
        // file that could not be read.
        [[nodiscard]] static bool TryIdentify(
            const std::string& path, std::optional<std::string>& label);
    };
}

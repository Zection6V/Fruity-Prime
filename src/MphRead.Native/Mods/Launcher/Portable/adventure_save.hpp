#pragma once

#include "GameState.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace fruityprime::launcher {

constexpr std::size_t SlotCount = 3;

struct SlotInfo {
    std::uint8_t slot = 0;
    bool used = false;
    std::string area;
    int octoliths = 0;
    int health = 0;
    int health_max = 0;

    [[nodiscard]] std::string describe() const;
};

// Portable equivalent of Mods/Launcher/Portable/AdventureSave.cs and the
// save portion of GameState.cs.  The on-disk format deliberately remains the
// managed build's PascalCase JSON so a native build can continue a save made
// by the existing C# executable, and vice versa.
class SaveStore {
public:
    explicit SaveStore(std::filesystem::path root_directory = {});

    [[nodiscard]] const std::filesystem::path& root_directory() const noexcept {
        return root_directory_;
    }
    [[nodiscard]] std::filesystem::path path_for_slot(
        std::uint8_t slot) const;
    [[nodiscard]] bool exists(std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<game::StorySave> read(
        std::uint8_t slot) const;
    [[nodiscard]] std::array<SlotInfo, SlotCount> read_all() const;

    // write() preserves values exactly.  commit() applies the same safety
    // fixes as GameState.CommitSave before writing the selected slot.
    [[nodiscard]] bool write(std::uint8_t slot,
                             const game::StorySave& save) const;
    [[nodiscard]] bool commit(std::uint8_t slot,
                              game::StorySave& save) const;

    [[nodiscard]] static std::string area_name(
        const game::StorySave& save);
    [[nodiscard]] static std::string start_room(
        const game::StorySave& save);

private:
    std::filesystem::path root_directory_;
};

// Static launcher contract from AdventureSave.cs. SaveStore remains the
// explicit native persistence adapter; this class supplies the process-wide
// ownership and Begin semantics used by the managed launcher.
class AdventureSave final {
public:
    static constexpr std::size_t SlotCount = launcher::SlotCount;
    using SlotInfo = launcher::SlotInfo;

    static void BindRuntime(std::filesystem::path root_directory,
                            game::State* state,
                            std::uint8_t* selected_save_slot) noexcept;
    [[nodiscard]] static SlotInfo Read(std::uint8_t slot);
    [[nodiscard]] static std::array<SlotInfo, SlotCount> ReadAll();
    [[nodiscard]] static std::string Begin(std::uint8_t slot,
                                           bool new_game);
    [[nodiscard]] static std::string StartRoom(
        const game::StorySave& save);
};

} // namespace fruityprime::launcher

namespace MphReadNative::Mods::Launcher {
using AdventureSave = ::fruityprime::launcher::AdventureSave;
using SlotInfo = ::fruityprime::launcher::SlotInfo;
}

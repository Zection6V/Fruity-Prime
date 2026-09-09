#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::players {

enum class DialogType : std::int8_t {
    None = -1,
    Overlay = 0,
    Hud = 1,
    Okay = 3,
    Event = 4,
    YesNo = 5,
    Scan = 6
};

enum class ConfirmState : std::uint8_t {
    No,
    Yes,
    Okay
};

enum class PromptType : std::uint8_t {
    Any,
    ShipHatch,
    GameOver
};

enum class EventType : std::uint8_t {
    EnergyTank = 0,
    VoltDriver = 1,
    MissileTank = 2,
    Battlehammer = 3,
    Imperialist = 4,
    Judicator = 5,
    Magmaul = 6,
    ShockCoil = 7,
    OmegaCannon = 8,
    Artifact = 15,
    Octolith = 16,
    UATank = 17
};

// Native counterpart of PlayerDialog.cs.  It preserves the managed dialog
// modes, page boundaries, confirmation state, and character timer while
// leaving glyph layout to the HUD backend.
class PlayerDialog final {
public:
    void show(DialogType type, std::string message, float duration_seconds,
              EventType event = EventType::EnergyTank,
              PromptType prompt = PromptType::Any) noexcept;
    void close() noexcept;
    void update(float frame_seconds) noexcept;
    void confirm() noexcept;
    void cancel() noexcept;
    void next_page() noexcept;

    // What PlayerDialog.ShowDialog / CloseDialogs need to know about the
    // player before they will do anything.  The managed code adds this gate
    // on top of the cartridge's: an entity can ask for a dialog before the
    // HUD objects exist, and the cartridge would read an uninitialized
    // pointer.
    struct DialogContext {
        // LoadFlags.Initial: the HUD has been built.
        bool initialized = false;
        // Dialogs only exist in the story; a match never shows one.
        bool single_player = false;
        bool is_main_player = false;
        // The scan visor is put away for any dialog that is not a scan, and
        // brought back when that dialog closes.
        bool scan_visor = false;
    };

    // PlayerDialog.ShowDialog.  Returns false when the gate refused it, so
    // the caller knows the dialog it asked for is not up.  A type outside
    // the known set closes whatever was showing rather than being ignored,
    // which is what the managed fall-through does.
    bool show_dialog(const DialogContext& context, DialogType type,
                     std::string message, float duration_seconds,
                     EventType event = EventType::EnergyTank,
                     PromptType prompt = PromptType::Any) noexcept;

    // PlayerDialog.CloseDialogs.  Returns true when the caller must switch
    // the visor back, which only happens when this dialog was the thing that
    // put it away.
    bool close_dialogs(const DialogContext& context) noexcept;

    // True while the scan visor is being held down by a dialog rather than
    // by the player.
    [[nodiscard]] bool silent_visor_switch() const noexcept {
        return silent_visor_switch_;
    }

    [[nodiscard]] DialogType type() const noexcept { return type_; }
    [[nodiscard]] ConfirmState confirm_state() const noexcept {
        return confirm_state_;
    }
    [[nodiscard]] PromptType prompt_type() const noexcept { return prompt_; }
    [[nodiscard]] EventType event_type() const noexcept { return event_; }
    [[nodiscard]] std::uint32_t page_index() const noexcept { return page_; }
    [[nodiscard]] std::uint32_t page_count() const noexcept { return pages_; }
    [[nodiscard]] std::string_view message() const noexcept { return message_; }
    [[nodiscard]] float timer() const noexcept { return timer_; }
    [[nodiscard]] float character_timer() const noexcept {
        return character_timer_;
    }

private:
    void rebuild_pages() noexcept;

    DialogType type_ = DialogType::None;
    ConfirmState confirm_state_ = ConfirmState::Okay;
    PromptType prompt_ = PromptType::Any;
    EventType event_ = EventType::EnergyTank;
    std::string message_;
    float timer_ = 0.0F;
    float character_timer_ = 0.0F;
    std::uint32_t page_ = 0;
    std::uint32_t pages_ = 0;
    std::array<std::size_t, 10> page_offsets_{};
    bool silent_visor_switch_ = false;
};

} // namespace fruityprime::players

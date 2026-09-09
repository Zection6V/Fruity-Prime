#include "Mods/Launcher/Gui/launcher_gui.hpp"
#include "Mods/Launcher/Gui/launcher_input_rows.hpp"
#include "Mods/Launcher/Gui/splash_view.hpp"
#include "Mods/Launcher/Gui/demo_picker_view.hpp"
#include "Mods/Launcher/Gui/pause_menu_view.hpp"
#include "Mods/Launcher/Gui/server_row.hpp"
#include "Mods/Launcher/Gui/pause_menu_window.hpp"
#include "Mods/Launcher/Gui/home_window.hpp"
#include "Mods/Launcher/Gui/home_view.hpp"
#include "Mods/Launcher/Gui/map_picker_view.hpp"
#include "Mods/Launcher/Gui/settings_view.hpp"
#include "Mods/Launcher/Gui/settings_window.hpp"
#include "Mods/Launcher/Gui/ui_capture.hpp"
#include "Mods/Launcher/Gui/launcher_rows.hpp"
#include "Formats/launcher_theme.hpp"
#include "Mods/Launcher/Gui/launcher_widgets.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        fruityprime::launcher::Selection selection;
        selection.rom_path = "fixture.nds";
        selection.room_name = "UNIT1_C0";
        fruityprime::launcher::gui::Model model(selection);

        require(model.page() == fruityprime::launcher::gui::Page::Splash,
                "launcher GUI model did not start on splash");
        require(!model.can_start(),
                "launcher GUI model started before game files were ready");
        require(model.dispatch(
                    fruityprime::launcher::gui::Action::FinishSplash),
                "first-run splash did not open game files");
        require(model.page() == fruityprime::launcher::gui::Page::GameFiles,
                "first-run splash opened the wrong page");
        require(!model.dispatch(
                    fruityprime::launcher::gui::Action::OpenSettings),
                "settings were reachable before game files setup");

        model.set_game_files_ready(true);
        require(model.dispatch(
                    fruityprime::launcher::gui::Action::OpenHome),
                "launcher home page did not open after setup");
        require(model.can_start(),
                "launcher GUI model rejected a complete selection");
        require(model.dispatch(
                    fruityprime::launcher::gui::Action::OpenMapPicker)
                    && model.page()
                        == fruityprime::launcher::gui::Page::MapPicker,
                "map picker navigation failed");
        require(model.dispatch(fruityprime::launcher::gui::Action::Back)
                    && model.page() == fruityprime::launcher::gui::Page::Home,
                "launcher GUI model Back navigation failed");
        model.set_room_name("");
        require(!model.can_start(),
                "launcher GUI model accepted an empty room");
        require(fruityprime::launcher::gui::page_name(
                    fruityprime::launcher::gui::Page::GameFiles)
                    == "game-files",
                "launcher GUI page name is wrong");
        const auto& theme = fruityprime::launcher::gui::palette();
        require(theme.ink == fruityprime::launcher::gui::Color{10, 12, 16, 255}
                    && theme.accent
                        == fruityprime::launcher::gui::Color{41, 197, 255, 255}
                    && theme.scrim
                        == fruityprime::launcher::gui::Color{10, 12, 16, 196},
                "launcher GUI palette drifted from the managed theme");
        require(fruityprime::launcher::gui::shade(theme.accent, 0.0)
                    == theme.accent
                    && fruityprime::launcher::gui::shade(theme.accent, -1.0)
                        == fruityprime::launcher::gui::Color{0, 0, 0, 255}
                    && fruityprime::launcher::gui::shade(theme.ink, 1.0)
                        == fruityprime::launcher::gui::Color{255, 255, 255, 255},
                "launcher GUI shade calculation changed");

        using fruityprime::launcher::gui::Caption;
        using fruityprime::launcher::gui::ChoiceRow;
        using fruityprime::launcher::gui::BindingType;
        using fruityprime::launcher::gui::FieldRow;
        using fruityprime::launcher::gui::FontMetrics;
        using fruityprime::launcher::gui::KeyBinding;
        using fruityprime::launcher::gui::KeyCode;
        using fruityprime::launcher::gui::KeyRow;
        using fruityprime::launcher::gui::MouseButton;
        using fruityprime::launcher::gui::MenuEntry;
        using fruityprime::launcher::gui::Note;
        using fruityprime::launcher::gui::PadRow;
        using fruityprime::launcher::gui::PlatformKey;
        using fruityprime::launcher::gui::ProgressRow;
        using fruityprime::launcher::gui::SliderRow;
        using fruityprime::launcher::gui::TrackedText;
        using fruityprime::launcher::gui::ToggleRow;
        using fruityprime::launcher::gui::UpdateBadge;
        using fruityprime::launcher::gui::WidgetKey;

        const FontMetrics metrics{
            [](std::string_view text, double size, bool bold) {
                double width = 0.0;
                for (const char character : text) {
                    width += character == ' ' ? size * 0.22
                                              : size * (bold ? 0.5 : 0.4);
                }
                return width;
            },
            [](double size) { return size + 4.0; },
        };
        require(std::abs(TrackedText::space_width(20.0, metrics) - 4.4)
                        < 0.0001
                    && std::abs(TrackedText::measure("A B", 20.0, 1.0,
                                                     metrics) - 27.4)
                        < 0.0001
                    && std::abs(TrackedText::line_height(20.0, metrics) - 24.0)
                        < 0.0001,
                "tracked text metrics changed");

        int menu_clicks = 0;
        MenuEntry menu("Join", "server", 21.0);
        menu.set_click_handler([&menu_clicks] { ++menu_clicks; });
        require(menu.height() == MenuEntry::SubtitledHeight
                    && menu.requested_width(metrics) > 0.0,
                "menu entry layout changed");
        menu.pointer_press();
        require(menu.pointer_release(10.0, 10.0, 300.0)
                    && menu_clicks == 1,
                "menu entry pointer activation changed");
        require(menu.key_press(WidgetKey::Enter) && menu_clicks == 2,
                "menu entry keyboard activation changed");
        menu.set_enabled(false);
        require(!menu.key_press(WidgetKey::Space) && menu_clicks == 2,
                "disabled menu entry activated");

        ProgressRow progress;
        progress.set(0.125, "Extracting");
        require(progress.visible() && progress.percent() == 12
                    && progress.filled_width(100.0) == 12.5,
                "progress row state changed");
        progress.set(1.5, "Done");
        require(progress.percent() == 100 && progress.filled_width(4.0) == 8.0,
                "progress row clamping changed");

        int slider_changes = 0;
        SliderRow slider("Volume", 50);
        slider.set_value_changed_handler([&slider_changes](int) {
            ++slider_changes;
        });
        slider.pointer_press(140.0, 220.0, SliderRow::Height);
        require(slider.dragging() && slider.value() == 50,
                "slider pointer start changed");
        slider.pointer_move(220.0, 220.0, SliderRow::Height);
        require(slider.value() == 100 && slider.hot(),
                "slider pointer movement changed");
        slider.pointer_release();
        require(!slider.dragging() && slider.key_press(WidgetKey::Left)
                    && slider.value() == 95 && slider_changes == 2,
                "slider keyboard adjustment changed");

        int badge_clicks = 0;
        UpdateBadge badge;
        require(!badge.visible(), "update badge was visible before Show");
        badge.show("FruityPrime.zip");
        badge.set_click_handler([&badge_clicks] { ++badge_clicks; });
        require(badge.visible() && badge.width(metrics) > 0.0
                    && badge.height(metrics) == 51.0,
                "update badge layout changed");
        badge.pointer_press();
        require(badge.pointer_release(true) && badge_clicks == 1,
                "update badge pointer activation changed");
        require(badge.key_press(WidgetKey::Space) && badge_clicks == 2,
                "update badge keyboard activation changed");

        Caption caption("display");
        require(caption.label() == "DISPLAY" && caption.height() == 26.0
                    && caption.requested_width(metrics, 1000.0) > 8.0
                    && caption.requested_width(metrics, 4.0) == 4.0,
                "caption layout changed");

        int choice_changes = 0;
        ChoiceRow choice("Room", {"Alpha", "Beta", "Gamma"}, 0);
        choice.set_changed_handler(
            [&choice_changes](int, std::string_view) { ++choice_changes; });
        const auto left = choice.left_arrow(400.0);
        const auto right = choice.right_arrow(400.0);
        require(left.x == 164.0 && right.x == 372.0
                    && choice.value_center(400.0) == 282.0,
                "choice row arrow layout changed");
        choice.pointer_move(left.x + 1.0, 10.0, 400.0);
        require(choice.left_hot() && !choice.right_hot(),
                "choice row left hover changed");
        choice.pointer_press(left.x + 1.0, 10.0, 400.0);
        require(choice.index() == 2 && choice.value() == "Gamma"
                    && choice_changes == 1,
                "choice row backward step changed");
        choice.pointer_press(right.x + 1.0, 10.0, 400.0);
        require(choice.index() == 0 && choice.value() == "Alpha"
                    && choice_changes == 2,
                "choice row wrapping step changed");
        require(choice.key_press(WidgetKey::Right)
                    && choice.key_press(WidgetKey::Space)
                    && choice.index() == 2 && choice_changes == 4,
                "choice row keyboard step changed");
        choice.set_items({}, 99);
        require(choice.index() == 0 && choice.value().empty()
                    && choice.key_press(WidgetKey::Left)
                    && choice.index() == 0 && choice_changes == 4,
                "empty choice row changed");

        int toggle_changes = 0;
        ToggleRow toggle("Fullscreen", false);
        toggle.set_changed_handler(
            [&toggle_changes](bool) { ++toggle_changes; });
        require(toggle.track(300.0).x == 256.0
                    && toggle.knob_center_x(300.0) == 266.0,
                "toggle row geometry changed");
        toggle.pointer_press();
        require(toggle.on() && toggle_changes == 1
                    && toggle.key_press(WidgetKey::Left) && !toggle.on()
                    && toggle_changes == 2,
                "toggle row activation changed");

        FieldRow field("Name", "Samus");
        field.set_value("Weavel");
        require(field.value() == "Weavel" && field.box(500.0).x == 350.0
                    && field.box_width() == FieldRow::DefaultBoxWidth,
                "field row state changed");

        const Note note("explanation");
        const Note colored("warning", theme.bad);
        require(note.text() == "explanation" && note.color() == theme.text_dim
                    && note.wraps() && colored.color() == theme.bad,
                "note row state changed");

        require(fruityprime::launcher::gui::translate_key(PlatformKey::A)
                    == KeyCode::A,
                "launcher letter translation changed");
        require(fruityprime::launcher::gui::translate_key(PlatformKey::NumPad4)
                    == KeyCode::KeyPad4,
                "launcher keypad translation changed");
        require(fruityprime::launcher::gui::translate_key(PlatformKey::OemPipe)
                    == KeyCode::Backslash
                    && !fruityprime::launcher::gui::translate_key(
                           PlatformKey::Escape).has_value(),
                "launcher punctuation translation changed");
        require(fruityprime::launcher::gui::key_name(KeyCode::D3) == "3",
                "launcher digit key name changed");
        require(fruityprime::launcher::gui::key_name(KeyCode::KeyPad4)
                    == "Key pad4",
                "launcher keypad key name changed");
        require(fruityprime::launcher::gui::key_name(KeyCode::LeftShift)
                    == "Left shift",
                "launcher modifier key name changed");
        require(fruityprime::launcher::gui::action_name("Pause")
                    == "Scoreboard",
                "launcher action name changed");

        KeyBinding key_binding{BindingType::Key, KeyCode::A,
                               MouseButton::Left};
        require(fruityprime::launcher::gui::describe_binding(key_binding)
                    == "A"
                    && fruityprime::launcher::gui::describe_binding(
                           KeyBinding{BindingType::ScrollDown,
                                      KeyCode::Unknown, MouseButton::Left})
                        == "Scroll down"
                    && fruityprime::launcher::gui::describe_binding(
                           KeyBinding{BindingType::Mouse, KeyCode::Unknown,
                                      MouseButton::Button4})
                        == "Mouse 4",
                "launcher binding descriptions changed");

        int key_rebounds = 0;
        KeyRow key_row("Move up", key_binding);
        key_row.set_rebound_handler(
            [&key_rebounds](const KeyBinding&) { ++key_rebounds; });
        require(key_row.box(400.0).x == 160.0
                    && key_row.box(400.0).width == 236.0
                    && key_row.box(400.0).height == 28.0,
                "key row box geometry changed");
        require(key_row.pointer_press(170.0, 10.0, 400.0)
                    && key_row.listening(),
                "key row did not start listening");
        require(key_row.key_press(PlatformKey::OemPlus)
                    && !key_row.listening()
                    && key_row.binding().key == KeyCode::Equal
                    && key_rebounds == 1,
                "key row keyboard rebind changed");
        require(key_row.key_press(PlatformKey::Enter)
                    && key_row.listening()
                    && key_row.key_press(PlatformKey::Escape)
                    && !key_row.listening() && key_rebounds == 2,
                "key row cancel behavior changed");
        require(key_row.key_press(PlatformKey::Space)
                    && key_row.key_press(PlatformKey::Delete)
                    && key_row.binding().key == KeyCode::Unknown
                    && key_rebounds == 3,
                "key row unbind behavior changed");
        require(key_row.pointer_press(170.0, 10.0, 400.0)
                    && key_row.pointer_press(170.0, 10.0, 400.0,
                                               MouseButton::Right)
                    && key_row.binding().type == BindingType::Mouse
                    && key_row.binding().mouse == MouseButton::Right
                    && key_rebounds == 4,
                "key row mouse rebind changed");
        require(key_row.pointer_press(170.0, 10.0, 400.0)
                    && key_row.pointer_wheel(-1.0)
                    && key_row.binding().type == BindingType::ScrollDown
                    && key_rebounds == 5,
                "key row wheel rebind changed");
        (void)key_row.key_press(PlatformKey::Enter);
        key_row.lost_focus();
        require(!key_row.listening() && key_rebounds == 5,
                "key row focus loss changed");

        using fruityprime::input::GamepadButtons;
        using fruityprime::input::PadAction;
        fruityprime::input::pad_bindings::reset();
        int pad_rebounds = 0;
        PadRow pad_row(PadAction::Shoot);
        pad_row.set_rebound_handler([&pad_rebounds] { ++pad_rebounds; });
        require(pad_row.label() == "Fire / alt attack"
                    && pad_row.description() == "RT"
                    && pad_row.box(400.0).x == 160.0,
                "pad row binding or geometry changed");
        require(pad_row.key_press(PlatformKey::Enter, GamepadButtons::A)
                    && pad_row.listening()
                    && !pad_row.poll(GamepadButtons::A)
                    && !pad_row.poll(GamepadButtons::None)
                    && pad_row.poll(GamepadButtons::A)
                    && !pad_row.listening()
                    && fruityprime::input::pad_bindings::get(PadAction::Shoot)
                        == GamepadButtons::A
                    && pad_rebounds == 1,
                "pad row baseline capture changed");
        pad_row.begin_listening(GamepadButtons::None);
        require(pad_row.poll(GamepadButtons::A | GamepadButtons::B)
                    && fruityprime::input::pad_bindings::get(PadAction::Shoot)
                        == GamepadButtons::A
                    && pad_rebounds == 2,
                "pad row first-button selection changed");
        require(pad_row.pointer_press(170.0, 10.0, 400.0,
                                      GamepadButtons::None)
                    && pad_row.listening()
                    && pad_row.key_press(PlatformKey::Delete,
                                         GamepadButtons::None)
                    && fruityprime::input::pad_bindings::get(PadAction::Shoot)
                        == GamepadButtons::None
                    && pad_rebounds == 3,
                "pad row unbind behavior changed");
        pad_row.begin_listening(GamepadButtons::None);
        require(pad_row.key_press(PlatformKey::Escape, GamepadButtons::None)
                    && !pad_row.listening() && pad_rebounds == 4,
                "pad row cancel behavior changed");
        pad_row.begin_listening(GamepadButtons::None);
        pad_row.lost_focus();
        require(!pad_row.listening() && pad_rebounds == 5,
                "pad row focus loss changed");
        fruityprime::input::pad_bindings::reset();

        using fruityprime::launcher::gui::SplashImageKind;
        std::vector<std::filesystem::path> probed_paths;
        const auto room_probe = [&probed_paths](
                                    const std::filesystem::path& path) {
            probed_paths.push_back(path);
            return path.filename() == "splash.jpg"
                || path.extension() == ".png";
        };
        fruityprime::launcher::gui::SplashView splash(
            "game-root", "launcher-root", room_probe);
        require(splash.image_kind() == SplashImageKind::Custom
                    && splash.image_path().filename() == "splash.png",
                "splash custom-image selection changed");
        splash.set_bottom_inset(0.4);
        require(splash.bottom_inset() == 0.0,
                "splash inset threshold changed");
        splash.set_bottom_inset(12.0);
        require(splash.bottom_inset() == 12.0,
                "splash inset update changed");
        splash.show_room("UNIT1_C0");
        require(splash.image_kind() == SplashImageKind::Room
                    && splash.image_path().filename() == "unit1_c0.png"
                    && splash.room_key() == "UNIT1_C0"
                    && !probed_paths.empty(),
                "splash room-image selection changed");
        const auto wash = splash.bottom_wash(640.0, 480.0);
        require(wash.x == 0.0 && wash.y == 378.0 && wash.width == 640.0
                    && wash.height == 102.0,
                "splash bottom wash geometry changed");
        const auto cover = fruityprime::launcher::gui::SplashView::cover_destination(
            640.0, 480.0, 1600.0, 900.0);
        require(std::abs(cover.x + 106.6666666667) < 0.0001
                    && cover.y == 0.0
                    && std::abs(cover.width - 853.3333333333) < 0.0001
                    && cover.height == 480.0,
                "splash cover geometry changed");
        const auto brand = fruityprime::launcher::gui::SplashView::brand_destination(
            640.0, 480.0, 400.0, 100.0, 12.0);
        require(brand.x == 24.0 && brand.y == 362.0 && brand.width == 320.0
                    && brand.height == 80.0,
                "splash corner-brand geometry changed");
        const auto title = fruityprime::launcher::gui::SplashView::title_brand_destination(
            640.0, 480.0, 400.0, 100.0);
        require(title.x == 120.0 && title.y == 170.0 && title.width == 400.0
                    && title.height == 100.0,
                "splash title-card geometry changed");

        using fruityprime::launcher::gui::DemoPickerView;
        const auto recorded = std::chrono::system_clock::now();
        std::vector<fruityprime::demo::Recording> demos{
            {"captures/second.fpdemo", "MP3 PROVING GROUND", recorded, 2048},
            {"captures/first.fpdemo", "", recorded, 1024}};
        DemoPickerView picker(std::move(demos), "native-demos");
        require(picker.entries().size() == 4
                    && picker.entries()[0].kind == DemoPickerView::EntryKind::Demo
                    && picker.entries()[0].title == "MP3 PROVING GROUND"
                    && picker.entries()[0].title_size == 15.0
                    && picker.entries()[1].title == "first.fpdemo"
                    && picker.entries()[2].kind
                        == DemoPickerView::EntryKind::Import
                    && picker.entries()[2].top_margin == 10.0
                    && picker.entries()[3].width == 120.0
                    && picker.first_focus_index() == 0,
                "demo picker entry construction changed");
        int picker_closed = 0;
        picker.set_closed_handler([&picker_closed] { ++picker_closed; });
        require(!picker.activate(8) && !picker.closed()
                    && picker.activate(0) && picker.closed()
                    && picker.path().has_value()
                    && picker.path()->filename() == "second.fpdemo"
                    && picker_closed == 1,
                "demo picker demo selection changed");

        DemoPickerView empty_picker({}, "native-demos");
        empty_picker.set_closed_handler([&picker_closed] { ++picker_closed; });
        require(empty_picker.entries().size() == 2
                    && empty_picker.empty_note().find("Nothing recorded yet.")
                        == 0
                    && empty_picker.empty_note().find("native-demos")
                        != std::string::npos
                    && empty_picker.first_focus_index() == 0
                    && empty_picker.activate(0)
                    && empty_picker.import_requested()
                    && empty_picker.closed() && picker_closed == 2,
                "empty demo picker import changed");
        std::vector<fruityprime::demo::Recording> import_demos{
            {"captures/second.fpdemo", "MP3 PROVING GROUND", recorded, 2048},
            {"captures/first.fpdemo", "", recorded, 1024}};
        DemoPickerView import_picker(std::move(import_demos), "native-demos");
        import_picker.set_closed_handler([&picker_closed] { ++picker_closed; });
        require(import_picker.activate(2) && import_picker.import_requested()
                    && import_picker.closed() && picker_closed == 3,
                "demo picker import activation changed");
        DemoPickerView escape_picker({}, "native-demos");
        require(escape_picker.key_press(WidgetKey::Escape)
                    && escape_picker.closed()
                    && !escape_picker.key_press(WidgetKey::Escape),
                "demo picker escape behavior changed");

        using fruityprime::launcher::gui::PauseMenuView;
        PauseMenuView::Options pause_options;
        pause_options.offer_window_mode = true;
        pause_options.spectator = true;
        pause_options.network_active = true;
        pause_options.recording = true;
        PauseMenuView pause(pause_options);
        require(pause.entries().size() == 7
                    && pause.entries()[0].kind == PauseMenuView::EntryKind::Resume
                    && pause.entries()[1].title == "Fullscreen"
                    && pause.entries()[2].title == "Settings"
                    && pause.entries()[3].title == "Rejoin match"
                    && pause.entries()[4].title == "Stop recording"
                    && pause.entries()[5].title == "Leave match"
                    && pause.entries()[6].title == "Quit"
                    && pause.needed_height() == 420.0
                    && pause.panel_width() == 420.0,
                "pause menu entry construction changed");
        int pause_actions = 0;
        PauseMenuView::EntryKind last_pause_action =
            PauseMenuView::EntryKind::Resume;
        pause.set_action_handler(
            [&pause_actions, &last_pause_action](PauseMenuView::EntryKind kind) {
                ++pause_actions;
                last_pause_action = kind;
            });
        require(pause.focused_index() == 0 && pause.activate(4)
                    && pause_actions == 1
                    && last_pause_action == PauseMenuView::EntryKind::RecordToggle
                    && !pause.activate(99),
                "pause menu action dispatch changed");
        pause.fit_to_host(210.0);
        require(pause.scale() == 0.5,
                "pause menu minimum scale changed");
        pause.fit_to_host(600.0);
        require(pause.scale() == 1.0,
                "pause menu maximum scale changed");
        pause.set_fullscreen(true);
        require(pause.entries()[1].title == "Windowed"
                    && pause.options().fullscreen,
                "pause menu window-mode label changed");
        PauseMenuView replay_pause({false, true, false, true, true, false, false});
        require(replay_pause.entries().size() == 4
                    && replay_pause.entries()[0].title == "Resume"
                    && replay_pause.entries()[1].title == "Settings"
                    && replay_pause.entries()[2].title == "Leave match"
                    && replay_pause.entries()[3].title == "Quit"
                    && replay_pause.needed_height() == 282.0,
                "demo playback pause menu filtering changed");

        using fruityprime::launcher::gui::ServerHeader;
        using fruityprime::launcher::gui::ServerRow;
        using fruityprime::launcher::gui::PauseWindowPlacement;
        using fruityprime::launcher::gui::PauseMenuWindow;
        using fruityprime::launcher::LaunchPlan;
        ServerRow server_row("Near server", "127.0.0.1:27888");
        require(server_row.map() == "asking..."
                    && server_row.name_color() == theme.text_dim
                    && server_row.ping().empty(),
                "server row initial state changed");
        fruityprime::net::ServerStatus online_status;
        online_status.online = true;
        online_status.room_key = "MP3 PROVING GROUND";
        online_status.mode = fruityprime::GameMode::Battle;
        online_status.players = 3;
        online_status.max_players = 8;
        online_status.latency_ms = 41;
        server_row.set_status(online_status);
        require(server_row.answered()
                    && server_row.map() == "MP3 PROVING GROUND"
                    && server_row.mode() == "Battle"
                    && server_row.players() == "3/8"
                    && server_row.ping() == "41"
                    && server_row.name_color() == theme.text
                    && server_row.ping_color() == theme.good,
                "server row online status changed");
        const auto server_layout = ServerRow::columns(400.0);
        require(server_layout.name_x == 8.0
                    && std::abs(server_layout.name_width - 88.88) < 0.0001
                    && std::abs(server_layout.map_x - 106.88) < 0.0001
                    && std::abs(server_layout.map_width - 103.12) < 0.0001
                    && server_layout.mode_x == 220.0
                    && server_layout.mode_width == 66.0
                    && server_layout.players_right == 348.0
                    && server_layout.ping_right == 392.0,
                "server row columns changed");
        const auto header_cells = ServerHeader::cells(400.0);
        require(header_cells.size() == 5
                    && header_cells[0].label == "SERVER"
                    && header_cells[3].right_aligned
                    && header_cells[4].label == "PING",
                "server header columns changed");
        int server_clicks = 0;
        server_row.set_clicked_handler([&server_clicks] { ++server_clicks; });
        require(server_row.pointer_press() && server_row.focused()
                    && server_clicks == 1
                    && server_row.key_press(WidgetKey::Space)
                    && server_clicks == 2
                    && !server_row.key_press(WidgetKey::Left),
                "server row activation changed");
        server_row.pointer_enter();
        require(server_row.hot(), "server row hover changed");
        server_row.pointer_exit();
        require(!server_row.hot(), "server row hover exit changed");
        server_row.set_status(fruityprime::net::ServerStatus::offline("timeout"));
        require(!server_row.answered() && server_row.map() == "did not answer"
                    && server_row.players().empty() && server_row.ping() == "--"
                    && server_row.ping_color() == theme.bad,
                "server row offline status changed");

        PauseMenuWindow pause_window(pause_options);
        int pause_window_closed = 0;
        pause_window.set_closed_handler(
            [&pause_window_closed] { ++pause_window_closed; });
        require(pause_window.title() == "Fruity Prime - paused"
                    && pause_window.can_resize() == false
                    && !pause_window.decorated()
                    && !pause_window.show_in_taskbar()
                    && !pause_window.open(),
                "pause window initial state changed");
        require(pause_window.show({100, 200, 1280, 720}, 2.0)
                    && pause_window.open()
                    && pause_window.placement()
                        == PauseWindowPlacement{false, 100, 200, 640.0, 360.0}
                    && pause_window.menu().focused_index() == 0,
                "pause window cover geometry changed");
        require(!pause_window.follow_game_window({100, 200, 1280, 720}, 2.0)
                    && pause_window.follow_game_window({101, 200, 1280, 720}, 2.0)
                    && pause_window.placement().x == 101,
                "pause window follow behavior changed");
        require(pause_window.open_settings() && pause_window.settings_open()
                    && !pause_window.topmost()
                    && !pause_window.open_settings(),
                "pause window settings overlay changed");
        pause_window.close_settings();
        require(!pause_window.settings_open() && pause_window.topmost(),
                "pause window settings close changed");
        require(pause_window.handle_escape() && !pause_window.open()
                    && pause_window_closed == 1
                    && !pause_window.handle_escape(),
                "pause window escape close changed");
        PauseMenuWindow centered_pause(pause_options);
        require(centered_pause.show({0, 0, 0, 0}, 0.0)
                    && centered_pause.placement().center_on_screen
                    && centered_pause.placement().width == 0.0,
                "pause window center fallback changed");

        using fruityprime::launcher::gui::HomeWindow;
        fruityprime::settings::MenuSettings home_settings;
        HomeWindow home(std::move(home_settings),
                        {"MP3 PROVING GROUND", "TEST ARENA"});
        require(home.title() == "Fruity Prime"
                    && home.rooms().size() == 2
                    && home.rooms()[1] == "TEST ARENA"
                    && home.plan().kind == fruityprime::launcher::LaunchKind::None
                    && !home.closed()
                    && home.centered() && home.decorated()
                    && HomeWindow::Width == 940.0
                    && HomeWindow::Height == 560.0
                    && HomeWindow::MinWidth == 780.0
                    && HomeWindow::MinHeight == 480.0,
                "home window frame state changed");
        int home_finished = 0;
        home.set_done_handler([&home_finished](const LaunchPlan& plan) {
            ++home_finished;
            if (plan.kind != fruityprime::launcher::LaunchKind::Offline) {
                throw std::runtime_error("home window plan was not forwarded");
            }
        });
        LaunchPlan offline_plan;
        offline_plan.kind = fruityprime::launcher::LaunchKind::Offline;
        offline_plan.room_key = "TEST ARENA";
        home.finish(offline_plan);
        require(home.closed() && home.plan().kind
                    == fruityprime::launcher::LaunchKind::Offline
                    && home_finished == 1,
                "home window plan handoff changed");
        home.finish({});
        require(home_finished == 1, "home window finished twice");
        home.reset();
        require(!home.closed()
                    && home.plan().kind == fruityprime::launcher::LaunchKind::None,
                "home window reset changed");

        using fruityprime::launcher::gui::MapPickerView;
        using fruityprime::launcher::gui::MapPickerWindow;
        using fruityprime::launcher::gui::MapTile;
        const auto preview_probe = [](const std::filesystem::path& path) {
            return path.filename() == "unit1_c0.png";
        };
        MapPickerView map_picker(
            {"UNIT1_C0", "MP3 PROVING GROUND"}, "MP3 PROVING GROUND",
            "game-root", preview_probe);
        require(map_picker.tiles().size() == 2
                    && map_picker.tiles()[0].room_key() == "UNIT1_C0"
                    && map_picker.tiles()[0].caption() == "Echo Hall"
                    && map_picker.tiles()[0].has_image()
                    && map_picker.tiles()[1].selected()
                    && map_picker.first_focus_index() == 1
                    && map_picker.tiles()[0].image_area().height == 142.0
                    && map_picker.tiles()[0].caption_area().y == 142.0,
                "map picker tile construction changed");
        int map_closed = 0;
        map_picker.set_closed_handler([&map_closed] { ++map_closed; });
        require(!map_picker.activate(9) && map_picker.activate(0)
                    && map_picker.room_key().has_value()
                    && *map_picker.room_key() == "UNIT1_C0"
                    && map_picker.closed() && map_closed == 1,
                "map picker selection changed");
        MapPickerView map_import({}, "", "game-root", preview_probe);
        require(map_import.first_focus_index() == 0
                    && map_import.key_press(WidgetKey::Escape)
                    && map_import.closed()
                    && !map_import.key_press(WidgetKey::Escape),
                "empty map picker close changed");
        MapPickerView wrapped_view({"UNIT1_C0"}, "", "game-root",
                                   preview_probe);
        MapPickerWindow map_window(std::move(wrapped_view));
        require(map_window.title() == "Choose a map"
                    && !map_window.open()
                    && map_window.centered_on_owner()
                    && map_window.decorated()
                    && MapPickerWindow::Width == 1120.0
                    && MapPickerWindow::Height == 720.0
                    && MapPickerWindow::MinWidth == 560.0
                    && MapPickerWindow::MinHeight == 420.0,
                "map picker window frame changed");
        map_window.show();
        require(map_window.open()
                    && map_window.view().activate(0)
                    && !map_window.open(),
                "map picker window close handoff changed");
        MapTile map_tile("UNIT1_C0", "game-root", preview_probe);
        int tile_clicks = 0;
        map_tile.set_clicked_handler([&tile_clicks] { ++tile_clicks; });
        require(map_tile.pointer_release(100.0, 100.0)
                    && map_tile.key_press(WidgetKey::Enter)
                    && tile_clicks == 2
                    && !map_tile.pointer_release(249.0, 100.0),
                "map tile activation changed");
        map_tile.focus();
        require(map_tile.focused() && map_tile.hover(), "map tile focus changed");
        map_tile.blur();
        require(!map_tile.focused() && !map_tile.hover(),
                "map tile focus loss changed");

        using fruityprime::launcher::gui::SettingsForm;
        using fruityprime::launcher::gui::SettingsSources;
        using fruityprime::launcher::gui::SettingsView;
        using fruityprime::launcher::gui::SettingsWindow;
        SettingsSources settings_sources;
        settings_sources.menu.language = "Japanese";
        settings_sources.menu.sfx_volume = "0.425";
        settings_sources.menu.team_play = "on";
        settings_sources.menu.features_json =
            "{\"ReticleOpacity\": \"1\", \"ProHud\": false}";
        settings_sources.preferences.player_name = "Pilot";
        settings_sources.preferences.server_address = "server.example";
        settings_sources.preferences.server_port = 27888;
        settings_sources.preferences.window_mode =
            fruityprime::window::StartMode::BorderlessFullscreen;
        settings_sources.input.mouse_sensitivity = 1.0F;
        settings_sources.input.gamepad_look_sensitivity = 1.0F;
        settings_sources.input.gamepad_dead_zone = 0.2F;
        settings_sources.render.resolution_scale = 80;
        settings_sources.render.lighting = false;
        settings_sources.features.pro_hud = false;
        SettingsView settings_view(std::move(settings_sources));
        require(settings_view.window_title() == "Fruity Prime settings"
                    && !settings_view.in_game()
                    && settings_view.section() == SettingsView::Section::Display
                    && SettingsView::section_names().size() == 6
                    && SettingsView::section_name(SettingsView::Section::Profile)
                        == "Profile"
                    && settings_view.form().language == "Japanese"
                    && settings_view.form().sfx_volume == 43
                    && settings_view.form().borderless_fullscreen
                    && settings_view.form().resolution_scale == 80,
                "settings view initialization changed");
        settings_view.set_width(720.0);
        require(!settings_view.layout().narrow
                    && settings_view.layout().rail_width == 216.0,
                "settings wide layout threshold changed");
        settings_view.set_width(719.0);
        require(settings_view.layout().narrow
                    && settings_view.layout().rail_width == 0.0
                    && settings_view.layout().rail_padding_left == 12.0,
                "settings narrow layout changed");
        require(settings_view.show_section("match RULES")
                    && settings_view.section() == SettingsView::Section::MatchRules
                    && !settings_view.show_section("missing"),
                "settings section selection changed");
        std::string parsed_host = "old";
        std::uint16_t parsed_port = 1234;
        require(SettingsView::parse_endpoint(" server.test:27900 ",
                                              parsed_host, parsed_port)
                    && parsed_host == "server.test" && parsed_port == 27900,
                "settings endpoint parsing changed");
        require(!SettingsView::parse_endpoint("server.test:0", parsed_host,
                                               parsed_port)
                    && parsed_host == "server.test" && parsed_port == 27900,
                "settings endpoint validation changed");
        SettingsForm& settings_form = settings_view.form();
        settings_form.pro_hud = true;
        settings_form.resolution_scale = 90;
        settings_form.lighting = true;
        settings_form.sfx_volume = 35;
        settings_form.server_endpoint = "native.test:28000";
        settings_form.player_name = "  New Pilot  ";
        require(settings_view.commit(), "settings view commit rejected");
        require(settings_view.saved() && settings_view.closed(),
                "settings view commit lifecycle changed");
        require(settings_view.menu_settings().resolution_scale == "90"
                    && settings_view.menu_settings().lighting == "on"
                    && settings_view.menu_settings().sfx_volume == "0.35"
                    && settings_view.menu_settings().team_play == "on",
                "settings view menu commit changed");
        require(settings_view.menu_settings().features_json.find(
                    "\"ProHud\": true") != std::string::npos,
                "settings view feature commit changed");
        require(settings_view.preferences().server_address == "native.test"
                    && settings_view.preferences().server_port == 28000
                    && settings_view.preferences().player_name == "New Pilot",
                "settings view launcher commit changed");
        require(settings_view.input_config().mouse_sensitivity > 0.09F,
                "settings view input commit changed");
        SettingsView escape_settings(fruityprime::settings::MenuSettings{});
        int settings_closed = 0;
        escape_settings.set_closed_handler([&settings_closed] {
            ++settings_closed;
        });
        require(escape_settings.handle_escape() && escape_settings.closed()
                    && settings_closed == 1
                    && !escape_settings.handle_escape(),
                "settings view escape close changed");

        SettingsWindow settings_window(
            fruityprime::settings::MenuSettings{}, true);
        require(settings_window.title() == "Fruity Prime settings"
                    && settings_window.in_game()
                    && !settings_window.can_resize()
                    && !settings_window.decorated()
                    && settings_window.topmost()
                    && !settings_window.show_in_taskbar()
                    && SettingsWindow::Width == 980.0
                    && SettingsWindow::Height == 660.0,
                "settings window frame state changed");
        int settings_window_closed = 0;
        settings_window.set_closed_handler([&settings_window_closed] {
            ++settings_window_closed;
        });
        require(settings_window.show({40, 50, 1280, 720}, 2.0)
                    && settings_window.open()
                    && settings_window.placement()
                        == fruityprime::launcher::gui::SettingsWindowPlacement{
                            false, 40, 50, 640.0, 360.0},
                "settings window cover geometry changed");
        require(settings_window.view().commit()
                    && !settings_window.open()
                    && settings_window.saved()
                    && settings_window_closed == 1,
                "settings window view close handoff changed");

        fruityprime::launcher::gui::GuiLauncher gui_launcher;
        fruityprime::launcher::gui::Environment no_display;
        require(!gui_launcher.ensure_setup(no_display)
                    && !gui_launcher.set_up()
                    && !gui_launcher.failed(),
                "GUI launcher display probe changed");
        fruityprime::launcher::gui::Environment desktop;
        desktop.windows = true;
        require(gui_launcher.ensure_setup(desktop)
                    && gui_launcher.set_up()
                    && gui_launcher.ensure_setup(no_display),
                "GUI launcher one-time setup changed");
        int gui_run = 0;
        require(gui_launcher.try_run(desktop, [&gui_run] { ++gui_run; })
                    && gui_run == 1,
                "GUI launcher run callback changed");
        require(!gui_launcher.try_run(desktop, [] {
                        throw std::runtime_error("launcher probe");
                    })
                    && gui_launcher.set_up(),
                "GUI launcher fallback changed");
        gui_launcher.pump();
        require(gui_launcher.pump_count() == 1,
                "GUI launcher pump changed");
        gui_launcher.reset();
        require(!gui_launcher.set_up() && !gui_launcher.failed()
                    && gui_launcher.pump_count() == 0,
                "GUI launcher reset changed");
        fruityprime::launcher::gui::Environment android;
        android.android = true;
        require(!gui_launcher.ensure_setup(android),
                "Android GUI launcher probe changed");
        gui_launcher.mark_setup_failed();
        require(gui_launcher.failed() && !gui_launcher.ensure_setup(desktop),
                "GUI launcher setup failure latch changed");

        using fruityprime::launcher::gui::HomeCard;
        using fruityprime::launcher::gui::HomeMatchChoice;
        using fruityprime::launcher::gui::HomeView;
        using fruityprime::launcher::LaunchKind;
        using fruityprime::game::Mode;
        HomeView setup_home(fruityprime::settings::MenuSettings{},
                            {"UNIT1_C0"}, false);
        require(setup_home.card() == HomeCard::Setup
                    && !setup_home.match_entries_enabled()
                    && !setup_home.show_card(HomeCard::Home)
                    && !setup_home.go_back(),
                "home setup gating changed");
        require(setup_home.handle_escape() && setup_home.finished()
                    && setup_home.plan().kind == LaunchKind::None,
                "home setup escape changed");
        HomeView home_view(fruityprime::settings::MenuSettings{},
                           {"UNIT1_C0", "MP3 PROVING GROUND"});
        require(home_view.card() == HomeCard::Home
                    && home_view.rooms().size() == 2
                    && home_view.version_visible()
                    && home_view.debug_visible()
                    && HomeView::mode_options().size() == 12
                    && HomeView::mode_options()[1].label == "Battle teams"
                    && HomeView::hunter_options().size() == 8
                    && HomeView::hunter_options()[7] == "Random",
                "home initial model changed");
        home_view.set_width(720.0);
        require(!home_view.layout().narrow
                    && home_view.layout().panel_width == 400.0,
                "home wide layout threshold changed");
        home_view.set_width(719.0);
        require(home_view.layout().narrow
                    && home_view.layout().panel_width == 0.0
                    && home_view.layout().splash_height == 150.0,
                "home narrow layout changed");
        require(home_view.show_card(HomeCard::Browse)
                    && home_view.layout().panel_width == 0.0
                    && !home_view.status_polling()
                    && home_view.show_card(HomeCard::Online)
                    && home_view.status_polling()
                    && !home_view.version_visible(),
                "home card selection changed");
        require(home_view.open_overlay(HomeView::SettingsOverlay)
                    && home_view.overlay() == HomeView::SettingsOverlay
                    && !home_view.status_polling()
                    && home_view.go_back() && home_view.overlay() == 0,
                "home overlay navigation changed");
        int home_view_done = 0;
        home_view.set_done_handler([&home_view_done](const LaunchPlan& plan) {
            ++home_view_done;
            if (plan.kind == LaunchKind::None) {
                throw std::runtime_error("home view plan was not forwarded");
            }
        });
        require(home_view.start_adventure(0, fruityprime::metadata::Hunter::Trace, false,
                                          false, "  Story Pilot  ")
                    && home_view.finished()
                    && home_view.plan().kind == LaunchKind::Adventure
                    && home_view.plan().save_slot == 1
                    && home_view.plan().new_game
                    && home_view.plan().hunter == fruityprime::metadata::Hunter::Trace
                    && home_view.plan().player_name == "Story Pilot"
                    && home_view_done == 1,
                "home adventure plan changed");
        home_view.reset();
        HomeMatchChoice offline_choice;
        offline_choice.kind = LaunchKind::Offline;
        offline_choice.room_key = "UNIT1_C0";
        offline_choice.mode = Mode::Nodes;
        offline_choice.hunter = fruityprime::metadata::Hunter::Samus;
        offline_choice.bots = 7;
        offline_choice.bot_level = 2;
        require(home_view.start_match(offline_choice)
                    && home_view.plan().kind == LaunchKind::Offline
                    && home_view.plan().room_key == "UNIT1_C0"
                    && home_view.plan().mode
                        == static_cast<std::uint8_t>(Mode::Nodes)
                    && home_view.plan().bots == 7
                    && home_view.plan().bot_level == 2,
                "home offline plan changed");
        home_view.reset();
        require(home_view.join("server.test", 27890,
                               fruityprime::metadata::Hunter::Noxus)
                    && home_view.plan().kind == LaunchKind::Online
                    && home_view.plan().server_address == "server.test"
                    && home_view.plan().port == 27890
                    && home_view.plan().mode
                        == static_cast<std::uint8_t>(Mode::Battle),
                "home online plan changed");
        home_view.reset();
        require(home_view.play_demo("recordings/test.fpdemo")
                    && home_view.plan().kind == LaunchKind::Demo
                    && home_view.plan().demo_path == "recordings/test.fpdemo",
                "home demo plan changed");

        using fruityprime::launcher::gui::UiCapture;
        using fruityprime::launcher::gui::UiCaptureScreen;
        const auto capture_screens = UiCapture::screens({"UNIT1_C0"});
        const auto empty_capture_screens = UiCapture::screens({});
        require(capture_screens.size() == 9
                    && capture_screens[0].name == "home"
                    && capture_screens[3].name == "mappicker"
                    && capture_screens[7].name == "pausemenu-small"
                    && capture_screens[7].size == UiCapture::SmallPauseSize
                    && empty_capture_screens.size() == 8
                    && empty_capture_screens[3].name == "demopicker",
                "UI capture screen list changed");
        const auto capture_directory = std::filesystem::temp_directory_path()
            / "fruity-prime-ui-capture-tests";
        std::error_code capture_cleanup_error;
        std::filesystem::remove_all(capture_directory,
                                     capture_cleanup_error);
        int captured_screens = 0;
        require(UiCapture::run(
                    capture_directory, {"UNIT1_C0"},
                    [&captured_screens](const UiCaptureScreen& screen,
                                        const std::filesystem::path& path) {
                        ++captured_screens;
                        return path.filename() == screen.name + ".png";
                    }) == 0
                    && captured_screens == 9,
                "UI capture run contract changed");
        require(UiCapture::run(capture_directory, {},
                               [](const UiCaptureScreen&, const auto&) {
                                   return false;
                               }) == 1
                    && UiCapture::run(capture_directory, {}, {}, false) == 1,
                "UI capture failure fallback changed");
        std::filesystem::remove_all(capture_directory, capture_cleanup_error);

        std::cout << "native launcher GUI tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native launcher GUI tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}

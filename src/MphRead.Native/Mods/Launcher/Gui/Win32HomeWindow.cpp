// Win32 head for the launcher front screen.  See win32_home_window.hpp.
#include "Mods/Launcher/Gui/win32_home_window.hpp"

#include "Formats/launcher_theme.hpp"
#include "Mods/Launcher/Gui/home_view.hpp"
#include "Mods/Launcher/Gui/home_window.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/branding.hpp"
#include "Mods/settings.hpp"
#include "Entities/room_catalog.hpp"
#include "Metadata/metadata_lookup.hpp"
#include "Mods/Network/master_client.hpp"
#include "Mods/Network/net_status.hpp"
#include "Mods/Network/demo_library.hpp"
#include "Mods/thumbnail_generator.hpp"
#include "Mods/Network/pad_bindings.hpp"
#include "Mods/render_options.hpp"
#include "Mods/Launcher/Portable/game_files.hpp"
#include "Mods/credits.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <wincodec.h>
#include <windowsx.h>
#endif

namespace fruityprime::launcher::gui::win32 {

#ifdef _WIN32

// The save slots and the room list the host card reads.
using launcher::SaveStore;
using launcher::SlotCount;
using launcher::SlotInfo;


namespace {

[[nodiscard]] COLORREF to_ref(Color color) noexcept {
    return RGB(color.red, color.green, color.blue);
}

// One clickable thing on the card, in window coordinates.
struct Hotspot final {
    RECT bounds{};
    int id = 0;
    bool enabled = true;
};

// A row that steps forward carries its own id; its back arrow carries the
// same id plus this, so one hit test answers both halves.
constexpr int BackArrowBias = 1000;

enum ControlId : int {
    ChooseRomControl = 1,
    HostControl = 2,
    JoinControl = 3,
    DemosControl = 4,
    SettingsControl = 5,
    QuitControl = 6,
    BackControl = 7,
    // Host card.
    HostModeControl = 20,
    HostCoopControl = 21,
    AdventureSlotControl = 22,
    AdventureHunterControl = 23,
    AdventureStartControl = 24,
    AdventureNewControl = 25,
    HostWhereControl = 26,
    MatchMapControl = 27,
    BrowseMapsControl = 28,
    // Join.
    BrowseRefreshControl = 40,
    OnlineHunterControl = 41,
    OnlineAddressControl = 42,
    ConnectControl = 43,
    PickerBackControl = 50,
    DemoImportControl = 51,
    // Settings: the rail, the footer, and one id per row.
    SettingsSaveControl = 60,
    SettingsCancelControl = 61,
    SettingsResetControl = 62,
    SettingsFilesControl = 63,
    SettingsSectionControl = 70,
    SettingsRowControl = 100'000,
    // The server rows are numbered from here, one per listing.
    ServerRowControl = 200,
    // The picker tiles and demo rows, one id each.
    PickerItemControl = 1'000'000,
    MatchModeControl = 29,
    MatchHunterControl = 30,
    MatchBotsControl = 31,
    MatchSkillControl = 32,
    MatchStartControl = 33,
};

// A decoded 32-bit premultiplied image, ready for AlphaBlend.
class Image final {
public:
    Image() = default;
    ~Image() { reset(); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    [[nodiscard]] bool valid() const noexcept { return bitmap_ != nullptr; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] HBITMAP bitmap() const noexcept { return bitmap_; }

    void reset() noexcept {
        if (bitmap_ != nullptr) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
        }
        width_ = 0;
        height_ = 0;
    }

    // WIC decodes straight to premultiplied BGRA, which is what AlphaBlend
    // wants; converting to 24-bit and compositing by hand would lose the
    // logo's cut-out edge against the panel behind it.
    [[nodiscard]] bool load(const std::filesystem::path& path) {
        reset();
        // WIC is COM, and the launcher runs before anything else in
        // the process has initialized it.  Apartment-threaded because
        // this is the thread that owns the window; RPC_E_CHANGED_MODE
        // means somebody already did it, which is equally fine.
        const HRESULT com = CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool owns_com = SUCCEEDED(com);
        IWICImagingFactory* factory = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory)))) {
            if (owns_com) {
                CoUninitialize();
            }
            return false;
        }
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        bool ok = false;
        if (SUCCEEDED(factory->CreateDecoderFromFilename(
                path.wstring().c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnLoad, &decoder))
            && SUCCEEDED(decoder->GetFrame(0, &frame))
            && SUCCEEDED(factory->CreateFormatConverter(&converter))
            && SUCCEEDED(converter->Initialize(
                frame, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0,
                WICBitmapPaletteTypeCustom))) {
            UINT width = 0;
            UINT height = 0;
            if (SUCCEEDED(frame->GetSize(&width, &height)) && width > 0
                && height > 0 && width <= 8192 && height <= 8192) {
                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(info.bmiHeader);
                info.bmiHeader.biWidth = static_cast<LONG>(width);
                // Negative height: a top-down DIB, so the rows arrive in the
                // order WIC produces them.
                info.bmiHeader.biHeight = -static_cast<LONG>(height);
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;
                void* pixels = nullptr;
                const HDC screen = GetDC(nullptr);
                bitmap_ = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                           &pixels, nullptr, 0);
                ReleaseDC(nullptr, screen);
                if (bitmap_ != nullptr && pixels != nullptr
                    && SUCCEEDED(converter->CopyPixels(
                        nullptr, width * 4, width * height * 4,
                        static_cast<BYTE*>(pixels)))) {
                    width_ = static_cast<int>(width);
                    height_ = static_cast<int>(height);
                    ok = true;
                }
            }
        }
        if (converter != nullptr) { converter->Release(); }
        if (frame != nullptr) { frame->Release(); }
        if (decoder != nullptr) { decoder->Release(); }
        factory->Release();
        if (owns_com) {
            CoUninitialize();
        }
        if (!ok) {
            reset();
        }
        return ok;
    }

private:
    HBITMAP bitmap_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

// Look for an asset beside the program first, then in the source tree, so a
// build run out of the repository finds the same files a shipped one carries.
[[nodiscard]] std::filesystem::path find_asset(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& relative,
    const std::filesystem::path& source_relative) {
    const std::filesystem::path candidates[] = {
        executable_directory / relative,
        executable_directory / relative.filename(),
        executable_directory / ".." / ".." / source_relative,
        executable_directory / ".." / ".." / ".." / source_relative,
    };
    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }
    return {};
}

// GuiTheme.Display is Inter, and the managed launcher gets it from Avalonia's
// font package.  Here the two weights are loaded privately: the program never
// installs a font on the machine it runs on, and a build without the files
// falls back to whatever CreateFont substitutes rather than failing.
class PrivateFonts final {
public:
    void load(const std::filesystem::path& executable_directory) {
        for (const char* name : {"Inter-Regular.ttf", "Inter-SemiBold.ttf"}) {
            const auto path = find_asset(
                executable_directory,
                std::filesystem::path("Assets") / "fonts" / name,
                std::filesystem::path("assets") / "fonts" / name);
            if (path.empty()) {
                continue;
            }
            if (AddFontResourceExA(path.string().c_str(), FR_PRIVATE,
                                   nullptr) != 0) {
                loaded_.push_back(path.string());
            }
        }
    }

    ~PrivateFonts() {
        for (const std::string& path : loaded_) {
            RemoveFontResourceExA(path.c_str(), FR_PRIVATE, nullptr);
        }
    }

    PrivateFonts() = default;
    PrivateFonts(const PrivateFonts&) = delete;
    PrivateFonts& operator=(const PrivateFonts&) = delete;

private:
    std::vector<std::string> loaded_;
};

// Where the wordmark can be: beside the program, and -- for a build run out of
// the source tree -- beside the managed assets it is shared with.
[[nodiscard]] std::filesystem::path find_brand(
    const std::filesystem::path& executable_directory) {
    return find_asset(
        executable_directory,
        std::filesystem::path("Assets") / "fruity-prime-logo.png",
        std::filesystem::path("src") / "MphRead" / "Assets"
            / "fruity-prime-logo.png");
}

[[nodiscard]] std::string pick_rom(HWND owner) {
    std::vector<char> buffer(MAX_PATH * 4, '\0');
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        "Nintendo DS ROM (*.nds)\0*.nds\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrTitle = "Your Metroid Prime Hunters cartridge dump";
    if (GetOpenFileNameA(&dialog) == FALSE) {
        return {};
    }
    return buffer.data();
}

// GuiTheme.Display is Inter; Segoe UI is the closest thing a Windows box is
// guaranteed to have, and CreateFont falls back to it on its own when Inter is
// not installed.
[[nodiscard]] HFONT make_font(int pixel_size, bool bold) {
    return CreateFontA(-pixel_size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, FF_DONTCARE, "Inter");
}

void fill(HDC dc, const RECT& area, Color color) {
    const HBRUSH brush = CreateSolidBrush(to_ref(color));
    FillRect(dc, &area, brush);
    DeleteObject(brush);
}

[[nodiscard]] RECT make_rect(int left, int top, int width, int height) {
    return RECT{left, top, left + width, top + height};
}

// MenuEntry.RenderPrimary: a filled rounded rectangle with the label centred
// in it, tracked out by two pixels.
void draw_primary(HDC dc, const RECT& body, const std::string& label,
                  HFONT font, bool hot, bool pressed, bool enabled = true) {
    const Palette& theme = palette();
    const Color background = !enabled ? theme.panel_light
        : pressed ? shade(theme.accent, -0.25)
        : hot ? shade(theme.accent, 0.18) : theme.accent;
    const HBRUSH brush = CreateSolidBrush(to_ref(background));
    const HGDIOBJ old_brush = SelectObject(dc, brush);
    const HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    // The +1s are because RoundRect excludes its right and bottom edge.
    RoundRect(dc, body.left, body.top, body.right + 1, body.bottom + 1, 10, 10);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);

    const HGDIOBJ old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? RGB(8, 12, 18)
                            : to_ref(palette().text_dim));
    SetTextCharacterExtra(dc, 2);
    SIZE extent{};
    GetTextExtentPoint32A(dc, label.c_str(), static_cast<int>(label.size()),
                          &extent);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    // The trailing tracking is not part of the mark on the page, so it is
    // taken back off before centring -- otherwise the label sits a pixel left.
    const int width = static_cast<int>(extent.cx) - 2;
    TextOutA(dc,
             static_cast<int>(body.left)
                 + (static_cast<int>(body.right - body.left) - width) / 2,
             static_cast<int>(body.top)
                 + (static_cast<int>(body.bottom - body.top)
                    - static_cast<int>(metrics.tmHeight)) / 2,
             label.c_str(), static_cast<int>(label.size()));
    SetTextCharacterExtra(dc, 0);
    SelectObject(dc, old_font);
}

// MenuEntry.Render for a plain row: an accent bar down the left, the title
// tracked out by one, and a hover fill.
void draw_entry(HDC dc, const RECT& body, const std::string& title,
                const std::string& subtitle, HFONT title_font,
                HFONT subtitle_font, bool hot, bool enabled) {
    const Palette& theme = palette();
    if (hot && enabled) {
        fill(dc, body, theme.panel_light);
    }
    RECT bar = body;
    bar.right = bar.left + 3;
    fill(dc, bar, hot && enabled ? theme.accent : Color{48, 56, 72, 255});

    const int text_left = static_cast<int>(body.left) + 3 + 14;
    SetBkMode(dc, TRANSPARENT);
    const HGDIOBJ old_font = SelectObject(dc, title_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    const int top = subtitle.empty()
        ? static_cast<int>(body.top)
              + (static_cast<int>(body.bottom - body.top)
                 - static_cast<int>(metrics.tmHeight)) / 2
        : static_cast<int>(body.top) + 8;
    SetTextColor(dc, to_ref(!enabled ? theme.text_dim
                                     : hot ? theme.accent : theme.text));
    SetTextCharacterExtra(dc, 1);
    TextOutA(dc, text_left, top, title.c_str(),
             static_cast<int>(title.size()));
    SetTextCharacterExtra(dc, 0);
    if (!subtitle.empty()) {
        SelectObject(dc, subtitle_font);
        SetTextColor(dc, to_ref(theme.text_dim));
        TextOutA(dc, text_left - 1,
                 top + static_cast<int>(metrics.tmHeight) + 1,
                 subtitle.c_str(), static_cast<int>(subtitle.size()));
    }
    SelectObject(dc, old_font);
}

// Rows.Caption: the small dim label with a hairline under it.
int draw_caption(HDC dc, int left, int top, int width,
                 const std::string& text, HFONT font) {
    constexpr int Height = 26;
    const Palette& theme = palette();
    const HGDIOBJ old_font = SelectObject(dc, font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(theme.text_dim));
    TextOutA(dc, left, top + Height - static_cast<int>(metrics.tmHeight) - 4,
             text.c_str(),
             static_cast<int>(text.size()));
    SelectObject(dc, old_font);
    RECT rule = make_rect(left, top + Height - 2, width, 1);
    fill(dc, rule, theme.edge);
    return Height;
}

// Rows.Note: dim wrapped body text with a four-pixel margin.
int draw_note(HDC dc, int left, int top, int width, const std::string& text,
              HFONT font, Color color) {
    const HGDIOBJ old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(color));
    RECT area = make_rect(left + 4, top + 4, std::max(width - 8, 1), 1);
    const int height = DrawTextA(dc, text.c_str(),
                                 static_cast<int>(text.size()), &area,
                                 DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    area = make_rect(left + 4, top + 4, std::max(width - 8, 1), height);
    DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &area,
              DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old_font);
    return height + 8;
}

// The server list, shared with the threads that fill it in.
//
// Every probe is its own request and answers whenever it answers, so the list
// is written from several threads and read by the one that paints.  The
// window is asked to repaint rather than painted from a worker.
struct Browse final {
    struct Row {
        std::string name;
        std::string endpoint;
        std::string address;
        std::uint16_t port = 0;
        bool answered = false;
        // Until the probe answers the row says what it is doing.
        std::string map = "asking...";
        std::string mode;
        std::string players;
        std::string ping;
        int latency = -1;
    };

    std::mutex lock;
    std::vector<Row> rows;
    std::string note;
    Color note_color = palette().text_dim;
    // Bumped on every reload so an answer from the previous one is dropped
    // rather than written into the new list.
    int generation = 0;
};

struct Screen final {
    std::filesystem::path directory;
    Selection selection;
    HomeView view;
    Image brand;
    HFONT caption_font = nullptr;
    HFONT note_font = nullptr;
    HFONT entry_font = nullptr;
    HFONT small_entry_font = nullptr;
    HFONT primary_font = nullptr;
    HFONT title_font = nullptr;
    HFONT row_font = nullptr;
    HFONT row_bold_font = nullptr;
    HFONT picker_title_font = nullptr;

    // The host card's rows, in the managed order.  Adventure is index 0
    // because that is what Host opens on.
    int host_mode = 0;
    bool host_coop = false;
    int adventure_slot = 0;
    int adventure_hunter = 0;
    int host_where = 0;
    int match_map = 0;
    int match_mode = 0;
    int match_hunter = 0;
    int match_bots = 0;
    int match_skill = 1;
    std::vector<std::string> rooms;
    std::array<SlotInfo, SlotCount> slots{};

    // Join.  The address is typed into, so it is a string rather than an
    // index, and the caret is where the next character lands.
    std::string online_address;
    std::size_t caret = 0;
    // The control id of the field taking keystrokes, or zero.  One at a time,
    // like every other focus in the window.
    int focused_field = 0;
    int online_hunter = 0;
    std::shared_ptr<Browse> browse;

    // The full-window pickers.  Zero is none; the two share the scroll offset
    // because only one is ever up.
    HomeView::Overlay overlay = 0;
    int scroll = 0;
    // Settings: a working copy of everything the pages edit, committed only
    // when Save is pressed.  Cancel leaves the files alone.
    int settings_section = 0;
    settings::MenuSettings menu{};
    settings::InputConfig input{};
    settings::RenderConfig render{};
    Preferences prefs{};
    int window_mode = 0;
    bool pro_hud = false;
    std::string field_point_goal;
    std::string field_time_limit;
    std::string field_player_name;
    std::string field_server;
    std::string field_master;
    int damage_row = 0;
    int language_row = 0;
    int hunter_row = 0;
    std::string save_error;
    // The pad row waiting for a button, or -1.
    int listening_pad = -1;
    // The slider row the pointer is dragging, as a settings row index, or -1.
    // A slider is set by where it is grabbed rather than stepped, so it wants
    // the pointer rather than a press on a row.
    int dragging_slider = -1;
    // Opened only while a row is listening: the pad is not polled for the
    // rest of the launcher's life.
    std::unique_ptr<input::Gamepad> pad;
    input::GamepadButtons pad_seen = input::GamepadButtons::None;
    int scroll_extent = 0;
    std::vector<demo::Recording> demos;
    // Decoded lazily, and only for the tiles that have a picture: a full set
    // is thirty 1600x900 PNGs.
    std::map<std::string, std::shared_ptr<Image>> previews;
    std::vector<Hotspot> hotspots;
    int hovered = 0;
    int pressed = 0;
    bool accepted = false;
    bool open_selection = false;

    // Unpacking a cartridge reads sixteen megabytes and writes a few thousand
    // files, which is far too long to hold the message loop for.  It runs on a
    // thread of its own and reports through this, which the paint reads.
    struct Setup {
        std::mutex lock;
        std::vector<std::string> lines;
        bool running = false;
        bool finished = false;
        bool ok = false;
        std::string game_root;
    };
    std::shared_ptr<Setup> setup = std::make_shared<Setup>();
    std::thread setup_thread;

    Screen(std::filesystem::path directory_in, Selection selection_in,
           bool files_ready)
        : directory(std::move(directory_in)),
          selection(std::move(selection_in)),
          view(settings::MenuSettings{}, std::vector<std::string>{},
               files_ready) {}

    ~Screen() {
        // The worker writes into a shared block that outlives it, so the only
        // thing that has to be waited for is the thread itself.
        if (setup_thread.joinable()) {
            setup_thread.join();
        }
    }

    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
};

// What a position from 0 to 100 means to each slider.  The stored form
// differs per row -- a percentage string, a raw scale, a multiplier -- so the
// conversion lives with the row rather than with the drawing.
void apply_slider(Screen& screen, int index, int percent);

// Rows.ChoiceRow: a label, the current answer in a fixed column, and an arrow
// on each side.
//
// The value gets a column of its own rather than pushing the arrows apart.
// Placing the back arrow from the width of the value made it slide as you
// stepped -- the button jumped out from under the pointer between one map and
// the next, so the second click landed on the row and stepped forward again.
int draw_choice(HDC dc, int left, int top, int width, const std::string& label,
                const std::string& value, Screen& screen, int id,
                bool enabled = true) {
    constexpr int Height = 34;
    constexpr int ArrowWidth = 28;
    constexpr int ValueColumn = 180;
    const Palette& theme = palette();
    const int left_arrow_x =
        std::max(left + 110, left + width - ArrowWidth - ValueColumn
                                 - ArrowWidth);
    const int right_arrow_x = left + width - ArrowWidth;

    SetBkMode(dc, TRANSPARENT);
    const HGDIOBJ old_font = SelectObject(dc, screen.row_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetTextColor(dc, to_ref(theme.text_dim));
    TextOutA(dc, left + 4,
             top + (Height - static_cast<int>(metrics.tmHeight)) / 2,
             label.c_str(), static_cast<int>(label.size()));

    SelectObject(dc, screen.row_bold_font);
    SetTextColor(dc, to_ref(enabled ? theme.text : theme.text_dim));
    RECT column = make_rect(left_arrow_x + ArrowWidth + 4,
                            top + (Height - static_cast<int>(metrics.tmHeight))
                                / 2,
                            std::max(right_arrow_x - left_arrow_x - ArrowWidth
                                     - 8, 20),
                            static_cast<int>(metrics.tmHeight));
    // Trimmed to the column with an ellipsis rather than allowed to run under
    // the arrows.
    DrawTextA(dc, value.c_str(), static_cast<int>(value.size()), &column,
              DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old_font);

    const auto arrow = [&](int x, bool points_left, bool hot) {
        const int cx = x + ArrowWidth / 2;
        const int cy = top + Height / 2;
        const POINT points[3] = {
            {points_left ? cx + 5 : cx - 5, cy - 6},
            {points_left ? cx - 5 : cx + 5, cy},
            {points_left ? cx + 5 : cx - 5, cy + 6},
        };
        const HBRUSH brush = CreateSolidBrush(
            to_ref(hot && enabled ? theme.accent : theme.text_dim));
        const HGDIOBJ old_brush = SelectObject(dc, brush);
        const HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, points, 3);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(brush);
    };
    arrow(left_arrow_x, true, screen.hovered == id + BackArrowBias);
    arrow(right_arrow_x, false, screen.hovered == id);

    // The back arrow first, then everything else: anywhere that is not the
    // back arrow steps forward, so the row can be poked at without aiming.
    screen.hotspots.push_back(Hotspot{
        make_rect(left_arrow_x, top, ArrowWidth, Height),
        id + BackArrowBias, enabled});
    screen.hotspots.push_back(Hotspot{
        make_rect(left, top, width, Height), id, enabled});
    return Height;
}

// Rows.ToggleRow: a label and a switch.
int draw_toggle(HDC dc, int left, int top, int width, const std::string& label,
                bool on, Screen& screen, int id) {
    constexpr int Height = 34;
    constexpr int TrackWidth = 40;
    constexpr int TrackHeight = 20;
    const Palette& theme = palette();
    SetBkMode(dc, TRANSPARENT);
    const HGDIOBJ old_font = SelectObject(dc, screen.row_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetTextColor(dc, to_ref(theme.text_dim));
    TextOutA(dc, left + 4,
             top + (Height - static_cast<int>(metrics.tmHeight)) / 2,
             label.c_str(), static_cast<int>(label.size()));
    SelectObject(dc, old_font);

    const int track_x = left + width - TrackWidth - 4;
    const int track_y = top + (Height - TrackHeight) / 2;
    const HBRUSH track = CreateSolidBrush(to_ref(on ? theme.accent
                                                    : theme.edge));
    HGDIOBJ old_brush = SelectObject(dc, track);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, track_x, track_y, track_x + TrackWidth + 1,
              track_y + TrackHeight + 1, TrackHeight, TrackHeight);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(track);

    const int knob_x = on ? track_x + TrackWidth - TrackHeight / 2
                          : track_x + TrackHeight / 2;
    const int knob_y = track_y + TrackHeight / 2;
    constexpr int KnobRadius = TrackHeight / 2 - 3;
    const HBRUSH knob = CreateSolidBrush(to_ref(on ? theme.ink
                                                   : theme.text_dim));
    old_brush = SelectObject(dc, knob);
    old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, knob_x - KnobRadius, knob_y - KnobRadius,
            knob_x + KnobRadius, knob_y + KnobRadius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(knob);

    screen.hotspots.push_back(Hotspot{
        make_rect(left, top, width, Height), id, true});
    return Height;
}

// SplashView.Render with no map picture: the panel, the wordmark across the
// middle, and the wash along the bottom.
void draw_splash(HDC dc, const RECT& body, Screen& screen) {
    const Palette& theme = palette();
    fill(dc, body, theme.ink);
    const int panel_width = static_cast<int>(body.right - body.left);
    const int panel_height = static_cast<int>(body.bottom - body.top);
    const int cx = static_cast<int>(body.left) + panel_width / 2;
    const int cy = static_cast<int>(body.top) + panel_height / 2 - 20;
    if (screen.brand.valid() && panel_width > 40) {
        // Fitted to a band of the panel and never blown up past its own
        // resolution: a wordmark upscaled past its source pixels looks soft.
        const double max_width = std::min(
            static_cast<double>(panel_width) * 0.72,
            static_cast<double>(screen.brand.width()));
        const double scale = max_width / static_cast<double>(
            screen.brand.width());
        const int width = static_cast<int>(std::lround(max_width));
        const int height = static_cast<int>(std::lround(
            static_cast<double>(screen.brand.height()) * scale));
        const HDC source = CreateCompatibleDC(dc);
        const HGDIOBJ old = SelectObject(source, screen.brand.bitmap());
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        SetStretchBltMode(dc, HALFTONE);
        AlphaBlend(dc, cx - width / 2, cy - height / 2, width, height, source,
                   0, 0, screen.brand.width(), screen.brand.height(), blend);
        SelectObject(source, old);
        DeleteDC(source);
    } else if (panel_width > 40) {
        // Only reachable when the asset is missing.  A word is still a screen.
        const HGDIOBJ old_font = SelectObject(dc, screen.title_font);
        std::string title(branding::Name);
        for (char& letter : title) {
            letter = static_cast<char>(std::toupper(
                static_cast<unsigned char>(letter)));
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, to_ref(theme.text));
        SIZE extent{};
        GetTextExtentPoint32A(dc, title.c_str(),
                              static_cast<int>(title.size()), &extent);
        TextOutA(dc, cx - static_cast<int>(extent.cx) / 2,
                 cy - static_cast<int>(extent.cy) / 2, title.c_str(),
                 static_cast<int>(title.size()));
        SelectObject(dc, old_font);
    }

    // The wash: ninety pixels of ink fading up from the bottom edge, so a
    // caption over a bright picture stays readable.  One premultiplied strip
    // stretched across is a single blit rather than ninety.
    constexpr int WashHeight = 90;
    if (panel_height > WashHeight && panel_width > 0) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = 1;
        info.bmiHeader.biHeight = -WashHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        const HBITMAP strip = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
                                               &pixels, nullptr, 0);
        if (strip != nullptr && pixels != nullptr) {
            auto* rows = static_cast<std::uint8_t*>(pixels);
            for (int row = 0; row < WashHeight; ++row) {
                // Row 0 is the top of the strip, where the wash is clear.
                const double t = static_cast<double>(row)
                    / static_cast<double>(WashHeight - 1);
                const auto alpha = static_cast<std::uint8_t>(
                    std::lround(220.0 * t));
                const auto premultiply = [alpha](std::uint8_t channel) {
                    return static_cast<std::uint8_t>(
                        static_cast<int>(channel) * alpha / 255);
                };
                rows[row * 4 + 0] = premultiply(theme.ink.blue);
                rows[row * 4 + 1] = premultiply(theme.ink.green);
                rows[row * 4 + 2] = premultiply(theme.ink.red);
                rows[row * 4 + 3] = alpha;
            }
            const HDC source = CreateCompatibleDC(dc);
            const HGDIOBJ old = SelectObject(source, strip);
            BLENDFUNCTION blend{};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat = AC_SRC_ALPHA;
            AlphaBlend(dc, static_cast<int>(body.left),
                       static_cast<int>(body.bottom) - WashHeight, panel_width,
                       WashHeight, source, 0, 0, 1, WashHeight, blend);
            SelectObject(source, old);
            DeleteDC(source);
        }
        if (strip != nullptr) {
            DeleteObject(strip);
        }
    }
}

// Defined below, next to the other cards.
void draw_host_card(HDC dc, int left, int width, int& top,
                    Screen& screen);
void draw_browse_card(HDC dc, int left, int width, int& top, Screen& screen);
void draw_online_card(HDC dc, int left, int width, int& top, Screen& screen);
void draw_map_picker(HDC dc, const RECT& client, Screen& screen);
void draw_demo_picker(HDC dc, const RECT& client, Screen& screen);
void draw_settings(HDC dc, const RECT& client, Screen& screen);

void draw_card(HDC dc, const RECT& body, Screen& screen) {
    const Palette& theme = palette();
    fill(dc, body, theme.panel);
    // HomeView's card padding.
    constexpr int PadLeft = 22;
    constexpr int PadTop = 20;
    constexpr int PadRight = 22;
    constexpr int Spacing = 2;
    const int left = static_cast<int>(body.left) + PadLeft;
    const int width = std::max(static_cast<int>(body.right) - PadRight - left, 1);
    int top = static_cast<int>(body.top) + PadTop;

    if (screen.view.card() == HomeCard::Host) {
        draw_host_card(dc, left, width, top, screen);
        return;
    }
    if (screen.view.card() == HomeCard::Browse) {
        draw_browse_card(dc, left, width, top, screen);
        return;
    }
    if (screen.view.card() == HomeCard::Online) {
        draw_online_card(dc, left, width, top, screen);
        return;
    }
    if (screen.view.card() == HomeCard::Setup) {
        top += draw_caption(dc, left, top, width, "GAME FILES",
                            screen.caption_font) + Spacing;
        top += draw_note(dc, left, top, width,
                         std::string(branding::Name)
                             + " needs your own Metroid Prime Hunters "
                               "cartridge dump. It unpacks what it needs next "
                               "to this program and leaves the file alone. No "
                               "game data is included in this download, and "
                               "none is downloaded.",
                         screen.note_font, theme.text_dim) + Spacing;
        bool running = false;
        std::vector<std::string> lines;
        {
            const std::lock_guard<std::mutex> guard(screen.setup->lock);
            running = screen.setup->running;
            lines = screen.setup->lines;
        }
        constexpr int PrimaryHeight = 44;
        const RECT choose = make_rect(left, top, width, PrimaryHeight);
        draw_primary(dc, choose,
                     running ? "UNPACKING..." : "CHOOSE YOUR .NDS FILE",
                     screen.primary_font,
                     !running && screen.hovered == ChooseRomControl,
                     screen.pressed == ChooseRomControl, !running);
        screen.hotspots.push_back(Hotspot{choose, ChooseRomControl, !running});
        top += PrimaryHeight + Spacing;
        if (!screen.view.game_files_problem().empty()) {
            top += draw_note(dc, left, top, width,
                             screen.view.game_files_problem(),
                             screen.note_font, theme.bad) + Spacing;
        }
        // SetupProgress: the last handful of lines, oldest at the top.  The
        // whole log is not worth a scrollbar on a screen nobody sees twice,
        // and the line that matters is always the newest one.
        if (!lines.empty()) {
            constexpr std::size_t Shown = 8;
            const std::size_t first =
                lines.size() > Shown ? lines.size() - Shown : 0;
            for (std::size_t i = first; i < lines.size(); ++i) {
                top += draw_note(dc, left, top, width, lines[i],
                                 screen.note_font,
                                 i + 1 == lines.size() ? theme.text
                                                       : theme.text_dim);
            }
            top += Spacing;
        }
        return;
    }

    // HomeView.BuildHomeCard: five rows, in this order, with no subtitles.
    // "Join" does not need a line saying that it joins; the only subtitles
    // anywhere are the ones reporting something the player could not
    // otherwise know.
    constexpr int PlainHeight = 42;
    const struct { const char* label; int id; } entries[] = {
        {"HOST", HostControl},
        {"JOIN", JoinControl},
        {"DEMOS", DemosControl},
        {"SETTINGS", SettingsControl},
        {"QUIT", QuitControl},
    };
    for (const auto& entry : entries) {
        const RECT row = make_rect(left, top, width, PlainHeight);
        const bool enabled = screen.view.match_entries_enabled()
            || entry.id == QuitControl;
        draw_entry(dc, row, entry.label, "", screen.entry_font,
                   screen.note_font, screen.hovered == entry.id, enabled);
        screen.hotspots.push_back(Hotspot{row, entry.id, enabled});
        top += PlainHeight + Spacing;
    }
}



// The chrome both full-window pickers share: a title bar with a Back entry on
// the right, and a scrolling body under it.  Shown over the front screen
// rather than in a window of its own -- the desktop and Android both show it
// that way, and neither should have a copy of the list.
int draw_picker_header(HDC dc, const RECT& client, const std::string& title,
                       Screen& screen) {
    const Palette& theme = palette();
    constexpr int Height = 48;
    const int width = static_cast<int>(client.right - client.left);
    fill(dc, make_rect(0, 0, width, Height), theme.panel);
    const HGDIOBJ old_font = SelectObject(dc, screen.picker_title_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(theme.text));
    TextOutA(dc, 18, (Height - static_cast<int>(metrics.tmHeight)) / 2,
             title.c_str(), static_cast<int>(title.size()));
    SelectObject(dc, old_font);

    // A back entry as well as Escape: shown as an overlay there is no title
    // bar to close, and a phone's back gesture is not something to rely on as
    // the only way out.
    const RECT back = make_rect(width - 12 - 120, (Height - 42) / 2, 120, 42);
    draw_entry(dc, back, "BACK", "", screen.small_entry_font, screen.note_font,
               screen.hovered == PickerBackControl, true);
    screen.hotspots.push_back(Hotspot{back, PickerBackControl, true});
    return Height;
}


// The launcher keeps the managed Features object as raw JSON, so that a
// native build saving settings cannot drop a flag it does not understand.
// Pro mode HUD is the one flag with a row, so it is read and written in place
// rather than by parsing and re-emitting the whole object.
[[nodiscard]] bool json_flag(const std::string& json, const std::string& key) {
    const std::size_t at = json.find("\"" + key + "\"");
    if (at == std::string::npos) {
        return false;
    }
    const std::size_t colon = json.find(':', at);
    if (colon == std::string::npos) {
        return false;
    }
    return json.find("true", colon) == json.find_first_not_of(" \t\r\n",
                                                             colon + 1);
}

[[nodiscard]] std::string set_json_flag(const std::string& json,
                                        const std::string& key, bool value) {
    const std::string text = value ? "true" : "false";
    const std::size_t at = json.find("\"" + key + "\"");
    if (at != std::string::npos) {
        const std::size_t colon = json.find(':', at);
        if (colon != std::string::npos) {
            const std::size_t first =
                json.find_first_not_of(" \t\r\n", colon + 1);
            const std::size_t last = json.find_first_of(",}", first);
            if (first != std::string::npos && last != std::string::npos) {
                return json.substr(0, first) + text + json.substr(last);
            }
        }
        return json;
    }
    const std::size_t brace = json.find('{');
    if (brace == std::string::npos) {
        return "{\"" + key + "\":" + text + "}";
    }
    const std::size_t rest = json.find_first_not_of(" \t\r\n", brace + 1);
    const std::string separator =
        rest != std::string::npos && json[rest] != '}' ? "," : "";
    return json.substr(0, brace + 1) + "\"" + key + "\":" + text + separator
        + json.substr(brace + 1);
}

// Formats.Language, in the order the managed enum declares them.
[[nodiscard]] const std::array<std::string, 5>& settings_languages() {
    static const std::array<std::string, 5> names{{
        "English", "French", "German", "Italian", "Spanish",
    }};
    return names;
}

// PadAction to the member of GamepadBindings it names.  A table rather than a
// switch so the two lists cannot drift apart silently.
[[nodiscard]] input::GamepadButtons input::GamepadBindings::* pad_member(
    input::PadAction action) noexcept {
    using Bindings = input::GamepadBindings;
    switch (action) {
    case input::PadAction::Shoot: return &Bindings::shoot;
    case input::PadAction::Zoom: return &Bindings::zoom;
    case input::PadAction::Jump: return &Bindings::jump;
    case input::PadAction::Morph: return &Bindings::morph;
    case input::PadAction::Scan: return &Bindings::scan;
    case input::PadAction::ScanVisor: return &Bindings::scan_visor;
    case input::PadAction::Scoreboard: return &Bindings::scoreboard;
    case input::PadAction::NextWeapon: return &Bindings::next_weapon;
    case input::PadAction::PrevWeapon: return &Bindings::prev_weapon;
    case input::PadAction::Missile: return &Bindings::missile;
    case input::PadAction::PowerBeam: return &Bindings::power_beam;
    default: return &Bindings::menu;
    }
}

// The fields the settings pages and the join card type into, addressed by the
// control id that owns each one.
[[nodiscard]] std::string* focused_text(Screen& screen) {
    switch (screen.focused_field) {
    case OnlineAddressControl: return &screen.online_address;
    case SettingsRowControl + 40: return &screen.field_point_goal;
    case SettingsRowControl + 41: return &screen.field_time_limit;
    case SettingsRowControl + 50: return &screen.field_player_name;
    case SettingsRowControl + 52: return &screen.field_server;
    case SettingsRowControl + 53: return &screen.field_master;
    default: return nullptr;
    }
}

// Rows.SliderRow: a tracked label, a four-pixel rail with a knob on it, and
// the formatted value on the right.
int draw_slider(HDC dc, int left, int top, int width, const std::string& label,
                int value, const std::string& formatted, Screen& screen,
                int id) {
    constexpr int Height = 34;
    constexpr int LabelWidth = 150;
    const Palette& theme = palette();
    const HGDIOBJ old_font = SelectObject(dc, screen.caption_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(theme.text_dim));
    SetTextCharacterExtra(dc, 1);
    std::string upper = label;
    for (char& letter : upper) {
        letter = static_cast<char>(
            std::toupper(static_cast<unsigned char>(letter)));
    }
    TextOutA(dc, left + 4,
             top + (Height - static_cast<int>(metrics.tmHeight)) / 2,
             upper.c_str(), static_cast<int>(upper.size()));
    SetTextCharacterExtra(dc, 0);

    const int track_x = left + LabelWidth;
    const int track_width = std::max(40, width - LabelWidth - 64);
    const int track_y = top + Height / 2 - 2;
    fill(dc, make_rect(track_x, track_y, track_width, 4), theme.panel_light);
    const int filled = track_width * std::clamp(value, 0, 100) / 100;
    const Color accent = screen.hovered == id
        ? shade(theme.accent, 0.15) : theme.accent;
    fill(dc, make_rect(track_x, track_y, filled, 4), accent);
    const HBRUSH knob = CreateSolidBrush(to_ref(accent));
    const HGDIOBJ old_brush = SelectObject(dc, knob);
    const HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, track_x + filled - 5, track_y + 2 - 5, track_x + filled + 5,
            track_y + 2 + 5);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(knob);

    SelectObject(dc, screen.row_bold_font);
    SetTextColor(dc, to_ref(theme.text));
    RECT value_area = make_rect(left + width - 64, top, 60, Height);
    DrawTextA(dc, formatted.c_str(), static_cast<int>(formatted.size()),
              &value_area, DT_SINGLELINE | DT_VCENTER | DT_RIGHT
                  | DT_NOPREFIX);
    SelectObject(dc, old_font);
    screen.hotspots.push_back(Hotspot{
        make_rect(track_x - 8, top, track_width + 16, Height), id, true});
    return Height;
}

// KeyRow/PadRow: a label and a framed box holding what it is bound to, which
// says what to do while it is listening.
int draw_bind(HDC dc, int left, int top, int width, const std::string& label,
              const std::string& value, bool listening, Screen& screen,
              int id) {
    constexpr int Height = 32;
    constexpr int LabelWidth = 190;
    const Palette& theme = palette();
    const HGDIOBJ old_font = SelectObject(dc, screen.row_bold_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(theme.text));
    TextOutA(dc, left + 4,
             top + (Height - static_cast<int>(metrics.tmHeight)) / 2,
             label.c_str(), static_cast<int>(label.size()));

    const int box_x = left + LabelWidth;
    const int box_width = std::max(60, width - LabelWidth - 4);
    const RECT box = make_rect(box_x, top + 2, box_width, Height - 4);
    const HBRUSH fill_brush = CreateSolidBrush(to_ref(theme.panel_light));
    const HPEN pen = CreatePen(PS_SOLID, 1,
                               to_ref(listening ? theme.warm
                                      : screen.hovered == id ? theme.accent
                                                             : theme.edge));
    HGDIOBJ old_brush = SelectObject(dc, fill_brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, box.left, box.top, box.right + 1, box.bottom + 1, 8, 8);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(fill_brush);
    DeleteObject(pen);

    const std::string text = listening
        ? std::string("press a button on the pad") : value;
    RECT inner = make_rect(box.left + 6, box.top, box_width - 12,
                           Height - 4);
    SetTextColor(dc, to_ref(listening ? theme.warm : theme.text));
    DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &inner,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX
                  | DT_END_ELLIPSIS);
    SelectObject(dc, old_font);
    screen.hotspots.push_back(Hotspot{
        make_rect(left, top, width, Height), id, true});
    return Height;
}

[[nodiscard]] std::string format_scale(int value) {
    return std::to_string(std::max(mods::render::Options::MinScale, value))
        + "%";
}

[[nodiscard]] std::string format_multiplier(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2fx", static_cast<double>(value));
    return buffer;
}

[[nodiscard]] std::string format_plain(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    return buffer;
}

// SettingsView's slider conversions, kept as they are because the ranges are
// chosen rather than derived: a pad's look runs 0.25x to 3x, and a dead zone
// stops at half the stick's travel because past that the pad is broken rather
// than worn.
[[nodiscard]] int sensitivity_to_slider(float value) {
    return std::clamp(static_cast<int>(std::lround(
        (value - 0.1F) / 2.9F * 100.0F)), 0, 100);
}
[[nodiscard]] float slider_to_sensitivity(int value) {
    return 0.1F + static_cast<float>(value) / 100.0F * 2.9F;
}
[[nodiscard]] int look_to_slider(float value) {
    return std::clamp(static_cast<int>(std::lround(
        (value - 0.25F) / 2.75F * 100.0F)), 0, 100);
}
[[nodiscard]] float slider_to_look(int value) {
    return 0.25F + static_cast<float>(value) / 100.0F * 2.75F;
}
[[nodiscard]] int dead_zone_to_slider(float value) {
    return std::clamp(static_cast<int>(std::lround(value / 0.5F * 100.0F)),
                      0, 100);
}
[[nodiscard]] float slider_to_dead_zone(int value) {
    return static_cast<float>(value) / 100.0F * 0.5F;
}
[[nodiscard]] int percent_of(const std::string& stored, int fallback) {
    try {
        return std::clamp(static_cast<int>(std::lround(
            std::stof(stored) * 100.0F)), 0, 100);
    } catch (const std::exception&) {
        return fallback;
    }
}
[[nodiscard]] std::string percent_to_stored(int value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f",
                  static_cast<double>(value) / 100.0);
    return buffer;
}

// Rows.FieldRow, defined with the other row widgets below.
int draw_field(HDC dc, int left, int top, int width,
               const std::string& label, const std::string& value,
               int box_width, Screen& screen, int id);

// SettingsView: a rail of sections beside their pages, with Save and Cancel
// under the rail.
//
// A rail rather than a card in the 400-pixel column beside the picture: this
// is six sections and several dozen rows, which is not a thing to page through
// in a column.  The keyboard bindings are the one group missing -- the native
// side has no keyboard binding table to edit yet, and drawing rows that cannot
// be changed would be worse than not drawing them.
constexpr std::array<const char*, 6> SettingsSections{{
    "Display", "Audio", "Controls", "Match rules", "Profile", "Credits",
}};

// Rows.Caption at the size the settings pages use for a group heading.
int draw_heading(HDC dc, int left, int top, int width, const std::string& text,
                 Screen& screen) {
    std::string upper = text;
    for (char& letter : upper) {
        letter = static_cast<char>(
            std::toupper(static_cast<unsigned char>(letter)));
    }
    // Caption's own 26 plus the 8 above and 4 below the settings pages give
    // their headings.
    draw_caption(dc, left, top + 8, width, upper, screen.caption_font);
    return 30 + 12;
}

void draw_settings_page(HDC dc, int left, int width, int& top, Screen& screen) {
    const Palette& theme = palette();
    constexpr int Spacing = 2;
    const auto hunters = HomeView::hunter_options();
    const int section = screen.settings_section;
    const auto row_id = [](int index) { return SettingsRowControl + index; };

    if (section == 0) {  // Display
        // A phone has one window, it is already the whole screen, and it has
        // no F11.  Everything in this group is about a desktop window.
        top += draw_heading(dc, left, top, width, "Window", screen);
        top += draw_choice(dc, left, top, width, "Mode",
                           screen.window_mode == 1
                               ? "Fullscreen (borderless)" : "Windowed",
                           screen, row_id(0)) + Spacing;
        top += draw_heading(dc, left, top, width, "Performance", screen);
        top += draw_slider(dc, left, top, width, "Render scale",
                           screen.render.resolution_scale,
                           format_scale(screen.render.resolution_scale),
                           screen, row_id(1)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Lighting",
                           screen.render.lighting, screen, row_id(2))
            + Spacing;
        top += draw_toggle(dc, left, top, width, "Fog", screen.render.fog,
                           screen, row_id(3)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Texture filtering",
                           screen.render.texture_filtering, screen,
                           row_id(4)) + Spacing;
        top += draw_toggle(dc, left, top, width, "FPS counter",
                           screen.render.show_fps, screen, row_id(5))
            + Spacing;
        top += draw_heading(dc, left, top, width, "Cel shading", screen);
        top += draw_toggle(dc, left, top, width, "Cel shading",
                           screen.render.cel_shading, screen, row_id(6))
            + Spacing;
        // One switch, and none of what it drives.  Pro mode is the whole HUD
        // decision: off is the game as the DS drew it, on is the competitive
        // layout.  The six settings underneath were six ways to end up
        // somewhere between the two.
        top += draw_heading(dc, left, top, width, "HUD", screen);
        top += draw_toggle(dc, left, top, width, "Pro mode HUD",
                           screen.pro_hud, screen, row_id(7)) + Spacing;
        return;
    }
    if (section == 1) {  // Audio
        top += draw_heading(dc, left, top, width, "Volume", screen);
        const int sfx = percent_of(screen.menu.sfx_volume, 35);
        top += draw_slider(dc, left, top, width, "Sound effects", sfx,
                           std::to_string(sfx) + "%", screen, row_id(20))
            + Spacing;
        const int music = percent_of(screen.menu.music_volume, 50);
        top += draw_slider(dc, left, top, width, "Music", music,
                           std::to_string(music) + "%", screen, row_id(21))
            + Spacing;
        top += draw_heading(dc, left, top, width, "Language", screen);
        top += draw_choice(dc, left, top, width, "Text",
                           settings_languages()[static_cast<std::size_t>(
                               screen.language_row)],
                           screen, row_id(22)) + Spacing;
        return;
    }
    if (section == 2) {  // Controls
        top += draw_heading(dc, left, top, width, "Mouse", screen);
        top += draw_slider(
            dc, left, top, width, "Sensitivity",
            sensitivity_to_slider(screen.input.mouse_sensitivity),
            format_multiplier(screen.input.mouse_sensitivity), screen,
            row_id(30)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Invert vertical aim",
                           screen.input.invert_mouse_y, screen, row_id(31))
            + Spacing;
        top += draw_toggle(dc, left, top, width, "Invert horizontal aim",
                           screen.input.invert_mouse_x, screen, row_id(32))
            + Spacing;
        top += draw_toggle(dc, left, top, width, "Wheel cycles every weapon",
                           screen.input.scroll_all_weapons, screen,
                           row_id(33)) + Spacing;
        // Its own group rather than more rows under Mouse: a pad has its own
        // sensitivity, and somebody who inverts one very often does not
        // invert the other.
        top += draw_heading(dc, left, top, width, "Gamepad", screen);
        top += draw_slider(
            dc, left, top, width, "Look sensitivity",
            look_to_slider(screen.input.gamepad_look_sensitivity),
            format_multiplier(screen.input.gamepad_look_sensitivity), screen,
            row_id(34)) + Spacing;
        top += draw_slider(
            dc, left, top, width, "Stick dead zone",
            dead_zone_to_slider(screen.input.gamepad_dead_zone),
            format_plain(screen.input.gamepad_dead_zone), screen, row_id(35))
            + Spacing;
        top += draw_toggle(dc, left, top, width, "Invert vertical aim (stick)",
                           screen.input.gamepad_invert_y, screen, row_id(36))
            + Spacing;
        top += draw_heading(dc, left, top, width, "Gamepad buttons", screen);
        const auto& actions = input::pad_bindings::actions();
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto action = actions[i];
            top += draw_bind(
                dc, left, top, width, input::pad_bindings::name(action),
                input::pad_bindings::describe(
                    screen.input.pad_bindings.*
                        pad_member(action)),
                screen.listening_pad == static_cast<int>(i), screen,
                row_id(60 + static_cast<int>(i))) + Spacing;
        }
        const RECT reset = make_rect(left, top + 8, width, 30);
        draw_entry(dc, reset, "RESET TO DEFAULTS", "", screen.small_entry_font,
                   screen.note_font, screen.hovered == SettingsResetControl,
                   true);
        screen.hotspots.push_back(Hotspot{reset, SettingsResetControl, true});
        top += 38 + Spacing;
        return;
    }
    if (section == 3) {  // Match rules
        top += draw_heading(dc, left, top, width, "Match rules", screen);
        top += draw_field(dc, left, top, width, "Point goal",
                          screen.field_point_goal, 120, screen, row_id(40))
            + Spacing;
        top += draw_field(dc, left, top, width, "Time limit",
                          screen.field_time_limit, 120, screen, row_id(41))
            + Spacing;
        const char* damage[] = {"low", "medium", "high"};
        top += draw_choice(dc, left, top, width, "Damage",
                           damage[std::clamp(screen.damage_row, 0, 2)],
                           screen, row_id(42)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Team play",
                           screen.menu.team_play == "on", screen, row_id(43))
            + Spacing;
        top += draw_toggle(dc, left, top, width, "Friendly fire",
                           screen.menu.friendly_fire == "on", screen,
                           row_id(44)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Hunter radar",
                           screen.menu.hunter_radar == "on", screen,
                           row_id(45)) + Spacing;
        top += draw_toggle(dc, left, top, width, "Affinity weapons",
                           screen.menu.affinity_weapons == "on", screen,
                           row_id(46)) + Spacing;
        return;
    }
    if (section == 4) {  // Profile
        top += draw_heading(dc, left, top, width, "You", screen);
        top += draw_field(dc, left, top, width, "Your name",
                          screen.field_player_name, 200, screen, row_id(50))
            + Spacing;
        top += draw_choice(dc, left, top, width, "Hunter",
                           std::string(hunters[static_cast<std::size_t>(
                               screen.hunter_row)]),
                           screen, row_id(51)) + Spacing;
        top += draw_heading(dc, left, top, width, "Servers", screen);
        top += draw_field(dc, left, top, width, "Default server",
                          screen.field_server, 220, screen, row_id(52))
            + Spacing;
        top += draw_field(dc, left, top, width, "Server directory",
                          screen.field_master, 220, screen, row_id(53))
            + Spacing;
        top += draw_toggle(dc, left, top, width,
                           "Check for updates on startup",
                           screen.prefs.auto_update, screen, row_id(54))
            + Spacing;
        top += draw_heading(dc, left, top, width, "Game files", screen);
        const RECT files = make_rect(left, top, width, 54);
        draw_entry(dc, files, "GAME FILES",
                   screen.selection.rom_path.empty()
                       ? "Not set up yet" : screen.selection.rom_path,
                   screen.primary_font, screen.note_font,
                   screen.hovered == SettingsFilesControl, true);
        screen.hotspots.push_back(Hotspot{files, SettingsFilesControl, true});
        top += 54 + Spacing;
        return;
    }
    // Credits: who this is built on, in full.
    top += draw_caption(dc, left, top, width, "LIVETEK", screen.caption_font)
        + Spacing;
    top += draw_note(dc, left, top, width, std::string(credits::ForkWork),
                     screen.note_font, theme.text_dim) + Spacing;
    for (const auto& entry : credits::entries()) {
        std::string who(entry.who);
        for (char& letter : who) {
            letter = static_cast<char>(
                std::toupper(static_cast<unsigned char>(letter)));
        }
        top += draw_caption(dc, left, top + 8, width, who,
                            screen.caption_font) + 12;
        top += draw_note(dc, left, top, width, std::string(entry.what),
                         screen.note_font, theme.text_dim) + Spacing;
        if (!entry.where.empty()) {
            top += draw_note(dc, left, top, width, std::string(entry.where),
                             screen.note_font, theme.text_dim) + Spacing;
        }
    }
}

void draw_settings(HDC dc, const RECT& client, Screen& screen) {
    const Palette& theme = palette();
    fill(dc, client, theme.ink);
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    constexpr int RailWidth = 216;
    constexpr int FooterHeight = 110;

    fill(dc, make_rect(0, 0, RailWidth, height), theme.panel);
    int rail_top = 20;
    draw_caption(dc, 18, rail_top, RailWidth - 32, "SETTINGS",
                 screen.caption_font);
    rail_top += 34;
    for (std::size_t i = 0; i < SettingsSections.size(); ++i) {
        const int id = SettingsSectionControl + static_cast<int>(i);
        const RECT row = make_rect(18, rail_top, RailWidth - 32, 32);
        std::string name = SettingsSections[i];
        for (char& letter : name) {
            letter = static_cast<char>(
                std::toupper(static_cast<unsigned char>(letter)));
        }
        // The section that is on screen carries the accent, like a selected
        // MenuEntry; hover is a different fact and only lights the bar.
        draw_entry(dc, row, name, "", screen.entry_font, screen.note_font,
                   screen.hovered == id
                       || screen.settings_section == static_cast<int>(i),
                   true);
        screen.hotspots.push_back(Hotspot{row, id, true});
        rail_top += 34;
    }

    // Save and Cancel under the rail rather than over the pages: the pages
    // scroll and these must not.
    int footer_top = height - FooterHeight;
    const RECT save = make_rect(18, footer_top, RailWidth - 32, 40);
    draw_primary(dc, save, "SAVE AND CLOSE", screen.primary_font,
                 screen.hovered == SettingsSaveControl,
                 screen.pressed == SettingsSaveControl, true);
    screen.hotspots.push_back(Hotspot{save, SettingsSaveControl, true});
    footer_top += 46;
    const RECT cancel = make_rect(18, footer_top, RailWidth - 32, 26);
    draw_entry(dc, cancel, "CANCEL", "", screen.small_entry_font,
               screen.note_font, screen.hovered == SettingsCancelControl,
               true);
    screen.hotspots.push_back(Hotspot{cancel, SettingsCancelControl, true});
    footer_top += 32;
    if (!screen.save_error.empty()) {
        // Writing settings touches the disk, and the disk is allowed to say
        // no.  That is worth a line on the screen rather than an exception.
        draw_note(dc, 18, footer_top, RailWidth - 32, screen.save_error,
                  screen.note_font, theme.warm);
    }

    // The page, inset by its own margin rather than the viewport's padding:
    // padding is not taken off the width the content is measured with, and
    // every wrapped note ran off the right edge by exactly that much.
    const int page_left = RailWidth + 26;
    const int page_width = std::max(width - page_left - 26, 1);
    int page_top = 22 - screen.scroll;
    const HRGN clip = CreateRectRgn(RailWidth, 0, width, height);
    SelectClipRgn(dc, clip);
    draw_settings_page(dc, page_left, page_width, page_top, screen);
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);
    screen.scroll_extent = std::max(0, page_top + screen.scroll + 22 - height);
}

// MapPickerView: every map at once, as pictures.
void draw_map_picker(HDC dc, const RECT& client, Screen& screen) {
    const Palette& theme = palette();
    fill(dc, client, theme.ink);
    const int width = static_cast<int>(client.right - client.left);
    const int header = draw_picker_header(dc, client, "Choose a map", screen);

    constexpr int TileWidth = 248;
    constexpr int TileHeight = 168;
    constexpr int Margin = 8;
    constexpr int CaptionHeight = 26;
    constexpr int Padding = 14;
    const int columns = std::max(
        1, (width - Padding * 2) / (TileWidth + Margin * 2));
    const std::string current = screen.rooms.empty()
        ? std::string()
        : screen.rooms[static_cast<std::size_t>(std::clamp(
              screen.match_map, 0,
              static_cast<int>(screen.rooms.size()) - 1))];

    const HGDIOBJ old_font = SelectObject(dc, screen.row_font);
    for (std::size_t i = 0; i < screen.rooms.size(); ++i) {
        const int column = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        const int x = Padding + Margin + column * (TileWidth + Margin * 2);
        const int y = header + Padding + Margin
            + row * (TileHeight + Margin * 2) - screen.scroll;
        if (y > static_cast<int>(client.bottom) || y + TileHeight < header) {
            continue;
        }
        const std::string& key = screen.rooms[i];
        const int id = PickerItemControl + static_cast<int>(i);
        const bool selected = key == current;
        const bool hot = screen.hovered == id;
        const RECT picture = make_rect(x, y, TileWidth,
                                       TileHeight - CaptionHeight);
        const auto found = screen.previews.find(key);
        const Image* preview = found == screen.previews.end()
            ? nullptr : found->second.get();
        if (preview != nullptr && preview->valid()) {
            const HDC source = CreateCompatibleDC(dc);
            const HGDIOBJ old = SelectObject(source, preview->bitmap());
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, x, y, TileWidth, TileHeight - CaptionHeight, source,
                       0, 0, preview->width(), preview->height(), SRCCOPY);
            SelectObject(source, old);
            DeleteDC(source);
        } else {
            fill(dc, picture, theme.panel);
            RECT none = picture;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, to_ref(theme.text_dim));
            DrawTextA(dc, "no preview", -1, &none,
                      DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        }
        const RECT caption = make_rect(x, y + TileHeight - CaptionHeight,
                                       TileWidth, CaptionHeight);
        fill(dc, caption, theme.panel);
        SelectObject(dc, screen.row_font);
        SetTextColor(dc, to_ref(selected || hot ? theme.accent : theme.text));
        RECT label = make_rect(x + 4, caption.top, TileWidth - 8,
                               CaptionHeight);
        DrawTextA(dc, key.c_str(), static_cast<int>(key.size()), &label,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX
                      | DT_END_ELLIPSIS);
        if (selected || hot) {
            const HPEN pen = CreatePen(PS_SOLID, selected ? 3 : 2,
                                       to_ref(theme.accent));
            const HGDIOBJ old_pen = SelectObject(dc, pen);
            const HGDIOBJ old_brush = SelectObject(dc,
                                                   GetStockObject(NULL_BRUSH));
            Rectangle(dc, x + 1, y + 1, x + TileWidth - 1,
                      y + TileHeight - 1);
            SelectObject(dc, old_brush);
            SelectObject(dc, old_pen);
            DeleteObject(pen);
        }
        screen.hotspots.push_back(Hotspot{
            make_rect(x, y, TileWidth, TileHeight), id, true});
    }
    SelectObject(dc, old_font);
    const int rows = screen.rooms.empty()
        ? 0
        : (static_cast<int>(screen.rooms.size()) + columns - 1) / columns;
    screen.scroll_extent = std::max(
        0, header + Padding * 2 + rows * (TileHeight + Margin * 2)
               - static_cast<int>(client.bottom));
}

// DemoPickerView: the recordings this machine made.
void draw_demo_picker(HDC dc, const RECT& client, Screen& screen) {
    const Palette& theme = palette();
    fill(dc, client, theme.ink);
    const int width = static_cast<int>(client.right - client.left);
    const int header = draw_picker_header(dc, client, "Demos", screen);
    constexpr int Padding = 14;
    constexpr int SubtitledHeight = 54;
    constexpr int PlainHeight = 42;
    const int left = Padding;
    const int body_width = std::max(width - Padding * 2, 1);
    int top = header + Padding - screen.scroll;

    for (std::size_t i = 0; i < screen.demos.size(); ++i) {
        const demo::Recording& recording = screen.demos[i];
        const int id = PickerItemControl + static_cast<int>(i);
        const RECT row = make_rect(left, top, body_width, SubtitledHeight);
        const std::string title = recording.room.empty()
            ? recording.file_name() : recording.room;
        draw_entry(dc, row, title, demo::describe(recording),
                   screen.primary_font, screen.note_font,
                   screen.hovered == id, true);
        screen.hotspots.push_back(Hotspot{row, id, true});
        top += SubtitledHeight + 2;
    }
    if (screen.demos.empty()) {
        // The folder, spelled out.  It is the app's own directory, and a
        // player who wants to copy a recording off the device needs the path
        // itself -- this is the only place it is ever written down.
        const auto directory = demo::demos_directory(screen.directory);
        top += draw_note(dc, left, top + 6, body_width,
                         "Nothing recorded yet. Recordings are made from the "
                         "pause menu during an online match, and are written "
                         "to:" + std::string("\n") + directory.string(),
                         screen.note_font, theme.text_dim) + 12;
    }
    const RECT import_row = make_rect(left, top + 10, body_width, PlainHeight);
    draw_entry(dc, import_row, "OPEN A FILE...",
               "A demo from somewhere else on this device",
               screen.small_entry_font, screen.note_font,
               screen.hovered == DemoImportControl, true);
    screen.hotspots.push_back(Hotspot{import_row, DemoImportControl, true});
    top += PlainHeight + 10;
    screen.scroll_extent = std::max(
        0, top + screen.scroll + Padding - static_cast<int>(client.bottom));
}

// ServerRow.Columns: five columns, of which only the name and the map give
// ground.  The mode column shrinks first on a narrow row -- a trimmed mode is
// still readable where a trimmed server name is not the server anybody was
// looking for.
struct ServerColumns final {
    int name_x = 0, name_width = 0;
    int map_x = 0, map_width = 0;
    int mode_x = 0, mode_width = 0;
    int players_right = 0, players_width = 0;
    int ping_right = 0, ping_width = 0;

    explicit ServerColumns(int left, int width) {
        constexpr int Margin = 8;
        constexpr int Gutter = 10;
        constexpr int MaxPing = 34;
        constexpr int MaxPlayers = 52;
        constexpr int MaxMode = 66;
        ping_right = left + width - Margin;
        ping_width = MaxPing;
        players_right = ping_right - MaxPing - Gutter;
        players_width = MaxPlayers;
        mode_width = std::min(MaxMode,
                              std::max(0, (width - 200) * 2 / 5));
        mode_x = players_right - MaxPlayers - Gutter - mode_width;
        name_x = left + Margin;
        const int rest = std::max(0, mode_x - Gutter - name_x);
        // 0.44 of what the fixed columns leave.
        name_width = rest * 44 / 100;
        map_x = name_x + name_width + Gutter;
        map_width = std::max(0, rest - name_width - Gutter);
    }
};

// One cell: a single line, trimmed to its column and clipped to it whatever
// the trimming decides.  Both halves are needed -- trimming cannot help a
// single unbreakable word wider than its column, which is what PLAYERS is.
void draw_cell(HDC dc, const std::string& text, int x, int width, int top,
               int height, Color color, HFONT font, bool right_align) {
    if (text.empty() || width <= 4) {
        return;
    }
    const HGDIOBJ old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(color));
    RECT area = make_rect(right_align ? x - width : x, top, width, height);
    DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), &area,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS
                  | (right_align ? DT_RIGHT : DT_LEFT));
    SelectObject(dc, old_font);
}

// HomeView.BuildBrowseCard: the directory's list, asked again by hand.
void draw_browse_card(HDC dc, int left, int width, int& top, Screen& screen) {
    const Palette& theme = palette();
    constexpr int Spacing = 2;
    constexpr int PlainHeight = 42;
    constexpr int HeaderHeight = 22;
    constexpr int RowHeight = 30;

    top += draw_caption(dc, left, top, width, "JOIN", screen.caption_font)
        + Spacing;

    std::string note;
    Color note_color = theme.text_dim;
    std::vector<Browse::Row> rows;
    {
        const std::lock_guard<std::mutex> guard(screen.browse->lock);
        note = screen.browse->note;
        note_color = screen.browse->note_color;
        rows = screen.browse->rows;
    }
    if (!note.empty()) {
        top += draw_note(dc, left, top, width, note, screen.note_font,
                         note_color) + Spacing;
    }

    // The headings sit outside the list so they stay put while it scrolls,
    // which is the whole point of having them.
    const ServerColumns columns(left, width);
    draw_cell(dc, "SERVER", columns.name_x, columns.name_width, top,
              HeaderHeight, theme.text_dim, screen.caption_font, false);
    draw_cell(dc, "MAP", columns.map_x, columns.map_width, top, HeaderHeight,
              theme.text_dim, screen.caption_font, false);
    draw_cell(dc, "TYPE", columns.mode_x, columns.mode_width, top,
              HeaderHeight, theme.text_dim, screen.caption_font, false);
    draw_cell(dc, "PLAYERS", columns.players_right, columns.players_width, top,
              HeaderHeight, theme.text_dim, screen.caption_font, true);
    draw_cell(dc, "PING", columns.ping_right, columns.ping_width, top,
              HeaderHeight, theme.text_dim, screen.caption_font, true);
    fill(dc, make_rect(left, top + HeaderHeight - 1, width, 1), theme.edge);
    top += HeaderHeight;

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Browse::Row& row = rows[i];
        const int id = ServerRowControl + static_cast<int>(i);
        const RECT body = make_rect(left, top, width, RowHeight);
        if (screen.hovered == id) {
            fill(dc, body, theme.panel_light);
        }
        // The name is dim until the server answers: a listing the directory
        // knows about is not the same as one this machine can reach.
        draw_cell(dc, row.name, columns.name_x, columns.name_width, top,
                  RowHeight, row.answered ? theme.text : theme.text_dim,
                  screen.row_bold_font, false);
        draw_cell(dc, row.map, columns.map_x, columns.map_width, top,
                  RowHeight, theme.text_dim, screen.row_font, false);
        draw_cell(dc, row.mode, columns.mode_x, columns.mode_width, top,
                  RowHeight, theme.text_dim, screen.row_font, false);
        draw_cell(dc, row.players, columns.players_right, columns.players_width,
                  top, RowHeight, theme.text, screen.row_font, true);
        // The same three bands Quake 3 uses: the number matters far less than
        // which of "fine", "playable" and "don't" it falls in.
        const Color ping_color = row.latency < 0
            ? (row.answered ? theme.text_dim : theme.bad)
            : row.latency < 80 ? theme.good
            : row.latency < 160 ? theme.warm : theme.bad;
        draw_cell(dc, row.ping, columns.ping_right, columns.ping_width, top,
                  RowHeight, ping_color, screen.row_font, true);
        screen.hotspots.push_back(Hotspot{body, id, true});
        top += RowHeight + Spacing;
    }

    const RECT refresh = make_rect(left, top, width, PlainHeight);
    draw_entry(dc, refresh, "REFRESH", "", screen.entry_font, screen.note_font,
               screen.hovered == BrowseRefreshControl, true);
    screen.hotspots.push_back(Hotspot{refresh, BrowseRefreshControl, true});
    top += PlainHeight + Spacing;

    const RECT back = make_rect(left, top, width, PlainHeight);
    draw_entry(dc, back, "BACK", "", screen.small_entry_font, screen.note_font,
               screen.hovered == BackControl, true);
    screen.hotspots.push_back(Hotspot{back, BackControl, true});
    top += PlainHeight + Spacing;
}

// Rows.FieldRow: a label and something to type in.
int draw_field(HDC dc, int left, int top, int width, const std::string& label,
               const std::string& value, int box_width, Screen& screen,
               int id) {
    const bool focused = screen.focused_field == id;
    constexpr int Height = 36;
    const Palette& theme = palette();
    const HGDIOBJ old_font = SelectObject(dc, screen.row_font);
    TEXTMETRICA metrics{};
    GetTextMetricsA(dc, &metrics);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_ref(theme.text_dim));
    TextOutA(dc, left + 4,
             top + (Height - static_cast<int>(metrics.tmHeight)) / 2,
             label.c_str(), static_cast<int>(label.size()));

    const int box_x = left + width - box_width;
    const int box_h = 26;
    const int box_y = top + (Height - box_h) / 2;
    const HBRUSH fill_brush = CreateSolidBrush(to_ref(theme.panel_light));
    const HPEN pen = CreatePen(PS_SOLID, 1,
                               to_ref(focused ? theme.accent : theme.edge));
    HGDIOBJ old_brush = SelectObject(dc, fill_brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, box_x, box_y, box_x + box_width + 1, box_y + box_h + 1, 8, 8);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(fill_brush);
    DeleteObject(pen);

    RECT text_area = make_rect(box_x + 8, box_y, box_width - 16, box_h);
    SetTextColor(dc, to_ref(theme.text));
    DrawTextA(dc, value.c_str(), static_cast<int>(value.size()), &text_area,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (focused) {
        // A caret rather than a selection: these fields take an address or a
        // number, and the one gesture they need is typing at the end of one.
        SIZE extent{};
        const auto caret = static_cast<int>(std::min(screen.caret,
                                                     value.size()));
        GetTextExtentPoint32A(dc, value.c_str(), caret, &extent);
        fill(dc, make_rect(box_x + 8 + static_cast<int>(extent.cx),
                           box_y + 5, 1, box_h - 10), theme.text);
    }
    SelectObject(dc, old_font);
    screen.hotspots.push_back(Hotspot{
        make_rect(box_x, box_y, box_width, box_h), id, true});
    return Height;
}

// HomeView.BuildOnlineCard: the page a server has already been picked on, so
// all that is left to choose is the hunter.
void draw_online_card(HDC dc, int left, int width, int& top, Screen& screen) {
    const Palette& theme = palette();
    constexpr int Spacing = 2;
    constexpr int PlainHeight = 42;
    constexpr int PrimaryHeight = 44;
    const auto hunters = HomeView::hunter_options();

    top += draw_caption(dc, left, top, width, "JOIN", screen.caption_font)
        + Spacing;
    top += draw_choice(dc, left, top, width, "Hunter",
                       std::string(hunters[static_cast<std::size_t>(
                           screen.online_hunter)]),
                       screen, OnlineHunterControl) + Spacing;
    top += draw_field(dc, left, top, width, "Server", screen.online_address,
                      190, screen, OnlineAddressControl) + Spacing;
    std::string status;
    {
        const std::lock_guard<std::mutex> guard(screen.browse->lock);
        status = screen.browse->note;
    }
    top += draw_note(dc, left, top, width, status, screen.note_font,
                     theme.text_dim) + Spacing;
    const RECT connect = make_rect(left, top, width, PrimaryHeight);
    draw_primary(dc, connect, "CONNECT", screen.primary_font,
                 screen.hovered == ConnectControl,
                 screen.pressed == ConnectControl, true);
    screen.hotspots.push_back(Hotspot{connect, ConnectControl, true});
    top += PrimaryHeight + Spacing;

    // Back goes to the list rather than to the front screen: somebody who
    // picked the wrong server is trying to reach the list.
    const RECT back = make_rect(left, top, width, PlainHeight);
    draw_entry(dc, back, "BACK", "", screen.small_entry_font, screen.note_font,
               screen.hovered == BackControl, true);
    screen.hotspots.push_back(Hotspot{back, BackControl, true});
    top += PlainHeight + Spacing;
}

// HomeView.BuildHostCard: the story or a match, and whichever one is chosen
// showing its own options underneath.
void draw_host_card(HDC dc, int left, int width, int& top, Screen& screen) {
    const Palette& theme = palette();
    constexpr int Spacing = 2;
    constexpr int PlainHeight = 42;
    constexpr int PrimaryHeight = 44;
    const auto hunters = HomeView::hunter_options();
    const auto modes = HomeView::mode_options();

    top += draw_caption(dc, left, top, width, "HOST", screen.caption_font)
        + Spacing;
    top += draw_choice(dc, left, top, width, "Mode",
                       screen.host_mode == 0 ? "Adventure" : "Battle", screen,
                       HostModeControl) + Spacing;

    if (screen.host_mode == 0) {
        // Announced rather than hidden.  A menu that simply does not mention
        // co-op tells somebody looking for it that it was never considered;
        // this says it is coming and refuses to pretend it works.
        top += draw_toggle(dc, left, top, width,
                           "Online co-op (coming soon!)", screen.host_coop,
                           screen, HostCoopControl) + Spacing;
        top += draw_choice(dc, left, top, width, "Save slot",
                           "Slot " + std::to_string(screen.adventure_slot + 1),
                           screen, AdventureSlotControl) + Spacing;
        const SlotInfo& slot = screen.slots[static_cast<std::size_t>(
            std::clamp(screen.adventure_slot, 0,
                       static_cast<int>(SlotCount) - 1))];
        // Nothing starts while co-op is ticked: the mode does not exist yet,
        // and a button that starts a single-player game after being asked for
        // a co-op one is worse than a button that will not go.
        const bool ready = !screen.host_coop;
        top += draw_note(dc, left, top, width,
                         ready ? slot.describe()
                               : "Online co-op is not built yet. Untick it to "
                                 "play.",
                         screen.note_font, theme.text_dim) + Spacing;
        top += draw_choice(dc, left, top, width, "Hunter",
                           std::string(hunters[static_cast<std::size_t>(
                               screen.adventure_hunter)]),
                           screen, AdventureHunterControl) + Spacing;
        const RECT start = make_rect(left, top, width, PrimaryHeight);
        // Nothing to continue in an empty slot, so the only button that means
        // anything there is the one that starts a game.
        draw_primary(dc, start,
                     slot.used ? "CONTINUE" : "START A NEW GAME",
                     screen.primary_font,
                     screen.hovered == AdventureStartControl,
                     screen.pressed == AdventureStartControl, ready);
        screen.hotspots.push_back(Hotspot{start, AdventureStartControl,
                                          ready});
        top += PrimaryHeight + Spacing;
        if (slot.used) {
            const RECT fresh = make_rect(left, top, width, PlainHeight);
            draw_entry(dc, fresh, "NEW GAME", "", screen.entry_font,
                       screen.note_font,
                       screen.hovered == AdventureNewControl, ready);
            screen.hotspots.push_back(Hotspot{fresh, AdventureNewControl,
                                              ready});
            top += PlainHeight + Spacing;
        }
    } else {
        // Local or online is what used to be two entries on the front screen
        // -- "play offline" and "host a game" -- which is the same match with
        // a server in front of it.  One row says which.
        top += draw_choice(dc, left, top, width, "Where",
                           screen.host_where == 0 ? "Local" : "Online", screen,
                           HostWhereControl) + Spacing;
        const std::string map = screen.rooms.empty()
            ? std::string("(no maps)")
            : screen.rooms[static_cast<std::size_t>(std::clamp(
                  screen.match_map, 0,
                  static_cast<int>(screen.rooms.size()) - 1))];
        top += draw_choice(dc, left, top, width, "Map", map, screen,
                           MatchMapControl, !screen.rooms.empty()) + Spacing;
        // Stepping through maps one at a time is the right gesture while the
        // picture beside it changes as you step, and the wrong one when the
        // map you want is twenty steps away.
        const RECT browse = make_rect(left, top, width, PlainHeight);
        draw_entry(dc, browse, "SEE EVERY MAP", "", screen.small_entry_font,
                   screen.note_font, screen.hovered == BrowseMapsControl,
                   true);
        screen.hotspots.push_back(Hotspot{browse, BrowseMapsControl, true});
        top += PlainHeight + Spacing;
        // Not "Mode": the row above already says Adventure or Battle, and two
        // rows called Mode under each other is a card nobody can read.
        top += draw_choice(dc, left, top, width, "Match type",
                           std::string(modes[static_cast<std::size_t>(
                               screen.match_mode)].label),
                           screen, MatchModeControl) + Spacing;
        top += draw_choice(dc, left, top, width, "Hunter",
                           std::string(hunters[static_cast<std::size_t>(
                               screen.match_hunter)]),
                           screen, MatchHunterControl) + Spacing;
        // A match the directory runs has no bots of this machine's choosing,
        // so the two rows that set them are not shown for one.
        const bool hosted = screen.host_where == 1;
        if (!hosted) {
            top += draw_choice(dc, left, top, width, "Bots",
                               std::to_string(screen.match_bots), screen,
                               MatchBotsControl) + Spacing;
            const char* skills[] = {"Easy", "Normal", "Hard"};
            top += draw_choice(dc, left, top, width, "Bot skill",
                               skills[std::clamp(screen.match_skill, 0, 2)],
                               screen, MatchSkillControl) + Spacing;
        } else {
            top += draw_note(dc, left, top, width,
                             "The directory runs the match, so nothing here "
                             "needs a forwarded port. To run one on your own "
                             "machine, use the dedicated server.",
                             screen.note_font, theme.text_dim) + Spacing;
        }
        const RECT start = make_rect(left, top, width, PrimaryHeight);
        draw_primary(dc, start, "START", screen.primary_font,
                     screen.hovered == MatchStartControl,
                     screen.pressed == MatchStartControl, true);
        screen.hotspots.push_back(Hotspot{start, MatchStartControl, true});
        top += PrimaryHeight + Spacing;
    }

    const RECT back = make_rect(left, top, width, PlainHeight);
    draw_entry(dc, back, "BACK", "", screen.small_entry_font, screen.note_font,
               screen.hovered == BackControl, true);
    screen.hotspots.push_back(Hotspot{back, BackControl, true});
    top += PlainHeight + Spacing;
}

void paint(HWND window, Screen& screen) {
    PAINTSTRUCT paint_struct{};
    const HDC dc = BeginPaint(window, &paint_struct);
    RECT client{};
    GetClientRect(window, &client);
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    // Drawn into a back buffer: the card is repainted on every pointer move,
    // and painting it straight to the window flickers.
    const HDC buffer = CreateCompatibleDC(dc);
    const HBITMAP surface = CreateCompatibleBitmap(dc, std::max(width, 1),
                                                   std::max(height, 1));
    const HGDIOBJ old_surface = SelectObject(buffer, surface);

    const Palette& theme = palette();
    fill(buffer, client, theme.panel);
    screen.hotspots.clear();
    if (screen.view.overlay() == HomeView::SettingsOverlay) {
        draw_settings(buffer, client, screen);
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old_surface);
        DeleteObject(surface);
        DeleteDC(buffer);
        EndPaint(window, &paint_struct);
        return;
    }
    if (screen.view.overlay() == HomeView::MapPickerOverlay) {
        draw_map_picker(buffer, client, screen);
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old_surface);
        DeleteObject(surface);
        DeleteDC(buffer);
        EndPaint(window, &paint_struct);
        return;
    }
    if (screen.view.overlay() == HomeView::DemoPickerOverlay) {
        draw_demo_picker(buffer, client, screen);
        BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old_surface);
        DeleteObject(surface);
        DeleteDC(buffer);
        EndPaint(window, &paint_struct);
        return;
    }
    screen.scroll = 0;
    screen.scroll_extent = 0;
    screen.view.set_width(static_cast<double>(width));
    const HomeLayout& layout = screen.view.layout();
    if (layout.narrow) {
        const int splash_height = static_cast<int>(layout.splash_height);
        draw_splash(buffer, make_rect(0, 0, width, splash_height), screen);
        draw_card(buffer, make_rect(0, splash_height, width,
                                    height - splash_height), screen);
    } else {
        const int panel_width = std::min(
            static_cast<int>(layout.panel_width), width);
        draw_splash(buffer, make_rect(0, 0, width - panel_width, height),
                    screen);
        draw_card(buffer, make_rect(width - panel_width, 0, panel_width,
                                    height), screen);
    }

    BitBlt(dc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, old_surface);
    DeleteObject(surface);
    DeleteDC(buffer);
    EndPaint(window, &paint_struct);
}

[[nodiscard]] int hit_test(const Screen& screen, POINT point) {
    for (const Hotspot& spot : screen.hotspots) {
        if (spot.enabled && PtInRect(&spot.bounds, point) != 0) {
            return spot.id;
        }
    }
    return 0;
}

// Posted by a probe thread when it has something new to show.  The work is
// never done on the painting thread: the directory and each server are asked
// over UDP with a timeout, and a launcher that will not draw until they answer
// looks broken on a bad connection.
constexpr UINT RefreshMessage = WM_APP + 1;
constexpr UINT_PTR StatusTimer = 1;
// A binding row waiting for a button polls the pad rather than waiting for a
// message: XInput has no window messages, and sixty hertz is what reading a
// button press needs to feel immediate.
constexpr UINT_PTR PadTimer = 2;
// While the cartridge is being unpacked: the worker has no way to reach the
// window, so the window looks at what it has written four times a second.
constexpr UINT_PTR SetupTimer = 3;

// Ask the directory who is up, then ask each of them directly.  Directly, not
// through the directory: the round trip that matters is this machine's, and an
// answer also proves the server is reachable from here rather than only from
// there.
void reload_servers(HWND window, Screen& screen) {
    const auto browse = screen.browse;
    const Preferences preferences = load_preferences(screen.directory);
    int generation = 0;
    {
        const std::lock_guard<std::mutex> guard(browse->lock);
        browse->rows.clear();
        browse->note = "Asking " + preferences.master_host + "...";
        browse->note_color = palette().text_dim;
        generation = ++browse->generation;
    }
    InvalidateRect(window, nullptr, FALSE);
    std::thread([browse, window, generation, host = preferences.master_host,
                 port = preferences.master_port]() {
        const auto result = net::MasterClient::query(
            host, static_cast<std::uint16_t>(port));
        std::vector<Browse::Row> probes;
        {
            const std::lock_guard<std::mutex> guard(browse->lock);
            if (browse->generation != generation) {
                return;
            }
            if (!result.answered) {
                browse->note = "The directory did not answer. It may be down, "
                               "or UDP may not reach it.";
                browse->note_color = palette().warm;
            } else if (result.servers.empty()) {
                browse->note = "The directory is up and has nobody listed.";
                browse->note_color = palette().warm;
            } else {
                browse->note = std::to_string(result.servers.size())
                    + " listed.";
                browse->note_color = palette().text_dim;
                for (const auto& listing : result.servers) {
                    Browse::Row row;
                    row.address = listing.address;
                    row.port = listing.port;
                    row.endpoint = listing.address + ":"
                        + std::to_string(listing.port);
                    row.name = listing.server_name.empty() ? row.endpoint
                                                           : listing.server_name;
                    browse->rows.push_back(row);
                }
                probes = browse->rows;
            }
        }
        if (IsWindow(window)) {
            PostMessageA(window, RefreshMessage, 0, 0);
        }
        for (std::size_t i = 0; i < probes.size(); ++i) {
            std::thread([browse, window, generation, i,
                         address = probes[i].address,
                         port = probes[i].port]() {
                const auto status = net::query(address, port, false);
                const std::lock_guard<std::mutex> guard(browse->lock);
                if (browse->generation != generation
                    || i >= browse->rows.size()) {
                    return;
                }
                Browse::Row& row = browse->rows[i];
                row.answered = status.online;
                if (!status.online) {
                    row.map = "did not answer";
                    row.mode.clear();
                    row.players.clear();
                    row.ping = "--";
                    row.latency = -1;
                } else {
                    row.map = status.room_key;
                    row.mode = net::mode_name(status.mode);
                    row.players = status.max_players > 0
                        ? std::to_string(status.players) + "/"
                            + std::to_string(status.max_players)
                        : std::to_string(status.players);
                    row.latency = status.latency_ms;
                    row.ping = status.latency_ms >= 0
                        ? std::to_string(status.latency_ms) : "--";
                }
                if (IsWindow(window)) {
                    PostMessageA(window, RefreshMessage, 0, 0);
                }
            }).detach();
        }
    }).detach();
}

// HomeView.ParseEndpoint: "host", "host:port" or an IPv6 literal in brackets.
void split_endpoint(const std::string& value, std::string& host,
                    std::uint16_t& port) {
    const std::size_t colon = value.rfind(':');
    if (colon == std::string::npos || colon + 1 >= value.size()) {
        if (!value.empty()) {
            host = value;
        }
        return;
    }
    const std::string tail = value.substr(colon + 1);
    if (tail.find_first_not_of("0123456789") != std::string::npos) {
        host = value;
        return;
    }
    host = value.substr(0, colon);
    const long parsed = std::strtol(tail.c_str(), nullptr, 10);
    if (parsed > 0 && parsed <= 65535) {
        port = static_cast<std::uint16_t>(parsed);
    }
}

// StatusQuery answers without claiming a slot, which is what makes polling it
// reasonable while somebody is reading the card.
void query_status_soon(HWND window, Screen& screen) {
    const auto browse = screen.browse;
    std::string host;
    std::uint16_t port = net::NetConfig::DefaultPort;
    split_endpoint(screen.online_address, host, port);
    {
        const std::lock_guard<std::mutex> guard(browse->lock);
        if (browse->note.empty()) {
            browse->note = "Checking...";
        }
    }
    std::thread([browse, window, host, port]() {
        const auto status = net::query(host, port, true);
        {
            const std::lock_guard<std::mutex> guard(browse->lock);
            browse->note = status.online
                ? net::mode_name(status.mode) + " on " + status.room_key + ", "
                    + std::to_string(status.players) + "/"
                    + std::to_string(status.max_players) + " players"
                : (status.message.empty() ? std::string("No answer.")
                                          : status.message);
            browse->note_color = status.online ? palette().text_dim
                                               : palette().warm;
        }
        if (IsWindow(window)) {
            PostMessageA(window, RefreshMessage, 0, 0);
        }
    }).detach();
}



// Which settings rows are a slider.  A press on one is not a step, so it
// never reaches activate() -- the pointer sets it directly.
[[nodiscard]] bool is_slider_row(int index) noexcept {
    return index == 1 || index == 20 || index == 21 || index == 30
        || index == 34 || index == 35;
}

// The tiles the map picker can show a picture for.  Decoded once, on the way
// into the picker: a full set is thirty 1600x900 PNGs, and decoding them while
// the grid scrolls would stutter.
void load_previews(Screen& screen) {
    if (!screen.previews.empty()) {
        return;
    }
    for (const std::string& room : screen.rooms) {
        const auto path = mods::thumbnail::path_for(screen.directory, room);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            continue;
        }
        auto image = std::make_shared<Image>();
        // A truncated PNG from an interrupted batch shows the placeholder
        // rather than taking the window down.
        if (image->load(path)) {
            screen.previews.emplace(room, std::move(image));
        }
    }
}

// Rows.ChoiceRow.Step: wrapping, because the lists are short and running
// off the end of one is more annoying than useful.
void step(int& index, int direction, int count);
void settings_row(HWND window, Screen& screen, int index);
void poll_pad_binding(HWND window, Screen& screen);
void drag_slider(Screen& screen, int index, int x);
void begin_setup(HWND window, Screen& screen,
                 const std::filesystem::path& rom);
void open_settings(HWND window, Screen& screen);
void commit_settings(HWND window, Screen& screen);

void step(int& index, int direction, int count) {
    if (count <= 0) {
        index = 0;
        return;
    }
    index = ((index + direction) % count + count) % count;
}



// One frame of the pad, while a binding row is listening.
void poll_pad_binding(HWND window, Screen& screen) {
    if (screen.listening_pad < 0 || screen.pad == nullptr) {
        KillTimer(window, PadTimer);
        return;
    }
    input::GamepadState state;
    if (!screen.pad->poll(state) || !state.connected) {
        return;
    }
    // Only what was pressed since the row started listening counts.
    const auto pressed = static_cast<input::GamepadButtons>(
        static_cast<std::uint16_t>(state.buttons)
        & ~static_cast<std::uint16_t>(screen.pad_seen));
    screen.pad_seen = state.buttons;
    if (pressed == input::GamepadButtons::None) {
        return;
    }
    const auto& actions = input::pad_bindings::actions();
    const auto index = static_cast<std::size_t>(screen.listening_pad);
    if (index < actions.size()) {
        screen.input.pad_bindings.*pad_member(actions[index]) = pressed;
    }
    screen.listening_pad = -1;
    KillTimer(window, PadTimer);
    InvalidateRect(window, nullptr, FALSE);
}

// Unpack a cartridge into files/ beside the program, the way the managed
// launcher's setup card does.  Picking a .nds is not the setup: the extraction
// is, and until it has run there is nothing for a match to read.
void begin_setup(HWND window, Screen& screen,
                 const std::filesystem::path& rom) {
    {
        const std::lock_guard<std::mutex> guard(screen.setup->lock);
        if (screen.setup->running) {
            return;
        }
        screen.setup->lines.clear();
        screen.setup->running = true;
        screen.setup->finished = false;
        screen.setup->ok = false;
        screen.setup->game_root.clear();
    }
    if (screen.setup_thread.joinable()) {
        screen.setup_thread.join();
    }
    screen.view.set_game_files_ready(false, {});
    // Captured by value: the thread outlives nothing it reads, and the block
    // it writes into is shared so the window can be gone before it finishes.
    auto state = screen.setup;
    const auto root = screen.directory;
    screen.setup_thread = std::thread([state, root, rom]() {
        const auto result = GameFiles::run_setup(root, rom,
            [&state](std::string_view line) {
                const std::lock_guard<std::mutex> guard(state->lock);
                state->lines.emplace_back(line);
            });
        const std::lock_guard<std::mutex> guard(state->lock);
        if (!result.error.empty()) {
            state->lines.push_back(result.error);
        }
        state->ok = result.ok;
        state->game_root = result.game_root.string();
        state->running = false;
        state->finished = true;
    });
    SetTimer(window, SetupTimer, 250, nullptr);
    InvalidateRect(window, nullptr, FALSE);
}

// The settings pages are opened on a working copy: Cancel has to leave the
// files exactly as they were, which is not something an in-place edit can
// promise.
void open_settings(HWND window, Screen& screen) {
    screen.menu = settings::load_menu(screen.directory);
    screen.input = settings::load_input(screen.directory);
    screen.render = settings::load_render(screen.directory);
    screen.prefs = load_preferences(screen.directory);
    screen.window_mode =
        screen.prefs.window_mode == window::StartMode::BorderlessFullscreen
            ? 1 : 0;
    screen.pro_hud = json_flag(screen.menu.features_json, "ProHud");
    screen.field_point_goal = screen.menu.point_goal;
    screen.field_time_limit = screen.menu.time_limit;
    screen.field_player_name = screen.prefs.player_name;
    screen.field_server = screen.prefs.server_address + ":"
        + std::to_string(screen.prefs.server_port);
    screen.field_master = screen.prefs.master_host + ":"
        + std::to_string(screen.prefs.master_port);
    screen.damage_row = screen.menu.damage_level == "low" ? 0
        : screen.menu.damage_level == "high" ? 2 : 1;
    screen.language_row = 0;
    for (std::size_t i = 0; i < settings_languages().size(); ++i) {
        if (settings_languages()[i] == screen.menu.language) {
            screen.language_row = static_cast<int>(i);
            break;
        }
    }
    screen.hunter_row = std::clamp(
        static_cast<int>(screen.prefs.last_hunter), 0, 7);
    screen.save_error.clear();
    screen.settings_section = 0;
    screen.scroll = 0;
    screen.listening_pad = -1;
    static_cast<void>(screen.view.open_overlay(HomeView::SettingsOverlay));
    InvalidateRect(window, nullptr, FALSE);
}

void commit_settings(HWND window, Screen& screen) {
    screen.menu.point_goal = screen.field_point_goal;
    screen.menu.time_limit = screen.field_time_limit;
    const char* damage[] = {"low", "medium", "high"};
    screen.menu.damage_level = damage[std::clamp(screen.damage_row, 0, 2)];
    screen.menu.language =
        settings_languages()[static_cast<std::size_t>(screen.language_row)];
    screen.prefs.player_name = screen.field_player_name;
    screen.prefs.last_hunter =
        static_cast<metadata::Hunter>(std::clamp(screen.hunter_row, 0, 7));
    // A typo leaves the address alone rather than silently changing it.
    split_endpoint(screen.field_server, screen.prefs.server_address,
                   screen.prefs.server_port);
    split_endpoint(screen.field_master, screen.prefs.master_host,
                   screen.prefs.master_port);
    screen.prefs.window_mode = screen.window_mode == 1
        ? window::StartMode::BorderlessFullscreen
        : window::StartMode::Windowed;
    screen.prefs.rom_path = screen.selection.rom_path;

    // Writing touches the disk, and the disk is allowed to say no -- a
    // read-only folder, a file open elsewhere, a full drive.  That is worth a
    // line on the screen, not a window that vanishes.
    // The render values are stored as strings inside the same settings file,
    // so they are written back into it rather than saved on their own.
    const auto on_off = [](bool value) {
        return std::string(value ? "on" : "off");
    };
    screen.menu.resolution_scale =
        std::to_string(screen.render.resolution_scale);
    screen.menu.lighting = on_off(screen.render.lighting);
    screen.menu.fog = on_off(screen.render.fog);
    screen.menu.texture_filtering = on_off(screen.render.texture_filtering);
    screen.menu.show_fps = on_off(screen.render.show_fps);
    screen.menu.cel_shading = on_off(screen.render.cel_shading);
    screen.menu.features_json =
        set_json_flag(screen.menu.features_json, "ProHud", screen.pro_hud);
    const bool saved = settings::save_menu(screen.directory, screen.menu)
        && settings::save_input(screen.directory, screen.input)
        && save_preferences(screen.directory, screen.prefs);
    if (!saved) {
        screen.save_error = "Could not save. Check the folder is writable.";
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    // The renderer reads its own copy, so what was just written has to reach
    // it as well as the file.
    auto& options = mods::render::options();
    options.set_resolution_scale(screen.render.resolution_scale);
    options.set_lighting(screen.render.lighting);
    options.set_fog(screen.render.fog);
    options.set_texture_filtering(screen.render.texture_filtering);
    options.set_show_fps(screen.render.show_fps);
    options.set_cel_shading(screen.render.cel_shading);
    screen.save_error.clear();
    screen.view.close_overlay();
    screen.focused_field = 0;
    screen.scroll = 0;
    InvalidateRect(window, nullptr, FALSE);
}

// One settings row was pressed.  The index is the row's own, so the switch
// reads in the order the pages draw them.
void apply_slider(Screen& screen, int index, int percent) {
    percent = std::clamp(percent, 0, 100);
    switch (index) {
    case 1:
        screen.render.resolution_scale =
            std::max(mods::render::Options::MinScale, percent);
        break;
    case 20: screen.menu.sfx_volume = percent_to_stored(percent); break;
    case 21: screen.menu.music_volume = percent_to_stored(percent); break;
    case 30:
        screen.input.mouse_sensitivity = slider_to_sensitivity(percent);
        break;
    case 34:
        screen.input.gamepad_look_sensitivity = slider_to_look(percent);
        break;
    case 35:
        screen.input.gamepad_dead_zone = slider_to_dead_zone(percent);
        break;
    default: break;
    }
}

// The rail's own span, recovered from the hotspot the row pushed: draw_slider
// pads it by eight pixels either side so the knob can be grabbed at the ends.
void drag_slider(Screen& screen, int index, int x) {
    for (const Hotspot& spot : screen.hotspots) {
        if (spot.id != SettingsRowControl + index) {
            continue;
        }
        const int first = static_cast<int>(spot.bounds.left) + 8;
        const int last = static_cast<int>(spot.bounds.right) - 8;
        if (last <= first) {
            return;
        }
        apply_slider(screen, index,
                     (x - first) * 100 / (last - first));
        return;
    }
}

void settings_row(HWND window, Screen& screen, int index) {
    const auto toggle = [](bool& value) { value = !value; };
    const auto toggle_word = [](std::string& value) {
        value = value == "on" ? "off" : "on";
    };
    const bool back = index >= BackArrowBias;
    if (back) {
        index -= BackArrowBias;
    }
    switch (index) {
    case 0: screen.window_mode = screen.window_mode == 1 ? 0 : 1; break;
    case 2: toggle(screen.render.lighting); break;
    case 3: toggle(screen.render.fog); break;
    case 4: toggle(screen.render.texture_filtering); break;
    case 5: toggle(screen.render.show_fps); break;
    case 6: toggle(screen.render.cel_shading); break;
    case 7: toggle(screen.pro_hud); break;
    case 22:
        step(screen.language_row, back ? -1 : 1,
             static_cast<int>(settings_languages().size()));
        break;
    case 31: toggle(screen.input.invert_mouse_y); break;
    case 32: toggle(screen.input.invert_mouse_x); break;
    case 33: toggle(screen.input.scroll_all_weapons); break;
    case 36: toggle(screen.input.gamepad_invert_y); break;
    case 40:
    case 41:
    case 50:
    case 52:
    case 53:
        screen.focused_field = SettingsRowControl + index;
        if (const std::string* field = focused_text(screen)) {
            screen.caret = field->size();
        }
        break;
    case 42: step(screen.damage_row, back ? -1 : 1, 3); break;
    case 43: toggle_word(screen.menu.team_play); break;
    case 44: toggle_word(screen.menu.friendly_fire); break;
    case 45: toggle_word(screen.menu.hunter_radar); break;
    case 46: toggle_word(screen.menu.affinity_weapons); break;
    case 51:
        step(screen.hunter_row, back ? -1 : 1,
             static_cast<int>(HomeView::hunter_options().size()));
        break;
    case 54: toggle(screen.prefs.auto_update); break;
    default:
        if (index >= 60 && index < 60 + 16) {
            // A press starts listening; the next pad button is the binding.
            screen.listening_pad = screen.listening_pad == index - 60
                ? -1 : index - 60;
            if (screen.listening_pad >= 0) {
                if (screen.pad == nullptr) {
                    screen.pad = std::make_unique<input::Gamepad>();
                }
                // Whatever is held when the row starts listening is not the
                // answer: a trigger still down from the click that opened it
                // would bind itself instantly.
                input::GamepadState state;
                screen.pad_seen = screen.pad->poll(state)
                    ? state.buttons : input::GamepadButtons::None;
                SetTimer(window, PadTimer, 16, nullptr);
            } else {
                KillTimer(window, PadTimer);
            }
        }
        break;
    }
    InvalidateRect(window, nullptr, FALSE);
}

void activate(HWND window, Screen& screen, int control) {
    switch (control) {
    case ChooseRomControl: {

        const std::string chosen = pick_rom(window);
        if (chosen.empty()) {
            return;
        }
        std::error_code error;
        if (!std::filesystem::is_regular_file(chosen, error)) {
            screen.view.set_game_files_ready(
                false, "That file could not be read. Pick the .nds again.");
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        begin_setup(window, screen, chosen);
        return;
    }
    case HostControl:
        // Host opens on the story, which is what it defaults to.
        screen.host_mode = 0;
        static_cast<void>(screen.view.show_card(HomeCard::Host));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case BackControl:
        KillTimer(window, StatusTimer);
        screen.focused_field = 0;
        if (screen.view.card() == HomeCard::Online) {
            // Back from a server goes to the list, not to the front screen.
            static_cast<void>(screen.view.show_card(HomeCard::Browse));
            reload_servers(window, screen);
            return;
        }
        static_cast<void>(screen.view.go_back());
        InvalidateRect(window, nullptr, FALSE);
        return;
    case JoinControl:
        // Join opens the list itself rather than a "find a server" entry.
        screen.online_address.clear();
        static_cast<void>(screen.view.show_card(HomeCard::Browse));
        reload_servers(window, screen);
        return;
    case BrowseRefreshControl:
        reload_servers(window, screen);
        return;
    case OnlineHunterControl:
    case OnlineHunterControl + BackArrowBias:
        step(screen.online_hunter, control == OnlineHunterControl ? 1 : -1,
             static_cast<int>(HomeView::hunter_options().size()));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case OnlineAddressControl:
        screen.focused_field = OnlineAddressControl;
        screen.caret = screen.online_address.size();
        InvalidateRect(window, nullptr, FALSE);
        return;
    case ConnectControl: {
        std::string host;
        std::uint16_t port = net::NetConfig::DefaultPort;
        split_endpoint(screen.online_address, host, port);
        if (host.empty()) {
            const std::lock_guard<std::mutex> guard(screen.browse->lock);
            screen.browse->note = "Type a server address first.";
            screen.browse->note_color = palette().warm;
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        screen.selection.connect_host = host;
        screen.selection.connect_port = port;
        screen.selection.hunter =
            static_cast<std::uint8_t>(screen.online_hunter);
        screen.selection.adventure = false;
        screen.accepted = true;
        DestroyWindow(window);
        return;
    }
    case DemosControl:
        screen.demos = demo::list_recordings(
            demo::demos_directory(screen.directory));
        screen.scroll = 0;
        static_cast<void>(screen.view.open_overlay(
            HomeView::DemoPickerOverlay));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case SettingsControl:
        open_settings(window, screen);
        return;
    case SettingsSaveControl:
        commit_settings(window, screen);
        return;
    case SettingsCancelControl:
        screen.listening_pad = -1;
        KillTimer(window, PadTimer);
        screen.view.close_overlay();
        screen.focused_field = 0;
        screen.scroll = 0;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case SettingsResetControl:
        // InputSettings.Reset puts the pad's buttons back too, so the rows
        // only have to be redrawn.
        screen.input = settings::InputConfig{};
        input::pad_bindings::reset();
        screen.listening_pad = -1;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case SettingsFilesControl: {
        const std::string chosen = pick_rom(window);
        if (!chosen.empty()) {
            // Setting the files up again is the same work from either card,
            // so it is the same call; the overlay closes because the progress
            // it reports is drawn on the card underneath.
            screen.view.close_overlay();
            begin_setup(window, screen, chosen);
            return;
        }
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    case HostModeControl:
    case HostModeControl + BackArrowBias:
        screen.host_mode = screen.host_mode == 0 ? 1 : 0;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case HostCoopControl:
        screen.host_coop = !screen.host_coop;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case HostWhereControl:
    case HostWhereControl + BackArrowBias:
        screen.host_where = screen.host_where == 0 ? 1 : 0;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case AdventureSlotControl:
    case AdventureSlotControl + BackArrowBias:
        step(screen.adventure_slot, control == AdventureSlotControl ? 1 : -1,
             static_cast<int>(SlotCount));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case AdventureHunterControl:
    case AdventureHunterControl + BackArrowBias:
        step(screen.adventure_hunter,
             control == AdventureHunterControl ? 1 : -1,
             static_cast<int>(HomeView::hunter_options().size()));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case MatchMapControl:
    case MatchMapControl + BackArrowBias:
        step(screen.match_map, control == MatchMapControl ? 1 : -1,
             static_cast<int>(screen.rooms.size()));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case MatchModeControl:
    case MatchModeControl + BackArrowBias:
        step(screen.match_mode, control == MatchModeControl ? 1 : -1,
             static_cast<int>(HomeView::mode_options().size()));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case MatchHunterControl:
    case MatchHunterControl + BackArrowBias:
        step(screen.match_hunter, control == MatchHunterControl ? 1 : -1,
             static_cast<int>(HomeView::hunter_options().size()));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case MatchBotsControl:
    case MatchBotsControl + BackArrowBias:
        step(screen.match_bots, control == MatchBotsControl ? 1 : -1, 8);
        InvalidateRect(window, nullptr, FALSE);
        return;
    case MatchSkillControl:
    case MatchSkillControl + BackArrowBias:
        step(screen.match_skill, control == MatchSkillControl ? 1 : -1, 3);
        InvalidateRect(window, nullptr, FALSE);
        return;
    case BrowseMapsControl:
        // Stepping through maps one at a time is the wrong gesture when the
        // map you want is twenty steps away, so this is every map at once.
        load_previews(screen);
        screen.scroll = 0;
        static_cast<void>(screen.view.open_overlay(
            HomeView::MapPickerOverlay));
        InvalidateRect(window, nullptr, FALSE);
        return;
    case PickerBackControl:
        screen.view.close_overlay();
        screen.scroll = 0;
        InvalidateRect(window, nullptr, FALSE);
        return;
    case DemoImportControl: {
        // The picker is still here, as the last entry, for the demo that came
        // from somewhere else.
        std::vector<char> buffer(MAX_PATH * 4, '\0');
        OPENFILENAMEA dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = window;
        dialog.lpstrFilter = "Fruity Prime demo (*.fpdemo)\0*.fpdemo\0"
                             "All files (*.*)\0*.*\0";
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
            | OFN_HIDEREADONLY;
        dialog.lpstrTitle = "Open a demo";
        if (GetOpenFileNameA(&dialog) == FALSE) {
            return;
        }
        screen.selection.demo_path = buffer.data();
        screen.accepted = true;
        DestroyWindow(window);
        return;
    }
    case AdventureStartControl:
    case AdventureNewControl: {
        const auto slot = static_cast<std::uint8_t>(
            std::clamp(screen.adventure_slot + 1, 1,
                       static_cast<int>(SlotCount)));
        // An empty slot has nothing to continue, so Continue starts a game
        // there whichever button was pressed.
        const bool used = screen.slots[static_cast<std::size_t>(slot - 1)].used;
        screen.selection.adventure = true;
        screen.selection.save_slot = slot;
        screen.selection.new_game =
            control == AdventureNewControl || !used;
        screen.selection.hunter =
            static_cast<std::uint8_t>(screen.adventure_hunter);
        screen.accepted = true;
        DestroyWindow(window);
        return;
    }
    case MatchStartControl: {
        if (!screen.rooms.empty()) {
            screen.selection.room_name = screen.rooms[
                static_cast<std::size_t>(std::clamp(
                    screen.match_map, 0,
                    static_cast<int>(screen.rooms.size()) - 1))];
        }
        const auto modes = HomeView::mode_options();
        screen.selection.mode = static_cast<std::uint8_t>(
            modes[static_cast<std::size_t>(screen.match_mode)].mode);
        screen.selection.hunter =
            static_cast<std::uint8_t>(screen.match_hunter);
        screen.selection.bots = screen.match_bots;
        screen.selection.bot_level = screen.match_skill;
        screen.selection.adventure = false;
        screen.accepted = true;
        DestroyWindow(window);
        return;
    }
    case QuitControl:
        DestroyWindow(window);
        return;
    default:
        if (control >= SettingsSectionControl
            && control < SettingsSectionControl + 16) {
            screen.settings_section = control - SettingsSectionControl;
            screen.scroll = 0;
            screen.focused_field = 0;
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        if (control >= SettingsRowControl
            && control < SettingsRowControl + 1000) {
            settings_row(window, screen, control - SettingsRowControl);
            return;
        }
        if (control >= PickerItemControl) {
            const std::size_t index =
                static_cast<std::size_t>(control - PickerItemControl);
            if (screen.view.overlay() == HomeView::MapPickerOverlay) {
                if (index < screen.rooms.size()) {
                    screen.match_map = static_cast<int>(index);
                }
            } else if (index < screen.demos.size()) {
                screen.selection.demo_path =
                    screen.demos[index].path.string();
                screen.accepted = true;
                DestroyWindow(window);
                return;
            }
            screen.view.close_overlay();
            screen.scroll = 0;
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        if (control >= ServerRowControl) {
            const std::size_t index =
                static_cast<std::size_t>(control - ServerRowControl);
            const std::lock_guard<std::mutex> guard(screen.browse->lock);
            if (index < screen.browse->rows.size()) {
                screen.online_address = screen.browse->rows[index].endpoint;
                screen.online_hunter =
                    static_cast<int>(screen.selection.hunter);
                screen.browse->note = "Checking...";
                screen.browse->note_color = palette().text_dim;
            }
        }
        if (control >= ServerRowControl) {
            static_cast<void>(screen.view.show_card(HomeCard::Online));
            query_status_soon(window, screen);
            SetTimer(window, StatusTimer, 4000, nullptr);
            InvalidateRect(window, nullptr, FALSE);
        }
        return;
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                             LPARAM lparam) {
    auto* screen = reinterpret_cast<Screen*>(
        GetWindowLongPtrA(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        const auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
        SetWindowLongPtrA(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }
    case WM_ERASEBKGND:
        // The paint handler covers every pixel; letting the class brush run
        // first would flash the default grey.
        return 1;
    case WM_PAINT:
        if (screen != nullptr) {
            paint(window, *screen);
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        RECT frame = make_rect(0, 0,
                               static_cast<int>(HomeWindow::MinWidth),
                               static_cast<int>(HomeWindow::MinHeight));
        AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
        info->ptMinTrackSize.x = frame.right - frame.left;
        info->ptMinTrackSize.y = frame.bottom - frame.top;
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        if (screen != nullptr && screen->scroll_extent > 0) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            screen->scroll = std::clamp(
                screen->scroll - delta * 60 / WHEEL_DELTA, 0,
                screen->scroll_extent);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        if (screen == nullptr) {
            break;
        }
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (screen->dragging_slider >= 0) {
            drag_slider(*screen, screen->dragging_slider, point.x);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        const int hovered = hit_test(*screen, point);
        if (hovered != screen->hovered) {
            screen->hovered = hovered;
            SetCursor(LoadCursor(nullptr, hovered != 0 ? IDC_HAND
                                                       : IDC_ARROW));
            InvalidateRect(window, nullptr, FALSE);
        }
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = window;
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (screen != nullptr && screen->hovered != 0) {
            screen->hovered = 0;
            screen->pressed = 0;
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        if (screen == nullptr) {
            break;
        }
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        screen->pressed = hit_test(*screen, point);
        if (screen->focused_field != 0
            && screen->pressed != screen->focused_field) {
            const bool address =
                screen->focused_field == OnlineAddressControl;
            screen->focused_field = 0;
            if (address) {
                // The managed box asks the server what it is running when it
                // loses focus.
                query_status_soon(window, *screen);
            }
            InvalidateRect(window, nullptr, FALSE);
        }
        if (screen->pressed >= SettingsRowControl
            && screen->pressed < SettingsRowControl + 1000
            && is_slider_row(screen->pressed - SettingsRowControl)) {
            // Grabbing a rail sets it at once, so the value follows the
            // pointer from the first pixel rather than after the release.
            screen->dragging_slider = screen->pressed - SettingsRowControl;
            drag_slider(*screen, screen->dragging_slider, point.x);
        }
        if (screen->pressed != 0) {
            SetCapture(window);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (screen == nullptr) {
            break;
        }
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const int pressed = screen->pressed;
        screen->pressed = 0;
        if (screen->dragging_slider >= 0) {
            screen->dragging_slider = -1;
            ReleaseCapture();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (pressed != 0) {
            ReleaseCapture();
            InvalidateRect(window, nullptr, FALSE);
            // Only a release over the same row counts, so a press dragged off
            // a button cancels the way every other button does.
            if (hit_test(*screen, point) == pressed) {
                activate(window, *screen, pressed);
            }
        }
        return 0;
    }
    case RefreshMessage:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_TIMER:
        if (screen != nullptr && wparam == StatusTimer
            && screen->view.card() == HomeCard::Online) {
            query_status_soon(window, *screen);
        }
        if (screen != nullptr && wparam == PadTimer) {
            poll_pad_binding(window, *screen);
        }
        if (screen != nullptr && wparam == SetupTimer) {
            bool done = false;
            bool ok = false;
            std::string root;
            {
                const std::lock_guard<std::mutex> guard(screen->setup->lock);
                done = screen->setup->finished;
                ok = screen->setup->ok;
                root = screen->setup->game_root;
            }
            if (done) {
                KillTimer(window, SetupTimer);
                // What the extraction wrote is what a match reads, so the
                // asset source becomes the tree rather than the cartridge --
                // which the player is then free to move or delete.
                const auto status = GameFiles::inspect(screen->directory);
                if (ok && status.ready) {
                    screen->selection.rom_path =
                        status.mph_file_system.string();
                } else if (ok && !root.empty()) {
                    screen->selection.rom_path = root;
                }
                screen->view.set_game_files_ready(
                    ok && status.ready,
                    ok && status.ready ? std::string{}
                        : status.problem.empty()
                            ? std::string("Setup did not finish.")
                            : status.problem);
            }
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_CHAR:
        if (screen != nullptr && screen->focused_field != 0 && wparam >= 0x20
            && wparam < 0x7f) {
            std::string* field = focused_text(*screen);
            if (field != nullptr) {
                screen->caret = std::min(screen->caret, field->size());
                field->insert(screen->caret, 1, static_cast<char>(wparam));
                ++screen->caret;
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
        }
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && screen != nullptr
            && screen->view.overlay() != 0) {
            screen->view.close_overlay();
            screen->scroll = 0;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (wparam == VK_ESCAPE) {
            if (screen != nullptr && screen->focused_field != 0) {
                screen->focused_field = 0;
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            DestroyWindow(window);
            return 0;
        }
        if (screen != nullptr && screen->focused_field != 0) {
            std::string* field = focused_text(*screen);
            if (field == nullptr) {
                break;
            }
            if (wparam == VK_BACK && screen->caret > 0 && !field->empty()) {
                --screen->caret;
                field->erase(screen->caret, 1);
            } else if (wparam == VK_DELETE && screen->caret < field->size()) {
                field->erase(screen->caret, 1);
            } else if (wparam == VK_LEFT && screen->caret > 0) {
                --screen->caret;
            } else if (wparam == VK_RIGHT && screen->caret < field->size()) {
                ++screen->caret;
            } else if (wparam == VK_HOME) {
                screen->caret = 0;
            } else if (wparam == VK_END) {
                screen->caret = field->size();
            } else if (wparam == VK_RETURN) {
                // The managed box queries when it loses focus; Enter is the
                // gesture that says "I have finished typing this".
                const bool address =
                    screen->focused_field == OnlineAddressControl;
                screen->focused_field = 0;
                if (address) {
                    query_status_soon(window, *screen);
                }
            } else {
                break;
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

} // namespace

bool run_front_screen(const std::filesystem::path& executable_directory,
                      Selection& selection, bool* open_details) {
    if (open_details != nullptr) {
        *open_details = false;
    }
    // What makes a copy ready to play is the extraction, not the cartridge:
    // the .nds is read once and never again, and a player who has moved or
    // deleted theirs since is not back at the setup card.
    const auto files = GameFiles::inspect(executable_directory);
    std::error_code error;
    const bool rom_readable = !selection.rom_path.empty()
        && (std::filesystem::is_regular_file(selection.rom_path, error)
            || std::filesystem::is_directory(selection.rom_path, error));
    const bool files_ready = files.ready || rom_readable;

    Screen screen(executable_directory, selection, files_ready);
    if (files.ready) {
        // Store::from_path reads an extracted tree as readily as a ROM, and
        // this is the tree paths.txt names.
        screen.selection.rom_path = files.mph_file_system.string();
    }
    // "No game files yet" is what the card already says in full; an
    // extraction that is there but stale is not, so that one is worth a line.
    if (!files_ready && !files.problem.empty()
        && files.problem != "No game files yet") {
        screen.view.set_game_files_ready(false, files.problem);
    }
    const auto brand_path = find_brand(executable_directory);
    if (!brand_path.empty()) {
        static_cast<void>(screen.brand.load(brand_path));
    }
    // Before any font is created: CreateFont can only find Inter once the
    // files are on this process's private list.
    PrivateFonts fonts;
    fonts.load(executable_directory);
    screen.caption_font = make_font(11, true);
    screen.note_font = make_font(12, false);
    screen.entry_font = make_font(13, true);
    // MenuEntry's titleSize: 13 for the rows that are an aside -- "see every
    // map", "back" -- against the 15 of a card's own entries.
    screen.small_entry_font = make_font(13, true);
    screen.primary_font = make_font(15, true);
    screen.title_font = make_font(30, true);
    screen.row_font = make_font(13, false);
    screen.row_bold_font = make_font(13, true);
    screen.picker_title_font = make_font(18, false);

    // The rows read the same places the managed card does.
    for (const auto& room : scene::multiplayer_rooms()) {
        screen.rooms.push_back(room.name);
    }
    const auto room_at = std::find(screen.rooms.begin(), screen.rooms.end(),
                                   screen.selection.room_name);
    if (room_at != screen.rooms.end()) {
        screen.match_map = static_cast<int>(
            std::distance(screen.rooms.begin(), room_at));
    }
    screen.slots = SaveStore(executable_directory).read_all();
    screen.browse = std::make_shared<Browse>();
    {
        const Preferences preferences = load_preferences(executable_directory);
        screen.online_address = preferences.server_address + ":"
            + std::to_string(preferences.server_port);
    }
    screen.online_hunter = screen.match_hunter;
    screen.match_hunter = std::clamp<int>(selection.hunter, 0, 7);
    screen.adventure_hunter = screen.match_hunter;
    screen.match_bots = std::clamp(selection.bots, 0, 7);
    screen.match_skill = std::clamp(selection.bot_level, 0, 2);
    {
        const auto modes = HomeView::mode_options();
        for (std::size_t i = 0; i < modes.size(); ++i) {
            if (static_cast<std::uint8_t>(modes[i].mode) == selection.mode) {
                screen.match_mode = static_cast<int>(i);
                break;
            }
        }
    }

    constexpr char ClassName[] = "FruityPrimeFrontScreen";
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = nullptr;  // set per row in WM_MOUSEMOVE
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = ClassName;
    const ATOM registered = RegisterClassExA(&window_class);
    if (registered == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT frame = make_rect(0, 0, static_cast<int>(HomeWindow::Width),
                           static_cast<int>(HomeWindow::Height));
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
    const int width = static_cast<int>(frame.right - frame.left);
    const int height = static_cast<int>(frame.bottom - frame.top);
    const HWND window = CreateWindowExA(
        0, ClassName, std::string(branding::Name).c_str(), WS_OVERLAPPEDWINDOW,
        (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - height) / 2, width, height, nullptr,
        nullptr, instance, &screen);
    if (window == nullptr) {
        UnregisterClassA(ClassName, instance);
        return false;
    }
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    DeleteObject(screen.caption_font);
    DeleteObject(screen.note_font);
    DeleteObject(screen.entry_font);
    DeleteObject(screen.small_entry_font);
    DeleteObject(screen.primary_font);
    DeleteObject(screen.title_font);
    DeleteObject(screen.row_font);
    DeleteObject(screen.row_bold_font);
    DeleteObject(screen.picker_title_font);
    UnregisterClassA(ClassName, instance);
    if (!screen.accepted) {
        return false;
    }
    if (open_details != nullptr) {
        *open_details = screen.open_selection;
    }
    selection = screen.selection;
    return true;
}

#else

bool run_front_screen(const std::filesystem::path&, Selection&, bool*) {
    return false;
}

#endif

} // namespace fruityprime::launcher::gui::win32

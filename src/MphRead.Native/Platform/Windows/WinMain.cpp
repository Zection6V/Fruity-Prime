#include "Metadata/room_metadata.hpp"
#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Assets/game_assets.hpp"
#include "Mods/Chat/ChatBox.hpp"
#include "Mods/Chat/PlayerEntityChatHud.hpp"
#include "Renderer/OpenGL/PlayerEntityChatHudRenderer.hpp"
#include "Formats/collision_query.hpp"
#include "Utility/console_setup.hpp"
#include "Utility/extract.hpp"
#include "Mods/console_window.hpp"
#include "Metadata/entity_metadata.hpp"
#include "Mods/Network/demo.hpp"
#include "Mods/Network/demo_playback.hpp"
#include "Mods/Network/demo_recorder.hpp"
#include "Formats/effects.hpp"
#include "Formats/effect_runtime.hpp"
#include "GameState.hpp"
#include "Entities/gameplay.hpp"
#include "Mods/Input/input.hpp"
#include "Mods/Launcher/Portable/launcher_prefs.hpp"
#include "Mods/GameSettings.hpp"
#include "Entities/match_flow.hpp"
#include "Mods/Launcher/Portable/match_start.hpp"
#include "Metadata/metadata.hpp"
#include "Formats/model_format.hpp"
#include "Assets/model_catalog.hpp"
#include "Formats/model_instance.hpp"
#include "Sound/music.hpp"
#include "Sound/music_runtime.hpp"
#include "Formats/movie.hpp"
#include "Mods/Launcher/native_launcher.hpp"
#include "Mods/Network/net_client.hpp"
#include "Mods/Network/net_damage.hpp"
#include "Mods/Network/net_diagnostics.hpp"
#include "Mods/Network/net_match_end.hpp"
#include "Mods/Network/net_match_sync.hpp"
#include "Mods/Network/net_log.hpp"
#include "Mods/Network/net_hooks.hpp"
#include "Mods/Network/player_entity_net_hud.hpp"
#include "Mods/pause_menu.hpp"
#include "Mods/Render/pro_hud.hpp"
#include "Mods/Network/net_player_bridge.hpp"
#include "Mods/Network/net_scoreboard.hpp"
#include "Mods/Network/net_room_change.hpp"
#include "Mods/Network/net_slot_manager.hpp"
#include "Mods/Network/net_player_setup.hpp"
#include "Entities/room_catalog.hpp"
#include "Mods/render_options.hpp"
#include "Utility/rng.hpp"
#include "Entities/scene.hpp"
#include "SceneSetup.hpp"
#include "Mods/Sound/sfx_mixer.hpp"
#include "Sound/sfx_runtime.hpp"
#include "Entities/static_entities.hpp"
#include "Strings.hpp"
#include "Mods/branding.hpp"
#include "Metadata/player_values.hpp"
#include "Metadata/player_metadata.hpp"
#include "Entities/Players/PlayerHud.hpp"
#include "Entities/Players/PlayerProcess.hpp"
#include "Features.hpp"
#include "Formats/camera_sequence.hpp"
#include "Mods/settings.hpp"
#include "Mods/InputSettings.hpp"
#include "Mods/spectator_mode.hpp"
#include "Mods/Update/update.hpp"
#include "Mods/window_mode.hpp"

#include "Entities/Enemies/enemy_catalog.hpp"
#include "Entities/Players/PlayerAi.hpp"
#include "Entities/Players/PlayerCamera.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Entities/Players/player_profile.hpp"
#include "HUD/hud.hpp"
#include "Mods/MapGen/custom_rooms.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <shellapi.h>

namespace {

std::string g_error;
HDC g_device_context = nullptr;
// Scene.SetTitle: the window is named after the room being played, which is
// how the managed head names it too.  It is a global because the window
// exists long before there is a room to name it after.
HWND g_window = nullptr;

// The name the window should carry: the room's, once there is one.
std::string g_window_title;

// Scene.SetTitle: a room that has an in-game name gives the window its own.
// The room is loaded before the window is created, so this records the name
// as well as applying it -- whichever happens first, the other picks it up.
void name_window_after_room(const fruityprime::scene::RoomCatalogEntry& room) {
    if (room.in_game_name.empty()) {
        return;
    }
    g_window_title = room.in_game_name;
    if (g_window != nullptr) {
        SetWindowTextA(g_window, g_window_title.c_str());
    }
}
std::unique_ptr<fruityprime::assets::Store> g_assets;
std::optional<fruityprime::gameplay::Config> g_gameplay_config;
std::optional<fruityprime::scene::Room> g_room;
std::optional<fruityprime::model::ModelInstance> g_room_instance;
std::optional<fruityprime::gameplay::Session> g_session;
std::optional<fruityprime::match::Flow> g_match_flow;
fruityprime::players::PlayerCamera g_unbound_player_camera;
fruityprime::game::State g_game_state;
std::uint64_t g_model_animation_tick =
    std::numeric_limits<std::uint64_t>::max();
std::unique_ptr<fruityprime::net::NetClient> g_net_client;
fruityprime::demo::Recorder g_demo_recorder;
fruityprime::demo::Playback g_replay_playback;
std::filesystem::path g_demo_path;
std::optional<fruityprime::net::RosterPacket> g_roster;
std::optional<fruityprime::game::StorySave> g_story_save;
std::array<char, fruityprime::game::StorySave::ScanCount>
    g_scan_categories{};
std::array<float, fruityprime::game::StorySave::ScanCount>
    g_scan_times{};

struct ScanTarget {
    const void* identity = nullptr;
    std::uint16_t scan_id = 0;
    int category = 5;
    fruityprime::net::Vec3 position{};
    float distance = 0.0F;
    float scale = 0.0F;
    float screen_x = 0.5F;
    float screen_y = 0.5F;
    float center_metric = std::numeric_limits<float>::max();
    bool dim = false;
};

struct ScanText {
    std::string value1;
    std::string value2;
};

ScanTarget g_scan_target;
std::array<ScanText, fruityprime::game::StorySave::ScanCount>
    g_scan_texts{};
const void* g_previous_scan_target = nullptr;
float g_scan_box_corner_factor = 0.125F;
float g_scan_box_size_factor = 4.0F;
float g_scan_box_x = 0.5F;
float g_scan_box_y = 0.5F;
float g_scan_progress = 0.0F;
bool g_scan_complete = false;
bool g_scan_button_was_held = false;
bool g_scan_dialog_active = false;
bool g_scan_dialog_confirm_was_down = false;
std::uint16_t g_scan_dialog_scan_id = 0;
std::string g_local_name = "FruityPrime";
std::chrono::steady_clock::time_point g_started_at;
std::chrono::steady_clock::time_point g_fps_window_started;
std::uint32_t g_fps_window_frames = 0;
float g_display_fps = 0.0F;
std::uint8_t g_local_slot = 0;
std::uint8_t g_local_hunter = 0;
bool g_is_authority = false;
fruityprime::net::SlotManager g_slot_manager;
fruityprime::net::NetPlayerBridge g_net_player_bridge;
fruityprime::net::DamageBridge g_net_damage;
fruityprime::net::MatchEnd g_net_match_end;
fruityprime::net::MatchSync g_net_match_sync;
fruityprime::net::RoomChange g_net_room_change;
fruityprime::net::NetLog g_net_log;
fruityprime::net::NetDiagnostics g_net_diagnostics;
std::optional<fruityprime::net::MatchStatePacket> g_server_match_state;
std::uint32_t g_net_frame = 1;
std::uint32_t g_demo_frame = 0;
std::uint32_t g_replay_frame = 0;
std::uint32_t g_replay_last_snapshot_frame = 0;
std::uint32_t g_replay_late_snapshot_run = 0;
std::uint8_t g_weapon_select = 0xff;
bool g_show_scoreboard = false;
bool g_paused = false;
fruityprime::mods::pause::Controller g_pause_menu;
fruityprime::mods::spectator::Controller g_spectator;
fruityprime::input::State g_input;
fruityprime::input::Gamepad g_gamepad;
fruityprime::input::GamepadState g_gamepad_state;
fruityprime::settings::InputConfig g_input_config;
bool g_scan_visor_active = false;
bool g_scan_visor_keyboard_was_down = false;
bool g_scan_visor_gamepad_was_down = false;
std::vector<std::uint8_t> g_bot_slots;
int g_bot_level = 1;
float g_aim_yaw = 0.0F;
float g_aim_pitch = 0.0F;
bool g_mouse_look = false;
bool g_no_helmet = false;
bool g_pro_hud = false;
fruityprime::window::State g_window_mode;
RECT g_windowed_rect{};
LONG_PTR g_windowed_style = 0;
LONG_PTR g_windowed_exstyle = 0;
bool g_windowed_state_saved = false;
std::vector<std::string> g_command_arguments;

constexpr std::size_t NoTexture = std::numeric_limits<std::size_t>::max();
constexpr std::size_t NoMaterial = std::numeric_limits<std::size_t>::max();
constexpr std::uint8_t NoHunter = 0xff;

struct TextureImage {
    const fruityprime::model::File* owner = nullptr;
    int texture_id = -1;
    int palette_id = -1;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    GLuint gl_id = 0;
};

struct HudImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    GLuint gl_id = 0;
};

struct HudObjectImage {
    int width = 0;
    int height = 0;
    std::size_t image_count = 0;
    std::size_t palette_count = 0;
    // Images are stored palette-major, then image/frame-major. This mirrors
    // HudInfo.TestObjects, where every 8x8 tile frame is rendered once for
    // each distinct palette bank in the object file.
    std::vector<HudImage> images;

    [[nodiscard]] const HudImage* frame(
        std::size_t image_index, std::size_t palette_index = 0) const noexcept {
        if (images.empty() || image_count == 0 || palette_count == 0) {
            return nullptr;
        }
        image_index = std::min(image_index, image_count - 1);
        palette_index = std::min(palette_index, palette_count - 1);
        const std::size_t index = palette_index * image_count + image_index;
        return index < images.size() ? &images[index] : nullptr;
    }
};

enum class DrawLayer {
    Room,
    Player,
    PlayerAlt,
    Entity,
    Gun
};

struct EntityRenderModel;

struct DrawPrimitive {
    fruityprime::model::GeometryPrimitive geometry;
    std::size_t texture_index = NoTexture;
    DrawLayer layer = DrawLayer::Room;
    float model_scale = 1.0F;
    const fruityprime::model::File* owner = nullptr;
    std::size_t material_id = NoMaterial;
    std::uint8_t hunter = NoHunter;
    const EntityRenderModel* entity_model = nullptr;
    std::int32_t node_id = -1;
};

struct HunterModelSet {
    std::optional<fruityprime::model::File> model;
    std::optional<fruityprime::model::File> alt_model;
    std::optional<fruityprime::model::ModelInstance> instance;
    std::optional<fruityprime::model::ModelInstance> alt_instance;
    // The arm cannon.  Only ever drawn for the player holding it, in first
    // person, which is why it has a layer of its own rather than being part
    // of the biped.
    std::optional<fruityprime::model::File> gun_model;
    std::optional<fruityprime::model::ModelInstance> gun_instance;
};

struct EntityRenderModel {
    std::unique_ptr<fruityprime::model::File> model;
    std::optional<fruityprime::model::ModelInstance> instance;
    const fruityprime::entities::static_entities::Entity* entity = nullptr;
    const fruityprime::scene::EntityInstance* source = nullptr;
    std::string model_name;
    float local_y = 0.0F;
    bool enemy_catalog_model = false;
    bool item_catalog_model = false;
    bool projectile_catalog_model = false;
    bool effect_catalog_model = false;
};

std::vector<DrawPrimitive> g_draw_primitives;
std::vector<TextureImage> g_texture_images;
std::array<HunterModelSet, fruityprime::metadata::HunterCount>
    g_player_models;
std::unique_ptr<fruityprime::entities::static_entities::World> g_static_world;
std::vector<std::unique_ptr<EntityRenderModel>> g_entity_models;
std::array<EntityRenderModel*, fruityprime::metadata::EnemyCount>
    g_enemy_models{};
std::array<EntityRenderModel*, fruityprime::metadata::ItemCount>
    g_item_models{};
std::unordered_map<std::string, EntityRenderModel*> g_effect_models;
// Projectile trails and the two model-backed beam variants are loaded once
// from the same cartridge model catalogue as room entities.  Their geometry
// is filtered out of the static entity pass and placed from live projectile
// state during draw_projectiles().
constexpr std::size_t ProjectileIceShard = 0;
constexpr std::size_t ProjectileEnergyBeam = 1;
constexpr std::size_t ProjectileTrail = 2;
constexpr std::size_t ProjectileElectroTrail = 3;
constexpr std::size_t ProjectileArcWelder = 4;
std::array<EntityRenderModel*, 5> g_projectile_models{};
// BeamEffectEntity uses these three named model resources for its non-splat
// beam variants. They are kept out of the static entity pass and are drawn
// only when a live beam-effect event selects the matching model.
constexpr std::size_t BeamEffectIceWave = 0;
constexpr std::size_t BeamEffectSniperBeam = 1;
constexpr std::size_t BeamEffectBossLaserBurn = 2;
std::array<EntityRenderModel*, 3> g_beam_effect_models{};
std::array<std::optional<fruityprime::effects::File>,
           fruityprime::metadata::EffectCount> g_effect_resources{};
std::unique_ptr<fruityprime::effects::Runtime> g_effect_runtime;
std::unordered_set<std::uint32_t> g_effect_runtime_seen;
std::size_t g_loaded_effect_resources = 0;
std::optional<fruityprime::sound::Catalog> g_sound_catalog;
std::unique_ptr<fruityprime::mods::sound::SfxMixer> g_sfx_mixer;
std::unique_ptr<fruityprime::sound::SfxRuntime> g_sfx_runtime;
std::unique_ptr<fruityprime::sound::MusicController> g_music_controller;
std::unique_ptr<fruityprime::sound::MusicRuntime> g_music_runtime;
std::optional<fruityprime::movie::Player> g_movie_player;
GLuint g_movie_texture = 0;
int g_movie_texture_width = 0;
int g_movie_texture_height = 0;
std::size_t g_movie_uploaded_frame =
    std::numeric_limits<std::size_t>::max();
fruityprime::mods::sound::SfxMixer::Handle g_movie_audio_buffer = 0;
fruityprime::mods::sound::SfxMixer::Handle g_movie_audio_source = 0;
std::array<fruityprime::sound::SoundSource,
           fruityprime::net::NetConfig::SlotCapacity> g_player_sound_sources;
// SoundSource pointers are retained by SfxRuntime until a voice ends. A
// fixed pool keeps those addresses stable while still allowing many impact
// cues in one simulation tick without allocating from the audio path.
constexpr std::size_t AudioEventSourceCapacity = 256;
std::array<fruityprime::sound::SoundSource, AudioEventSourceCapacity>
    g_audio_event_sources;
std::size_t g_audio_event_source_cursor = 0;
std::optional<HudImage> g_hud_reticle;
std::optional<HudImage> g_hud_ice;
std::optional<HudImage> g_hud_helmet;
std::optional<HudImage> g_hud_helmet_drop;
std::optional<HudImage> g_hud_visor;
std::optional<HudImage> g_hud_scan_visor;
std::optional<HudObjectImage> g_hud_health_bar;
std::optional<HudObjectImage> g_hud_health_bar_sub;
std::optional<HudObjectImage> g_hud_ammo_bar;
std::optional<HudObjectImage> g_hud_weapon_icon;
// The alphabet the HUD's own numbers and words are drawn with, and the same
// glyphs in flat white.  HudObjectInstance.SetData takes either a palette or
// an explicit colour; the second form replaces every lit pixel, which is what
// the rules screen and the frame counter use, so it needs its own atlas
// rather than a tint over the palette's green.
std::optional<HudObjectImage> g_hud_font;
std::optional<HudObjectImage> g_hud_font_mask;
// The scoreboard's own art: a portrait per hunter, and the two halves of the
// star bar behind a nickname.
std::array<std::optional<HudObjectImage>,
           fruityprime::metadata::HunterCount> g_hud_hunters;
std::optional<HudObjectImage> g_hud_stars;
// Scene.ElapsedTime, for the HUD's own animations: the scoreboard's pulse on
// the local player's row, and the rules screen's typing.
float g_hud_elapsed_seconds = 0.0F;
// PlayerHud._healthbarYOffset, which eases rather than snapping and so
// has to survive between frames.
float g_healthbar_y_offset = 0.0F;
// Renderer.SetRoomValues: a room lights everything in it with two directional
// lights of its own.  Until these were read the scene had one invented light
// and a scene-wide ambient, which is why every surface facing away from that
// one direction came out nearly black -- most visibly the inside of the arm
// cannon's muzzle, which read as a shapeless dark collar.
struct SceneLight {
    std::array<float, 3> vector{0.0F, 0.0F, 0.0F};
    std::array<float, 3> color{0.0F, 0.0F, 0.0F};
};
SceneLight g_light1;
SceneLight g_light2;

void set_room_lights(int room_id) {
    g_light1 = SceneLight{};
    g_light2 = SceneLight{};
    const auto* meta = fruityprime::metadata::room_metadata_by_id(room_id);
    if (meta == nullptr) {
        return;
    }
    const auto to_color = [](const fruityprime::formats::ColorRgb& value) {
        return std::array<float, 3>{
            static_cast<float>(value.red) / 31.0F,
            static_cast<float>(value.green) / 31.0F,
            static_cast<float>(value.blue) / 31.0F};
    };
    g_light1.vector = {meta->light1_vector.x, meta->light1_vector.y,
                       meta->light1_vector.z};
    g_light1.color = to_color(meta->light1_color);
    g_light2.vector = {meta->light2_vector.x, meta->light2_vector.y,
                       meta->light2_vector.z};
    g_light2.color = to_color(meta->light2_color);
}

// The managed shader's light_calc, expressed as fixed-function state.  It
// sums, per light, ambient*colour + diffuse*colour*max(0, -L.n) + a specular
// term, with no scene-wide ambient at all -- so each light's GL ambient is
// its own colour and the light model's ambient is zero.  GL's directional
// position points *towards* the light, which is the negation of the vector
// the cartridge stores.
void apply_scene_lights() {
    if (!fruityprime::mods::render::options().lighting()) {
        return;
    }
    constexpr GLfloat no_ambient[] = {0.0F, 0.0F, 0.0F, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, no_ambient);
    const auto apply = [](GLenum light, const SceneLight& value) {
        const GLfloat color[] = {value.color[0], value.color[1],
                                 value.color[2], 1.0F};
        const GLfloat position[] = {-value.vector[0], -value.vector[1],
                                    -value.vector[2], 0.0F};
        glLightfv(light, GL_AMBIENT, color);
        glLightfv(light, GL_DIFFUSE, color);
        glLightfv(light, GL_SPECULAR, color);
        glLightfv(light, GL_POSITION, position);
        glEnable(light);
    };
    apply(GL_LIGHT0, g_light1);
    apply(GL_LIGHT1, g_light2);
}
// The arena fly-through a multiplayer match opens on, and how long it has
// been running.  The player does not exist yet as far as the view is
// concerned: the camera follows the authored path and the rules are typed out
// over it until somebody presses fire.
std::optional<fruityprime::camera::File> g_intro_file;
std::optional<fruityprime::camera::Playback> g_intro;
float g_intro_seconds = 0.0F;
// Strings.GetHudMessage reads all three: an id is looked up in the common
// table first and then in the one for the game being played.
std::vector<fruityprime::strings::TableEntry> g_hud_msgs_common;
std::vector<fruityprime::strings::TableEntry> g_hud_msgs_single;
std::vector<fruityprime::strings::TableEntry> g_hud_msgs_multi;
GLuint g_scene_scale_texture = 0;
int g_scene_scale_width = 0;
int g_scene_scale_height = 0;

void start_movie_audio() {
    if (!g_movie_player.has_value()) {
        return;
    }
    const auto& header = g_movie_player->decoder().header();
    if (header.audio_sample_rate <= 0) {
        return;
    }
    const auto samples = g_movie_player->decoder().audio_samples();
    if (samples.empty()) {
        return;
    }
    std::vector<std::uint8_t> bytes(samples.size() * 2U);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto value = static_cast<std::uint16_t>(samples[index]);
        bytes[index * 2U] = static_cast<std::uint8_t>(value);
        bytes[index * 2U + 1U] = static_cast<std::uint8_t>(value >> 8);
    }

    auto mixer = std::make_unique<fruityprime::mods::sound::SfxMixer>(
        32728);
    fruityprime::mods::sound::SfxMixer::Handle buffer = 0;
    fruityprime::mods::sound::SfxMixer::Handle source = 0;
    try {
        static_cast<void>(mixer->open());
        buffer = mixer->new_buffer();
        source = mixer->new_source();
        if (!mixer->fill_buffer(buffer,
                                fruityprime::sound::AudioBufferFormat::Mono16,
                                bytes, static_cast<std::uint32_t>(
                                    header.audio_sample_rate))) {
            throw std::runtime_error("movie audio buffer upload failed");
        }
        mixer->set_relative(source, true);
        mixer->set_buffer(source, buffer);
        mixer->set_gain(source, 1.0F);
        mixer->play(source);
        g_sfx_mixer = std::move(mixer);
        g_movie_audio_buffer = buffer;
        g_movie_audio_source = source;
    } catch (const std::exception& error) {
        if (source != 0) {
            mixer->delete_source(source);
        }
        if (buffer != 0) {
            mixer->delete_buffer(buffer);
        }
        OutputDebugStringA((std::string("Fruity Prime movie audio disabled: ")
                            + error.what() + "\n").c_str());
    }
}

void pause_movie_audio(bool paused) noexcept {
    if (g_sfx_mixer == nullptr || g_movie_audio_source == 0) {
        return;
    }
    if (paused) {
        g_sfx_mixer->pause(g_movie_audio_source);
    } else if (g_movie_player.has_value()
               && g_movie_player->state()
                   != fruityprime::movie::PlaybackState::Finished) {
        g_sfx_mixer->play(g_movie_audio_source);
    }
}

void set_projection(int width, int height, float fov_degrees) {
    const int safe_height = std::max(height, 1);
    const float aspect = static_cast<float>(std::max(width, 1))
        / static_cast<float>(safe_height);
    constexpr float near_plane = 0.1F;
    constexpr float far_plane = 200.0F;
    const float safe_fov = std::clamp(
        std::isfinite(fov_degrees) ? fov_degrees : 78.0F,
        1.0F, 179.0F);
    const float half_height = std::tan(
        safe_fov * 0.5F * 3.14159265358979323846F / 180.0F)
        * near_plane;
    const float half_width = half_height * aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-half_width, half_width, -half_height, half_height,
              near_plane, far_plane);
    glMatrixMode(GL_MODELVIEW);
}

void apply_camera_view(const fruityprime::players::CameraInfo& camera) {
    const auto subtract = [](fruityprime::net::Vec3 a,
                             fruityprime::net::Vec3 b) {
        return fruityprime::net::Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto dot = [](fruityprime::net::Vec3 a,
                        fruityprime::net::Vec3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto cross = [](fruityprime::net::Vec3 a,
                          fruityprime::net::Vec3 b) {
        return fruityprime::net::Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
    };
    const auto normalize = [](fruityprime::net::Vec3 value,
                              fruityprime::net::Vec3 fallback) {
        const float length_squared = value.x * value.x
            + value.y * value.y + value.z * value.z;
        if (!std::isfinite(length_squared) || length_squared <= 0.000001F) {
            return fallback;
        }
        const float inverse = 1.0F / std::sqrt(length_squared);
        return fruityprime::net::Vec3{
            value.x * inverse, value.y * inverse, value.z * inverse};
    };
    const auto forward = normalize(
        subtract(camera.target, camera.position), {0.0F, 0.0F, 1.0F});
    auto side = normalize(cross(forward, camera.up), {1.0F, 0.0F, 0.0F});
    const auto up = normalize(cross(side, forward), {0.0F, 1.0F, 0.0F});
    const auto& position = camera.position;
    // The array is column-major because it is consumed by glLoadMatrixf.
    // This is the same LookAt basis used by OpenTK's Matrix4.LookAt in the
    // managed Renderer after PlayerCamera.UpdateCameraFirst.
    const GLfloat view[16]{
        side.x, up.x, -forward.x, 0.0F,
        side.y, up.y, -forward.y, 0.0F,
        side.z, up.z, -forward.z, 0.0F,
        -dot(side, position), -dot(up, position), dot(forward, position),
        1.0F};
    glLoadMatrixf(view);
}

std::uint8_t expand_color_channel(std::uint16_t color, int shift) {
    return static_cast<std::uint8_t>(std::lround(
        static_cast<float>((color >> shift) & 0x1f) / 31.0F * 255.0F));
}

std::size_t cache_texture_ids(const fruityprime::model::File& model,
                              int texture_id, int palette_id) {
    if (texture_id < 0
        || static_cast<std::size_t>(texture_id) >= model.textures().size()) {
        return NoTexture;
    }
    for (std::size_t i = 0; i < g_texture_images.size(); ++i) {
        const auto& image = g_texture_images[i];
        if (image.owner == &model
            && image.texture_id == texture_id
            && image.palette_id == palette_id) {
            return i;
        }
    }

    const auto& texture = model.textures()[static_cast<std::size_t>(texture_id)];
    const auto pixels = model.decode_texture(static_cast<std::size_t>(texture_id));
    const bool direct = texture.format == 5;
    std::vector<std::uint16_t> palette;
    if (!direct && palette_id >= 0
        && static_cast<std::size_t>(palette_id) < model.palettes().size()) {
        palette = model.decode_palette(static_cast<std::size_t>(palette_id));
    }
    const std::size_t expected = static_cast<std::size_t>(texture.width)
        * static_cast<std::size_t>(texture.height);
    if (pixels.size() != expected) {
        throw std::runtime_error("decoded texture pixel count is invalid");
    }
    TextureImage image;
    image.owner = &model;
    image.texture_id = texture_id;
    image.palette_id = palette_id;
    image.width = texture.width;
    image.height = texture.height;
    image.rgba.resize(expected * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        std::uint16_t color = 0x7fff;
        if (direct) {
            color = static_cast<std::uint16_t>(pixels[i].data);
        } else if (pixels[i].data < palette.size()) {
            color = palette[pixels[i].data];
        }
        image.rgba[i * 4 + 0] = expand_color_channel(color, 0);
        image.rgba[i * 4 + 1] = expand_color_channel(color, 5);
        image.rgba[i * 4 + 2] = expand_color_channel(color, 10);
        image.rgba[i * 4 + 3] = pixels[i].alpha;
    }
    g_texture_images.push_back(std::move(image));
    return g_texture_images.size() - 1;
}

std::size_t cache_texture(const fruityprime::model::File& model,
                          const fruityprime::model::Material& material) {
    return cache_texture_ids(model, material.texture_id, material.palette_id);
}

[[nodiscard]] std::int32_t node_id_for_mesh(
    const fruityprime::model::File& model, std::size_t mesh_index) noexcept {
    for (std::size_t node_index = 0; node_index < model.nodes().size();
         ++node_index) {
        const auto& node = model.nodes()[node_index];
        if (node.mesh_count == 0) {
            continue;
        }
        const std::size_t start = node.mesh_id / 2;
        if (mesh_index >= start
            && mesh_index - start < node.mesh_count) {
            return static_cast<std::int32_t>(node_index);
        }
    }
    return -1;
}

// RoomEntity.Setup applies the node layer mask for the match the room is
// being loaded into.  Without it the room draws its multiplayer LOD0 and LOD1
// geometry, and its story-only variants, on top of each other.  The caller
// passes the match description rather than reading the process game state,
// because the first room is loaded before that state is synced.
void apply_room_node_layers(fruityprime::scene::Room& room,
                            bool single_player, int player_count,
                            bool capture) {
    // A story room's mask comes from its own row in the room table.
    int room_layer = 0;
    if (single_player) {
        const auto* meta = fruityprime::metadata::find_room_metadata(
            room.definition().name);
        if (meta == nullptr) {
            // Without the row there is no mask to apply, and a zero mask
            // would disable every layered node rather than select one set.
            return;
        }
        room_layer = meta->node_layer;
    }
    room.mutable_model().filter_nodes(fruityprime::model::node_layer_mask(
        single_player, room_layer, player_count, capture));
}

void append_model_geometry(const fruityprime::model::File& model,
                           DrawLayer layer = DrawLayer::Room,
                           std::uint8_t hunter = NoHunter,
                           const EntityRenderModel* entity_model = nullptr) {
    // A room's display lists are decoded with the managed renderer's room
    // rule: matrix ID pinned to 0, because room node transforms are off.
    const bool is_room = layer == DrawLayer::Room;
    if (model.meshes().empty()) {
        for (std::size_t i = 0; i < model.instructions().size(); ++i) {
            const auto batches = model.decode_geometry(i, 0, 0, false, is_room);
            for (auto batch : batches) {
                g_draw_primitives.push_back(DrawPrimitive{
                    std::move(batch), NoTexture, layer, model.world_scale(),
                    &model, NoMaterial, hunter, entity_model
                });
            }
        }
        return;
    }
    for (std::size_t mesh_index = 0;
         mesh_index < model.meshes().size(); ++mesh_index) {
        const std::int32_t owning_node = node_id_for_mesh(model, mesh_index);
        // Model.FilterNodes disabled this node for the current game mode and
        // detail level; the managed renderer never reaches its meshes.
        if (owning_node >= 0
            && model.nodes()[static_cast<std::size_t>(owning_node)].enabled
                == 0) {
            continue;
        }
        const auto& mesh = model.meshes()[mesh_index];
        int texture_width = 0;
        int texture_height = 0;
        bool texgen = false;
        if (mesh.material_id < model.materials().size()) {
            const auto& material = model.materials()[mesh.material_id];
            texgen = material.texcoord_transform_mode == 2;
            if (material.texture_id >= 0
                && static_cast<std::size_t>(material.texture_id)
                    < model.textures().size()) {
                const auto& texture = model.textures()[material.texture_id];
                texture_width = texture.width;
                texture_height = texture.height;
            }
        }
        if (mesh.display_list_id >= model.instructions().size()) {
            throw std::runtime_error("model mesh references an invalid display list");
        }
        const auto batches = model.decode_geometry(
            mesh.display_list_id, texture_width, texture_height, texgen,
            is_room);
        std::size_t texture_index = NoTexture;
        if (mesh.material_id < model.materials().size()) {
            texture_index = cache_texture(model, model.materials()[mesh.material_id]);
        }
        for (auto batch : batches) {
            g_draw_primitives.push_back(
                DrawPrimitive{
                    std::move(batch), texture_index, layer, model.world_scale(),
                    &model, mesh.material_id, hunter, entity_model,
                    owning_node
                });
        }
    }
}

[[nodiscard]] std::string hunter_archive_name(
    const fruityprime::metadata::HunterInfo& info, bool alt) {
    // Guardian's alt form is explicitly SamusAlt_lod0 in the managed
    // HunterModels table and therefore comes from Samus.arc, not
    // Guardian.arc.
    if (alt && info.id == fruityprime::metadata::Hunter::Guardian) {
        return "Samus";
    }
    return std::string(info.archive);
}

[[nodiscard]] std::string hunter_model_name(
    const fruityprime::metadata::HunterInfo& info, bool alt) {
    return std::string(alt ? info.alt_model_entry : info.base_model_entry)
        .substr(0, std::string(alt ? info.alt_model_entry
                                   : info.base_model_entry).size()
                       - std::string_view("_Model.bin").size());
}

// Metadata.HunterModels[hunter][3], without the resource suffix the loader
// adds back.
void set_gun_animation(fruityprime::model::ModelInstance& instance,
                       std::size_t hunter, std::uint8_t weapon,
                       int animation);

[[nodiscard]] std::string gun_model_name(
    const fruityprime::metadata::HunterInfo& info) {
    constexpr std::string_view Suffix = "_Model.bin";
    std::string entry(info.gun_model_entry);
    if (entry.size() > Suffix.size()
        && entry.compare(entry.size() - Suffix.size(), Suffix.size(),
                         Suffix) == 0) {
        entry.resize(entry.size() - Suffix.size());
    }
    return entry;
}

// The arm cannon does not sit where try_load_named_model looks.  It lives in
// the hunter's own local archive -- the same folder the HUD art comes from --
// while its textures are a recolor beside the ordinary models, so it needs
// both paths given to it rather than guessed.
[[nodiscard]] fruityprime::model::File load_gun_model(
    const fruityprime::assets::Store& assets,
    const fruityprime::metadata::HunterInfo& info) {
    const std::string name = gun_model_name(info);
    const std::string archive = "_archives/local" + std::string(info.archive);
    auto model_bytes = assets.bytes(archive + "/" + name + "_Model.bin");
    // img_01 is the first recolor, which is what a player who has not joined
    // a team is drawn in.
    auto model = fruityprime::model::File::from_recolor_resources(
        std::move(model_bytes),
        assets.bytes("models/" + name + "_img_01_Model.bin"),
        assets.bytes("models/" + name + "_img_01_Tex.bin"));
    try {
        model.load_animations(assets.bytes(archive + "/" + name + "_Anim.bin"),
                              name);
    } catch (const std::exception&) {
        // The cannon is drawn whether or not its animations are there.
    }
    return model;
}

[[nodiscard]] fruityprime::model::File load_hunter_model(
    const fruityprime::assets::Store& assets,
    const fruityprime::metadata::HunterInfo& info, bool alt) {
    const std::string archive_name = hunter_archive_name(info, alt);
    const std::string archive_path = "archives/" + archive_name + ".arc";
    const auto resource = assets.archive(archive_path);
    const std::string entry_name = std::string(
        alt ? info.alt_model_entry : info.base_model_entry);
    const auto entry = std::find_if(
        resource.entries().begin(), resource.entries().end(),
        [&entry_name](const auto& candidate) {
            return candidate.filename == entry_name;
        });
    if (entry == resource.entries().end()) {
        throw std::runtime_error("hunter archive " + archive_path
                                 + " does not contain " + entry_name);
    }

    const std::string recolor_prefix = std::string(
        alt ? info.alt_recolor_prefix : info.base_recolor_prefix);
    const std::string recolor_model = "models/" + recolor_prefix
        + "_pal_01_Model.bin";
    const std::string recolor_texture = "models/" + recolor_prefix
        + "_pal_01_Tex.bin";
    return fruityprime::model::File::from_recolor_resources(
        resource.file(static_cast<std::size_t>(
            entry - resource.entries().begin())),
        assets.bytes(recolor_model), assets.bytes(recolor_texture));
}

void load_hunter_animations(
    const fruityprime::assets::Store& assets,
    fruityprime::model::File& model,
    const fruityprime::metadata::HunterInfo& info, bool alt) {
    // SamusAlt_lod0 is a static morph-form model. Guardian reuses that exact
    // model, while the other hunters have a dedicated Alt animation stream.
    const bool has_animation = !alt
        || (info.id != fruityprime::metadata::Hunter::Samus
            && info.id != fruityprime::metadata::Hunter::Guardian);
    if (!has_animation) {
        return;
    }

    const std::string archive_name = std::string(info.archive);
    const std::string animation_name = archive_name
        + (alt ? "Alt" : "") + "_Anim.bin";
    const std::string animation_path = "_archives/" + archive_name + "/"
        + animation_name;
    model.load_animations(assets.bytes(animation_path),
                          hunter_model_name(info, alt));

    const std::string shared_path =
        (info.id == fruityprime::metadata::Hunter::Noxus
         || info.id == fruityprime::metadata::Hunter::Trace)
        ? "models/NoxSharedAnim_Anim.bin"
        : "models/SamusSharedAnim_Anim.bin";
    model.append_animations(assets.bytes(shared_path),
                            hunter_model_name(info, alt));
}

void load_hunter_models(const fruityprime::assets::Store& assets) {
    for (std::size_t index = 0;
         index < fruityprime::metadata::HunterCount; ++index) {
        auto& destination = g_player_models[index];
        const auto& info = fruityprime::metadata::hunter_info(
            static_cast<std::uint8_t>(index));
        try {
            destination.model.emplace(load_hunter_model(assets, info, false));
            load_hunter_animations(assets, *destination.model, info, false);
            destination.instance.emplace(*destination.model);
            append_model_geometry(
                *destination.model, DrawLayer::Player,
                static_cast<std::uint8_t>(index));

            destination.alt_model.emplace(load_hunter_model(assets, info, true));
            load_hunter_animations(assets, *destination.alt_model, info, true);
            destination.alt_instance.emplace(*destination.alt_model);
            append_model_geometry(
                *destination.alt_model, DrawLayer::PlayerAlt,
                static_cast<std::uint8_t>(index));

            // A hunter with no arm cannon in the dump is not a hunter that
            // cannot be played: the view simply has nothing in the corner.
            try {
                destination.gun_model.emplace(load_gun_model(assets, info));
                destination.gun_instance.emplace(*destination.gun_model);
                // GunAnimation.Idle with the Power Beam: the pose the cannon
                // is in before anybody switches weapon.  Without it the first
                // update poses it from animation 0, which for Samus is the
                // barrels open.
                set_gun_animation(*destination.gun_instance, index, 0, 3);
                append_model_geometry(
                    *destination.gun_model, DrawLayer::Gun,
                    static_cast<std::uint8_t>(index));
            } catch (const std::exception& error) {
                destination.gun_model.reset();
                destination.gun_instance.reset();
                OutputDebugStringA((std::string("Fruity Prime gun model for ")
                    + std::string(info.name) + " unavailable: "
                    + error.what() + "\n").c_str());
            }
        } catch (const std::exception& error) {
            throw std::runtime_error("failed to load "
                                     + std::string(info.name)
                                     + " hunter models: " + error.what());
        }
    }
}

const fruityprime::entities::static_entities::Entity*
find_static_entity(const fruityprime::scene::EntityInstance& source) {
    if (g_static_world == nullptr || source.entity_id < 0) {
        return nullptr;
    }
    auto* entity = g_static_world->find(source.entity_id);
    return entity != nullptr && entity->kind() == source.kind ? entity : nullptr;
}

EntityRenderModel* add_entity_render_model(
    const fruityprime::assets::Store& assets,
    const fruityprime::scene::EntityInstance& source,
    std::string_view name,
    float local_y = 0.0F) {
    if (name.empty()) {
        return nullptr;
    }
    auto loaded = fruityprime::assets::try_load_named_model(assets, name);
    if (!loaded.has_value()) {
        return nullptr;
    }
    auto render_model = std::make_unique<EntityRenderModel>();
    render_model->model = std::make_unique<fruityprime::model::File>(
        std::move(loaded->model));
    render_model->source = &source;
    render_model->entity = find_static_entity(source);
    render_model->model_name = name;
    render_model->local_y = local_y;
    render_model->instance.emplace(*render_model->model);
    if (render_model->model->animations().any()) {
        render_model->instance->set_animation(0);
    }
    try {
        append_model_geometry(*render_model->model, DrawLayer::Entity,
                              NoHunter, render_model.get());
    } catch (const std::exception& error) {
        throw std::runtime_error("entity model " + std::string(name)
                                 + " could not decode: " + error.what());
    }
    g_entity_models.push_back(std::move(render_model));
    return g_entity_models.back().get();
}

EntityRenderModel* add_catalog_render_model(
    const fruityprime::assets::Store& assets, std::string_view name,
    bool item_catalog_model, bool projectile_catalog_model,
    bool effect_catalog_model = false) {
    if (name.empty()) {
        return nullptr;
    }
    auto loaded = fruityprime::assets::try_load_named_model(assets, name);
    if (!loaded.has_value()) {
        return nullptr;
    }
    auto render_model = std::make_unique<EntityRenderModel>();
    render_model->model = std::make_unique<fruityprime::model::File>(
        std::move(loaded->model));
    render_model->model_name = name;
    render_model->item_catalog_model = item_catalog_model;
    render_model->projectile_catalog_model = projectile_catalog_model;
    render_model->effect_catalog_model = effect_catalog_model;
    render_model->instance.emplace(*render_model->model);
    if (render_model->model->animations().any()) {
        render_model->instance->set_animation(0);
    }
    try {
        append_model_geometry(*render_model->model, DrawLayer::Entity,
                              NoHunter, render_model.get());
    } catch (const std::exception&) {
        // Catalog models are optional visual resources. Keep the gameplay
        // entity and let the relevant fallback renderer handle a ROM or
        // extracted tree that does not carry the matching model table.
        return nullptr;
    }
    g_entity_models.push_back(std::move(render_model));
    return g_entity_models.back().get();
}

EntityRenderModel* add_item_render_model(
    const fruityprime::assets::Store& assets, std::string_view name) {
    return add_catalog_render_model(assets, name, true, false);
}

EntityRenderModel* add_projectile_render_model(
    const fruityprime::assets::Store& assets, std::string_view name) {
    return add_catalog_render_model(assets, name, false, true);
}

EntityRenderModel* add_effect_render_model(
    const fruityprime::assets::Store& assets, std::string_view name) {
    return add_catalog_render_model(assets, name, false, false, true);
}

void load_item_models(const fruityprime::assets::Store& assets) {
    g_item_models.fill(nullptr);
    std::array<EntityRenderModel*, fruityprime::metadata::ItemCount>
        by_name{};
    std::array<std::string_view, fruityprime::metadata::ItemCount> names{};
    std::size_t name_count = 0;
    for (std::size_t index = 0;
         index < fruityprime::metadata::ItemCount; ++index) {
        const auto* info = fruityprime::metadata::item_info(
            static_cast<std::int32_t>(index));
        if (info == nullptr || info->asset_name.empty()) {
            continue;
        }
        const auto known = std::find(names.begin(), names.begin() + name_count,
                                     info->asset_name);
        const std::size_t known_index = known == names.begin() + name_count
            ? name_count : static_cast<std::size_t>(known - names.begin());
        if (known == names.begin() + name_count) {
            names[name_count] = info->asset_name;
            by_name[name_count] = add_item_render_model(
                assets, info->asset_name);
            ++name_count;
        }
        g_item_models[index] = by_name[known_index];
    }
}

void load_projectile_models(const fruityprime::assets::Store& assets) {
    g_projectile_models.fill(nullptr);
    constexpr std::array<std::string_view, 5> names{
        "iceShard", "energyBeam", "trail", "electroTrail", "arcWelder"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        g_projectile_models[index] = add_projectile_render_model(
            assets, names[index]);
    }
}

void load_beam_effect_models(const fruityprime::assets::Store& assets) {
    g_beam_effect_models.fill(nullptr);
    constexpr std::array<std::string_view, 3> names{
        "iceWave", "sniperBeam", "cylBossLaserBurn"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        g_beam_effect_models[index] = add_effect_render_model(
            assets, names[index]);
    }
}

void load_effect_resources(const fruityprime::assets::Store& assets) {
    for (auto& resource : g_effect_resources) {
        resource.reset();
    }
    g_loaded_effect_resources = 0;

    // Load the complete metadata table up front.  The managed renderer loads
    // the same resources through its startup/entity closure; native events
    // can therefore resolve an item, enemy, bomb, or child effect without
    // falling back merely because its ID was not part of the weapon subset.
    for (std::size_t index = 1;
         index < fruityprime::metadata::EffectCount; ++index) {
        const auto id = static_cast<std::uint16_t>(index);
        const auto* info = fruityprime::metadata::effect_info(id);
        if (info == nullptr || info->name.empty()) {
            continue;
        }
        const std::string path = info->archive.empty()
            ? "effects/" + std::string(info->name) + "_PS.bin"
            : "_archives/" + std::string(info->archive) + "/"
                + std::string(info->name) + "_PS.bin";
        try {
            g_effect_resources[id].emplace(
                fruityprime::effects::File::parse(
                    assets.bytes(path), static_cast<std::int32_t>(id),
                    std::string(info->name)));
            ++g_loaded_effect_resources;
        } catch (const std::exception&) {
            // A few managed metadata rows deliberately name optional or
            // region-specific resources. Keep the runtime usable with an
            // extracted tree that does not carry one of those files.
        }
    }
}

void load_effect_models(const fruityprime::assets::Store& assets) {
    g_effect_models.clear();

    const auto register_model = [&](std::string_view name) {
        if (name.empty()) {
            return;
        }
        const std::string key(name);
        if (g_effect_models.find(key) != g_effect_models.end()) {
            return;
        }
        // BeamEffectEntity has already loaded these shared resources. Reuse
        // those instances so their geometry and texture caches have one
        // owner, just as Read caches ModelInstance by model name.
        for (auto* beam_model : g_beam_effect_models) {
            if (beam_model != nullptr && beam_model->model_name == name) {
                g_effect_models.emplace(key, beam_model);
                return;
            }
        }
        if (auto* render_model = add_effect_render_model(assets, name);
            render_model != nullptr) {
            g_effect_models.emplace(key, render_model);
        }
    };

    for (const auto& resource : g_effect_resources) {
        if (!resource.has_value()) {
            continue;
        }
        for (const auto& element : resource->elements()) {
            register_model(element.model_name);
        }
    }
    // Match Metadata.PreloadResources and Read.GetSingleParticle.  The
    // particle model/node path is useful even when no live effect currently
    // references one of these named particles.
    for (const auto name : {
             std::string_view{"deathParticle"},
             std::string_view{"particles"},
             std::string_view{"particles2"},
             std::string_view{"TearParticle"},
             std::string_view{"icons"},
             std::string_view{"iceWave"},
             std::string_view{"sniperBeam"},
             std::string_view{"cylBossLaserBurn"}}) {
        register_model(name);
    }
}

void load_enemy_models(const fruityprime::assets::Store& assets) {
    g_enemy_models.fill(nullptr);
    if (!g_room.has_value()) {
        return;
    }

    for (const auto& source : g_room->entities()) {
        if (source.kind != fruityprime::scene::EntityKind::EnemySpawn) {
            continue;
        }
        const auto* data = std::get_if<fruityprime::scene::EnemySpawnData>(
            &source.typed_data);
        if (data == nullptr || data->enemy_type >= g_enemy_models.size()) {
            continue;
        }
        if (g_enemy_models[data->enemy_type] != nullptr) {
            continue;
        }
        std::string_view model_name = fruityprime::enemy::model_name(
            data->enemy_type);
        if (model_name.empty()
            && data->enemy_type == static_cast<std::uint8_t>(
                fruityprime::formats::EnemyType::CarnivorousPlant)) {
            const auto plant = fruityprime::enemy::
                decode_carnivorous_plant_profile(
                    data->enemy_type, data->fields,
                    {source.position.x.to_float(), source.position.y.to_float(),
                     source.position.z.to_float()});
            model_name = plant.model_name;
        }
        if (model_name.empty()) {
            continue;
        }

        EntityRenderModel* render_model = nullptr;
        const auto existing = std::find_if(
            g_entity_models.begin(), g_entity_models.end(),
            [model_name](const auto& candidate) {
                return candidate->enemy_catalog_model
                    && candidate->model_name == model_name;
            });
        if (existing != g_entity_models.end()) {
            render_model = existing->get();
        } else {
            // Reuse the regular model loader so shared texture/recolor
            // resources and optional animation streams follow the same path
            // as static room entities. This temporary source is cleared below:
            // enemy instances get their live position from gameplay::Session.
            render_model = add_entity_render_model(
                assets, source, model_name);
            if (render_model != nullptr) {
                render_model->enemy_catalog_model = true;
                render_model->entity = nullptr;
                render_model->source = nullptr;
            }
        }
        g_enemy_models[data->enemy_type] = render_model;
    }
}

void load_scan_categories(const fruityprime::assets::Store& assets) {
    g_scan_categories.fill('\0');
    g_scan_times.fill(2.0F);
    for (auto& text : g_scan_texts) {
        text.value1.clear();
        text.value2.clear();
    }
    // Most regions use ScanLog.bin. AMHK0 carries the same records in the
    // sorted variant; accepting both keeps category lookup independent of
    // which regional ROM the player selected.
    for (const auto path : {std::string_view{"stringTables/ScanLog.bin"},
                            std::string_view{"stringTables/ScanLogSorted.bin"}}) {
        try {
            const auto entries = fruityprime::strings::read_table(
                assets.bytes(path), true);
            std::size_t loaded = 0;
            for (const auto& entry : entries) {
                if (entry.id.size() != 4 || entry.id.front() != 'L') {
                    continue;
                }
                std::uint32_t id = 0;
                const auto result = std::from_chars(
                    entry.id.data() + 1, entry.id.data() + entry.id.size(), id);
                if (result.ec == std::errc{}
                    && result.ptr == entry.id.data() + entry.id.size()
                    && id < g_scan_categories.size()) {
                    g_scan_categories[id] = entry.category;
                    g_scan_times[id] = fruityprime::strings::scan_time_seconds(
                        entry.speed);
                    g_scan_texts[id].value1 = entry.value1;
                    g_scan_texts[id].value2 = entry.value2;
                    if (g_story_save.has_value()) {
                        g_story_save->set_scan_category(
                            static_cast<std::int32_t>(id), entry.category);
                    }
                    ++loaded;
                }
            }
            if (loaded != 0) {
                return;
            }
        } catch (const std::exception&) {
            // The alternate table is optional for extracted trees and older
            // regional layouts. Entity fallback categories remain usable.
        }
    }
}

void load_entity_models(const fruityprime::assets::Store& assets) {
    g_entity_models.clear();
    g_effect_models.clear();
    g_enemy_models.fill(nullptr);
    g_item_models.fill(nullptr);
    g_projectile_models.fill(nullptr);
    g_beam_effect_models.fill(nullptr);
    g_effect_runtime.reset();
    g_effect_runtime_seen.clear();
    g_static_world.reset();
    try {
        fruityprime::players::PlayerEntity::WeaponNameTable(
            fruityprime::strings::read_table(
                assets.bytes("stringTables/WeaponNames.bin")));
    } catch (const std::exception&) {
        fruityprime::players::PlayerEntity::WeaponNameTable({});
    }
    fruityprime::players::PlayerEntity::LoadWeaponNames();
    load_scan_categories(assets);
    if (!g_room.has_value()) {
        return;
    }
    g_static_world = std::make_unique<
        fruityprime::entities::static_entities::World>(g_room->entities());
    for (const auto& source : g_room->entities()) {
        switch (source.kind) {
        case fruityprime::scene::EntityKind::Object: {
            const auto* data = std::get_if<fruityprime::scene::ObjectData>(
                &source.typed_data);
            const auto* info = data == nullptr || data->model_id < 0
                ? nullptr : fruityprime::metadata::object_info(
                    static_cast<std::uint32_t>(data->model_id));
            if (info != nullptr) {
                add_entity_render_model(assets, source, info->name);
            }
            break;
        }
        case fruityprime::scene::EntityKind::Platform: {
            const auto* data = std::get_if<fruityprime::scene::PlatformData>(
                &source.typed_data);
            if (data == nullptr) {
                break;
            }
            if (const auto* info = fruityprime::metadata::platform_model_info(
                    data->model_id); info != nullptr) {
                add_entity_render_model(assets, source, info->name);
            }
            break;
        }
        case fruityprime::scene::EntityKind::Door: {
            const auto* data = std::get_if<fruityprime::scene::DoorData>(
                &source.typed_data);
            if (data == nullptr) {
                break;
            }
            if (const auto* info = fruityprime::metadata::door_info(
                    data->door_type); info != nullptr) {
                add_entity_render_model(assets, source, info->name);
                add_entity_render_model(assets, source, info->lock_name,
                                        info->lock_offset);
            }
            break;
        }
        case fruityprime::scene::EntityKind::JumpPad: {
            const auto* data = std::get_if<fruityprime::scene::JumpPadData>(
                &source.typed_data);
            if (data == nullptr) {
                break;
            }
            add_entity_render_model(assets, source,
                                    fruityprime::metadata::jump_pad_name(
                                        data->model_id));
            add_entity_render_model(assets, source, "JumpPad_Beam", 0.25F);
            break;
        }
        case fruityprime::scene::EntityKind::ItemSpawn: {
            const auto* data = std::get_if<fruityprime::scene::ItemSpawnData>(
                &source.typed_data);
            if (data != nullptr && data->has_base) {
                add_entity_render_model(assets, source, "items_base");
            }
            break;
        }
        case fruityprime::scene::EntityKind::Artifact: {
            const auto* data = std::get_if<fruityprime::scene::ArtifactData>(
                &source.typed_data);
            if (data == nullptr) {
                break;
            }
            std::string name = data->model_id >= 8 ? "Octolith" : "Artifact0";
            if (data->model_id < 8) {
                name += std::to_string(data->model_id + 1);
            }
            add_entity_render_model(assets, source, name,
                                    data->model_id >= 8 ? 1.75F : 0.5F);
            if (data->has_base) {
                add_entity_render_model(assets, source, "ArtifactBase");
            }
            break;
        }
        case fruityprime::scene::EntityKind::Teleporter: {
            const auto* data = std::get_if<fruityprime::scene::TeleporterData>(
                &source.typed_data);
            if (data == nullptr || data->invisible) {
                break;
            }
            add_entity_render_model(assets, source,
                                    data->artifact_id < 8
                                        ? "Teleporter" : "TeleporterSmall");
            break;
        }
        default:
            break;
        }
    }
    load_projectile_models(assets);
    load_beam_effect_models(assets);
    load_effect_resources(assets);
    load_effect_models(assets);
    load_enemy_models(assets);
    load_item_models(assets);
    g_effect_runtime = std::make_unique<fruityprime::effects::Runtime>(
        [](std::uint32_t effect_id) -> const fruityprime::effects::File* {
            if (effect_id >= g_effect_resources.size()
                || !g_effect_resources[effect_id].has_value()) {
                return nullptr;
            }
            return &g_effect_resources[effect_id].value();
        });
}

void attach_gameplay_message_sink() {
    if (!g_session.has_value()) {
        return;
    }
    g_session->set_message_sink([](const auto& info) {
        // The Win32 host owns the static-world instance directly rather than
        // through scene_runtime::Scene. Forward gameplay-generated cartridge
        // messages to both managed boundaries: GameState owns process-wide
        // story flags and the static world owns fixed-entity activation.
        g_game_state.process_message(info);
        if (g_story_save.has_value() && g_game_state.single_player) {
            g_story_save = g_game_state.story_save;
        }
        if (g_static_world != nullptr) {
            static_cast<void>(g_static_world->dispatch(info));
        }
    });
}

void bind_gorea_model_to_session() {
    if (!g_session.has_value()) {
        return;
    }
    const std::size_t gorea1a = static_cast<std::size_t>(
        fruityprime::formats::EnemyType::Gorea1A);
    const std::size_t gorea1b = static_cast<std::size_t>(
        fruityprime::formats::EnemyType::Gorea1B);
    const std::size_t gorea2 = static_cast<std::size_t>(
        fruityprime::formats::EnemyType::Gorea2);
    if (gorea1a < g_enemy_models.size()) {
        EntityRenderModel* render_model = g_enemy_models[gorea1a];
        if (render_model != nullptr && render_model->model != nullptr) {
            g_session->set_gorea1a_model(render_model->model.get());
        }
    }
    if (gorea1b < g_enemy_models.size()) {
        EntityRenderModel* render_model = g_enemy_models[gorea1b];
        if (render_model != nullptr && render_model->model != nullptr) {
            g_session->set_gorea1b_model(render_model->model.get());
        }
    }
    if (gorea2 < g_enemy_models.size()) {
        EntityRenderModel* render_model = g_enemy_models[gorea2];
        if (render_model != nullptr && render_model->model != nullptr) {
            g_session->set_gorea2_model(render_model->model.get());
        }
    }
}

void sync_process_game_state();

fruityprime::players::PlayerCamera& active_player_camera() noexcept {
    auto* main = fruityprime::players::PlayerEntity::Main();
    return main == nullptr ? g_unbound_player_camera : main->Camera();
}

void update_model_instance(fruityprime::model::ModelInstance& instance,
                           bool use_node_transform = true);

void clear_room_render_resources() {
    // DrawPrimitive stores non-owning model pointers. Destroy the draw list
    // before any room or catalog model so a room transition cannot leave a
    // stale pointer in the renderer.
    g_draw_primitives.clear();
    g_static_world.reset();
    g_room_instance.reset();
    active_player_camera().reset();
    g_entity_models.clear();
    for (auto& model : g_player_models) {
        model.instance.reset();
        model.alt_instance.reset();
        model.model.reset();
        model.alt_model.reset();
    }
    g_effect_runtime.reset();
    g_effect_runtime_seen.clear();
    for (auto& resource : g_effect_resources) {
        resource.reset();
    }
    g_loaded_effect_resources = 0;
    g_effect_models.clear();
    g_enemy_models.fill(nullptr);
    g_item_models.fill(nullptr);
    g_projectile_models.fill(nullptr);
    g_beam_effect_models.fill(nullptr);
    for (auto& image : g_texture_images) {
        if (image.gl_id != 0) {
            glDeleteTextures(1, &image.gl_id);
        }
    }
    g_texture_images.clear();
}

void populate_story_hunters(
    const fruityprime::scene::RoomCatalogEntry& catalog_entry) {
    g_bot_slots.clear();
    if (!g_story_save.has_value() || !g_room.has_value()
        || !g_session.has_value()) {
        return;
    }
    fruityprime::features::Registry story_settings;
    const auto hunter_spawners =
        fruityprime::scene_setup::collect_story_hunter_spawners(*g_room);
    const auto hunter_plan = fruityprime::scene_setup::select_hunter_spawns(
        hunter_spawners, catalog_entry.id,
        fruityprime::metadata::area_info(catalog_entry.id), g_game_state,
        *g_story_save, story_settings.features, story_settings.cheats,
        fruityprime::utility::global_rng(),
        static_cast<fruityprime::metadata::Hunter>(g_local_hunter), 0);
    for (const auto& hunter : hunter_plan.spawns) {
        std::uint8_t slot = 1;
        while (slot < fruityprime::net::NetConfig::SlotCapacity
               && g_session->has_player(slot)) {
            ++slot;
        }
        if (slot >= fruityprime::net::NetConfig::SlotCapacity) {
            break;
        }
        static_cast<void>(g_session->add_player(
            slot, static_cast<std::uint8_t>(hunter.hunter)));
        g_session->configure_story_hunter(
            slot, hunter.position, hunter.facing, hunter.health,
            hunter.health_max, hunter.health_threshold, hunter.hunter_weapon);
        g_game_state.encounter_state[slot] =
            static_cast<std::int32_t>(hunter.encounter_type);
        g_bot_slots.push_back(slot);
    }
}

[[nodiscard]] bool transition_game_room(
    const fruityprime::gameplay::RoomTransitionRequest& request) {
    if (!g_assets || !g_session.has_value()
        || !g_gameplay_config.has_value()
        || !g_game_state.single_player) {
        return false;
    }
    const auto* catalog_entry = fruityprime::scene::find_room(
        request.room_name);
    if (catalog_entry == nullptr) {
        g_session->clear_room_transition();
        return false;
    }

    try {
        clear_room_render_resources();
        g_spectator.reset();
        fruityprime::players::PlayerEntity::Reset();
        g_session.reset();
        g_room.reset();
        fruityprime::scene::RoomDefinition definition =
            catalog_entry->definition;
        if (const auto* metadata =
                fruityprime::metadata::find_room_metadata(
                    catalog_entry->name);
            metadata != nullptr && !metadata->node_path.empty()) {
            definition.node_path = metadata->node_path;
        }
        definition.node_path =
            fruityprime::scene_setup::resolve_node_data_path(
                definition.node_path, catalog_entry->id,
                fruityprime::game::Mode::Story);
        g_room.emplace(fruityprime::scene::Room::load(*g_assets, definition));
        name_window_after_room(*catalog_entry);
        set_room_lights(catalog_entry->id);
        // This path is guarded by single player above.
        apply_room_node_layers(*g_room, /*single_player=*/true,
                               /*player_count=*/1, /*capture=*/false);
        g_room_instance.emplace(g_room->model());
        update_model_instance(*g_room_instance,
                              /*use_node_transform=*/false);
        append_model_geometry(g_room->model(), DrawLayer::Room);
        load_entity_models(*g_assets);
        load_hunter_models(*g_assets);
        g_session.emplace(*g_room, *g_gameplay_config);
        fruityprime::players::PlayerEntity::Construct(*g_session,
                                                       &g_game_state);
        bind_gorea_model_to_session();
        attach_gameplay_message_sink();

        g_game_state.begin_room(
            catalog_entry->name, static_cast<std::uint32_t>(catalog_entry->id),
            false, fruityprime::game::Mode::Story);
        g_game_state.local_slot = g_local_slot;
        g_game_state.friendly_fire = g_gameplay_config->friendly_fire;
        g_game_state.match_time = -1.0F;
        g_game_state.match_clock_enabled = false;
        g_game_state.point_goal = 0;
        g_game_state.teams = false;
        static_cast<void>(g_session->add_player(g_local_slot, g_local_hunter));
        if (g_story_save.has_value()) {
            g_session->apply_story_save(g_local_slot, *g_story_save);
            g_session->apply_story_room_state(
                catalog_entry->id, *g_story_save);
            populate_story_hunters(*catalog_entry);
            g_game_state.story_save = *g_story_save;
        }

        const fruityprime::scene::EntityInstance* target = nullptr;
        for (const auto& entity : g_room->entities()) {
            if (entity.kind != fruityprime::scene::EntityKind::Teleporter) {
                continue;
            }
            const auto* data = std::get_if<fruityprime::scene::TeleporterData>(
                &entity.typed_data);
            if (data != nullptr
                && data->load_index == request.target_entity_id) {
                target = &entity;
                break;
            }
        }
        if (target != nullptr) {
            fruityprime::net::Vec3 position{
                target->position.x.to_float(), target->position.y.to_float(),
                target->position.z.to_float()};
            position.y += 0.5F;
            const fruityprime::net::Vec3 facing{
                target->facing_vector.x.to_float(),
                target->facing_vector.y.to_float(),
                target->facing_vector.z.to_float()};
            g_session->place_player(
                g_local_slot, position, facing, request.alt_form);
        }
        if (g_music_controller != nullptr && g_music_runtime != nullptr) {
            fruityprime::sound::Music::TryPlayRoomMusic(catalog_entry->id, 0);
            g_music_runtime->update(0.0F);
        }
        if (g_effect_runtime != nullptr) {
            g_effect_runtime->clear();
            g_effect_runtime_seen.clear();
        }
        g_model_animation_tick = std::numeric_limits<std::uint64_t>::max();
        sync_process_game_state();
        return true;
    } catch (const std::exception& error) {
        g_error = std::string("native room transition failed: ")
            + error.what();
        fruityprime::players::PlayerEntity::Reset();
        g_session.reset();
        g_room.reset();
        return false;
    }
}

void set_hud_image(std::optional<HudImage>& destination,
                   const MphReadNative::Hud::HudTexture& texture) {
    if (!texture.valid()) {
        return;
    }
    if (texture.pixels.size()
        > std::numeric_limits<std::size_t>::max() / 4) {
        throw std::runtime_error("HUD texture is too large for RGBA upload");
    }
    HudImage image;
    image.width = texture.width;
    image.height = texture.height;
    image.rgba.resize(texture.pixels.size() * 4);
    for (std::size_t i = 0; i < texture.pixels.size(); ++i) {
        const auto& color = texture.pixels[i];
        image.rgba[i * 4 + 0] = color.red;
        image.rgba[i * 4 + 1] = color.green;
        image.rgba[i * 4 + 2] = color.blue;
        image.rgba[i * 4 + 3] = color.alpha;
    }
    destination = std::move(image);
}

void set_hud_object_image(
    std::optional<HudObjectImage>& destination,
    const MphReadNative::Hud::HudObjectAsset& object) {
    if (object.width <= 0 || object.height <= 0
        || object.image_count == 0) {
        throw std::runtime_error("HUD object has no renderable images");
    }
    const std::size_t palette_count = std::max<std::size_t>(
        1, object.palette.size() / 16);
    if (palette_count > std::numeric_limits<std::size_t>::max()
            / object.image_count) {
        throw std::runtime_error("HUD object image table is too large");
    }
    HudObjectImage result;
    result.width = object.width;
    result.height = object.height;
    result.image_count = object.image_count;
    result.palette_count = palette_count;
    result.images.reserve(palette_count * object.image_count);
    for (std::size_t palette = 0; palette < palette_count; ++palette) {
        for (std::size_t image_index = 0;
             image_index < object.image_count; ++image_index) {
            const auto texture = object.render_frame(image_index, palette);
            if (!texture.valid()) {
                throw std::runtime_error(
                    "HUD object image did not decode");
            }
            std::optional<HudImage> image;
            set_hud_image(image, texture);
            if (!image.has_value()) {
                throw std::runtime_error(
                    "HUD object image upload buffer is empty");
            }
            result.images.push_back(std::move(*image));
        }
    }
    destination = std::move(result);
}

// The HUD's own alphabet, as one 8x8 frame per glyph.
//
// It is not an object file: the cartridge stores the font as widths, vertical
// offsets and packed 4bpp characters, which Extract has already unpacked into
// one byte per pixel by the time this runs.  Wrapping it in the same asset
// type the bars use means the glyphs go through exactly the same palette
// lookup and upload as everything else on the HUD, so a run of text is the
// colour the bar beside it is.
// A HUD message by id, or nothing when the tables are not loaded -- which is
// every build with no cartridge behind it, and is not an error there.
[[nodiscard]] std::string hud_message(std::uint32_t id) {
    if (g_hud_msgs_common.empty() && g_hud_msgs_single.empty()
        && g_hud_msgs_multi.empty()) {
        return {};
    }
    return fruityprime::strings::get_hud_message(
        id, g_hud_msgs_common, g_hud_msgs_single, g_hud_msgs_multi);
}

// SceneSetup: the arena intros are sequences 172 to 198, one per
// multiplayer room, in room-id order from MP1.
void load_intro_sequence(const fruityprime::assets::Store& assets,
                         int room_id) {
    g_intro.reset();
    g_intro_file.reset();
    g_intro_seconds = 0.0F;
    const int sequence = room_id - 93 + 172;
    if (sequence < 172 || sequence > 198) {
        return;
    }
    const std::string path = fruityprime::camera::asset_path(sequence);
    if (path.empty()) {
        return;
    }
    try {
        g_intro_file.emplace(fruityprime::camera::File::from_bytes(
            assets.bytes(path), sequence));
        g_intro.emplace(sequence, *g_intro_file);
        // GameState.SetUpMatch loops it: the fly-through is shorter than the
        // time a player may take to press fire.
        g_intro->set_flags(g_intro->flags()
                           | fruityprime::camera::Flags::Loop);
        g_intro->set_up(fruityprime::camera::CameraState{});
    } catch (const std::exception& error) {
        g_intro.reset();
        g_intro_file.reset();
        OutputDebugStringA((std::string("Fruity Prime intro camera "
            "unavailable: ") + error.what() + "\n").c_str());
    }
}

void load_hud_messages(const fruityprime::assets::Store& assets) {
    g_hud_msgs_common.clear();
    g_hud_msgs_single.clear();
    g_hud_msgs_multi.clear();
    const auto read = [&assets](std::string_view name,
                                std::vector<fruityprime::strings::TableEntry>&
                                    destination) {
        try {
            destination = fruityprime::strings::read_table(
                assets.bytes("stringTables/" + std::string(name)), false);
        } catch (const std::exception&) {
            // A regional dump missing one table is not worth stopping for:
            // the readouts that use it simply have no word beside them.
        }
    };
    read(fruityprime::strings::tables::HudMsgsCommon, g_hud_msgs_common);
    read(fruityprime::strings::tables::HudMessagesSp, g_hud_msgs_single);
    read(fruityprime::strings::tables::HudMessagesMp, g_hud_msgs_multi);
}

void load_hud_font(const fruityprime::assets::Store& assets,
                   std::string_view palette_source) {
    g_hud_font.reset();
    const auto& characters =
        fruityprime::strings::Font::normal().character_data();
    if (characters.size() < 64) {
        return;
    }
    try {
        // Any HUD object drawn in the same green will do for the palette; the
        // managed launcher takes the ammo bar's, so this does too.
        const auto source = MphReadNative::Hud::parse_object(
            assets.bytes(std::string(palette_source)), palette_source);
        MphReadNative::Hud::HudObjectAsset font;
        font.width = 8;
        font.height = 8;
        font.image_count = static_cast<std::uint16_t>(
            std::min<std::size_t>(characters.size() / 64, 0xFFFFU));
        font.frame_count = font.image_count;
        font.character_indices.assign(characters.begin(), characters.end());
        font.palette = source.palette;
        set_hud_object_image(g_hud_font, font);
        // The same glyphs with one flat white entry: every lit pixel becomes
        // white, which is what an explicit text colour multiplies.
        MphReadNative::Hud::HudObjectAsset mask = font;
        mask.palette.assign(16, MphReadNative::Hud::ColorRgba{});
        for (std::size_t i = 1; i < mask.palette.size(); ++i) {
            mask.palette[i] = MphReadNative::Hud::ColorRgba{255, 255, 255,
                                                           255};
        }
        set_hud_object_image(g_hud_font_mask, mask);
    } catch (const std::exception& error) {
        const std::string message = std::string("Fruity Prime HUD font "
            "unavailable: ") + error.what() + "\n";
        OutputDebugStringA(message.c_str());
    }
}

void load_hud_object(const fruityprime::assets::Store& assets,
                     std::optional<HudObjectImage>& destination,
                     std::string asset_path, std::string_view kind) {
    destination.reset();
    try {
        const auto object = MphReadNative::Hud::parse_object(
            assets.bytes(asset_path), asset_path);
        set_hud_object_image(destination, object);
    } catch (const std::exception& error) {
        const std::string message = "Fruity Prime HUD " + std::string(kind)
            + " unavailable: " + error.what() + " (" + asset_path + ")\n";
        OutputDebugStringA(message.c_str());
    }
}

void load_hud_assets(const fruityprime::assets::Store& assets,
                     std::uint8_t hunter) {
    struct HunterHudPaths {
        std::string_view local_archive;
        std::string_view helmet_archive;
        std::string_view visor_archive;
    };
    constexpr std::array<HunterHudPaths, 8> paths{{
        {"Samus", "Samus", "Samus"},
        {"Kanden", "Kanden", "Kanden"},
        {"Trace", "Trace", "Trace"},
        {"Sylux", "Sylux", "Sylux"},
        {"Nox", "Nox", "Nox"},
        {"Spire", "Spire", "Spire"},
        {"Weavel", "Weavel", "Weavel"},
        // The managed Guardian table reuses these hunter layers.
        {"Samus", "Weavel", "Kanden"}
    }};
    const auto& path = paths[std::min<std::size_t>(hunter, paths.size() - 1)];

    const auto report_unavailable = [](std::string_view kind,
                                       std::string_view asset_path,
                                       const std::exception& error) {
        const std::string message = "Fruity Prime HUD " + std::string(kind)
            + " unavailable: " + std::string(error.what())
            + " (" + std::string(asset_path) + ")\n";
        OutputDebugStringA(message.c_str());
    };

    const auto load_char_map = [&assets, &report_unavailable](
                                   std::optional<HudImage>& destination,
                                   std::string asset_path, int start_x,
                                   int start_y, int tiles_x, int tiles_y,
                                   std::string_view kind) {
        destination.reset();
        try {
            const auto texture = MphReadNative::Hud::decode_char_map(
                assets.bytes(asset_path), start_x, start_y, tiles_x, tiles_y,
                {}, -1, asset_path);
            set_hud_image(destination, texture);
        } catch (const std::exception& error) {
            report_unavailable(kind, asset_path, error);
        }
    };

    g_hud_reticle.reset();
    try {
        const std::string asset_path = "_archives/local"
            + std::string(path.local_archive) + "/hud_targetcircle.bin";
        const auto object = MphReadNative::Hud::parse_object(
            assets.bytes(asset_path), asset_path);
        set_hud_image(g_hud_reticle, object.render_frame());
    } catch (const std::exception& error) {
        report_unavailable("reticle", "target-circle", error);
    }

    load_char_map(g_hud_helmet,
                  "_archives/local" + std::string(path.helmet_archive)
                      + "/bg_top.bin",
                  0, 0, 0, 0, "helmet");
    load_char_map(g_hud_helmet_drop,
                  "_archives/local" + std::string(path.helmet_archive)
                      + "/bg_top_drop.bin",
                  0, 0, 0, 0, "helmet-drop");
    load_char_map(g_hud_visor,
                  "_archives/local" + std::string(path.visor_archive)
                      + "/bg_top_ovl.bin",
                  0, 0, 0, 32, "visor");
    load_char_map(g_hud_scan_visor,
                  "_archives/localSamus/bg_top_ovl.bin",
                  0, 96, 0, 32, "scan-visor");
    load_char_map(g_hud_ice, "_archives/common/bg_ice.bin",
                  16, 0, 32, 32, "ice");

    // Guardian uses the managed HUD table's Samus object set. The other
    // hunters keep their own energy/ammo/icon assets in their local archive.
    const std::string_view object_archive = hunter == 7
        ? "Samus" : path.local_archive;
    const auto object_path = [&object_archive](std::string_view filename) {
        return "_archives/local" + std::string(object_archive) + "/"
            + std::string(filename);
    };
    load_hud_object(assets, g_hud_health_bar,
                    object_path("hud_energybar.bin"), "health-bar");
    load_hud_object(assets, g_hud_health_bar_sub,
                    object_path("hud_energybar2.bin"), "health-bar-sub");
    load_hud_object(assets, g_hud_ammo_bar,
                    object_path("hud_ammobar.bin"), "ammo-bar");
    load_hud_object(assets, g_hud_weapon_icon,
                    object_path("hud_weaponicon.bin"), "weapon-icon");
    load_hud_font(assets, object_path("hud_ammobar.bin"));
    load_hud_messages(assets);
    const auto& elements = MphReadNative::Hud::elements();
    for (std::size_t i = 0; i < g_hud_hunters.size(); ++i) {
        load_hud_object(assets, g_hud_hunters[i], elements.hunters[i],
                        "hunter-portrait");
    }
    load_hud_object(assets, g_hud_stars, elements.stars, "stars");
}

void create_hud_texture(HudImage& image) {
    if (image.width <= 0 || image.height <= 0 || image.rgba.empty()) {
        return;
    }
    glGenTextures(1, &image.gl_id);
    glBindTexture(GL_TEXTURE_2D, image.gl_id);
    const GLint filter = fruityprime::mods::render::options()
                             .texture_filtering()
        ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
}

void create_hud_object_textures(std::optional<HudObjectImage>& object) {
    if (!object.has_value()) {
        return;
    }
    for (auto& image : object->images) {
        create_hud_texture(image);
    }
}

void upload_gl_texture(TextureImage& image) {
    if (image.gl_id != 0 || image.width <= 0 || image.height <= 0
        || image.rgba.empty()) {
        return;
    }
    glGenTextures(1, &image.gl_id);
    glBindTexture(GL_TEXTURE_2D, image.gl_id);
    const GLint filter = fruityprime::mods::render::options()
                             .texture_filtering()
        ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
}

void create_gl_textures() {
    for (auto& image : g_texture_images) {
        upload_gl_texture(image);
    }
    if (g_hud_reticle.has_value()) {
        create_hud_texture(*g_hud_reticle);
    }
    if (g_hud_ice.has_value()) {
        create_hud_texture(*g_hud_ice);
    }
    if (g_hud_helmet.has_value()) {
        create_hud_texture(*g_hud_helmet);
    }
    if (g_hud_helmet_drop.has_value()) {
        create_hud_texture(*g_hud_helmet_drop);
    }
    if (g_hud_visor.has_value()) {
        create_hud_texture(*g_hud_visor);
    }
    if (g_hud_scan_visor.has_value()) {
        create_hud_texture(*g_hud_scan_visor);
    }
    create_hud_object_textures(g_hud_health_bar);
    create_hud_object_textures(g_hud_health_bar_sub);
    create_hud_object_textures(g_hud_ammo_bar);
    create_hud_object_textures(g_hud_weapon_icon);
    create_hud_object_textures(g_hud_font);
    create_hud_object_textures(g_hud_font_mask);
    for (auto& portrait : g_hud_hunters) {
        create_hud_object_textures(portrait);
    }
    create_hud_object_textures(g_hud_stars);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_primitive(
    const DrawPrimitive& drawable,
    const fruityprime::model::ModelInstance* instance = nullptr,
    const std::array<float, 4>* tint = nullptr) {
    const auto& primitive = drawable.geometry;
    const fruityprime::model::Material* authored_material = nullptr;
    if (drawable.owner != nullptr
        && drawable.material_id < drawable.owner->materials().size()) {
        authored_material = &drawable.owner->materials()[drawable.material_id];
    }
    const auto color_vector = [](const fruityprime::model::ColorRgb& color) {
        return fruityprime::formats::Vector3{
            static_cast<float>(color.red) / 31.0F,
            static_cast<float>(color.green) / 31.0F,
            static_cast<float>(color.blue) / 31.0F};
    };
    fruityprime::formats::Vector3 diffuse{1.0F, 1.0F, 1.0F};
    fruityprime::formats::Vector3 ambient{};
    fruityprime::formats::Vector3 specular{};
    float material_alpha = 1.0F;
    bool lighting = false;
    auto culling = fruityprime::formats::CullingMode::Neither;
    bool wireframe = false;
    auto polygon_mode = fruityprime::formats::PolygonMode::Modulate;
    auto x_repeat = fruityprime::formats::RepeatMode::Clamp;
    auto y_repeat = fruityprime::formats::RepeatMode::Clamp;
    if (authored_material != nullptr) {
        diffuse = color_vector(authored_material->diffuse);
        ambient = color_vector(authored_material->ambient);
        specular = color_vector(authored_material->specular);
        material_alpha = static_cast<float>(authored_material->alpha) / 31.0F;
        lighting = authored_material->lighting != 0;
        culling = static_cast<fruityprime::formats::CullingMode>(
            std::min<std::uint8_t>(authored_material->culling, 2));
        wireframe = authored_material->wireframe != 0;
        polygon_mode = static_cast<fruityprime::formats::PolygonMode>(
            std::min<std::uint32_t>(authored_material->polygon_mode, 3));
        x_repeat = static_cast<fruityprime::formats::RepeatMode>(
            std::min<std::uint8_t>(authored_material->x_repeat, 2));
        y_repeat = static_cast<fruityprime::formats::RepeatMode>(
            std::min<std::uint8_t>(authored_material->y_repeat, 2));
    }
    if (instance != nullptr && drawable.owner == &instance->model()
        && drawable.material_id < instance->material_states().size()) {
        const auto& runtime_material = instance->material_states()[
            drawable.material_id];
        diffuse = runtime_material.current_diffuse;
        ambient = runtime_material.current_ambient;
        specular = runtime_material.current_specular;
        material_alpha = runtime_material.current_alpha;
    }
    const auto& render_options = fruityprime::mods::render::options();
    lighting = render_options.lighting() && lighting;
    const float tint_alpha = tint == nullptr ? 1.0F : (*tint)[3];
    const float draw_alpha = std::clamp(material_alpha * tint_alpha,
                                        0.0F, 1.0F);
    if (lighting) {
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
        const GLfloat material_ambient[] = {
            ambient.x, ambient.y, ambient.z, 1.0F};
        const GLfloat material_specular[] = {
            specular.x, specular.y, specular.z, 1.0F};
        constexpr GLfloat material_emission[] = {0.0F, 0.0F, 0.0F, 1.0F};
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material_ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, material_emission);
    } else {
        glDisable(GL_LIGHTING);
    }
    if (culling == fruityprime::formats::CullingMode::Neither) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(culling == fruityprime::formats::CullingMode::Front
                       ? GL_FRONT : GL_BACK);
    }
    glPolygonMode(GL_FRONT_AND_BACK,
                  wireframe ? GL_LINE : GL_FILL);
    if (drawable.texture_index != NoTexture
        && drawable.texture_index < g_texture_images.size()) {
        const auto repeat_gl = [](fruityprime::formats::RepeatMode mode) {
            switch (mode) {
            case fruityprime::formats::RepeatMode::Clamp:
                return static_cast<GLint>(0x812f); // GL_CLAMP_TO_EDGE
            case fruityprime::formats::RepeatMode::Repeat:
                return static_cast<GLint>(GL_REPEAT);
            case fruityprime::formats::RepeatMode::Mirror:
                return static_cast<GLint>(0x8370); // GL_MIRRORED_REPEAT
            }
            return static_cast<GLint>(0x812f);
        };
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        repeat_gl(x_repeat));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        repeat_gl(y_repeat));
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                  polygon_mode == fruityprime::formats::PolygonMode::Decal
                      ? GL_DECAL : GL_MODULATE);
    } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    GLenum mode = GL_TRIANGLES;
    switch (primitive.type) {
    case fruityprime::model::GeometryPrimitiveType::Triangles:
        mode = GL_TRIANGLES;
        break;
    case fruityprime::model::GeometryPrimitiveType::Quads:
        mode = GL_QUADS;
        break;
    case fruityprime::model::GeometryPrimitiveType::TriangleStrip:
        mode = GL_TRIANGLE_STRIP;
        break;
    case fruityprime::model::GeometryPrimitiveType::QuadStrip:
        mode = GL_QUAD_STRIP;
        break;
    }
    // Renderer's pass split: the opaque pass takes only fragments whose alpha
    // is exactly 1 and writes depth; the translucent passes run with
    // DepthMask(false).  Writing depth for a polygon that is not fully opaque
    // punches a hole in everything drawn after it -- most visibly the arm
    // cannon, whose five beam glows are invisible (alpha 0) but were still
    // stamping their shape through the barrel behind them.
    const bool opaque = draw_alpha >= 1.0F;
    if (!opaque) {
        glDepthMask(GL_FALSE);
    }
    glBegin(mode);
    const int cel_bands = std::max(2, render_options.cel_bands());
    const auto shade = [cel_bands, &render_options](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        if (!render_options.cel_shading()) {
            return value;
        }
        const float steps = static_cast<float>(cel_bands - 1);
        return std::floor(value * steps + 0.5F) / steps;
    };
    for (const auto& vertex : primitive.vertices) {
        const std::uint32_t color = vertex.color;
        const auto vertex_color = fruityprime::formats::Vector3{
            static_cast<float>((color >> 0) & 0x1f) / 31.0F,
            static_cast<float>((color >> 5) & 0x1f) / 31.0F,
            static_cast<float>((color >> 10) & 0x1f) / 31.0F};
        // With lighting enabled the managed shader uses the material diffuse
        // unless the display list emitted DIF_AMB (whose alpha marker is
        // retained in GeometryVertex).  With lighting disabled the display
        // list colour is the final vertex colour.  This is also why an
        // omitted COLOR must fall back to the material rather than black.
        const bool use_vertex_color = !lighting || vertex.diffuse_ambient;
        fruityprime::formats::Vector3 color_value =
            use_vertex_color && vertex.has_color ? vertex_color : diffuse;
        float red = color_value.x;
        float green = color_value.y;
        float blue = color_value.z;
        float alpha = draw_alpha;
        if (tint != nullptr) {
            red *= (*tint)[0];
            green *= (*tint)[1];
            blue *= (*tint)[2];
        }
        glColor4f(shade(red), shade(green), shade(blue), alpha);
        fruityprime::formats::Vector3 position{
            vertex.x, vertex.y, vertex.z};
        fruityprime::formats::Vector3 normal{
            vertex.normal_x, vertex.normal_y, vertex.normal_z};
        float texture_s = vertex.texture_s;
        float texture_t = vertex.texture_t;
        if (instance != nullptr && drawable.owner == &instance->model()
            && vertex.matrix_id < instance->matrix_stack().size()) {
            const auto& matrix = instance->matrix_stack()[vertex.matrix_id];
            position = fruityprime::formats::MatrixOps::vec3_mult_mtx4(
                position, matrix);
            normal = fruityprime::formats::Vector3{
                normal.x * matrix.m11 + normal.y * matrix.m21
                    + normal.z * matrix.m31,
                normal.x * matrix.m12 + normal.y * matrix.m22
                    + normal.z * matrix.m32,
                normal.x * matrix.m13 + normal.y * matrix.m23
                    + normal.z * matrix.m33
            }.normalized();
        }
        if (instance != nullptr && drawable.owner == &instance->model()
            && drawable.material_id < instance->material_states().size()) {
            const auto& material = instance->material_states()[
                drawable.material_id];
            const auto transformed_uv =
                fruityprime::formats::MatrixOps::vec3_mult_mtx4(
                    {texture_s, texture_t, 0.0F}, material.texcoord_matrix);
            texture_s = transformed_uv.x;
            texture_t = transformed_uv.y;
        }
        glNormal3f(normal.x, normal.y, normal.z);
        glTexCoord2f(texture_s, texture_t);
        glVertex3f(position.x, position.y, position.z);
    }
    glEnd();
    if (!opaque) {
        glDepthMask(GL_TRUE);
    }
}

void draw_fallback() {
    glBegin(GL_TRIANGLES);
    glColor3f(0.95F, 0.55F, 0.12F);
    glVertex3f(-1.0F, -1.0F, 0.0F);
    glColor3f(0.18F, 0.75F, 0.92F);
    glVertex3f(1.0F, -1.0F, 0.0F);
    glColor3f(0.45F, 0.95F, 0.34F);
    glVertex3f(0.0F, 1.0F, 0.0F);
    glEnd();
}

// The placeholder box stands in when a hunter model could not be loaded.  It
// follows the same first-person rule as the real biped.
void draw_player_marker(int hidden_slot) {
    const bool has_player_model = std::any_of(
        g_player_models.begin(), g_player_models.end(),
        [](const HunterModelSet& model) {
            return model.model.has_value() || model.alt_model.has_value();
        });
    if (has_player_model || !g_session.has_value()
        || g_session->players().empty()) {
        return;
    }
    constexpr float half_width = 0.28F;
    constexpr float height = 1.4F;
    glColor3f(0.98F, 0.72F, 0.12F);
    for (const auto& player : g_session->players()) {
        if (hidden_slot >= 0
            && player.slot_index == static_cast<std::uint8_t>(hidden_slot)) {
            continue;
        }
        if (player.health == 0
            || (player.flags & fruityprime::net::PlayerState::FlagActive) == 0
            || (player.flags & fruityprime::net::PlayerState::FlagSpawned) == 0
            || (player.flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            continue;
        }
        glPushMatrix();
        glTranslatef(player.position.x, player.position.y, player.position.z);
        glBegin(GL_QUADS);
    glVertex3f(-half_width, 0.0F, -half_width);
    glVertex3f(half_width, 0.0F, -half_width);
    glVertex3f(half_width, height, -half_width);
    glVertex3f(-half_width, height, -half_width);
    glVertex3f(half_width, 0.0F, half_width);
    glVertex3f(-half_width, 0.0F, half_width);
    glVertex3f(-half_width, height, half_width);
    glVertex3f(half_width, height, half_width);
    glVertex3f(-half_width, 0.0F, half_width);
    glVertex3f(-half_width, 0.0F, -half_width);
    glVertex3f(-half_width, height, -half_width);
    glVertex3f(-half_width, height, half_width);
    glVertex3f(half_width, 0.0F, -half_width);
    glVertex3f(half_width, 0.0F, half_width);
    glVertex3f(half_width, height, half_width);
    glVertex3f(half_width, height, -half_width);
    glVertex3f(-half_width, height, -half_width);
    glVertex3f(half_width, height, -half_width);
    glVertex3f(half_width, height, half_width);
    glVertex3f(-half_width, height, half_width);
        glEnd();
        glPopMatrix();
    }
}

void draw_item_markers() {
    if (!g_session.has_value()) {
        return;
    }
    glDisable(GL_TEXTURE_2D);
    const float spin = static_cast<float>(g_session->tick_count() % 360);
    for (const auto& item : g_session->items()) {
        const auto raw_type = static_cast<std::int32_t>(item.type);
        if (raw_type >= 0
            && raw_type < static_cast<std::int32_t>(g_item_models.size())) {
            const auto* render_model = g_item_models[static_cast<std::size_t>(
                raw_type)];
            if (render_model != nullptr && render_model->instance.has_value()) {
                continue;
            }
        }
        switch (item.type) {
        case fruityprime::gameplay::ItemType::HealthMedium:
        case fruityprime::gameplay::ItemType::HealthSmall:
        case fruityprime::gameplay::ItemType::HealthBig:
            glColor3f(0.18F, 0.95F, 0.34F);
            break;
        case fruityprime::gameplay::ItemType::UASmall:
        case fruityprime::gameplay::ItemType::UABig:
            glColor3f(0.25F, 0.78F, 1.0F);
            break;
        case fruityprime::gameplay::ItemType::MissileSmall:
        case fruityprime::gameplay::ItemType::MissileBig:
            glColor3f(1.0F, 0.34F, 0.18F);
            break;
        case fruityprime::gameplay::ItemType::Cloak:
            glColor3f(0.72F, 0.42F, 1.0F);
            break;
        case fruityprime::gameplay::ItemType::DoubleDamage:
            glColor3f(1.0F, 0.78F, 0.16F);
            break;
        default:
            glColor3f(0.96F, 0.92F, 0.66F);
            break;
        }
        constexpr float half = 0.18F;
        glPushMatrix();
        glTranslatef(item.position.x, item.position.y, item.position.z);
        glRotatef(spin, 0.0F, 1.0F, 0.0F);
        glBegin(GL_QUADS);
        glVertex3f(-half, -half, -half);
        glVertex3f(half, -half, -half);
        glVertex3f(half, half, -half);
        glVertex3f(-half, half, -half);
        glVertex3f(half, -half, half);
        glVertex3f(-half, -half, half);
        glVertex3f(-half, half, half);
        glVertex3f(half, half, half);
        glVertex3f(-half, -half, half);
        glVertex3f(-half, -half, -half);
        glVertex3f(-half, half, -half);
        glVertex3f(-half, half, half);
        glVertex3f(half, -half, -half);
        glVertex3f(half, -half, half);
        glVertex3f(half, half, half);
        glVertex3f(half, half, -half);
        glVertex3f(-half, half, -half);
        glVertex3f(half, half, -half);
        glVertex3f(half, half, half);
        glVertex3f(-half, half, half);
        glVertex3f(-half, -half, -half);
        glVertex3f(-half, -half, half);
        glVertex3f(half, -half, half);
        glVertex3f(half, -half, -half);
        glEnd();
        glPopMatrix();
    }
}

using HudGlyph = std::array<std::uint8_t, 7>;

[[nodiscard]] HudGlyph hud_glyph(char value) noexcept {
    switch (value) {
    case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'B': return {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    case 'C': return {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f};
    case 'D': return {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
    case 'E': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    case 'F': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10};
    case 'G': return {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f};
    case 'H': return {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'I': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f};
    case 'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    case 'M': return {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O': return {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'P': return {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    case 'Q': return {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d};
    case 'R': return {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    case 'S': return {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a};
    case 'X': return {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f};
    case '0': return {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
    case '1': return {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x1f};
    case '2': return {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
    case '3': return {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};
    case '4': return {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
    case '5': return {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
    case '6': return {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e};
    case '7': return {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
    case '9': return {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e};
    case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    case '-': return {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    case '/': return {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
    case '#': return {0x0a, 0x1f, 0x0a, 0x0a, 0x1f, 0x0a, 0x00};
    default: return {};
    }
}

void draw_hud_box(float left, float top, float right, float bottom) {
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
}

struct HudTint {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
};

void draw_hud_texture(const HudImage& image, float left, float top,
                      float right, float bottom, float alpha = 1.0F,
                      HudTint tint = {}) {
    if (image.gl_id == 0 || image.width <= 0 || image.height <= 0) {
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, image.gl_id);
    glColor4f(tint.red, tint.green, tint.blue,
              std::clamp(alpha, 0.0F, 1.0F));
    glBegin(GL_QUADS);
    // The decoder hands over rows top to bottom and glTexImage2D takes the
    // first of them as v=0, so v runs the same way the screen does: v=0 at
    // the top edge.  Screen-space geometry stays in the coordinates
    // HudInfo.CharMapToTexture uses.
    glTexCoord2f(0.0F, 0.0F);
    glVertex2f(left, top);
    glTexCoord2f(1.0F, 0.0F);
    glVertex2f(right, top);
    glTexCoord2f(1.0F, 1.0F);
    glVertex2f(right, bottom);
    glTexCoord2f(0.0F, 1.0F);
    glVertex2f(left, bottom);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool draw_hud_object_image(const HudObjectImage& object, std::size_t image_index,
                     std::size_t palette_index, float left, float top,
                     float scale, float alpha = 1.0F, HudTint tint = {}) {
    const HudImage* image = object.frame(image_index, palette_index);
    if (image == nullptr) {
        return false;
    }
    draw_hud_texture(*image, left, top,
                     left + static_cast<float>(object.width) * scale,
                     top + static_cast<float>(object.height) * scale, alpha,
                     tint);
    return true;
}

void draw_hud_layers(const MphReadNative::Hud::PlayerState& player,
                     const MphReadNative::Hud::Rect& area, float scale,
                     bool scan_visor) {
    if (scale <= 0.0F || player.health == 0 || player.alt_form
        || player.spectating) {
        return;
    }
    const float center_x = (area.left + area.right) * 0.5F;
    const float center_y = (area.top + area.bottom) * 0.5F;

    // PlayerHud.DrawHudModels renders these in this order: ice, helmet back,
    // visor, and helmet front.  The source char maps intentionally keep their
    // 256/512 pixel widths; the managed renderer applies the same 2x helmet
    // scale and 256/192 vertical correction here in screen coordinates.
    if (player.frozen && g_hud_ice.has_value()) {
        const float ice_width = 256.0F * scale;
        const float ice_height = 256.0F * scale;
        draw_hud_texture(*g_hud_ice,
                         center_x - ice_width * 0.5F,
                         center_y - ice_height * 0.5F,
                         center_x + ice_width * 0.5F,
                         center_y + ice_height * 0.5F,
                         9.0F / 16.0F);
    }
    if (g_hud_helmet_drop.has_value()) {
        const float width = 512.0F * scale;
        const float height = 256.0F * scale;
        draw_hud_texture(*g_hud_helmet_drop,
                         center_x - width * 0.5F,
                         center_y - height * 0.5F,
                         center_x + width * 0.5F,
                         center_y + height * 0.5F);
    }
    const HudImage* visor = scan_visor && g_hud_scan_visor.has_value()
        ? &*g_hud_scan_visor
        : (g_hud_visor.has_value() ? &*g_hud_visor : nullptr);
    if (visor != nullptr) {
        const float width = 256.0F * scale;
        const float height = 256.0F * scale;
        draw_hud_texture(*visor,
                         center_x - width * 0.5F,
                         center_y - height * 0.5F,
                         center_x + width * 0.5F,
                         center_y + height * 0.5F);
    }
    if (g_hud_helmet.has_value()) {
        const float width = 512.0F * scale;
        const float height = 256.0F * scale;
        draw_hud_texture(*g_hud_helmet,
                         center_x - width * 0.5F,
                         center_y - height * 0.5F,
                         center_x + width * 0.5F,
                         center_y + height * 0.5F);
    }
}

void draw_hud_text(std::string_view text, float left, float top,
                   float scale) {
    if (scale <= 0.0F) {
        return;
    }
    float cursor = left;
    glBegin(GL_QUADS);
    for (const char value : text) {
        if (value == ' ') {
            cursor += 6.0F * scale;
            continue;
        }
        const char upper = static_cast<char>(std::toupper(
            static_cast<unsigned char>(value)));
        const HudGlyph glyph = hud_glyph(upper);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1u << (4 - column))) == 0) {
                    continue;
                }
                const float x = cursor + static_cast<float>(column) * scale;
                const float y = top + static_cast<float>(row) * scale;
                glVertex2f(x, y);
                glVertex2f(x + scale, y);
                glVertex2f(x + scale, y + scale);
                glVertex2f(x, y + scale);
            }
        }
        cursor += 6.0F * scale;
    }
    glEnd();
}

[[nodiscard]] std::string roster_name(std::uint8_t slot) {
    if (g_roster.has_value()) {
        for (std::size_t i = 0; i < g_roster->count
            && i < fruityprime::net::RosterPacket::MaxSlots; ++i) {
            if (g_roster->slots[i] == slot && !g_roster->names[i].empty()) {
                return g_roster->names[i];
            }
        }
    }
    return "PLAYER " + std::to_string(static_cast<unsigned>(slot + 1));
}

[[nodiscard]] int roster_ping(std::uint8_t slot) {
    if (!g_roster.has_value()) {
        return 0;
    }
    for (std::size_t index = 0;
         index < g_roster->count
             && index < fruityprime::net::RosterPacket::MaxSlots;
         ++index) {
        if (g_roster->slots[index] == slot) {
            return static_cast<int>(g_roster->pings[index]);
        }
    }
    return 0;
}

void snapshot_net_log() {
    if (!g_net_log.enabled() || g_net_client == nullptr
        || !g_net_client->connected()) {
        return;
    }
    fruityprime::net::NetLog::SnapshotContext context;
    context.game_state = &g_game_state;
    context.session = g_session.has_value() ? &*g_session : nullptr;
    context.server_match = g_server_match_state.has_value()
        ? &*g_server_match_state : nullptr;
    context.role = g_is_authority ? "authority" : "client";
    context.authority = g_is_authority;
    context.local_slot = g_local_slot;
    context.main_player = g_local_slot;
    if (g_roster.has_value()) {
        for (std::size_t index = 0;
             index < g_roster->count
                 && index < fruityprime::net::RosterPacket::MaxSlots;
             ++index) {
            const std::uint8_t slot = g_roster->slots[index];
            if (slot >= fruityprime::net::NetConfig::SlotCapacity) {
                continue;
            }
            context.occupied[slot] = true;
            context.pings[slot] = g_roster->pings[index];
        }
    }
    g_net_log.snapshot(static_cast<double>(g_net_frame) / 60.0, context);
}

void report_net_diagnostics() {
    if (g_net_client == nullptr || !g_net_client->connected()) {
        return;
    }
    fruityprime::net::NetDiagnostics::Context context;
    context.game_state = &g_game_state;
    context.session = g_session.has_value() ? &*g_session : nullptr;
    context.server_match = g_server_match_state.has_value()
        ? &*g_server_match_state : nullptr;
    context.role = g_is_authority ? "authority" : "client";
    context.active = true;
    context.local_slot = g_local_slot;
    if (g_roster.has_value()) {
        for (std::size_t index = 0;
             index < g_roster->count
                 && index < fruityprime::net::RosterPacket::MaxSlots;
             ++index) {
            const std::uint8_t slot = g_roster->slots[index];
            if (slot < fruityprime::net::NetConfig::SlotCapacity) {
                context.occupied[slot] = true;
            }
        }
    }
    for (std::size_t index = 0;
         index < fruityprime::net::NetConfig::SlotCapacity; ++index) {
        const auto slot = static_cast<std::uint8_t>(index);
        context.state_valid[index] = g_net_player_bridge.state_valid(slot);
        context.intent_valid[index] = g_net_player_bridge.intent_valid(slot);
    }
    g_net_diagnostics.report(static_cast<double>(g_net_frame) / 60.0,
                             context);
}

void record_and_repair_network_players(std::uint32_t frame) {
    if (!g_session.has_value() || g_net_client == nullptr
        || !g_net_client->connected()) {
        return;
    }
    auto& aim = g_net_player_bridge.aim();
    for (std::size_t index = 0;
         index < fruityprime::net::NetConfig::SlotCapacity; ++index) {
        const auto slot = static_cast<std::uint8_t>(index);
        if (!g_session->has_player(slot)) {
            continue;
        }
        const auto& player = g_session->player(slot);
        if ((player.flags & fruityprime::net::PlayerState::FlagActive) == 0) {
            continue;
        }
        aim.record_position(*g_session, slot, frame);
        aim.repair_vectors(*g_session, slot);
    }
}

[[nodiscard]] bool chat_single_player() noexcept {
    return g_game_state.single_player;
}

[[nodiscard]] bool chat_net_active() noexcept {
    return g_net_client != nullptr && g_net_client->connected();
}

[[nodiscard]] std::string chat_player_name() {
    return g_local_name;
}

void send_chat_packet(std::string_view text) {
    if (!chat_net_active()) {
        return;
    }
    fruityprime::net::ChatPacket packet;
    packet.slot = g_local_slot;
    packet.kind = fruityprime::net::ChatPacket::KindSay;
    packet.name = g_local_name;
    packet.text = std::string(text);
    const auto encoded = packet.encode();
    g_net_client->send(fruityprime::net::PacketType::Chat, encoded);
}

[[nodiscard]] int chat_key_from_windows(WPARAM key, LPARAM key_data) {
    if (key >= 32 && key <= 126) {
        return static_cast<int>(key);
    }
    if (key >= VK_F1 && key <= VK_F24) {
        return 290 + static_cast<int>(key - VK_F1);
    }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        return 320 + static_cast<int>(key - VK_NUMPAD0);
    }
    switch (key) {
    case VK_ESCAPE: return 256;
    case VK_RETURN: return (key_data & (1LL << 24)) != 0 ? 335 : 257;
    case VK_TAB: return 258;
    case VK_BACK: return 259;
    case VK_INSERT: return 260;
    case VK_DELETE: return 261;
    case VK_RIGHT: return 262;
    case VK_LEFT: return 263;
    case VK_DOWN: return 264;
    case VK_UP: return 265;
    case VK_PRIOR: return 266;
    case VK_NEXT: return 267;
    case VK_HOME: return 268;
    case VK_END: return 269;
    case VK_CAPITAL: return 280;
    case VK_SCROLL: return 281;
    case VK_NUMLOCK: return 282;
    case VK_SNAPSHOT: return 283;
    case VK_PAUSE: return 284;
    case VK_DECIMAL: return 330;
    case VK_DIVIDE: return 331;
    case VK_MULTIPLY: return 332;
    case VK_SUBTRACT: return 333;
    case VK_ADD: return 334;
    case VK_LSHIFT: return 340;
    case VK_LCONTROL: return 341;
    case VK_LMENU: return 342;
    case VK_LWIN: return 343;
    case VK_RSHIFT: return 344;
    case VK_RCONTROL: return 345;
    case VK_RMENU: return 346;
    case VK_RWIN: return 347;
    case VK_APPS: return 348;
    case VK_OEM_1: return 59;
    case VK_OEM_PLUS: return 61;
    case VK_OEM_COMMA: return 44;
    case VK_OEM_MINUS: return 45;
    case VK_OEM_PERIOD: return 46;
    case VK_OEM_2: return 47;
    case VK_OEM_3: return 96;
    case VK_OEM_4: return 91;
    case VK_OEM_5: return 92;
    case VK_OEM_6: return 93;
    case VK_OEM_7: return 39;
    case VK_SHIFT: {
        const UINT scan = static_cast<UINT>((key_data >> 16) & 0xFF);
        const UINT side = MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX);
        return side == VK_RSHIFT ? 344 : 340;
    }
    case VK_CONTROL: return (key_data & (1LL << 24)) != 0 ? 345 : 341;
    case VK_MENU: return (key_data & (1LL << 24)) != 0 ? 346 : 342;
    default: return 0;
    }
}

[[nodiscard]] std::uint32_t demo_frame() noexcept {
    return g_demo_frame == 0 ? 0 : g_demo_frame - 1;
}

void record_demo_bytes(std::span<const std::uint8_t> data) {
    g_demo_recorder.record_bytes(demo_frame(), data);
}

void record_demo_packet(fruityprime::net::PacketType type,
                        std::span<const std::uint8_t> payload) {
    g_demo_recorder.record_packet(demo_frame(), type, payload);
}

[[nodiscard]] std::string match_time_text(float seconds) {
    const int rounded = std::max(0, static_cast<int>(std::ceil(seconds)));
    const int minutes = rounded / 60;
    const int remaining = rounded % 60;
    std::ostringstream text;
    text << "TIME " << std::setfill('0') << std::setw(2) << minutes
         << ':' << std::setw(2) << remaining;
    return text.str();
}

[[nodiscard]] std::string objective_hud_text(
    std::uint8_t mode, const fruityprime::net::PlayerState& player) {
    if (!g_session.has_value()) {
        return {};
    }
    const auto& objectives = g_session->objective_state();
    if (mode == 10 || mode == 11) {
        std::size_t owned = 0;
        for (const auto& node : objectives.nodes) {
            if (node.current_team == player.team) {
                ++owned;
            }
        }
        return "NODES " + std::to_string(owned) + "/"
            + std::to_string(objectives.nodes.size());
    }
    if (mode == 12 || mode == 13) {
        const std::size_t team = std::min<std::size_t>(
            player.team, objectives.team_time.size() - 1);
        return "HOLD " + match_time_text(objectives.team_time[team]);
    }
    if (mode == 14) {
        const auto hunter = objectives.prime_hunter;
        if (hunter < 0 || static_cast<std::size_t>(hunter)
                >= objectives.player_time.size()) {
            return "HUNTER WAIT";
        }
        return "HUNTER " + std::to_string(static_cast<unsigned>(hunter + 1))
            + " " + match_time_text(
                objectives.player_time[static_cast<std::size_t>(hunter)]);
    }
    if (mode == 7 || mode == 8 || mode == 9) {
        const bool carrying = std::any_of(
            objectives.flags.begin(), objectives.flags.end(),
            [&player](const fruityprime::gameplay::FlagObjectiveState& flag) {
                return flag.carrier_slot == player.slot_index;
            });
        return mode == 7
            ? (carrying ? "FLAG CARRY" : "FLAG BASE")
            : (carrying ? "BOUNTY CARRY" : "BOUNTY");
    }
    if (mode == 5 || mode == 6) {
        const auto goal = g_match_flow.has_value()
            ? g_match_flow->state().point_goal : 0;
        const int lives = std::max(
            0, static_cast<int>(goal) - static_cast<int>(player.deaths));
        return "LIVES " + std::to_string(lives);
    }
    return {};
}

void draw_hud_text_right(std::string_view text, float right, float top,
                         float scale) {
    const float width = static_cast<float>(text.size()) * 6.0F * scale;
    draw_hud_text(text, right - width, top, scale);
}

void draw_pro_hud(
    const fruityprime::mods::render::pro_hud::Frame& frame,
    const MphReadNative::Hud::Rect& area, const float scale) {
    if (scale <= 0.0F) {
        return;
    }
    using fruityprime::mods::render::pro_hud::Color;
    using fruityprime::mods::render::pro_hud::tone_color;

    const auto set_color = [](const Color color) {
        glColor3f(color.red, color.green, color.blue);
    };
    const auto draw_number = [&](const std::string& text, const float x,
                                 const float y, const float text_scale,
                                 const bool right, const Color color) {
        glColor3f(0.0F, 0.0F, 0.0F);
        if (right) {
            draw_hud_text_right(text, x + 0.8F * scale,
                                y + 0.8F * scale, text_scale);
        } else {
            draw_hud_text(text, x + 0.8F * scale,
                          y + 0.8F * scale, text_scale);
        }
        set_color(color);
        if (right) {
            draw_hud_text_right(text, x, y, text_scale);
        } else {
            draw_hud_text(text, x, y, text_scale);
        }
    };
    const auto draw_bar = [&](const float x, const float y,
                              const float width, const float fraction,
                              const Color color) {
        glColor4f(0.0F, 0.0F, 0.0F, 0.55F);
        draw_hud_box(x - scale, y - scale,
                     x + (width + 1.0F) * scale,
                     y + 4.0F * scale);
        glColor4f(1.0F, 1.0F, 1.0F, 0.16F);
        draw_hud_box(x, y, x + width * scale, y + 3.0F * scale);
        if (fraction > 0.0F) {
            glColor4f(color.red, color.green, color.blue, 1.0F);
            draw_hud_box(x, y, x + width * scale * fraction,
                         y + 3.0F * scale);
        }
    };

    const float health_left = area.left + 2.0F * scale;
    const float health_top = area.top + 170.0F * scale;
    const float health_right = area.left + 46.0F * scale;
    glColor4f(0.0F, 0.0F, 0.0F, 0.50F);
    draw_hud_box(health_left, health_top, health_right,
                 area.top + 190.0F * scale);
    const Color health_color = tone_color(frame.health_tone);
    draw_number(frame.health_text, area.left + 6.0F * scale,
                area.top + 172.0F * scale, 1.5F * scale, false,
                health_color);
    draw_bar(area.left + 4.0F * scale, area.top + 186.0F * scale,
             40.0F, frame.health_fraction, health_color);

    if (frame.ammo_visible) {
        const float ammo_right = area.right - 2.0F * scale;
        const float ammo_left = ammo_right - 58.0F * scale;
        const float ammo_top = area.top + 170.0F * scale;
        glColor4f(0.0F, 0.0F, 0.0F, 0.50F);
        draw_hud_box(ammo_left, ammo_top, ammo_right,
                     area.top + 190.0F * scale);

        if (g_hud_weapon_icon.has_value()) {
            const HudImage* icon = g_hud_weapon_icon->frame(frame.weapon, 0);
            if (icon != nullptr && icon->width > 0 && icon->height > 0) {
                const float side = 12.0F * scale;
                const float icon_scale = side / static_cast<float>(
                    std::max(icon->width, icon->height));
                const float icon_width = static_cast<float>(icon->width)
                    * icon_scale;
                const float icon_height = static_cast<float>(icon->height)
                    * icon_scale;
                const float icon_x = ammo_left + 2.0F * scale
                    + (side - icon_width) * 0.5F;
                const float icon_y = ammo_top + 2.0F * scale
                    + (side - icon_height) * 0.5F;
                static_cast<void>(draw_hud_object_image(
                    *g_hud_weapon_icon, frame.weapon, 0, icon_x, icon_y,
                    icon_scale));
                glDisable(GL_TEXTURE_2D);
            }
        }
        const Color ammo_color = tone_color(frame.ammo_tone);
        draw_number(frame.ammo_text,
                    ammo_right - 4.0F * scale,
                    area.top + 172.0F * scale, 1.5F * scale, true,
                    ammo_color);
        draw_bar(ammo_left + 2.0F * scale,
                 area.top + 186.0F * scale, 54.0F,
                 frame.ammo_fraction, ammo_color);
    }

    const Color ink{235.0F / 255.0F, 238.0F / 255.0F, 245.0F / 255.0F};
    const Color dim{178.0F / 255.0F, 186.0F / 255.0F, 200.0F / 255.0F};
    draw_number(frame.score_label, area.left + 4.0F * scale,
                area.top + frame.score_y * scale, 0.55F * scale, false,
                dim);
    draw_number(frame.score_text, area.left + 4.0F * scale,
                area.top + (frame.score_y + 8.0F) * scale, 1.1F * scale,
                false, ink);
}

void draw_scan_overlay(const MphReadNative::Hud::Rect& area, float hud_scale) {
    if (hud_scale <= 0.0F || g_scan_target.identity == nullptr) {
        return;
    }
    const float design_width = area.right - area.left;
    const float design_height = area.bottom - area.top;
    const float center_x = area.left + g_scan_box_x * design_width;
    const float center_y = area.top + g_scan_box_y * design_height;
    const float design_size = std::clamp(
        g_scan_target.scale * 90.0F, 4.0F, 14.0F)
        * g_scan_box_size_factor;
    const float half_width = design_size * hud_scale * 0.5F;
    const float half_height = design_size * hud_scale * 0.5F;
    const float arm = std::min(8.0F * hud_scale,
                               std::max(2.0F * hud_scale,
                                        design_size * hud_scale * 0.45F));
    const float left = center_x - half_width;
    const float right = center_x + half_width;
    const float top = center_y - half_height;
    const float bottom = center_y + half_height;
    const bool in_range = g_scan_target.distance < 12.0F;
    const bool dim = g_scan_target.dim;
    glColor4f(in_range ? 0.30F : 1.0F,
              in_range ? 1.0F : 0.28F,
              in_range ? 0.52F : 0.18F, 0.92F);
    glBegin(GL_LINES);
    // Four animated corner brackets, matching the DS scan box's 16-pixel
    // design grid while allowing the existing viewport to scale uniformly.
    glVertex2f(left, top + arm);
    glVertex2f(left, top);
    glVertex2f(left, top);
    glVertex2f(left + arm, top);
    glVertex2f(right - arm, top);
    glVertex2f(right, top);
    glVertex2f(right, top);
    glVertex2f(right, top + arm);
    glVertex2f(left, bottom - arm);
    glVertex2f(left, bottom);
    glVertex2f(left, bottom);
    glVertex2f(left + arm, bottom);
    glVertex2f(right - arm, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, bottom - arm);
    glEnd();

    const float bar_width = 48.0F * hud_scale;
    const float bar_height = 3.0F * hud_scale;
    const float bar_left = center_x - bar_width * 0.5F;
    const float bar_top = bottom + 7.0F * hud_scale;
    if (!dim && g_scan_progress > 0.0F) {
        const float duration = g_scan_target.scan_id < g_scan_times.size()
            && g_scan_times[g_scan_target.scan_id] > 0.0F
            ? g_scan_times[g_scan_target.scan_id] : 2.0F;
        const float fraction = std::clamp(g_scan_progress / duration,
                                          0.0F, 1.0F);
        glColor4f(0.04F, 0.12F, 0.08F, 0.86F);
        draw_hud_box(bar_left, bar_top, bar_left + bar_width,
                     bar_top + bar_height);
        glColor4f(0.30F, 1.0F, 0.52F, 0.95F);
        draw_hud_box(bar_left, bar_top,
                     bar_left + bar_width * fraction,
                     bar_top + bar_height);
        glColor3f(0.72F, 1.0F, 0.80F);
        draw_hud_text(g_scan_complete ? "SCAN COMPLETE" : "SCANNING",
                      bar_left, bar_top + 6.0F * hud_scale,
                      std::max(1.0F, hud_scale * 0.75F));
    } else if (dim) {
        glColor3f(0.58F, 0.86F, 1.0F);
        draw_hud_text("SCANNED", bar_left, bar_top,
                      std::max(1.0F, hud_scale * 0.75F));
    } else if (!in_range) {
        glColor3f(1.0F, 0.42F, 0.30F);
        draw_hud_text("OUT OF RANGE", bar_left, bar_top,
                      std::max(1.0F, hud_scale * 0.75F));
    }
}

void draw_scan_dialog(const MphReadNative::Hud::Rect& area, float hud_scale) {
    if (hud_scale <= 0.0F || !g_scan_dialog_active
        || g_scan_dialog_scan_id >= g_scan_texts.size()) {
        return;
    }
    const ScanText& text = g_scan_texts[g_scan_dialog_scan_id];
    if (text.value1.empty() && text.value2.empty()) {
        return;
    }
    const float width = std::min(224.0F * hud_scale,
                                 area.right - area.left - 16.0F * hud_scale);
    const float left = (area.left + area.right - width) * 0.5F;
    const float top = area.top + 116.0F * hud_scale;
    const float line_scale = std::max(1.0F, hud_scale * 0.72F);
    glColor4f(0.015F, 0.055F, 0.075F, 0.93F);
    draw_hud_box(left, top, left + width, top + 58.0F * hud_scale);
    glColor3f(0.36F, 1.0F, 0.70F);
    draw_hud_text("SCAN COMPLETE", left + 8.0F * hud_scale,
                  top + 5.0F * hud_scale, line_scale);
    glColor3f(0.80F, 0.96F, 1.0F);
    const auto draw_line = [&](std::string_view value, float y) {
        constexpr std::size_t MaxCharacters = 34;
        draw_hud_text(value.substr(0, std::min(value.size(), MaxCharacters)),
                      left + 8.0F * hud_scale, y, line_scale);
    };
    draw_line(text.value1, top + 17.0F * hud_scale);
    draw_line(text.value2, top + 29.0F * hud_scale);
    glColor3f(0.64F, 0.76F, 0.86F);
    draw_hud_text("ENTER / A  OK    ESC  CANCEL",
                  left + 8.0F * hud_scale, top + 45.0F * hud_scale,
                  std::max(1.0F, hud_scale * 0.58F));
}

void update_fps_counter() {
    const auto now = std::chrono::steady_clock::now();
    if (g_fps_window_started.time_since_epoch().count() == 0) {
        g_fps_window_started = now;
    }
    ++g_fps_window_frames;
    const double elapsed = std::chrono::duration<double>(
        now - g_fps_window_started).count();
    if (elapsed < 0.5) {
        return;
    }
    g_display_fps = static_cast<float>(
        static_cast<double>(g_fps_window_frames) / elapsed);
    g_fps_window_frames = 0;
    g_fps_window_started = now;
}

void draw_model_layer(
    DrawLayer layer,
    std::uint8_t hunter,
    const fruityprime::model::ModelInstance* instance = nullptr,
    const EntityRenderModel* entity_model = nullptr,
    std::int32_t node_id = -1,
    const std::array<float, 4>* tint = nullptr) {
    float model_scale = 1.0F;
    bool found = false;
    for (const auto& primitive : g_draw_primitives) {
        if (primitive.layer == layer && primitive.hunter == hunter
            && primitive.entity_model == entity_model
            && (node_id < 0 || primitive.node_id == node_id)) {
            model_scale = primitive.model_scale;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    glPushMatrix();
    glScalef(model_scale, model_scale, model_scale);
    for (const auto& primitive : g_draw_primitives) {
        if (primitive.layer != layer || primitive.hunter != hunter
            || primitive.entity_model != entity_model
            || (node_id >= 0 && primitive.node_id != node_id)) {
            continue;
        }
        // GetDrawItems walks the node tree and skips a node the animation has
        // switched off.  append_model_geometry only knows the model file's own
        // flag, which never changes; a model whose parts are chosen at
        // runtime -- the arm cannon, whose barrels belong to the equipped
        // weapon -- needs the instance's live state instead.
        if (instance != nullptr && primitive.owner == &instance->model()
            && primitive.node_id >= 0
            && static_cast<std::size_t>(primitive.node_id)
                   < instance->node_states().size()
            && !instance->node_states()[
                   static_cast<std::size_t>(primitive.node_id)].enabled) {
            continue;
        }
        std::size_t texture_index = primitive.texture_index;
        if (instance != nullptr && primitive.owner == &instance->model()
            && primitive.material_id < instance->material_states().size()) {
            const auto& material = instance->material_states()[
                primitive.material_id];
            texture_index = cache_texture_ids(
                instance->model(), material.current_texture_id,
                material.current_palette_id);
        }
        if (texture_index != NoTexture
            && texture_index < g_texture_images.size()) {
            auto& image = g_texture_images[texture_index];
            upload_gl_texture(image);
            glBindTexture(GL_TEXTURE_2D, image.gl_id);
        } else {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        draw_primitive(primitive, instance, tint);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}

struct EffectParticleBinding {
    const EntityRenderModel* render_model = nullptr;
    std::int32_t node_id = -1;
};

[[nodiscard]] std::optional<EffectParticleBinding> find_particle_binding(
    std::string_view model_name, std::string_view particle_name) {
    const auto model_it = g_effect_models.find(std::string(model_name));
    if (model_it == g_effect_models.end() || model_it->second == nullptr
        || model_it->second->model == nullptr) {
        return std::nullopt;
    }
    const auto& model = *model_it->second->model;
    auto node = std::find_if(
        model.nodes().begin(), model.nodes().end(),
        [particle_name](const auto& candidate) {
            return candidate.name == particle_name;
        });
    if (node == model.nodes().end() && model_name == "geo1"
        && particle_name == "gib") {
        // The managed loader intentionally maps this legacy alias to the
        // first visible gib mesh in geo1.
        node = std::find_if(
            model.nodes().begin(), model.nodes().end(),
            [](const auto& candidate) { return candidate.name == "gib3"; });
    }
    if (node == model.nodes().end() || node->mesh_count == 0) {
        return std::nullopt;
    }
    return EffectParticleBinding{
        model_it->second,
        static_cast<std::int32_t>(node - model.nodes().begin())};
}

[[nodiscard]] std::optional<std::pair<std::string_view, std::string_view>>
single_particle_asset(
    fruityprime::gameplay::SingleParticleType type) noexcept {
    using Type = fruityprime::gameplay::SingleParticleType;
    switch (type) {
    case Type::Death: return std::pair{"deathParticle", "death"};
    case Type::Fuzzball: return std::pair{"particles", "fuzzBall"};
    case Type::Lore: return std::pair{"icons", "lore"};
    case Type::LoreDim: return std::pair{"icons", "lore_dim"};
    case Type::Enemy: return std::pair{"icons", "enemy"};
    case Type::EnemyDim: return std::pair{"icons", "enemy_dim"};
    case Type::Object: return std::pair{"icons", "object"};
    case Type::ObjectDim: return std::pair{"icons", "object_dim"};
    case Type::Equipment: return std::pair{"icons", "equipment"};
    case Type::EquipmentDim: return std::pair{"icons", "equipment_dim"};
    case Type::Red: return std::pair{"icons", "red"};
    case Type::RedDim: return std::pair{"icons", "red_dim"};
    }
    return std::nullopt;
}

[[nodiscard]] int scan_category_for(std::uint16_t scan_id,
                                    int fallback) noexcept {
    if (scan_id < g_scan_categories.size()
        && g_scan_categories[scan_id] != '\0') {
        return fruityprime::strings::scan_category(
            g_scan_categories[scan_id]);
    }
    return fallback;
}

[[nodiscard]] bool scan_is_dim(std::uint16_t scan_id) noexcept {
    return g_story_save.has_value()
        && g_story_save->logbook_found(static_cast<std::int32_t>(scan_id));
}

[[nodiscard]] std::uint16_t static_entity_scan_id(
    const fruityprime::entities::static_entities::Entity& entity,
    int& fallback_category) noexcept {
    using namespace fruityprime::entities::static_entities;
    fallback_category = 2; // room objects and machinery use the O icon.
    if (const auto* object = dynamic_cast<const ObjectEntity*>(&entity);
        object != nullptr) {
        return object->data().scan_id;
    }
    if (const auto* platform = dynamic_cast<const PlatformEntity*>(&entity);
        platform != nullptr) {
        // PlatformEntity's moving flag is the native equivalent of the
        // managed Awake state used to choose ScanData1/ScanData2.
        return platform->moving() ? platform->data().scan_data1
                                  : platform->data().scan_data2;
    }
    if (const auto* door = dynamic_cast<const DoorEntity*>(&entity);
        door != nullptr) {
        static constexpr std::array<std::uint16_t, 10> locked_ids{
            0, 255, 264, 252, 256, 253, 254, 249, 266, 265};
        if (door->data().door_type == 2) { // DoorType.Boss
            return 269;
        }
        if (door->locked()) {
            const std::size_t palette = std::min<std::size_t>(
                door->data().palette_id, locked_ids.size() - 1);
            return locked_ids[palette];
        }
        return 251;
    }
    if (const auto* artifact = dynamic_cast<const ArtifactEntity*>(&entity);
        artifact != nullptr) {
        return artifact->scan_id();
    }
    if (const auto* field = dynamic_cast<const ForceFieldEntity*>(&entity);
        field != nullptr) {
        return field->scan_id();
    }
    if (const auto* teleporter = dynamic_cast<const TeleporterEntity*>(&entity);
        teleporter != nullptr) {
        if (teleporter->data().invisible) {
            return 0;
        }
        fallback_category = 0;
        const bool big = teleporter->data().artifact_id < 8;
        return teleporter->active() ? (big ? 46 : 26) : (big ? 38 : 25);
    }
    return 0;
}

struct ProjectedScanPosition {
    float screen_x = 0.5F;
    float screen_y = 0.5F;
    float eye_depth = 0.0F;
};

[[nodiscard]] bool projected_scan_position(
    const fruityprime::net::Vec3& position,
    const GLdouble modelview[16], const GLdouble projection[16],
    ProjectedScanPosition& result) noexcept {
    const GLdouble world[4]{position.x, position.y, position.z, 1.0};
    GLdouble eye[4]{};
    GLdouble clip[4]{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            eye[row] += modelview[column * 4 + row] * world[column];
        }
    }
    // OpenGL's camera looks down -Z. Keep the same front/depth gate as the
    // managed Matrix.GetProjectedValues path before emitting an icon.
    if (!std::isfinite(eye[2]) || eye[2] >= -0.1) {
        return false;
    }
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            clip[row] += projection[column * 4 + row] * eye[column];
        }
    }
    if (!std::isfinite(clip[3]) || clip[3] <= 0.0) {
        return false;
    }
    const double ndc_x = clip[0] / clip[3];
    const double ndc_y = clip[1] / clip[3];
    // PlayerScan accepts a 16-pixel margin around the 256x192 design area.
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)
        || ndc_x <= -1.125 || ndc_x >= 1.125
        || ndc_y <= -1.1666666667 || ndc_y >= 1.1666666667) {
        return false;
    }
    result.screen_x = static_cast<float>((ndc_x + 1.0) * 0.5);
    result.screen_y = static_cast<float>((1.0 - ndc_y) * 0.5);
    result.eye_depth = static_cast<float>(-eye[2]);
    return std::isfinite(result.screen_x) && std::isfinite(result.screen_y)
        && std::isfinite(result.eye_depth);
}

void queue_scan_particle(std::uint16_t scan_id,
                         fruityprime::net::Vec3 position, int fallback_category,
                         const fruityprime::net::Vec3& camera_position,
                         const GLdouble modelview[16],
                         const GLdouble projection[16],
                         std::size_t& queued, const void* identity) {
    if (scan_id == 0 || queued >= 32) {
        return;
    }
    ProjectedScanPosition projected;
    if (!projected_scan_position(position, modelview, projection, projected)) {
        return;
    }
    const auto delta = fruityprime::net::Vec3{
        position.x - camera_position.x, position.y - camera_position.y,
        position.z - camera_position.z};
    const float distance_squared = delta.x * delta.x + delta.y * delta.y
        + delta.z * delta.z;
    if (!std::isfinite(distance_squared) || distance_squared >= 24.0F * 24.0F) {
        return;
    }
    const int category = scan_category_for(scan_id, fallback_category);
    if (category < 0 || category >= 5) {
        return;
    }
    const bool dim = scan_is_dim(scan_id);
    const float distance = std::sqrt(std::max(distance_squared, 0.0F));
    const float scale = 1.0F / std::max(projected.eye_depth, 0.01F);
    const int pixel_x = static_cast<int>(
        std::floor(projected.screen_x * 256.0F));
    const int pixel_y = static_cast<int>(
        std::floor(projected.screen_y * 192.0F));
    if (pixel_x > 58 && pixel_x < 198
        && pixel_y > 64 && pixel_y < 128) {
        const float ndc_x = projected.screen_x * 2.0F - 1.0F;
        const float ndc_y = 1.0F - projected.screen_y * 2.0F;
        const float center_metric = 4.0F * (ndc_x * ndc_x + ndc_y * ndc_y)
            + distance / 32.0F;
        if (center_metric < g_scan_target.center_metric) {
            g_scan_target.identity = identity;
            g_scan_target.scan_id = scan_id;
            g_scan_target.category = category;
            g_scan_target.position = position;
            g_scan_target.distance = distance;
            g_scan_target.scale = scale;
            g_scan_target.screen_x = projected.screen_x;
            g_scan_target.screen_y = projected.screen_y;
            g_scan_target.center_metric = center_metric;
            g_scan_target.dim = dim;
        }
    }
    const auto type = static_cast<fruityprime::gameplay::SingleParticleType>(
        static_cast<unsigned>(category * 2 + (dim ? 1 : 0)));
    g_session->add_single_particle(type, position, {1.0F, 1.0F, 1.0F},
                                   dim ? 24.0F / 31.0F : 1.0F, 0.625F);
    ++queued;
}

void queue_scan_particles() {
    g_scan_target = {};
    if (!g_session.has_value()
        || (static_cast<std::uint32_t>(g_input.buttons())
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::ScanVisor)) == 0
        || !g_session->has_player(g_local_slot)) {
        if (!g_scan_dialog_active) {
            g_previous_scan_target = nullptr;
            g_scan_progress = 0.0F;
            g_scan_complete = false;
            g_scan_button_was_held = false;
        }
        return;
    }
    // ScanLog confirmation pauses the managed scene. Keep the completed entry
    // on screen until the local dialog action accepts or cancels it instead
    // of immediately selecting a different room entity.
    if (g_scan_dialog_active) {
        return;
    }
    const bool scan_button = g_input.scan();
    GLdouble modelview[16]{};
    GLdouble projection[16]{};
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    const auto& local = g_session->player(g_local_slot);
    const fruityprime::net::Vec3 camera_position{
        local.position.x, local.position.y + 0.5F, local.position.z};
    std::size_t queued = 0;

    static constexpr std::array<std::uint16_t, 23> item_scan_ids{
        9, 11, 12, 0, 10, 21, 13, 23, 22, 18, 20, 19, 28, 14, 15, 16,
        17, 0, 24, 463, 0, 0, 0};
    for (const auto& item : g_session->items()) {
        const auto raw_type = static_cast<std::int32_t>(item.type);
        if (raw_type < 0
            || static_cast<std::size_t>(raw_type) >= item_scan_ids.size()) {
            continue;
        }
        queue_scan_particle(item_scan_ids[static_cast<std::size_t>(raw_type)],
                            item.position, 3, camera_position, modelview,
                            projection, queued, &item);
    }
    for (const auto& enemy : g_session->enemies()) {
        if (!enemy.active || !enemy.visible || enemy.health == 0) {
            continue;
        }
        const std::uint16_t scan_id = enemy.enemy_type
            < fruityprime::metadata::EnemyCount
            ? fruityprime::metadata::enemy_info(enemy.enemy_type).scan_id : 0;
        queue_scan_particle(scan_id, enemy.position, 1, camera_position,
                            modelview, projection, queued, &enemy);
    }
    if (g_static_world != nullptr) {
        for (const auto& entity : g_static_world->entities()) {
            if (entity == nullptr || !entity->active() || entity->hidden()) {
                continue;
            }
            int fallback_category = 2;
            const std::uint16_t scan_id = static_entity_scan_id(
                *entity, fallback_category);
            const auto& position = entity->position();
            queue_scan_particle(scan_id,
                                {position.x, position.y, position.z},
                                fallback_category, camera_position, modelview,
                                projection, queued, entity.get());
            if (queued >= 32) {
                break;
            }
        }
    }

    const void* target_identity = g_scan_target.identity;
    if (target_identity != nullptr && g_room.has_value()) {
        const fruityprime::collision::Vec3 scan_start{
            camera_position.x, camera_position.y, camera_position.z};
        const fruityprime::collision::Vec3 scan_end{
            g_scan_target.position.x, g_scan_target.position.y,
            g_scan_target.position.z};
        if (fruityprime::collision::sweep_sphere(
                g_room->collision(), scan_start, scan_end, 0.0F)
                .has_value()) {
            g_scan_target = {};
            target_identity = nullptr;
        }
    }
    if (target_identity != g_previous_scan_target) {
        g_scan_progress = 0.0F;
        g_scan_complete = false;
        g_scan_box_size_factor = 3.0F;
        g_scan_box_corner_factor = 0.125F;
        if (target_identity == nullptr) {
            g_scan_box_x = 0.5F;
            g_scan_box_y = 0.5F;
        }
    }
    if (target_identity == nullptr) {
        g_previous_scan_target = nullptr;
        g_scan_box_corner_factor = std::max(
            0.125F, g_scan_box_corner_factor - 0.03125F);
        g_scan_box_size_factor = std::min(
            4.0F, g_scan_box_size_factor + 0.5F);
        g_scan_progress = 0.0F;
        g_scan_complete = false;
        g_scan_button_was_held = false;
        return;
    }
    g_previous_scan_target = target_identity;
    g_scan_box_corner_factor = std::min(
        1.0F, g_scan_box_corner_factor + 0.03125F);
    g_scan_box_size_factor = std::max(
        1.0F, g_scan_box_size_factor - 0.125F);
    g_scan_box_x += (g_scan_target.screen_x - g_scan_box_x)
        * g_scan_box_corner_factor;
    g_scan_box_y += (g_scan_target.screen_y - g_scan_box_y)
        * g_scan_box_corner_factor;
    if (scan_button && !g_scan_target.dim && g_scan_target.distance < 12.0F) {
        const float duration = g_scan_target.scan_id < g_scan_times.size()
            && g_scan_times[g_scan_target.scan_id] > 0.0F
            ? g_scan_times[g_scan_target.scan_id] : 2.0F;
        g_scan_progress = std::min(duration,
                                   g_scan_progress + 1.0F / 60.0F);
        if (!g_scan_complete && g_scan_progress >= duration) {
            g_scan_complete = true;
            g_scan_dialog_active = true;
            g_scan_dialog_scan_id = g_scan_target.scan_id;
            g_scan_dialog_confirm_was_down = false;
            g_paused = true;
        }
    }
    g_scan_button_was_held = scan_button;
}

void draw_single_particle(
    const fruityprime::gameplay::SingleParticleState& particle) {
    const auto asset = single_particle_asset(particle.type);
    if (!asset.has_value() || !std::isfinite(particle.alpha)
        || particle.alpha <= 0.0F || !std::isfinite(particle.scale)
        || particle.scale <= 0.0F) {
        return;
    }
    const auto binding = find_particle_binding(asset->first, asset->second);
    if (!binding.has_value() || binding->render_model->model == nullptr) {
        return;
    }
    const auto& model = *binding->render_model->model;
    const auto node_index = static_cast<std::size_t>(binding->node_id);
    if (node_index >= model.nodes().size()) {
        return;
    }
    const auto& node = model.nodes()[node_index];
    const std::size_t mesh_index = node.mesh_id / 2;
    if (mesh_index >= model.meshes().size()) {
        return;
    }
    const auto& mesh = model.meshes()[mesh_index];
    if (mesh.material_id >= model.materials().size()) {
        return;
    }

    std::size_t texture = NoTexture;
    float texture_scale_s = 1.0F;
    float texture_scale_t = 1.0F;
    const auto& material = model.materials()[mesh.material_id];
    try {
        texture = cache_texture(model, material);
    } catch (const std::exception&) {
        // Keep the same optional-resource behavior as the C# path: an
        // extracted tree without a companion texture can still show the
        // particle as a colored billboard.
    }
    if (material.x_repeat == 2) {
        texture_scale_s = material.scale_s.to_float();
    }
    if (material.y_repeat == 2) {
        texture_scale_t = material.scale_t.to_float();
    }
    const bool textured = texture != NoTexture
        && texture < g_texture_images.size();

    GLfloat view[16]{};
    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    fruityprime::formats::Vector3 right{view[0], view[1], view[2]};
    fruityprime::formats::Vector3 up{view[4], view[5], view[6]};
    if (right.length_squared() < 0.0001F
        || up.length_squared() < 0.0001F) {
        right = {1.0F, 0.0F, 0.0F};
        up = {0.0F, 1.0F, 0.0F};
    } else {
        right = right.normalized();
        up = up.normalized();
    }

    const auto center = fruityprime::formats::Vector3{
        particle.position.x, particle.position.y, particle.position.z};
    const float scale = std::abs(particle.scale);
    const auto first = center - right * scale + up * scale;
    const auto second = center + right * scale + up * scale;
    const auto third = center + right * scale - up * scale;
    const auto fourth = center - right * scale - up * scale;

    glDisable(GL_LIGHTING);
    if (textured) {
        auto& image = g_texture_images[texture];
        upload_gl_texture(image);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, image.gl_id);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    glColor4f(std::clamp(particle.color.x, 0.0F, 1.0F),
              std::clamp(particle.color.y, 0.0F, 1.0F),
              std::clamp(particle.color.z, 0.0F, 1.0F),
              std::clamp(particle.alpha, 0.0F, 1.0F));
    glBegin(GL_QUADS);
    if (textured) glTexCoord2f(0.0F, 0.0F);
    glVertex3f(first.x, first.y, first.z);
    if (textured) glTexCoord2f(1.0F * texture_scale_s, 0.0F);
    glVertex3f(second.x, second.y, second.z);
    if (textured) glTexCoord2f(1.0F * texture_scale_s, 1.0F * texture_scale_t);
    glVertex3f(third.x, third.y, third.z);
    if (textured) glTexCoord2f(0.0F, 1.0F * texture_scale_t);
    glVertex3f(fourth.x, fourth.y, fourth.z);
    glEnd();
    if (textured) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }
}

void draw_single_particles() {
    if (!g_session.has_value() || g_session->single_particles().empty()) {
        return;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& particle : g_session->single_particles()) {
        draw_single_particle(particle);
    }
}

void draw_player_death_particles() {
    if (!g_session.has_value()) {
        return;
    }
    const std::uint32_t duration = g_session->respawn_duration_ticks();
    if (duration == 0) {
        return;
    }
    const float first_third = static_cast<float>(duration) / 3.0F;
    constexpr float pi = 3.14159265358979323846F;

    const auto rotate_from_player = [](fruityprime::formats::Vector3 local,
                                       const fruityprime::net::Vec3& position,
                                       float yaw) {
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        return fruityprime::formats::Vector3{
            position.x + local.x * cosine + local.z * sine,
            position.y + local.y,
            position.z - local.x * sine + local.z * cosine};
    };
    const auto normalized_or_zero = [](fruityprime::formats::Vector3 value) {
        const float squared = value.length_squared();
        return squared > 0.0001F
            ? value / std::sqrt(squared)
            : fruityprime::formats::Vector3{};
    };

    for (const auto& player : g_session->players()) {
        if (player.health != 0
            || (player.flags & fruityprime::net::PlayerState::FlagActive) == 0
            || (player.flags & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            continue;
        }
        const std::uint32_t remaining = g_session->respawn_ticks(
            player.slot_index);
        if (remaining == 0 || remaining > duration) {
            continue;
        }
        // C# uses the first third of RespawnTime for DrawDeathParticles.
        const float time_pct = (static_cast<float>(duration)
                                - static_cast<float>(remaining))
            / first_third;
        if (time_pct < 0.0F || time_pct > 1.0F) {
            continue;
        }
        std::size_t hunter_index = 0;
        try {
            hunter_index = static_cast<std::size_t>(
                g_session->player_profile(player.slot_index).hunter);
        } catch (const std::exception&) {
            continue;
        }
        if (hunter_index >= g_player_models.size()) {
            continue;
        }
        const auto& hunter = g_player_models[hunter_index];
        if (!hunter.model.has_value() || !hunter.instance.has_value()) {
            continue;
        }
        const auto& model = *hunter.model;
        const auto& instance = *hunter.instance;
        const auto& states = instance.node_states();
        if (states.size() < 2) {
            continue;
        }

        const float scale = time_pct * 0.5F + 0.1F;
        const float angle = std::sin((270.0F - 90.0F * time_pct)
                                     * pi / 180.0F);
        // sin(180)-sin(270) is exactly one, so this is the managed offset
        // calculation written without a second source of rounding drift.
        const float offset = angle + 1.0F;
        const float yaw = std::atan2(player.facing.x, player.facing.z);
        const auto player_position = fruityprime::formats::Vector3{
            player.position.x, player.position.y, player.position.z};
        const auto node_position = [&](std::size_t index) {
            auto local = fruityprime::formats::Vector3{
                states[index].animation.m41,
                states[index].animation.m42 + offset,
                states[index].animation.m43};
            return rotate_from_player(local, player.position, yaw);
        };
        const auto emit = [&](fruityprime::formats::Vector3 position) {
            draw_single_particle(fruityprime::gameplay::SingleParticleState{
                fruityprime::gameplay::SingleParticleType::Death,
                {position.x, position.y, position.z},
                {1.0F, 1.0F, 1.0F}, 1.0F - time_pct, scale});
        };

        for (std::size_t index = 1; index < model.nodes().size(); ++index) {
            if (index >= states.size()) {
                break;
            }
            const auto& node = model.nodes()[index];
            const auto node_pos = node_position(index);
            const auto draw_segments = [&](std::int16_t link) {
                if (link <= 0
                    || static_cast<std::size_t>(link) >= states.size()) {
                    return;
                }
                const auto linked_pos = node_position(
                    static_cast<std::size_t>(link));
                for (int segment = 1; segment < 5; ++segment) {
                    const float fraction = static_cast<float>(segment) / 5.0F;
                    auto position = node_pos
                        + (linked_pos - node_pos) * fraction;
                    const auto away = normalized_or_zero(position
                                                         - player_position);
                    position += away * offset;
                    emit(position);
                }
            };
            draw_segments(node.child_id);
            draw_segments(node.next_id);
            auto final_position = node_pos;
            final_position += normalized_or_zero(final_position
                                                 - player_position) * offset;
            emit(final_position);
        }
    }
}

fruityprime::formats::Vector3 entity_vector(
    const fruityprime::scene::VolumePoint& value) noexcept {
    return {value.x, value.y, value.z};
}

void draw_entity_model(const EntityRenderModel& render_model) {
    if (render_model.model == nullptr || !render_model.instance.has_value()) {
        return;
    }
    if (render_model.enemy_catalog_model || render_model.item_catalog_model
        || render_model.projectile_catalog_model
        || render_model.effect_catalog_model) {
        return;
    }
    if (render_model.entity != nullptr
        && (!render_model.entity->active() || render_model.entity->hidden())) {
        return;
    }

    fruityprime::formats::Vector3 position{};
    fruityprime::formats::Vector3 up{0.0F, 1.0F, 0.0F};
    fruityprime::formats::Vector3 facing{0.0F, 0.0F, 1.0F};
    if (render_model.entity != nullptr) {
        position = entity_vector(render_model.entity->position());
        up = entity_vector(render_model.entity->up());
        facing = entity_vector(render_model.entity->facing());
    } else if (render_model.source != nullptr) {
        position = entity_vector({
            render_model.source->position.x.to_float(),
            render_model.source->position.y.to_float(),
            render_model.source->position.z.to_float()});
        up = entity_vector({
            render_model.source->up_vector.x.to_float(),
            render_model.source->up_vector.y.to_float(),
            render_model.source->up_vector.z.to_float()});
        facing = entity_vector({
            render_model.source->facing_vector.x.to_float(),
            render_model.source->facing_vector.y.to_float(),
            render_model.source->facing_vector.z.to_float()});
    }
    if (facing.length_squared() < 0.0001F) {
        facing = {0.0F, 0.0F, 1.0F};
    }
    if (up.length_squared() < 0.0001F) {
        up = {0.0F, 1.0F, 0.0F};
    }
    const auto transform = fruityprime::formats::MatrixOps::get_transform4(
        facing.normalized(), up.normalized(), position);
    const GLfloat matrix[16]{
        transform.m11, transform.m12, transform.m13, transform.m14,
        transform.m21, transform.m22, transform.m23, transform.m24,
        transform.m31, transform.m32, transform.m33, transform.m34,
        transform.m41, transform.m42, transform.m43, transform.m44};
    glPushMatrix();
    glMultMatrixf(matrix);
    if (render_model.local_y != 0.0F) {
        glTranslatef(0.0F, render_model.local_y, 0.0F);
    }
    draw_model_layer(DrawLayer::Entity, NoHunter, &*render_model.instance,
                     &render_model);
    glPopMatrix();
}

void draw_entity_models() {
    for (const auto& render_model : g_entity_models) {
        draw_entity_model(*render_model);
    }
}

void draw_item_catalog_model(
    const EntityRenderModel& render_model,
    const fruityprime::gameplay::ItemState& item,
    std::uint64_t tick) {
    if (render_model.model == nullptr || !render_model.instance.has_value()) {
        return;
    }
    constexpr float degrees_per_tick = 360.0F * 0.35F / 60.0F;
    const float spin = std::fmod(
        static_cast<float>(tick) * degrees_per_tick, 360.0F);
    const float float_offset = (std::sin(spin * 3.14159265358979323846F
                                          / 180.0F) + 1.0F) / 8.0F;
    glPushMatrix();
    glTranslatef(item.position.x, item.position.y + float_offset,
                 item.position.z);
    glRotatef(spin, 0.0F, 1.0F, 0.0F);
    draw_model_layer(DrawLayer::Entity, NoHunter, &*render_model.instance,
                     &render_model);
    glPopMatrix();
}

void draw_item_models() {
    if (!g_session.has_value()) {
        return;
    }
    for (const auto& item : g_session->items()) {
        const auto raw_type = static_cast<std::int32_t>(item.type);
        if (raw_type < 0
            || raw_type >= static_cast<std::int32_t>(g_item_models.size())) {
            continue;
        }
        const auto* render_model = g_item_models[static_cast<std::size_t>(
            raw_type)];
        if (render_model == nullptr) {
            continue;
        }
        draw_item_catalog_model(*render_model, item, g_session->tick_count());
    }
}

// PlayerDraw.GetDrawInfo: drawBiped is false for the main player while the
// first-person camera is on them, and the gun model is drawn in its place.
// The native camera is first person exactly when a player camera was built,
// so the biped of the player it follows must not be drawn -- otherwise the
// local hunter is rendered around the camera and its body pokes into view.
void draw_player_model(int hidden_slot) {
    const bool has_player_model = std::any_of(
        g_player_models.begin(), g_player_models.end(),
        [](const HunterModelSet& model) {
            return model.model.has_value() || model.alt_model.has_value();
        });
    if (!has_player_model || !g_session.has_value()
        || g_session->players().empty()) {
        return;
    }
    for (const auto& player : g_session->players()) {
        if (hidden_slot >= 0
            && player.slot_index == static_cast<std::uint8_t>(hidden_slot)) {
            continue;
        }
        if (player.health == 0
            || (player.flags & fruityprime::net::PlayerState::FlagActive) == 0
            || (player.flags & fruityprime::net::PlayerState::FlagSpawned) == 0
            || (player.flags
                & fruityprime::net::PlayerState::FlagSpectating) != 0) {
            continue;
        }
        const float yaw = std::atan2(player.facing.x, player.facing.z)
            * 180.0F / 3.14159265358979323846F;
        const bool alt_form =
            (player.flags & fruityprime::net::PlayerState::FlagAltForm) != 0;
        std::size_t hunter_index = static_cast<std::size_t>(
            g_session->player_profile(player.slot_index).hunter);
        if (hunter_index >= fruityprime::metadata::HunterCount) {
            hunter_index = std::min<std::size_t>(
                g_local_hunter, fruityprime::metadata::HunterCount - 1);
        }
        auto& model = g_player_models[hunter_index];
        const bool use_alt = alt_form && model.alt_model.has_value();
        if (!model.model.has_value()
            && !model.alt_model.has_value()) {
            continue;
        }
        const DrawLayer layer = use_alt ? DrawLayer::PlayerAlt
                                        : DrawLayer::Player;
        glPushMatrix();
        glTranslatef(player.position.x, player.position.y, player.position.z);
        glRotatef(yaw, 0.0F, 1.0F, 0.0F);
        const auto* instance = use_alt && model.alt_instance.has_value()
            ? &*model.alt_instance
            : (model.instance.has_value() ? &*model.instance : nullptr);
        draw_model_layer(layer, static_cast<std::uint8_t>(hunter_index),
                         instance);
        glPopMatrix();
    }
}



// Scene.DrawHudFilterModel: the wash the managed HUD puts over the scene
// while it is showing something else -- the intro's rules, the scoreboard,
// the weapon menu.  The cartridge draws a one-material model whose own alpha
// is 20/31, multiplied by whatever the caller passes; this stands in for it
// with a flat quad of the same strength rather than loading the model, which
// is a screen-filling square with nothing on it but that alpha.
void draw_hud_filter(float multiplier, int width, int height) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.0F, 0.0F, 0.0F,
              std::clamp(20.0F / 31.0F * multiplier, 0.0F, 1.0F));
    draw_hud_box(0.0F, 0.0F, static_cast<float>(width),
                 static_cast<float>(height));
}

// PlayerHud's renderer seam, implemented against this head's GL.  The layout
// is all in PlayerHud.cpp, in the counterpart of the file it came from; this
// only turns a DS-space request into a textured quad.
class GlHudBackend final : public fruityprime::players::HudBackend {
public:
    GlHudBackend(const MphReadNative::Hud::Rect& area, float scale,
                 int width, int height) noexcept
        : area_(area), scale_(scale), width_(width), height_(height) {}

    [[nodiscard]] bool draw_hud_object(Object object, std::size_t variant,
                                       std::size_t frame,
                                       std::size_t palette, float x, float y,
                                       float scale, float alpha,
                                       const Color* color) override {
        const HudObjectImage* image = nullptr;
        switch (object) {
        case Object::Font:
            image = g_hud_font.has_value() ? &*g_hud_font : nullptr;
            break;
        case Object::FontMask:
            image = g_hud_font_mask.has_value() ? &*g_hud_font_mask : nullptr;
            break;
        case Object::HealthBar:
            image = g_hud_health_bar.has_value() ? &*g_hud_health_bar
                                                 : nullptr;
            break;
        case Object::HealthBarSub:
            image = g_hud_health_bar_sub.has_value() ? &*g_hud_health_bar_sub
                                                     : nullptr;
            break;
        case Object::AmmoBar:
            image = g_hud_ammo_bar.has_value() ? &*g_hud_ammo_bar : nullptr;
            break;
        case Object::WeaponIcon:
            image = g_hud_weapon_icon.has_value() ? &*g_hud_weapon_icon
                                                  : nullptr;
            break;
        case Object::Stars:
            image = g_hud_stars.has_value() ? &*g_hud_stars : nullptr;
            break;
        case Object::HunterPortrait:
            image = variant < g_hud_hunters.size()
                    && g_hud_hunters[variant].has_value()
                ? &*g_hud_hunters[variant] : nullptr;
            break;
        }
        if (image == nullptr || scale_ <= 0.0F) {
            return false;
        }
        const HudTint tint = color == nullptr
            ? HudTint{}
            : HudTint{color->red, color->green, color->blue};
        return draw_hud_object_image(*image, frame, palette,
                                     area_.left + x * scale_,
                                     area_.top + y * scale_,
                                     scale_ * scale, alpha, tint);
    }

    void draw_hud_filter_model(float alpha) override {
        draw_hud_filter(alpha, width_, height_);
    }

private:
    MphReadNative::Hud::Rect area_;
    float scale_ = 0.0F;
    int width_ = 0;
    int height_ = 0;
};

// Everything PlayerHud reads that lives on GameState and the scene here.
[[nodiscard]] fruityprime::players::HudContext hud_context(
    fruityprime::players::HudBackend& backend) {
    fruityprime::players::HudContext context;
    context.backend = &backend;
    context.session = g_session.has_value() ? &*g_session : nullptr;
    context.state = &g_game_state;
    context.local_slot = g_local_slot;
    context.hunter = g_local_hunter;
    context.elapsed_seconds = g_hud_elapsed_seconds;
    context.intro_seconds = g_intro_seconds;
    context.frames_per_second = g_display_fps;
    context.match_time_remaining = g_match_flow.has_value()
        ? g_match_flow->state().time_remaining : -1.0F;
    context.mode = g_match_flow.has_value()
        ? static_cast<fruityprime::game::Mode>(g_match_flow->state().mode)
        : g_game_state.mode;
    context.match_over = g_match_flow.has_value()
        && g_match_flow->phase() != fruityprime::match::Phase::InProgress;
    context.networked = g_net_client != nullptr && g_net_client->connected();
    context.hud_message = [](int id) {
        return hud_message(static_cast<std::uint32_t>(id));
    };
    context.rules_message = [](int id) {
        if (g_hud_msgs_multi.empty()) {
            return std::string{};
        }
        return fruityprime::strings::get_message(
            g_hud_msgs_multi, 'S', static_cast<std::uint32_t>(id));
    };
    context.nickname = [](std::uint8_t slot) { return roster_name(slot); };
    context.ping = [](std::uint8_t slot) { return roster_ping(slot); };
    return context;
}

// PlayerEntity.SetGunAnimation, for the one animation this head needs so
// far: the cannon at rest with the equipped weapon's own barrels out.
//
// Slot 0 carries the node animation -- without it every part of the cannon
// sits at its bind pose, which for Samus is the barrels splayed open like a
// flower.  Slot 1 is the per-weapon overlay, which must NOT set nodes: it
// would undo slot 0.
// AnimationSetFlags has no complement operator, and adding one for two
// call sites would be a wider change than clearing a bit here.
[[nodiscard]] fruityprime::model::AnimationSetFlags without(
    fruityprime::model::AnimationSetFlags value,
    fruityprime::model::AnimationSetFlags bit) noexcept {
    return static_cast<fruityprime::model::AnimationSetFlags>(
        static_cast<std::uint16_t>(value)
        & ~static_cast<std::uint16_t>(bit));
}

void set_gun_animation(fruityprime::model::ModelInstance& instance,
                       std::size_t hunter, std::uint8_t weapon,
                       int animation) {
    using Flags = fruityprime::model::AnimationSetFlags;
    const auto& ids = fruityprime::metadata::GunAnimationIds;
    if (hunter >= 8 || animation < 0 || animation >= 13) {
        return;
    }
    auto set_flags = Flags::Texture | Flags::Texcoord | Flags::Material
        | Flags::Unused | Flags::Node;
    const int node_animation = ids[hunter][animation][0];
    if (node_animation >= 0) {
        instance.set_animation(node_animation, 0, set_flags);
    }
    const std::size_t slot = static_cast<std::size_t>(weapon) + 1;
    if (slot >= 10) {
        return;
    }
    const int weapon_animation = ids[hunter][animation][slot];
    if (weapon_animation < 0) {
        return;
    }
    set_flags = without(set_flags, Flags::Node);
    if (hunter == static_cast<std::size_t>(
            fruityprime::metadata::Hunter::Sylux)) {
        set_flags = without(set_flags, Flags::Texcoord);
    }
    instance.set_animation(weapon_animation, 1, set_flags);
}

// PlayerDraw's first-person branch: the arm cannon, placed by
// PlayerProcess.UpdateAimVecs.
//
// The gun does not sit at the camera and point where the camera points.  It
// hangs off the camera by three fixed offsets -- along the facing, along the
// camera's horizontal right, and along its up -- and then aims at the point
// the player is aiming at, which is a little in front.  That is what makes it
// swing across the view as you turn instead of being painted onto it.
void draw_gun_model(const fruityprime::players::CameraInfo& camera) {
    using Vec3 = fruityprime::net::Vec3;
    if (!g_session.has_value() || !g_session->has_player(g_local_slot)) {
        return;
    }
    const auto& player = g_session->player(g_local_slot);
    if (player.health == 0
        || (player.flags & fruityprime::net::PlayerState::FlagAltForm) != 0
        || (player.flags & fruityprime::net::PlayerState::FlagSpectating)
               != 0) {
        return;
    }
    std::size_t hunter_index = static_cast<std::size_t>(
        g_session->player_profile(g_local_slot).hunter);
    if (hunter_index >= fruityprime::metadata::HunterCount) {
        hunter_index = std::min<std::size_t>(
            g_local_hunter, fruityprime::metadata::HunterCount - 1);
    }
    // Guardian carries no arm cannon in the managed draw either.
    if (hunter_index
        == static_cast<std::size_t>(fruityprime::metadata::Hunter::Guardian)) {
        return;
    }
    auto& models = g_player_models[hunter_index];
    if (!models.gun_instance.has_value()) {
        return;
    }
    // GunAnimation.Idle, reapplied when the weapon changes: the overlay in
    // slot 1 is per-weapon, so the cannon has to be told.
    static std::size_t posed_hunter = static_cast<std::size_t>(-1);
    static std::uint8_t posed_weapon = 0xFF;
    if (posed_hunter != hunter_index
        || posed_weapon != player.current_weapon) {
        posed_hunter = hunter_index;
        posed_weapon = player.current_weapon;
        set_gun_animation(*models.gun_instance, hunter_index,
                          player.current_weapon, /*GunAnimation.Idle=*/3);
    }
    const auto subtract = [](Vec3 a, Vec3 b) {
        return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto dot = [](Vec3 a, Vec3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto cross = [](Vec3 a, Vec3 b) {
        return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x};
    };
    const auto normalize = [&dot](Vec3 value, Vec3 fallback) {
        const float length_squared = dot(value, value);
        if (!std::isfinite(length_squared) || length_squared <= 0.000001F) {
            return fallback;
        }
        const float inverse = 1.0F / std::sqrt(length_squared);
        return Vec3{value.x * inverse, value.y * inverse,
                    value.z * inverse};
    };
    // PlayerProcess.UpdateAimVecs, which lives on the player because the
    // shot path and the bots read the same vectors.
    const auto aim_vectors =
        fruityprime::players::PlayerProcess::update_aim_vecs(
            fruityprime::metadata::PlayerValuesTable[hunter_index],
            camera.position, camera.facing, camera.up,
            // CameraInfo.Field50/54 is the horizontal basis the camera was
            // built from, which is what the managed player calls _gunVec2.
            Vec3{camera.field50, 0.0F, camera.field54},
            // _gunVec1: where the camera is looking, pitch included.
            normalize(subtract(camera.target, camera.position),
                      Vec3{0.0F, 0.0F, 1.0F}),
            // _gunViewBob, the walk sway, which this head does not drive yet.
            0.0F,
            fruityprime::features::Features::FixedWeapon());
    const Vec3 gun_pos = aim_vectors.gun_draw_pos;
    const Vec3 aim = aim_vectors.aim_vec;
    const Vec3 up = camera.up;

    // GetTransformMatrix(aim, up, gun_pos), column-major for glMultMatrixf.
    const Vec3 right = normalize(cross(up, aim), {1.0F, 0.0F, 0.0F});
    const Vec3 fixed_up = cross(aim, right);
    const std::array<float, 16> transform{{
        right.x, right.y, right.z, 0.0F,
        fixed_up.x, fixed_up.y, fixed_up.z, 0.0F,
        aim.x, aim.y, aim.z, 0.0F,
        gun_pos.x, gun_pos.y, gun_pos.z, 1.0F}};

    // The arm cannon is inches from the eye, so it reaches through anything
    // the camera is standing against.  Its own depth range keeps it in front
    // of the room without letting the room's near geometry cut into it.
    glClear(GL_DEPTH_BUFFER_BIT);
    // The effects pass runs immediately before this one and leaves the
    // texture unit and the blend function as it wants them.  The cannon has
    // parts whose material alpha is zero -- the five beam glows, of which the
    // game shows at most one -- so inheriting a blend function that ignores
    // alpha draws all five.
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glPushMatrix();
    glMultMatrixf(transform.data());
    draw_model_layer(DrawLayer::Gun,
                     static_cast<std::uint8_t>(hunter_index),
                     &*models.gun_instance);
    glPopMatrix();
}

void update_model_instance(
    fruityprime::model::ModelInstance& instance,
    bool use_node_transform) {
    if (instance.model().animations().any()) {
        if (instance.animation_info().index[0] < 0) {
            instance.set_animation(0);
        }
        instance.update_anim_frames();
    }
    instance.compute_node_matrices();
    // RoomEntity.UseNodeTransform is false in the managed implementation: a
    // room's authored node transforms are stored without the scale division
    // the runtime would need, so applying them scatters parts of the room
    // away from its collision.  Every other entity uses them.
    instance.animate_nodes(use_node_transform);
    instance.update_matrix_stack();
    instance.update_materials();
}

void update_render_models() {
    if (!g_session.has_value()
        || g_model_animation_tick == g_session->tick_count()) {
        return;
    }
    for (auto& model : g_player_models) {
        if (model.instance.has_value()) {
            update_model_instance(*model.instance);
        }
        if (model.alt_instance.has_value()) {
            update_model_instance(*model.alt_instance);
        }
        if (model.gun_instance.has_value()) {
            update_model_instance(*model.gun_instance);
        }
    }
    for (auto& model : g_entity_models) {
        if (model->instance.has_value()) {
            update_model_instance(*model->instance);
        }
    }
    if (g_room_instance.has_value()) {
        update_model_instance(*g_room_instance,
                              /*use_node_transform=*/false);
    }
    g_model_animation_tick = g_session->tick_count();
}

void update_static_entities() {
    if (g_static_world != nullptr) {
        g_static_world->process(1.0F / 60.0F);
    }
}

[[nodiscard]] fruityprime::sound::AudioVector3 audio_position(
    fruityprime::net::Vec3 value) noexcept {
    return {value.x, value.y, value.z};
}

void update_audio_listener_and_sources() {
    if (g_sfx_runtime == nullptr || !g_sound_catalog.has_value()
        || !g_session.has_value() || g_session->players().empty()) {
        return;
    }
    const fruityprime::net::PlayerState* listener = nullptr;
    if (g_spectator.is_spectating() && !g_spectator.free_camera()
        && g_session->has_player(g_spectator.view_slot())) {
        listener = &g_session->player(g_spectator.view_slot());
    } else if (g_session->has_player(g_local_slot)) {
        listener = &g_session->player(g_local_slot);
    } else {
        listener = &g_session->players().front();
    }
    g_sfx_runtime->set_listener(
        audio_position(listener->position), audio_position(listener->facing),
        {0.0F, 1.0F, 0.0F});

    for (const auto& player : g_session->players()) {
        if (player.slot_index >= g_player_sound_sources.size()) {
            continue;
        }
        const std::int32_t range_index = player.slot_index == g_local_slot
            ? -1 : 1;
        g_player_sound_sources[player.slot_index].update(
            audio_position(player.position), range_index,
            g_sound_catalog->sound_3d);
    }
}

[[nodiscard]] fruityprime::sound::SoundSource* acquire_audio_event_source(
    fruityprime::net::Vec3 position, std::int32_t range_index) noexcept {
    if (!g_sound_catalog.has_value() || g_audio_event_sources.empty()) {
        return nullptr;
    }
    auto& source = g_audio_event_sources[
        g_audio_event_source_cursor++ % g_audio_event_sources.size()];
    source.update(audio_position(position), range_index,
                  g_sound_catalog->sound_3d);
    return &source;
}

void play_audio_event(const fruityprime::gameplay::SoundEvent& event) {
    if (g_sfx_runtime == nullptr || !g_sound_catalog.has_value()
        || !g_session.has_value()) {
        return;
    }
    const auto* metadata = g_sound_catalog->metadata.has_value()
        ? &*g_sound_catalog->metadata : nullptr;
    constexpr std::int32_t ItemSpawnSfx = 28;
    constexpr std::int32_t PowerUpSmallSfx = 29;
    constexpr std::int32_t PowerUpLargeSfx = 30;
    constexpr std::int32_t AmmoPowerUpSmallSfx = 31;
    constexpr std::int32_t AmmoPowerUpLargeSfx = 32;
    constexpr std::int32_t WeaponPowerUpSfx = 35;
    constexpr std::int32_t DoubleDamagePowerUpSfx = 38;
    constexpr std::int32_t CloakPowerUpSfx = 42;
    constexpr std::int32_t PlayerSpawnSfx = 71;
    constexpr std::int32_t KeyPickupSfx = 97;
    constexpr std::int32_t KeyAppearSfx = 98;
    constexpr std::int32_t OpponentDamageSfx = 133;
    constexpr std::int32_t OpponentDeathSfx = 134;
    constexpr std::int32_t BeamSfx = 135;
    constexpr std::int32_t BeamHitSfx = 136;
    constexpr std::int32_t MissileSfx = 140;
    constexpr std::int32_t MissileHitSfx = 141;
    constexpr std::int32_t GenericHitSfx = 189;
    constexpr std::int32_t TurretAttackSfx = 237;
    constexpr std::int32_t GoreaAttack3ASfx = 323;
    constexpr std::int32_t GoreaAttack3BSfx = 324;
    constexpr std::int32_t Gorea1BDamageSfx = 320;
    constexpr std::int32_t Gorea2DamageSfx = 348;
    constexpr std::int32_t DamageSfx = 368;
    constexpr std::int32_t DeathSfx = 372;

    const auto play_at_source =
        [](std::int32_t id, fruityprime::sound::SoundSource* source,
           float recency = -1.0F) {
            if (id < 0) {
                return;
            }
            static_cast<void>(g_sfx_runtime->play_encoded(
                id, source, std::optional<bool>{false}, false, recency,
                source != nullptr));
        };
    const auto play_free = [](std::int32_t id) {
        if (id >= 0) {
            static_cast<void>(g_sfx_runtime->play_free_sfx(id));
        }
    };
    const auto player_source = [&](std::uint8_t slot)
        -> fruityprime::sound::SoundSource* {
        return slot < g_player_sound_sources.size()
            ? &g_player_sound_sources[slot] : nullptr;
    };
    const auto player_hunter = [&](std::uint8_t slot) {
        if (!g_session->has_player(slot)) {
            return static_cast<std::uint8_t>(0);
        }
        return static_cast<std::uint8_t>(
            g_session->player_profile(slot).hunter);
    };
    const auto beam_row = [](std::uint8_t weapon) {
        // Player weapon slots are Power, Missile, Volt, Hammer, Imperialist,
        // Judicator, Magmaul, Shock, Omega; SoundMeta uses cartridge BeamType
        // order with Missile between Power and Volt.
        constexpr std::array<std::uint8_t, 9> rows{0, 2, 1, 3, 4, 5, 6, 7, 8};
        return weapon < rows.size() ? rows[weapon] : static_cast<std::uint8_t>(
            0xff);
    };

    switch (event.cue) {
    case fruityprime::gameplay::SoundCue::BeamShot: {
        const std::int32_t id = metadata != nullptr
            ? metadata->beam(beam_row(event.weapon),
                             fruityprime::sound::BeamSfx::Shot)
            : event.weapon == 0 ? BeamSfx : event.weapon == 1
                ? MissileSfx : -1;
        play_at_source(id, player_source(event.slot));
        break;
    }
    case fruityprime::gameplay::SoundCue::TurretAttack:
        play_at_source(TurretAttackSfx,
                       acquire_audio_event_source(event.position, 3));
        break;
    case fruityprime::gameplay::SoundCue::GoreaAttack3A:
        play_at_source(GoreaAttack3ASfx,
                       acquire_audio_event_source(event.position, 3));
        break;
    case fruityprime::gameplay::SoundCue::GoreaAttack3B:
        play_at_source(GoreaAttack3BSfx,
                       acquire_audio_event_source(event.position, 3));
        break;
    case fruityprime::gameplay::SoundCue::Gorea1BDamage:
        play_at_source(Gorea1BDamageSfx,
                       acquire_audio_event_source(event.position, 3));
        break;
    case fruityprime::gameplay::SoundCue::Gorea2Damage:
        play_at_source(Gorea2DamageSfx,
                       acquire_audio_event_source(event.position, 3));
        break;
    case fruityprime::gameplay::SoundCue::BeamImpact: {
        const bool missile = event.weapon == 1;
        const auto type = event.impact
            == fruityprime::gameplay::BeamImpactKind::Surface
            ? fruityprime::sound::BeamSfx::Ricochet
            : fruityprime::sound::BeamSfx::Hit;
        const std::int32_t id = metadata != nullptr
            ? metadata->beam(beam_row(event.weapon), type)
            : event.impact
                == fruityprime::gameplay::BeamImpactKind::Surface
                ? missile ? MissileHitSfx : BeamHitSfx
                : missile ? MissileHitSfx : BeamHitSfx;
        play_at_source(id, acquire_audio_event_source(event.position, 3));
        break;
    }
    case fruityprime::gameplay::SoundCue::PlayerDamage: {
        const bool local = event.slot == g_local_slot;
        std::int32_t id = metadata != nullptr
            ? metadata->hunter(
                player_hunter(event.slot),
                local ? fruityprime::sound::HunterSfx::Damage
                      : fruityprime::sound::HunterSfx::DamageEnemy)
            : local ? DamageSfx : OpponentDamageSfx;
        play_at_source(id, player_source(event.slot), 5.0F / 30.0F);
        break;
    }
    case fruityprime::gameplay::SoundCue::PlayerDeath: {
        const bool local = event.slot == g_local_slot;
        const std::int32_t id = metadata != nullptr
            ? metadata->hunter(
                player_hunter(event.slot),
                local ? fruityprime::sound::HunterSfx::Death
                      : fruityprime::sound::HunterSfx::DeathEnemy)
            : local ? DeathSfx : OpponentDeathSfx;
        if (local) {
            play_free(id);
        } else {
            play_at_source(id, player_source(event.slot));
        }
        break;
    }
    case fruityprime::gameplay::SoundCue::PlayerSpawn: {
        const std::int32_t id = metadata != nullptr
            ? metadata->hunter(player_hunter(event.slot),
                               fruityprime::sound::HunterSfx::Spawn)
            : PlayerSpawnSfx;
        play_at_source(id, player_source(event.slot));
        break;
    }
    case fruityprime::gameplay::SoundCue::PlayerJump: {
        const std::int32_t id = metadata != nullptr
            ? metadata->hunter(player_hunter(event.slot),
                               fruityprime::sound::HunterSfx::Jump) : -1;
        play_at_source(id, player_source(event.slot));
        break;
    }
    case fruityprime::gameplay::SoundCue::ItemSpawn: {
        if (event.item_type == fruityprime::gameplay::ItemType::ArtifactKey) {
            play_free(KeyAppearSfx);
        } else {
            play_at_source(ItemSpawnSfx,
                           acquire_audio_event_source(event.position, 7));
        }
        break;
    }
    case fruityprime::gameplay::SoundCue::ItemPickup: {
        const bool local = event.slot == g_local_slot;
        const auto source = player_source(event.slot);
        std::int32_t id = -1;
        switch (event.item_type) {
        case fruityprime::gameplay::ItemType::HealthSmall:
            id = PowerUpSmallSfx;
            break;
        case fruityprime::gameplay::ItemType::HealthMedium:
        case fruityprime::gameplay::ItemType::HealthBig:
            id = PowerUpLargeSfx;
            break;
        case fruityprime::gameplay::ItemType::UASmall:
        case fruityprime::gameplay::ItemType::MissileSmall:
            id = AmmoPowerUpSmallSfx;
            break;
        case fruityprime::gameplay::ItemType::UABig:
        case fruityprime::gameplay::ItemType::MissileBig:
            id = AmmoPowerUpLargeSfx;
            break;
        case fruityprime::gameplay::ItemType::VoltDriver:
        case fruityprime::gameplay::ItemType::Battlehammer:
        case fruityprime::gameplay::ItemType::Imperialist:
        case fruityprime::gameplay::ItemType::Judicator:
        case fruityprime::gameplay::ItemType::Magmaul:
        case fruityprime::gameplay::ItemType::ShockCoil:
        case fruityprime::gameplay::ItemType::OmegaCannon:
            id = WeaponPowerUpSfx;
            break;
        case fruityprime::gameplay::ItemType::AffinityWeapon:
            id = player_hunter(event.slot) == 0
                    || player_hunter(event.slot) == 7
                ? AmmoPowerUpSmallSfx : WeaponPowerUpSfx;
            break;
        case fruityprime::gameplay::ItemType::DoubleDamage:
        case fruityprime::gameplay::ItemType::Deathalt:
            id = DoubleDamagePowerUpSfx;
            break;
        case fruityprime::gameplay::ItemType::Cloak:
            id = CloakPowerUpSfx;
            break;
        case fruityprime::gameplay::ItemType::ArtifactKey:
            id = KeyPickupSfx;
            break;
        case fruityprime::gameplay::ItemType::EnergyTank:
        case fruityprime::gameplay::ItemType::MissileExpansion:
        case fruityprime::gameplay::ItemType::UAExpansion:
        case fruityprime::gameplay::ItemType::None:
        case fruityprime::gameplay::ItemType::PickWpnMissile:
            break;
        }
        if (local) {
            play_free(id);
        } else {
            play_at_source(id, source);
        }
        break;
    }
    case fruityprime::gameplay::SoundCue::EnemyDamage: {
        const std::int32_t id = metadata != nullptr
            ? metadata->enemy_damage(event.enemy_type) : GenericHitSfx;
        play_at_source(id, acquire_audio_event_source(event.position, 3));
        break;
    }
    case fruityprime::gameplay::SoundCue::EnemyDeath: {
        const std::int32_t id = metadata != nullptr
            ? metadata->enemy_death(event.enemy_type) : -1;
        play_at_source(id, acquire_audio_event_source(event.position, 3));
        break;
    }
    }
}

void update_audio_after_tick() {
    if (!g_session.has_value()) {
        return;
    }
    if (g_game_state.single_player) {
        g_game_state.process_story_frame(1.0F / 60.0F);
    }
    if (g_sfx_runtime != nullptr) {
        update_audio_listener_and_sources();
        g_sfx_runtime->update(1.0F / 60.0F);
        for (const auto& event : g_session->sound_events()) {
            play_audio_event(event);
        }
    }
    if (g_music_controller != nullptr && g_game_state.single_player
        && g_game_state.escape_timer >= 0.0F) {
        // MusicController takes the same 30 Hz frame countdown as the
        // managed UpdateEscapeState path; State stores it in seconds so the
        // rest of the frontend remains unit-safe.
        fruityprime::sound::Music::UpdateEscapeMusic();
    }
    if (g_music_runtime != nullptr) {
        g_music_runtime->update(1.0F / 60.0F);
    }
}

void draw_hud(int width, int height) {
    static bool wrote_hud_state = false;
    if (!wrote_hud_state) {
        std::ofstream hud_state(
            "FruityPrime-native-hud-state.log", std::ios::trunc);
        hud_state << "session=" << (g_session.has_value() ? 1 : 0)
                  << " players="
                  << (g_session.has_value()
                          ? g_session->players().size() : 0)
                  << " has_local="
                  << (g_session.has_value()
                          && g_session->has_player(g_local_slot) ? 1 : 0)
                  << " local_slot="
                  << static_cast<unsigned>(g_local_slot) << '\n';
        wrote_hud_state = true;
    }
    if (!g_session.has_value() || !g_session->has_player(g_local_slot)) {
        return;
    }
    const auto& player = g_session->player(g_local_slot);
    const auto& inventory = g_session->inventory(g_local_slot);
    const auto hud_player = MphReadNative::Hud::player_state(
        *g_session, g_local_slot);
    const int safe_width = std::max(width, 1);
    const int safe_height = std::max(height, 1);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(safe_width),
            static_cast<double>(safe_height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const MphReadNative::Hud::Viewport hud_viewport{safe_width, safe_height};
    const MphReadNative::Hud::Rect hud_area = hud_viewport.design_area();
    const float hud_scale = hud_viewport.scale();
    const bool scan_visor = g_scan_visor_active
        || (static_cast<std::uint32_t>(g_input.buttons())
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::ScanVisor)) != 0;
    if (fruityprime::mods::render::options().show_fps()) {
        glEnable(GL_TEXTURE_2D);
        GlHudBackend backend(hud_area, hud_scale, safe_width, safe_height);
        fruityprime::players::PlayerHud::DrawFps(hud_context(backend));
        glDisable(GL_TEXTURE_2D);
    }
    if (g_intro.has_value()) {
        // PlayerHud: while the intro plays, the helmet, the readouts and the
        // reticle are all absent -- the screen belongs to the rules.
        draw_hud_filter(15.0F / 31.0F, safe_width, safe_height);
        glEnable(GL_TEXTURE_2D);
        GlHudBackend backend(hud_area, hud_scale, safe_width, safe_height);
        fruityprime::players::PlayerHud::DrawModeRules(hud_context(backend));
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        return;
    }
    if (!g_no_helmet && !g_pro_hud && !g_show_scoreboard) {
        draw_hud_layers(hud_player, hud_area, hud_scale, scan_visor);
    }
    glDisable(GL_TEXTURE_2D);

    const float ui_scale = std::clamp(
        std::min(static_cast<float>(safe_width) / 640.0F,
                 static_cast<float>(safe_height) / 360.0F),
        1.0F, 3.0F);
    const float margin = 16.0F * ui_scale;

    if (auto* main_player = fruityprime::players::PlayerEntity::Main();
        main_player != nullptr) {
        fruityprime::chat::player_entity_chat_hud::draw(
            main_player->ChatHudState(), safe_width, safe_height,
            fruityprime::renderer::opengl::draw_chat_text);
    }
    // PlayerEntityChatHud only clears the score/readout it explicitly owns;
    // moving spectator and fallback HUD elements here was native-only drift.
    const float status_top = margin;

    const float center_x = static_cast<float>(safe_width) * 0.5F;
    const float center_y = static_cast<float>(safe_height) * 0.5F;
    // The clock and the mode's name used to sit here permanently.  Neither
    // is on the managed HUD: the clock belongs to the scoreboard, and the
    // mode's name is never drawn during a match at all.  What remains is the
    // objective line, which stands in for the mode HUDs (nodes, octoliths,
    // the defender's ring) until those are ported -- Battle has none, so it
    // is absent in the screenshot this was matched against.
    if (g_match_flow.has_value()) {
        if (const std::string objective = objective_hud_text(
                g_match_flow->state().mode, player); !objective.empty()) {
            glColor3f(1.0F, 0.80F, 0.30F);
            draw_hud_text_right(objective,
                                static_cast<float>(safe_width) - margin,
                                margin + 14.0F * ui_scale, ui_scale);
        }
    }
    if (g_show_scoreboard) {
        // InitHudState turns the reticle off every frame and the guarded
        // block turns it back on; with the scoreboard up that block does not
        // run, so nothing aims at the middle of a list of names.
    } else if (g_hud_reticle.has_value() && hud_scale > 0.0F) {
        const float reticle_width = static_cast<float>(
            g_hud_reticle->width) * hud_scale;
        const float reticle_height = static_cast<float>(
            g_hud_reticle->height) * hud_scale;
        draw_hud_texture(*g_hud_reticle,
                         center_x - reticle_width * 0.5F,
                         center_y - reticle_height * 0.5F,
                         center_x + reticle_width * 0.5F,
                         center_y + reticle_height * 0.5F);
        glDisable(GL_TEXTURE_2D);
    } else {
        glColor3f(1.0F, 0.88F, 0.28F);
        glBegin(GL_LINES);
        glVertex2f(center_x - 10.0F * ui_scale, center_y);
        glVertex2f(center_x - 3.0F * ui_scale, center_y);
        glVertex2f(center_x + 3.0F * ui_scale, center_y);
        glVertex2f(center_x + 10.0F * ui_scale, center_y);
        glVertex2f(center_x, center_y - 10.0F * ui_scale);
        glVertex2f(center_x, center_y - 3.0F * ui_scale);
        glVertex2f(center_x, center_y + 3.0F * ui_scale);
        glVertex2f(center_x, center_y + 10.0F * ui_scale);
        glEnd();
    }
    if (scan_visor && !g_show_scoreboard) {
        draw_scan_overlay(hud_area, hud_scale);
        draw_scan_dialog(hud_area, hud_scale);
    }

    if (!g_show_scoreboard && g_spectator.is_spectating()) {
        glColor3f(1.0F, 0.88F, 0.28F);
        draw_hud_text("SPECTATING", margin, status_top, ui_scale);
        glColor3f(0.78F, 0.84F, 0.92F);
        draw_hud_text(g_spectator.free_camera()
                          ? "F6 WATCH PLAYER"
                          : "F6 NEXT PLAYER",
                      margin, status_top + 16.0F * ui_scale, ui_scale);
        draw_hud_text("F7 REJOIN",
                      margin, status_top + 32.0F * ui_scale, ui_scale);
    } else if (!g_show_scoreboard) {
        if (g_pro_hud) {
            if (!g_paused && !scan_visor && player.health > 0
                && !hud_player.spectating) {
                const auto energy_tank = fruityprime::players::profile(
                    g_session->player_hunter(g_local_slot)).energy_tank;
                const auto frame = fruityprime::mods::render::pro_hud::build(
                    hud_player, inventory, g_game_state, g_local_slot,
                    energy_tank,
                    fruityprime::chat::player_entity_chat_hud::clearance(
                        fruityprime::chat::ChatBox::Available(), 12.0F));
                draw_pro_hud(frame, hud_area, hud_scale);
            }
        } else {
        // PlayerHud.DrawHealthbars, DrawAmmoBar and DrawModeScore, which
        // are all in PlayerHud.cpp now.  The offset they hang from is eased
        // once a frame rather than per readout.
        bool asset_health = false;
        if (!g_paused && !scan_visor) {
            GlHudBackend backend(hud_area, hud_scale, safe_width,
                                 safe_height);
            const auto context = hud_context(backend);
            const bool alt_form = hud_player.alt_form;
            g_healthbar_y_offset =
                fruityprime::players::PlayerHud::UpdateHealthbarOffset(
                    context, g_healthbar_y_offset, alt_form);
            glEnable(GL_TEXTURE_2D);
            asset_health =
                fruityprime::players::PlayerHud::DrawHealthbars(
                    context, hud_player, g_healthbar_y_offset);
            fruityprime::players::PlayerHud::DrawAmmoBar(
                context, hud_player, inventory);
            fruityprime::players::PlayerHud::DrawModeScore(
                context, 212,
                fruityprime::players::PlayerHud::FormatModeScore(
                    context, g_local_slot));
            glDisable(GL_TEXTURE_2D);
        }
        // The readouts below are what a copy with no cartridge assets gets.
        // Drawn on top of the real HUD they are two of everything, so the
        // real one wins whenever it drew.
        if (!asset_health) {
        const float bar_left = margin;
        const float bar_top = status_top + 20.0F * ui_scale;
        const float bar_width = 220.0F * ui_scale;
        const float bar_height = 14.0F * ui_scale;
        glColor4f(0.08F, 0.08F, 0.10F, 0.86F);
        draw_hud_box(bar_left, bar_top, bar_left + bar_width,
                     bar_top + bar_height);
        const float health_fraction = std::clamp(
            static_cast<float>(hud_player.health)
                / static_cast<float>(std::max<std::uint16_t>(
                    hud_player.health_max, 1)),
            0.0F, 1.0F);
        glColor4f(0.20F, 0.88F, 0.32F, 1.0F);
        draw_hud_box(bar_left, bar_top,
                     bar_left + bar_width * health_fraction,
                     bar_top + bar_height);
        glColor3f(0.92F, 0.96F, 1.0F);
        draw_hud_text("HEALTH " + std::to_string(hud_player.health),
                      bar_left, status_top, ui_scale);
        glColor3f(0.86F, 0.90F, 0.95F);
        draw_hud_text("SCORE " + std::to_string(hud_player.points),
                      bar_left + bar_width + 16.0F * ui_scale,
                      status_top, ui_scale);
        draw_hud_text("K " + std::to_string(hud_player.kills)
                          + " D " + std::to_string(hud_player.deaths),
                      bar_left + bar_width + 16.0F * ui_scale,
                      status_top + 12.0F * ui_scale, ui_scale);
        glColor3f(0.62F, 0.86F, 1.0F);
        draw_hud_text("UA " + std::to_string(inventory.ammo[0])
                          + " M " + std::to_string(inventory.ammo[1]),
                      bar_left, bar_top + 18.0F * ui_scale, ui_scale);
        std::string effects;
        if (inventory.double_damage_ticks > 0) {
            effects += " DD";
        }
        if (inventory.cloak_ticks > 0) {
            effects += " CLOAK";
        }
        if (!effects.empty()) {
            glColor3f(1.0F, 0.78F, 0.22F);
            draw_hud_text(effects, bar_left + 92.0F * ui_scale,
                          bar_top + 18.0F * ui_scale, ui_scale);
        }

        const float weapon_top = static_cast<float>(safe_height)
            - margin - 34.0F * ui_scale;
        glColor3f(0.92F, 0.96F, 1.0F);
        draw_hud_text("WEAPON " + hud_player.weapon_name,
                      margin, weapon_top - 12.0F * ui_scale, ui_scale);
        constexpr float weapon_cell_width = 22.0F;
        for (std::uint8_t weapon = 0; weapon < 9; ++weapon) {
            const float left = margin
                + static_cast<float>(weapon) * weapon_cell_width * ui_scale;
            const bool equipped = weapon == player.current_weapon;
            glColor4f(equipped ? 0.95F : 0.18F,
                      equipped ? 0.72F : 0.20F,
                      equipped ? 0.18F : 0.24F,
                      equipped ? 1.0F : 0.74F);
            draw_hud_box(left, weapon_top,
                         left + 18.0F * ui_scale,
                         weapon_top + 18.0F * ui_scale);
            glColor3f(0.04F, 0.04F, 0.05F);
            draw_hud_text(std::to_string(static_cast<unsigned>(weapon + 1)),
                          left + 6.0F * ui_scale,
                          weapon_top + 5.0F * ui_scale, ui_scale);
        }
        }
        if (player.health == 0) {
            glEnable(GL_TEXTURE_2D);
            GlHudBackend backend(hud_area, hud_scale, safe_width,
                                 safe_height);
            fruityprime::players::PlayerHud::DrawRespawnPrompt(
                hud_context(backend));
            glDisable(GL_TEXTURE_2D);
        }
        }
    } else {
        draw_hud_filter(1.0F, safe_width, safe_height);
        // DrawMatchTime and DrawScoreboard are the pair the managed HUD
        // draws here: the clock above the list, both in the cartridge's own
        // art.  Nothing is drawn behind them -- the DS scoreboard sits
        // straight on the match.
        glEnable(GL_TEXTURE_2D);
        GlHudBackend backend(hud_area, hud_scale, safe_width, safe_height);
        const auto context = hud_context(backend);
        fruityprime::players::PlayerHud::DrawMatchTime(context);
        fruityprime::players::PlayerHud::DrawScoreboard(context);
        glDisable(GL_TEXTURE_2D);
    }

    if (g_match_flow.has_value()
        && g_match_flow->phase() != fruityprime::match::Phase::InProgress) {
        const float panel_width = 260.0F * ui_scale;
        const float panel_height = 74.0F * ui_scale;
        const float panel_left = (static_cast<float>(safe_width) - panel_width)
            * 0.5F;
        const float panel_top = (static_cast<float>(safe_height) - panel_height)
            * 0.5F;
        glColor4f(0.03F, 0.04F, 0.08F, 0.94F);
        draw_hud_box(panel_left, panel_top,
                     panel_left + panel_width, panel_top + panel_height);
        glColor3f(1.0F, 0.82F, 0.22F);
        draw_hud_text("MATCH OVER", panel_left + 28.0F * ui_scale,
                      panel_top + 18.0F * ui_scale, ui_scale);
        glColor3f(0.78F, 0.84F, 0.92F);
        draw_hud_text("WAITING FOR NEXT MAP", panel_left + 28.0F * ui_scale,
                      panel_top + 42.0F * ui_scale, ui_scale);
    }

    if (g_paused && !g_scan_dialog_active) {
        const float panel_width = 220.0F * ui_scale;
        const float panel_height = 68.0F * ui_scale;
        const float panel_left = (static_cast<float>(safe_width) - panel_width)
            * 0.5F;
        const float panel_top = (static_cast<float>(safe_height) - panel_height)
            * 0.5F;
        glColor4f(0.02F, 0.025F, 0.05F, 0.96F);
        draw_hud_box(panel_left, panel_top,
                     panel_left + panel_width, panel_top + panel_height);
        glColor3f(0.98F, 0.88F, 0.28F);
        draw_hud_text("PAUSED", panel_left + 58.0F * ui_scale,
                      panel_top + 18.0F * ui_scale, ui_scale);
        glColor3f(0.78F, 0.84F, 0.92F);
        draw_hud_text("ESC RESUME", panel_left + 40.0F * ui_scale,
                      panel_top + 42.0F * ui_scale, ui_scale);
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void configure_scene_render_state() {
    const auto& render_options = fruityprime::mods::render::options();
    if (render_options.lighting()) {
        // The managed shader keeps ambient/specular as material uniforms and
        // only uses the current vertex colour for the diffuse override.  The
        // native fixed-function equivalent therefore tracks diffuse only;
        // ambient and specular are set per primitive in draw_primitive().
        // The lights themselves are placed after the view matrix, in
        // apply_scene_lights(), because a direction set before it would be
        // interpreted in the wrong space.
        glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_LIGHTING);
    } else {
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
        glDisable(GL_COLOR_MATERIAL);
    }

    if (render_options.fog()) {
        constexpr GLfloat fog_color[] = {0.035F, 0.045F, 0.075F, 1.0F};
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fog_color);
        glFogf(GL_FOG_START, 35.0F);
        glFogf(GL_FOG_END, 180.0F);
        glEnable(GL_FOG);
    } else {
        glDisable(GL_FOG);
    }
}

void draw_projectiles() {
    if (!g_session.has_value() || g_session->projectiles().empty()) {
        return;
    }

    const auto to_render_vector = [](fruityprime::net::Vec3 value) {
        return fruityprime::formats::Vector3{value.x, value.y, value.z};
    };
    const auto distance = [](fruityprime::net::Vec3 left,
                             fruityprime::net::Vec3 right) {
        const float x = left.x - right.x;
        const float y = left.y - right.y;
        const float z = left.z - right.z;
        return std::sqrt(x * x + y * y + z * z);
    };

    const auto draw_model_projectile = [&](const EntityRenderModel& resource,
                                           const fruityprime::runtime::BeamProjectile& projectile,
                                           bool elongate) {
        if (resource.model == nullptr || !resource.instance.has_value()) {
            return false;
        }
        fruityprime::formats::Vector3 direction = to_render_vector(
            projectile.direction).normalized();
        if (direction.length_squared() < 0.0001F) {
            direction = {0.0F, 0.0F, 1.0F};
        }
        fruityprime::formats::Vector3 up{0.0F, 1.0F, 0.0F};
        if (std::abs(fruityprime::formats::dot(direction, up)) > 0.95F) {
            up = {1.0F, 0.0F, 0.0F};
        }
        const auto transform = fruityprime::formats::MatrixOps::get_transform4(
            direction, up, to_render_vector(projectile.position));
        const GLfloat matrix[16]{
            transform.m11, transform.m12, transform.m13, transform.m14,
            transform.m21, transform.m22, transform.m23, transform.m24,
            transform.m31, transform.m32, transform.m33, transform.m34,
            transform.m41, transform.m42, transform.m43, transform.m44};
        glEnable(GL_TEXTURE_2D);
        glPushMatrix();
        glMultMatrixf(matrix);
        if (elongate) {
            glScalef(1.0F, std::max(0.25F,
                                    distance(projectile.position,
                                             projectile.back_position)),
                     1.0F);
        }
        draw_model_layer(DrawLayer::Entity, NoHunter, &*resource.instance,
                         &resource);
        glPopMatrix();
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    };

    const auto setup_trail_texture = [&](const EntityRenderModel& resource,
                                         TextureImage*& image,
                                         const fruityprime::model::Material*& material) {
        if (resource.model == nullptr || resource.model->materials().empty()) {
            return false;
        }
        material = &resource.model->materials().front();
        std::size_t texture = NoTexture;
        try {
            texture = cache_texture(*resource.model, *material);
        } catch (const std::exception&) {
            return false;
        }
        if (texture == NoTexture || texture >= g_texture_images.size()) {
            return false;
        }
        image = &g_texture_images[texture];
        upload_gl_texture(*image);
        glBindTexture(GL_TEXTURE_2D, image->gl_id);
        const auto repeat_mode = [](std::uint8_t value) -> GLint {
            switch (static_cast<fruityprime::formats::RepeatMode>(
                std::min<std::uint8_t>(value, 2))) {
            case fruityprime::formats::RepeatMode::Clamp:
                return static_cast<GLint>(0x812f); // GL_CLAMP_TO_EDGE
            case fruityprime::formats::RepeatMode::Repeat:
                return static_cast<GLint>(GL_REPEAT);
            case fruityprime::formats::RepeatMode::Mirror:
                return static_cast<GLint>(0x8370); // GL_MIRRORED_REPEAT
            }
            return static_cast<GLint>(0x812f);
        };
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        repeat_mode(material->x_repeat));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        repeat_mode(material->y_repeat));
        return true;
    };

    const auto trail_alpha = [](const fruityprime::runtime::BeamProjectile& projectile) {
        const float lifetime = fruityprime::metadata::weapon_info(
            projectile.weapon).lifetime_seconds;
        const float remaining = std::max(0.0F, lifetime - projectile.lifetime);
        // BeamProjectileEntity uses clamp(Lifespan * 30 * 8, 0, 31) / 31.
        return std::clamp(remaining * 240.0F / 31.0F, 0.0F, 1.0F);
    };

    const auto draw_trail_single = [&](const EntityRenderModel& resource,
                                       const fruityprime::runtime::BeamProjectile& projectile,
                                       fruityprime::formats::Vector3 head,
                                       fruityprime::formats::Vector3 tail,
                                       float height,
                                       bool quarter_texture) {
        TextureImage* image = nullptr;
        const fruityprime::model::Material* material = nullptr;
        if (!setup_trail_texture(resource, image, material)) {
            return false;
        }
        const float texture_u = image->width > 0
            ? std::max(0.0F, (static_cast<float>(image->width) - 1.0F / 16.0F)
                / static_cast<float>(image->width))
            : 1.0F;
        const float texture_v = image->height > 0
            ? std::max(0.0F,
                ((quarter_texture ? static_cast<float>(image->height) / 4.0F
                                   : static_cast<float>(image->height))
                    - 1.0F / 16.0F)
                / static_cast<float>(image->height))
            : 1.0F;
        const float alpha = trail_alpha(projectile);
        glColor4f(projectile.color.x, projectile.color.y, projectile.color.z,
                  alpha);
        // TrailSingle is not a camera billboard in the managed renderer.  Its
        // vertices use world Y for the two sides and a translation at the
        // stored tail point.  Keep that distinction from Particle/B8 quads.
        glBegin(GL_QUAD_STRIP);
        glTexCoord2f(0.0F, 0.0F);
        glVertex3f(head.x, head.y - height, head.z);
        glTexCoord2f(0.0F, texture_v);
        glVertex3f(head.x, head.y + height, head.z);
        glTexCoord2f(texture_u, 0.0F);
        glVertex3f(tail.x, tail.y - height, tail.z);
        glTexCoord2f(texture_u, texture_v);
        glVertex3f(tail.x, tail.y + height, tail.z);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    };

    const auto draw_trail_multi = [&](const EntityRenderModel& resource,
                                      const fruityprime::runtime::BeamProjectile& projectile,
                                      float height, std::size_t segments) {
        segments = std::clamp<std::size_t>(segments, 2,
                                            projectile.past_positions.size() / 2);
        TextureImage* image = nullptr;
        const fruityprime::model::Material* material = nullptr;
        if (!setup_trail_texture(resource, image, material)) {
            return false;
        }
        const float texture_v = image->height > 0
            ? std::max(0.0F, (static_cast<float>(image->height) - 1.0F / 16.0F)
                / static_cast<float>(image->height))
            : 1.0F;
        const float alpha = trail_alpha(projectile);
        glColor4f(projectile.color.x, projectile.color.y, projectile.color.z,
                  alpha);
        glBegin(GL_QUAD_STRIP);
        for (std::size_t segment = 0; segment < segments; ++segment) {
            const auto& point = projectile.past_positions[segment * 2];
            float texture_u = 0.0F;
            if (segment > 0 && image->width > 0) {
                texture_u = std::max(0.0F,
                    (static_cast<float>(image->width)
                        / static_cast<float>(segments - 1)
                        * static_cast<float>(segment) - 1.0F / 16.0F)
                    / static_cast<float>(image->width));
            }
            glTexCoord2f(texture_u, 0.0F);
            glVertex3f(point.x, point.y - height, point.z);
            glTexCoord2f(texture_u, texture_v);
            glVertex3f(point.x, point.y + height, point.z);
        }
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    };

    const auto draw_trail_shock = [&](const EntityRenderModel& resource,
                                      const fruityprime::runtime::BeamProjectile& projectile,
                                      float height, float range,
                                      std::size_t segments) {
        if (segments < 2) {
            return false;
        }
        TextureImage* image = nullptr;
        const fruityprime::model::Material* material = nullptr;
        if (!setup_trail_texture(resource, image, material)) {
            return false;
        }
        const float texture_v = image->height > 0
            ? std::max(0.0F, (static_cast<float>(image->height) - 1.0F / 16.0F)
                / static_cast<float>(image->height))
            : 1.0F;
        const auto& origin = projectile.past_positions[8];
        const fruityprime::net::Vec3 vector{
            projectile.position.x - origin.x,
            projectile.position.y - origin.y,
            projectile.position.z - origin.z};
        const int frames = static_cast<int>(g_session->tick_count() / 2);
        std::uint32_t rng = static_cast<std::uint32_t>(
            frames + static_cast<int>(projectile.position.x * 4096.0F));
        const std::uint32_t random_range = static_cast<std::uint32_t>(
            std::max(0.0F, std::round(range * 4096.0F)));
        const float half_range = range / 2.0F;
        const auto random_offset = [&]() {
            return static_cast<float>(fruityprime::utility::Rng::call(
                rng, random_range)) / 4096.0F - half_range;
        };
        glColor4f(projectile.color.x, projectile.color.y, projectile.color.z,
                  1.0F);
        glBegin(GL_QUAD_STRIP);
        for (std::size_t segment = 0; segment < segments; ++segment) {
            const int factor = (frames & 15) + static_cast<int>(segment);
            float texture_u = 0.0F;
            if (factor > 0 && image->width > 0) {
                texture_u = std::max(0.0F,
                    (2.0F * static_cast<float>(image->width)
                        / static_cast<float>(segments - 1)
                        * static_cast<float>(factor) - 1.0F / 16.0F)
                    / static_cast<float>(image->width));
            }
            const float pct = static_cast<float>(segment)
                / static_cast<float>(segments - 1);
            float x = vector.x * pct
                + projectile.direction.x * projectile.speed / 60.0F
                    * 0.25F * pct * (1.0F - pct);
            float y = vector.y * pct
                + projectile.direction.y * projectile.speed / 60.0F
                    * 0.25F * pct * (1.0F - pct);
            float z = vector.z * pct
                + projectile.direction.z * projectile.speed / 60.0F
                    * 0.25F * pct * (1.0F - pct);
            if (segment > 0 && segment + 1 < segments) {
                x += random_offset();
                y += random_offset();
                z += random_offset();
            }
            glTexCoord2f(texture_u, 0.0F);
            glVertex3f(origin.x + x, origin.y + y - height, origin.z + z);
            glTexCoord2f(texture_u, texture_v);
            glVertex3f(origin.x + x, origin.y + y + height, origin.z + z);
        }
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    };

    glDisable(GL_LIGHTING);
    for (const auto& projectile : g_session->projectiles()) {
        bool drawn = false;
        if (projectile.draw_func_id == 3
            && g_projectile_models[ProjectileIceShard] != nullptr) {
            drawn = draw_model_projectile(
                *g_projectile_models[ProjectileIceShard], projectile, false);
            const auto* trail = g_projectile_models[ProjectileTrail];
            if (trail != nullptr) {
                drawn = draw_trail_multi(*trail, projectile,
                                         204.0F / 4096.0F, 5) || drawn;
            }
        } else if (projectile.draw_func_id == 17
                   && g_projectile_models[ProjectileEnergyBeam] != nullptr) {
            drawn = draw_model_projectile(
                *g_projectile_models[ProjectileEnergyBeam], projectile, true);
        } else {
            std::size_t resource_index = ProjectileTrail;
            if (projectile.draw_func_id == 1
                || projectile.draw_func_id == 2) {
                resource_index = ProjectileElectroTrail;
            } else if (projectile.draw_func_id == 9) {
                resource_index = ProjectileArcWelder;
            }
            const auto* resource = g_projectile_models[resource_index];
            if (resource != nullptr) {
                switch (projectile.draw_func_id) {
                case 0:
                    drawn = draw_trail_single(
                        *resource, projectile,
                        to_render_vector(projectile.position),
                        to_render_vector(projectile.back_position),
                        122.0F / 4096.0F, false);
                    break;
                case 1:
                    drawn = draw_trail_single(
                        *resource, projectile,
                        to_render_vector(projectile.position),
                        to_render_vector(projectile.back_position),
                        614.0F / 4096.0F, false);
                    break;
                case 2:
                    drawn = draw_trail_multi(*resource, projectile,
                                             1024.0F / 4096.0F, 5);
                    break;
                case 6:
                case 12: {
                    drawn = draw_trail_single(
                        *resource, projectile,
                        to_render_vector(projectile.past_positions[0]),
                        to_render_vector(projectile.past_positions[8]),
                        204.0F / 4096.0F, true);
                    break;
                }
                case 7:
                    drawn = draw_trail_multi(*resource, projectile,
                                             204.0F / 4096.0F, 5);
                    break;
                case 9: {
                    const bool has_target = (projectile.flags
                        & fruityprime::runtime::BeamProjectile::Homing) != 0;
                    const bool local_projectile = projectile.owner_slot
                        == g_local_slot;
                    if (has_target || local_projectile) {
                        drawn = draw_trail_shock(
                            *resource, projectile,
                            has_target ? 0.15F : 0.025F,
                            has_target ? 0.5F : 0.35F,
                            has_target ? 10 : 5);
                    }
                    break;
                }
                case 10:
                    drawn = draw_trail_multi(*resource, projectile,
                                             81.0F / 4096.0F, 2);
                    break;
                default:
                    break;
                }
            }
        }
        if (!drawn) {
            glDisable(GL_TEXTURE_2D);
            glColor4f(projectile.color.x, projectile.color.y,
                      projectile.color.z, 0.95F);
            glPushMatrix();
            glTranslatef(projectile.position.x, projectile.position.y,
                         projectile.position.z);
            glBegin(GL_LINES);
            glVertex3f(-projectile.direction.x * 0.15F,
                       -projectile.direction.y * 0.15F,
                       -projectile.direction.z * 0.15F);
            glVertex3f(projectile.direction.x * 0.35F,
                       projectile.direction.y * 0.35F,
                       projectile.direction.z * 0.35F);
            glEnd();
            glPopMatrix();
        }
    }
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_2D);
}

void draw_effects() {
    if (!g_session.has_value()) {
        return;
    }

    const auto to_render_vector = [](fruityprime::net::Vec3 value) {
        return fruityprime::formats::Vector3{value.x, value.y, value.z};
    };
    const auto effect_transform = [&](const fruityprime::gameplay::EffectState& effect) {
        auto direction = to_render_vector(effect.direction);
        if (direction.length_squared() < 0.0001F) {
            direction = {0.0F, 0.0F, 1.0F};
        } else {
            direction = direction.normalized();
        }
        auto up = to_render_vector(effect.up);
        if (up.length_squared() < 0.0001F) {
            up = {0.0F, 1.0F, 0.0F};
        } else {
            up = up.normalized();
        }
        if (std::abs(fruityprime::formats::dot(direction, up)) > 0.95F) {
            up = {1.0F, 0.0F, 0.0F};
        }
        return fruityprime::formats::MatrixOps::get_transform4(
            direction, up, to_render_vector(effect.position));
    };

    // Gameplay emits a short-lived event, while the cartridge effect owns
    // the actual element/particle lifetime.  Start each event once and let
    // the native runtime keep children alive after the source event expires.
    const float effect_time = static_cast<float>(g_session->tick_count())
        * (1.0F / 60.0F);
    if (g_effect_runtime != nullptr) {
        for (const auto& effect : g_session->effects()) {
            if (effect.detached) {
                g_effect_runtime->detach(effect.id,
                                          effect.detached_set_expired);
                continue;
            }
            if (g_effect_runtime_seen.find(effect.id)
                != g_effect_runtime_seen.end()) {
                if (effect.persistent) {
                    g_effect_runtime->set_transform(
                        effect.id, effect_transform(effect));
                }
                continue;
            }
            if (effect.effect_id >= g_effect_resources.size()
                || !g_effect_resources[effect.effect_id].has_value()) {
                continue;
            }
            fruityprime::effects::SpawnRequest request;
            request.instance_id = effect.id;
            request.effect_id = effect.effect_id;
            request.source_entity_id = effect.source_entity_id;
            request.transform = effect_transform(effect);
            request.owner_lifespan = effect.lifespan_seconds;
            request.element_extension = effect.element_extension;
            if (g_effect_runtime->spawn(
                    g_effect_resources[effect.effect_id].value(), request,
                    effect_time)) {
                g_effect_runtime_seen.insert(effect.id);
            }
        }
        g_effect_runtime->update(1.0F / 60.0F, effect_time);
        // EffectEntry.IsFinished is a property of the expanded element graph,
        // not of the short gameplay event.  Persistent native records use the
        // same boundary: once their runtime instance has drained, detach them
        // with setExpired=false so particles can finish without leaving a
        // stale gameplay handle attached to a linked enemy.
        for (const auto& effect : g_session->effects()) {
            if (!effect.persistent || effect.detached
                || g_effect_runtime_seen.find(effect.id)
                    == g_effect_runtime_seen.end()
                || g_effect_runtime->has_instance(effect.id)) {
                continue;
            }
            g_session->detach_effect_from_runtime(effect.id, false);
        }
    }
    if (g_session->effects().empty()
        && (g_effect_runtime == nullptr
            || g_effect_runtime->instances().empty())) {
        return;
    }

    GLfloat view[16]{};
    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    fruityprime::formats::Vector3 right{view[0], view[1], view[2]};
    fruityprime::formats::Vector3 up{view[4], view[5], view[6]};
    if (right.length_squared() < 0.0001F
        || up.length_squared() < 0.0001F) {
        right = {1.0F, 0.0F, 0.0F};
        up = {0.0F, 1.0F, 0.0F};
    } else {
        right = right.normalized();
        up = up.normalized();
    }

    const auto color_for_effect = [](std::uint16_t id) {
        // These families follow Metadata.Effects' weapon naming. The actual
        // particle colors are still supplied by the parsed effect file once
        // the full particle action VM is connected; this keeps the fallback
        // deterministic and readable on ROMs with an optional effect absent.
        if (id == 59 || id == 78 || id == 96 || id == 123
            || id == 126 || id == 131 || id == 132 || id == 133
            || id == 166 || id == 217) {
            return std::array<float, 3>{0.35F, 0.78F, 1.0F};
        }
        if (id == 57 || id == 85 || id == 86 || id == 137
            || id == 138 || id == 171 || id == 237) {
            return std::array<float, 3>{0.35F, 0.95F, 1.0F};
        }
        if (id == 60 || id == 61 || id == 63 || id == 64
            || id == 89 || id == 90 || id == 94 || id == 95
            || id == 130 || id == 134 || id == 176 || id == 183
            || id == 193 || id == 194 || id == 211) {
            return std::array<float, 3>{1.0F, 0.58F, 0.18F};
        }
        return std::array<float, 3>{1.0F, 0.88F, 0.42F};
    };

    const auto draw_model_effect = [to_render_vector](
      const EntityRenderModel& resource,
      const fruityprime::gameplay::EffectState& effect, float scale) {
        if (resource.model == nullptr || !resource.instance.has_value()) {
            return false;
        }
        auto direction = to_render_vector(effect.direction);
        if (direction.length_squared() < 0.0001F) {
            direction = {0.0F, 0.0F, 1.0F};
        } else {
            direction = direction.normalized();
        }
        auto up = to_render_vector(effect.up);
        if (up.length_squared() < 0.0001F) {
            up = {0.0F, 1.0F, 0.0F};
        } else {
            up = up.normalized();
        }
        if (std::abs(fruityprime::formats::dot(direction, up)) > 0.95F) {
            up = {1.0F, 0.0F, 0.0F};
        }
        const auto transform = fruityprime::formats::MatrixOps::get_transform4(
            direction, up, to_render_vector(effect.position));
        const GLfloat matrix[16]{
            transform.m11, transform.m12, transform.m13, transform.m14,
            transform.m21, transform.m22, transform.m23, transform.m24,
            transform.m31, transform.m32, transform.m33, transform.m34,
            transform.m41, transform.m42, transform.m43, transform.m44};
        glEnable(GL_TEXTURE_2D);
        glPushMatrix();
        glMultMatrixf(matrix);
        glScalef(scale, scale, scale);
        draw_model_layer(DrawLayer::Entity, NoHunter, &*resource.instance,
                         &resource);
        glPopMatrix();
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    };

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto has_effect_flag = [](fruityprime::formats::EffElemFlags value,
                                    fruityprime::formats::EffElemFlags flag) {
        return (static_cast<std::uint32_t>(value)
                & static_cast<std::uint32_t>(flag)) != 0;
    };
    const auto find_runtime_instance =
        [](const std::vector<fruityprime::effects::InstanceState>& instances,
           std::uint32_t id) -> const fruityprime::effects::InstanceState* {
        const auto found = std::find_if(
            instances.begin(), instances.end(),
            [id](const fruityprime::effects::InstanceState& instance) {
                return instance.id == id;
            });
        return found == instances.end() ? nullptr : &*found;
    };

    // Particle action state is now the renderer's source of truth.  Mesh
    // particles use the parsed model/node resource.  The non-mesh path below
    // mirrors Effects.cs' B8/C4/CC/D0 vertex construction instead of reducing
    // every draw function to one generic marker.
    if (g_effect_runtime != nullptr) {
        const fruityprime::formats::Vector3 view_direction{
            view[8], view[9], view[10]};
        const auto camera_forward = view_direction.length_squared() > 0.0001F
            ? view_direction.normalized()
            : fruityprime::formats::Vector3{0.0F, 0.0F, 1.0F};
        const auto gl_repeat_mode = [](
            fruityprime::formats::RepeatMode mode) -> GLint {
            switch (mode) {
            case fruityprime::formats::RepeatMode::Clamp:
                return static_cast<GLint>(0x812f); // GL_CLAMP_TO_EDGE
            case fruityprime::formats::RepeatMode::Repeat:
                return static_cast<GLint>(GL_REPEAT);
            case fruityprime::formats::RepeatMode::Mirror:
                return static_cast<GLint>(0x8370); // GL_MIRRORED_REPEAT
            }
            return static_cast<GLint>(0x812f);
        };

        const auto find_particle_binding_for_particle = [&](
            const fruityprime::effects::ElementState& element,
            const fruityprime::effects::ParticleState& particle)
            -> std::optional<EffectParticleBinding> {
            if (element.definition == nullptr
                || particle.particle_id
                    >= element.definition->particle_names.size()) {
                return std::nullopt;
            }
            return find_particle_binding(
                element.definition->model_name,
                element.definition->particle_names[particle.particle_id]);
        };

        const auto draw_mesh_particle = [&](
            const fruityprime::effects::ElementState& element,
            const fruityprime::effects::ParticleState& particle,
            const std::array<float, 3>& base_color,
            float alpha) {
            if (particle.functions.draw != 7) {
                return false;
            }
            const auto binding = find_particle_binding_for_particle(
                element, particle);
            if (!binding.has_value()
                || binding->render_model == nullptr
                || binding->render_model->instance == std::nullopt) {
                return false;
            }
            const auto& render_model = *binding->render_model;
            const auto node_id = binding->node_id;
            const bool has_geometry = std::any_of(
                g_draw_primitives.begin(), g_draw_primitives.end(),
                [&render_model, node_id](const DrawPrimitive& primitive) {
                    return primitive.layer == DrawLayer::Entity
                        && primitive.entity_model == &render_model
                        && primitive.node_id == node_id;
                });
            if (!has_geometry || !std::isfinite(particle.scale)
                || std::abs(particle.scale) <= 0.0001F) {
                return false;
            }

            auto center = particle.position;
            if (has_effect_flag(
                    element.flags,
                    fruityprime::formats::EffElemFlags::UseTransform)) {
                // DrawDC uses Position + Owner.Transform.Row3 for mesh
                // particles.  The element rotation is part of the particle's
                // own mesh orientation, not its billboard-space translation.
                center += {element.transform.m41, element.transform.m42,
                           element.transform.m43};
            }
            const std::array<float, 4> tint{
                std::clamp(base_color[0] * particle.red, 0.0F, 1.0F),
                std::clamp(base_color[1] * particle.green, 0.0F, 1.0F),
                std::clamp(base_color[2] * particle.blue, 0.0F, 1.0F),
                alpha};
            glEnable(GL_TEXTURE_2D);
            glPushMatrix();
            if (particle.functions.set_vecs == 4) {
                auto facing = particle.speed;
                if (facing.length_squared() < 0.0001F) {
                    facing = camera_forward;
                } else {
                    facing = facing.normalized();
                }
                auto orientation_up = camera_forward;
                if (std::abs(fruityprime::formats::dot(
                        facing, orientation_up)) > 0.95F) {
                    orientation_up = right;
                }
                const auto transform =
                    fruityprime::formats::MatrixOps::get_transform4(
                        facing, orientation_up.normalized(), center);
                const GLfloat matrix[16]{
                    transform.m11, transform.m12, transform.m13, transform.m14,
                    transform.m21, transform.m22, transform.m23, transform.m24,
                    transform.m31, transform.m32, transform.m33, transform.m34,
                    transform.m41, transform.m42, transform.m43, transform.m44};
                glMultMatrixf(matrix);
            } else {
                glTranslatef(center.x, center.y, center.z);
            }
            const float scale = std::abs(particle.scale);
            glScalef(scale, scale, scale);
            draw_model_layer(DrawLayer::Entity, NoHunter,
                             &*render_model.instance, &render_model, node_id,
                             &tint);
            glPopMatrix();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
            return true;
        };

        for (const auto& instance : g_effect_runtime->instances()) {
            const auto base_color = color_for_effect(instance.effect_id);
            for (const auto& element : instance.elements) {
                for (const auto& particle : element.particles) {
                    if (!std::isfinite(particle.scale)
                        || !std::isfinite(particle.alpha)) {
                        continue;
                    }
                    const float alpha = std::clamp(
                        particle.alpha, 0.0F, 1.0F);
                    if (alpha <= 0.0F) {
                        continue;
                    }
                    if (draw_mesh_particle(element, particle, base_color,
                                           alpha)) {
                        continue;
                    }

                    const bool use_transform = has_effect_flag(
                        element.flags,
                        fruityprime::formats::EffElemFlags::UseTransform);
                    const auto transform_direction =
                        [&](fruityprime::formats::Vector3 value) {
                            return use_transform
                                ? fruityprime::formats::MatrixOps::vec3_mult_mtx3(
                                    value, element.transform)
                                : value;
                        };
                    const auto center = use_transform
                        ? fruityprime::formats::MatrixOps::vec3_mult_mtx4(
                            particle.position, element.transform)
                        : particle.position;

                    // Effects.cs uses one of four geometry families:
                    // B8 is a square, CC is a rotated square, and C4/D0 are
                    // the two shared ribbon builders.  Keep the edge vectors
                    // explicit so transform ownership and billboard mode do
                    // not get conflated.
                    fruityprime::formats::Vector3 edge_a{};
                    fruityprime::formats::Vector3 edge_b{};
                    const float scale = particle.scale;
                    switch (particle.functions.draw) {
                    case 1:
                    case 2: {
                        if (particle.functions.set_vecs == 1) {
                            // SetVecsB0 is a spherical billboard.  The native
                            // fixed-function host supplies the view vectors
                            // that the managed shader applies later.
                            edge_a = right * scale;
                            edge_b = up * scale;
                        } else {
                            // SetVecsBC: unit X by unit Z, optionally under
                            // the element transform.
                            edge_a = transform_direction({1.0F, 0.0F, 0.0F})
                                * scale;
                            edge_b = transform_direction({0.0F, 0.0F, 1.0F})
                                * scale;
                        }
                        break;
                    }
                    case 4:
                    case 5: {
                        fruityprime::formats::Vector3 basis_a =
                            particle.functions.set_vecs == 1
                            ? right : transform_direction(
                                {1.0F, 0.0F, 0.0F});
                        fruityprime::formats::Vector3 basis_b =
                            particle.functions.set_vecs == 1
                            ? up : transform_direction(
                                {0.0F, 0.0F, 1.0F});
                        const float rotation = std::isfinite(
                            particle.rotation) ? particle.rotation : 0.0F;
                        const float angle1 = rotation * 3.14159265358979323846F
                            / 180.0F;
                        const float angle2 = angle1
                            + 90.0F * 3.14159265358979323846F / 180.0F;
                        const float sin1 = std::sin(angle1);
                        const float cos1 = std::cos(angle1);
                        const float sin2 = std::sin(angle2);
                        const float cos2 = std::cos(angle2);
                        edge_a = (basis_a * sin2 + basis_b * cos2) * scale;
                        edge_b = (basis_a * sin1 + basis_b * cos1) * scale;
                        break;
                    }
                    case 3:
                    case 6: {
                        // DrawC4 suppresses a zero-speed trail.  The fixed
                        // value is 128/4096 in the managed implementation.
                        if (particle.functions.draw == 3
                            && particle.speed.length_squared()
                                <= 128.0F / 4096.0F) {
                            continue;
                        }
                        fruityprime::formats::Vector3 basis_a{};
                        fruityprime::formats::Vector3 basis_b{};
                        if (particle.functions.set_vecs == 4) {
                            basis_a = particle.speed.length_squared()
                                > 0.0001F
                                ? particle.speed.normalized()
                                : camera_forward;
                            basis_b = camera_forward;
                        } else {
                            // SetVecsC0: -Y by the view's positive Z.
                            basis_a = {0.0F, 1.0F, 0.0F};
                            basis_b = camera_forward;
                        }
                        edge_a = transform_direction(basis_a) * scale;
                        auto cross_edge = fruityprime::formats::cross(
                            basis_a, basis_b);
                        if (cross_edge.length_squared() > 0.0001F) {
                            cross_edge = cross_edge.normalized();
                        }
                        const float rotation = std::isfinite(
                            particle.rotation) ? particle.rotation : 0.0F;
                        edge_b = transform_direction(cross_edge) * rotation;
                        break;
                    }
                    default:
                        // Keep a malformed/unknown cartridge draw record
                        // visible as a small billboard, matching the native
                        // parser's non-fatal resource policy.
                        edge_a = right * scale;
                        edge_b = up * scale;
                        break;
                    }

                    const auto first = center - edge_a * 0.5F
                        + edge_b * 0.5F;
                    const auto second = first + edge_a;
                    const auto third = second - edge_b;
                    const auto fourth = third - edge_a;

                    std::size_t particle_texture = NoTexture;
                    float texture_scale_s = 1.0F;
                    float texture_scale_t = 1.0F;
                    auto particle_x_repeat =
                        fruityprime::formats::RepeatMode::Clamp;
                    auto particle_y_repeat =
                        fruityprime::formats::RepeatMode::Clamp;
                    if (const auto binding = find_particle_binding_for_particle(
                            element, particle); binding.has_value()) {
                        const auto* render_model = binding->render_model;
                        const auto& model = *render_model->model;
                        const auto node_id = binding->node_id;
                        if (node_id >= 0
                            && static_cast<std::size_t>(node_id)
                                < model.nodes().size()) {
                            const auto& node = model.nodes()[
                                static_cast<std::size_t>(node_id)];
                            const std::size_t mesh_start = node.mesh_id / 2;
                            if (mesh_start < model.meshes().size()) {
                                const auto& mesh = model.meshes()[mesh_start];
                                if (mesh.material_id < model.materials().size()) {
                                    const auto& material = model.materials()[
                                        mesh.material_id];
                                    try {
                                        particle_texture = cache_texture(
                                            model, material);
                                    } catch (const std::exception&) {
                                        // A missing optional particle texture
                                        // should leave the colored quad path
                                        // usable for extracted development
                                        // trees.
                                    }
                                    particle_x_repeat = static_cast<
                                        fruityprime::formats::RepeatMode>(
                                            material.x_repeat);
                                    particle_y_repeat = static_cast<
                                        fruityprime::formats::RepeatMode>(
                                            material.y_repeat);
                                    if (material.x_repeat
                                        == static_cast<std::uint8_t>(
                                            fruityprime::formats::RepeatMode::Mirror)) {
                                        texture_scale_s =
                                            material.scale_s.to_float();
                                    }
                                    if (material.y_repeat
                                        == static_cast<std::uint8_t>(
                                            fruityprime::formats::RepeatMode::Mirror)) {
                                        texture_scale_t =
                                            material.scale_t.to_float();
                                    }
                                }
                            }
                        }
                    }
                    const bool textured = particle_texture != NoTexture
                        && particle_texture < g_texture_images.size();
                    if (textured) {
                        auto& image = g_texture_images[particle_texture];
                        upload_gl_texture(image);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, image.gl_id);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                        gl_repeat_mode(particle_x_repeat));
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                        gl_repeat_mode(particle_y_repeat));
                    } else {
                        glDisable(GL_TEXTURE_2D);
                    }
                    glColor4f(
                        std::clamp(base_color[0] * particle.red, 0.0F, 1.0F),
                        std::clamp(base_color[1] * particle.green, 0.0F, 1.0F),
                        std::clamp(base_color[2] * particle.blue, 0.0F, 1.0F),
                        alpha);
                    glBegin(GL_QUADS);
                    if (textured) {
                        glTexCoord2f(0.0F, 0.0F);
                    }
                    glVertex3f(first.x, first.y, first.z);
                    if (textured) {
                        glTexCoord2f(0.0F, texture_scale_t);
                    }
                    glVertex3f(second.x, second.y, second.z);
                    if (textured) {
                        glTexCoord2f(texture_scale_s, texture_scale_t);
                    }
                    glVertex3f(third.x, third.y, third.z);
                    if (textured) {
                        glTexCoord2f(texture_scale_s, 0.0F);
                    }
                    glVertex3f(fourth.x, fourth.y, fourth.z);
                    glEnd();
                    if (textured) {
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glDisable(GL_TEXTURE_2D);
                    }
                }
            }
        }
    }

    for (const auto& effect : g_session->effects()) {
        if (effect.detached) {
            continue;
        }
        const auto* runtime_instance = g_effect_runtime == nullptr
            ? nullptr
            : find_runtime_instance(g_effect_runtime->instances(), effect.id);
        const bool runtime_has_particles = runtime_instance != nullptr
            && std::any_of(
                runtime_instance->elements.begin(),
                runtime_instance->elements.end(),
                [](const fruityprime::effects::ElementState& element) {
                    return !element.particles.empty();
                });
        const float life_ratio = effect.persistent ? 1.0F
            : effect.lifespan_seconds > 0.0F
            ? std::clamp(effect.remaining_seconds / effect.lifespan_seconds,
                        0.0F, 1.0F)
            : 0.0F;
        const auto* info = fruityprime::metadata::effect_info(
            effect.effect_id);
        const auto* resource = effect.effect_id
                < g_effect_resources.size()
            ? &g_effect_resources[effect.effect_id] : nullptr;
        const std::size_t element_count = resource != nullptr
                && resource->has_value()
            ? resource->value().elements().size() : 0;
        const float resource_scale = element_count == 0
            ? 1.0F : std::clamp(1.0F
                + static_cast<float>(element_count) * 0.08F, 1.0F, 1.7F);
        float evaluated_scale = 1.0F;
        float evaluated_alpha = 1.0F;
        auto color = color_for_effect(effect.effect_id);
        if (resource != nullptr && resource->has_value()) {
            // Use a private stream for visual evaluation. The managed effect
            // VM consumes its gameplay RNG, but a missing optional resource
            // must never perturb player/projectile simulation in native mode.
            fruityprime::utility::Rng effect_rng(
                fruityprime::utility::Rng::Rng1StartValue
                    ^ (effect.id * 0x9e3779b9u),
                effect.id ^ 0xa511e9b3u);
            fruityprime::effects::Evaluator evaluator(
                resource->value(), effect_rng);
            const fruityprime::effects::TimeValues times{
                std::max(0.0F, effect.lifespan_seconds
                    - effect.remaining_seconds),
                std::max(0.0F, effect.lifespan_seconds
                    - effect.remaining_seconds),
                effect.lifespan_seconds};
            fruityprime::effects::EvaluationState state;
            state.position = {effect.position.x, effect.position.y,
                               effect.position.z};
            state.transform_position = state.position;
            state.owner_lifespan = effect.lifespan_seconds;
            for (const auto& element : resource->value().elements()) {
                const auto evaluate_action =
                    [&](fruityprime::effects::FuncAction action,
                        float& destination) {
                        const auto found = element.actions.find(
                            static_cast<std::uint32_t>(action));
                        if (found == element.actions.end()) {
                            return false;
                        }
                        return evaluator.evaluate_float(
                            found->second, times, state, destination);
                    };
                float value = 0.0F;
                if (evaluate_action(
                        fruityprime::effects::FuncAction::SetParticleScale,
                        value) && std::isfinite(value)) {
                    evaluated_scale = std::clamp(std::abs(value), 0.05F, 4.0F);
                }
                if (evaluate_action(
                        fruityprime::effects::FuncAction::SetParticleAlpha,
                        value) && std::isfinite(value)) {
                    evaluated_alpha = std::clamp(value, 0.0F, 1.0F);
                }
                const std::array<fruityprime::effects::FuncAction, 3> color_actions{
                    fruityprime::effects::FuncAction::SetParticleRed,
                    fruityprime::effects::FuncAction::SetParticleGreen,
                    fruityprime::effects::FuncAction::SetParticleBlue};
                for (std::size_t channel = 0; channel < color_actions.size();
                     ++channel) {
                    if (evaluate_action(color_actions[channel], value)
                        && std::isfinite(value)) {
                        color[channel] = std::clamp(value, 0.0F, 1.0F);
                    }
                }
            }
        }
        const float pulse = 0.82F + 0.18F * std::sin(
            static_cast<float>(effect.id & 31u) * 0.7F
            + (1.0F - life_ratio) * 4.0F);
        const float size = 0.11F * resource_scale * evaluated_scale * pulse
            * (0.75F + 0.25F * life_ratio);
        const float alpha = std::clamp(
            life_ratio * 2.5F * evaluated_alpha, 0.0F, 1.0F);
        if (info == nullptr || alpha <= 0.0F) {
            continue;
        }
        // BeamEffectEntity type 0 creates the animated iceWave model and an
        // accompanying effect 78. Prefer the cartridge model when it loaded;
        // the billboard below remains the safe fallback for partial assets.
        if (effect.effect_id == 78
            && g_beam_effect_models[BeamEffectIceWave] != nullptr
            && draw_model_effect(*g_beam_effect_models[BeamEffectIceWave],
                                 effect,
                                 resource_scale * evaluated_scale * 0.65F)) {
            continue;
        }
        if (runtime_has_particles) {
            continue;
        }
        if (effect.persistent && runtime_instance != nullptr) {
            // The cartridge's attached entry has already handed ownership
            // to its particles; do not replace an expired entry with the
            // generic fallback billboard forever.
            continue;
        }
        glColor4f(color[0], color[1], color[2], alpha);
        const fruityprime::formats::Vector3 center{
            effect.position.x, effect.position.y, effect.position.z};
        const auto left = center - right * size;
        const auto right_point = center + right * size;
        const auto top = center + up * size;
        const auto bottom = center - up * size;
        glBegin(GL_QUADS);
        glVertex3f(left.x, left.y, left.z);
        glVertex3f(bottom.x, bottom.y, bottom.z);
        glVertex3f(right_point.x, right_point.y, right_point.z);
        glVertex3f(top.x, top.y, top.z);
        glEnd();
    }
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}

bool sync_gorea_animation(
    fruityprime::model::ModelInstance& instance,
    const fruityprime::gameplay::EnemyState& enemy) {
    using fruityprime::formats::EnemyType;
    const auto type = static_cast<EnemyType>(enemy.enemy_type);
    if (type != EnemyType::Gorea1A && type != EnemyType::Gorea1B
        && type != EnemyType::Gorea2) {
        return false;
    }
    if (!instance.model().animations().any()) {
        return false;
    }

    const int animation = static_cast<int>(enemy.gorea_model_animation);
    const auto flags = enemy.gorea_animation_loop
        ? fruityprime::model::AnimationFlags::None
        : fruityprime::model::AnimationFlags::NoLoop;
    const auto& current = instance.animation_info();
    const bool current_no_loop = fruityprime::model::has_flag(
        current.flags[0], fruityprime::model::AnimationFlags::NoLoop);
    if (current.index[0] != animation || current_no_loop != !enemy.gorea_animation_loop) {
        try {
            instance.set_animation(animation, flags);
        } catch (const std::exception&) {
            // A ROM can carry a controller animation index without a matching
            // optional animation resource. Keep the existing static pose in
            // that case instead of dropping the whole enemy draw.
            return false;
        }
    }
    instance.set_animation_frame(
        0, static_cast<int>(enemy.gorea_animation_frame));
    instance.compute_node_matrices();
    instance.animate_nodes();
    instance.update_matrix_stack();
    instance.update_materials();
    return true;
}

void draw_enemy_catalog_model(
    EntityRenderModel& render_model,
    const fruityprime::gameplay::EnemyState& enemy) {
    if (render_model.model == nullptr || !render_model.instance.has_value()) {
        return;
    }

    // Parent Gorea models contain the visible head, arms, chest sphere, and
    // rope attachment nodes.  Their native controllers advance the timeline
    // during simulation; consume that cursor immediately before drawing the
    // shared catalog instance so every live parent is rendered at its own
    // state/frame instead of all of them remaining on animation 0.
    static_cast<void>(sync_gorea_animation(*render_model.instance, enemy));

    fruityprime::formats::Vector3 facing{
        enemy.facing.x, enemy.facing.y, enemy.facing.z};
    if (facing.length_squared() < 0.0001F) {
        facing = {enemy.velocity.x, enemy.velocity.y, enemy.velocity.z};
    }
    if (facing.length_squared() < 0.0001F) {
        facing = {0.0F, 0.0F, 1.0F};
    }
    const auto transform = fruityprime::formats::MatrixOps::get_transform4(
        facing.normalized(), {0.0F, 1.0F, 0.0F},
        {enemy.position.x, enemy.position.y, enemy.position.z});
    const GLfloat matrix[16]{
        transform.m11, transform.m12, transform.m13, transform.m14,
        transform.m21, transform.m22, transform.m23, transform.m24,
        transform.m31, transform.m32, transform.m33, transform.m34,
        transform.m41, transform.m42, transform.m43, transform.m44};
    glPushMatrix();
    glMultMatrixf(matrix);
    draw_model_layer(DrawLayer::Entity, NoHunter, &*render_model.instance,
                     &render_model);
    glPopMatrix();
}

void draw_enemy_models() {
    if (!g_session.has_value() || g_session->enemies().empty()) {
        return;
    }

    // The native session owns enemy lifecycle, targeting, collision, and
    // damage. Prefer the cartridge model loaded from the same model catalog;
    // the marker remains a deliberate fallback for an unsupported or missing
    // model so a gameplay entity is never silently invisible.
    const auto color_for_family = [](fruityprime::enemy::BehaviorFamily family) {
        switch (family) {
        case fruityprime::enemy::BehaviorFamily::Flying:
            return std::array<float, 3>{0.25F, 0.90F, 1.0F};
        case fruityprime::enemy::BehaviorFamily::Ground:
            return std::array<float, 3>{0.40F, 1.0F, 0.35F};
        case fruityprime::enemy::BehaviorFamily::Stationary:
            return std::array<float, 3>{1.0F, 0.72F, 0.20F};
        case fruityprime::enemy::BehaviorFamily::Boss:
            return std::array<float, 3>{0.95F, 0.35F, 1.0F};
        case fruityprime::enemy::BehaviorFamily::BossPart:
            return std::array<float, 3>{0.70F, 0.35F, 1.0F};
        case fruityprime::enemy::BehaviorFamily::Plant:
            return std::array<float, 3>{1.0F, 0.35F, 0.22F};
        case fruityprime::enemy::BehaviorFamily::Spawner:
            return std::array<float, 3>{1.0F, 0.55F, 0.10F};
        case fruityprime::enemy::BehaviorFamily::Hunter:
            return std::array<float, 3>{1.0F, 0.95F, 0.35F};
        case fruityprime::enemy::BehaviorFamily::Unknown:
            return std::array<float, 3>{0.82F, 0.82F, 0.86F};
        }
        return std::array<float, 3>{0.82F, 0.82F, 0.86F};
    };

    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0F);
    for (const auto& enemy : g_session->enemies()) {
        if (!enemy.active || !enemy.visible) {
            continue;
        }
        EntityRenderModel* render_model =
            enemy.enemy_type < g_enemy_models.size()
                ? g_enemy_models[enemy.enemy_type] : nullptr;
        if (render_model != nullptr && render_model->instance.has_value()) {
            draw_enemy_catalog_model(*render_model, enemy);
            continue;
        }
        const auto tuning = fruityprime::enemy::combat_tuning(
            enemy.enemy_type);
        const auto color = color_for_family(
            fruityprime::enemy::profile(enemy.enemy_type).family);
        const float size = std::max(0.25F, tuning.body_radius);
        glColor4f(color[0], color[1], color[2], 0.95F);
        glPushMatrix();
        glTranslatef(enemy.position.x, enemy.position.y,
                     enemy.position.z);
        glBegin(GL_LINE_LOOP);
        glVertex3f(-size, 0.0F, 0.0F);
        glVertex3f(0.0F, size * 0.8F, 0.0F);
        glVertex3f(size, 0.0F, 0.0F);
        glVertex3f(0.0F, -size * 0.8F, 0.0F);
        glEnd();
        glBegin(GL_LINES);
        glVertex3f(0.0F, 0.0F, -size);
        glVertex3f(0.0F, 0.0F, size);
        glVertex3f(0.0F, -size * 0.8F, 0.0F);
        glVertex3f(0.0F, size * 1.15F, 0.0F);
        glEnd();

        const float health_ratio = enemy.health_max == 0
            ? 0.0F : std::clamp(
                static_cast<float>(enemy.health)
                    / static_cast<float>(enemy.health_max),
                0.0F, 1.0F);
        glColor4f(0.08F, 0.08F, 0.10F, 0.90F);
        glBegin(GL_LINES);
        glVertex3f(-size, size * 1.45F, 0.0F);
        glVertex3f(size, size * 1.45F, 0.0F);
        glEnd();
        glColor4f(0.25F + (1.0F - health_ratio) * 0.75F,
                  0.25F + health_ratio * 0.70F, 0.18F, 1.0F);
        glBegin(GL_LINES);
        glVertex3f(-size, size * 1.45F, 0.01F);
        glVertex3f(-size + size * 2.0F * health_ratio,
                   size * 1.45F, 0.01F);
        glEnd();
        glPopMatrix();
    }
    glLineWidth(1.0F);
    glEnable(GL_TEXTURE_2D);
}

void draw_objectives() {
    if (!g_session.has_value() || !g_match_flow.has_value()) {
        return;
    }
    const auto& objectives = g_session->objective_state();
    const auto mode = g_match_flow->state().mode;
    const bool show_nodes = mode == 10 || mode == 11 || mode == 12 || mode == 13;
    const bool show_flags = mode == 7 || mode == 8 || mode == 9;
    if ((!show_nodes || objectives.nodes.empty())
        && (!show_flags || objectives.flags.empty())) {
        return;
    }

    const auto color_for_team = [](std::uint8_t team, float alpha) {
        if (team == fruityprime::gameplay::NeutralObjectiveTeam) {
            glColor4f(0.85F, 0.85F, 0.88F, alpha);
        } else if ((team & 1u) == 0) {
            glColor4f(0.25F, 0.55F, 1.0F, alpha);
        } else {
            glColor4f(1.0F, 0.28F, 0.25F, alpha);
        }
    };
    const auto draw_ring = [](fruityprime::scene::VolumePoint center,
                              float radius, float y) {
        constexpr int segments = 24;
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            const float angle = static_cast<float>(i)
                * 2.0F * 3.14159265358979323846F
                / static_cast<float>(segments);
            glVertex3f(center.x + std::cos(angle) * radius,
                       y,
                       center.z + std::sin(angle) * radius);
        }
        glEnd();
    };

    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0F);
    if (show_nodes) {
        for (const auto& node : objectives.nodes) {
            const auto center = node.volume.center();
            const auto owner = node.contested
                ? fruityprime::gameplay::NeutralObjectiveTeam
                : node.occupying_team
                        != fruityprime::gameplay::NeutralObjectiveTeam
                    ? node.occupying_team : node.current_team;
            color_for_team(owner, node.contested ? 0.95F : 0.75F);
            draw_ring(center, 1.25F, center.y + 0.05F);
            glBegin(GL_LINES);
            glVertex3f(center.x, center.y, center.z);
            glVertex3f(center.x, center.y + 1.2F, center.z);
            glEnd();
            if (node.progress > 0.0F) {
                color_for_team(node.occupying_team, 1.0F);
                draw_ring(center, 0.85F
                              + std::clamp(node.progress / 10.0F, 0.0F, 1.0F)
                                  * 0.4F,
                          center.y + 0.08F);
            }
        }
    }

    if (show_flags) {
        for (const auto& flag : objectives.flags) {
            const auto position = flag.position;
            color_for_team(flag.bounty
                               ? fruityprime::gameplay::NeutralObjectiveTeam
                               : flag.team_id,
                           flag.at_base ? 0.65F : 1.0F);
            glBegin(GL_LINES);
            glVertex3f(position.x, position.y - 1.0F, position.z);
            glVertex3f(position.x, position.y + 1.0F, position.z);
            glEnd();
            glBegin(GL_TRIANGLES);
            glVertex3f(position.x, position.y + 1.0F, position.z);
            glVertex3f(position.x + 0.55F, position.y + 0.7F, position.z);
            glVertex3f(position.x, position.y + 0.4F, position.z);
            glEnd();
            draw_ring(position, flag.at_base ? 0.75F : 0.95F,
                      position.y - 1.0F);
        }
    }
    glLineWidth(1.0F);
    glEnable(GL_TEXTURE_2D);
}

void ensure_scene_scale_texture(int width, int height) {
    if (g_scene_scale_texture == 0) {
        glGenTextures(1, &g_scene_scale_texture);
    }
    if (g_scene_scale_width == width && g_scene_scale_height == height) {
        return;
    }
    g_scene_scale_width = width;
    g_scene_scale_height = height;
    glBindTexture(GL_TEXTURE_2D, g_scene_scale_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    const GLint filter = fruityprime::mods::render::options()
        .texture_filtering() ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void draw_scene_scale_quad(int width, int height,
                           int scene_width, int scene_height) {
    ensure_scene_scale_texture(scene_width, scene_height);
    glBindTexture(GL_TEXTURE_2D, g_scene_scale_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                        scene_width, scene_height);
    const GLint filter = fruityprime::mods::render::options()
        .texture_filtering() ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width),
            static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
    glBegin(GL_QUADS);
    // glCopyTexSubImage2D receives the back buffer from bottom to top, while
    // the HUD/quad coordinates below are top-to-bottom screen coordinates.
    glTexCoord2f(0.0F, 1.0F);
    glVertex2f(0.0F, 0.0F);
    glTexCoord2f(1.0F, 1.0F);
    glVertex2f(static_cast<float>(width), 0.0F);
    glTexCoord2f(1.0F, 0.0F);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glTexCoord2f(0.0F, 0.0F);
    glVertex2f(0.0F, static_cast<float>(height));
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ensure_movie_texture() {
    if (!g_movie_player.has_value()
        || g_movie_player->frame_count() == 0) {
        return;
    }
    const auto& header = g_movie_player->decoder().header();
    if (header.frame_width <= 0 || header.frame_height <= 0) {
        return;
    }
    if (g_movie_texture == 0) {
        glGenTextures(1, &g_movie_texture);
        glBindTexture(GL_TEXTURE_2D, g_movie_texture);
        const GLint filter = fruityprime::mods::render::options()
                                 .texture_filtering()
            ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, header.frame_width,
                     header.frame_height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     nullptr);
        g_movie_texture_width = header.frame_width;
        g_movie_texture_height = header.frame_height;
        g_movie_uploaded_frame = std::numeric_limits<std::size_t>::max();
    }
    if (g_movie_texture_width != header.frame_width
        || g_movie_texture_height != header.frame_height) {
        glBindTexture(GL_TEXTURE_2D, g_movie_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, header.frame_width,
                     header.frame_height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     nullptr);
        g_movie_texture_width = header.frame_width;
        g_movie_texture_height = header.frame_height;
        g_movie_uploaded_frame = std::numeric_limits<std::size_t>::max();
    }
    const std::size_t frame = g_movie_player->frame_index();
    if (frame == g_movie_uploaded_frame) {
        return;
    }
    const auto rgb = g_movie_player->image_rgb();
    const std::size_t expected = static_cast<std::size_t>(
        header.frame_width) * static_cast<std::size_t>(header.frame_height)
        * 3U;
    if (rgb.size() != expected) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, g_movie_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, header.frame_width,
                    header.frame_height, GL_RGB, GL_UNSIGNED_BYTE,
                    rgb.data());
    g_movie_uploaded_frame = frame;
}

void draw_movie_frame(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(client.right - client.left, 1L);
    const int height = std::max(client.bottom - client.top, 1L);
    glViewport(0, 0, width, height);
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ensure_movie_texture();
    if (g_movie_texture == 0 || !g_movie_player.has_value()) {
        SwapBuffers(g_device_context);
        if (g_sfx_mixer != nullptr) {
            static_cast<void>(g_sfx_mixer->pump());
        }
        return;
    }

    const auto& header = g_movie_player->decoder().header();
    const float scale = std::min(
        static_cast<float>(width) / header.frame_width,
        static_cast<float>(height) / header.frame_height);
    const float draw_width = header.frame_width * scale;
    const float draw_height = header.frame_height * scale;
    const float left = (static_cast<float>(width) - draw_width) * 0.5F;
    const float top = (static_cast<float>(height) - draw_height) * 0.5F;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height),
            0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_movie_texture);
    glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0F, 0.0F);
    glVertex2f(left, top);
    glTexCoord2f(1.0F, 0.0F);
    glVertex2f(left + draw_width, top);
    glTexCoord2f(1.0F, 1.0F);
    glVertex2f(left + draw_width, top + draw_height);
    glTexCoord2f(0.0F, 1.0F);
    glVertex2f(left, top + draw_height);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    SwapBuffers(g_device_context);
    if (g_sfx_mixer != nullptr) {
        static_cast<void>(g_sfx_mixer->pump());
    }
}

void draw_frame(HWND window) {
    if (g_movie_player.has_value()) {
        draw_movie_frame(window);
        return;
    }
    update_render_models();
    RECT client{};
    GetClientRect(window, &client);
    const int width = std::max(client.right - client.left, 1L);
    const int height = std::max(client.bottom - client.top, 1L);
    const int scene_width = fruityprime::mods::render::options().scaled(width);
    const int scene_height = fruityprime::mods::render::options().scaled(height);
    static bool wrote_render_state = false;
    if (!wrote_render_state) {
        std::ofstream render_state(
            "FruityPrime-native-render-state.log", std::ios::trunc);
        render_state << "session=" << (g_session.has_value() ? 1 : 0)
                     << " players="
                     << (g_session.has_value()
                             ? g_session->players().size() : 0)
                     << " has_local="
                     << (g_session.has_value()
                             && g_session->has_player(g_local_slot) ? 1 : 0)
                     << " local_slot="
                     << static_cast<unsigned>(g_local_slot)
                     << " match_flow=" << (g_match_flow.has_value() ? 1 : 0)
                     << " primitives=" << g_draw_primitives.size()
                     << " helmet=" << (g_hud_helmet.has_value() ? 1 : 0)
                     << " reticle=" << (g_hud_reticle.has_value() ? 1 : 0)
                     << " room=" << (g_room.has_value() ? 1 : 0)
                     << " font_frames="
                     << (g_hud_font.has_value()
                             ? g_hud_font->image_count : 0)
                     << " hud_msgs=" << g_hud_msgs_common.size()
                     << '/' << g_hud_msgs_multi.size()
                     << " gun="
                     << (g_player_models[std::min<std::size_t>(
                             g_local_hunter,
                             fruityprime::metadata::HunterCount - 1)]
                            .gun_model.has_value() ? 1 : 0)
                     << " gun_prims="
                     << std::count_if(
                            g_draw_primitives.begin(),
                            g_draw_primitives.end(),
                            [](const DrawPrimitive& value) {
                                return value.layer == DrawLayer::Gun;
                            })
                     << '\n';
        wrote_render_state = true;
    }
    glViewport(0, 0, width, height);
    update_fps_counter();
    const fruityprime::players::CameraInfo* player_camera = nullptr;
    float camera_fov = 78.0F;
    // The intro owns the view while it runs: the player has not started
    // playing, so there is no first-person camera to show yet.
    static fruityprime::players::CameraInfo intro_camera;
    if (g_intro.has_value()) {
        const auto& state = g_intro->camera();
        intro_camera.position = {state.position.x, state.position.y,
                                 state.position.z};
        intro_camera.target = {state.target.x, state.target.y,
                               state.target.z};
        intro_camera.up = {state.up_vector.x, state.up_vector.y,
                           state.up_vector.z};
        intro_camera.facing = {state.facing.x, state.facing.y,
                               state.facing.z};
        intro_camera.fov_degrees = state.fov > 0.0F ? state.fov : 78.0F;
        player_camera = &intro_camera;
        camera_fov = intro_camera.fov_degrees;
    } else if (g_session.has_value() && g_session->has_player(g_local_slot)
        && !g_spectator.is_spectating()) {
        const auto& local_player = g_session->player(g_local_slot);
        const auto& profile = g_session->player_profile(g_local_slot);
        auto& camera = active_player_camera();
        camera.update(
            local_player, profile, 1.0F / 60.0F, false,
            g_session->morph_camera_position(g_local_slot));
        player_camera = &camera.info();
        camera_fov = player_camera->fov_degrees;
    }
    glClearColor(0.035F, 0.045F, 0.075F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, scene_width, scene_height);
    set_projection(scene_width, scene_height, camera_fov);
    glEnable(GL_DEPTH_TEST);
    configure_scene_render_state();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (player_camera != nullptr) {
        apply_camera_view(*player_camera);
    } else if (g_session.has_value() && !g_session->players().empty()) {
        const auto& local_player = g_session->player(g_local_slot);
        if (g_spectator.is_spectating() && !g_spectator.free_camera()
            && g_session->has_player(g_spectator.view_slot())) {
            const auto& viewed = g_session->player(g_spectator.view_slot());
            const float view_yaw = std::atan2(
                viewed.facing.x, viewed.facing.z)
                * 180.0F / 3.14159265358979323846F;
            glTranslatef(0.0F, -2.0F, -12.0F);
            glRotatef(12.0F, 1.0F, 0.0F, 0.0F);
            glRotatef(-view_yaw, 0.0F, 1.0F, 0.0F);
            glTranslatef(-viewed.position.x, -viewed.position.y,
                         -viewed.position.z);
        } else {
            glTranslatef(0.0F, -2.0F, -12.0F);
            glRotatef(12.0F, 1.0F, 0.0F, 0.0F);
            glRotatef(g_aim_pitch * 180.0F / 3.14159265358979323846F,
                      1.0F, 0.0F, 0.0F);
            glRotatef(-g_aim_yaw * 180.0F / 3.14159265358979323846F,
                      0.0F, 1.0F, 0.0F);
            glTranslatef(-local_player.position.x, -local_player.position.y,
                         -local_player.position.z);
        }
    } else {
        glTranslatef(0.0F, 0.0F, -5.0F);
        static float angle = 0.0F;
        angle += 0.35F;
        glRotatef(angle, 0.0F, 1.0F, 0.0F);
    }
    apply_scene_lights();
    if (g_draw_primitives.empty()) {
        draw_fallback();
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        draw_model_layer(DrawLayer::Room, NoHunter,
                         g_room_instance.has_value()
                             ? &*g_room_instance : nullptr);
        draw_entity_models();
        draw_item_models();
        draw_objectives();
        // player_camera is non-null only for the local player's first-person
        // view; the spectator path uses a chase camera, which must still draw
        // the player it is following.
        // Everyone is drawn during the intro, the local player included:
        // the camera is not theirs, so hiding them would leave a hole on
        // their own spawn point.
        draw_player_model(player_camera != nullptr && !g_intro.has_value()
                              ? static_cast<int>(g_local_slot) : -1);
        draw_enemy_models();
        draw_projectiles();
        queue_scan_particles();
        draw_single_particles();
        draw_player_death_particles();
        draw_effects();
        // Last of the world draw: it is the nearest thing to the eye, and it
        // clears depth for itself.
        if (player_camera != nullptr && !g_intro.has_value()) {
            draw_gun_model(*player_camera);
        }
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }
    draw_player_marker(player_camera != nullptr
                           ? static_cast<int>(g_local_slot) : -1);
    draw_item_markers();
    if (scene_width != width || scene_height != height) {
        draw_scene_scale_quad(width, height, scene_width, scene_height);
    }
    draw_hud(width, height);
    SwapBuffers(g_device_context);
    // Keep one device-sized block per rendered frame. Simulation ticks update
    // voice lifetimes; this is the only place that advances the PCM output
    // cursor, including while the game is paused.
    if (g_sfx_runtime != nullptr) {
        static_cast<void>(g_sfx_runtime->pump());
    }
}

void center_mouse(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    POINT center{
        (client.right - client.left) / 2,
        (client.bottom - client.top) / 2
    };
    ClientToScreen(window, &center);
    SetCursorPos(center.x, center.y);
}

void sync_window_topmost(HWND window) {
    const bool topmost = g_window_mode.fullscreen && !g_paused;
    g_window_mode.set_topmost(topmost);
    SetWindowPos(window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void set_native_fullscreen(HWND window, bool fullscreen) {
    if (fullscreen == g_window_mode.fullscreen) {
        sync_window_topmost(window);
        return;
    }
    if (fullscreen) {
        if (!g_windowed_state_saved) {
            g_windowed_rect = {};
            GetWindowRect(window, &g_windowed_rect);
            g_windowed_style = GetWindowLongPtrA(window, GWL_STYLE);
            g_windowed_exstyle = GetWindowLongPtrA(window, GWL_EXSTYLE);
            g_windowed_state_saved = true;
        }
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        const HMONITOR handle = MonitorFromWindow(
            window, MONITOR_DEFAULTTONEAREST);
        if (handle == nullptr || GetMonitorInfoA(handle, &monitor) == FALSE) {
            return;
        }
        g_window_mode.enter();
        SetWindowLongPtrA(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(window, HWND_TOPMOST,
                     monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        sync_window_topmost(window);
        return;
    }
    g_window_mode.leave();
    if (g_windowed_state_saved) {
        SetWindowLongPtrA(window, GWL_STYLE, g_windowed_style);
        SetWindowLongPtrA(window, GWL_EXSTYLE, g_windowed_exstyle);
        SetWindowPos(window, HWND_NOTOPMOST,
                     g_windowed_rect.left, g_windowed_rect.top,
                     g_windowed_rect.right - g_windowed_rect.left,
                     g_windowed_rect.bottom - g_windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        sync_window_topmost(window);
    }
}

void apply_window_startup(HWND window) {
    if (g_window_mode.startup
        == fruityprime::window::StartMode::BorderlessFullscreen) {
        set_native_fullscreen(window, true);
    } else {
        sync_window_topmost(window);
    }
}

[[nodiscard]] bool key_down(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

[[nodiscard]] bool gamepad_binding_down(
    fruityprime::input::GamepadButtons binding) {
    auto buttons = g_gamepad_state.buttons;
    if (g_gamepad_state.left_trigger >= 166) {
        buttons |= fruityprime::input::GamepadButtons::LeftTrigger;
    }
    if (g_gamepad_state.right_trigger >= 166) {
        buttons |= fruityprime::input::GamepadButtons::RightTrigger;
    }
    return g_gamepad_state.connected
        && (buttons & binding) != fruityprime::input::GamepadButtons::None;
}

void update_scan_visor_toggle() {
    const bool keyboard_down = key_down('V');
    const bool gamepad_down = gamepad_binding_down(
        g_input_config.pad_bindings.scan_visor);
    if ((keyboard_down && !g_scan_visor_keyboard_was_down)
        || (gamepad_down && !g_scan_visor_gamepad_was_down)) {
        g_scan_visor_active = !g_scan_visor_active;
    }
    g_scan_visor_keyboard_was_down = keyboard_down;
    g_scan_visor_gamepad_was_down = gamepad_down;
}

void update_mouse_aim(HWND window) {
    if (!g_mouse_look || GetForegroundWindow() != window) {
        return;
    }
    RECT client{};
    GetClientRect(window, &client);
    POINT center{
        (client.right - client.left) / 2,
        (client.bottom - client.top) / 2
    };
    POINT center_screen = center;
    ClientToScreen(window, &center_screen);
    POINT cursor{};
    if (GetCursorPos(&cursor) == FALSE) {
        return;
    }
    const int delta_x = cursor.x - center_screen.x;
    const int delta_y = cursor.y - center_screen.y;
    constexpr float sensitivity = 0.0035F;
    const float scaled_sensitivity = sensitivity
        * g_input_config.mouse_sensitivity;
    g_aim_yaw += static_cast<float>(delta_x) * scaled_sensitivity
        * (g_input_config.invert_mouse_x ? -1.0F : 1.0F);
    g_aim_pitch += static_cast<float>(delta_y) * scaled_sensitivity
        * (g_input_config.invert_mouse_y ? 1.0F : -1.0F);
    constexpr float max_pitch = 1.45F;
    g_aim_pitch = std::clamp(g_aim_pitch, -max_pitch, max_pitch);
    if (delta_x != 0 || delta_y != 0) {
        SetCursorPos(center_screen.x, center_screen.y);
    }
}

void update_gamepad_aim() {
    if (!g_gamepad_state.connected) {
        return;
    }
    constexpr float AxisMaximum = 32767.0F;
    const float raw_x = std::clamp(
        static_cast<float>(g_gamepad_state.right_x) / AxisMaximum,
        -1.0F, 1.0F);
    const float raw_y = std::clamp(
        static_cast<float>(g_gamepad_state.right_y) / AxisMaximum,
        -1.0F, 1.0F);
    const float magnitude = std::sqrt(raw_x * raw_x + raw_y * raw_y);
    const float dead_zone = std::clamp(
        g_input_config.gamepad_dead_zone, 0.0F, 0.9F);
    if (magnitude <= dead_zone) {
        return;
    }
    const float scaled_magnitude = std::min(
        (magnitude - dead_zone) / (1.0F - dead_zone), 1.0F);
    const float inverse_magnitude = 1.0F / magnitude;
    const float x = raw_x * inverse_magnitude * scaled_magnitude;
    const float y = raw_y * inverse_magnitude * scaled_magnitude;
    const float turn_rate = 3.5F * std::clamp(
        g_input_config.gamepad_look_sensitivity, 0.1F, 5.0F);
    constexpr float radians_per_degree = 0.0174532925199433F;
    // Match the managed pad's squared response: the centre of the stick is
    // precise while the outer travel still turns quickly.
    g_aim_yaw += x * std::abs(x) * turn_rate * radians_per_degree;
    g_aim_pitch += y * std::abs(y) * turn_rate * radians_per_degree
        * (g_input_config.gamepad_invert_y ? -1.0F : 1.0F);
    constexpr float max_pitch = 1.45F;
    g_aim_pitch = std::clamp(g_aim_pitch, -max_pitch, max_pitch);
}

[[nodiscard]] std::string describe_gamepad(
    const fruityprime::input::GamepadState& state) {
    if (!state.connected) {
        return "no pad";
    }
    const auto buttons = state.buttons;
    fruityprime::input::State mapped;
    fruityprime::input::apply_gamepad(mapped, state,
        fruityprime::input::GamepadConfig{g_input_config.gamepad_dead_zone,
                                          0.5F,
                                          g_input_config.pad_bindings});
    std::string names;
    const auto add = [&names, buttons](
                         fruityprime::input::GamepadButtons button,
                         const char* name) {
        if ((buttons & button)
            != fruityprime::input::GamepadButtons::None) {
            if (!names.empty()) {
                names += ", ";
            }
            names += name;
        }
    };
    using fruityprime::input::GamepadButtons;
    add(GamepadButtons::A, "A");
    add(GamepadButtons::B, "B");
    add(GamepadButtons::X, "X");
    add(GamepadButtons::Y, "Y");
    add(GamepadButtons::LeftBumper, "LB");
    add(GamepadButtons::RightBumper, "RB");
    add(GamepadButtons::Back, "Back");
    add(GamepadButtons::Start, "Start");
    add(GamepadButtons::DpadUp, "D-pad up");
    add(GamepadButtons::DpadRight, "D-pad right");
    add(GamepadButtons::DpadDown, "D-pad down");
    add(GamepadButtons::DpadLeft, "D-pad left");
    add(GamepadButtons::LeftTrigger, "LT");
    add(GamepadButtons::RightTrigger, "RT");
    std::ostringstream output;
    output << std::fixed << std::setprecision(2)
           << "L(" << static_cast<float>(state.left_x) / 32'767.0F
           << ',' << static_cast<float>(state.left_y) / 32'767.0F
           << ") R(" << static_cast<float>(state.right_x) / 32'767.0F
           << ',' << static_cast<float>(state.right_y) / 32'767.0F
           << ") LT" << static_cast<float>(state.left_trigger) / 255.0F
           << " RT" << static_cast<float>(state.right_trigger) / 255.0F
           << " buttons=" << (names.empty() ? "none" : names)
           << " actions=0x" << std::hex << std::setfill('0')
           << std::setw(8) << static_cast<std::uint32_t>(mapped.buttons());
    return output.str();
}

[[nodiscard]] bool prepare_gamepad_console(bool& owned) {
    const auto result = fruityprime::mods::console::show();
    owned = result.owns_console;
    return result.available;
}

int run_gamepad_probe(double seconds) {
    bool owns_console = false;
    if (!prepare_gamepad_console(owns_console)) {
        return EXIT_FAILURE;
    }
    std::cout << "[gamepad] watching for " << seconds << " s\n";
    std::cout << "[gamepad] A jump/boost, B morph, RT fire, LT zoom, Y scan "
                 "visor, bumpers/D-pad cycle weapons\n";
    const auto started = std::chrono::steady_clock::now();
    std::string previous;
    bool ever_connected = false;
    bool ever_moved = false;
    while (std::chrono::duration<double>(
               std::chrono::steady_clock::now() - started).count() < seconds) {
        static_cast<void>(g_gamepad.poll(g_gamepad_state));
        ever_connected |= g_gamepad_state.connected;
        const std::string description = describe_gamepad(g_gamepad_state);
        if (description != previous) {
            previous = description;
            std::cout << "  " << description << '\n';
            if (g_gamepad_state.connected
                && (g_gamepad_state.buttons
                        != fruityprime::input::GamepadButtons::None
                    || std::abs(g_gamepad_state.left_x) > 16'384
                    || std::abs(g_gamepad_state.left_y) > 16'384
                    || std::abs(g_gamepad_state.right_x) > 16'384
                    || std::abs(g_gamepad_state.right_y) > 16'384
                    || g_gamepad_state.left_trigger >= 166
                    || g_gamepad_state.right_trigger >= 166)) {
                ever_moved = true;
            }
        }
        std::cout.flush();
        Sleep(16);
    }
    if (!ever_connected) {
        std::cout << "[gamepad] FAIL: no XInput controller was seen.\n";
    } else if (!ever_moved) {
        std::cout << "[gamepad] a controller is connected, but no input was "
                     "pressed or moved far enough.\n";
    } else {
        std::cout << "[gamepad] PASS: controller input arrived.\n";
    }
    std::cout.flush();
    if (owns_console) {
        FreeConsole();
    }
    return ever_connected && ever_moved ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] fruityprime::net::Vec3 aim_from_angles() {
    const float horizontal = std::cos(g_aim_pitch);
    return {std::sin(g_aim_yaw) * horizontal,
            std::sin(g_aim_pitch),
            std::cos(g_aim_yaw) * horizontal};
}

bool ensure_network_player(std::uint8_t slot, std::uint8_t hunter) {
    if (g_session.has_value() && !g_session->has_player(slot)) {
        static_cast<void>(g_session->add_player(slot, hunter));
        return true;
    }
    return false;
}

void handle_network_roster(const fruityprime::net::RosterPacket& roster) {
    g_roster = roster;
    if (!g_session.has_value()) {
        return;
    }
    static_cast<void>(g_slot_manager.sync(
        *g_session, roster, g_local_slot, g_game_state));
    static_cast<void>(fruityprime::net::NetPlayerSetup::ApplyOnce(
        *g_session, g_local_slot));
}

void toggle_spectating() {
    if (!g_session.has_value()) {
        return;
    }
    static_cast<void>(g_spectator.toggle(
        *g_session, g_local_slot, g_game_state.multiplayer(),
        g_replay_playback.active(), g_input));
}

void rejoin_spectating() {
    if (!g_session.has_value()) {
        return;
    }
    if (g_spectator.rejoin(*g_session, g_local_slot)) {
        g_show_scoreboard = false;
    }
}

void sync_process_game_state() {
    if (!g_session.has_value() || !g_match_flow.has_value()) {
        return;
    }
    const auto& match = g_match_flow->state();
    const auto mode = static_cast<fruityprime::game::Mode>(match.mode);
    g_game_state.mode = mode;
    g_game_state.single_player = mode
        == fruityprime::game::Mode::SinglePlayer;
    g_game_state.teams = fruityprime::match::is_team_mode(match.mode);
    g_game_state.friendly_fire = match.friendly_fire();
    g_game_state.local_slot = g_local_slot;
    g_game_state.match_time = match.time_remaining;
    g_game_state.point_goal = match.point_goal;
    g_game_state.time_goal = fruityprime::match::defaults_for_mode(match.mode)
        .time_goal_seconds;
    g_game_state.player_count = match.player_count;
    g_game_state.frame_count = g_session->tick_count();
    switch (g_match_flow->phase()) {
    case fruityprime::match::Phase::InProgress:
        g_game_state.match_state = fruityprime::game::MatchState::InProgress;
        break;
    case fruityprime::match::Phase::GameOver:
        g_game_state.match_state = fruityprime::game::MatchState::GameOver;
        break;
    case fruityprime::match::Phase::Ending:
        g_game_state.match_state = fruityprime::game::MatchState::Ending;
        break;
    case fruityprime::match::Phase::Disconnected:
        g_game_state.match_state = fruityprime::game::MatchState::Disconnected;
        break;
    }

    g_game_state.sync_players(g_session->players());
    const auto& objectives = g_session->objective_state();
    g_game_state.player_time = objectives.player_time;
    g_game_state.team_time = objectives.team_time;
    g_game_state.prime_hunter = objectives.prime_hunter;
    // sync_players builds the result order before objective timers are copied;
    // refresh once more so Survival/Defender/Prime Hunter rankings see the
    // current frame's time values.
    g_game_state.update_results();
}

void feed_match_objectives() {
    if (!g_session.has_value() || !g_match_flow.has_value()) {
        return;
    }
    const auto& objectives = g_session->objective_state();
    for (std::size_t team = 0; team < objectives.team_time.size(); ++team) {
        g_match_flow->set_team_objective_time(
            static_cast<std::uint8_t>(team), objectives.team_time[team]);
    }
    for (std::size_t slot = 0; slot < objectives.player_time.size(); ++slot) {
        g_match_flow->set_player_objective_time(
            static_cast<std::uint8_t>(slot), objectives.player_time[slot]);
    }
    g_match_flow->set_prime_hunter(objectives.prime_hunter);
    sync_process_game_state();
}

void poll_network() {
    if (!g_net_client || !g_net_client->connected() || !g_session.has_value()) {
        return;
    }
    for (const auto& packet : g_net_client->drain()) {
        // ReceivedPacket::data already includes the type byte, matching the
        // managed DemoRecorder boundary exactly.
        record_demo_bytes(packet.data);
        switch (packet.type()) {
        case fruityprime::net::PacketType::MatchState:
        case fruityprime::net::PacketType::MapChange: {
            const auto state = fruityprime::net::MatchStatePacket::decode(
                packet.payload());
            if (state && g_match_flow.has_value()) {
                g_server_match_state = *state;
                static_cast<void>(g_net_match_sync.apply(
                    g_game_state, *state,
                    fruityprime::net::MatchEnd::in_intermission(
                        true, g_game_state, &*state)));
                // Keep packet-loss-safe room bookkeeping separate from the
                // asset loader. The loader consumes this state at its own
                // transition point; observing it here makes repeated state
                // packets idempotent instead of restarting a fade every frame.
                static_cast<void>(g_net_room_change.observe(
                    g_game_state.room_name, *state, g_net_frame));
                g_match_flow->apply_server_state(*state);
                if (g_session.has_value()) {
                    g_session->set_match_mode(state->mode, state->point_goal);
                    g_session->set_team_mode(
                        fruityprime::match::is_team_mode(state->mode));
                    g_session->set_friendly_fire(state->friendly_fire());
                }
            }
            break;
        }
        case fruityprime::net::PacketType::Authority:
            g_is_authority = !packet.payload().empty()
                && packet.payload()[0] != 0;
            if (g_session.has_value()) {
                g_session->set_objective_authority(g_is_authority);
                g_session->set_network_damage_authority(g_is_authority);
            }
            break;
        case fruityprime::net::PacketType::Roster: {
            const auto roster = fruityprime::net::RosterPacket::decode(
                packet.payload());
            if (roster) {
                handle_network_roster(*roster);
            }
            break;
        }
        case fruityprime::net::PacketType::SlotIntent:
            if (g_is_authority && packet.payload().size() > 1) {
                const std::uint8_t slot = packet.payload()[0];
                const auto intent = fruityprime::net::IntentState::decode(
                    packet.payload().subspan(1));
                if (intent) {
                    ensure_network_player(slot, 0);
                    if (g_session->has_player(slot)) {
                        static_cast<void>(fruityprime::net::NetHooks::try_apply_remote_input(
                            *g_session, g_net_player_bridge, slot, *intent,
                            fruityprime::net::NetHookContext{
                                true, g_is_authority, false, false,
                                static_cast<int>(g_local_slot)}));
                    }
                }
            }
            break;
        case fruityprime::net::PacketType::Snapshot:
            if (!g_is_authority) {
                const auto snapshot = fruityprime::net::SnapshotPacket::decode(
                    packet.payload());
                if (snapshot) {
                    static_cast<void>(fruityprime::net::NetHooks::apply_authority_snapshot(
                        *g_session, g_net_player_bridge, *snapshot,
                        g_net_damage,
                        fruityprime::net::NetHookContext{
                            true, g_is_authority, false, false,
                            static_cast<int>(g_local_slot)}));
                }
            }
            break;
        case fruityprime::net::PacketType::Chat: {
            const auto chat = fruityprime::net::ChatPacket::decode(
                packet.payload());
            if (chat) {
                fruityprime::chat::ChatBox::Receive(*chat);
            }
            break;
        }
        case fruityprime::net::PacketType::Ping:
            g_net_client->send(fruityprime::net::PacketType::Pong,
                               packet.payload());
            break;
        default:
            break;
        }
    }
    sync_process_game_state();
    snapshot_net_log();
    report_net_diagnostics();
    if (g_net_client && g_net_client->connected()) {
        const auto result = g_net_match_end.sync(
            g_net_frame, true, g_is_authority, g_game_state,
            g_server_match_state.has_value() ? &*g_server_match_state : nullptr);
        if (result.should_report) {
            g_net_client->send(fruityprime::net::PacketType::MatchEnd);
        }
    }
}

void pump_replay_frame() {
    if (!g_replay_playback.active() || !g_session.has_value()) {
        return;
    }
    std::vector<fruityprime::net::SnapshotPacket> snapshots;
    const auto records = g_replay_playback.take_until(g_replay_frame);
    for (const auto& record : records) {
        if (record.data.empty()) {
            continue;
        }
        const auto packet = std::span<const std::uint8_t>(record.data);
        switch (static_cast<fruityprime::net::PacketType>(packet.front())) {
        case fruityprime::net::PacketType::MatchState:
        case fruityprime::net::PacketType::MapChange: {
            const auto state = fruityprime::net::MatchStatePacket::decode(
                packet.subspan(1));
            if (state && g_match_flow.has_value()) {
                static_cast<void>(g_net_match_sync.apply(
                    g_game_state, *state, state->ending()));
                g_match_flow->apply_server_state(*state);
                g_session->set_match_mode(state->mode, state->point_goal);
                g_session->set_team_mode(
                    fruityprime::match::is_team_mode(state->mode));
                g_session->set_friendly_fire(state->friendly_fire());
            }
            break;
        }
        case fruityprime::net::PacketType::Roster: {
            const auto roster = fruityprime::net::RosterPacket::decode(
                packet.subspan(1));
            if (roster) {
                handle_network_roster(*roster);
            }
            break;
        }
        case fruityprime::net::PacketType::SlotIntent:
            if (packet.size() >= 2 + fruityprime::net::IntentState::Size) {
                const std::uint8_t slot = packet[1];
                const auto intent = fruityprime::net::IntentState::decode(
                    packet.subspan(2));
                if (intent && slot < fruityprime::net::NetConfig::SlotCapacity) {
                    ensure_network_player(slot, 0);
                    if (g_session->has_player(slot)) {
                        static_cast<void>(fruityprime::net::NetHooks::try_apply_remote_input(
                            *g_session, g_net_player_bridge, slot, *intent,
                            fruityprime::net::NetHookContext{
                                true, false, true, false,
                                static_cast<int>(g_local_slot)}));
                    }
                }
            }
            break;
        case fruityprime::net::PacketType::Snapshot: {
            const auto snapshot = fruityprime::net::SnapshotPacket::decode(
                packet.subspan(1));
            if (snapshot) {
                snapshots.push_back(*snapshot);
            }
            break;
        }
        case fruityprime::net::PacketType::Chat: {
            const auto chat = fruityprime::net::ChatPacket::decode(
                packet.subspan(1));
            if (chat) {
                fruityprime::chat::ChatBox::Receive(*chat);
            }
            break;
        }
        default:
            break;
        }
    }

    if (g_paused) {
        ++g_replay_frame;
        return;
    }
    g_session->tick();
    update_static_entities();
    for (const auto& snapshot : snapshots) {
        if (g_replay_last_snapshot_frame != 0
            && snapshot.header.frame <= g_replay_last_snapshot_frame
            && g_replay_last_snapshot_frame - snapshot.header.frame < 600) {
            if (++g_replay_late_snapshot_run < 12) {
                continue;
            }
        }
        g_replay_late_snapshot_run = 0;
        g_replay_last_snapshot_frame = snapshot.header.frame;
        static_cast<void>(fruityprime::net::NetHooks::apply_authority_snapshot(
            *g_session, g_net_player_bridge, snapshot, g_net_damage,
            fruityprime::net::NetHookContext{
                true, false, true, false, static_cast<int>(g_local_slot)}));
    }
    fruityprime::net::NetHooks::after_simulation(
        *g_session, g_net_player_bridge,
        fruityprime::net::NetHookContext{
            true, false, true, false, static_cast<int>(g_local_slot)});
    record_and_repair_network_players(g_replay_frame);
    if (g_match_flow.has_value()) {
        g_match_flow->tick(1.0F / 60.0F, g_session->players(), false, false);
    }
    sync_process_game_state();
    update_audio_after_tick();
    ++g_replay_frame;
}

void send_network_frame() {
    if (!g_net_client || !g_net_client->connected() || !g_session.has_value()
        || !g_session->has_player(g_local_slot)) {
        return;
    }
    if (g_match_flow.has_value() && !g_match_flow->in_progress()) {
        return;
    }
    if (g_is_authority) {
        const auto packet = g_session->snapshot(g_net_frame++).encode();
        // The relay deliberately does not echo an authority's own snapshot.
        // Include it locally so an authority recording remains replayable.
        record_demo_packet(fruityprime::net::PacketType::Snapshot, packet);
        g_net_client->send(fruityprime::net::PacketType::Snapshot, packet);
        return;
    }
    auto buttons = g_input.buttons();
    const auto& local_player = g_session->player(g_local_slot);
    if ((local_player.flags & fruityprime::net::PlayerState::FlagZoomed) != 0) {
        buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(buttons)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::ZoomedState));
    }
    if ((local_player.flags & fruityprime::net::PlayerState::FlagAltForm) != 0) {
        buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(buttons)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::AltFormState));
    }
    if ((local_player.flags & fruityprime::net::PlayerState::FlagSpawned) != 0
        && local_player.health > 0) {
        buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(buttons)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::InPlayState));
    }
    if ((local_player.flags
            & fruityprime::net::PlayerState::FlagSpectating) != 0) {
        buttons = static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(buttons)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::SpectatingState));
    }
    const auto intent = g_net_player_bridge.capture_intent(
        *g_session, g_local_slot, g_net_frame++, buttons, g_input.aim(),
        g_weapon_select);
    const auto packet = intent.encode();
    std::vector<std::uint8_t> own_intent;
    own_intent.reserve(packet.size() + 2);
    own_intent.push_back(static_cast<std::uint8_t>(
        fruityprime::net::PacketType::SlotIntent));
    own_intent.push_back(g_local_slot);
    own_intent.insert(own_intent.end(), packet.begin(), packet.end());
    record_demo_bytes(own_intent);
    g_net_client->send(fruityprime::net::PacketType::Intent, packet);
}

void dismiss_scan_dialog(bool accept) {
    if (accept && g_story_save.has_value()
        && g_scan_dialog_scan_id < fruityprime::game::StorySave::ScanCount) {
        g_story_save->update_logbook(
            static_cast<std::int32_t>(g_scan_dialog_scan_id));
    }
    g_scan_progress = 0.0F;
    g_scan_complete = false;
    g_scan_dialog_active = false;
    g_scan_dialog_confirm_was_down = false;
    g_scan_dialog_scan_id = 0;
    g_scan_button_was_held = false;
    g_previous_scan_target = nullptr;
    // Do not let a V/Y held while the dialog was open become a second visor
    // toggle on the first resumed simulation frame.
    g_scan_visor_keyboard_was_down = key_down('V');
    g_scan_visor_gamepad_was_down = gamepad_binding_down(
        g_input_config.pad_bindings.scan_visor);
    g_paused = false;
}

void update_scan_dialog(HWND window) {
    if (!g_scan_dialog_active || !g_session.has_value()) {
        return;
    }
    static_cast<void>(g_gamepad.poll(g_gamepad_state));
    const bool confirm = key_down(VK_RETURN) || key_down(VK_SPACE)
        || gamepad_binding_down(fruityprime::input::GamepadButtons::A);
    const bool cancel = key_down(VK_ESCAPE);
    if (cancel) {
        dismiss_scan_dialog(false);
    } else if (confirm && !g_scan_dialog_confirm_was_down) {
        dismiss_scan_dialog(true);
    } else {
        g_scan_dialog_confirm_was_down = confirm;
    }
    g_input.clear();
    g_input.set_aim(aim_from_angles());
    g_session->set_input(g_local_slot, fruityprime::gameplay::Input{
        fruityprime::net::IntentButtons::None,
        g_input.aim()
    });
    g_show_scoreboard = false;
    if (!g_scan_dialog_active) {
        sync_window_topmost(window);
    }
    send_network_frame();
}

void update_bot_inputs() {
    if (!g_session.has_value() || g_net_client != nullptr
        || g_bot_slots.empty() || !g_session->has_player(g_local_slot)) {
        return;
    }
    for (const std::uint8_t slot : g_bot_slots) {
        if (!g_session->has_player(slot)) {
            continue;
        }
        auto* entity = fruityprime::players::PlayerEntity::Players()[slot];
        fruityprime::players::BotDecision decision;
        if (entity != nullptr) {
            entity->IsBot(true);
            entity->BotLevel(g_bot_level);
            decision = fruityprime::players::BotAi::decide(
                *g_session, slot, entity->AiData(), entity->BotLevel());
        } else {
            decision = fruityprime::players::BotAi::decide(
                *g_session, slot, g_bot_level);
        }
        g_session->set_input(slot, decision.input);
    }
}

void update_game(HWND window) {
    if (g_movie_player.has_value()) {
        if (!g_paused) {
            g_movie_player->update(1.0 / 60.0);
            if (g_movie_player->state()
                == fruityprime::movie::PlaybackState::Finished) {
                // Keep the final frame visible until the normal window loop
                // observes the end. A caller can still use -seconds to keep
                // a completed movie open for capture.
                g_paused = true;
                pause_movie_audio(true);
            }
        }
        return;
    }
    if (!g_session.has_value()) {
        return;
    }
    ++g_demo_frame;
    g_hud_elapsed_seconds += 1.0F / 60.0F;
    poll_network();
    if (g_replay_playback.active()) {
        if (!g_paused && !fruityprime::chat::ChatBox::Composing()) {
            pump_replay_frame();
        }
        g_show_scoreboard = key_down(VK_TAB)
            || gamepad_binding_down(g_input_config.pad_bindings.scoreboard);
        return;
    }
    if (!fruityprime::chat::ChatBox::Available()
        && fruityprime::chat::ChatBox::Composing()) {
        fruityprime::chat::ChatBox::Cancel();
    }
    if (fruityprime::chat::ChatBox::ConsumeJustClosed()) {
        // ModForgetInputDeltas clears the managed mouse/keyboard snapshots.
        if (auto* main_player = fruityprime::players::PlayerEntity::Main();
            main_player != nullptr) {
            main_player->ModForgetInputDeltas();
        }
        fruityprime::chat::player_entity_chat_hud::forget_input_deltas(
            g_input);
    }
    if (g_scan_dialog_active) {
        update_scan_dialog(window);
        return;
    }
    if (fruityprime::chat::ChatBox::Composing()) {
        g_input.clear();
        g_input.set_aim(aim_from_angles());
        g_session->set_input(g_local_slot, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::None,
            g_input.aim()
        });
        send_network_frame();
        return;
    }
    if (g_match_flow.has_value() && !g_match_flow->in_progress()) {
        g_spectator.reset();
        g_input.clear();
        g_show_scoreboard = key_down(VK_TAB)
            || gamepad_binding_down(g_input_config.pad_bindings.scoreboard);
        return;
    }
    if (g_paused) {
        g_input.clear();
        g_input.set_aim(aim_from_angles());
        g_session->set_input(g_local_slot, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::None,
            g_input.aim()
        });
        g_show_scoreboard = key_down(VK_TAB)
            || gamepad_binding_down(g_input_config.pad_bindings.scoreboard);
        send_network_frame();
        return;
    }
    if (g_spectator.is_spectating()) {
        g_input.clear();
        g_input.set_aim(aim_from_angles());
        g_session->set_input(g_local_slot, fruityprime::gameplay::Input{
            fruityprime::net::IntentButtons::SpectatingState,
            g_input.aim()
        });
        g_spectator.note_scoreboard(key_down(VK_TAB)
            || gamepad_binding_down(g_input_config.pad_bindings.scoreboard));
        g_show_scoreboard = g_spectator.show_scoreboard();
        g_session->tick();
        fruityprime::net::NetHooks::after_simulation(
            *g_session, g_net_player_bridge,
            fruityprime::net::NetHookContext{
                true, g_is_authority, false, false,
                static_cast<int>(g_local_slot)});
        record_and_repair_network_players(g_net_frame);
        update_static_entities();
        update_audio_after_tick();
        if (g_match_flow.has_value()) {
            feed_match_objectives();
            const bool networked = g_net_client != nullptr
                && g_net_client->connected();
            g_match_flow->tick(
                1.0F / 60.0F,
                g_session->players(),
                !networked || g_is_authority,
                !networked);
            sync_process_game_state();
        }
        send_network_frame();
        return;
    }
    using fruityprime::input::Action;
    g_input.clear();
    g_input.set(Action::MoveUp, key_down('W'));
    g_input.set(Action::MoveDown, key_down('S'));
    g_input.set(Action::MoveLeft, key_down('A'));
    g_input.set(Action::MoveRight, key_down('D'));
    g_input.set(Action::Jump, key_down(VK_SPACE));
    g_input.set(Action::AltFormState, key_down('M'));
    g_input.set(Action::Shoot, key_down(VK_LBUTTON));
    g_input.set(Action::Zoom, key_down(VK_RBUTTON));
    g_input.set(Action::Scan, key_down('Q'));
    static_cast<void>(g_gamepad.poll(g_gamepad_state));
    update_scan_visor_toggle();
    g_input.set(Action::ScanVisor, g_scan_visor_active);
    g_input.set(Action::PrevWeapon,
                key_down('Q') && !g_scan_visor_active);
    g_input.set(Action::NextWeapon,
                key_down('E') && !g_scan_visor_active);
    fruityprime::input::apply_gamepad(g_input, g_gamepad_state,
        fruityprime::input::GamepadConfig{g_input_config.gamepad_dead_zone,
                                          0.5F,
                                          g_input_config.pad_bindings});
    // Scan visor is a toggled local mode.  Override the raw held Y mapping
    // after applying the other gamepad actions so a release does not turn a
    // mode off immediately and a held button cannot retrigger it each frame.
    g_input.set_gamepad(Action::ScanVisor, g_scan_visor_active);
    update_gamepad_aim();
    update_mouse_aim(window);
    g_input.set_aim(aim_from_angles());
    g_weapon_select = 0xff;
    for (std::uint8_t weapon = 0; weapon < 9; ++weapon) {
        if (key_down('1' + weapon)) {
            g_weapon_select = weapon;
            break;
        }
    }
    if (gamepad_binding_down(g_input_config.pad_bindings.power_beam)) {
        g_weapon_select = 0;
    } else if (gamepad_binding_down(g_input_config.pad_bindings.missile)) {
        g_weapon_select = 1;
    }
    g_show_scoreboard = key_down(VK_TAB)
        || gamepad_binding_down(g_input_config.pad_bindings.scoreboard);
    auto intent = g_input.buttons();
    auto intent_weapon = g_weapon_select;
    if (g_intro.has_value()) {
        // CameraSequence.Process, and PlayerEntity.Spawn's "somebody pressed
        // fire" end: the match does not begin until they do.
        g_intro_seconds += 1.0F / 60.0F;
        g_intro->process(1.0F / 60.0F);
        const bool shoot = (static_cast<std::uint32_t>(intent)
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::Shoot)) != 0;
        if (shoot) {
            g_intro->end();
            g_intro.reset();
            g_intro_file.reset();
        } else {
            // The player is standing on their spawn point but is not playing
            // yet, so nothing they press reaches the match.  Only the intent
            // is suppressed: the rest of the frame -- the simulation, the net
            // hooks, the packet this client owes the server -- runs exactly
            // as it does during play, because a client that goes quiet for
            // the length of an intro is a client the server drops.
            intent = fruityprime::net::IntentButtons::None;
            intent_weapon = 0xff;
        }
    }
    g_session->set_input(g_local_slot, fruityprime::gameplay::Input{
        intent, g_input.aim(), intent_weapon
    });
    update_bot_inputs();
    g_session->tick();
    fruityprime::net::NetHooks::after_simulation(
        *g_session, g_net_player_bridge,
        fruityprime::net::NetHookContext{
            true, g_is_authority, false, false,
            static_cast<int>(g_local_slot)});
    record_and_repair_network_players(g_net_frame);
    if (g_game_state.single_player
        && g_session->room_transition().has_value()) {
        const auto request = *g_session->room_transition();
        if (!transition_game_room(request)) {
            g_paused = true;
        }
        return;
    }
    update_static_entities();
    update_audio_after_tick();
    if (g_match_flow.has_value()) {
        feed_match_objectives();
        const bool networked = g_net_client != nullptr
            && g_net_client->connected();
        g_match_flow->tick(
            1.0F / 60.0F,
            g_session->players(),
            !networked || g_is_authority,
            !networked);
        sync_process_game_state();
        static_cast<void>(g_match_flow->consume_end_report());
        if (networked) {
            const auto result = g_net_match_end.sync(
                g_net_frame, true, g_is_authority, g_game_state,
                g_server_match_state.has_value()
                    ? &*g_server_match_state : nullptr);
            if (result.should_report) {
                g_net_client->send(fruityprime::net::PacketType::MatchEnd);
            }
        }
    }
    send_network_frame();
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                             LPARAM lparam) {
    if (message == WM_CLOSE || message == WM_DESTROY) {
        if (message == WM_CLOSE && g_window_mode.fullscreen) {
            set_native_fullscreen(window, false);
        }
        if (g_mouse_look) {
            ReleaseCapture();
            g_mouse_look = false;
        }
        g_pause_menu.mark_closed();
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_KEYDOWN) {
        const bool can_open = g_session.has_value()
            && g_match_flow.has_value() && !g_replay_playback.active()
            && !g_paused;
        if (fruityprime::chat::ChatBox::HandleKeyDown(
                chat_key_from_windows(wparam, lparam), key_down(VK_CONTROL),
                key_down(VK_MENU), can_open)) {
            if (g_mouse_look) {
                ReleaseCapture();
                g_mouse_look = false;
            }
            return 0;
        }
    }
    if (message == WM_CHAR && fruityprime::chat::ChatBox::Composing()) {
        fruityprime::chat::ChatBox::HandleText(static_cast<int>(wparam));
        return 0;
    }
    if (message == WM_KEYDOWN && (lparam & (1LL << 30)) == 0
        && (wparam == VK_F11
            || (wparam == VK_RETURN && key_down(VK_MENU)))) {
        set_native_fullscreen(window, !g_window_mode.fullscreen);
        return 0;
    }
    if (message == WM_KEYDOWN && (lparam & (1LL << 30)) == 0
        && wparam == VK_F6) {
        toggle_spectating();
        return 0;
    }
    if (message == WM_KEYDOWN && (lparam & (1LL << 30)) == 0
        && wparam == VK_F7) {
        rejoin_spectating();
        return 0;
    }
    if (message == WM_KEYDOWN && g_scan_dialog_active
        && wparam == VK_ESCAPE && (lparam & (1LL << 30)) == 0) {
        dismiss_scan_dialog(false);
        sync_window_topmost(window);
        return 0;
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE
        && (lparam & (1LL << 30)) == 0) {
        RECT client{};
        GetClientRect(window, &client);
        POINT origin{client.left, client.top};
        ClientToScreen(window, &origin);
        const auto consumed = g_pause_menu.handle_escape(
            {origin.x, origin.y, client.right - client.left,
             client.bottom - client.top});
        if (consumed) {
            g_paused = g_pause_menu.open();
            if (g_movie_player.has_value()) {
                pause_movie_audio(g_paused);
            }
            sync_window_topmost(window);
            if (g_mouse_look) {
                ReleaseCapture();
                g_mouse_look = false;
            }
            return 0;
        }
    }
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
        g_mouse_look = true;
        SetCapture(window);
        center_mouse(window);
        return 0;
    }
    if (message == WM_LBUTTONUP || message == WM_RBUTTONUP) {
        if (!key_down(VK_LBUTTON) && !key_down(VK_RBUTTON)) {
            ReleaseCapture();
            g_mouse_look = false;
        }
        return 0;
    }
    if (message == WM_KILLFOCUS) {
        if (g_mouse_look) {
            ReleaseCapture();
            g_mouse_look = false;
        }
        return 0;
    }
    if (message == WM_ERASEBKGND) {
        return 1;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

void load_replay_file(const std::filesystem::path& path,
                      fruityprime::net::MatchStatePacket& first_match) {
    std::string error;
    if (!g_replay_playback.open(path, first_match, error)) {
        throw std::runtime_error(error);
    }
    if (g_replay_playback.protocol_mismatch()) {
        OutputDebugStringA("Fruity Prime replay protocol differs from this build\n");
    }
    g_replay_frame = 0;
    g_replay_last_snapshot_frame = 0;
    g_replay_late_snapshot_run = 0;
    g_spectator.reset();
}

void initialize_command_arguments() {
    int argument_count = 0;
    LPWSTR* wide_arguments = CommandLineToArgvW(
        GetCommandLineW(), &argument_count);
    if (wide_arguments == nullptr) {
        return;
    }
    g_command_arguments.clear();
    g_command_arguments.reserve(static_cast<std::size_t>(argument_count));
    for (int index = 0; index < argument_count; ++index) {
        const int required = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, wide_arguments[index], -1,
            nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            g_command_arguments.emplace_back();
            continue;
        }
        std::string value(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            wide_arguments[index], -1, value.data(),
                            required, nullptr, nullptr);
        value.resize(static_cast<std::size_t>(required - 1));
        g_command_arguments.push_back(std::move(value));
    }
    LocalFree(wide_arguments);
}

std::string argument_value(std::string_view flag) {
    for (std::size_t i = 1; i + 1 < g_command_arguments.size(); ++i) {
        if (std::string_view(g_command_arguments[i]) == flag) {
            return g_command_arguments[i + 1];
        }
    }
    return {};
}

[[nodiscard]] bool argument_present(std::string_view flag) {
    return std::any_of(g_command_arguments.begin() + std::min<std::size_t>(
                           1, g_command_arguments.size()),
                       g_command_arguments.end(),
                       [flag](const std::string& value) {
        if (std::string_view(value) == flag) {
            return true;
        }
        return false;
    });
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    fruityprime::utility::console::run();
    initialize_command_arguments();
    g_started_at = std::chrono::steady_clock::now();
    fruityprime::chat::detail::BindRuntime({
        &chat_single_player,
        &chat_net_active,
        &chat_player_name,
        &send_chat_packet,
        nullptr
    });
    fruityprime::chat::ChatBox::Clear();
    double run_seconds = 0.0;
    bool adventure_argument = false;
    std::uint8_t story_save_slot = 1;
    std::error_code executable_error;
    const auto executable = std::filesystem::absolute(
        g_command_arguments.empty() ? std::string{} : g_command_arguments[0],
        executable_error);
    const auto executable_directory = !executable_error
        && executable.has_parent_path()
        ? executable.parent_path() : std::filesystem::current_path();
    if (argument_present("-applyupdate")) {
        const auto flag = std::find(g_command_arguments.begin() + 1,
                                    g_command_arguments.end(),
                                    "-applyupdate");
        const auto flag_index = static_cast<std::size_t>(
            std::distance(g_command_arguments.begin(), flag));
        if (flag == g_command_arguments.end()
            || flag_index + 2 >= g_command_arguments.size()) {
            MessageBoxA(nullptr, "-applyupdate needs TARGET and PID",
                        "Fruity Prime update", MB_OK | MB_ICONERROR);
            return EXIT_FAILURE;
        }
        const std::string pid_text = g_command_arguments[flag_index + 2];
        std::size_t consumed = 0;
        long long pid = 0;
        try {
            pid = std::stoll(pid_text, &consumed, 10);
        } catch (const std::exception&) {
            consumed = 0;
        }
        if (consumed != pid_text.size() || pid <= 0
            || pid > std::numeric_limits<int>::max()) {
            MessageBoxA(nullptr, "-applyupdate PID is invalid",
                        "Fruity Prime update", MB_OK | MB_ICONERROR);
            return EXIT_FAILURE;
        }
        return fruityprime::update::DesktopUpdate::apply(
            std::filesystem::path(g_command_arguments[flag_index + 1]),
            static_cast<int>(pid), executable_directory);
    }
    fruityprime::update::DesktopUpdate::clean(executable_directory);
    const auto launcher_preferences = fruityprime::launcher::load_preferences(
        executable_directory);
    g_window_mode.startup = launcher_preferences.window_mode;
    if (argument_present("-fullscreen") || argument_present("-borderless")) {
        g_window_mode.startup =
            fruityprime::window::StartMode::BorderlessFullscreen;
    } else if (argument_present("-windowed")) {
        g_window_mode.startup = fruityprime::window::StartMode::Windowed;
    }
    g_no_helmet = argument_present("-nohelmet");
    g_input_config = fruityprime::settings::load_input(executable_directory);
    fruityprime::mods::InputSettings::BindRuntime(&g_input_config);
    const auto saved_menu_config = fruityprime::settings::load_menu(
        executable_directory);
    const auto saved_match_config = fruityprime::settings::load_match(
        executable_directory);
    fruityprime::mods::GameSettings::BindRuntime(
        &g_game_state, nullptr, nullptr, saved_menu_config.mph_version);
    fruityprime::mods::GameSettings::Apply(saved_menu_config);
    auto& render_options = fruityprime::mods::render::options();
    try {
        const auto render_toggle_argument =
            [](std::string_view flag, bool current) {
                if (!argument_present(flag)) {
                    return current;
                }
                const std::string value = argument_value(flag);
                if (value.empty() || value.front() == '-') {
                    throw std::invalid_argument(
                        std::string(flag) + " needs on or off");
                }
                return fruityprime::mods::render::Options::parse_on_off(
                    value, current);
            };
        render_options.set_cel_shading(render_toggle_argument(
            "-cel", render_options.cel_shading()));
        render_options.set_fog(render_toggle_argument(
            "-fog", render_options.fog()));
        if (argument_present("-fps") || argument_present("-showfps")) {
            const std::string flag = argument_present("-fps")
                ? "-fps" : "-showfps";
            render_options.set_show_fps(render_toggle_argument(
                flag, render_options.show_fps()));
        }
        if (argument_present("-prohud")) {
            const std::string value = argument_value("-prohud");
            if (value.empty() || value.front() == '-') {
                throw std::invalid_argument("-prohud needs on or off");
            }
            g_pro_hud = fruityprime::mods::render::Options::parse_on_off(
                value, g_pro_hud);
        }
        if (argument_present("-celbands")) {
            const std::string value = argument_value("-celbands");
            if (value.empty() || value.front() == '-') {
                throw std::invalid_argument("-celbands needs an integer");
            }
            render_options.set_cel_bands(
                fruityprime::mods::render::Options::parse_int(
                    value, render_options.cel_bands()));
        }
        if (argument_present("-celedge")) {
            const std::string value = argument_value("-celedge");
            if (value.empty() || value.front() == '-') {
                throw std::invalid_argument("-celedge needs a percentage");
            }
            const int percentage = fruityprime::mods::render::Options::parse_int(
                value, static_cast<int>(render_options.cel_edge() * 100.0F));
            render_options.set_cel_edge(
                static_cast<float>(percentage) / 100.0F);
        }
        if (argument_present("-resolution-scale")) {
            const std::string value = argument_value("-resolution-scale");
            if (value.empty() || value.front() == '-') {
                throw std::invalid_argument(
                    "-resolution-scale needs a percentage");
            }
            render_options.set_resolution_scale(
                fruityprime::mods::render::Options::parse_scale(
                    value, render_options.resolution_scale()));
        }
        const std::string model_path = argument_value("-model");
        std::string rom_path = argument_value("-rom");
        std::string room_name = argument_value("-room");
        // Not const: the launcher's demo picker sets it too, and the
        // replay path below reads it either way.
        std::string replay_path = argument_value("-replay");
        const std::string movie_argument = argument_value("-movie");
        // Not const: the launcher's join card sets it too, and the
        // connect path below reads it either way.
        std::string connect_host = argument_value("-connect");
        // Zero until something names one; -port and the launcher both do.
        int launcher_port = 0;
        const std::string netlag_argument = argument_value("-netlag");
        const std::string netloss_argument = argument_value("-netloss");
        if (argument_present("-netlag") && netlag_argument.empty()) {
            throw std::invalid_argument("-netlag needs a value");
        }
        if (argument_present("-netloss") && netloss_argument.empty()) {
            throw std::invalid_argument("-netloss needs a value");
        }
        const auto network_conditions =
            fruityprime::net::NetworkConditions::from_options(
                netlag_argument, netloss_argument);
        const std::string name_argument = argument_value("-name");
        const std::string hunter_argument = argument_value("-hunter");
        const std::string bots_argument = argument_value("-bots");
        const std::string bot_level_argument = argument_value("-bot-level");
        const std::string mode_argument = argument_value("-mode");
        const std::string match_time_argument = argument_value("-time");
        const std::string point_goal_argument = argument_value("-goal");
        const bool friendly_fire_argument = argument_present("-friendlyfire");
        adventure_argument = argument_present("-adventure");
        // Not const: the launcher's story card sets it too, and the
        // save load below reads it either way.
        bool new_game_argument = argument_present("-newgame");
        std::string player_name = name_argument.empty()
            ? "FruityPrime" : name_argument;
        int selected_hunter = hunter_argument.empty()
            ? 0 : std::stoi(hunter_argument);
        int bot_count = bots_argument.empty()
            ? 0 : std::clamp(std::stoi(bots_argument), 0, 7);
        g_bot_level = bot_level_argument.empty()
            ? 1 : std::clamp(std::stoi(bot_level_argument), 0, 2);
        const std::string seconds_argument = argument_value("-seconds");
        if (!seconds_argument.empty()) {
            run_seconds = std::max(0.0, std::stod(seconds_argument));
        }
        if (argument_present("-movie") && movie_argument.empty()) {
            throw std::invalid_argument("-movie needs a VX file or movie name");
        }
        std::uint8_t match_mode = saved_match_config.mode;
        const auto saved_mode_defaults = fruityprime::match::defaults_for_mode(
            match_mode);
        float match_time_limit = saved_match_config.time_limit_set
            ? saved_match_config.time_limit_seconds
            : saved_mode_defaults.time_limit_seconds;
        if (!match_time_argument.empty()) {
            match_time_limit = std::stof(match_time_argument);
            if (!std::isfinite(match_time_limit) || match_time_limit < 0.0F) {
                throw std::invalid_argument(
                    "-time must be a finite number of seconds >= 0");
            }
        }
        std::uint16_t point_goal = saved_match_config.point_goal_set
            ? saved_match_config.point_goal
            : saved_mode_defaults.point_goal;
        if (!point_goal_argument.empty()) {
            const int parsed_goal = std::stoi(point_goal_argument);
            if (parsed_goal < 0 || parsed_goal > 65'535) {
                throw std::invalid_argument("-goal must be between 0 and 65535");
            }
            point_goal = static_cast<std::uint16_t>(parsed_goal);
        }
        if (!mode_argument.empty()) {
            const int parsed_mode = std::stoi(mode_argument);
            if (parsed_mode < 0 || parsed_mode > 255) {
                throw std::invalid_argument("-mode must be between 0 and 255");
            }
            match_mode = static_cast<std::uint8_t>(parsed_mode);
        }
        if (match_time_argument.empty() && !saved_match_config.time_limit_set) {
            match_time_limit = fruityprime::match::defaults_for_mode(match_mode)
                .time_limit_seconds;
        }
        if (point_goal_argument.empty() && !saved_match_config.point_goal_set) {
            point_goal = fruityprime::match::defaults_for_mode(match_mode)
                .point_goal;
        }
        bool friendly_fire = saved_match_config.friendly_fire
            || friendly_fire_argument;
        const std::string mapdir_argument = argument_value("-mapdir");
        if (!mapdir_argument.empty()) {
            fruityprime::mapgen::custom_rooms::set_map_directory(
                fruityprime::utility::console::resolve_launch_path(
                    mapdir_argument));
        }
        static_cast<void>(fruityprime::mapgen::custom_rooms::generate_missing());
        // Reading the save is what turns "the story, slot N" into a
        // room, a mode and a set of limits.  The launcher can ask
        // for it as well as -adventure can, and it answers after
        // this point, so the work is named once and called twice.
        const auto load_story_save = [&]() {
            fruityprime::launcher::SaveStore saves(executable_directory);
            g_story_save = new_game_argument
                ? fruityprime::game::StorySave{}
                : saves.read(story_save_slot).value_or(
                    fruityprime::game::StorySave{});
            // SceneSetup.UpdateAreaHunters runs when the cockpit rebuilds the
            // encounter distribution. A fresh native process is equivalent
            // to a new save-slot visit, so reset the managed RNG pair before
            // mutating the save image used by the room and bot plan.
            fruityprime::utility::set_rng1(
                fruityprime::utility::Rng::Rng1StartValue);
            fruityprime::utility::set_rng2(
                fruityprime::utility::Rng::Rng2StartValue);
            fruityprime::scene_setup::update_area_hunters(
                g_game_state, *g_story_save,
                fruityprime::utility::global_rng(), true);
            room_name = fruityprime::launcher::SaveStore::start_room(
                *g_story_save);
            match_mode = static_cast<std::uint8_t>(
                fruityprime::game::Mode::Story);
            match_time_limit = fruityprime::match::defaults_for_mode(
                match_mode).time_limit_seconds;
            point_goal = fruityprime::match::defaults_for_mode(
                match_mode).point_goal;
            bot_count = 0;
            friendly_fire = false;
        };
        if (adventure_argument) {
            if (rom_path.empty()) {
                throw std::invalid_argument(
                    "-adventure requires -rom FILE");
            }
            if (!connect_host.empty() || !replay_path.empty()) {
                throw std::invalid_argument(
                    "-adventure cannot be combined with -connect or -replay");
            }
            const std::string slot_argument = argument_value("-save-slot");
            if (!slot_argument.empty()) {
                int parsed_slot = 0;
                try {
                    parsed_slot = std::stoi(slot_argument);
                } catch (const std::exception&) {
                    parsed_slot = 0;
                }
                if (parsed_slot < 1
                    || parsed_slot > static_cast<int>(
                        fruityprime::launcher::SlotCount)) {
                    throw std::invalid_argument(
                        "-save-slot must be between 1 and 3");
                }
                story_save_slot = static_cast<std::uint8_t>(parsed_slot);
            }
            load_story_save();
        }
        if (argument_present("-gamepad")) {
            return run_gamepad_probe(run_seconds > 0.0 ? run_seconds : 15.0);
        }
        // The match a recording was made in is read out of the file, and the
        // launcher's demo picker names one as well as -replay does, so this is
        // named once and called twice.
        const auto load_replay_match = [&]() {
            if (replay_path.empty()) {
                return;
            }
            if (rom_path.empty()) {
                throw std::invalid_argument(
                    "-replay requires -rom FILE for the recorded room");
            }
            fruityprime::net::MatchStatePacket replay_match;
            load_replay_file(replay_path, replay_match);
            if (room_name.empty()) {
                room_name = replay_match.room_key;
            }
            if (mode_argument.empty()) {
                match_mode = replay_match.mode;
            }
            if (match_time_argument.empty()) {
                match_time_limit = replay_match.time_remaining;
            }
            if (point_goal_argument.empty()) {
                point_goal = replay_match.point_goal;
            }
            if (!friendly_fire_argument) {
                friendly_fire = replay_match.friendly_fire();
            }
        };
        load_replay_match();
        if (!argument_present("-netprobe") && model_path.empty()
            && rom_path.empty() && connect_host.empty()
            && movie_argument.empty()
            && (argument_present("-launcher") || __argc <= 1)) {
            fruityprime::launcher::Selection launcher_defaults;
            launcher_defaults.mode = match_mode;
            launcher_defaults.time_limit_seconds = match_time_limit;
            launcher_defaults.point_goal = point_goal;
            launcher_defaults.friendly_fire = friendly_fire;
            const auto selection = fruityprime::launcher::choose_game(
                executable_directory,
                room_name.empty() ? "UNIT1_C0" : room_name, true,
                launcher_defaults);
            if (!selection.has_value()) {
                return EXIT_SUCCESS;
            }
            rom_path = selection->rom_path;
            room_name = selection->room_name;
            if (name_argument.empty()) {
                player_name = selection->player_name;
            }
            if (hunter_argument.empty()) {
                selected_hunter = selection->hunter;
            }
            if (bots_argument.empty()) {
                bot_count = selection->bots;
            }
            if (bot_level_argument.empty()) {
                g_bot_level = selection->bot_level;
            }
            if (mode_argument.empty()) {
                match_mode = selection->mode;
            }
            if (!selection->demo_path.empty()) {
                // The demo picker was what started this, so the argument it
                // stands in for is set: the replay path below is the one
                // -replay FILE takes.
                replay_path = selection->demo_path;
                load_replay_match();
            }
            if (!selection->connect_host.empty()) {
                // The join card was what started this, so the arguments it
                // stands in for are set: the connect path below is the same
                // one -connect HOST -port N takes.
                connect_host = selection->connect_host;
                launcher_port = selection->connect_port;
            }
            if (selection->adventure) {
                // The story card was the one that started this, so the
                // arguments it stands in for are set before the same setup
                // runs: the launcher is a way of typing -adventure.
                adventure_argument = true;
                story_save_slot = selection->save_slot;
                new_game_argument = selection->new_game;
                load_story_save();
            }
            if (match_time_argument.empty()) {
                match_time_limit = selection->time_limit_seconds;
            }
            if (point_goal_argument.empty()) {
                point_goal = selection->point_goal;
            }
            if (!friendly_fire_argument) {
                friendly_fire = selection->friendly_fire;
            }
        }
        g_local_name = player_name;
        g_local_hunter = static_cast<std::uint8_t>(
            std::clamp(selected_hunter, 0, 255));
        if (argument_present("-netprobe")) {
            if (connect_host.empty()) {
                throw std::invalid_argument("-netprobe requires -connect HOST");
            }
            const int requested_port = launcher_port > 0
                ? launcher_port
                : std::clamp(
                    std::stoi(argument_value("-port").empty()
                                  ? std::to_string(
                                      fruityprime::net::NetConfig::DefaultPort)
                                  : argument_value("-port")),
                    1, 65'535);
            const auto endpoint = fruityprime::net::NetClient::resolve_ipv4(
                connect_host, static_cast<std::uint16_t>(requested_port));
            g_net_client = std::make_unique<fruityprime::net::NetClient>(
                endpoint, network_conditions);
            if (!g_net_client->connect()) {
                throw std::runtime_error(
                    "native GUI network probe failed: "
                    + std::string(g_net_client->last_error()));
            }
            g_net_client->identify(0, "Native GUI probe");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            g_net_log.close();
            return EXIT_SUCCESS;
        }
        if (!movie_argument.empty()) {
            if (adventure_argument || !replay_path.empty()
                || !connect_host.empty()) {
                throw std::invalid_argument(
                    "-movie cannot be combined with -adventure, -replay, or -connect");
            }
            const std::filesystem::path direct_movie(movie_argument);
            if (std::filesystem::is_regular_file(direct_movie)
                && (direct_movie.extension() == ".vx"
                    || direct_movie.extension() == ".VX")) {
                g_movie_player.emplace(
                    fruityprime::movie::Player::read_file(direct_movie));
            } else {
                if (rom_path.empty()) {
                    throw std::invalid_argument(
                        "-movie NAME requires -rom ROM_OR_DIRECTORY");
                }
                const auto assets = fruityprime::assets::Store::from_path(
                    rom_path);
                std::string movie_path = direct_movie.generic_string();
                for (char& value : movie_path) {
                    if (value == '\\') {
                        value = '/';
                    }
                }
                if (!movie_path.starts_with("movies/")) {
                    movie_path = "movies/" + movie_path;
                }
                g_movie_player.emplace(
                    fruityprime::movie::Player::from_bytes(
                        assets.bytes(movie_path)));
            }
            if (g_movie_player->frame_count() == 0) {
                throw std::runtime_error("-movie contains no video frames");
            }
            g_movie_player->play();
            start_movie_audio();
        } else if (!rom_path.empty() && !room_name.empty()) {
            g_assets = std::make_unique<fruityprime::assets::Store>(
                fruityprime::assets::Store::from_path(rom_path));
            const auto& assets = *g_assets;
            static_cast<void>(
                fruityprime::utility::extract::load_runtime_data(assets));
            // The sound catalog is a ROM-side dependency of the game host,
            // but an incomplete extracted tree should not prevent the native
            // renderer from starting. SfxRuntime remains fully usable in
            // headless tests even when WinMM has no output device.
            try {
                g_sound_catalog.emplace(fruityprime::sound::Catalog::load(
                    assets));
                g_sfx_mixer = std::make_unique<
                    fruityprime::mods::sound::SfxMixer>(32728);
                g_sfx_runtime = std::make_unique<fruityprime::sound::SfxRuntime>(
                    *g_sound_catalog, *g_sfx_mixer);
                if (!g_sfx_runtime->load()) {
                    g_music_runtime.reset();
                    g_music_controller.reset();
                    g_sfx_runtime.reset();
                    g_sfx_mixer.reset();
                    g_sound_catalog.reset();
                } else {
                    g_music_controller = std::make_unique<
                        fruityprime::sound::MusicController>(*g_sound_catalog);
                    g_music_controller->init();
                    g_music_runtime = std::make_unique<
                        fruityprime::sound::MusicRuntime>(
                            *g_sound_catalog, *g_music_controller, *g_sfx_mixer);
                    fruityprime::sound::MusicPlayer::BindRuntime(
                        g_music_runtime.get());
                }
            } catch (const std::exception& error) {
                OutputDebugStringA((std::string(
                    "Fruity Prime native audio disabled: ")
                    + error.what() + "\n").c_str());
                g_music_runtime.reset();
                g_music_controller.reset();
                g_sfx_runtime.reset();
                g_sfx_mixer.reset();
                g_sound_catalog.reset();
            }
            fruityprime::mods::GameSettings::BindRuntime(
                &g_game_state, g_sfx_runtime.get(), g_music_controller.get(),
                g_assets->game_key().value_or(saved_menu_config.mph_version));
            const auto* catalog_entry = fruityprime::scene::find_room(
                room_name);
            fruityprime::scene::RoomDefinition definition = catalog_entry != nullptr
                ? catalog_entry->definition
                : fruityprime::scene::RoomDefinition{
                    room_name,
                    "archives/unit1_C0.arc",
                    "unit1_c0_model.bin",
                    "levels/textures/unit1_c0_tex.bin",
                    "unit1_c0_collision.bin",
                    "levels/entities/Unit1_C0_Ent.bin", {}, {}, {}
                };
            if (!argument_value("-archive").empty()) {
                definition.model_archive = argument_value("-archive");
            }
            if (!argument_value("-model-entry").empty()) {
                definition.model_entry = argument_value("-model-entry");
            }
            if (!argument_value("-texture").empty()) {
                definition.texture_path = argument_value("-texture");
            }
            if (!argument_value("-collision-entry").empty()) {
                definition.collision_entry = argument_value("-collision-entry");
            }
            if (!argument_value("-entity").empty()) {
                definition.entity_path = argument_value("-entity");
            }
            if (catalog_entry != nullptr) {
                if (const auto* metadata =
                        fruityprime::metadata::find_room_metadata(
                            catalog_entry->name);
                    metadata != nullptr && !metadata->node_path.empty()) {
                    definition.node_path = metadata->node_path;
                }
                definition.node_path =
                    fruityprime::scene_setup::resolve_node_data_path(
                        definition.node_path, catalog_entry->id,
                        static_cast<fruityprime::game::Mode>(match_mode));
            }
            g_room.emplace(fruityprime::scene::Room::load(assets, definition));
            name_window_after_room(*catalog_entry);
            set_room_lights(catalog_entry->id);
            if (!adventure_argument) {
                load_intro_sequence(assets, catalog_entry->id);
            }
            apply_room_node_layers(
                *g_room, adventure_argument, bot_count + 1,
                match_mode == static_cast<std::uint8_t>(
                    fruityprime::game::Mode::Capture));
            g_room_instance.emplace(g_room->model());
            update_model_instance(*g_room_instance,
                                  /*use_node_transform=*/false);
            append_model_geometry(g_room->model(), DrawLayer::Room);
            load_entity_models(assets);
            if (adventure_argument && g_story_save.has_value()
                && catalog_entry != nullptr && g_static_world != nullptr) {
                g_static_world->apply_story_save(
                    catalog_entry->id, *g_story_save);
            }
            load_hunter_models(assets);
            load_hud_assets(assets, static_cast<std::uint8_t>(
                std::clamp(selected_hunter, 0, 255)));
            if (!connect_host.empty()) {
                const int requested_port = launcher_port > 0
                    ? launcher_port
                    : std::clamp(
                        std::stoi(argument_value("-port").empty()
                                      ? std::to_string(
                                          fruityprime::net::NetConfig::DefaultPort)
                                      : argument_value("-port")),
                        1, 65'535);
                const auto endpoint = fruityprime::net::NetClient::resolve_ipv4(
                    connect_host, static_cast<std::uint16_t>(requested_port));
                g_net_client = std::make_unique<fruityprime::net::NetClient>(
                    endpoint, network_conditions);
                if (!g_net_client->connect()) {
                    throw std::runtime_error(
                        "native game network connect failed: "
                        + std::string(g_net_client->last_error())
                        + " host=" + connect_host + " port="
                        + std::to_string(requested_port));
                }
                static_cast<void>(g_net_log.open(executable_directory,
                                                 player_name));
                g_net_log.event("joining " + connect_host + ":"
                    + std::to_string(requested_port) + " as \""
                    + player_name + "\"");
                const int hunter = std::clamp(selected_hunter, 0, 255);
                g_net_client->identify(static_cast<std::uint8_t>(hunter),
                                       player_name);
                g_local_slot = static_cast<std::uint8_t>(std::clamp(
                    fruityprime::net::NetHooks::local_slot(
                        true, g_net_client->local_slot(), false),
                    0, static_cast<int>(fruityprime::net::NetConfig::SlotCapacity - 1)));
            } else {
                g_local_slot = static_cast<std::uint8_t>(
                    fruityprime::net::NetHooks::local_slot(false, -1, false));
            }
            fruityprime::launcher::LaunchPlan launch_plan;
            launch_plan.kind = adventure_argument
                ? fruityprime::launcher::LaunchKind::Adventure
                : g_net_client == nullptr
                    ? fruityprime::launcher::LaunchKind::Offline
                    : fruityprime::launcher::LaunchKind::Online;
            launch_plan.hunter = static_cast<fruityprime::metadata::Hunter>(
                std::clamp(selected_hunter, 0, 255));
            launch_plan.room_key = room_name;
            launch_plan.mode = match_mode;
            launch_plan.bots = bot_count;
            launch_plan.bot_level = g_bot_level;
            launch_plan.save_slot = story_save_slot;
            launch_plan.new_game = new_game_argument;
            fruityprime::launcher::MatchStartOptions start_options;
            start_options.time_limit_seconds = match_time_limit;
            start_options.point_goal = point_goal;
            start_options.team_play = saved_match_config.team_play;
            start_options.friendly_fire = friendly_fire;
            start_options.objective_authority = g_net_client == nullptr
                && replay_path.empty();
            start_options.local_slot = g_local_slot;
            const auto prepared = fruityprime::launcher::MatchStart::prepare(
                launch_plan, start_options);
            if (!prepared.ok) {
                throw std::runtime_error("match preparation failed: "
                                         + prepared.error);
            }
            g_gameplay_config = prepared.gameplay;
            g_slot_manager.reset();
            g_net_player_bridge.reset();
            g_net_damage.reset();
            g_net_match_end.reset();
            g_net_match_sync.reset();
            g_net_room_change.reset();
            g_server_match_state.reset();
            g_spectator.reset();
            fruityprime::net::NetPlayerSetup::Reset();
            g_session.emplace(*g_room, *g_gameplay_config);
            fruityprime::players::PlayerEntity::Reset();
            fruityprime::players::PlayerEntity::Construct(*g_session,
                                                           &g_game_state);
            bind_gorea_model_to_session();
            attach_gameplay_message_sink();
            if (g_music_controller != nullptr && g_music_runtime != nullptr
                && catalog_entry != nullptr) {
                fruityprime::sound::Music::TryPlayRoomMusic(catalog_entry->id, 0);
                g_music_runtime->update(0.0F);
            }
            if (g_effect_runtime != nullptr) {
                g_effect_runtime->clear();
                g_effect_runtime_seen.clear();
            }
            g_match_flow.emplace(prepared.rules);
            g_game_state.begin_room(
                room_name,
                catalog_entry == nullptr ? 0U
                                          : static_cast<std::uint32_t>(
                                                catalog_entry->id),
                prepared.mode != static_cast<std::uint8_t>(
                    fruityprime::game::Mode::SinglePlayer),
                static_cast<fruityprime::game::Mode>(prepared.mode));
            fruityprime::mods::GameSettings::ApplyMatchRules();
            g_game_state.local_slot = g_local_slot;
            g_game_state.friendly_fire = friendly_fire;
            g_game_state.match_time = prepared.rules.time_limit_seconds;
            g_game_state.match_clock_enabled = prepared.rules.time_limit_seconds
                > 0.0F;
            g_game_state.point_goal = prepared.rules.point_goal;
            g_game_state.teams = prepared.rules.team_mode;
            g_bot_slots.clear();
            std::string populate_error;
            if (!fruityprime::launcher::MatchStart::populate(
                    *g_session, prepared, &g_bot_slots, &populate_error)) {
                throw std::runtime_error("player setup failed: "
                                         + populate_error);
            }
            if (adventure_argument && g_story_save.has_value()) {
                g_session->apply_story_save(g_local_slot, *g_story_save);
                if (catalog_entry != nullptr) {
                    g_session->apply_story_room_state(
                        catalog_entry->id, *g_story_save);
                    // SceneSetup creates story hunters from the room's S09
                    // spawners before the normal frame loop. Keep that
                    // decision in the same helper used after a teleporter
                    // room transition.
                    populate_story_hunters(*catalog_entry);
                }
                // Keep the process-wide GameState mirror on the same save
                // image as the launcher/static-world path.  This matters for
                // story messages handled after room construction.
                g_game_state.story_save = *g_story_save;
            }
            sync_process_game_state();

            // Keep the native visual-comparison launch observable even when
            // the Win32 window is captured before the first meaningful frame.
            // This is intentionally beside the executable so a screenshot
            // run can distinguish an asset/render failure from a missing
            // gameplay session without attaching a debugger.
            {
                std::ofstream startup_state(
                    executable_directory / "FruityPrime-native-startup-state.log",
                    std::ios::trunc);
                startup_state << "session=" << (g_session.has_value() ? 1 : 0)
                              << " players="
                              << (g_session.has_value()
                                      ? g_session->players().size() : 0)
                              << " local_slot="
                              << static_cast<unsigned>(g_local_slot)
                              << " room=" << room_name << '\n';
            }

            const std::string demo_output = argument_value("-demoout");
            if ((argument_present("-recorddemo") || !demo_output.empty())
                && g_net_client != nullptr && g_net_client->connected()) {
                g_demo_path = demo_output.empty()
                    ? fruityprime::demo::Recorder::default_path(
                        executable_directory / "export", room_name)
                    : std::filesystem::path(demo_output);
                if (!g_demo_recorder.start(g_demo_path, demo_frame())) {
                    throw std::runtime_error(
                        "could not start demo recording: "
                        + g_demo_path.string());
                }
            }
        } else if (!connect_host.empty()) {
            throw std::invalid_argument(
                "-connect requires -rom and -room for the native game host");
        } else if (!rom_path.empty()) {
            const std::string archive_path = argument_value("-archive");
            const std::string entry_name = argument_value("-entry");
            if (archive_path.empty() || entry_name.empty()) {
                throw std::invalid_argument(
                    "-rom requires -archive and -entry");
            }
            const auto assets = fruityprime::assets::Store::from_path(rom_path);
            const auto model = assets.model_from_archive(archive_path, entry_name);
            append_model_geometry(model, DrawLayer::Room);
        } else if (!model_path.empty()) {
            const auto model = fruityprime::model::File::read_file(model_path);
            append_model_geometry(model, DrawLayer::Room);
        }
    } catch (const std::exception& error) {
        g_error = error.what();
        if (argument_value("-seconds").empty()
            && !argument_present("-netprobe")) {
            MessageBoxA(nullptr, g_error.c_str(), "Fruity Prime native model",
                        MB_OK | MB_ICONERROR);
        } else {
            std::error_code error_code;
            const auto executable = std::filesystem::absolute(
                g_command_arguments.empty() ? std::string{} : g_command_arguments[0],
                error_code);
            const auto log_path = !error_code && executable.has_parent_path()
                ? executable.parent_path() / "FruityPrime-native-error.log"
                : std::filesystem::current_path()
                    / "FruityPrime-native-error.log";
            std::ofstream log(log_path, std::ios::trunc);
            log << g_error << '\n';
        }
        return EXIT_FAILURE;
    }

    WNDCLASSEXA window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = "FruityPrimeNativeWindow";
    if (RegisterClassExA(&window_class) == 0) {
        return EXIT_FAILURE;
    }
    HWND window = CreateWindowExA(
        0, window_class.lpszClassName,
        fruityprime::branding::Name,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 720,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        return EXIT_FAILURE;
    }

    g_window = window;
    if (!g_window_title.empty()) {
        SetWindowTextA(window, g_window_title.c_str());
    }
    HDC device_context = GetDC(window);
    g_device_context = device_context;
    PIXELFORMATDESCRIPTOR pixel_format{};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
        | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cDepthBits = 24;
    pixel_format.cStencilBits = 8;
    const int format = ChoosePixelFormat(device_context, &pixel_format);
    if (format == 0 || SetPixelFormat(device_context, format, &pixel_format) == FALSE) {
        ReleaseDC(window, device_context);
        DestroyWindow(window);
        return EXIT_FAILURE;
    }
    HGLRC context = wglCreateContext(device_context);
    if (context == nullptr || wglMakeCurrent(device_context, context) == FALSE) {
        if (context != nullptr) {
            wglDeleteContext(context);
        }
        ReleaseDC(window, device_context);
        DestroyWindow(window);
        return EXIT_FAILURE;
    }

    create_gl_textures();

    ShowWindow(window, show_command);
    UpdateWindow(window);
    apply_window_startup(window);
    MSG message{};
    bool running = true;
    auto previous = std::chrono::steady_clock::now();
    const auto started = previous;
    double accumulator = 0.0;
    constexpr double fixed_seconds = 1.0 / 60.0;
    while (running) {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (running) {
            const auto now = std::chrono::steady_clock::now();
            if (run_seconds > 0.0
                && std::chrono::duration<double>(now - started).count()
                    >= run_seconds) {
                break;
            }
            const double elapsed = std::chrono::duration<double>(now - previous).count();
            previous = now;
            accumulator += std::min(elapsed, 0.25);
            while (accumulator >= fixed_seconds) {
                update_game(window);
                accumulator -= fixed_seconds;
            }
            draw_frame(window);
        }
    }
    if (g_scene_scale_texture != 0) {
        glDeleteTextures(1, &g_scene_scale_texture);
        g_scene_scale_texture = 0;
        g_scene_scale_width = 0;
        g_scene_scale_height = 0;
    }
    if (g_movie_texture != 0) {
        glDeleteTextures(1, &g_movie_texture);
        g_movie_texture = 0;
        g_movie_texture_width = 0;
        g_movie_texture_height = 0;
        g_movie_uploaded_frame = std::numeric_limits<std::size_t>::max();
    }
    for (auto& image : g_texture_images) {
        if (image.gl_id != 0) {
            glDeleteTextures(1, &image.gl_id);
            image.gl_id = 0;
        }
    }
    const auto destroy_hud_texture = [](std::optional<HudImage>& image) {
        if (image.has_value() && image->gl_id != 0) {
            glDeleteTextures(1, &image->gl_id);
            image->gl_id = 0;
        }
    };
    const auto destroy_hud_object =
        [](std::optional<HudObjectImage>& object) {
            if (!object.has_value()) {
                return;
            }
            for (auto& image : object->images) {
                if (image.gl_id != 0) {
                    glDeleteTextures(1, &image.gl_id);
                    image.gl_id = 0;
                }
            }
        };
    destroy_hud_texture(g_hud_reticle);
    destroy_hud_texture(g_hud_ice);
    destroy_hud_texture(g_hud_helmet);
    destroy_hud_texture(g_hud_helmet_drop);
    destroy_hud_texture(g_hud_visor);
    destroy_hud_texture(g_hud_scan_visor);
    destroy_hud_object(g_hud_health_bar);
    destroy_hud_object(g_hud_health_bar_sub);
    destroy_hud_object(g_hud_ammo_bar);
    destroy_hud_object(g_hud_weapon_icon);
    if (g_demo_recorder.recording()) {
        g_demo_recorder.stop();
        OutputDebugStringA(("Fruity Prime demo saved to "
            + g_demo_path.string() + "\n").c_str());
    }
    g_net_log.close();
    if (adventure_argument && g_story_save.has_value()) {
        fruityprime::launcher::SaveStore saves(executable_directory);
        if (!saves.commit(story_save_slot, *g_story_save)) {
            OutputDebugStringA("Fruity Prime could not commit the story save\n");
        }
    }
    g_music_runtime.reset();
    fruityprime::sound::MusicPlayer::BindRuntime(nullptr);
    fruityprime::mods::GameSettings::BindRuntime(
        &g_game_state, nullptr, nullptr,
        g_assets != nullptr
            ? g_assets->game_key().value_or(saved_menu_config.mph_version)
            : saved_menu_config.mph_version);
    g_music_controller.reset();
    g_sfx_runtime.reset();
    if (g_sfx_mixer != nullptr) {
        if (g_movie_audio_source != 0) {
            g_sfx_mixer->stop(g_movie_audio_source);
            g_sfx_mixer->delete_source(g_movie_audio_source);
            g_movie_audio_source = 0;
        }
        if (g_movie_audio_buffer != 0) {
            g_sfx_mixer->delete_buffer(g_movie_audio_buffer);
            g_movie_audio_buffer = 0;
        }
    }
    g_sfx_mixer.reset();
    g_sound_catalog.reset();
    g_gameplay_config.reset();
    g_assets.reset();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, device_context);
    DestroyWindow(window);
    return EXIT_SUCCESS;
}

#else

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    fruityprime::utility::console::run();
    std::cout << "Fruity Prime native C++ game host requires the Windows OpenGL "
                 "window target on this build.\n";
    if (argc > 1 && std::string_view(argv[1]) == "-model") {
        std::cout << "model argument: " << (argc > 2 ? argv[2] : "<missing>")
                  << '\n';
    }
    return 0;
}

#endif

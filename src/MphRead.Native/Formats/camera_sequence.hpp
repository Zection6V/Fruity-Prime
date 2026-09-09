#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Native counterpart of Entities/CamSeq/CameraSequence.cs.  The parser keeps
// the cartridge keyframe values in a renderer-independent representation and
// the sampler reproduces the authored camera path without requiring an OpenGL
// context.  A live scene supplies entity poses through the resolver callback.
namespace fruityprime::camera {

// CameraSequence.Filenames equivalent.  An empty result means that the
// sequence id is outside the cartridge table.
[[nodiscard]] std::string_view asset_name(int sequence_id) noexcept;
[[nodiscard]] std::string asset_path(int sequence_id);

// CameraSequence.MusicData and CameraSequence.SfxData.  These are indexed by
// the same 199 sequence IDs as Filenames; the high bits are authored routing
// flags and must remain part of the values.
inline constexpr std::array<std::int32_t, 199> MusicData{{
    28, 27, 29, 30, 0, 0, 0, 0,
    0, 0, 0, 16385, 0, 0, 0, 16414,
    0, 0, 16387, 0, 16389, 0, 32806, 0,
    0, 0, 0, 0, 17424, 17422, 0, 16433,
    16432, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 32821, 16393,
    16421, 16397, 0, 0, 18473, 0, 20499, 20531,
    20537, 24620, 0, 16394, 16389, 0, 0, 0,
    0, 0, 0, 0, 0, 16400, 16444, 20530,
    20533, 20530, 20533, 0, 20533, 0, 0, 0,
    0, 0, 0, 20532, 0, 20480, 0, 0,
    16423, 20480, 0, 0, 0, 0, 0, 0,
    0, 16385, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 20533,
    0, 0, 0, 0, 0, 16398, 0, 0,
    0, 0, 0, 0, 16412, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 59,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0
}};

inline constexpr std::array<std::int32_t, 199> SfxData{{
    32860, 32785, 32862, 32861, 0, 0, 19, 0,
    0, 19, 0, 0, 19, 0, 19, 0,
    19, 19, 0, 0, 0, 32786, 32788, 0,
    0, 19, 0, 0, 0, 19, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 70, 0, 8211, 0, 69, 16450, 0,
    0, 21, 0, 32863, 0, 0, 98, 80,
    90, 0, 0, 0, 0, 0, 0, 0,
    0, 19, 0, 19, 19, 0, 19, 72,
    71, 0, 79, 73, 0, 19, 0, 0,
    0, 0, 0, 19, 0, 22, 0, 0,
    0, 19, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 19, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 19, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 16449, 84, 0, 0,
    0, 97, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 19,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 19, 0, 0, 0, 32867,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0
}};

enum class Flags : std::uint8_t {
    None = 0,
    Complete = 1 << 0,
    CanEnd = 1 << 1,
    BlockInput = 1 << 2,
    ForceAlt = 1 << 3,
    ForceBiped = 1 << 4,
    Loop = 1 << 5,
};

[[nodiscard]] constexpr Flags operator|(Flags left, Flags right) noexcept {
    return static_cast<Flags>(static_cast<std::uint8_t>(left)
                              | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr Flags operator&(Flags left, Flags right) noexcept {
    return static_cast<Flags>(static_cast<std::uint8_t>(left)
                              & static_cast<std::uint8_t>(right));
}

constexpr Flags& operator|=(Flags& left, Flags right) noexcept {
    left = left | right;
    return left;
}

constexpr Flags& operator&=(Flags& left, Flags right) noexcept {
    left = left & right;
    return left;
}

[[nodiscard]] constexpr bool has_flag(Flags value, Flags flag) noexcept {
    return (static_cast<std::uint8_t>(value)
            & static_cast<std::uint8_t>(flag)) != 0;
}

struct Header {
    static constexpr std::size_t Size = 8;

    std::uint16_t count = 0;
    std::uint8_t version = 0;
    std::uint8_t padding_3 = 0;
    std::uint32_t padding_4 = 0;
};

// RawCameraSequenceKeyframe is 100 bytes in the cartridge.  Strings are
// decoded to UTF-8-compatible ASCII here; the game files use ASCII node names.
struct Keyframe {
    static constexpr std::size_t Size = 100;

    formats::Vector3 position;
    formats::Vector3 to_target;
    float roll = 0.0F;
    float fov = 0.0F;
    float move_time = 0.0F;
    float hold_time = 0.0F;
    float fade_in_time = 0.0F;
    float fade_out_time = 0.0F;
    formats::FadeType fade_in_type = formats::FadeType::None;
    formats::FadeType fade_out_type = formats::FadeType::None;
    std::uint8_t prev_frame_influence = 0;
    std::uint8_t after_frame_influence = 0;
    bool use_entity_transform = false;
    std::int16_t pos_entity_type = -1;
    std::int16_t pos_entity_id = -1;
    std::int16_t target_entity_type = -1;
    std::int16_t target_entity_id = -1;
    std::int16_t message_target_type = -1;
    std::int16_t message_target_id = -1;
    std::uint16_t message_id = 0;
    std::uint16_t message_param = 0;
    float easing = 0.0F;
    std::string node_name;
};

struct Sample {
    formats::Vector3 position;
    formats::Vector3 target;
    formats::Vector3 up_vector{0.0F, 1.0F, 0.0F};
    float roll = 0.0F;
    float fov = 0.0F;
};

// The managed CameraSequence resolves these references against live Scene
// entities before it evaluates a keyframe.  Keep the resolver small and
// renderer-independent so the Win32 host, replay path, and future Android
// scene can provide the same transform data.
struct EntityPose {
    formats::Vector3 position;
    formats::Vector3 up{0.0F, 1.0F, 0.0F};
    formats::Vector3 facing{0.0F, 0.0F, 1.0F};
};

using EntityResolver = std::function<std::optional<EntityPose>(
    std::int16_t type, std::int16_t id)>;
struct MessageEvent {
    std::uint16_t message = 0;
    std::int16_t target_type = -1;
    std::int16_t target_id = -1;
    std::uint16_t parameter = 0;
};

using MessageSink = std::function<void(const MessageEvent& event)>;
using FadeSink = std::function<void(formats::FadeType type, float seconds,
                                    bool overwrite)>;
using FormLockSink = std::function<void(bool alt_form)>;

struct CameraState {
    formats::Vector3 position;
    formats::Vector3 previous_position;
    formats::Vector3 target;
    formats::Vector3 up_vector{0.0F, 1.0F, 0.0F};
    formats::Vector3 facing{0.0F, 0.0F, 1.0F};
    float fov = 0.0F;
    float shake = 0.0F;
    std::string node_name;
};

struct ResolvedKeyframe {
    formats::Vector3 position;
    formats::Vector3 to_target;
};

class File {
public:
    [[nodiscard]] static File read_file(const std::filesystem::path& path,
                                        int id = -1,
                                        std::string name = {});
    [[nodiscard]] static File from_bytes(std::vector<std::uint8_t> bytes,
                                         int id = -1,
                                         std::string name = {});

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const Header& header() const noexcept { return header_; }
    [[nodiscard]] std::uint8_t version() const noexcept {
        return header_.version;
    }
    [[nodiscard]] const std::vector<Keyframe>& keyframes() const noexcept {
        return keyframes_;
    }
    [[nodiscard]] float duration() const noexcept;

    // Returns the camera state at elapsed time within the selected keyframe.
    // The value is clamped to the authored keyframe interval.  The managed
    // implementation applies live entity transforms before this calculation;
    // native callers can do the same by adjusting Keyframe values first.
    [[nodiscard]] Sample sample(std::size_t keyframe_index,
                                float elapsed) const;

private:
    explicit File(std::vector<std::uint8_t> bytes, int id,
                  std::string name);

    void parse();

    std::vector<std::uint8_t> bytes_;
    int id_ = -1;
    std::string name_;
    Header header_;
    std::vector<Keyframe> keyframes_;
};

// Runtime counterpart of the managed CameraSequence playback object.  File
// remains an immutable cartridge resource; Playback owns only mutable timing
// and camera state.  Callers can attach scene/message/fade callbacks without
// making this layer depend on Scene, OpenGL, Win32, or Android.
class Playback {
public:
    Playback(int sequence_id, const File& file);

    [[nodiscard]] int sequence_id() const noexcept { return sequence_id_; }
    [[nodiscard]] const File& file() const noexcept { return file_; }

    // CameraSequence.IsIntro: sequences 172 through 198 are the multiplayer
    // arena intros.  They are the ones the match start can cut short, so
    // whether a sequence is one decides what a skip does.
    [[nodiscard]] bool is_intro() const noexcept {
        return sequence_id_ >= 172 && sequence_id_ <= 198;
    }
    [[nodiscard]] Flags flags() const noexcept { return flags_; }
    void set_flags(Flags flags) noexcept { flags_ = flags; }
    [[nodiscard]] bool complete() const noexcept {
        return has_flag(flags_, Flags::Complete);
    }
    [[nodiscard]] bool can_end() const noexcept {
        return has_flag(flags_, Flags::CanEnd);
    }
    [[nodiscard]] bool block_input() const noexcept {
        return has_flag(flags_, Flags::BlockInput);
    }
    [[nodiscard]] bool force_alt() const noexcept {
        return has_flag(flags_, Flags::ForceAlt);
    }
    [[nodiscard]] bool force_biped() const noexcept {
        return has_flag(flags_, Flags::ForceBiped);
    }
    [[nodiscard]] std::uint16_t transition_timer() const noexcept {
        return transition_timer_;
    }
    [[nodiscard]] std::uint16_t transition_time() const noexcept {
        return transition_time_;
    }
    [[nodiscard]] std::size_t keyframe_index() const noexcept {
        return keyframe_index_;
    }
    [[nodiscard]] float keyframe_elapsed() const noexcept {
        return keyframe_elapsed_;
    }
    [[nodiscard]] const CameraState& camera() const noexcept {
        return camera_;
    }
    [[nodiscard]] const CameraState& initial_camera() const noexcept {
        return initial_camera_;
    }

    void set_entity_resolver(EntityResolver resolver) {
        entity_resolver_ = std::move(resolver);
    }
    void set_message_sink(MessageSink sink) { message_sink_ = std::move(sink); }
    void set_fade_sink(FadeSink sink) { fade_sink_ = std::move(sink); }
    void set_form_lock_sink(FormLockSink sink) {
        form_lock_sink_ = std::move(sink);
    }

    void set_up(CameraState camera, std::uint16_t transition_time = 0);
    void restart(std::uint16_t transition_timer = 0,
                 std::uint16_t transition_time = 0);
    void end() noexcept;
    void process(float frame_seconds);

private:
    [[nodiscard]] ResolvedKeyframe resolve_keyframe(
        const Keyframe& keyframe) const;
    [[nodiscard]] Sample calculate_sample(float elapsed) const;
    void calculate_frame_values();
    void emit_fade(const Keyframe& keyframe, float frame_length,
                   bool first_frame) const;

    int sequence_id_ = -1;
    const File& file_;
    Flags flags_ = Flags::None;
    std::uint16_t transition_timer_ = 0;
    std::uint16_t transition_time_ = 0;
    std::size_t keyframe_index_ = 0;
    float keyframe_elapsed_ = 0.0F;
    CameraState camera_;
    CameraState initial_camera_;
    EntityResolver entity_resolver_;
    MessageSink message_sink_;
    FadeSink fade_sink_;
    FormLockSink form_lock_sink_;
};

} // namespace fruityprime::camera

namespace MphReadNative {
namespace CameraSequence = ::fruityprime::camera;
}

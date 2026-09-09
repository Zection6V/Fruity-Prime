#pragma once

#include <string>
#include <string_view>

namespace fruityprime::mods::render {

// Native counterpart of Mods/RenderOptions.cs. The state is frontend-neutral
// so a Win32, Vulkan, or Android renderer can read the same live settings.
class Options final {
public:
    static constexpr int MinScale = 25;

    [[nodiscard]] int resolution_scale() const noexcept {
        return resolution_scale_;
    }
    void set_resolution_scale(int value) noexcept;

    [[nodiscard]] bool lighting() const noexcept { return lighting_; }
    void set_lighting(bool value) noexcept { lighting_ = value; }
    [[nodiscard]] bool cel_shading() const noexcept { return cel_shading_; }
    void set_cel_shading(bool value) noexcept { cel_shading_ = value; }
    [[nodiscard]] bool show_fps() const noexcept { return show_fps_; }
    void set_show_fps(bool value) noexcept { show_fps_ = value; }
    [[nodiscard]] int cel_bands() const noexcept { return cel_bands_; }
    void set_cel_bands(int value) noexcept;
    [[nodiscard]] float cel_edge() const noexcept { return cel_edge_; }
    void set_cel_edge(float value) noexcept;
    [[nodiscard]] bool fog() const noexcept { return fog_; }
    void set_fog(bool value) noexcept { fog_ = value; }
    [[nodiscard]] bool texture_filtering() const noexcept {
        return texture_filtering_;
    }
    void set_texture_filtering(bool value) noexcept {
        texture_filtering_ = value;
    }

    [[nodiscard]] int scaled(int pixels) const noexcept;

    [[nodiscard]] static bool parse_on_off(std::string_view value,
                                            bool fallback) noexcept;
    [[nodiscard]] static std::string on_off(bool value);
    [[nodiscard]] static int parse_scale(std::string_view value,
                                          int fallback) noexcept;
    [[nodiscard]] static int parse_int(std::string_view value,
                                        int fallback) noexcept;

private:
    int resolution_scale_ = 100;
    bool lighting_ = true;
    bool cel_shading_ = false;
    bool show_fps_ = false;
    int cel_bands_ = 8;
    float cel_edge_ = 0.5F;
    bool fog_ = true;
    bool texture_filtering_ = false;
};

Options& options() noexcept;

} // namespace fruityprime::mods::render

namespace fruityprime::mods {

// C++ surface for the managed static Mods.RenderOptions class. The existing
// Options value API remains as a compatibility adapter for native renderer
// call sites; these methods all address the same process-wide state.
class RenderOptions final {
public:
    static constexpr int MinScale = render::Options::MinScale;

    [[nodiscard]] static int ResolutionScale() noexcept;
    static void SetResolutionScale(int value) noexcept;
    [[nodiscard]] static bool Lighting() noexcept;
    static void SetLighting(bool value) noexcept;
    [[nodiscard]] static bool CelShading() noexcept;
    static void SetCelShading(bool value) noexcept;
    [[nodiscard]] static bool ShowFps() noexcept;
    static void SetShowFps(bool value) noexcept;
    [[nodiscard]] static int CelBands() noexcept;
    static void SetCelBands(int value) noexcept;
    [[nodiscard]] static float CelEdge() noexcept;
    static void SetCelEdge(float value) noexcept;
    [[nodiscard]] static bool Fog() noexcept;
    static void SetFog(bool value) noexcept;
    [[nodiscard]] static bool TextureFiltering() noexcept;
    static void SetTextureFiltering(bool value) noexcept;

    [[nodiscard]] static int Scaled(int pixels) noexcept;
    [[nodiscard]] static bool ParseOnOff(std::string_view value,
                                         bool fallback) noexcept;
    [[nodiscard]] static std::string OnOff(bool value);
    [[nodiscard]] static int ParseScale(std::string_view value,
                                        int fallback) noexcept;
    [[nodiscard]] static int ParseInt(std::string_view value,
                                      int fallback) noexcept;
};

} // namespace fruityprime::mods

namespace MphReadNative::Mods {
using RenderOptions = ::fruityprime::mods::RenderOptions;
}

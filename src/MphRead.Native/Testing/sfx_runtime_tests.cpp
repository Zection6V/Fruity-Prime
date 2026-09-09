#include "Mods/Sound/sfx_mixer.hpp"
#include "Sound/sfx_runtime.hpp"

#include "Assets/game_assets.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fruityprime::sound::Sample make_sample(std::initializer_list<std::uint8_t> data,
                                       std::uint16_t sample_rate = 22050) {
    fruityprime::sound::Sample sample;
    sample.present = true;
    sample.format = fruityprime::sound::WaveFormat::Pcm8;
    sample.sample_rate = sample_rate;
    sample.encoded.assign(data.begin(), data.end());
    return sample;
}

} // namespace

int main() {
    try {
        fruityprime::sound::Catalog catalog;
        catalog.samples.push_back(make_sample({128, 255, 192, 128}));
        catalog.samples.push_back(make_sample({128, 255, 128, 255}));
        catalog.sound_tables.entries.resize(2);
        catalog.sound_tables.entries[0].initial_volume = 127;
        catalog.sound_tables.entries[1].initial_volume = 127;

        fruityprime::sound::SfxScriptFile script;
        script.header.initial_volume = 127;
        script.entries.push_back({1, 0, 127, 255, 0x2000, 0});
        catalog.sfx_script_files.push_back(std::move(script));

        fruityprime::sound::DgnFile dgn;
        dgn.header.initial_volume = 127;
        fruityprime::sound::DgnEntry dgn_entry;
        dgn_entry.sfx_id = 0;
        dgn_entry.data[0] = {{0, 0}, {100, 127}};
        dgn_entry.data[1] = {{0, 127}, {100, 127}};
        dgn_entry.data[2] = {{0, 0x2000}, {100, 0x2000}};
        dgn_entry.data[3] = {{0, 0x2000}, {100, 0x2000}};
        dgn.entries.push_back(std::move(dgn_entry));
        catalog.dgn_files.push_back(std::move(dgn));

        fruityprime::mods::sound::SfxMixer mixer(22050);
        fruityprime::sound::SfxRuntime runtime(catalog, mixer);
        require(runtime.load(), "SfxRuntime did not load its mixer");

        const auto sample_handle = runtime.play_sample(
            0, nullptr, false, false, -1.0F, false, false);
        require(sample_handle != fruityprime::sound::SfxRuntime::InvalidHandle,
                "SfxRuntime did not start a sample");
        require(runtime.is_handle_playing(sample_handle),
                "SfxRuntime did not retain the sample handle");
        require(runtime.cached_sample_count() == 1,
                "SfxRuntime did not lazily cache one sample");

        std::vector<float> output(8);
        mixer.mix(output);
        require(output[4] < -0.15F,
                "SfxRuntime did not decode and mix the sample");

        const auto recent_handle = runtime.play_sample(
            0, nullptr, false, false, 1.0F, false, false);
        require(recent_handle == sample_handle,
                "SfxRuntime recency did not reuse a recent voice");
        mixer.mix(output);
        runtime.update(1.0F / 60.0F);
        require(!runtime.is_handle_playing(sample_handle),
                "SfxRuntime did not retire a completed sample");

        const auto script_handle = runtime.play_script(0);
        require(script_handle != fruityprime::sound::SfxRuntime::InvalidHandle,
                "SfxRuntime did not create an SFX script");
        runtime.update(0.0F);
        require(runtime.count_playing(0x4000) == 1,
                "SfxRuntime did not fire a zero-delay script entry");
        require(runtime.cached_sample_count() == 2,
                "SfxRuntime did not lazily cache the script sample");
        runtime.set_scripts_paused(true);
        runtime.update(1.0F);
        require(runtime.is_handle_playing(script_handle),
                "SfxRuntime advanced a paused script");
        runtime.set_scripts_paused(false);
        runtime.stop_scripts();
        require(!runtime.is_handle_playing(script_handle),
                "SfxRuntime did not stop script voices");

        const auto dgn_handle = runtime.play_encoded(
            0x8000, nullptr, false, false, -1.0F, true, false, 100.0F,
            100.0F);
        require(dgn_handle != fruityprime::sound::SfxRuntime::InvalidHandle,
                "SfxRuntime did not dispatch a DGN voice");
        require(runtime.count_playing(0x8000) == 1,
                "SfxRuntime did not retain the DGN instance");
        mixer.mix(output);
        require(output[4] < -0.15F,
                "SfxRuntime did not apply DGN volume and pitch curves");
        runtime.stop_all();
        require(!runtime.is_handle_playing(dgn_handle),
                "SfxRuntime did not stop the DGN instance");

        const char* rom_path = std::getenv("FRUITY_PRIME_TEST_NDS");
        if (rom_path != nullptr && *rom_path != '\0') {
            const auto real_catalog = fruityprime::sound::Catalog::load(
                fruityprime::assets::Store::from_path(rom_path));
            fruityprime::mods::sound::SfxMixer real_mixer(32728);
            fruityprime::sound::SfxRuntime real_runtime(real_catalog,
                                                         real_mixer);
            require(real_runtime.load(), "real SfxRuntime did not load");
            std::size_t first_sample = 0;
            while (first_sample < real_catalog.samples.size()
                   && !real_catalog.samples[first_sample].present) {
                ++first_sample;
            }
            require(first_sample < real_catalog.samples.size(),
                    "real sound catalog has no playable sample");
            const auto real_handle = real_runtime.play_sample(
                static_cast<std::int32_t>(first_sample));
            require(real_handle != fruityprime::sound::SfxRuntime::InvalidHandle,
                    "real SfxRuntime could not start the first sample");
            std::vector<float> real_output(128);
            real_mixer.mix(real_output);
            std::cout << "real SfxRuntime: sample=" << first_sample
                      << " cached=" << real_runtime.cached_sample_count()
                      << "\n";
        }

        runtime.shutdown();
        std::cout << "native SfxRuntime tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native SfxRuntime tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}

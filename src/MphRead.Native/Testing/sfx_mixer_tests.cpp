#include "Mods/Sound/al_es.hpp"
#include "Mods/Sound/sfx_mixer.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        fruityprime::mods::sound::SfxMixer mixer(22050);
        const auto buffer = mixer.new_buffer();
        const std::vector<std::uint8_t> samples{128, 192, 255, 128};
        require(mixer.fill_buffer(
                    buffer, fruityprime::sound::AudioBufferFormat::Mono8,
                    samples, 22050),
                "SfxMixer did not fill a PCM8 buffer");

        const auto source = mixer.new_source();
        mixer.set_buffer(source, buffer);
        mixer.play(source);
        std::vector<float> output(8, 1.0F);
        mixer.mix(output);
        require(std::abs(output[0]) < 0.0001F
                    && std::abs(output[2]) < 0.0001F,
                "closed SfxMixer did not expose silent output");

        require(mixer.open() && mixer.is_open(),
                "SfxMixer did not open its native stream");
        mixer.play(source);
        mixer.mix(output);
        require(output[2] > 0.45F && output[3] > 0.45F,
                "SfxMixer did not mix its source");
        require(mixer.pump() || !mixer.output_available(),
                "SfxMixer device pump failed while its output was available");

        mixer.close();
        require(!mixer.is_open()
                    && mixer.source_state(source)
                        == fruityprime::sound::AudioSourceState::Stopped,
                "SfxMixer close did not stop its voices");
        mixer.delete_source(source);
        mixer.delete_buffer(buffer);

        fruityprime::mods::sound::AlEs al(22050);
        const auto al_buffer = al.gen_buffer();
        const auto al_source = al.gen_source();
        const std::array<std::uint8_t, 4> al_samples{128, 192, 255, 128};
        require(al.buffer_data(al_buffer,
                               fruityprime::mods::sound::AlFormat::Mono8,
                               al_samples, 22050),
                "AlEs did not forward BufferData");
        al.source(al_source,
                  fruityprime::mods::sound::SourceInteger::Buffer, al_buffer);
        al.source(al_source,
                  fruityprime::mods::sound::SourceBoolean::Looping, true);
        al.source(al_source,
                  fruityprime::mods::sound::SourceFloat::Gain, 0.5F);
        al.listener(fruityprime::mods::sound::ListenerVector::Position,
                    {2.0F, 3.0F, 4.0F});
        al.listener_orientation({0.0F, 0.0F, -1.0F}, {0.0F, 1.0F, 0.0F});
        al.set_loop_points(al_buffer,
                           fruityprime::mods::sound::BufferLoopPoint::LoopPointsSoft,
                           1, 3);
        al.source_play(al_source);
        require(al.get_source(
                    al_source, fruityprime::mods::sound::SourceQuery::SourceState)
                    == static_cast<int>(fruityprime::mods::sound::SourceState::Playing)
                    && al.get_source(
                        al_source, fruityprime::mods::sound::SourceQuery::Buffer)
                        == static_cast<int>(al_buffer),
                "AlEs did not preserve source state and current buffer");
        require(al.loop_points_extension_present()
                    && al.get_error()
                        == fruityprime::mods::sound::Error::NoError,
                "AlEs extension/error contract changed");
        al.source_pause(al_source);
        require(al.get_source(
                    al_source, fruityprime::mods::sound::SourceQuery::SourceState)
                    == static_cast<int>(fruityprime::mods::sound::SourceState::Paused),
                "AlEs did not pause a source");
        al.source_stop(al_source);
        al.delete_source(al_source);
        al.delete_buffer(al_buffer);
        fruityprime::mods::sound::AlcEs alc(al);
        const auto device = alc.open_device();
        const auto context = alc.create_context(device);
        require(device.valid() && context.valid()
                    && alc.make_context_current(context)
                    && alc.destroy_context(context)
                    && alc.close_device(device)
                    && alc.get_error(device)
                        == fruityprime::mods::sound::ContextError::NoError,
                "AlcEs device/context lifecycle changed");
        std::cout << "native SfxMixer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native SfxMixer tests failed: " << error.what() << '\n';
        return 1;
    }
}

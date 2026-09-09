// SoundSample's intro/loop/outro spans and the stream channel interleave.
//
// ADPCM packs two samples per byte, so its loop points are scaled by two and
// PCM's are not.  Getting that factor wrong makes a looping sound restart in
// the middle of itself, which is audible but easy to mistake for a bad
// sample rather than a bad offset.
#include "Sound/sound_resources.hpp"
#include <cstdio>
int main() {
    using namespace fruityprime::sound;
    Sample s;
    s.format = WaveFormat::Pcm16;
    s.loop_start = 4; s.loop_length = 4;
    s.encoded.assign(16, 0);
    const auto i = s.intro(), l = s.loop_span(), o = s.outro();
    // PCM: no doubling. 4 + 4 + 8 = 16.
    const bool pcm = i.size() == 4 && l.size() == 4 && o.size() == 8;
    Sample a = s;
    a.format = WaveFormat::Adpcm;
    const auto ai = a.intro(), al = a.loop_span(), ao = a.outro();
    // ADPCM doubles the loop points: 8 + 8 + 0 = 16.
    const bool adpcm = ai.size() == 8 && al.size() == 8 && ao.empty();
    // A sample with no intro reports an empty one rather than a zero-length
    // slice of the loop.
    Sample n = s; n.loop_start = 0;
    const bool no_intro = n.intro().empty();
    // Interleaving two mono PCM channels alternates them.
    std::vector<std::vector<std::uint8_t>> ch{{1, 2, 3}, {9, 8, 7}};
    const auto mixed = interleave_stream_channels(ch, WaveFormat::Pcm8);
    const bool woven = mixed.size() == 6 && mixed[0] == 1 && mixed[1] == 9
                    && mixed[2] == 2 && mixed[3] == 8;
    std::printf("native sound samples: pcm=%d adpcm=%d no_intro=%d woven=%d\n",
                pcm, adpcm, no_intro, woven);
    return (pcm && adpcm && no_intro && woven) ? 0 : 1;
}

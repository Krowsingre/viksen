// Not a pass/fail test: reports how much faster than real time the engine runs a
// representative load on this machine. Real-device latency must be measured on
// hardware (see docs/TESTING.md).
#include <chrono>
#include <cstdio>
#include <memory>
#include <vector>

#include "engine/audio/AudioEngine.h"
#include "tests/TestUtil.h"

int main() {
    using namespace atmos;
    AudioEngine e;
    e.prepare({48000, 256});
    auto clip = std::make_shared<const AudioClip>(test::makeSine(48000, 48000 * 10, 220, 0.1f));
    for (int s = 0; s < 32; ++s) e.setLayer(s, clip, s % 2 ? Bus::Music : Bus::Atmosphere, s * 100);
    e.setAtmosphereBedEnabled(true);
    e.setMicInputEnabled(true);
    e.setLooping(true);
    e.play();
    std::vector<float> in(256, 0.01f), l(256), r(256);
    const float* ins[1] = {in.data()};
    float* outs[2] = {l.data(), r.data()};
    const int blocks = 48000 * 20 / 256; // 20 s of audio
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.process(ins, 1, outs, 2, 256);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("32 layers + bed + mic chain: 20 s audio rendered in %.3f s (%.1fx real time, %.2f%% of one core)\n",
                secs, 20.0 / secs, secs / 20.0 * 100.0);
    return 0;
}

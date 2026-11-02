#include "engine/audio/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace atmos {

AudioEngine::AudioEngine() {
    for (int b = 0; b < kNumBuses; ++b) buses_[b].gain.set(defaultBusGain(static_cast<Bus>(b)));
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::prepare(const EngineConfig& config) {
    config_ = config;
    config_.maxBlockSize = std::max(16, config_.maxBlockSize);
    const double sr = config_.sampleRate;

    for (auto& buf : busBuffers_) buf.setSize(2, config_.maxBlockSize);
    mix_.setSize(2, config_.maxBlockSize);
    micMono_.assign(static_cast<size_t>(config_.maxBlockSize), 0.0f);
    micWet_.assign(static_cast<size_t>(config_.maxBlockSize), 0.0f);

    for (auto& l : layers_) l.gain.prepare(sr);
    for (auto& b : buses_) b.gain.prepare(sr);
    master_.prepare(sr);
    appliedCeilingDb_ = limiterCeilingDb_.load();
    limiter_.prepare(sr, appliedCeilingDb_);
    bed_.prepare(sr);
    bed_.setSeed(0xA7305u);
    features_.prepare(sr);
    micTransform_.prepare(sr, config_.maxBlockSize);
    roomMemory_.prepare(sr);
    prepared_ = true;
}

// ---------------------------------------------------------------- control thread

bool AudioEngine::setLayer(int slot, ClipPtr clip, Bus bus, int64_t startFrame) {
    if (slot < 0 || slot >= kMaxLayers || !clip || bus == Bus::Count) return false;
    if (std::fabs(clip->sampleRate - config_.sampleRate) > 1e-6) return false;
    const AudioClip* raw = clip.get();
    {
        std::lock_guard<std::mutex> lock(ownedMutex_);
        owned_.push_back(std::move(clip));
    }
    if (!commands_.push(Command{Command::Type::SetLayer, slot, raw, bus, std::max<int64_t>(0, startFrame)})) {
        std::lock_guard<std::mutex> lock(ownedMutex_);
        owned_.pop_back();
        droppedCommands_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    layerEnds_[slot] = std::max<int64_t>(0, startFrame) + raw->numFrames();
    timelineEnd_.store(*std::max_element(layerEnds_.begin(), layerEnds_.end()), std::memory_order_relaxed);
    return true;
}

bool AudioEngine::clearLayer(int slot) {
    if (slot < 0 || slot >= kMaxLayers) return false;
    if (!commands_.push(Command{Command::Type::ClearLayer, slot, nullptr, Bus::Music, 0})) {
        droppedCommands_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    layerEnds_[slot] = 0;
    timelineEnd_.store(*std::max_element(layerEnds_.begin(), layerEnds_.end()), std::memory_order_relaxed);
    return true;
}

void AudioEngine::setLayerGain(int slot, float g) noexcept {
    if (slot >= 0 && slot < kMaxLayers) layers_[slot].gain.set(std::max(0.0f, g));
}
void AudioEngine::setLayerMuted(int slot, bool m) noexcept {
    if (slot >= 0 && slot < kMaxLayers) layers_[slot].muted.store(m, std::memory_order_relaxed);
}
int64_t AudioEngine::timelineLengthFrames() const { return timelineEnd_.load(std::memory_order_relaxed); }

void AudioEngine::setBusGain(Bus b, float g) noexcept {
    if (b != Bus::Count) buses_[static_cast<int>(b)].gain.set(std::clamp(g, 0.0f, 4.0f));
}
float AudioEngine::busGain(Bus b) const noexcept {
    return b != Bus::Count ? buses_[static_cast<int>(b)].gain.get() : 0.0f;
}
void AudioEngine::setBusMuted(Bus b, bool m) noexcept {
    if (b != Bus::Count) buses_[static_cast<int>(b)].muted.store(m, std::memory_order_relaxed);
}
bool AudioEngine::busMuted(Bus b) const noexcept {
    return b != Bus::Count && buses_[static_cast<int>(b)].muted.load(std::memory_order_relaxed);
}
void AudioEngine::setMasterGain(float g) noexcept { master_.set(std::clamp(g, 0.0f, 1.0f)); }

void AudioEngine::collectGarbage() {
    const AudioClip* dead = nullptr;
    std::lock_guard<std::mutex> lock(ownedMutex_);
    while (retired_.pop(dead)) {
        auto it = std::find_if(owned_.begin(), owned_.end(), [dead](const ClipPtr& p) { return p.get() == dead; });
        if (it != owned_.end()) owned_.erase(it);
    }
}

EngineMeters AudioEngine::meters() const noexcept {
    EngineMeters m;
    for (int c = 0; c < 2; ++c) m.outputPeak[c] = outPeak_[c].load(std::memory_order_relaxed);
    for (int b = 0; b < kNumBuses; ++b) m.busPeak[b] = busPeak_[b].load(std::memory_order_relaxed);
    m.limiterGain = limiterGain_.load(std::memory_order_relaxed);
    m.positionFrames = publishedPosition_.load(std::memory_order_relaxed);
    m.playing = playing_.load(std::memory_order_relaxed);
    m.micActive = micActive_.load(std::memory_order_relaxed);
    m.panicked = panic_.load(std::memory_order_relaxed);
    m.blocksProcessed = blocks_.load(std::memory_order_relaxed);
    m.underruns = underruns_.load(std::memory_order_relaxed);
    m.droppedCommands = droppedCommands_.load(std::memory_order_relaxed);
    return m;
}

// ---------------------------------------------------------------- audio thread

void AudioEngine::applyCommands() noexcept {
    Command cmd;
    while (commands_.pop(cmd)) {
        LayerState& l = layers_[cmd.slot];
        const AudioClip* old = l.clip;
        if (cmd.type == Command::Type::SetLayer) {
            l.clip = cmd.clip;
            l.bus = cmd.bus;
            l.startFrame = cmd.startFrame;
        } else {
            l.clip = nullptr;
        }
        // If the retire queue is full the clip simply stays owned until shutdown: safe.
        if (old) retired_.push(old);
    }
}

void AudioEngine::renderLayers(int numFrames, int64_t position) noexcept {
    for (auto& l : layers_) {
        l.gain.beginBlock();
        if (!l.clip) {
            continue;
        }
        const AudioClip& c = *l.clip;
        const bool muted = l.muted.load(std::memory_order_relaxed);
        const int64_t clipFrom = position - l.startFrame;
        const int64_t clipTo = clipFrom + numFrames;
        if (muted || clipTo <= 0 || clipFrom >= c.numFrames() || c.numChannels() == 0) {
            // Advance the smoother so un-muting ramps from the right value.
            for (int i = 0; i < numFrames; ++i) l.gain.next();
            continue;
        }
        AudioBuffer& dst = busBuffers_[static_cast<int>(l.bus)];
        const int outStart = static_cast<int>(std::max<int64_t>(0, -clipFrom));
        const int64_t srcStart = std::max<int64_t>(0, clipFrom);
        const int count = static_cast<int>(std::min<int64_t>(numFrames - outStart, c.numFrames() - srcStart));
        const float* srcL = c.samples.channel(0);
        const float* srcR = c.numChannels() > 1 ? c.samples.channel(1) : srcL;
        float* L = dst.channel(0);
        float* R = dst.channel(1);
        for (int i = 0; i < outStart; ++i) l.gain.next();
        for (int i = 0; i < count; ++i) {
            const float g = l.gain.next();
            L[outStart + i] += srcL[srcStart + i] * g;
            R[outStart + i] += srcR[srcStart + i] * g;
        }
        for (int i = outStart + count; i < numFrames; ++i) l.gain.next();
    }
}

void AudioEngine::renderMic(const float* const* inputs, int numInputs, int numFrames) noexcept {
    const bool active = micEnabled_.load(std::memory_order_relaxed) && inputs && numInputs > 0;
    micActive_.store(active, std::memory_order_relaxed);
    if (!active) {
        if (micWasActive_) roomMemory_.forget(); // privacy: no room audio outlives the mic
        micWasActive_ = false;
        return;
    }
    micWasActive_ = true;

    float* mono = micMono_.data();
    std::fill(mono, mono + numFrames, 0.0f);
    int used = 0;
    for (int ch = 0; ch < numInputs; ++ch) {
        if (!inputs[ch]) continue;
        for (int i = 0; i < numFrames; ++i) mono[i] += inputs[ch][i];
        ++used;
    }
    if (used > 1) {
        const float inv = 1.0f / static_cast<float>(used);
        for (int i = 0; i < numFrames; ++i) mono[i] *= inv;
    }

    features_.process(mono, numFrames);

    AudioBuffer& raw = busBuffers_[static_cast<int>(Bus::MicRaw)];
    for (int i = 0; i < numFrames; ++i) { raw.channel(0)[i] += mono[i]; raw.channel(1)[i] += mono[i]; }

    micTransform_.process(mono, micWet_.data(), numFrames);
    AudioBuffer& wet = busBuffers_[static_cast<int>(Bus::MicTransformed)];
    for (int i = 0; i < numFrames; ++i) { wet.channel(0)[i] += micWet_[i]; wet.channel(1)[i] += micWet_[i]; }

    roomMemory_.capture(mono, numFrames);
    roomMemory_.renderAdd(wet, numFrames);
}

void AudioEngine::sumToMaster(int numFrames) noexcept {
    mix_.clear(numFrames);
    for (int b = 0; b < kNumBuses; ++b) {
        BusState& bs = buses_[b];
        const bool muted = bs.muted.load(std::memory_order_relaxed);
        bs.gain.beginBlock();
        AudioBuffer& src = busBuffers_[b];
        float peak = 0.0f;
        for (int i = 0; i < numFrames; ++i) {
            const float g = bs.gain.next() * (muted ? 0.0f : 1.0f);
            const float l = src.channel(0)[i] * g;
            const float r = src.channel(1)[i] * g;
            mix_.channel(0)[i] += l;
            mix_.channel(1)[i] += r;
            peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
        }
        busPeak_[b].store(peak, std::memory_order_relaxed);
    }
    master_.beginBlock();
    for (int i = 0; i < numFrames; ++i) {
        const float g = master_.next();
        mix_.channel(0)[i] *= g;
        mix_.channel(1)[i] *= g;
    }
    const float ceiling = limiterCeilingDb_.load(std::memory_order_relaxed);
    if (ceiling != appliedCeilingDb_) { limiter_.setCeilingDb(ceiling); appliedCeilingDb_ = ceiling; }
    limiter_.process(mix_, numFrames);
    limiterGain_.store(limiter_.lastGainReduction(), std::memory_order_relaxed);
}

void AudioEngine::writeOutputs(float* const* outputs, int numOutputs, int numFrames) noexcept {
    float peak[2] = {0.0f, 0.0f};
    for (int ch = 0; ch < numOutputs; ++ch) {
        if (!outputs[ch]) continue;
        float* out = outputs[ch];
        if (numOutputs == 1) {
            for (int i = 0; i < numFrames; ++i) out[i] = 0.5f * (mix_.channel(0)[i] + mix_.channel(1)[i]);
        } else if (ch < 2) {
            std::copy(mix_.channel(ch), mix_.channel(ch) + numFrames, out);
        } else {
            std::fill(out, out + numFrames, 0.0f);
        }
        if (ch < 2)
            for (int i = 0; i < numFrames; ++i) peak[ch] = std::max(peak[ch], std::fabs(out[i]));
    }
    outPeak_[0].store(peak[0], std::memory_order_relaxed);
    outPeak_[1].store(numOutputs == 1 ? peak[0] : peak[1], std::memory_order_relaxed);
}

void AudioEngine::process(const float* const* inputs, int numInputs,
                          float* const* outputs, int numOutputs, int numFrames) noexcept {
    blocks_.fetch_add(1, std::memory_order_relaxed);
    if (numFrames <= 0 || !outputs) return;
    if (!prepared_) {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch]) std::fill(outputs[ch], outputs[ch] + numFrames, 0.0f);
        return;
    }
    if (numFrames <= config_.maxBlockSize) {
        processBlock(inputs, numInputs, outputs, numOutputs, numFrames);
        return;
    }
    // Hosts may deliver blocks larger than maxBlockSize: process in chunks.
    constexpr int kMaxCh = 16;
    const float* in[kMaxCh];
    float* out[kMaxCh];
    const int ni = std::min(numInputs, kMaxCh), no = std::min(numOutputs, kMaxCh);
    for (int c = no; c < numOutputs; ++c)
        if (outputs[c]) std::fill(outputs[c], outputs[c] + numFrames, 0.0f);
    for (int done = 0; done < numFrames; done += config_.maxBlockSize) {
        const int n = std::min(config_.maxBlockSize, numFrames - done);
        for (int c = 0; c < ni; ++c) in[c] = inputs && inputs[c] ? inputs[c] + done : nullptr;
        for (int c = 0; c < no; ++c) out[c] = outputs[c] ? outputs[c] + done : nullptr;
        processBlock(inputs ? in : nullptr, ni, out, no, n);
    }
}

void AudioEngine::processBlock(const float* const* inputs, int numInputs,
                               float* const* outputs, int numOutputs, int numFrames) noexcept {
    applyCommands();

    const int64_t seek = seekRequest_.exchange(-1, std::memory_order_relaxed);
    if (seek >= 0) position_ = seek;

    if (panic_.load(std::memory_order_relaxed)) {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch]) std::fill(outputs[ch], outputs[ch] + numFrames, 0.0f);
        micActive_.store(false, std::memory_order_relaxed);
        master_.resetTo(0.0f); // ramp up from silence when the panic is cleared
        limiter_.reset();
        outPeak_[0].store(0.0f); outPeak_[1].store(0.0f);
        return;
    }

    for (auto& b : busBuffers_) b.clear(numFrames);

    const bool playing = playing_.load(std::memory_order_relaxed);
    if (playing) {
        renderLayers(numFrames, position_);
    } else {
        for (auto& l : layers_) { l.gain.beginBlock(); for (int i = 0; i < numFrames; ++i) l.gain.next(); }
    }

    if (bedEnabled_.load(std::memory_order_relaxed)) bed_.processAdd(busBuffers_[static_cast<int>(Bus::Atmosphere)], numFrames);

    renderMic(inputs, numInputs, numFrames);
    sumToMaster(numFrames);
    writeOutputs(outputs, numOutputs, numFrames);

    if (playing) {
        position_ += numFrames;
        const int64_t end = timelineEnd_.load(std::memory_order_relaxed);
        if (end > 0 && position_ >= end) {
            if (looping_.load(std::memory_order_relaxed)) position_ = 0;
            else { position_ = end; playing_.store(false, std::memory_order_relaxed); }
        }
    }
    publishedPosition_.store(position_, std::memory_order_relaxed);
}

} // namespace atmos

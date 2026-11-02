#include "engine/spatial/SteamAudioRenderer.h"

#if defined(ATMOS_WITH_STEAM_AUDIO)

#include <phonon.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>

namespace atmos {

namespace {
// One Steam Audio context + default HRTF per sample rate, shared by all renderers.
struct Shared {
    IPLContext context = nullptr;
    IPLHRTF hrtf = nullptr;
    int sampleRate = 0;
    int users = 0;
};
std::mutex gSharedMutex;
Shared gShared;

bool acquireShared(int sampleRate, IPLContext& ctx, IPLHRTF& hrtf) {
    std::lock_guard<std::mutex> lock(gSharedMutex);
    if (gShared.context && gShared.sampleRate != sampleRate && gShared.users == 0) {
        iplHRTFRelease(&gShared.hrtf);
        iplContextRelease(&gShared.context);
    }
    if (!gShared.context) {
        IPLContextSettings cs{};
        cs.version = STEAMAUDIO_VERSION;
        if (iplContextCreate(&cs, &gShared.context) != IPL_STATUS_SUCCESS) { gShared.context = nullptr; return false; }
        IPLAudioSettings as{sampleRate, SteamAudioRenderer::kFrameSize};
        IPLHRTFSettings hs{};
        hs.type = IPL_HRTFTYPE_DEFAULT;
        hs.volume = 1.0f;
        hs.normType = IPL_HRTFNORMTYPE_NONE;
        if (iplHRTFCreate(gShared.context, &as, &hs, &gShared.hrtf) != IPL_STATUS_SUCCESS) {
            iplContextRelease(&gShared.context);
            gShared.context = nullptr;
            return false;
        }
        gShared.sampleRate = sampleRate;
    }
    if (gShared.sampleRate != sampleRate) return false; // another rate still in use
    ++gShared.users;
    ctx = gShared.context;
    hrtf = gShared.hrtf;
    return true;
}

void releaseShared() {
    std::lock_guard<std::mutex> lock(gSharedMutex);
    if (gShared.users > 0) --gShared.users;
}
} // namespace

struct SteamAudioRenderer::Impl {
    IPLContext context = nullptr;
    IPLHRTF hrtf = nullptr;
    IPLBinauralEffect effect = nullptr;
    std::vector<float> inFifo;           // mono samples waiting for a full frame
    int inCount = 0;
    std::vector<float> outL, outR;       // rendered samples waiting to be played
    int outRead = 0, outCount = 0, outCap = 0;
    std::vector<float> frameIn, frameL, frameR;
    float curAz = 0, curEl = 0, curGain = 0;  // smoothed per frame (audio thread)
    bool first = true;
};

SteamAudioRenderer::SteamAudioRenderer() : impl_(std::make_unique<Impl>()) {}

SteamAudioRenderer::~SteamAudioRenderer() {
    if (impl_->effect) iplBinauralEffectRelease(&impl_->effect);
    if (impl_->context) releaseShared();
}

void SteamAudioRenderer::prepare(double sampleRate, int maxBlockSize) {
    Impl& m = *impl_;
    if (m.effect) iplBinauralEffectRelease(&m.effect);
    if (m.context) { releaseShared(); m.context = nullptr; }
    ok_ = acquireShared(static_cast<int>(std::lround(sampleRate)), m.context, m.hrtf);
    if (ok_) {
        IPLAudioSettings as{static_cast<int>(std::lround(sampleRate)), kFrameSize};
        IPLBinauralEffectSettings bs{m.hrtf};
        if (iplBinauralEffectCreate(m.context, &as, &bs, &m.effect) != IPL_STATUS_SUCCESS) {
            m.effect = nullptr;
            ok_ = false;
        }
    }
    m.inFifo.assign(kFrameSize, 0.0f);
    m.outCap = maxBlockSize + 2 * kFrameSize;
    m.outL.assign(static_cast<size_t>(m.outCap), 0.0f);
    m.outR.assign(static_cast<size_t>(m.outCap), 0.0f);
    m.frameIn.assign(kFrameSize, 0.0f);
    m.frameL.assign(kFrameSize, 0.0f);
    m.frameR.assign(kFrameSize, 0.0f);
    reset();
}

void SteamAudioRenderer::reset() noexcept {
    Impl& m = *impl_;
    if (m.effect) iplBinauralEffectReset(m.effect);
    m.inCount = 0;
    // Start with one frame of silence queued: this is the renderer's fixed latency.
    std::fill(m.outL.begin(), m.outL.end(), 0.0f);
    std::fill(m.outR.begin(), m.outR.end(), 0.0f);
    m.outRead = 0;
    m.outCount = kFrameSize;
    m.first = true;
}

void SteamAudioRenderer::setPosition(const SpatialPosition& p) noexcept {
    az_.store(p.azimuthDeg, std::memory_order_relaxed);
    el_.store(p.elevationDeg, std::memory_order_relaxed);
    dist_.store(p.distanceM, std::memory_order_relaxed);
    spread_.store(p.spread, std::memory_order_relaxed);
}

void SteamAudioRenderer::processAdd(const float* mono, AudioBuffer& out, int numFrames) noexcept {
    Impl& m = *impl_;
    if (!ok_ || out.numChannels() < 2) return;
    const float pi = std::numbers::pi_v<float>;
    int consumed = 0;
    while (consumed < numFrames) {
        const int take = std::min(numFrames - consumed, kFrameSize - m.inCount);
        std::copy(mono + consumed, mono + consumed + take, m.inFifo.begin() + m.inCount);
        m.inCount += take;
        consumed += take;
        if (m.inCount < kFrameSize) break;

        // Smooth the direction per frame (head tracking updates arrive in steps).
        float targetAz = az_.load(std::memory_order_relaxed);
        const float targetEl = std::clamp(el_.load(std::memory_order_relaxed), -89.0f, 89.0f);
        const float d = std::max(0.25f, dist_.load(std::memory_order_relaxed));
        const float targetGain = std::min(2.0f, 1.0f / d);   // inverse distance, 0 dB at 1 m
        if (m.first) { m.curAz = targetAz; m.curEl = targetEl; m.curGain = targetGain; m.first = false; }
        float dAz = std::remainder(targetAz - m.curAz, 360.0f);  // shortest way round
        m.curAz += 0.5f * dAz;
        m.curEl += 0.5f * (targetEl - m.curEl);
        const float g0 = m.curGain;
        m.curGain += 0.5f * (targetGain - m.curGain);

        const float a = m.curAz * pi / 180.0f, e = m.curEl * pi / 180.0f;
        // Steam Audio listener space: +x right, +y up, -z ahead.
        IPLBinauralEffectParams params{};
        params.direction = IPLVector3{std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)};
        params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
        params.spatialBlend = 1.0f - 0.6f * std::clamp(spread_.load(std::memory_order_relaxed), 0.0f, 1.0f);
        params.hrtf = m.hrtf;
        params.peakDelays = nullptr;

        float* inPtr[1] = {m.inFifo.data()};
        float* outPtr[2] = {m.frameL.data(), m.frameR.data()};
        IPLAudioBuffer inBuf{1, kFrameSize, inPtr};
        IPLAudioBuffer outBuf{2, kFrameSize, outPtr};
        iplBinauralEffectApply(m.effect, &params, &inBuf, &outBuf);
        m.inCount = 0;

        // Queue the rendered frame (ring of outCap samples) with a gain ramp.
        for (int i = 0; i < kFrameSize; ++i) {
            const float g = g0 + (m.curGain - g0) * (static_cast<float>(i) / kFrameSize);
            const int idx = (m.outRead + m.outCount + i) % m.outCap;
            m.outL[static_cast<size_t>(idx)] = m.frameL[static_cast<size_t>(i)] * g;
            m.outR[static_cast<size_t>(idx)] = m.frameR[static_cast<size_t>(i)] * g;
        }
        m.outCount += kFrameSize;
    }

    // Play numFrames from the output queue (always available thanks to the initial frame).
    float* L = out.channel(0);
    float* R = out.channel(1);
    const int n = std::min(numFrames, m.outCount);
    for (int i = 0; i < n; ++i) {
        const int idx = (m.outRead + i) % m.outCap;
        L[i] += m.outL[static_cast<size_t>(idx)];
        R[i] += m.outR[static_cast<size_t>(idx)];
    }
    m.outRead = (m.outRead + n) % m.outCap;
    m.outCount -= n;
}

} // namespace atmos

#endif

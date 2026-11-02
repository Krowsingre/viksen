#include "platform/desktop/MiniaudioHost.h"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace atmos::desktop {

namespace {
constexpr unsigned kMaxPeriod = 8192;
constexpr int kMaxInputChannels = 2;
} // namespace

struct MiniaudioHost::Impl {
    core::LiveSession& session;
    ma_context context{};
    bool contextOk = false;
    bool nullBackend = false;
    ma_device device{};
    bool deviceOk = false;
    std::vector<ma_device_info> playbackInfos, captureInfos;
    // Planar scratch for the engine (pre-allocated; the callback never allocates).
    std::vector<float> outL, outR, in[kMaxInputChannels];
    int inChannels = 0;

    explicit Impl(core::LiveSession& s) : session(s) {
        outL.assign(kMaxPeriod, 0.0f);
        outR.assign(kMaxPeriod, 0.0f);
        for (auto& v : in) v.assign(kMaxPeriod, 0.0f);
    }

    bool initContext(bool null) {
        if (contextOk && null == nullBackend) return true;
        if (contextOk) { ma_context_uninit(&context); contextOk = false; }
        nullBackend = null;
        ma_backend nullOnly[] = {ma_backend_null};
        contextOk = ma_context_init(null ? nullOnly : nullptr, null ? 1 : 0, nullptr, &context) == MA_SUCCESS;
        return contextOk;
    }

    void refreshDevices() {
        playbackInfos.clear();
        captureInfos.clear();
        if (!contextOk && !initContext(false)) return;
        ma_device_info *pb = nullptr, *cp = nullptr;
        ma_uint32 npb = 0, ncp = 0;
        if (ma_context_get_devices(&context, &pb, &npb, &cp, &ncp) == MA_SUCCESS) {
            playbackInfos.assign(pb, pb + npb);
            captureInfos.assign(cp, cp + ncp);
        }
    }

    static void callback(ma_device* dev, void* output, const void* input, ma_uint32 frames) {
        auto* self = static_cast<Impl*>(dev->pUserData);
        float* out = static_cast<float*>(output);
        const float* inInterleaved = static_cast<const float*>(input);
        ma_uint32 done = 0;
        while (done < frames) {
            const ma_uint32 n = std::min<ma_uint32>(frames - done, kMaxPeriod);
            const float* ins[kMaxInputChannels] = {nullptr, nullptr};
            if (inInterleaved && self->inChannels > 0) {
                for (int c = 0; c < self->inChannels; ++c) {
                    float* dst = self->in[c].data();
                    for (ma_uint32 i = 0; i < n; ++i) dst[i] = inInterleaved[(done + i) * self->inChannels + c];
                    ins[c] = dst;
                }
            }
            float* outs[2] = {self->outL.data(), self->outR.data()};
            self->session.engine().process(inInterleaved ? ins : nullptr, inInterleaved ? self->inChannels : 0, outs, 2,
                                           static_cast<int>(n));
            for (ma_uint32 i = 0; i < n; ++i) {
                out[(done + i) * 2] = self->outL[i];
                out[(done + i) * 2 + 1] = self->outR[i];
            }
            done += n;
        }
    }
};

MiniaudioHost::MiniaudioHost(core::LiveSession& session) : impl_(std::make_unique<Impl>(session)) {}

MiniaudioHost::~MiniaudioHost() {
    stop();
    if (impl_->contextOk) ma_context_uninit(&impl_->context);
}

static std::vector<AudioDeviceInfo> toInfos(const std::vector<ma_device_info>& v) {
    std::vector<AudioDeviceInfo> out;
    for (const auto& d : v) out.push_back({d.name, d.isDefault != 0});
    return out;
}

std::vector<AudioDeviceInfo> MiniaudioHost::playbackDevices() { impl_->refreshDevices(); return toInfos(impl_->playbackInfos); }
std::vector<AudioDeviceInfo> MiniaudioHost::captureDevices() { impl_->refreshDevices(); return toInfos(impl_->captureInfos); }

bool MiniaudioHost::start(const HostOptions& o, std::string& error) {
    stop();
    Impl& m = *impl_;
    if (!m.initContext(o.nullBackend)) { error = "No audio system available"; return false; }
    m.refreshDevices();
    ma_device_config cfg = ma_device_config_init(o.withInput ? ma_device_type_duplex : ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    if (o.playbackDevice >= 0 && o.playbackDevice < static_cast<int>(m.playbackInfos.size()))
        cfg.playback.pDeviceID = &m.playbackInfos[static_cast<size_t>(o.playbackDevice)].id;
    if (o.withInput) {
        cfg.capture.format = ma_format_f32;
        cfg.capture.channels = 1;   // mono environment capture
        if (o.captureDevice >= 0 && o.captureDevice < static_cast<int>(m.captureInfos.size()))
            cfg.capture.pDeviceID = &m.captureInfos[static_cast<size_t>(o.captureDevice)].id;
    }
    cfg.sampleRate = 0;                        // the device's native rate: no hidden resampling
    cfg.periodSizeInFrames = std::min(o.periodFrames, kMaxPeriod);
    cfg.performanceProfile = ma_performance_profile_low_latency;
    cfg.noPreSilencedOutputBuffer = MA_TRUE;   // the engine writes every sample
    cfg.dataCallback = Impl::callback;
    cfg.pUserData = &m;
    if (ma_device_init(&m.context, &cfg, &m.device) != MA_SUCCESS) { error = "Could not open the audio device"; return false; }
    m.deviceOk = true;
    m.inChannels = o.withInput ? static_cast<int>(m.device.capture.channels) : 0;
    if (m.inChannels > kMaxInputChannels) m.inChannels = kMaxInputChannels;
    // The callback is not running yet: safe to rebuild the engine for this rate.
    m.session.rebuildIfNeeded(static_cast<double>(m.device.sampleRate), static_cast<int>(kMaxPeriod));
    if (ma_device_start(&m.device) != MA_SUCCESS) {
        ma_device_uninit(&m.device);
        m.deviceOk = false;
        error = "Could not start the audio device";
        return false;
    }
    return true;
}

void MiniaudioHost::stop() {
    if (!impl_->deviceOk) return;
    ma_device_uninit(&impl_->device);   // stops and waits for the callback to finish
    impl_->deviceOk = false;
}

bool MiniaudioHost::running() const { return impl_->deviceOk && ma_device_is_started(&impl_->device); }
double MiniaudioHost::sampleRate() const { return impl_->deviceOk ? impl_->device.sampleRate : 0.0; }
unsigned MiniaudioHost::periodFrames() const { return impl_->deviceOk ? impl_->device.playback.internalPeriodSizeInFrames : 0; }
std::string MiniaudioHost::backendName() const {
    return impl_->contextOk ? ma_get_backend_name(impl_->context.backend) : "none";
}
std::string MiniaudioHost::deviceName() const { return impl_->deviceOk ? impl_->device.playback.name : ""; }

// ---------------------------------------------------------------- stem decoder

namespace {
class MiniaudioStemDecoder final : public core::StemDecoder {
public:
    MiniaudioStemDecoder(ma_decoder* d, double rate) : dec_(d), rate_(rate) {
        ma_uint64 len = 0;
        if (ma_decoder_get_length_in_pcm_frames(dec_, &len) == MA_SUCCESS) length_ = static_cast<int64_t>(len);
    }
    ~MiniaudioStemDecoder() override { ma_decoder_uninit(dec_); delete dec_; }
    double outputRate() const override { return rate_; }
    int64_t lengthFrames() const override { return length_; }
    bool seek(int64_t f) override { return ma_decoder_seek_to_pcm_frame(dec_, static_cast<ma_uint64>(std::max<int64_t>(0, f))) == MA_SUCCESS; }
    int read(float* lr, int frames) override {
        ma_uint64 got = 0;
        if (ma_decoder_read_pcm_frames(dec_, lr, static_cast<ma_uint64>(frames), &got) != MA_SUCCESS && got == 0) return 0;
        return static_cast<int>(got);
    }
private:
    ma_decoder* dec_;
    double rate_;
    int64_t length_ = 0;
};
} // namespace

std::unique_ptr<core::StemDecoder> makeMiniaudioStemDecoder(const std::string& path, double outputRate, std::string& error) {
    // Output: interleaved stereo float at the engine rate (miniaudio converts channels and rate).
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 2, static_cast<ma_uint32>(outputRate));
    cfg.resampling.linear.lpfOrder = MA_MAX_FILTER_ORDER;   // best anti-aliasing when the file's rate differs
    auto* d = new ma_decoder;
    if (ma_decoder_init_file(path.c_str(), &cfg, d) != MA_SUCCESS) {
        delete d;
        error = "Unsupported or unreadable audio file: " + path;
        return nullptr;
    }
    return std::make_unique<MiniaudioStemDecoder>(d, outputRate);
}

} // namespace atmos::desktop

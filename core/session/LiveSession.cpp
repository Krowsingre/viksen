#include "core/session/LiveSession.h"

#include <algorithm>
#include <cmath>

#include "core/assets/ClipPreparation.h"

namespace atmos::core {

LiveSession::LiveSession(double sampleRate, int maxBlockSize) { rebuild(sampleRate, maxBlockSize); }

LiveSession::~LiveSession() { stopFeeder(); }

void LiveSession::rebuild(double sampleRate, int maxBlockSize) {
    // The World stream feeds the old engine's player: stop it first and resume
    // at the same musical position with decoders for the new rate.
    const bool worldWasPlaying = feeder_ && feeder_->running();
    double resumeSeconds = 0.0;
    if (feeder_) resumeSeconds = static_cast<double>(feeder_->playbackFrame()) / feeder_->rate();
    stopFeeder();
    engine_ = std::make_unique<AudioEngine>();
    applySettings(); // before prepare(): smoothers start at their targets
    engine_->prepare({sampleRate, maxBlockSize});
    if (original_) engine_->setLayer(kTrackSlot, prepareClipForEngine(*original_, sampleRate), Bus::Music, 0);
    if (worldWasPlaying) startFeeder(static_cast<int64_t>(resumeSeconds * sampleRate), nullptr);
}

bool LiveSession::rebuildIfNeeded(double sampleRate, int maxBlockSize) {
    if (std::fabs(sampleRate - this->sampleRate()) < 1e-6 && maxBlockSize <= this->maxBlockSize()) return false;
    rebuild(sampleRate, std::max(maxBlockSize, this->maxBlockSize()));
    return true;
}

bool LiveSession::setTrack(std::shared_ptr<const AudioClip> original, ClipPtr prepared, double preparedRate) {
    if (!original || original->numFrames() == 0) return false;
    const double rate = sampleRate();
    if (!prepared || std::fabs(preparedRate - rate) > 1e-6) prepared = prepareClipForEngine(*original, rate);
    engine_->pause();
    engine_->seek(0);
    if (!engine_->setLayer(kTrackSlot, std::move(prepared), Bus::Music, 0)) return false;
    original_ = std::move(original);
    return true;
}

void LiveSession::clearTrack() {
    engine_->pause();
    engine_->clearLayer(kTrackSlot);
    original_.reset();
}

double LiveSession::trackDurationSeconds() const noexcept { return original_ ? original_->durationSeconds() : 0.0; }

void LiveSession::applySettings() {
    AudioEngine& e = *engine_;
    for (int b = 0; b < kNumBuses; ++b) {
        e.setBusGain(static_cast<Bus>(b), settings_.busGain[b]);
        e.setBusMuted(static_cast<Bus>(b), settings_.busMuted[b]);
    }
    e.setMasterGain(settings_.masterGain);
    e.setLimiterCeilingDb(settings_.limiterCeilingDb);
    e.setAtmosphereBedEnabled(settings_.bedEnabled);
    e.atmosphereBed().level.set(settings_.bedLevel);
    e.atmosphereBed().colourHz.set(settings_.bedColourHz);
    e.micTransform().cutoffHz.set(settings_.micToneHz);
    e.micTransform().echoSeconds.set(settings_.micEchoSeconds);
    e.micTransform().feedback.set(settings_.micFeedback);
    e.micTransform().wet.set(settings_.micWet);
    e.micTransform().shiftHz.set(settings_.micShiftHz);
    e.micTransform().shiftMix.set(settings_.micShiftMix);
    e.roomMemory().setEnabled(settings_.roomMemoryEnabled);
    e.roomMemory().level.set(settings_.roomMemoryLevel);
    e.roomMemory().pitchSemitones.store(settings_.roomMemoryPitch);
    e.roomMemory().listenerYawDeg.store(listenerYaw_);
    e.worldPlayer().setListenerYaw(listenerYaw_);
    e.worldPlayer().setSpatialEnabled(worldSpatial_);
    e.setMicInputEnabled(settings_.micEnabled);
    e.setLooping(settings_.looping);
}

void LiveSession::setBusGain(Bus b, float g) {
    if (b == Bus::Count) return;
    settings_.busGain[static_cast<int>(b)] = g;
    engine_->setBusGain(b, g);
}
void LiveSession::setBusMuted(Bus b, bool m) {
    if (b == Bus::Count) return;
    settings_.busMuted[static_cast<int>(b)] = m;
    engine_->setBusMuted(b, m);
}
void LiveSession::setMasterGain(float g) { settings_.masterGain = g; engine_->setMasterGain(g); }
void LiveSession::setLimiterCeilingDb(float db) { settings_.limiterCeilingDb = db; engine_->setLimiterCeilingDb(db); }
void LiveSession::setBedEnabled(bool e) { settings_.bedEnabled = e; engine_->setAtmosphereBedEnabled(e); }
void LiveSession::setBedLevel(float v) { settings_.bedLevel = v; engine_->atmosphereBed().level.set(v); }
void LiveSession::setBedColourHz(float v) { settings_.bedColourHz = v; engine_->atmosphereBed().colourHz.set(v); }
void LiveSession::setMicToneHz(float v) { settings_.micToneHz = v; engine_->micTransform().cutoffHz.set(v); }
void LiveSession::setMicEchoSeconds(float v) { settings_.micEchoSeconds = v; engine_->micTransform().echoSeconds.set(v); }
void LiveSession::setMicFeedback(float v) { settings_.micFeedback = v; engine_->micTransform().feedback.set(v); }
void LiveSession::setMicWet(float v) { settings_.micWet = v; engine_->micTransform().wet.set(v); }
void LiveSession::setMicEnabled(bool e) { settings_.micEnabled = e; engine_->setMicInputEnabled(e); }
void LiveSession::setLooping(bool l) { settings_.looping = l; engine_->setLooping(l); }
void LiveSession::setMicShiftHz(float v) { settings_.micShiftHz = v; engine_->micTransform().shiftHz.set(v); }
void LiveSession::setMicShiftMix(float v) { settings_.micShiftMix = v; engine_->micTransform().shiftMix.set(v); }
void LiveSession::setRoomMemoryEnabled(bool e) { settings_.roomMemoryEnabled = e; engine_->roomMemory().setEnabled(e); }
void LiveSession::setRoomMemoryLevel(float v) { settings_.roomMemoryLevel = v; engine_->roomMemory().level.set(v); }
void LiveSession::setRoomMemoryPitch(float v) { settings_.roomMemoryPitch = v; engine_->roomMemory().pitchSemitones.store(v); }

void LiveSession::updateEnvironment(double dt) {
    states_.update(engine_->environmentFeatures(), engine_->meters().micActive, dt);
}
void LiveSession::addMotionSample(double ax, double ay, double az, double dt) {
    motion_.addSample(ax, ay, az, dt);
    states_.updateMotion(motion_.energy(), dt);
}
void LiveSession::setMotionAvailable(bool available) { states_.setMotionAvailable(available); }
void LiveSession::setListenerYaw(float degrees) {
    listenerYaw_ = degrees;
    engine_->roomMemory().listenerYawDeg.store(degrees, std::memory_order_relaxed);
    engine_->worldPlayer().setListenerYaw(degrees);
}

// ---------------------------------------------------------------- Worlds

void LiveSession::stopFeeder() {
    if (feeder_) {
        resumeFrame_ = feeder_->playbackFrame();
        feeder_->stop();
        feeder_.reset();
    }
}

bool LiveSession::startFeeder(int64_t fromFrame, std::string* error) {
    if (!world_ || !worldFactory_) return false;
    std::string err;
    auto decoders = worldFactory_(sampleRate(), err);
    if (decoders.empty() || decoders.size() != world_->stems.size()) {
        if (error) *error = err.empty() ? "could not open the World's audio" : err;
        return false;
    }
    const double rate = sampleRate();
    const int64_t loopStart = static_cast<int64_t>(world_->loopFromSeconds * rate);
    const int64_t loopEnd = world_->loopToSeconds ? static_cast<int64_t>(*world_->loopToSeconds * rate) : 0;
    feeder_ = std::make_unique<WorldFeeder>(engine_->worldPlayer(), std::move(decoders), loopStart, loopEnd);
    applyWorldSpatial();
    for (int i = 0; i < static_cast<int>(world_->stems.size()); ++i)
        engine_->worldPlayer().setStemGain(i, director_ ? director_->linearGain(i) * static_cast<float>(lifecycle_ ? lifecycle_->gain() : 0.0) : 0.0f);
    feeder_->start(fromFrame);
    return true;
}

bool LiveSession::loadWorld(const WorldManifest& world, StemDecoderFactory factory, std::string* error) {
    unloadWorld();
    world_ = world;
    worldAuthored_ = world;
    worldFactory_ = std::move(factory);
    director_ = std::make_unique<WorldDirector>(world);
    resumeFrame_ = 0;
    // Validate that the audio opens now rather than on first play.
    std::string err;
    auto probe = worldFactory_(sampleRate(), err);
    if (probe.size() != world.stems.size()) {
        if (error) *error = err.empty() ? "could not open the World's audio" : err;
        unloadWorld();
        return false;
    }
    return true;
}

void LiveSession::unloadWorld() {
    stopFeeder();
    for (int i = 0; i < WorldStreamPlayer::kMaxStems; ++i) engine_->worldPlayer().setStemGain(i, 0.0f);
    world_.reset();
    worldAuthored_.reset();
    worldFactory_ = nullptr;
    director_.reset();
    lifecycle_.reset();
    resumeFrame_ = 0;
}

void LiveSession::startWorld() {
    if (!world_) return;
    LifecycleTiming timing;
    timing.fadeInSeconds = 3.0;
    timing.fadeOutSeconds = 2.5;
    lifecycle_ = std::make_unique<WorldLifecycle>(timing);
    lifecycle_->setStayForever(true);   // the listener decides when to leave
    if (!feeder_) startFeeder(resumeFrame_, nullptr);
}

void LiveSession::stopWorld() {
    if (lifecycle_) lifecycle_->requestExit();
}

void LiveSession::updateWorld(double dt, const ConditionContext& context) {
    if (!world_ || !director_) return;
    if (!lifecycle_) {
        director_->snapToTargets(context);
        return;
    }
    director_->update(context, dt);
    lifecycle_->update(true, dt);
    const float master = static_cast<float>(lifecycle_->gain());
    for (int i = 0; i < director_->stemCount(); ++i)
        engine_->worldPlayer().setStemGain(i, director_->linearGain(i) * master);
    if (lifecycle_->finished()) {
        stopFeeder();
        lifecycle_.reset();
    }
}

void LiveSession::applyWorldSpatial() {
    WorldStreamPlayer& p = engine_->worldPlayer();
    p.setSpatialEnabled(worldSpatial_);
    if (!world_) return;
    for (size_t i = 0; i < world_->stems.size(); ++i) {
        const auto& s = world_->stems[i];
        p.setStemPosition(static_cast<int>(i), s.azimuthDeg, s.elevationDeg, s.distanceM, s.widthDeg);
    }
}

void LiveSession::setWorldSpatialEnabled(bool on) {
    worldSpatial_ = on;
    engine_->worldPlayer().setSpatialEnabled(on);
}

void LiveSession::setWorldStemPosition(int stem, float az, float el, float dist) {
    if (!world_ || stem < 0 || stem >= static_cast<int>(world_->stems.size())) return;
    auto& s = world_->stems[static_cast<size_t>(stem)];
    s.azimuthDeg = std::remainder(az, 360.0f);
    s.elevationDeg = std::clamp(el, -90.0f, 90.0f);
    s.distanceM = std::clamp(dist, 0.25f, 20.0f);
    engine_->worldPlayer().setStemPosition(stem, s.azimuthDeg, s.elevationDeg, s.distanceM, s.widthDeg);
}

void LiveSession::resetWorldStemPositions() {
    if (!world_ || !worldAuthored_) return;
    for (size_t i = 0; i < world_->stems.size(); ++i) {
        const auto& a = worldAuthored_->stems[i];
        setWorldStemPosition(static_cast<int>(i), a.azimuthDeg, a.elevationDeg, a.distanceM);
    }
}

WorldStatus LiveSession::worldStatus() const {
    WorldStatus st;
    if (!world_) return st;
    st.loaded = true;
    st.id = world_->id;
    st.name = world_->name;
    st.playing = lifecycle_ != nullptr;
    st.master = lifecycle_ ? static_cast<float>(lifecycle_->gain()) : 0.0f;
    const WorldStreamPlayer& p = const_cast<AudioEngine&>(*engine_).worldPlayer();
    st.spatial = worldSpatial_;
    st.hrtf = p.hrtfActive();
    st.underruns = p.underruns();
    if (feeder_) {
        st.positionSeconds = static_cast<double>(feeder_->playbackFrame()) / feeder_->rate();
        st.lengthSeconds = static_cast<double>(feeder_->loopEnd()) / feeder_->rate();
    } else {
        st.positionSeconds = static_cast<double>(resumeFrame_) / sampleRate();
    }
    for (int i = 0; i < static_cast<int>(world_->stems.size()); ++i) {
        WorldStemStatus s;
        s.id = world_->stems[static_cast<size_t>(i)].id;
        s.title = world_->stems[static_cast<size_t>(i)].title;
        s.azimuthDeg = world_->stems[static_cast<size_t>(i)].azimuthDeg;
        s.elevationDeg = world_->stems[static_cast<size_t>(i)].elevationDeg;
        s.distanceM = world_->stems[static_cast<size_t>(i)].distanceM;
        s.widthDeg = world_->stems[static_cast<size_t>(i)].widthDeg;
        s.gainDb = director_ ? director_->currentDb(i) : -60.0f;
        s.peak = p.stemPeak(i);
        if (director_) s.reasons = director_->reasons(i);
        st.stems.push_back(std::move(s));
    }
    return st;
}

} // namespace atmos::core

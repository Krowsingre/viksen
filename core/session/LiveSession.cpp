#include "core/session/LiveSession.h"

#include <algorithm>
#include <cmath>

#include "core/assets/ClipPreparation.h"

namespace atmos::core {

LiveSession::LiveSession(double sampleRate, int maxBlockSize) { rebuild(sampleRate, maxBlockSize); }

LiveSession::~LiveSession() {
    recorder_.stop();
    stopFeeder();
}

bool LiveSession::startRecording(const std::filesystem::path& file, CaptureMode mode, std::string* error) {
    return recorder_.start(engine_->capture(), mode, file, sampleRate(), error);
}

double LiveSession::stopRecording() { return recorder_.stop(); }

void LiveSession::rebuild(double sampleRate, int maxBlockSize) {
    // The World stream feeds the old engine's player: stop it first and resume
    // at the same musical position with decoders for the new rate.
    const bool worldWasPlaying = feeder_ && feeder_->running();
    double resumeSeconds = 0.0;
    if (feeder_) resumeSeconds = static_cast<double>(feeder_->playbackFrame()) / feeder_->rate();
    stopFeeder();
    // The capture tap belongs to the old engine: finish the recording cleanly.
    if (recorder_.recording()) { recorder_.stop(); recordingInterrupted_ = true; }
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

void LiveSession::applyAllSettings(const LiveSettings& next) {
    const bool mic = settings_.micEnabled;
    settings_ = next;
    settings_.micEnabled = mic;   // the microphone only ever changes by explicit user action
    applySettings();
}

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
    planner_.reset();
    int64_t crossfade = 0;
    if (!world_->sections.empty()) {
        // Adaptive sections: jump points on the bar grid of the song.
        int64_t len = 0;
        for (auto& d : decoders) len = std::max(len, d->lengthFrames());
        const int64_t end = loopEnd > 0 ? std::min(loopEnd, len) : len;
        const double beat = 60.0 / std::max(1.0, world_->tempo);
        const double bar = beat * world_->beatsPerBar;
        std::vector<std::pair<int64_t, int64_t>> frames;
        for (const auto& sec : world_->sections)
            frames.emplace_back(static_cast<int64_t>(std::llround(sec.fromSeconds * rate)),
                                static_cast<int64_t>(std::llround(sec.toSeconds * rate)));
        const int64_t gridStart = static_cast<int64_t>(std::llround(world_->barOffsetSeconds * rate));
        planner_ = std::make_shared<SectionPlanner>(std::move(frames), static_cast<int64_t>(std::llround(bar * world_->jumpEveryBars * rate)),
                                                    std::max(loopStart, gridStart), end,
                                                    static_cast<int64_t>(std::llround(bar * world_->minStayBars * rate)));
        crossfade = static_cast<int64_t>(std::llround(beat * world_->crossfadeBeats * rate));
        planner_->setChoice(lastChoice_);
        planner_->setStopAtSongEnd(dimension_ && dimension_->songs.size() > 1);
    }
    feeder_ = std::make_unique<WorldFeeder>(engine_->worldPlayer(), std::move(decoders), loopStart, loopEnd, planner_, crossfade);
    applyWorldSpatial();
    for (int i = 0; i < static_cast<int>(world_->stems.size()); ++i)
        engine_->worldPlayer().setStemGain(i, director_ ? director_->linearGain(i) * static_cast<float>(lifecycle_ ? lifecycle_->gain() : 0.0) : 0.0f);
    feeder_->start(fromFrame);
    return true;
}

bool LiveSession::loadWorld(const WorldManifest& world, StemDecoderFactory factory, std::string* error) {
    unloadWorld();
    return loadSong(world, std::move(factory), error);
}

bool LiveSession::loadSong(const WorldManifest& world, StemDecoderFactory factory, std::string* error) {
    world_ = world;
    worldAuthored_ = world;
    worldFactory_ = std::move(factory);
    director_ = std::make_unique<WorldDirector>(world);
    resumeFrame_ = 0;
    lastChoice_ = SectionChoice{};
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

StemDecoderFactory LiveSession::songFactory(int song) const {
    std::vector<std::string> files;
    for (const auto& st : dimension_->songs[static_cast<size_t>(song)].stems) files.push_back(st.file);
    StemFileDecoder decode = fileDecoder_;
    return [files, decode](double rate, std::string& err) {
        std::vector<std::unique_ptr<StemDecoder>> v;
        for (const auto& f : files) {
            auto d = decode(f, rate, err);
            if (!d) return std::vector<std::unique_ptr<StemDecoder>>{};
            v.push_back(std::move(d));
        }
        return v;
    };
}

bool LiveSession::loadDimension(const DimensionManifest& d, StemFileDecoder decode, std::string* error) {
    unloadWorld();
    if (d.songs.empty()) { if (error) *error = "the Dimension has no songs"; return false; }
    dimension_ = d;
    fileDecoder_ = std::move(decode);
    song_ = 0;
    if (!loadSong(d.songs[0], songFactory(0), error)) return false;
    dimension_ = d;            // loadSong's failure path clears it; success keeps it
    return true;
}

// Multi-song Dimensions: move to another song when the listener's state suits one of
// its parts and nothing in this song does (held for a while, so a passing sound does
// not change the song), or when this song has ended. Control thread.
void LiveSession::updateSongChoice(double dt, const ConditionContext& context) {
    if (!dimension_ || dimension_->songs.size() < 2 || !lifecycle_ || switchTo_ >= 0) return;
    const int n = static_cast<int>(dimension_->songs.size());
    if (feeder_ && feeder_->finishedPlaying()) {
        beginSongSwitch((song_ + 1) % n, context, true);
        return;
    }
    int candidate = -1;
    if (lastChoice_.composed) {
        for (int k = 1; k < n && candidate < 0; ++k) {
            const int i = (song_ + k) % n;
            if (!chooseSections(dimension_->songs[static_cast<size_t>(i)].sections, context).composed) candidate = i;
        }
    }
    if (candidate != candidate_) { candidate_ = candidate; candidateFor_ = 0; return; }
    if (candidate_ < 0) return;
    candidateFor_ += dt;
    const double bar = 60.0 / std::max(1.0, world_->tempo) * world_->beatsPerBar;
    if (candidateFor_ >= std::max(4.0, bar * world_->minStayBars)) beginSongSwitch(candidate_, context, false);
}

void LiveSession::beginSongSwitch(int song, const ConditionContext& context, bool immediate) {
    switchTo_ = song;
    candidate_ = -1;
    candidateFor_ = 0;
    // Start the next song at a part that suits now (or from the top when composed).
    const auto& next = dimension_->songs[static_cast<size_t>(song)];
    const SectionChoice ch = chooseSections(next.sections, context);
    switchFromSeconds_ = 0;
    if (!ch.composed)
        for (size_t i = 0; i < next.sections.size(); ++i)
            if ((ch.acceptable >> i & 1) && (ch.preferred == 0 || (ch.preferred >> i & 1))) { switchFromSeconds_ = next.sections[i].fromSeconds; break; }
    LifecycleTiming t;
    t.fadeInSeconds = 3.0;
    t.fadeOutSeconds = immediate ? 0.05 : 3.0;   // after a natural ending there is nothing left to fade
    lifecycle_->setTiming(t);
    lifecycle_->requestExit();
}

void LiveSession::finishSongSwitch() {
    const int song = switchTo_;
    switchTo_ = -1;
    stopFeeder();
    auto keep = dimension_;
    auto decode = fileDecoder_;
    std::string err;
    if (!loadSong(keep->songs[static_cast<size_t>(song)], songFactory(song), &err)) return;
    dimension_ = std::move(keep);
    fileDecoder_ = std::move(decode);
    song_ = song;
    applyWorldSpatial();
    resumeFrame_ = static_cast<int64_t>(switchFromSeconds_ * sampleRate());
    startWorld(false);
    songSwitches_++;
}

void LiveSession::unloadWorld() {
    stopFeeder();
    for (int i = 0; i < WorldStreamPlayer::kMaxStems; ++i) engine_->worldPlayer().setStemGain(i, 0.0f);
    world_.reset();
    worldAuthored_.reset();
    worldFactory_ = nullptr;
    director_.reset();
    lifecycle_.reset();
    planner_.reset();
    lastChoice_ = SectionChoice{};
    resumeFrame_ = 0;
    dimension_.reset();
    switchTo_ = -1;
    candidate_ = -1;
}

void LiveSession::startWorld(bool quick) {
    if (!world_) return;
    LifecycleTiming timing;
    timing.fadeInSeconds = quick ? 0.15 : 3.0;
    timing.fadeOutSeconds = quick ? 0.15 : 2.5;
    lifecycle_ = std::make_unique<WorldLifecycle>(timing);
    lifecycle_->setStayForever(true);   // the listener decides when to leave
    if (!feeder_) startFeeder(resumeFrame_, nullptr);
}

void LiveSession::stopWorld(bool quick) {
    if (!lifecycle_) return;
    switchTo_ = -1;            // the listener is leaving: no next song
    candidate_ = -1;
    if (quick) {
        LifecycleTiming t;
        t.fadeInSeconds = 0.15;
        t.fadeOutSeconds = 0.15;
        lifecycle_->setTiming(t);
    }
    lifecycle_->requestExit();
}

void LiveSession::seekWorld(double seconds) {
    if (!world_) return;
    const int64_t frame = std::max<int64_t>(0, static_cast<int64_t>(seconds * sampleRate()));
    if (feeder_) {
        stopFeeder();
        startFeeder(frame, nullptr);
    } else {
        resumeFrame_ = frame;
    }
}

void LiveSession::setWorldStemLevel(int stem, float gainDb, bool muted) {
    if (!world_ || stem < 0 || stem >= static_cast<int>(world_->stems.size())) return;
    world_->stems[static_cast<size_t>(stem)].baseGainDb = muted ? WorldDirector::kSilenceDb : std::clamp(gainDb, -60.0f, 6.0f);
    if (director_) director_->setBaseGainDb(stem, world_->stems[static_cast<size_t>(stem)].baseGainDb);
}

void LiveSession::updateWorld(double dt, const ConditionContext& context) {
    if (!world_ || !director_) return;
    if (!lifecycle_) {
        director_->snapToTargets(context);
        return;
    }
    director_->update(context, dt);
    if (!world_->sections.empty()) {
        lastChoice_ = chooseSections(world_->sections, context);
        if (planner_) planner_->setChoice(lastChoice_);
    }
    updateSongChoice(dt, context);
    lifecycle_->update(true, dt);
    const float master = static_cast<float>(lifecycle_->gain());
    for (int i = 0; i < director_->stemCount(); ++i)
        engine_->worldPlayer().setStemGain(i, director_->linearGain(i) * master);
    if (lifecycle_->finished() && switchTo_ >= 0) {
        finishSongSwitch();
        return;
    }
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
    st.playing = lifecycle_ != nullptr || switchTo_ >= 0;
    st.master = lifecycle_ ? static_cast<float>(lifecycle_->gain()) : 0.0f;
    const WorldStreamPlayer& p = const_cast<AudioEngine&>(*engine_).worldPlayer();
    st.spatial = worldSpatial_;
    st.hrtf = p.hrtfActive();
    st.underruns = p.underruns();
    st.adaptive = !world_->sections.empty();
    if (dimension_) {
        st.id = dimension_->id;
        st.name = dimension_->name;
        st.songCount = static_cast<int>(dimension_->songs.size());
        st.songIndex = song_;
        st.songTitle = world_->name;
        st.songSwitches = songSwitches_;
    } else {
        st.songTitle = world_->name;
    }
    st.composed = lastChoice_.composed;
    if (feeder_ && planner_) {
        const int sec = planner_->sectionAt(feeder_->playbackFrame());
        // The planner sorts by start time; find the authored section with that range.
        if (sec >= 0) {
            const double t = static_cast<double>(feeder_->playbackFrame()) / feeder_->rate();
            for (const auto& ws : world_->sections)
                if (t >= ws.fromSeconds && t < ws.toSeconds) { st.sectionTitle = ws.title; st.sectionLabel = ws.label; break; }
        }
        st.sectionJumps = feeder_->jumps();
    }
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
